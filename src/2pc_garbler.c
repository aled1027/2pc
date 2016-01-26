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
                    block *output_map, int num_garb_inputs, int num_eval_inputs,
                    const int *inputs, unsigned long *tot_time)
{
    /* Does 2PC in the classic way, without components. */
    int serverfd, fd;
    struct state state;
    unsigned long start, end;

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

    start = RDTSC;

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
    block *garb_labels, *eval_labels;

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

    end = RDTSC;
    *tot_time = end - start;
}

static void
sendInstructions(const Instructions* instructions, int fd)
{
    char *buffer;
    size_t buf_size;

    buffer = malloc(instructionBufferSize(instructions));
    buf_size = writeInstructionsToBuffer(instructions, buffer);

    net_send(fd, &buf_size, sizeof(buf_size), 0);
    net_send(fd, buffer, buf_size, 0);

    free(buffer);
}

static void
garbler_go(int fd, const FunctionSpec *function, const char *dir,
           const ChainedGarbledCircuit *chained_gcs,
           int num_chained_gcs, const int *circuitMapping, const int *inputs)
{
    /* primary role: send appropriate labels to evaluator and garbled circuits*/
    InputMapping imap = function->input_mapping;
    int num_eval_inputs = function->num_eval_inputs;
    int num_garb_inputs = function->num_garb_inputs;
    block *garbLabels, *evalLabels;
    unsigned long _start, _end;

    _start = RDTSC;
    {
        sendInstructions(&function->instructions, fd);
    }
    _end = RDTSC;
    fprintf(stderr, "send inst: %lu\n", _end - _start);

    // 3. send circuitMapping
    _start = RDTSC;
    {
        int circuitMappingSize = function->components.totComponents + 1;
        net_send(fd, &circuitMappingSize, sizeof(int), 0);
        net_send(fd, circuitMapping, sizeof(int) * circuitMappingSize, 0);
    }
    _end = RDTSC;
    fprintf(stderr, "send circmap: %lu\n", _end - _start);

    // 4. send labels
    _start = RDTSC;
    {
        evalLabels = allocate_blocks(2 * num_eval_inputs);
        garbLabels = allocate_blocks(num_garb_inputs);
        bool *usedGarbInputIdx = calloc(sizeof(bool), num_garb_inputs);
        bool *usedEvalInputIdx = calloc(sizeof(bool), num_eval_inputs);

        for (int i = 0; i < imap.size; i++) {
            int input_idx = imap.input_idx[i];
            switch(imap.inputter[i]) {
            case PERSON_GARBLER:
                if (usedGarbInputIdx[input_idx] == false) {
                    assert(inputs[input_idx] == 0 || inputs[input_idx] == 1);
                    memcpy(&garbLabels[input_idx], 
                           &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i] + inputs[input_idx]], 
                           sizeof(block));
                    usedGarbInputIdx[input_idx] = true;
                }
                break;
            case PERSON_EVALUATOR:
                if (usedEvalInputIdx[input_idx] == false) {
                    memcpy(&evalLabels[2*input_idx], 
                           &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i]], 
                           sizeof(block)*2);
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

/*         extract_labels_cgc(garbLabels, evalLabels, chained_gcs, circuitMapping, */
/*                            &imap, inputs); */

        net_send(fd, &num_garb_inputs, sizeof(int), 0);
        if (num_garb_inputs > 0) {
            net_send(fd, garbLabels, sizeof(block) * num_garb_inputs, 0);
        }
    }
    _end = RDTSC;
    fprintf(stderr, "send labels: %lu\n", _end - _start);

    /* Send evaluator's labels via OT correct */
    /* OT correction */
    _start = RDTSC;
    if (num_eval_inputs > 0) {
        int *corrections;
        block *randLabels;
        char lblName[50];

        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */

        randLabels = loadOTLabels(lblName);
        if (randLabels == NULL) {
            perror("loadOTLabels");
            exit(EXIT_FAILURE);
        }

        corrections = malloc(sizeof(int) * num_eval_inputs);
        if (corrections == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        net_recv(fd, corrections, sizeof(int) * num_eval_inputs, 0);

        for (int i = 0; i < num_eval_inputs; ++i) {
            switch (corrections[i]) {
            case 0:
                evalLabels[2 * i] = xorBlocks(evalLabels[2 * i],
                                              randLabels[2 * i]);
                evalLabels[(2 * i) + 1] = xorBlocks(evalLabels[(2 * i) + 1],
                                                    randLabels[(2 * i) + 1]);
                break;
            case 1:
                evalLabels[2 * i] = xorBlocks(evalLabels[2 * i],
                                              randLabels[(2 * i) + 1]);
                evalLabels[(2 * i) + 1] = xorBlocks(evalLabels[(2 * i) + 1],
                                                    randLabels[2 * i]);
                break;
            default:
                assert(false);
                abort();
            }
        }

        net_send(fd, evalLabels, sizeof(block) * 2 * num_eval_inputs, 0);
        free(corrections);
        free(randLabels);
    }
    _end = RDTSC;
    fprintf(stderr, "ot correction: %lu\n", _end - _start);

    _start = RDTSC;
    // 5a. send "output" 
    // output is from the json, and tells which components/wires are used for outputs
    // note that size is not size of the output, but length of the arrays in output
    net_send(fd, &function->output.size, sizeof(int), 0); 
    net_send(fd, function->output.gc_id, sizeof(int)*function->output.size, 0);
    net_send(fd, function->output.start_wire_idx, sizeof(int)*function->output.size, 0);
    net_send(fd, function->output.end_wire_idx, sizeof(int)*function->output.size, 0);

    // 5b. send outputMap
    {
        block *outputmap;
        int p_outputmap = 0;

        outputmap = allocate_blocks(2 * function->m);

        for (int j = 0; j < function->output.size; j++) {
            int gc_id = circuitMapping[function->output.gc_id[j]];
            int start_wire_idx = function->output.start_wire_idx[j];
            int end_wire_idx = function->output.end_wire_idx[j];
            // add 1 because values are inclusive
            int dist = end_wire_idx - start_wire_idx + 1; 
            printf("copying gc_id %d for outputmap\n", gc_id);
            memcpy(&outputmap[p_outputmap],
                   &chained_gcs[gc_id].outputMap[start_wire_idx],
                   sizeof(block)*2*dist);
            p_outputmap += (dist*2);
        }
        assert(p_outputmap == 2 * function->m);
        net_send(fd, &function->m, sizeof(int), 0); 
        net_send(fd, outputmap, sizeof(block)*2*function->m, 0);

        /* print_blocks("chained_gcs[1].inputLabels", &chained_gcs[1].inputLabels[16], 4); */

        free(outputmap);
    }
    _end = RDTSC;
    fprintf(stderr, "send output/outputmap: %lu\n", _end - _start);

    // 6. clean up
    if (num_garb_inputs > 0)
        free(garbLabels);
    if (num_eval_inputs > 0)
        free(evalLabels);
}

