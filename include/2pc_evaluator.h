#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"

void
evaluator_classic_2pc(const int *input, int *output,
                      const int num_garb_inputs, const int num_eval_inputs,
                      uint64_t *tot_time);

void
evaluator_offline(char *dir, const int num_eval_inputs, const int nchains);

void
evaluator_online(char *dir, const int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, uint64_t *tot_time);

static void
loadChainedGarbledCircuits(ChainedGarbledCircuit *cgc, int ncgcs, char *dir);

static void
loadOTPreprocessing(block **eval_labels, int **corrections, char *dir);

static Output*
recvOutput(const int outputArrSize, const int sockfd) ;

static void
mapOutputsWithOutputInstructions(const Output *outputInstructions, const int outputInstructionsSize,
                                 int *output, const int noutputs, block **computedOutputMap,
                                 const block *outputMap);
#endif
