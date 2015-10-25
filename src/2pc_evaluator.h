#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"
#include "2pc_garbled_circuit.h"


int chainedEvaluate(GarbledCircuit *gcs, int num_gcs, Instruction* instructions, int num_instr, 
        block* inputLabels, block* receivedOutputMap, 
        int* inputs, int* output, InputMapping* input_mapping);
// abstraction of OT transfer of labels and extractLabels
int getLabels(block** labels, int* eval_inputs, int eval_num_inputs, 
        block* input_labels, InputMapping *input_mapping) ;

static void array_slice(int* dest, int* src, int a, int b) ;
int new_choice_reader(void *choices, int idx);
int new_msg_writer(void *array, int idx, void *msg, size_t msglength);

//int newChainedEvaluate(FunctionSpec *function, GarbledCircuit *gcs, int num_gcs, 
        //block* receivedOutputMap, int* inputs, int* outputs, InputLabels* inputLabels);
// TODO:
// int receive_cs();
// int save_gcs();
// int load_gcs();


#endif
