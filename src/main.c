#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "arg.h"
#include "gc_comm.h"
#include "gates.h" /* XORGate, ANDGate */

static void
buildAdderCircuit(GarbledCircuit *gc)
{
	srand(time(NULL));
    srand_sse(time(NULL));

    int n = 2; // number of inputs 
    int m = 2; // number of outputs
    int q = 20; // number of gates
	int r = n+q;  // number of wires

	int *inputs = (int *) malloc(sizeof(int) * n);
	countToN(inputs, n);
    int* outputs = (int*) malloc(sizeof(int) * m);

	block *labels = (block*) malloc(sizeof(block) * 2 * n);
	block *outputmap = (block*) malloc(sizeof(block) * 2 * m);
	GarblingContext gc_context;

	createInputLabels(labels, n);
	createEmptyGarbledCircuit(gc, n, m, q, r, labels);
	startBuilding(gc, &gc_context);

    int wire1 = getNextWire(&gc_context);
    int wire2 = getNextWire(&gc_context);

    XORGate(gc, &gc_context, inputs[0], inputs[1], wire1);
	ANDGate(gc, &gc_context, inputs[0], inputs[1], wire2);

	outputs[0] = wire1;
	outputs[1] = wire2;

	finishBuilding(gc, &gc_context, outputmap, outputs);
}

int
main(int argc, char* argv[]) {
    printf("starting...\n");

    // 1. build circuits
    GarbledCircuit gc1, gc2, gc3;
    buildAdderCircuit(&gc1);
    buildAdderCircuit(&gc2);

    // 2. garble the circuits
    block* inputLabels = (block *) memalign(128, sizeof(block) * 2 * gc1.n);
    block* outputMap = (block *) memalign(128, sizeof(block) * 2 * gc1.m);
    garbleCircuit(&gc1, inputLabels, outputMap);

    // 3. evaluate circuits
	block computedOutputMap[gc1.m], extractedLabels[gc1.n];
	int inputs[gc1.n], outputVals[gc1.m];

    inputs[0] = 1;
    inputs[1] = 0;

	extractLabels(extractedLabels, inputLabels, inputs, gc1.n);
	evaluate(&gc1, extractedLabels, computedOutputMap);
	mapOutputs(outputMap, computedOutputMap, outputVals, gc1.m);

    printf("xor output: %d\n", outputVals[0]);
    printf("and output: %d\n", outputVals[1]);

    free(inputLabels);
    free(outputMap);

    printf("...ending\n");
    return 0;
}


