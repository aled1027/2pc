#include "2pc_garbler.h"

#include <assert.h> 
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>

#include <garble.h>
#include <garble/aes.h>

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"
#include "utils.h"

#include "gc_comm.h"

static inline size_t
addToBuffer(char *buffer, const void *item, size_t length)
{
    (void) memcpy(buffer, item, length);
    return length;
}

static void *
new_msg_reader(void *msgs, int idx)
{
    block *m = (block *) msgs;
    return &m[2 * idx];
}

static void *
new_item_reader(void *item, int idx, ssize_t *mlen)
{
    block *a = (block *) item;
    *mlen = sizeof(block);
    return &a[idx];
}

static void
extract_labels_gc(block *garbLabels, block *evalLabels, const garble_circuit *gc,
                  const OldInputMapping *map, const bool *inputs)
{
    int eval_p = 0, garb_p = 0;
    for (int i = 0; i < map->size; ++i) {
        block *wire = &gc->wires[2 * map->wire_id[i]];
        switch (map->inputter[i]) {
        case PERSON_GARBLER:
            memcpy(&garbLabels[garb_p],
                   inputs[garb_p] ? wire + 1 : wire, sizeof(block));
            garb_p++;
            break;
        case PERSON_EVALUATOR:
            memcpy(&evalLabels[eval_p], wire, sizeof(block));
            memcpy(&evalLabels[eval_p + 1], wire + 1, sizeof(block));
            eval_p += 2;
            break;
        default:
            assert(0);
            abort();
        }
    }
}

void 
garbler_classic_2pc(garble_circuit *gc, const OldInputMapping *input_mapping,
                    const block *output_map, int num_garb_inputs,
                    int num_eval_inputs, const bool *inputs, uint64_t *tot_time)
{
    block *randLabels;
    block garb_labels[num_garb_inputs], eval_labels[2 * num_eval_inputs];
    int serverfd, fd;
    size_t p = 0;
    uint64_t start, end;

    assert(gc->n == (size_t) num_garb_inputs + num_eval_inputs);

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }
    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    /* pre-process OT */
    if (num_eval_inputs > 0) {
        struct state state;
        state_init(&state);
        if (num_eval_inputs > 0) {
            randLabels = garble_allocate_blocks(2 * num_eval_inputs);
            for (int i = 0; i < 2 * num_eval_inputs; ++i) {
                randLabels[i] = garble_random_block();
            }
            ot_np_send(&state, fd, randLabels, sizeof(block), num_eval_inputs, 2,
                       new_msg_reader, new_item_reader);
        }
        state_cleanup(&state);
    }

    start = current_time_();
    g_bytes_sent = g_bytes_received = 0;

    extract_labels_gc(garb_labels, eval_labels, gc, input_mapping, inputs);

    if (num_eval_inputs > 0) {
        int corrections[num_eval_inputs];
        (void) net_recv(fd, corrections, sizeof corrections, 0);
        for (int i = 0; i < num_eval_inputs; ++i) {
            assert(corrections[i] == 0 || corrections[i] == 1);
            eval_labels[2 * i] = garble_xor(eval_labels[2 * i],
                                           randLabels[2 * i + corrections[i]]);
            eval_labels[2 * i + 1] = garble_xor(eval_labels[2 * i + 1],
                                               randLabels[2 * i + !corrections[i]]);
        }
        free(randLabels);

        (void) net_send(fd, eval_labels, sizeof eval_labels, 0);
    }

    if (num_garb_inputs > 0) {
        (void) net_send(fd, garb_labels, sizeof garb_labels, 0);
    }

    gc_comm_send(fd, gc);

    (void) net_send(fd, output_map, 2 * gc->m * sizeof output_map[0], 0);

    {
        size_t size;
        size = inputMappingBufferSize(input_mapping);
        (void) net_send(fd, &size, sizeof size, 0);
        char buffer[size];
        (void) writeInputMappingToBuffer(input_mapping, buffer);
        (void) net_send(fd, buffer, size, 0);
    }

    close(fd);
    close(serverfd);

    end = current_time_();
    if (tot_time)
        *tot_time += end - start;
}

