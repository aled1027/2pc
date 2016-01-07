#include "2pc_garbler.h"

#include <assert.h> 
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>

#include "arg.h"
#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"
#include "utils.h"

#include "gc_comm.h"

void
garbler_offline(ChainedGarbledCircuit* chained_gcs, int num_eval_inputs,
                int num_chained_gcs)
{
    /* sends ChainedGcs to evaluator and saves ChainedGcs to disk */
    // setup connection
    int serverfd, fd, res;
    struct state state;
    state_init(&state);

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }

    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    /* send GCs */
    for (int i = 0; i < num_chained_gcs; i++) {
        chained_gc_comm_send(fd, &chained_gcs[i]);
        saveChainedGC(&chained_gcs[i], true);
    }

    /* pre-processing OT using random labels */
    {
        block *evalLabels;

        char lblName[50];

        (void) sprintf(lblName, "%s/%s", GARBLER_DIR, "lbl");

        (void) posix_memalign((void **) &evalLabels, 128,
                              sizeof(block) * 2 * num_eval_inputs);
        if (evalLabels == NULL) {
            perror("posix_memalign");
            exit(EXIT_FAILURE);
        }
            
        ot_np_send(&state, fd, evalLabels, sizeof(block), num_eval_inputs, 2,
                   new_msg_reader, new_item_reader);
        saveOTLabels(lblName, evalLabels, num_eval_inputs, true);

        free(evalLabels);
    }

    close(fd);
    close(serverfd);
    state_cleanup(&state);
}

int
garbler_online(char* function_path, int *inputs, int num_garb_inputs,
               int num_chained_gcs, unsigned long *ot_time,
               unsigned long *tot_time) 
{
    // tot_time and ot_time should be malloced before call
    assert(ot_time && tot_time);

    // runs the garbler code
    // First, initializes and loads function, and then calls garbler_go which
    // runs the core of the garbler's code
    unsigned long tot_time_start = RDTSC;

    int *circuitMapping; 

    FunctionSpec function;
    ChainedGarbledCircuit* chained_gcs; 

    chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    assert(chained_gcs);
    for (int i=0; i<num_chained_gcs; i++) {
        loadChainedGC(&chained_gcs[i], i, true);
        //printf("loaded circuit of type: %d\n", chained_gcs[i].type);
    }

    // load function allocates a bunch of memory for the function
    // this is later freed by freeFunctionSpec
    if (load_function_via_json(function_path, &function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        free(chained_gcs);
        return FAILURE;
    }
    assert(num_garb_inputs == function.num_garbler_inputs);

    circuitMapping = (int*) malloc(sizeof(int) * function.num_components);
    assert(circuitMapping);
    garbler_make_real_instructions(&function, chained_gcs, num_chained_gcs, circuitMapping);

    // main function; does core of work
    garbler_go(&function, chained_gcs, NUM_GCS, circuitMapping, inputs, ot_time);

    *tot_time = RDTSC - tot_time_start;

    free(inputs);
    free(circuitMapping);
    freeChainedGcs(chained_gcs, num_chained_gcs);
    free(chained_gcs);
    freeFunctionSpec(&function);

    return SUCCESS;
}

void 
garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        int* circuitMapping, int *inputs, unsigned long *ot_time) 
{
    /* primary role: send appropriate labels to evaluator and garbled circuits*/
    
    // 1. setup connection
    int serverfd, fd, res;
    struct state state;
    state_init(&state);

    if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        perror("net_init_server");
        exit(EXIT_FAILURE);
    }

    if ((fd = net_server_accept(serverfd)) == FAILURE) {
        perror("net_server_accept");
        exit(EXIT_FAILURE);
    }

    // 2. send instructions, input_mapping
    send_instructions_and_input_mapping(function, fd);

    // 3. send circuitMapping
    net_send(fd, &function->num_components, sizeof(int), 0);
    net_send(fd, circuitMapping, sizeof(int) * function->num_components, 0);

    // 4. send labels
    // upper bounding memory with n
    block *evalLabels, *garbLabels;
    evalLabels = allocate_blocks(2 * function->n * num_chained_gcs);
    garbLabels = allocate_blocks(2 * function->n * num_chained_gcs);

    // TODO could probably figure out a way to avoid copying, but easiest and fast enough for now.
    InputMapping imap = function->input_mapping;
    int eval_p = 0, garb_p = 0; // counters for looping over garbler and evaluator's structures
    for (int i=0; i<imap.size; i++) {
        if (imap.inputter[i] == PERSON_GARBLER) {
            assert(inputs[garb_p] == 0 || inputs[garb_p] == 1);
            // copy into garbLabels from the circuit's inputLabels
            memcpy(&garbLabels[garb_p], 
                    &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i] + inputs[garb_p]], 
                    sizeof(block));
            garb_p++;
        } else if (imap.inputter[i] == PERSON_EVALUATOR) {
            memcpy(&evalLabels[eval_p], 
                    &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i]], 
                    sizeof(block)*2);
            eval_p+=2;
        } 
    }
    int num_eval_inputs = eval_p / 2;
    int num_garb_inputs = garb_p;

    net_send(fd, &num_garb_inputs, sizeof(int), 0);
    if (num_garb_inputs > 0) {
        net_send(fd, garbLabels, sizeof(block) * num_garb_inputs, 0);
    }

    /* OT correction */
    {
        int *corrections;
        block *randLabels;
        char lblName[50];

        (void) sprintf(lblName, "%s/%s", GARBLER_DIR, "lbl");

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
                evalLabels[2 * i + 1] = xorBlocks(evalLabels[2 * i + 1],
                                                  randLabels[2 * i + 1]);
                break;
            case 1:
                evalLabels[2 * i] = xorBlocks(evalLabels[2 * i],
                                              randLabels[2 * i + 1]);
                evalLabels[2 * i + 1] = xorBlocks(evalLabels[2 * i + 1],
                                                  randLabels[2 * i]);
                break;
            default:
                assert(false);
                exit(EXIT_FAILURE);
            }
        }

        net_send(fd, evalLabels, sizeof(block) * 2 * num_eval_inputs, 0);

        free(corrections);
        free(randLabels);
    }

    // 5a. send "output" 
    //  output is from the json, and tells which components/wires are used for outputs
    // note that size is not size of the output, but length of the arrays in output
    net_send(fd, &function->output.size, sizeof(int), 0); 
    net_send(fd, function->output.gc_id, sizeof(int)*function->output.size, 0);
    net_send(fd, function->output.start_wire_idx, sizeof(int)*function->output.size, 0);
    net_send(fd, function->output.end_wire_idx, sizeof(int)*function->output.size, 0);

    // 5b. send outputMap
    {
        block *outputmap;

        outputmap = allocate_blocks(2 * function->m);
        int p_outputmap = 0;

        for (int j=0; j<function->output.size; j++) {
            int gc_id = circuitMapping[function->output.gc_id[j]];
            int start_wire_idx = function->output.start_wire_idx[j];
            int end_wire_idx = function->output.end_wire_idx[j];
            // add 1 because values are inclusive
            int dist = end_wire_idx - start_wire_idx + 1; 
            memcpy(&outputmap[p_outputmap],
                   &chained_gcs[gc_id].outputMap[start_wire_idx],
                   sizeof(block)*2*dist);
            p_outputmap += (dist*2);
        }
        assert(p_outputmap == 2 * function->m);
        net_send(fd, &function->m, sizeof(int), 0); 
        net_send(fd, outputmap, sizeof(block)*2*function->m, 0);

        free(outputmap);
    }

    // 6. clean up
    free(evalLabels);
    free(garbLabels);
    state_cleanup(&state);
    close(fd);
    close(serverfd);
}

