#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

// core functions
int garbler_send_gcs(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
int garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
void garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, 
        int *inputs);

// helper functions
int garbler_make_real_instructions(FunctionSpec *function, CircuitType *saved_gcs_type, 
        int num_saved_gcs, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
void *new_msg_reader(void *msgs, int idx);
void *new_item_reader(void *item, int idx, ssize_t *mlen);

// other functions
int createInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs);
void send_instructions_and_input_mapping(FunctionSpec *function, int fd);

// TO MAKE:
//int load_function_spec(char* path);
//int map_to_real_function(FunctionSpec* function, GCsMetadata* gcs_metadata);
//int send_gcs();
#endif
