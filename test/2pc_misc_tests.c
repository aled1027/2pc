#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 
#include "utils.h"
#include <math.h>

#include "arg.h"
#include "gates.h"

void notGateTest()
{
    /* A test that takes one input, and performs 3 sequential
     * not gates. The evaluation happens locally (ie no networking and OT).
     * The reason this tests exists in some cases with sequential not gates, 
     * the output would be incorrect. I believe the problem was fixed
     * when I 
     */
    /* Paramters */
    printf("Running not gate test\n");
    int n = 1;
    int m = 3;
    int q = 3;
    int r = n + q;

    /* Inputs */
    int *inputs = allocate_ints(2*n);
    inputs[0] = 0;

    /* Build Circuit */
    InputLabels inputLabels = allocate_blocks(2*n);
    createInputLabels(inputLabels, n);
    GarbledCircuit gc;
	createEmptyGarbledCircuit(&gc, n, m, q, r, inputLabels);
    GarblingContext gcContext;
	startBuilding(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    NOTGate(&gc, &gcContext, 0, 1);
    NOTGate(&gc, &gcContext, 1, 2);
    NOTGate(&gc, &gcContext, 2, 3);

    countToN(outputWires, m);
    OutputMap outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, &gcContext, outputMap, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap);
    //print_gc(&gc);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(2*n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(2*m);
    evaluate(&gc, extractedLabels, computedOutputMap);

    /* Results */
    int *outputs = allocate_ints(m);
    for (int i=0; i<m; i++)
        outputs[i] = 55;
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    bool failed = false;
    for (int i = 0; i < m; i++) {
        if (outputs[i] != (i % 2)) {
            printf("Output %d is incorrect\n", i);
            failed = true;
        }
    }
    if (failed)
        printf("Not gate test failed\n");
}

int main(int argc, char *argv[]) 
{
	srand(time(NULL));
    srand_sse(time(NULL));
    assert(argc == 2);

    if (strcmp(argv[1], "not") == 0) {
        for (int i = 0; i < 100; i++)
            notGateTest();
    } else {
        printf("See test/2pc_misc_tests.c:main for usage\n");
    }
    
    return 0;
} 