static int
garbler_make_real_instructions(FunctionSpec *function,
                               ChainedGarbledCircuit *chained_gcs,
                               int num_chained_gcs, int *circuitMapping) 
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
    
    // set the chaining offsets.
    int num_instructions = function->instructions.size;

    // size of n
    /* Did not know what to name these arrays. They are used for filling
     * in the offsets for InputComponent */
    int *imapCirc = allocate_ints(function->n);
    int *imapWire = allocate_ints(function->n);
    for (int i = 0; i < function->n; ++i)
        imapCirc[i] = -1;

    Instruction *cur;
    for (int i = 0; i < num_instructions; ++i) {
        cur = &(function->instructions.instr[i]);
        if (cur->type == CHAIN) {
            if (cur->chFromCircId == 0) {
                int idx = cur->chFromWireId;

                if (imapCirc[idx] == -1) {
                    /* this input has not been used */
                    imapCirc[idx] = cur->chToCircId;
                    imapWire[idx] = cur->chToWireId;
                    cur->chOffset = zero_block();
                } else {
                    /* this idx has been used */
                    cur->chOffset = xorBlocks(
                        chained_gcs[circuitMapping[imapCirc[idx]]].inputLabels[2*imapWire[idx]],
                        chained_gcs[circuitMapping[cur->chToCircId]].inputLabels[2*cur->chToWireId]); 
                }
            } else {
                cur->chOffset = xorBlocks(
                    chained_gcs[circuitMapping[cur->chFromCircId]].outputMap[2*cur->chFromWireId],
                    chained_gcs[circuitMapping[cur->chToCircId]].inputLabels[2*cur->chToWireId]); 
            }
        }
    }
    free(is_circuit_used);
    free(imapCirc);
    free(imapWire);
    return SUCCESS;
}

