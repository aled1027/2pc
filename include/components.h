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

void buildLevenshteinCircuit(GarbledCircuit *gc, block *inputLabels, block *outputMap,
                        int *outputWires, int l, int sigma, int m);

void addLevenshteinCoreCircuit(GarbledCircuit *gc, GarblingContext *gctxt, 
        int l, int sigma, int *inputWires, int *outputWires);
/* Makes a "LevenshteinCore" circuit as defined in 
 * Faster Secure Two-Party Computation Using Garbled Circuits
 * Page 9, figure 5c. 
 *
 * Use an alphabet of size sigma.
 *
 * Input = D[i-1][j-1] ||D[i-1][j] || D[i][j-1] || a[i] || b[j] 
 * |Input| = l-bits || l-bits || l-bits || 2-bits || 2-bits
 *
 * Wires are ordered such that 1s digit is first, 2s digit second, and so forth.
 * This is way in which JustGarble oriented their adders.
 */

int MINCircuitWithLEQOutput(GarbledCircuit *gc, GarblingContext *garblingContext, int n,
		int* inputs, int* outputs);

int INCCircuitWithSwitch(GarbledCircuit *gc, GarblingContext *ctxt,
		int the_switch, int n, int *inputs, int *outputs);

void AddAESCircuit(GarbledCircuit *gc, GarblingContext *garblingContext, int numAESRounds, 
        int *inputWires, int *outputWires);
void buildANDCircuit(GarbledCircuit *gc, int n, int nlayers);
void buildCBCFullCircuit(GarbledCircuit *gc, int num_message_blocks, int num_aes_rounds, block *delta);
void buildAdderCircuit(GarbledCircuit *gc);
void buildAESCircuit(GarbledCircuit *gc);
void buildXORCircuit(GarbledCircuit *gc, block* delta);
void buildAESRoundComponentCircuit(GarbledCircuit *gc, bool isFinalFound, block* delta);

#endif
