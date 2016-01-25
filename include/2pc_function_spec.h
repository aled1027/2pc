#ifndef MPC_FUNCTION_SPEC_H
#define MPC_FUNCTION_SPEC_H

#include <jansson.h>
#include "2pc_garbled_circuit.h"

// XOR is 128 bit xor
// GENIV generates a 128 bit random string and plugs into some wires?
typedef enum {EVAL, CHAIN, INSTR_ERR} InstructionType;
typedef enum {PERSON_GARBLER = 0, PERSON_EVALUATOR = 1, PERSON_ERR = -1} Person;


typedef struct {
    /* not actually a single component, but a component type 
     * with num components of the type, and ids circuit_ids
     */
    int numComponentTypes;
    int totComponents;
    CircuitType *circuitType;
    int *nCircuits;
    int **circuitIds; 
    // so circuit_type[i] has circuitIds[i], which is an integer array of size nCircuits[i]
} FunctionComponent;

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
    // the output is for all i from 0 to size
    // of gc_id[i] from wire start_wire_idx[i] to wire end_wire_idx[i]
    int *gc_id;
    int *start_wire_idx;
    int *end_wire_idx;
    int size; // size of the arrays
} 
Output;

typedef struct {
    /* The specifiction for a function. 
     * That is, the components, instructions for evaluating and chaining components,
     * and everything else necessary for evaluating a function 
     */
    char* name; // make 128
    char* description; // make 128
    int n, m;
    int num_garb_inputs, num_eval_inputs;
    FunctionComponent components;
    InputMapping input_mapping;
    Instructions instructions;
    Output output;
} 
FunctionSpec;


// json loading functions
// public interface
int load_function_via_json(char* path, FunctionSpec *function);
void print_function(FunctionSpec* function);
int freeFunctionSpec(FunctionSpec* function);

// private interface - below this should be static:
int json_load_metadata(json_t *root, FunctionSpec *function);
int json_load_components(json_t *root, FunctionSpec* function);
int json_load_input_mapping(json_t *root, FunctionSpec* function);
int json_load_instructions(json_t* root, FunctionSpec* function);
int json_load_output(json_t *root, FunctionSpec *function);
InstructionType get_instruction_type_from_string(const char* type);
CircuitType get_circuit_type_from_string(const char* type);
void print_metadata(FunctionSpec *function);
void print_components(FunctionComponent* components);
void print_input_mapping(InputMapping* inputMapping);
void print_instructions(Instructions* instr);
void print_output(Output *output);

int writeInstructionsToBuffer(const Instructions* instructions, char* buffer);
int readBufferIntoInstructions(Instructions* instructions, const char* buffer);

int writeInputMappingToBuffer(const InputMapping* input_mapping, char* buffer);
int readBufferIntoInputMapping(InputMapping* input_mapping, const char* buffer);

void newInputMapping(InputMapping *map, int size);
void deleteInputMapping(InputMapping *map);

size_t
instructionBufferSize(const Instructions *instructions);
size_t
inputMappingBufferSize(const InputMapping *map);

#endif
