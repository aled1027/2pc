#include "2pc_garbler.h"

#include <assert.h> 
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"
#include "utils.h"

#include "gc_comm.h"

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
extract_labels_gc(block *garbLabels, block *evalLabels, const GarbledCircuit *gc,
                  const InputMapping *map, const int *inputs)
{
    int eval_p = 0, garb_p = 0;
    for (int i = 0; i < map->size; ++i) {
        Wire *wire;

        switch (map->imap_instr[i].inputter) {
            case PERSON_GARBLER:
                for (int j = 0; j < map->imap_instr[i].dist; ++j) {
                    wire = &gc->wires[map->imap_instr[i].wire_id];
                    memcpy(&garbLabels[garb_p],
                           inputs[garb_p] ? &wire->label1 : &wire->label0, sizeof(block));
                    garb_p++;
                }
                break;
            case PERSON_EVALUATOR:
                for (int j = 0; j < map->imap_instr[i].dist; ++j) {
                    wire = &gc->wires[map->imap_instr[i].wire_id];
                    memcpy(&evalLabels[eval_p], wire, sizeof(Wire));
                    eval_p+=2;
                }

                break;
            default:
                assert(0);
                abort();
        }
    }
}

//static void
//extract_labels_cgc(block *garbLabels, block *evalLabels,
//                   const ChainedGarbledCircuit *cgc,
//                   const int *circuitMapping, const InputMapping *map,
//                   const int *inputs)
//{
//    int eval_p = 0, garb_p = 0;
//    for (int i = 0; i < map->size; ++i) {
//        Wire *wire = &cgc[circuitMapping[map->gc_id[i]]].gc.wires[map->wire_id[i]];
//        switch (map->inputter[i]) {
//        case PERSON_GARBLER:
//            memcpy(&garbLabels[garb_p],
//                   inputs[garb_p] ? &wire->label1 : &wire->label0, sizeof(block));
//            garb_p++;
//            break;
//        case PERSON_EVALUATOR:
//            memcpy(&evalLabels[eval_p], wire, sizeof(Wire));
//            eval_p += 2;
//            break;
//        default:
//            assert(0);
//            abort();
//        }
//    }
//    
//}

void 
garbler_classic_2pc(GarbledCircuit *gc, const InputMapping *input_mapping,
                    const block *output_map, int num_garb_inputs, int num_eval_inputs,
                    const int *inputs, uint64_t *tot_time)
{
    /* Does 2PC in the classic way, without components. */
    int serverfd, fd;
    struct state state;
    uint64_t start, end;

    assert(gc->n == num_garb_inputs + num_eval_inputs);

    state_init(&state);

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }
    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    start = current_time_();;

    gc_comm_send(fd, gc);

    /* Send input_mapping to evaluator */
    {
        size_t buf_size;
        char *buffer;

        buf_size = inputMappingBufferSize(input_mapping);
        buffer = malloc(buf_size);
        (void) writeInputMappingToBuffer(input_mapping, buffer);
        net_send(fd, &buf_size, sizeof(buf_size), 0);
        net_send(fd, buffer, buf_size, 0);
        free(buffer);
    }

    /* Setup for communicating labels to evaluator */
    /* Populate garb_labels and eval_labels as instructed by input_mapping */
    block *garb_labels = NULL, *eval_labels = NULL;
    if (num_garb_inputs > 0) {
        garb_labels = allocate_blocks(num_garb_inputs);
    }
    if (num_eval_inputs > 0) {
        eval_labels = allocate_blocks(2 * num_eval_inputs);
    }

    extract_labels_gc(garb_labels, eval_labels, gc, input_mapping, inputs);

    /* Send garb_labels */
    if (num_garb_inputs > 0) {
        net_send(fd, garb_labels, sizeof(block) * num_garb_inputs, 0);
        free(garb_labels);
    }

    /* Send eval_label via OT */
    if (num_eval_inputs > 0) {
        ot_np_send(&state, fd, eval_labels, sizeof(block), num_eval_inputs, 2,
                   new_msg_reader, new_item_reader);
        free(eval_labels);
    }

    /* Send output_map to evaluator */
    net_send(fd, output_map, sizeof(block) * 2 * gc->m, 0);

    /* close and clean up network connection */
    close(fd);
    close(serverfd);
    state_cleanup(&state);

    end = current_time_();
    *tot_time = end - start;
}

