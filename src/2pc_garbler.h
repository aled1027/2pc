#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include <jansson.h>
#include "chaining.h"
#include "chaining.h"

int garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);

int garbler_make_real_instructions(FunctionSpec *function, CircuitType *saved_gcs_type, 
        int num_saved_gcs, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs);

int load_function_via_json(char* path, FunctionSpec *function);
int json_load_components(json_t *root, FunctionSpec* function);
int json_load_input_mapping(json_t *root, FunctionSpec* function);
int json_load_instructions(json_t* root, FunctionSpec* function);
InstructionType get_instruction_type_from_string(const char* type);
CircuitType get_circuit_type_from_string(const char* type);
void print_function(FunctionSpec* function);
void print_components(FunctionComponent* components, int num_components);
void print_input_mapping(InputMapping* inputMapping);
void print_instructions(Instructions* instr);

#endif
