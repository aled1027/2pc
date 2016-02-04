#include "2pc_garbler.h"
#include <assert.h> 
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>

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
        Wire *wire = &gc->wires[map->wire_id[i]];
        switch (map->inputter[i]) {
        case PERSON_GARBLER:
            memcpy(&garbLabels[garb_p],
                   inputs[garb_p] ? &wire->label1 : &wire->label0, sizeof(block));
            garb_p++;
            break;
        case PERSON_EVALUATOR:
            memcpy(&evalLabels[eval_p], wire, sizeof(Wire));
            eval_p += 2;
            break;
        default:
            assert(0);
            abort();
        }
    }
}

static void
extract_labels_cgc(block *garbLabels, block *evalLabels,
                   const ChainedGarbledCircuit *cgc,
                   const int *circuitMapping, const InputMapping *map,
                   const int *inputs)
{
    int eval_p = 0, garb_p = 0;
    for (int i = 0; i < map->size; ++i) {
        Wire *wire = &cgc[circuitMapping[map->gc_id[i]]].gc.wires[map->wire_id[i]];
        switch (map->inputter[i]) {
        case PERSON_GARBLER:
            memcpy(&garbLabels[garb_p],
                   inputs[garb_p] ? &wire->label1 : &wire->label0, sizeof(block));
            garb_p++;
            break;
        case PERSON_EVALUATOR:
            memcpy(&evalLabels[eval_p], wire, sizeof(Wire));
            eval_p += 2;
            break;
        default:
            assert(0);
            abort();
        }
    }
    
}

void 
garbler_classic_2pc(GarbledCircuit *gc, const InputMapping *input_mapping,
                    const block *output_map, const int num_garb_inputs, const int num_eval_inputs,
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

    start = current_time();;

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

    end = current_time();
    *tot_time = end - start;
}

static void
sendInstructions(const Instructions *insts, const int fd, const block *offsets, 
        const int noffsets, ChainingType chainingType)
{
    net_send(fd, &insts->size, sizeof(int), 0);
    net_send(fd, insts->instr, sizeof(Instruction) * insts->size, 0);

    net_send(fd, &noffsets, sizeof(int), 0);
    net_send(fd, offsets, sizeof(block) * noffsets, 0);
}

