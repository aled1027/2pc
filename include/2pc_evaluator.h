#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include <stdint.h>

void
evaluator_classic_2pc(const int *input, bool *output,
                      int num_garb_inputs, int num_eval_inputs,
                      uint64_t *tot_time);

void
evaluator_offline(char *dir, int num_eval_inputs, int nchains,
                  ChainingType chainingType);

int
evaluator_online(char *dir, const int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, ChainingType chainingType,
                 uint64_t *tot_time, uint64_t *tot_time_no_load);

#endif