static void
garbler_go(int fd, const FunctionSpec *function, const char *dir,
           const ChainedGarbledCircuit *chained_gcs, const block *randLabels,
           int num_chained_gcs, const int *circuitMapping, const bool *inputs,
           const block *offsets, int noffsets, uint64_t *tot_time)
{
    /* primary role: send the appropriate data to the evaluator. 
     * garbler_go sends: 
     *  - instructions, offsets
     *  - garbler labels
     *  - evaluator labels
     *  - output instruction, outputmap */
    int num_eval_inputs = function->num_eval_inputs;
    int num_garb_inputs = function->num_garb_inputs;
    block garbLabels[num_garb_inputs], evalLabels[2 * num_eval_inputs];
    char *buffer;
    size_t p = 0;
    uint64_t start, end, _start, _end;

    start = current_time_();
    g_bytes_sent = 0;
    g_bytes_received = 0;

    {
        bool *usedInput[2];
        usedInput[0] = calloc(num_garb_inputs, sizeof(bool));
        usedInput[1] = calloc(num_eval_inputs, sizeof(bool));

        InputMapping imap = function->input_mapping;
        for (int i = 0; i < imap.size; i++) {
            InputMappingInstruction *cur = &imap.imap_instr[i];
            int gc_id = circuitMapping[cur->gc_id];
            int input_idx = 0;
            int wire_id = 0;

            if (usedInput[cur->inputter][cur->input_idx]) {
                continue;
            }

            switch(cur->inputter) {
                case PERSON_GARBLER:
                    for (int j = 0; j < cur->dist; ++j) {
                        input_idx = cur->input_idx + j;
                        wire_id = cur->wire_id + j;
                        garbLabels[input_idx] = chained_gcs[gc_id].inputLabels[2*wire_id + inputs[input_idx]];
                    }
                    break;
                case PERSON_EVALUATOR:
                    input_idx = cur->input_idx;
                    wire_id = cur->wire_id;
                    memcpy(&evalLabels[2*input_idx],
                        &chained_gcs[gc_id].inputLabels[2*wire_id],
                        2 * cur->dist * sizeof(block));
                    break;
                default:
                    fprintf(stderr, "Person not detected while processing input_mapping.\n");
                    break;
            }
            for (int j = 0; j < cur->dist; ++j) {
                usedInput[cur->inputter][cur->input_idx + j] = true;
            }
        }
        free(usedInput[0]);
        free(usedInput[1]);
    }

    /* Send evaluator's labels via OT correction */
    if (num_eval_inputs > 0) {
        int corrections[num_eval_inputs];
        (void) net_recv(fd, corrections, sizeof(int) * num_eval_inputs, 0);

        for (int i = 0; i < num_eval_inputs; ++i) {
            assert(corrections[i] == 0 || corrections[i] == 1);
            evalLabels[2 * i] = garble_xor(evalLabels[2 * i],
                                           randLabels[2 * i + corrections[i]]);
            evalLabels[2 * i + 1] = garble_xor(evalLabels[2 * i + 1],
                                               randLabels[2 * i + !corrections[i]]);
        }

        buffer = realloc(NULL, sizeof(block) * 2 * num_eval_inputs);
        p += addToBuffer(buffer + p, evalLabels, sizeof(block) * 2 * num_eval_inputs);
    } else {
        buffer = NULL;
    }
    

    {
        buffer = realloc(buffer, p + sizeof num_garb_inputs
                         + (num_garb_inputs ? sizeof(block) * num_garb_inputs
                            : 0));
        p += addToBuffer(buffer + p, &num_garb_inputs, sizeof num_garb_inputs);
        if (num_garb_inputs) {
            p += addToBuffer(buffer + p, garbLabels, sizeof(block) * num_garb_inputs);
        }
    }
    {
        buffer = realloc(buffer, p + sizeof function->output_instructions.size
                         + function->output_instructions.size * sizeof(OutputInstruction));
        p += addToBuffer(buffer + p, &function->output_instructions.size,
                         sizeof(function->output_instructions.size));
        p += addToBuffer(buffer + p,
                         function->output_instructions.output_instruction,
                         function->output_instructions.size * sizeof(OutputInstruction));
    }

    {
        buffer = realloc(buffer, p + sizeof(int) + (sizeof(block) * noffsets));
        p += addToBuffer(buffer + p, &noffsets, sizeof noffsets);
        p += addToBuffer(buffer + p, offsets, sizeof(block) * noffsets);
    }

    /* Send everything */
    {
        net_send(fd, buffer, p, 0);
    }

    free(buffer);

    end = current_time_();

    if (tot_time)
        *tot_time += end - start;
}

