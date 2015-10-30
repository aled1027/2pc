#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"


void evaluator_receive_gcs(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);
int chainedEvaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, block* outputmap, int* output, int* circuitMapping);
int evaluator_go(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int* eval_inputs, int num_eval_inputs);
int new_choice_reader(void *choices, int idx);
int new_msg_writer(void *array, int idx, void *msg, size_t msglength);

#endif
