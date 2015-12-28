#ifndef MPC_FUNCTION_SPEC_H
#define MPC_FUNCTION_SPEC_H

#include <jansson.h>
#include "2pc_garbled_circuit.h"

typedef enum {EVAL, CHAIN, INSTR_ERR} InstructionType;
typedef enum {PERSON_GARBLER, PERSON_EVALUATOR, PERSON_ERR} Person;

typedef struct {
    /* not actually a single component, but a component type 
     * with num components of the type, and ids circuit_ids
     */
    CircuitType circuit_type;
    int num;
    int* circuit_ids; // e.g if circuit ids 4,5,9 correspond to 22adder, this would be {4,5,9}.
} 
FunctionComponent;

typedef struct {
    int size;
    int* input_idx; // so input_idx[i] maps to (gc_id[i], wire_id[i])
    int* gc_id; // each int array should be sizeof(int)*n (where n is number of inputs)
    int* wire_id;
    Person* inputter; // inputter[i] == GARBLER means the ith input should be inputted by the garbler.
} 
InputMapping;

typedef struct {
    InstructionType type; 

    // EVAL:
    // assume inputs are already plugged into the circuit evCircId.
    int evCircId;

    // CHAIN:
    int chFromCircId;
    int chFromWireId;
    int chToCircId;
    int chToWireId;
    block chOffset; // from_wire xor to_wire, assuming the same deltas
} 
Instruction;

typedef struct {
    int size;
    Instruction* instr;
} 
Instructions;

typedef struct {
    /* The specifiction for a function. 
     * That is, the components, instructions for evaluating and chaining components,
     * and everything else necessary for evaluating a function 
     */
    char* name; // make 128
    char* description; // make 128
    int n, m, num_component_types, num_components;
    FunctionComponent* components;
    InputMapping input_mapping;
    Instructions instructions;
} 
FunctionSpec;


// json loading functions
// public interface
int load_function_via_json(char* path, FunctionSpec *function);
void print_function(FunctionSpec* function);
int freeFunctionSpec(FunctionSpec* function);

// private interface - below this should be static:
int json_load_components(json_t *root, FunctionSpec* function);
int json_load_input_mapping(json_t *root, FunctionSpec* function);
int json_load_instructions(json_t* root, FunctionSpec* function);
InstructionType get_instruction_type_from_string(const char* type);
CircuitType get_circuit_type_from_string(const char* type);
void print_components(FunctionComponent* components, int num_component_types);
void print_input_mapping(InputMapping* inputMapping);
void print_instructions(Instructions* instr);

int writeInstructionsToBuffer(Instructions* instructions, char* buffer);
int readBufferIntoInstructions(Instructions* instructions, char* buffer);

int writeInputMappingToBuffer(InputMapping* input_mapping, char* buffer);
int readBufferIntoInputMapping(InputMapping* input_mapping, char* buffer);

#endif
