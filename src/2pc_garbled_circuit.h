#ifndef MPC_GARBLED_CIRCUIT_H
#define MPC_GARBLED_CIRCUIT_H

#include <stdbool.h>
#include "justGarble.h"

typedef enum {
    ADDER22 = 0, 
    ADDER23 = 1, 
    ADD = 2,
    MULT = 3,
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
void buildAdderCircuit(GarbledCircuit *gc);
void buildAESCircuit(GarbledCircuit *gc);

int saveChainedGC(ChainedGarbledCircuit* chained_gc, bool isGarbler);
int loadChainedGC(ChainedGarbledCircuit* chained_gc, int id, bool isGarbler);

void print_block(block blk);
void print_garbled_gate(GarbledGate *gg);
void print_garbled_table(GarbledTable *gt);
void print_wire(Wire *w);
void print_gc(GarbledCircuit *gc);
void print_blocks(const char *str, block *blks, int length);
#endif
