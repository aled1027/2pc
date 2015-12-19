#include "2pc_garbler.h"

#include <assert.h> 
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h> // memalign

#include "arg.h"
#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"

#include "gc_comm.h"

void garbler_offline()
{
    /* sends ChainedGcs to evaluator and saves ChainedGcs to disk */
    int num_chained_gcs = NUM_GCS;
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    createGarbledCircuits(chained_gcs, num_chained_gcs);

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

    // send chained_gcs over
    for (int i=0; i<num_chained_gcs; i++) {
        chained_gc_comm_send(fd, &chained_gcs[i]);
        saveChainedGC(&chained_gcs[i], true);
    }

    free(chained_gcs);
    state_cleanup(&state);
    close(fd);
    close(serverfd);
}

int garbler_run(char* function_path) 
{
    // runs the garbler code
    // First, initializes and loads function, and then calls garbler_go which
    // runs the core of the garbler's code

    unsigned long *ot_time = malloc(sizeof(unsigned long));
    unsigned long tot_time_start, tot_time;
    tot_time_start = RDTSC;

    int *circuitMapping, *inputs, num_garb_inputs;

    FunctionSpec function;
    ChainedGarbledCircuit* chained_gcs; 

    num_garb_inputs = 128*10;
    inputs = malloc(sizeof(int) * num_garb_inputs);
    assert(inputs);
    for (int i=0; i<num_garb_inputs; i++) {
        inputs[i] = rand() % 2; 
    }

    chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);
    assert(chained_gcs);
    for (int i=0; i<NUM_GCS; i++) {
        loadChainedGC(&chained_gcs[i], i, true);
    }

    

    // load function allocates a bunch of memory for the function
    // this is later freed by freeFunctionSpec
    if (load_function_via_json(function_path, &function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    circuitMapping = (int*) malloc(sizeof(int) * function.num_components);
    assert(circuitMapping);
    garbler_make_real_instructions(&function, chained_gcs, NUM_GCS, circuitMapping);

    // main function; does core of work
    garbler_go(&function, chained_gcs, NUM_GCS, circuitMapping, inputs, ot_time);

    tot_time = RDTSC - tot_time_start;
    printf("%lu, %lu\n", *ot_time, tot_time);

    free(inputs);
    free(circuitMapping);
    freeChainedGcs(chained_gcs, NUM_GCS);
    free(chained_gcs);
    freeFunctionSpec(&function);

    return SUCCESS;
}

void 
garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        int* circuitMapping, int *inputs, unsigned long *ot_time) 
{
    /* primary role: send appropriate labels to evaluator and garbled circuits*/
    // hardcoding 22adder for now
    
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
    // TODO change these to memalign
    block* evalLabels = malloc(sizeof(block) * 2 * function->n * num_chained_gcs); 
    block* garbLabels = malloc(sizeof(block) * 2 * function->n * num_chained_gcs); 
    assert(evalLabels && garbLabels);

    // TODO is all of this copying necessary? Are we timing the garbler?
    // Could probably get around this.
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
    int num_eval_inputs = eval_p / 2, num_garb_inputs = garb_p;
    net_send(fd, &num_garb_inputs, sizeof(int), 0);

    // TODO this could be spedup
    if (num_garb_inputs > 0) {
        net_send(fd, garbLabels, sizeof(block)*num_garb_inputs, 0);
    }

    // send evaluator's labels
    unsigned long otStartTime;
    otStartTime = RDTSC;
    ot_np_send(&state, fd, evalLabels, sizeof(block), num_eval_inputs , 2,
               new_msg_reader, new_item_reader);
    *ot_time = RDTSC - otStartTime;

    // 5. send output map
    int final_circuit = circuitMapping[function->instructions.instr[ function->instructions.size - 1].evCircId];
    //printf("sending outputmap for circuit %d\n", final_circuit);
    int output_size = chained_gcs[final_circuit].gc.m;
    net_send(fd, &output_size, sizeof(int), 0); 
    net_send(fd, chained_gcs[final_circuit].outputMap, sizeof(block)*2*output_size, 0); 
    
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

    buf_size1 = writeInstructionsToBuffer(&function->instructions, buffer1);
    buf_size2 = writeInputMappingToBuffer(&function->input_mapping, buffer2);

    net_send(fd, &buf_size1, sizeof(buf_size1), 0);
    net_send(fd, &buf_size2, sizeof(buf_size2), 0);
    net_send(fd, buffer1, buf_size1, 0);
    net_send(fd, buffer2, buf_size2, 0);

    free(buffer1);
    free(buffer2);
}

