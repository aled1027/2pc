
#ifndef MPC_CHAINING_H
#define MPC_CHAINING_H


#include "circuit_builder.h" /* buildAdderCircuit */
#include "gc_comm.h"
#include "gates.h"
#include "circuits.h"

typedef enum {ADDER22, ADDER23} CircuitType;

typedef enum {INPUT, REQUEST_INPUT, EVAL, CHAIN, INSTR_ERR} InstructionType;

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
    // TODO remove instruciton; change to type. keeping to not break old things for now.
    InstructionType instruction; // todo remove input/chaining. will happen automatically.
    InstructionType type; // todo remove input/chaining. will happen automatically.
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
    char* name;
    char* description;
    int n,m;
    FunctionComponent* components;
    InputMapping input_mapping;
    Instruction* instructions;
} FunctionSpec;

typedef struct {
    GarbledCircuit gc;
    int id;
    CircuitType type;
    // TODO looks like GarbledCircuit as a slot for inputLabels and outputLabels
    // Could use that. but would need to be careful about saving gc to disk.
    // I think I like them abstracted out.
    //InputLabels inputLabels; // block*
    //OutputMap outputMap; // block*
    block* inputLabels; // block*
    block* outputMap; // block*
} ChainedGarbledCircuit;

typedef struct {
    int from_gc_id;
    int from_output_idx;
    int to_gc_id;
    int to_input_idx;
    block offset; // from_wire xor to_wire, assuming the same deltas
} ChainingMap;

int createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n);
int createInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs);
int createChainingMap(ChainingMap* c_map, ChainedGarbledCircuit* chained_gcs);
int chainedEvaluate(GarbledCircuit *gcs, int num_gcs, 
        Instruction* instructions, int num_instr, 
        InputLabels* labels, block* receivedOutputMap, 
        int* inputs[], int* output);
int chainedEvaluateOld(GarbledCircuit *gcs, int num_gcs, 
        ChainingMap* c_map, int c_map_size, 
        InputLabels* labels, block* receivedOutputMap, int* output);
int freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc);

#endif