static int 
make_real_output_instructions(FunctionSpec* function,
                              ChainedGarbledCircuit *chained_gcs,
                              int num_chained_gcs, int *circuitMapping)
{
    /* 
     * Turns the output instructions from json into usable output instructions
     * using the chained_gcs.  Specially, populates
     * function->output_instructions->output_instruction[i].label0 and label1
     */
    unsigned char *rand;
    OutputInstructions* output_instructions = &function->output_instructions;

    rand = calloc((output_instructions->size + 8) / 8, sizeof(char));
    if (rand == NULL)
        return FAILURE;
    (void) RAND_bytes(rand, (output_instructions->size + 8) / 8);
    for (int i = 0; i < output_instructions->size; i++) {
        AES_KEY key;
        bool choice = rand[(i / 8) & (1 << (i % 8))] ? true : false;
        OutputInstruction* o = &output_instructions->output_instruction[i];
        int savedGCId = circuitMapping[o->gc_id];
        block key_zero = chained_gcs[savedGCId].outputMap[2 * o->wire_id];
        block key_one = chained_gcs[savedGCId].outputMap[2 * o->wire_id + 1];
        block b_zero = garble_zero_block();
        block b_one = garble_make_block((uint64_t) 0, (uint64_t) 1); // 000...00001

        AES_set_encrypt_key(key_zero, &key);
        AES_ecb_encrypt_blks(&b_zero, 1, &key);
        AES_set_encrypt_key(key_one, &key);
        AES_ecb_encrypt_blks(&b_one, 1, &key);
        o->labels[choice] = b_zero;
        o->labels[!choice] = b_one;
    }
    free(rand);
    return SUCCESS;
}


