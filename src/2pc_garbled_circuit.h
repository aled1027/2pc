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
void buildAdderCircuit(GarbledCircuit *gc);

// reading and writing methods
// these methods are for writing/reading garbled circuit for evaluator stream
// they do not save wire labels, which the garbler will need if they want to save to disk and load again later.
int saveGarbledCircuit(GarbledCircuit* gc, char* fileName);
int loadGarbledCircuit(GarbledCircuit* gc, char* fileName);
static int writeGarbledCircuitToBuffer(GarbledCircuit* gc, char* buffer, size_t max_buffer_size);
static int readBufferIntoGarbledCircuit(GarbledCircuit *gc, char* buffer);
static size_t sizeofGarbledCircuit(GarbledCircuit* gc);


#endif
