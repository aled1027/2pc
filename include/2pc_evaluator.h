#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

void evaluator_classic_2pc(int *input, int *output,
        int num_garb_inputs, int num_eval_inputs,
        unsigned long *tot_time);

void evaluator_offline(ChainedGarbledCircuit *chained_gcs, 
        int num_eval_inputs, int num_chained_gcs);

void evaluator_online(int *eval_inputs, int num_eval_inputs, int num_garb_inputs,
        int num_chained_gcs, unsigned long *ot_time, unsigned long *tot_time);

void evaluator_evaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, int* circuitMapping,
        block **computedOutputMap);


int new_choice_reader(void *choices, int idx);
int new_msg_writer(void *array, int idx, void *msg, size_t msglength);

#endif
