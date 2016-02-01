#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"

void
evaluator_classic_2pc(int *input, int *output,
                      int num_garb_inputs, int num_eval_inputs,
                      uint64_t *tot_time);

void
evaluator_offline(char *dir, int num_eval_inputs, int nchains);

void
evaluator_online(char *dir, int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, uint64_t *tot_time);

static void
loadChainedGarbledCircuits(ChainedGarbledCircuit *cgc, int ncgcs, char *dir);

static void
loadOTPreprocessing(block **eval_labels, int **corrections, char *dir);

static Output*
recvOutput(int outputArrSize, int sockfd) ;

static void
mapOutputsWithOutputInstructions(Output *outputInstructions, int outputInstructionsSize,
                                 int *output, int noutputs, block **computedOutputMap,
                                 block *outputMap);
#endif
