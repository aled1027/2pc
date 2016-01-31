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

#include "gates.h"

/* XXX: l < 35 */
static int l = 2;
static int numCircuits = 4;
static char *COMPONENT_FUNCTION_PATH = "functions/leven_2.json"; 

static int getDIntSize() { return (int) floor(log2(l)) + 1; }
static int getInputsDevotedToD() { return getDIntSize() * (l+1); }
static int getN() { return getInputsDevotedToD() + (2*2*l); }
int levenNumOutputs() { return getDIntSize(); }
int levenNumEvalInputs() { return 2*l; }
int levenNumEvalLabels() { return 2*(l*l); }
int levenNumGarbInputs() { return getN() - levenNumEvalInputs(); }
int levenNumCircs() { return numCircuits; }
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
        int inputWires[coreN], outputWires[coreM];
        countToN(inputWires, coreN);
        chainedGCs[i].inputLabels = allocate_blocks(2*coreN);
        chainedGCs[i].outputMap = allocate_blocks(2*coreM);
        GarbledCircuit *gc = &chainedGCs[i].gc;

        /* Garble */
        createInputLabelsWithR(chainedGCs[i].inputLabels, coreN, delta);
	    createEmptyGarbledCircuit(gc, coreN, coreM, coreQ, coreR);
	    startBuilding(gc, &gcContext);
        addLevenshteinCoreCircuit(gc, &gcContext, l, inputWires, outputWires);
	    finishBuilding(gc, &gcContext, outputWires);
        garbleCircuit(gc, chainedGCs[i].inputLabels, chainedGCs[i].outputMap,
                      GARBLE_TYPE_STANDARD);

        /* Declare chaining vars */
        chainedGCs[i].id = i;
        chainedGCs[i].type = LEVEN_CORE;
    }
    int numEvalLabels = levenNumEvalLabels();
    garbler_offline("files/garbler_gcs", chainedGCs, numEvalLabels, numCircuits);
}

void leven_garb_on()
{
    printf("Running leven garb online\n");
    char *functionPath = COMPONENT_FUNCTION_PATH;
    int DIntSize = getDIntSize();
    int inputsDevotedToD = getInputsDevotedToD();
    int numGarbInputs = levenNumGarbInputs();

    /* Set inputs */
    int *garbInputs = allocate_ints(numGarbInputs);

    /* The first inputsDevotedToD inputs are 0 to l+1 in binary */
    for (int i = 0; i < l + 1; i++) 
        convertToBinary(i, garbInputs + (DIntSize) * i, DIntSize);

    for (int i = inputsDevotedToD; i < numGarbInputs; i++) {
        garbInputs[i] = rand() % 2;
        printf("%d", garbInputs[i]);
    }
    printf("\n");

    uint64_t tot_time;
    garbler_online(functionPath, "files/garbler_gcs", garbInputs, numGarbInputs, numCircuits, &tot_time);
    free(garbInputs);
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
    block *inputLabels = allocate_blocks(2*n);
    block *outputMap = allocate_blocks(2*m);
    buildLevenshteinCircuit(&gc, inputLabels, outputMap, outputWires, l, m);
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);
    
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
    uint64_t tot_time;
    garbler_classic_2pc(&gc, &imap, outputMap, numGarbInputs, numEvalInputs,
                        inputs, &tot_time);

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
    uint64_t tot_time;
    evaluator_classic_2pc(inputs, outputs, numGarbInputs, numEvalInputs, &tot_time);

    /* Results */
    printf("Output: ");
    for (int i = 0; i < m; i++) 
        printf("%d", outputs[i]);
    printf("\n");

    free(inputs);
    free(outputs);
}
