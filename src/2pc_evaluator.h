#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"

int chainedEvaluate(GarbledCircuit *gcs, int num_gcs, 
        Instruction* instructions, int num_instr, 
        InputLabels* labels, block* receivedOutputMap, 
        int* inputs[], int* output);

// abstraction of OT transfer of labels and extractLabels
int getLabels(block* labels, int* inputs, int n, block* inputLabels);

// TODO:
// int receive_cs();
// int save_gcs();
// int load_gcs();


#endif
