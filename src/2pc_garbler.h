#ifndef MPC_GARBLER_H
#define MPC_GARBLER_H

#include <jansson.h>
#include "chaining.h"

int garbler_init();
int load_function_via_json(char* path, FunctionSpec *function);
void print_function(FunctionSpec* function);

static int json_load_components(json_t *root, FunctionSpec* function);
static int json_load_input_mapping(json_t *root, FunctionSpec* function);
static int json_load_instructions(json_t* root, FunctionSpec* function);

static InstructionType get_instruction_type_from_string(const char* type);
static CircuitType get_circuit_type_from_string(const char* type);

static void print_components(FunctionComponent* components, int num_components);
static void print_input_mapping(InputMapping* inputMapping);
static void print_instructions(Instructions* instr);



#endif