static inline size_t
addToBuffer(char *buffer, const void *item, size_t length)
{
    (void) memcpy(buffer, item, length);
    return length;
}

static void
garbler_go(int fd, const FunctionSpec *function, const char *dir,
           const ChainedGarbledCircuit *chained_gcs, const block *randLabels,
           int num_chained_gcs, const int *circuitMapping, const int *inputs,
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
    block *garbLabels, *evalLabels;
    char *buffer;
    size_t p = 0;
    uint64_t start, end, _start, _end;

    start = current_time_();
    _start = current_time_();
    {
        bool *usedInput[2];
        usedInput[0] = calloc(num_garb_inputs, sizeof(bool));
        usedInput[1] = calloc(num_eval_inputs, sizeof(bool));

        InputMapping imap = function->input_mapping;
        evalLabels = allocate_blocks(2 * num_eval_inputs);
        garbLabels = allocate_blocks(num_garb_inputs);
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
                    printf("Person not detected while processing input_mapping.\n");
                    break;
            }
            for (int j = 0; j < cur->dist; ++j) {
                usedInput[cur->inputter][cur->input_idx + j] = true;
            }
        }
    }
    _end = current_time_();
    fprintf(stderr, "Set up input labels: %llu\n", _end - _start);

    /* Send evaluator's labels via OT correction */
    _start = current_time_();
    if (num_eval_inputs > 0) {
        uint64_t _start, _end;
        int *corrections = calloc(num_eval_inputs, sizeof(int));
        _start = current_time_();
        {
            net_recv(fd, corrections, sizeof(int) * num_eval_inputs, 0);
        }
        _end = current_time_();
        fprintf(stderr, "OT correction (receive): %llu\n", _end - _start);

        for (int i = 0; i < num_eval_inputs; ++i) {
            assert(corrections[i] == 0 || corrections[i] == 1);
            evalLabels[2 * i] = xorBlocks(evalLabels[2 * i],
                                          randLabels[2 * i + corrections[i]]);
            evalLabels[2 * i + 1] = xorBlocks(evalLabels[2 * i + 1],
                                              randLabels[2 * i + !corrections[i]]);
        }
        free(corrections);

        buffer = realloc(NULL, sizeof(block) * 2 * num_eval_inputs);
        p += addToBuffer(buffer + p, evalLabels, sizeof(block) * 2 * num_eval_inputs);
    } else {
        buffer = NULL;
    }
    _end = current_time_();
    fprintf(stderr, "OT correction: %llu\n", _end - _start);

    _start = current_time_();
    {
        int size = function->components.totComponents + 1;
        buffer = realloc(buffer, p + sizeof size + sizeof(int) * size);
        p += addToBuffer(buffer + p, &size, sizeof size);
        p += addToBuffer(buffer + p, circuitMapping, sizeof(int) * size);
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
        buffer = realloc(buffer, p
                         + sizeof(int) + sizeof(Instruction) * function->instructions.size
                         + sizeof(int) + sizeof(block) * noffsets);
        p += addToBuffer(buffer + p, &function->instructions.size, sizeof(int));
        p += addToBuffer(buffer + p, &noffsets, sizeof noffsets);
        p += addToBuffer(buffer + p, function->instructions.instr,
                         sizeof(Instruction) * function->instructions.size);
        p += addToBuffer(buffer + p, offsets, sizeof(block) * noffsets);
    }
    _end = current_time_();
    fprintf(stderr, "Fill buffer: %llu\n", _end - _start);
    /* Send everything */
    _start = current_time_();
    {
        net_send(fd, buffer, p, 0);
    }
    _end = current_time_();
    fprintf(stderr, "Send buffer: %llu\n", _end - _start);

    if (num_garb_inputs > 0)
        free(garbLabels);
    if (num_eval_inputs > 0)
        free(evalLabels);
    free(buffer);

    end = current_time_();
    fprintf(stderr, "Total (post connection): %llu\n", end - start);
    fprintf(stderr, "Bytes sent: %lu\n", g_bytes_sent);
    fprintf(stderr, "Bytes received: %lu\n", g_bytes_received);

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
    }
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
    offsets[0] = zero_block();
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
                offsets[offsetsIdx] = xorBlocks(
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
                offsets[offsetsIdx] = xorBlocks(
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

        // hardcoding 3 as the max number of simd_info.input_blocks
        int gcChainingMap[num_chained_gcs+1][num_chained_gcs+1][3]; 
        for (int i = 0; i < num_chained_gcs+1; i++) {
            for (int j = 0; j < num_chained_gcs+1; j++) {
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
                    offsets[offsetsIdx] = xorBlocks(
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
    {
        /* Send chained garbled circuits */
        for (int i = 0; i < num_chained_gcs; i++) {
            chained_gc_comm_send(fd, &chained_gcs[i], chainingType);
            saveChainedGC(&chained_gcs[i], dir, true, chainingType);
        }

        /* pre-processing OT using random labels */
        if (num_eval_inputs > 0) {
            block *evalLabels;
            char lblName[50];

            (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */

            evalLabels = allocate_blocks(2 * num_eval_inputs);
            for (int i = 0; i < 2 * num_eval_inputs; ++i) {
                evalLabels[i] = randomBlock();
            }
            
            ot_np_send(&state, fd, evalLabels, sizeof(block), num_eval_inputs, 2,
                       new_msg_reader, new_item_reader);
            saveOTLabels(lblName, evalLabels, num_eval_inputs, true);

            free(evalLabels);
        }
    }
    end = current_time_();
    fprintf(stderr, "garbler offline: %llu\n", (end - start));

    close(fd);
    close(serverfd);
    state_cleanup(&state);
}

int
garbler_online(char *function_path, char *dir, int *inputs, int num_garb_inputs,
               int num_chained_gcs, uint64_t *tot_time, ChainingType chainingType) 
{
    /*runs the garbler code
     * First, initializes and loads function, and then calls garbler_go which
     * runs the core of the garbler's code
     */

    int serverfd, fd;
    uint64_t _start, _end, total = 0;
    ChainedGarbledCircuit *chained_gcs;
    FunctionSpec function;
    int *circuitMapping, noffsets = 0;
    block *randLabels = NULL, *offsets = NULL;
    char lblName[50];

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }

    /* Load function from disk */
    _start = current_time_();
    {
        /*load function allocates a bunch of memory for the function*/
        /*this is later freed by freeFunctionSpec*/
        if (load_function_via_json(function_path, &function, chainingType) == FAILURE) {
            fprintf(stderr, "Could not load function %s\n", function_path);
            return FAILURE;
        }
    }
    _end = current_time_();
    total += _end - _start;
    fprintf(stderr, "Load function: %llu\n", (_end - _start));

    /* Load everything else from disk */
    _start = current_time_();
    {
        /* load chained garbled circuits from disk */
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        for (int i = 0; i < num_chained_gcs; ++i) {
            loadChainedGC(&chained_gcs[i], dir, i, true, chainingType);
        }

        /* load ot info from disk */
        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */
        randLabels = loadOTLabels(lblName);
    }
    _end = current_time_();
    total += _end - _start;
    fprintf(stderr, "Load components: %llu\n", (_end - _start));

    _start = current_time_();
    {
        /* +1 because 0th component is inputComponent*/
        circuitMapping = malloc(sizeof(int) * (function.components.totComponents + 1));
        offsets = allocate_blocks(function.instructions.size);
        if (make_real_instructions(&function, chained_gcs, num_chained_gcs,
                                   circuitMapping, offsets, &noffsets,
                                   chainingType) == FAILURE) {
            fprintf(stderr, "Could not make instructions\n");
            return FAILURE;
        }
    }
    _end = current_time_();
    total += _end - _start;
    fprintf(stderr, "Make instructions: %llu\n", (_end - _start));

    _start = current_time_();
    {
        int res = make_real_output_instructions(&function, chained_gcs,
                                                num_chained_gcs, circuitMapping);
        if (res == FAILURE) {
            fprintf(stderr, "Could not make output instructions\n");
            return FAILURE;
        }

    }
    _end = current_time_();
    total += _end - _start;
    fprintf(stderr, "Make output: %llu\n", (_end - _start));

    /* Accept connection after loading is all done */
    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    /*main function; does core of work*/
    garbler_go(fd, &function, dir, chained_gcs, randLabels, num_chained_gcs,
               circuitMapping, inputs, offsets, noffsets, &total);
    
    free(circuitMapping);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], true, chainingType);
    }
    free(chained_gcs);
    freeFunctionSpec(&function);

    if (tot_time) {
        *tot_time = total;
    }

    close(fd);
    close(serverfd);

    //free(randLabels);

    return SUCCESS;
}
