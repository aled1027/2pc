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

void leven_garb_off() 
{
    printf("Running leven garb offline\n");
}

void leven_eval_off() 
{
    printf("Running leven eval offline\n");
}

void leven_garb_on() 
{
    printf("Running leven garb online\n");
}

void leven_eval_on() 
{
    printf("Running leven eval online\n");
}

void full_leven_garb()
{
    /* Garbler offline phase for leven where no components are used:
     * only a single circuit
     */
    printf("Running full leven garb\n");

    int l = 2; /* single parameter */

    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);

    int n = inputsDevotedToD + 2*2*l;
    int core_n = (3 * DIntSize) + 4;
    int m = 400;

    /* Build and Garble */
    GarbledCircuit gc;
    int *outputWires = allocate_ints(m);
    InputLabels inputLabels = allocate_blocks(2*n);
    OutputMap outputMap = allocate_blocks(2*m);
    buildLevenshteinCircuit(&gc, inputLabels, outputMap, outputWires, l, m);
    garbleCircuit(&gc, inputLabels, outputMap);
    //print_gc(&gc);

    /* Get Inputs */
    int *inputs = allocate_ints(n);
    //since l = 1, we need to include 0,1
    // L = 1
    //inputs[0] = 0;
    //inputs[1] = 1;
    //inputs[2] = 1;
    //inputs[3] = 1;
    //inputs[4] = 1;
    //inputs[5] = 0;

    // L = 2
    inputs[0] = 0;
    inputs[1] = 0;
    inputs[2] = 1;
    inputs[3] = 0;
    inputs[4] = 0;
    inputs[5] = 1;

    inputs[6] = 0;
    inputs[7] = 0;
    inputs[8] = 0;
    inputs[9] = 0;

    inputs[10] = 1;
    inputs[11] = 1;
    inputs[12] = 0;
    inputs[13] = 0;
    printf("Inputs: ");
    for (int i = 0; i < n; i++) 
        printf("%d", inputs[i]);
    printf("\n");

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap);

    /* Results */
    int *outputs = allocate_ints(m);
    for (int i=0; i<m; i++)
        outputs[i] = 55;
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    printf("Outputs: \n");

    printf("outputs[354]: %d\n", outputs[354]);
    printf("outputs[358]: %d\n", outputs[358]);

    //for (int i = 0; i < m; i++)
    //    printf("%d", outputs[i]);
    //    //printf("%i: %d\n", i, outputs[i]);
}

void full_leven_eval()
{
}

int main(int argc, char *argv[]) 
{
    // TODO add in arg.h stuff
	srand(time(NULL));
    srand_sse(time(NULL));
    assert(argc == 2);
    if (strcmp(argv[1], "eval_online") == 0) {
        leven_eval_on();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        leven_garb_on();
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        leven_garb_off();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        leven_eval_off();

    } else if (strcmp(argv[1], "full_garb") == 0) {
        full_leven_garb();
    } else if (strcmp(argv[1], "full_eval") == 0) {
        full_leven_eval();

    } else {
        printf("See test/2pc_leven.c:main for usage\n");
    }
    
    return 0;
} 