void
garbler_offline(char *dir, ChainedGarbledCircuit* chained_gcs,
                int num_eval_labels, int num_chained_gcs)
{
    int serverfd, fd;
    struct state state;
    unsigned long start, end;

    state_init(&state);

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }

    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    start = RDTSC;
    {
        for (int i = 0; i < num_chained_gcs; i++) {
            chained_gc_comm_send(fd, &chained_gcs[i]);
            saveChainedGC(&chained_gcs[i], dir, true);
        }

        /* pre-processing OT using random labels */
        if (num_eval_labels > 0) {
            block *evalLabels;
            char lblName[50];

            (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */

            evalLabels = allocate_blocks(2 * num_eval_labels);
            for (int i = 0; i < 2 * num_eval_labels; ++i) {
                evalLabels[i] = randomBlock();
            }
            
            ot_np_send(&state, fd, evalLabels, sizeof(block), num_eval_labels, 2,
                       new_msg_reader, new_item_reader);
            saveOTLabels(lblName, evalLabels, num_eval_labels, true);

            free(evalLabels);
        }
    }
    end = RDTSC;
    fprintf(stderr, "garbler offline: %lu\n", end - start);

    close(fd);
    close(serverfd);
    state_cleanup(&state);
}

int
garbler_online(char *function_path, char *dir, int *inputs, int num_garb_inputs,
               int num_chained_gcs, unsigned long *tot_time) 
{
    /*runs the garbler code
     * First, initializes and loads function, and then calls garbler_go which
     * runs the core of the garbler's code
     */

    int serverfd, fd;
    struct state state;
    unsigned long start, end, _start, _end;
    ChainedGarbledCircuit *chained_gcs;
    FunctionSpec function;
    int *circuitMapping;

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
    start = RDTSC;

    _start = RDTSC;
    {
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        for (int i = 0; i < num_chained_gcs; ++i) {
            loadChainedGC(&chained_gcs[i], dir, i, true);
        }
    }
    _end = RDTSC;
    fprintf(stderr, "load gcs: %lu\n", _end - _start);

    /*load function allocates a bunch of memory for the function*/
    /*this is later freed by freeFunctionSpec*/
    _start = RDTSC;
    {
        if (load_function_via_json(function_path, &function) == FAILURE) {
            fprintf(stderr, "Could not load function %s\n", function_path);
            return FAILURE;
        }
    }
    _end = RDTSC;
    fprintf(stderr, "load function: %lu\n", _end - _start);

    _start = RDTSC;
    {
        // + 1 because 0th component is inputComponent
        circuitMapping = malloc(sizeof(int) * (function.components.totComponents + 1));
        if (garbler_make_real_instructions(&function, chained_gcs,
                                           num_chained_gcs,
                                           circuitMapping) == FAILURE) {
            fprintf(stderr, "Could not make instructions\n");
            return FAILURE;
        }
    }
    _end = RDTSC;
    fprintf(stderr, "make inst: %lu\n", _end - _start);

    /*main function; does core of work*/
    _start = RDTSC;
    garbler_go(fd, &function, dir, chained_gcs, num_chained_gcs, circuitMapping,
               inputs);
    _end = RDTSC;
    fprintf(stderr, "garbler_go: %lu\n", _end - _start);
    
    free(circuitMapping);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], true);
    }
    free(chained_gcs);
    freeFunctionSpec(&function);

    end = RDTSC;
    if (tot_time) {
        *tot_time = end - start;
    }

    state_cleanup(&state);
    close(fd);
    close(serverfd);

    return SUCCESS;
}
