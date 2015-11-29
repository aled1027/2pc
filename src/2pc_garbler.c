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

    // clean up
    state_cleanup(&state);
    close(fd);
    close(serverfd);
}

int garbler_run(char* function_path) 
{
    FunctionSpec function;
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);

    for (int i=0; i<NUM_GCS; i++) {
        loadChainedGC(&chained_gcs[i], i, true);
    }
    int* circuitMapping;

    int inputs[2]; // TODO no inputs for aes, so this can be ignored
    //inputs[0] = rand() % 2;
    //inputs[1] = rand() % 2;
    //printf("inputs: (%d,%d)\n", inputs[0], inputs[1]);

    garbler_init(&function, chained_gcs, NUM_GCS, &circuitMapping, function_path);
    garbler_go(&function, chained_gcs, NUM_GCS, circuitMapping, inputs);

    freeFunctionSpec(&function);

    return SUCCESS;
}

int 
garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int **circuitMapping, char* function_path) 
{
    /* primary role: fill function function with information provided by chained_gcs */

    if (load_function_via_json(function_path, function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    // make instructions based on these circuits. Instructions are saved inside of function.
    *circuitMapping = (int*) malloc(sizeof(int) * function->num_components);
    garbler_make_real_instructions(function, chained_gcs, num_chained_gcs, *circuitMapping);

    return SUCCESS;
}

void 
garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        int* circuitMapping, int *inputs) 
{
    /* primary role: send appropriate labels to evaluator and garbled circuits*/
    // hardcoding 22adder for now
    
    // 1. setup connection
    //int serverfd, fd, res;
    //struct state state;
    //state_init(&state);

    //if ((serverfd = net_init_server(HOST, PORT)) == FAILURE) {
        //perror("net_init_server");
        //exit(EXIT_FAILURE);
    //}

    //if ((fd = net_server_accept(serverfd)) == FAILURE) {
        //perror("net_server_accept");
        //exit(EXIT_FAILURE);
    //}
    // 2. send instructions, input_mapping
    //send_instructions_and_input_mapping(function, fd);

    // 3. send circuitMapping
    //net_send(fd, &function->num_components, sizeof(int), 0);
    //net_send(fd, circuitMapping, sizeof(int) * function->num_components, 0);

    // 4. send labels
    // upper bounding memory with n
    block* evalLabels = malloc(sizeof(block) * 2 * function->n * num_chained_gcs); 
    block* garbLabels = malloc(sizeof(block) * 2 * function->n * num_chained_gcs); 
    //assert(evalLabels && garbLabels);

    InputMapping imap = function->input_mapping;
    int eval_p = 0, garb_p = 0;
    for (int i=0; i<imap.size; i++) {
        if (imap.inputter[i] == PERSON_GARBLER) {
            // check input here.
            assert(inputs[i] == 0 || inputs[i] == 1);
            memcpy(&garbLabels[garb_p], 
                    &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i] + inputs[i]], 
                    sizeof(block));
            garb_p++;
        } else if (imap.inputter[i] == PERSON_EVALUATOR) {
            memcpy(&evalLabels[eval_p], 
                    &chained_gcs[circuitMapping[imap.gc_id[i]]].inputLabels[2*imap.wire_id[i]], 
                    sizeof(block)*2);
            eval_p+=2;
        } 
    }
    //int num_eval_inputs = eval_p / 2, num_garb_inputs = garb_p;
    //net_send(fd, &num_garb_inputs, sizeof(int), 0);
    //if (num_garb_inputs > 0) {
        //net_send(fd, garbLabels, sizeof(block)*num_garb_inputs, 0);
    //}

    // send evaluator's labels
    //ot_np_send(&state, fd, evalLabels, sizeof(block), num_eval_inputs , 2,
               //new_msg_reader, new_item_reader);

    // 5. send output map
    //int final_circuit = circuitMapping[function->instructions.instr[ function->instructions.size - 1].evCircId];
    //int output_size = chained_gcs[final_circuit].gc.m;
    //net_send(fd, &output_size, sizeof(int), 0); 
    //net_send(fd, chained_gcs[final_circuit].outputMap, sizeof(block)*2*output_size, 0); 
    
    // 6. clean up
    //state_cleanup(&state);
    //close(fd);
    //close(serverfd);

    // 7. adding this. remove later. TODO
    printf("break it\n");
    block* labs = memalign(128, sizeof(block)*256);
    int outputs[128];
    for (int i=0; i<256; i++) {
        int d = rand() % 2;
        labs[i] = evalLabels[2*i + d];
    } 

    block* cmap = memalign(128, sizeof(block) * 128);
    evaluate(&chained_gcs[0].gc, labs, cmap);
	mapOutputs(chained_gcs[0].outputMap, cmap, outputs, 128);
    printf("output: (");
    for (int i=0; i<128; i++) {
        printf("%d, ", outputs[i]);
    }
    printf(")\n");
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
                chained_gcs[circuitMapping[cur->chFromCircId]].outputMap[cur->chFromWireId],
                chained_gcs[circuitMapping[cur->chToCircId]].inputLabels[2*cur->chToWireId]); 
        }
    }
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

    Instructions is;
    readBufferIntoInstructions(&is, buffer1);
}

int 
hardcodeInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs) 
{
    instr[0].type = EVAL;
    instr[0].evCircId = 0;
    
    instr[1].type = EVAL;
    instr[1].evCircId = 1;

    instr[2].type = CHAIN;
    instr[2].chFromCircId = 0;
    instr[2].chFromWireId = 0;
    instr[2].chToCircId = 2;
    instr[2].chToWireId = 0;
    instr[2].chOffset = xorBlocks(chained_gcs[0].outputMap[0], chained_gcs[2].inputLabels[0]);

    instr[3].type = CHAIN;
    instr[3].chFromCircId = 1;
    instr[3].chFromWireId = 0;
    instr[3].chToCircId = 2;
    instr[3].chToWireId = 1;
    instr[3].chOffset = xorBlocks(chained_gcs[1].outputMap[0], chained_gcs[2].inputLabels[2]);

    instr[4].type = EVAL;
    instr[4].evCircId = 2;
    return SUCCESS;
}


