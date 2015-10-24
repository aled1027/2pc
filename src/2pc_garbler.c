#include "2pc_garbler.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

int 
garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) 
{
    char* function_path = "functions/22Adder.json";

    if (load_function_via_json(function_path, function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    CircuitType saved_gcs_type[] = {
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER23,
        ADDER23,
        ADDER23,
        ADDER23};
    int num_saved_gcs = 10;
    garbler_make_real_instructions(function, saved_gcs_type, num_saved_gcs, chained_gcs, num_chained_gcs);
    return SUCCESS;
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



