#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include <stdint.h>
#include "2pc_function_spec.h"


void
garbler_classic_2pc(GarbledCircuit *gc, const OldInputMapping *input_mapping,
                    const block *outputMap, int num_garb_inputs, int num_eval_inputs,
                    const int *inputs, uint64_t *tot_time);

void garbler_offline(char *dir, ChainedGarbledCircuit* chained_gcs,
                     int num_eval_inputs, int num_chained_gcs, ChainingType chainingType);

int garbler_online(char *function_path, char *dir, int *inputs,
                   int num_garb_inputs, int num_chained_gcs,
                   uint64_t *tot_time, ChainingType chainingType);

#endif
