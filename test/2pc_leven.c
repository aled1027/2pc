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

int l = 2;
int numCircuits = 4;
char *COMPONENT_FUNCTION_PATH = "functions/leven_2.json"; 

static int getDIntSize() { return (int) floor(log2(l)) + 1; }
static int getInputsDevotedToD() { return getDIntSize() * (l+1); }
static int getN() { return getInputsDevotedToD() + (2*2*l); }
static int getM() { return getDIntSize(); }
static int getNumEvalInputs() { return 2*l; }
static int getNumGarbInputs() { return getN() - getNumEvalInputs(); }
static int getCoreN() { return (3 * getDIntSize()) + 4; }
static int getCoreM() { return getDIntSize(); }
static int getCoreQ() { return 10000; } // figure out this number

static int getNumGatesPerCore() {return 10; } // figure out this number
static int getNumCoresForL() { return l * 10; } // figure out this number
static int getNumGates() { return getNumCoresForL() * getNumGatesPerCore(); }



void leven_garb_off() 
{
    printf("Running leven garb offline\n");

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    int coreN = getCoreN();
    int coreM = getCoreM();
    int coreQ = getCoreQ();
    int coreR = coreN + coreQ;

    ChainedGarbledCircuit chainedGCs[numCircuits];
    for (int i = 0; i < numCircuits; i++) {
        /* Initialize */
        GarblingContext gcContext;
        int *inputWires = allocate_ints(coreN);
        countToN(inputWires, coreN);
        int *outputWires = allocate_ints(coreM);
        chainedGCs[i].inputLabels = allocate_blocks(2*coreN);
        chainedGCs[i].outputMap = allocate_blocks(2*coreM);
        GarbledCircuit *gc = &chainedGCs[i].gc;

        /* Garble */
        createInputLabelsWithR(chainedGCs[i].inputLabels, coreN, &delta);
	    createEmptyGarbledCircuit(gc, coreN, coreM, coreQ, coreR, chainedGCs[i].inputLabels);
	    startBuilding(gc, &gcContext);
        addLevenshteinCoreCircuit(gc, &gcContext, l, inputWires, outputWires);
        garbleCircuit(gc, chainedGCs[i].inputLabels, chainedGCs[i].outputMap);
	    finishBuilding(gc, &gcContext, chainedGCs[i].outputMap, outputWires);

        /* Declare chaining vars */
        chainedGCs[i].id = i;
        chainedGCs[i].type = LEVEN_CORE;
        /* Clean up */
        removeGarblingContext(&gcContext);
    }
    int numEvalInputs = getNumEvalInputs();
    garbler_offline(chainedGCs, numEvalInputs, numCircuits);
}

void leven_eval_off() 
{
    ChainedGarbledCircuit chainedGCs[numCircuits];
    int numEvalInputs = getNumEvalInputs(); /* Total number of inputs, not per circuit */
    evaluator_offline(chainedGCs, numEvalInputs, numCircuits);
}

void leven_garb_on() 
{
    printf("Running leven garb online\n");
    char *functionPath = COMPONENT_FUNCTION_PATH;
    int DIntSize = getDIntSize();
    int inputsDevotedToD = getInputsDevotedToD();
    int n = getN();
    int numEvalInputs = getNumEvalInputs();
    int numGarbInputs = getNumGarbInputs();
    int m = getM();

    /* Set Inputs */
    int *garbInputs = allocate_ints(numGarbInputs);

    /* The first inputsDevotedToD inputs are 0 to l+1 in binary */
    for (int i = 0; i < l + 1; i++) 
        convertToBinary(i, garbInputs + (DIntSize) * i, DIntSize);

    for (int i = inputsDevotedToD; i < numGarbInputs; i++) {
        garbInputs[i] = rand() % 2;
        printf("%d", garbInputs[i]);
    }
    printf("\n");

    unsigned long tot_time;
    garbler_online(functionPath, garbInputs, numGarbInputs, numEvalInputs, numCircuits, NULL, &tot_time);
}

void leven_eval_on() 
{
    printf("Running leven eval online\n");

    printf("Running cbc eval online\n");
    int numEvalInputs = getNumEvalInputs();
    int *evalInputs = allocate_ints(numEvalInputs);
    for (int i = 0; i < numEvalInputs; i++)
        evalInputs[i] = rand() % 2;

    unsigned long tot_time;
    evaluator_online(evalInputs, numEvalInputs, numCircuits, NULL, &tot_time);
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
