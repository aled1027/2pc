#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

int garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
int garbler_make_real_instructions(FunctionSpec *function, CircuitType *saved_gcs_type, 
        int num_saved_gcs, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
int createInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs);

// TO MAKE:
int load_function_spec(char* path);
int map_to_real_function(FunctionSpec* function, GCsMetadata* gcs_metadata);
int send_gcs();

#endif
