#ifndef MPC_FUNCTION_SPEC_H
#define MPC_FUNCTION_SPEC_H

#include <jansson.h>
#include "2pc_garbled_circuit.h"

typedef enum {EVAL, CHAIN, INSTR_ERR} InstructionType;

typedef struct {
    CircuitType circuit_type;
    int num;
    int* circuit_ids; // e.g if circuit ids 4,5,9 correspond to 22adder, this would be {4,5,9}.
} FunctionComponent;

typedef struct {
    int size;
    int* input_idx; // so input_idx[i] maps to (gc_id[i], wire_id[i])
    int* gc_id; // each int array should be sizeof(int)*n (where n is number of inputs)
    int* wire_id;
} InputMapping;

typedef struct {
    // TODO remove instruction; change to type. keeping to not break old things for now.
    InstructionType type; 
    InputLabels inInputLabels; // type block*
    int inCircId;
    
    // REQUEST_INPUT:
    // what input to request, where will it go?
    
    // EVAL:
    // assume inputs are already plugged in
    int evCircId;

    // CHAIN:
    int chFromCircId;
    int chFromWireId;
    int chToCircId;
    int chToWireId;
    block chOffset; // from_wire xor to_wire, assuming the same deltas
} Instruction;

typedef struct {
    int size;
    Instruction* instr;
} Instructions;

typedef struct {
    /* The specifiction for a function. 
     * That is, the components, instructions for evaluating and chaining components,
     * and everything else necessary for evaluating a function 
     */
    char* name;
    char* description;
    int n,m;
    int num_components;
    FunctionComponent* components;
    InputMapping input_mapping;
    Instructions instructions;
} FunctionSpec;


// json loading functions
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

int freeFunctionSpec(FunctionSpec* function);

#endif