static void
garbler_go(int fd, const FunctionSpec *function, const char *dir,
           const ChainedGarbledCircuit *chained_gcs, const block *randLabels,
           int num_chained_gcs, const int *circuitMapping, const int *inputs,
           const block *offsets, const int noffsets, ChainingType chainingType)
{
    /* primary role: send appropriate labels to evaluator and garbled circuits*/
    InputMapping imap = function->input_mapping;
    int num_eval_inputs = function->num_eval_inputs;
    int num_garb_inputs = function->num_garb_inputs;
    block *garbLabels, *evalLabels;
    uint64_t _start, _end;

    _start = current_time();
    {
        sendInstructions(&function->instructions, fd, offsets, noffsets, chainingType);
    }
    _end = current_time();
    fprintf(stderr, "send inst: %llu\n", _end - _start);

    // 3. send circuitMapping
    _start = current_time();
    {
        int size = function->components.totComponents + 1;
        net_send(fd, &size, sizeof(int), 0);
        net_send(fd, circuitMapping, sizeof(int) * size, 0);
    }
    _end = current_time();
    fprintf(stderr, "send circmap: %llu\n", _end - _start);

    _start = current_time();
    {
        bool *usedGarbInputIdx, *usedEvalInputIdx;

        evalLabels = allocate_blocks(2 * num_eval_inputs);
        garbLabels = allocate_blocks(num_garb_inputs);
        usedGarbInputIdx = calloc(sizeof(bool), num_garb_inputs);
        usedEvalInputIdx = calloc(sizeof(bool), num_eval_inputs);

        for (int i = 0; i < imap.size; i++) {
            block *label = &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i]];
            int input_idx = imap.input_idx[i];
            switch(imap.inputter[i]) {
            case PERSON_GARBLER:
                if (usedGarbInputIdx[input_idx] == false) {
                    garbLabels[input_idx] = *(label + inputs[input_idx]);
                    usedGarbInputIdx[input_idx] = true;
                }
                break;
            case PERSON_EVALUATOR:
                if (usedEvalInputIdx[input_idx] == false) {
                    evalLabels[2*input_idx] = *label;
                    evalLabels[2*input_idx + 1] = *(label + 1);
                    usedEvalInputIdx[input_idx] = true;
                }
                break;
            default:
                printf("Person not detected while processing input_mapping.\n");
                break;
            } 
        }
        free(usedGarbInputIdx);
        free(usedEvalInputIdx);

        net_send(fd, &num_garb_inputs, sizeof(int), 0);
        if (num_garb_inputs > 0) {
            net_send(fd, garbLabels, sizeof(block) * num_garb_inputs, 0);
        }
    }
    _end = current_time();
    fprintf(stderr, "send labels: %llu\n", _end - _start);

    /* Send evaluator's labels via OT correct */
    /* OT correction */
    _start = current_time();
    if (num_eval_inputs > 0) {
        int *corrections = malloc(sizeof(int) * num_eval_inputs);
        net_recv(fd, corrections, sizeof(int) * num_eval_inputs, 0);

        for (int i = 0; i < num_eval_inputs; ++i) {
            switch (corrections[i]) {
            case 0:
                evalLabels[2 * i] =
                    xorBlocks(evalLabels[2 * i], randLabels[2 * i]);
                evalLabels[2 * i + 1] =
                    xorBlocks(evalLabels[2 * i + 1], randLabels[2 * i + 1]);
                break;
            case 1:
                evalLabels[2 * i] =
                    xorBlocks(evalLabels[2 * i], randLabels[2 * i + 1]);
                evalLabels[2 * i + 1] =
                    xorBlocks(evalLabels[2 * i + 1], randLabels[2 * i]);
                break;
            default:
                assert(0);
                abort();
            }
        }

        net_send(fd, evalLabels, sizeof(block) * 2 * num_eval_inputs, 0);
        free(corrections);
    }
    _end = current_time();
    fprintf(stderr, "ot correction: %llu\n", _end - _start);

    _start = current_time();
    {
        int size = function->output.size;

        // Send "output" 
        // output is from the json, and tells which components/wires are used
        // for outputs note that size is not size of the output, but length of
        // the arrays in output
        net_send(fd, &size, sizeof size, 0); 
        net_send(fd, function->output.gc_id, sizeof(int) * size, 0);
        net_send(fd, function->output.start_wire_idx, sizeof(int) * size, 0);
        net_send(fd, function->output.end_wire_idx, sizeof(int) * size, 0);

        // 5b. send outputMap
        net_send(fd, &function->m, sizeof function->m, 0);
        for (int j = 0; j < size; j++) {
            int gc_id = circuitMapping[function->output.gc_id[j]];
            int start_wire_idx = function->output.start_wire_idx[j];
            int end_wire_idx = function->output.end_wire_idx[j];
            // add 1 because values are inclusive
            int dist = end_wire_idx - start_wire_idx + 1;


            net_send(fd, &chained_gcs[gc_id].outputMap[start_wire_idx],
                    sizeof(block) * 2 * dist, 0);
        }
    }
    _end = current_time();
    fprintf(stderr, "send output/outputmap: %llu\n", _end - _start);

    if (num_garb_inputs > 0)
        free(garbLabels);
    if (num_eval_inputs > 0)
        free(evalLabels);
}

static int
garbler_make_real_instructions(FunctionSpec *function,
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

    // loop over type of circuit
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
     * mapping. The second loop does everything for regular chaining. While clearly
     * not the most efficient, as it could all be done in a single loop, that would require
     * a lot of jumping, and is not the most readable/maintainable code.*/

    /* Add input component chaining offsets */
    int *imapCirc = allocate_ints(function->n);
    int *imapWire = allocate_ints(function->n);
    for (int i = 0; i < function->n; ++i)
        imapCirc[i] = -1;

    int offsetsIdx = 1;
    int num_instructions = function->instructions.size;
    offsets[0] = zero_block();
    Instruction *cur;

    for (int i = 0; i < num_instructions; ++i) {
        cur = &(function->instructions.instr[i]);
        if (cur->type == CHAIN && cur->chFromCircId == 0) {
            int idx = cur->chFromWireId;
            if (imapCirc[idx] == -1) {
                /* this input has not been used */
                imapCirc[idx] = cur->chToCircId;
                imapWire[idx] = cur->chToWireId;
                cur->chOffsetIdx = 0;
            } else {
                /* this idx has been used */
                offsets[offsetsIdx] = xorBlocks(
                    chained_gcs[circuitMapping[imapCirc[idx]]].inputLabels[2*imapWire[idx]],
                    chained_gcs[circuitMapping[cur->chToCircId]].inputLabels[2*cur->chToWireId]); 
                cur->chOffsetIdx = offsetsIdx;
                ++offsetsIdx;
            }
        }
    }
    free(imapCirc);
    free(imapWire);

    if (chainingType == CHAINING_TYPE_STANDARD) {
        Instruction *cur;
        for (int i = 0; i < num_instructions; ++i) {
            cur = &(function->instructions.instr[i]);
            if (cur->type == CHAIN && cur->chFromCircId != 0) {
                offsets[offsetsIdx] = xorBlocks(
                    chained_gcs[circuitMapping[cur->chFromCircId]].outputMap[2*cur->chFromWireId],
                    chained_gcs[circuitMapping[cur->chToCircId]].inputLabels[2*cur->chToWireId]); 

                cur->chOffsetIdx = offsetsIdx;
                ++offsetsIdx;
            }
        }
    } else { /* CHAINING_TYPE_SIMD */
        /* Do chaining with SIMD. Here an entire component is chained with a single offset */

        /* gcChainingMap tracks which component is mapping to which component. 
         * In particular, maps gc-id \times gc-id to offset idx */
        int gcChainingMap[num_chained_gcs+1][num_chained_gcs+1];
        for (int i = 0; i < num_chained_gcs+1; i++)
            for (int j = 0; j < num_chained_gcs+1; j++)
                gcChainingMap[i][j] = -1;
        
        for (int i = 0; i < num_instructions; ++i) {
            cur = &(function->instructions.instr[i]);
            if (cur->type == CHAIN && cur->chFromCircId != 0) {
                if (gcChainingMap[cur->chFromCircId][cur->chToCircId] == -1) {
                /* add approparite offset to offsets */
                offsets[offsetsIdx] = xorBlocks(
                        chained_gcs[circuitMapping[cur->chFromCircId]].outputSIMDBlock,
                        chained_gcs[circuitMapping[cur->chToCircId]].inputSIMDBlock);
                gcChainingMap[cur->chFromCircId][cur->chToCircId] = offsetsIdx;
                
                cur->chOffsetIdx = offsetsIdx;
                ++offsetsIdx;

                } else {
                    /* reference block already in offsets */
                    cur->chOffsetIdx = gcChainingMap[cur->chFromCircId][cur->chToCircId];
                }
            }
        }
    }
    *noffsets = offsetsIdx;
    return SUCCESS;
}

