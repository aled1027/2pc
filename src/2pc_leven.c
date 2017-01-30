#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h"
#include "components.h"
#include "utils.h"
#include <math.h>

//#include "gates.h"

#include <garble.h>

static int getDIntSize(int l) { return (int) floor(log2(l)) + 1; }
static int getInputsDevotedToD(int l) { return getDIntSize(l) * (l+1); }
static int getN(int l, int sigma) { return getInputsDevotedToD(l) + (2*sigma*l); }
int levenNumOutputs(int l) { return getDIntSize(l); }
int levenNumEvalInputs(int l, int sigma) { return sigma * l; }
int levenNumGarbInputs(int l, int sigma) { return getN(l, sigma) - levenNumEvalInputs(l, sigma); }
int levenNumCircs(int l) { return l * l; }
static int getCoreN(int l, int sigma) { return (3 * getDIntSize(l)) + (2 * sigma); }
static int getCoreM(int l) { return getDIntSize(l); }
static int getCoreQ() { return 10000; } // TODO figure out this number

void
leven_garb_off(int l, int sigma, ChainingType chainingType) 
{
    block delta = garble_create_delta();

    int coreN = getCoreN(l, sigma);
    int coreM = getCoreM(l);
    int numCircuits = levenNumCircs(l);
    ChainedGarbledCircuit chainedGCs[numCircuits];
    for (int i = 0; i < numCircuits; i++) {
        /* Initialize */
        garble_context gcContext;
        int inputWires[coreN], outputWires[coreM];
        countToN(inputWires, coreN);
        chainedGCs[i].inputLabels = garble_allocate_blocks(2*coreN);
        chainedGCs[i].outputMap = garble_allocate_blocks(2*coreM);
        garble_circuit *gc = &chainedGCs[i].gc;

        /* Garble */
	    garble_new(gc, coreN, coreM, garble_type);
	    builder_start_building(gc, &gcContext);
        addLevenshteinCoreCircuit(gc, &gcContext, l, sigma, inputWires, outputWires);
	    builder_finish_building(gc, &gcContext, outputWires);
        if (chainingType == CHAINING_TYPE_SIMD) {
            createSIMDInputLabelsWithRForLeven(&chainedGCs[i], delta, l);
        } else {
            garble_create_input_labels(chainedGCs[i].inputLabels, coreN, &delta, false);
        }

        garble_garble(gc, chainedGCs[i].inputLabels, chainedGCs[i].outputMap);

        /* Declare chaining vars */
        chainedGCs[i].id = i;
        chainedGCs[i].type = LEVEN_CORE;
    }

    if (chainingType == CHAINING_TYPE_SIMD) {
        for (int i = 0; i < numCircuits; i++)
            generateOfflineChainingOffsets(&chainedGCs[i]);
    }

    int num_eval_inputs = levenNumEvalInputs(l, sigma);
    garbler_offline(GARBLER_DIR, chainedGCs, num_eval_inputs, numCircuits, chainingType);
}

ChainedGarbledCircuit* leven_circuits(int l, int sigma) 
{
    int coreN = getCoreN(l, sigma);
    int coreM = getCoreM(l);
    int numCircuits = levenNumCircs(l);
    ChainedGarbledCircuit *chainedGCs = calloc(numCircuits, sizeof chainedGCs[0]);
    for (int i = 0; i < numCircuits; i++) {
        /* Initialize */
        garble_context gcContext;
        int inputWires[coreN], outputWires[coreM];
        countToN(inputWires, coreN);
        garble_circuit *gc = &chainedGCs[i].gc;

	    garble_new(gc, coreN, coreM, garble_type);
	    builder_start_building(gc, &gcContext);
        addLevenshteinCoreCircuit(gc, &gcContext, l, sigma, inputWires, outputWires);
	    builder_finish_building(gc, &gcContext, outputWires);
    }

    return chainedGCs;
}