static int
make_real_instructions(FunctionSpec *function,
                       ChainedGarbledCircuit *chained_gcs,
                       int num_chained_gcs, int *circuitMapping,
                       block *offsets, int *noffsets, ChainingType chainingType)
{
    /* primary role: populate circuitMapping
     * circuitMapping maps instruction-gc-ids --> saved-gc-ids 
     */

    bool* is_circuit_used;
    int num_component_types;
        
    is_circuit_used = calloc(num_chained_gcs, sizeof(bool));
    num_component_types = function->components.numComponentTypes;
    circuitMapping[0] = 0;

    /* construct circuit mapping */
    for (int i = 0; i < num_component_types; i++) {
        CircuitType needed_type = function->components.circuitType[i];
        int num_needed = function->components.nCircuits[i];
        int *circuit_ids = function->components.circuitIds[i];
        
        // j indexes circuit_ids, k indexes is_circuit_used and saved_gcs
        int j = 0;

        // find an available circuit
        for (int k = 0; k < num_chained_gcs; k++) {
            if (!is_circuit_used[k] && chained_gcs[k].type == needed_type) {
                // map it, and increment j
                circuitMapping[circuit_ids[j]] = k;
                is_circuit_used[k] = true;
                j++;
            }
            if (num_needed == j) break;
        }
        if (j < num_needed) {
            fprintf(stderr, "Not enough circuits of type %d available\n", needed_type);
            return FAILURE;
        }
    }
    free(is_circuit_used);

    /* Part 2: Set chaining offsets */
    /* This is done in two loops. The first loop does everything for the input
     * mapping. The second loop does everything for regular chaining. While
     * clearly not the most efficient, as it could all be done in a single loop,
     * that would require a lot of jumping, and is not the most
     * readable/maintainable code.*/

    /* Add input component chaining offsets */
    int *imapCirc = allocate_ints(function->n); // a somewhat misleading name.
    int *imapWire = allocate_ints(function->n); // a somewhat misleading name.

    for (int i = 0; i < function->n; ++i) {
        imapCirc[i] = -1;
    }

    Instruction *cur;
    int offsetsIdx = 1;
    int num_instructions = function->instructions.size;
    offsets[0] = garble_zero_block();
    for (int i = 0; i < num_instructions; ++i) {
        cur = &(function->instructions.instr[i]);
        /* Chain input/output wires */
        if (cur->type == CHAIN && cur->ch.fromCircId == 0) { /* if input component */
            int idx = cur->ch.fromWireId;
            if (imapCirc[idx] == -1) {
                /* this input has not been used */
                for (int j = 0; j < cur->ch.wireDist; j++) {
                    imapCirc[idx + j] = cur->ch.toCircId;
                    imapWire[idx + j] = cur->ch.toWireId + j;
                }
                cur->ch.offsetIdx = 0;
            } else {
                /* this idx has been used */
                /* this is only used for levenshtein presently */
                offsets[offsetsIdx] = garble_xor(
                    chained_gcs[circuitMapping[imapCirc[idx]]].inputLabels[2 * imapWire[idx]],
                    chained_gcs[circuitMapping[cur->ch.toCircId]].inputLabels[2*cur->ch.toWireId]); 
                cur->ch.offsetIdx = offsetsIdx;
                ++offsetsIdx;
            }
        }
    }

    free(imapCirc);
    free(imapWire);

    if (chainingType == CHAINING_TYPE_STANDARD) {
        for (int i = 0; i < num_instructions; ++i) {
            cur = &(function->instructions.instr[i]);
            if (cur->type == CHAIN && cur->ch.fromCircId != 0) {
                offsets[offsetsIdx] = garble_xor(
                    chained_gcs[circuitMapping[cur->ch.fromCircId]].outputMap[2*cur->ch.fromWireId],
                    chained_gcs[circuitMapping[cur->ch.toCircId]].inputLabels[2*cur->ch.toWireId]); 
                
                cur->ch.offsetIdx = offsetsIdx;
                ++offsetsIdx;
            }
        }
    } else { /* CHAINING_TYPE_SIMD */
        /* Do chaining with SIMD. Here an entire component is chained with a single offset */

        /* gcChainingMap tracks which component is mapping to which component. 
         * maps (gc-id, gc-id, simd-idx) --> offset-idx */

        int ***gcChainingMap;
        /* int gcChainingMap[num_chained_gcs+1][num_chained_gcs+1][3];  */

        gcChainingMap = calloc(num_chained_gcs + 1, sizeof(int **));
        for (int i = 0; i < num_chained_gcs+1; i++) {
            gcChainingMap[i] = calloc(num_chained_gcs + 1, sizeof(int *));
            for (int j = 0; j < num_chained_gcs+1; j++) {
                gcChainingMap[i][j] = calloc(3, sizeof(int));
                for (int k = 0; k < 3; k++) {
                    gcChainingMap[i][j][k] = -1;
                }
            }
        }
        
        for (int i = 0; i < num_instructions; ++i) {
            cur = &(function->instructions.instr[i]);
            if (cur->type == CHAIN && cur->ch.fromCircId != 0) {
                int simd_idx = chained_gcs[circuitMapping[cur->ch.toCircId]]
                    .simd_info.iblock_map[cur->ch.toWireId];

                if (gcChainingMap[cur->ch.fromCircId][cur->ch.toCircId][simd_idx] == -1) {
                    /* add approparite offset to offsets */
                    offsets[offsetsIdx] = garble_xor(
                            chained_gcs[circuitMapping[cur->ch.toCircId]].simd_info.input_blocks[simd_idx],
                            chained_gcs[circuitMapping[cur->ch.fromCircId]].simd_info.output_block);

                    gcChainingMap[cur->ch.fromCircId][cur->ch.toCircId][simd_idx] = offsetsIdx;
                    
                    cur->ch.offsetIdx = offsetsIdx;
                    ++offsetsIdx;



                } else {
                    /* reference block already in offsets */
                    cur->ch.offsetIdx = gcChainingMap[cur->ch.fromCircId][cur->ch.toCircId][simd_idx];
                }
            }
        }

        for (int i = 0; i < num_chained_gcs + 1; ++i) {
            for (int j = 0; j < num_chained_gcs + 1; ++j) {
                free(gcChainingMap[i][j]);
            }
            free(gcChainingMap[i]);
        }
        free(gcChainingMap);
    }
    *noffsets = offsetsIdx;
    return SUCCESS;
}

