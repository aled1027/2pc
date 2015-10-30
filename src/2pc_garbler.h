#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

int run_garbler();
int garbler_offline(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
int garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, 
        int** circuitMapping);
void garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, 
        int* circuitMapping, int *inputs);
int garbler_make_real_instructions(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int* circuitMapping);
int hardcodeInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs);
void send_instructions_and_input_mapping(FunctionSpec *function, int fd);

// for OT
void *new_msg_reader(void *msgs, int idx);
void *new_item_reader(void *item, int idx, ssize_t *mlen);

#endif
