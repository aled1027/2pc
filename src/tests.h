#ifndef MPC_TESTS_H
#define MPC_TESTS_H

#include <jansson.h>

int run_all_tests();
int simple_test();
int test2();
int test_saving_reading();
int json_test();
int json_get_components(json_t *root);
int json_get_input_mapping(json_t *root, InputMapping* inputMapping);
int json_get_instructions(json_t* root, Instructions* instructions);

static void print_input_mapping(InputMapping* inputMapping);
static void print_instructions(Instructions* instr);
static InstructionType get_instruction_type_from_string(const char* type);

#endif
