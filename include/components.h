#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "justGarble.h"
#include <stdbool.h>

typedef enum {
    ADDER22 = 0, 
    ADDER23 = 1, 
    ADD = 2,
    MULT = 3,
    AES_ROUND = 4,
    AES_FINAL_ROUND = 5,
    XOR = 6,
    FULL_CBC = 7,
    LEVEN_CORE = 8,
    CIRCUIT_TYPE_ERR = -1
} CircuitType;

bool isFinalCircuitType(CircuitType type);

void buildMinCircuit(GarbledCircuit *gc, block *inputLabels, block *outputMap,
                     int *outputWires);
void buildLevenshteinCircuit(GarbledCircuit *gc, block *inputLabels,
                             block *outputMap, int *outputWires, int l, int m);

void addLevenshteinCoreCircuit(GarbledCircuit *gc, GarblingContext *gcContext, 
        int l, int *inputWires, int *outputWires);

int MINCircuitWithLEQOutput(GarbledCircuit *gc, GarblingContext *garblingContext, int n,
		int* inputs, int* outputs);

void AddAESCircuit(GarbledCircuit *gc, GarblingContext *garblingContext, int numAESRounds, 
        int *inputWires, int *outputWires);
void buildANDCircuit(GarbledCircuit *gc, int n, int nlayers);
void buildCBCFullCircuit(GarbledCircuit *gc, int num_message_blocks, int num_aes_rounds, block *delta);
void buildAdderCircuit(GarbledCircuit *gc);
void buildAESCircuit(GarbledCircuit *gc);
void buildXORCircuit(GarbledCircuit *gc, block* delta);
void buildAESRoundComponentCircuit(GarbledCircuit *gc, bool isFinalFound, block* delta);

#endif
