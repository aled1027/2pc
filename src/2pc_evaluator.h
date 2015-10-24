#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

int chainedEvaluate(GarbledCircuit *gcs, int num_gcs, 
        Instruction* instructions, int num_instr, 
        InputLabels* labels, block* receivedOutputMap, 
        int* inputs[], int* output);

// TODO:
int receive_cs();
int save_gcs();
int load_gcs();


#endif
