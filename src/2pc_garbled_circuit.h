#ifndef MPC_GARBLED_CIRCUIT_H
#define MPC_GARBLED_CIRCUIT_H

#include <stdbool.h>
#include "justGarble.h"

typedef enum {
    ADDER22 = 0, 
    ADDER23 = 1, 
    CIRCUIT_TYPE_ERR = -1
    } CircuitType;

typedef struct {
    /* Our abstraction/layer on top of GarbledCircuit */
    GarbledCircuit gc;
    int id;
    CircuitType type;
    block* inputLabels; // block*
    block* outputMap; // block*
} ChainedGarbledCircuit; 

typedef struct { 
    /* index is i \in {0, ... , num_gcs-1} 
     * so gc with index i has properties gc_types[i], gc_valids[i], gc_paths[i]. 
     */

    int num_gcs;
    CircuitType* gc_types;
    bool* gc_valids;
    char** gc_paths; 
} GCsMetadata;

int createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n);
int freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc);
int saveGarbledCircuit(GarbledCircuit* gc, char* fileName);
int loadGarbledCircuit(GarbledCircuit* gc, char* fileName);

void buildAdderCircuit(GarbledCircuit *gc);

#endif
