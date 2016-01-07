#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

void
garbler_offline(ChainedGarbledCircuit* chained_gcs, int num_eval_inputs,
                int num_chained_gcs);

int garbler_online(char* function_path, int *inputs, int num_garb_inputs, int num_chained_gcs, 
        unsigned long *ot_time, unsigned long *tot_time);

void garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, 
        int* circuitMapping, int *inputs, unsigned long *ot_time);
int garbler_make_real_instructions(FunctionSpec *function, 
        ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int* circuitMapping);

void send_instructions_and_input_mapping(FunctionSpec *function, int fd);

// for OT
void *new_msg_reader(void *msgs, int idx);
void *new_item_reader(void *item, int idx, ssize_t *mlen);

#endif
