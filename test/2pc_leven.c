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
const int sigma = 8;
const int l = 5; // if you change this, you need to change the json as well (use the script)
char *COMPONENT_FUNCTION_PATH = "functions/leven_8.json"; 

static int getDIntSize() { return (int) floor(log2(l)) + 1; }
static int getInputsDevotedToD() { return getDIntSize() * (l+1); }
static int getN() { return getInputsDevotedToD() + (2*sigma*l); }
int levenNumOutputs() { return getDIntSize(); }
int levenNumEvalInputs() { return sigma * l; }
int levenNumGarbInputs() { return getN() - levenNumEvalInputs(); }
int levenNumCircs() { return l * l; }
static int getCoreN() { return (3 * getDIntSize()) + (2 * sigma); }
static int getCoreM() { return getDIntSize(); }
static int getCoreQ() { return 10000; } // TODO figure out this number

void
leven_garb_off(ChainingType chainingType) 
{
    printf("Running leven garb offline\n");
    printf("l = %d, sigma = %d\n", l, sigma);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    int coreN = getCoreN();
    int coreM = getCoreM();
    int coreQ = getCoreQ();
    int coreR = coreN + coreQ;
    int numCircuits = levenNumCircs();
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
	    createEmptyGarbledCircuit(gc, coreN, coreM, coreQ, coreR);
	    startBuilding(gc, &gcContext);
        addLevenshteinCoreCircuit(gc, &gcContext, l, sigma, inputWires, outputWires);
	    finishBuilding(gc, outputWires);
        if (chainingType == CHAINING_TYPE_SIMD) {
            createSIMDInputLabelsWithRForLeven(&chainedGCs[i], delta, l);
        } else {
            createInputLabelsWithR(chainedGCs[i].inputLabels, coreN, delta);
        }

        garbleCircuit(gc, chainedGCs[i].inputLabels, chainedGCs[i].outputMap,
                      GARBLE_TYPE_STANDARD);

        /* Declare chaining vars */
        chainedGCs[i].id = i;
        chainedGCs[i].type = LEVEN_CORE;
    }

    if (chainingType == CHAINING_TYPE_SIMD) {
        for (int i = 0; i < numCircuits; i++)
            generateOfflineChainingOffsets(&chainedGCs[i]);
    }

    int num_eval_inputs = levenNumEvalInputs();
    garbler_offline("files/garbler_gcs", chainedGCs, num_eval_inputs, numCircuits, chainingType);
}

void leven_garb_on(ChainingType chainingType)
{
    printf("Running leven garb online\n");
    printf("l = %d, sigma = %d\n", l, sigma);
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
    int numCircuits = levenNumCircs();
    garbler_online(functionPath, "files/garbler_gcs", garbInputs, numGarbInputs, 
            numCircuits, &tot_time, chainingType);
    free(garbInputs);
}

void
leven_garb_full(void)
{
    /* Runs the garbler for a full circuit of levenshtein distance. 
     * The only paramter is the integer l, which defines
     * the length of the input string for each party.
     * The alphabet is of size 4, i.e. 2 bits, so the actual length
     * of each party's input string is 2*l bits.
     */
    /* int sigma = 2; */
    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);
    int n = inputsDevotedToD + 2*2*l;
    int numEvalInputs = 2*l;
    int numGarbInputs = n - numEvalInputs;
    int m = DIntSize;
    int *inputs = calloc(numGarbInputs, sizeof(int));

    /* The first inputsDevotedToD inputs are the numbers 0 through l+1 encoded
     * in binary */
    for (int i = 0; i < l + 1; i++) {
        convertToBinary(i, inputs + (DIntSize) * i, DIntSize);
    }
    for (int i = inputsDevotedToD; i < numGarbInputs; i++) {
        inputs[i] = rand() % 2;
    }

    /* Build and Garble */
    GarbledCircuit gc;
    int *outputWires = allocate_ints(m);
    block *outputMap = allocate_blocks(2*m);
    buildLevenshteinCircuit(&gc, l, sigma);

    OldInputMapping imap;
    newOldInputMapping(&imap, numGarbInputs, numEvalInputs);

    {
        uint64_t start, end;
        uint64_t tot_time = 0;
        start = current_time_();
        garbleCircuit(&gc, NULL, outputMap, GARBLE_TYPE_STANDARD);
        end = current_time_();
        fprintf(stderr, "Garble: %llu\n", end - start);
        tot_time += end - start;
        start = current_time_();
        garbler_classic_2pc(&gc, &imap, outputMap, numGarbInputs, numEvalInputs,
                            inputs, NULL);
        end = current_time_();
        fprintf(stderr, "Classic 2PC: %llu\n", end - start);
        fprintf(stderr, "Total: %llu\n", tot_time + end - start);
    }

    /* Results */
    removeGarbledCircuit(&gc);
    free(inputs);
    free(outputWires);

    deleteOldInputMapping(&imap);
    free(outputMap);
}

void
leven_eval_full(void)
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
    for (int i = 0; i < numEvalInputs; i++) { 
        inputs[i] = rand() % 2;
    }
    printf("\n");

    /* Online work */
    int *outputs = allocate_ints(m);
    uint64_t tot_time = 0;
    evaluator_classic_2pc(inputs, outputs, numGarbInputs, numEvalInputs, &tot_time);
    printf("Total: %llu\n", tot_time);

    /* Results */
    /* printf("Output: "); */
    /* for (int i = 0; i < m; i++)  */
    /*     printf("%d", outputs[i]); */
    /* printf("\n"); */

    free(inputs);
    free(outputs);
}
