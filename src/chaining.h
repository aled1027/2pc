
#ifndef MPC_CHAINING_H
#define MPC_CHAINING_H


#include "circuit_builder.h" /* buildAdderCircuit */
#include "gc_comm.h"
#include "gates.h"
#include "circuits.h"

typedef enum {ADDER22, ADDER23} CircuitType;

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

int go();
int createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n);
int createChainingMap(ChainingMap* c_map, ChainedGarbledCircuit* chained_gcs, int num_maps);
int chainedEvaluate(GarbledCircuit *gcs, int num_gcs, 
        ChainingMap* c_map, int c_map_size, 
        InputLabels* labels, block* receivedOutputMap, int* output);
int freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc);

#endif
