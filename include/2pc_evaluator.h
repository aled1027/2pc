#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

void evaluator_online(int *eval_inputs, int num_eval_inputs, int num_chained_gcs, 
        unsigned long *ot_time, unsigned long *tot_time);

void evaluator_evaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, block* outputmap, int* output, int* circuitMapping);

void evaluator_offline(ChainedGarbledCircuit *chained_gcs, int num_chained_gcs);
int new_choice_reader(void *choices, int idx);
int new_msg_writer(void *array, int idx, void *msg, size_t msglength);

#endif