int
garbler_make_real_instructions(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int* circuitMapping) 
{
    /* primary role: populate circuitMapping
     * circuitMapping maps instruction-gc-ids --> saved-gc-ids 
     */

    bool* is_circuit_used;
    int num_component_types;
        
    is_circuit_used = malloc(sizeof(bool) * num_chained_gcs);

    assert(is_circuit_used);
    memset(is_circuit_used, 0, sizeof(bool) * num_chained_gcs);
    num_component_types = function->num_component_types;

    // loop over type of circuit
    for (int i=0; i<num_component_types; i++) {
        CircuitType needed_type = function->components[i].circuit_type;
        int num_needed = function->components[i].num;
        int* circuit_ids = function->components[i].circuit_ids; // = {0,1,2};
        
        // j indexes circuit_ids, k indexes is_circuit_used and saved_gcs
        int j = 0, k = 0;

        // find an available circuit
        for (int k=0; k<num_chained_gcs; k++) {
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
    Instruction *cur;
    for (int i=0; i<num_instructions; i++) {
        cur = &(function->instructions.instr[i]);
        if (cur->type == CHAIN) {
            cur->chOffset = xorBlocks(
                chained_gcs[circuitMapping[cur->chFromCircId]].outputMap[2*cur->chFromWireId],
                chained_gcs[circuitMapping[cur->chToCircId]].inputLabels[2*cur->chToWireId]); 
        }
    }
    free(is_circuit_used);
    return SUCCESS;
}

void *
new_msg_reader(void *msgs, int idx)
{
    block *m = (block *) msgs;
    return &m[2 * idx];
}

void *
new_item_reader(void *item, int idx, ssize_t *mlen)
{
    block *a = (block *) item;
    *mlen = sizeof(block);
    return &a[idx];
}

void 
send_instructions_and_input_mapping(FunctionSpec *function, int fd) 
{
    char *buffer1, *buffer2;
    size_t buf_size1, buf_size2;
    buffer1 = malloc(MAX_BUF_SIZE);
    buffer2 = malloc(MAX_BUF_SIZE);
    assert(buffer1 && buffer2);

    buf_size1 = writeInstructionsToBuffer(&function->instructions, buffer1);
    buf_size2 = writeInputMappingToBuffer(&function->input_mapping, buffer2);

    net_send(fd, &buf_size1, sizeof(buf_size1), 0);
    net_send(fd, &buf_size2, sizeof(buf_size2), 0);
    net_send(fd, buffer1, buf_size1, 0);
    net_send(fd, buffer2, buf_size2, 0);

    free(buffer1);
    free(buffer2);
}

