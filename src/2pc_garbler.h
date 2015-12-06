#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

// could make: garbler_setup_labels, garbler_verb_instructions, etc.

// garbler_offline
// garbler_run
    // garbler_init
    // garbler_go

void garbler_offline();
int garbler_run(char* function_path);
void garbler_go(FunctionSpec* function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, 
        int* circuitMapping, int *inputs);
int garbler_make_real_instructions(FunctionSpec *function, 
        ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int* circuitMapping);

void send_instructions_and_input_mapping(FunctionSpec *function, int fd);

// for OT
void *new_msg_reader(void *msgs, int idx);
void *new_item_reader(void *item, int idx, ssize_t *mlen);

#endif
