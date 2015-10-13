#ifndef MPC_GARBLED_CIRCUIT_H
#define MPC_GARBLED_CIRCUIT_H

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


int createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n);
int freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc);
int saveGarbledCircuit(GarbledCircuit* gc, char* fileName);
int readGarbledCircuit(GarbledCircuit* gc, char* fileName);
void buildAdderCircuit(GarbledCircuit *gc);

#endif
