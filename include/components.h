#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <garble.h>
#include <circuit_builder.h>
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

void circuit_argmax4(garble_circuit *gc, garble_context *ctxt,
        int *inputs, int *outputs, int num_len);

void circuit_argmax2(garble_circuit *gc, garble_context *ctxt,
        int *inputs, int *outputs, int num_len);

void new_circuit_les(garble_circuit *circuit, garble_context *context, int n,
        int *inputs, int *output);

void new_circuit_or(garble_circuit *gc, garble_context *ctxt, int *input, int *output);

void new_circuit_mux21(garble_circuit *gc, garble_context *ctxt, 
              int theSwitch, int input0, int input1, int output[1]);
int
countToN(int *a, int n);

bool isFinalCircuitType(CircuitType type);

void buildMinCircuit(garble_circuit *gc, block *inputLabels, block *outputMap,
                     int *outputWires);

void buildLevenshteinCircuit(garble_circuit *gc, int l, int sigma);

void addLevenshteinCoreCircuit(garble_circuit *gc, garble_context *gctxt, 
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

int MINCircuitWithLEQOutput(garble_circuit *gc, garble_context *garblingContext, int n,
		int* inputs, int* outputs);

int INCCircuitWithSwitch(garble_circuit *gc, garble_context *ctxt,
		int the_switch, int n, int *inputs, int *outputs);

void AddAESCircuit(garble_circuit *gc, garble_context *garblingContext, int numAESRounds, 
        int *inputWires, int *outputWires);
void buildANDCircuit(garble_circuit *gc, int n, int nlayers);
void buildCBCFullCircuit(garble_circuit *gc, int num_message_blocks, int num_aes_rounds, block *delta);
void buildAdderCircuit(garble_circuit *gc);
void buildAESCircuit(garble_circuit *gc);
void buildXORCircuit(garble_circuit *gc, block* delta);
void buildAESRoundComponentCircuit(garble_circuit *gc, bool isFinalFound, block* delta);

#endif
