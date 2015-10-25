#include "2pc_garbler.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include "arg.h"
#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"

#include "gc_comm.h"

int 
garbler_send_gcs(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) 
{
    /* sends chained gcs over to evaluator */

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
    }

    // clean up
    state_cleanup(&state);
    close(fd);
    close(serverfd);

    return SUCCESS;
}

int 
garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) 
{
    /* primary role: fill function function with information provided by chained_gcs */
    char* function_path = "functions/22Adder.json";

    if (load_function_via_json(function_path, function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    CircuitType *saved_gcs_type = malloc(sizeof(CircuitType) * num_chained_gcs);
    for (int i =0; i<num_chained_gcs; i++) {
        saved_gcs_type[i] = ADDER22;
    }

    // make instructions based on these circuits. Instructions are saved inside of function.
    garbler_make_real_instructions(function, saved_gcs_type, num_chained_gcs, chained_gcs, num_chained_gcs);

    return SUCCESS;
}

void 
garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        int *inputs) 
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
    // 2. send instructions, input_mapping, relevant info from function

    send_instructions_and_input_mapping(function, fd);

    // ---- unfinished past here ---


    // 3. send labels for evaluator's inputs
    //int num_gcs = 2;
    //block* evalLabels = malloc(sizeof(block) * 2 * 2 * num_gcs); 
    //memcpy(evalLabels, chained_gcs[0].inputLabels, sizeof(block)*2*2);
    //memcpy(&evalLabels[4], chained_gcs[1].inputLabels, sizeof(block)*2*2);

    //ot_np_send(&state, fd, evalLabels, sizeof(block), 4, 2,
    //           new_msg_reader, new_item_reader);

    //// 4. send labels for garbler's inputs.
    ////net_send(fd, outputMap, sizeof(block) * 2 * gc.m, 0);
    //
    //// 5. send output map
    //// chained_gcs[2].outputMap;  // check FunctionSpec for which circuit id is the output circuit. 
    //// it should say in the final instruction in 
    //// function.instructions.instr[function.instruction.instr.size -1 ]
    //// send that shit.
    //
    //// 6. clean up
    //state_cleanup(&state);
    //close(fd);
    //close(serverfd);
   


    // send things over to evaluator
}

int
garbler_make_real_instructions(FunctionSpec *function, CircuitType *saved_gcs_type, int num_saved_gcs,
        ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) 
{
    // suppose we have N garbled circuits total
    // 6 ADDER22 and 4 ADDER23
    bool* is_circuit_used = malloc(sizeof(bool) * num_saved_gcs);
    assert(is_circuit_used);
    memset(is_circuit_used, 0, sizeof(bool) * num_saved_gcs);

    // create mapping functionCircuitToRealCircuit: FunctionCircuit int id -> Real Circuit id
    int num_components = function->num_components;
    int* functionCircuitToRealCircuit = malloc(sizeof(int) * num_components);

    // loop over type of circuit
    for (int i=0; i<num_components; i++) {
        CircuitType type = function->components[i].circuit_type;
        int num = function->components[i].num;
        int* circuit_ids = function->components[i].circuit_ids; // = {0,1,2};
        
        // j indexes circuit_ids, k indexes is_circuit_used and saved_gcs
        int j = 0, k = 0;

        // find an available circuit
        for (int k=0; k<num_saved_gcs; k++) {
            if (!is_circuit_used[k] && saved_gcs_type[k] == type) {
                // map it, and increment j
                functionCircuitToRealCircuit[circuit_ids[j]] = k;
                is_circuit_used[k] = true;
                j++;
            }
            if (num == j) break;
        }
        if (j < num) {
            fprintf(stderr, "Not enough circuits of type %d available\n", type);
            return FAILURE;
        }
    }
    
    // Apply FunctionCircuitToRealCircuit to update instructions to reflect real circuit ids
    int num_instructions = function->instructions.size;
    Instruction *cur;
    for (int i=0; i<num_instructions; i++) {
        cur = &(function->instructions.instr[i]);
        switch(cur->type) {
            case EVAL:
                cur->evCircId = functionCircuitToRealCircuit[cur->evCircId];
                break;
            case CHAIN:
                cur->chFromCircId = functionCircuitToRealCircuit[cur->chFromCircId];
                cur->chToCircId = functionCircuitToRealCircuit[cur->chToCircId];

                cur->chOffset = xorBlocks(
                        chained_gcs[cur->chFromCircId].outputMap[cur->chFromWireId],
                        chained_gcs[cur->chToCircId].inputLabels[2*cur->chToWireId]); 
                // pretty sure this 2 is right
                break;
            default:
                fprintf(stderr, "Instruction type not recognized\n");
        }
    }
    //print_instructions(&function->instructions);
    return SUCCESS;
}


int 
createInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs) 
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
}


