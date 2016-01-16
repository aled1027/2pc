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

int l = 29;

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
    /* Runs the garbler for a full circuit of levenshtein distance. 
     * The only paramter is the integer l, which defines
     * the length of the input string for each party.
     * The alphabet is of size 4, i.e. 2 bits, so the actual length
     * of each party's input string is 2*l bits.
     */
    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);
    int n = inputsDevotedToD + 2*2*l;
    int numEvalInputs = 2*l;
    int numGarbInputs = n - numEvalInputs;
    int m = DIntSize;

    /* Set Inputs */
    int *inputs = allocate_ints(numGarbInputs);

    /* The first inputsDevotedToD inputs are the numbers 
     * 0 through l+1 encoded in binary */
    for (int i = 0; i < l + 1; i++) 
        convertToBinary(i, inputs + (DIntSize) * i, DIntSize);
    for (int i = inputsDevotedToD; i < numGarbInputs; i++) {
        inputs[i] = rand() % 2;
        printf("%d", inputs[i]);
    }
    printf("\n");

    /* Build and Garble */
    GarbledCircuit gc;
    int *outputWires = allocate_ints(m);
    InputLabels inputLabels = allocate_blocks(2*n);
    OutputMap outputMap = allocate_blocks(2*m);
    buildLevenshteinCircuit(&gc, inputLabels, outputMap, outputWires, l, m);
    garbleCircuit(&gc, inputLabels, outputMap);
    
    /* Set input mapping */
    InputMapping imap; 
    imap.size = n;
    imap.input_idx = malloc(sizeof(int) * imap.size);
    imap.gc_id = malloc(sizeof(int) * imap.size);
    imap.wire_id = malloc(sizeof(int) * imap.size);
    imap.inputter = malloc(sizeof(Person) * imap.size);

    for (int i = 0; i < numGarbInputs; i++) {
        imap.input_idx[i] = i;
        imap.gc_id[i] = 0;
        imap.wire_id[i] = i;
        imap.inputter[i] = PERSON_GARBLER;
    }

    for (int i = numGarbInputs; i < n; i++) {
        imap.input_idx[i] = i;
        imap.gc_id[i] = 0;
        imap.wire_id[i] = i;
        imap.inputter[i] = PERSON_EVALUATOR;
    }

    /* Online work */
    unsigned long tot_time;
    garbler_classic_2pc(&gc, inputLabels, &imap, outputMap,
        numGarbInputs, numEvalInputs, inputs, &tot_time);

    /* Results */
    removeGarbledCircuit(&gc);
    free(inputs);
    free(inputLabels);
    free(outputWires);
    free(outputMap);
}

void full_leven_eval()
{
    /* Runs the evaluator for a full circuit of 
     * levenshtein distance. The only paramter is the integer l,
     * which defines the length of the input string for each party.
     * The alphabet is of size 4, i.e. 2 bits, so the actual length
     * of each party's input string is 2*l bits.
     */
    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);
    int n = inputsDevotedToD + 2*2*l;
    int numEvalInputs = 2*l;
    int numGarbInputs = n - numEvalInputs;
    int m = DIntSize;

    /* Set Inputs */
    int *inputs = allocate_ints(numEvalInputs);
    printf("Input: ");
    for (int i = 0; i < numEvalInputs; i++) { 
        inputs[i] = rand() % 2;
        printf("%d", inputs[i]);
    }
    printf("\n");

    /* Online work */
    int *outputs = allocate_ints(m);
    unsigned long tot_time;
    evaluator_classic_2pc(inputs, outputs, numGarbInputs, numEvalInputs, &tot_time);

    /* Results */
    printf("Output: ");
    for (int i = 0; i < m; i++) 
        printf("%d", outputs[i]);
    printf("\n");

    free(inputs);
    free(outputs);
}

int main(int argc, char *argv[]) 
{
    assert(l < 35); /* it might crash your computer */

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
