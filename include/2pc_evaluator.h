#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"

void evaluator_classic_2pc(int *input, int *output,
        int num_garb_inputs, int num_eval_inputs,
        unsigned long *tot_time);

void evaluator_offline(ChainedGarbledCircuit *chained_gcs, 
        int num_eval_inputs, int num_chained_gcs);

void evaluator_online(int *eval_inputs, int num_eval_inputs, int num_chained_gcs, 
        unsigned long *ot_time, unsigned long *tot_time);

#endif
