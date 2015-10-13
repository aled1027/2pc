
#ifndef MPC_CHAINING_H
#define MPC_CHAINING_H


#include "circuit_builder.h" /* buildAdderCircuit */
#include "gc_comm.h"
#include "gates.h"
#include "circuits.h"

typedef enum {
    ADDER22 = 0, 
    ADDER23 = 1, 
    CIRCUIT_TYPE_ERR = -1
    } CircuitType;

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

typedef struct {
    /* Our abstraction/layer on top of GarbledCircuit */
    GarbledCircuit gc;
    int id;
    CircuitType type;
    block* inputLabels; // block*
    block* outputMap; // block*
} ChainedGarbledCircuit; 

int createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n);
int createInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs);
int chainedEvaluate(GarbledCircuit *gcs, int num_gcs, 
        Instruction* instructions, int num_instr, 
        InputLabels* labels, block* receivedOutputMap, 
        int* inputs[], int* output);
int freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc);
int freeFunctionSpec(FunctionSpec* function);

#endif