void
garbler_offline(char *dir, ChainedGarbledCircuit* chained_gcs,
                int num_eval_inputs, int num_chained_gcs, ChainingType chainingType)
{
    int serverfd, fd;
    struct state state;
    uint64_t start, end;

    state_init(&state);

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }

    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    start = current_time_();

    for (int i = 0; i < num_chained_gcs; i++) {
        chained_gc_comm_send(fd, &chained_gcs[i], chainingType);
        saveChainedGC(&chained_gcs[i], dir, true, chainingType);
    }

    /* pre-processing OT using random labels */
    if (num_eval_inputs > 0) {
        block *evalLabels;
        char *fname;
        size_t size;

        size = strlen(dir) + strlen("/lbl") + 1;
        fname = malloc(size);
        (void) snprintf(fname, size, "%s/%s", dir, "lbl");

        evalLabels = garble_allocate_blocks(2 * num_eval_inputs);
        for (int i = 0; i < 2 * num_eval_inputs; ++i) {
            evalLabels[i] = garble_random_block();
        }
            
        ot_np_send(&state, fd, evalLabels, sizeof(block), num_eval_inputs, 2,
                   new_msg_reader, new_item_reader);
        saveOTLabels(fname, evalLabels, num_eval_inputs, true);

        free(evalLabels);
        free(fname);
    }

    end = current_time_();
    fprintf(stderr, "garbler offline: %llu\n", (end - start));

    close(fd);
    close(serverfd);
    state_cleanup(&state);
}

int
garbler_online(char *function_path, char *dir, bool *inputs, int num_garb_inputs,
               int num_chained_gcs, uint64_t *tot_time, ChainingType chainingType) 
{
    /*runs the garbler code
     * First, initializes and loads function, and then calls garbler_go which
     * runs the core of the garbler's code
     */

    int serverfd, fd;
    uint64_t start, end;
    ChainedGarbledCircuit *chained_gcs;
    FunctionSpec function;
    int *circuitMapping, noffsets = 0;
    block *randLabels = NULL, *offsets = NULL;

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        return FAILURE;
    }

    start = current_time_();

    /* Load function from disk */
    if (load_function_via_json(function_path, &function, chainingType) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    /* Load everything else from disk */
    {
        size_t size;
        /* load chained garbled circuits from disk */
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        for (int i = 0; i < num_chained_gcs; ++i) {
            if (loadChainedGC(&chained_gcs[i], dir, i, true, chainingType) == FAILURE) {
                fprintf(stderr, "Could not load chained GC\n");
                return FAILURE;
            }
        }

        size = strlen(dir)  + strlen("/lbl") + 1;
        char lblName[size];
        (void) snprintf(lblName, size, "%s/%s", dir, "lbl");
        randLabels = loadOTLabels(lblName);
    }

    {
        /* +1 because 0th component is inputComponent*/
        circuitMapping = malloc(sizeof(int) * (function.components.totComponents + 1));
        offsets = garble_allocate_blocks(function.instructions.size);
        if (make_real_instructions(&function, chained_gcs, num_chained_gcs,
                                   circuitMapping, offsets, &noffsets,
                                   chainingType) == FAILURE) {
            fprintf(stderr, "Could not make instructions\n");
            return FAILURE;
        }
    }

    {
        int res = make_real_output_instructions(&function, chained_gcs,
                                                num_chained_gcs, circuitMapping);
        if (res == FAILURE) {
            fprintf(stderr, "Could not make output instructions\n");
            return FAILURE;
        }

    }

    /* Accept connection after loading is all done */
    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        return FAILURE;
    }

    // Send instructions to evaluator
    (void) net_send(fd, &function.instructions.size, sizeof(int), 0);
    (void) net_send_compressed(fd, function.instructions.instr, function.instructions.size * sizeof(Instruction), 0);

    // Send circuit mapping
    {
        int size = function.components.totComponents + 1;
        (void) net_send(fd, &size, sizeof size, 0);
        (void) net_send_compressed(fd, circuitMapping, sizeof(int) * size, 0);
    }

    /*main function; does core of work*/
    garbler_go(fd, &function, dir, chained_gcs, randLabels, num_chained_gcs,
               circuitMapping, inputs, offsets, noffsets, NULL);

    free(circuitMapping);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], true, chainingType);
    }
    free(chained_gcs);
    freeFunctionSpec(&function);
    free(offsets);
    free(randLabels);

    end = current_time_();
    
    close(fd);
    close(serverfd);

    if (tot_time) {
        *tot_time = end - start;
    }

    return SUCCESS;
}
