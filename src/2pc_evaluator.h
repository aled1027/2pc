#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

// evaluator_offline
// evaluator_run
    // evaluator_go
    // evaluator_evaluate
    
// new
void evaluator_run();
void evaluator_evaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, block* outputmap, int* output, int* circuitMapping);
void evaluator_offline();
int new_choice_reader(void *choices, int idx);
int new_msg_writer(void *array, int idx, void *msg, size_t msglength);

#endif