void
garbler_offline(char *dir, ChainedGarbledCircuit* chained_gcs,
                const int num_eval_inputs, const int num_chained_gcs, ChainingType chainingType)
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

    start = current_time();
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
    end = current_time();
    fprintf(stderr, "garbler offline: %llu\n", end - start);

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
    struct state state;
    uint64_t start, end, _start, _end;
    ChainedGarbledCircuit *chained_gcs;
    FunctionSpec function;
    int *circuitMapping, noffsets = 0;
    block *randLabels = NULL, *offsets = NULL;
    char lblName[50];

    state_init(&state);
    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }
    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    /* start timing after socket connection */
    start = current_time();

    /* Load function from disk */
    _start = current_time();
    {
        /*load function allocates a bunch of memory for the function*/
        /*this is later freed by freeFunctionSpec*/
        if (load_function_via_json(function_path, &function) == FAILURE) {
            fprintf(stderr, "Could not load function %s\n", function_path);
            return FAILURE;
        }
    }
    _end = current_time();
    fprintf(stderr, "load function: %llu\n", _end - _start);

    /* Load everything else from disk */
    _start = current_time();
    {
        net_send(fd, &function.m, sizeof(int), 0);

        /* load chained garbled circuits from disk */
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        for (int i = 0; i < num_chained_gcs; ++i) {
            loadChainedGC(&chained_gcs[i], dir, i, true, chainingType);
        }

        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */
        randLabels = loadOTLabels(lblName);
        
        /* load ot info from disk */
    }
    _end = current_time();
    fprintf(stderr, "loading: %llu\n", _end - _start);

    /* Tell evaluator that we are done loading circuits so they can start timing */
    int empty = 0;
    net_send(fd, &empty, sizeof(int), 0);
    
    _start = current_time();
    {
        /* +1 because 0th component is inputComponent*/
        circuitMapping = malloc(sizeof(int) * (function.components.totComponents + 1));
        offsets = allocate_blocks(function.instructions.size);
        if (garbler_make_real_instructions(&function, chained_gcs,
                                           num_chained_gcs,
                                           circuitMapping,
                                           offsets,
                                           &noffsets,
                                           chainingType) == FAILURE) {
            fprintf(stderr, "Could not make instructions\n");
            return FAILURE;
        }
    }
    _end = current_time();
    fprintf(stderr, "make inst: %llu\n", _end - _start);

    /*main function; does core of work*/
    _start = current_time();
    garbler_go(fd, &function, dir, chained_gcs, randLabels, num_chained_gcs, circuitMapping,
               inputs, offsets, noffsets, chainingType);
    _end = current_time();
    fprintf(stderr, "garbler_go: %llu\n", _end - _start);
    
    free(circuitMapping);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], true);
    }
    free(chained_gcs);
    freeFunctionSpec(&function);

    end = current_time();
    if (tot_time) {
        *tot_time = end - start;
    }

    state_cleanup(&state);
    close(fd);
    close(serverfd);

    free(randLabels);

    return SUCCESS;
}
