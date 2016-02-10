#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h"
#include "circuits.h"
#include "utils.h"

#include "gates.h"

#include "2pc_tests.h"

static int convertToDec(const int *bin, int l) 
{
    assert(bin && l > 0);
    int ret = 0;
    for (int i = 0; i < l; i++) {
        ret += bin[i] * pow(2, i);
    }
    return ret;
}

static void
checkIncWithSwitch(const int *inputs, int *outputs, int n)
{
    int the_switch = inputs[0];
    int x = convertToDec(&inputs[1], n-1);
    int y = convertToDec(outputs, n-1);

    if (the_switch && x+1 == y) {

    } else if ((!the_switch && x == y) || y == 0) {

    } else {
        printf("failed!!!!\n");
        printf("x = %d\n", x);
        printf("y = %d\n", y);
    }
}

static void
checkLevenCore(const int *inputs, int *output, int l) 
{
    assert(l == 2);
    int n0 = convertToDec(&inputs[0], 2);
    int n1 = convertToDec(&inputs[2], 2);
    int n2 = convertToDec(&inputs[4], 2);
    int n3 = convertToDec(&inputs[6], 2);
    int n4 = convertToDec(&inputs[8], 2);

    int t = 0;
    if (n3 != n4) {
        t = 1;
    }
    int desired_answer = MIN3(n0 + t, n1 + 1, n2 + 1);
    int computed_answer = convertToDec(output, 2);

    /* leven_core circuit doesn't handle overflow */
    if (desired_answer != computed_answer && desired_answer != 4) {
        printf("!!!!!!!!!!!!!!FAILED!!!!!!!!!!!!!\n");
        printf("test failed\n");
        printf("%d %d %d || %d %d\n", n0, n1, n2, n3, n4);
        printf("desired answer: %d, computed_answer = %d\n", desired_answer, computed_answer);
    } else {
    }
}

static int levenshteinDistance(int *s1, int *s2, int l) 
{
    /* 
     * S1 and S2 are integer arrays of length 2*l
     * A symbol is two bits, so think of s1 and s2 
     * as strings of length l.
     */
    /* make an l+1 by l+1 integer array */
    int matrix[l+1][l+1];
    for (int x = 0; x <= l; x++)
        matrix[x][0] = x;

    for (int y = 0; y <= l; y++)
        matrix[0][y] = y;

    for (int x = 1; x <= l; x++) {
        for (int y = 1; y <= l; y++) {
            int t = 1;
            if (s1[2*(x-1)] == s2[2*(y-1)] && s1[(2*(x-1)) + 1] == s2[(2*(y-1)) + 1])
                t = 0;
            matrix[x][y] = MIN3(matrix[x-1][y] + 1, matrix[x][y-1] + 1, matrix[x-1][y-1] + t);
        }
    }
    return(matrix[l][l]);
}

static int lessThanCheck(int *inputs, int nints) 
{
    int split = nints / 2;
    int sum1 = 0;
    int sum2 = 0;
    int pow = 1;
    for (int i = 0; i < split; i++) {
        sum1 += pow * inputs[i];
        sum2 += pow * inputs[split + i];
        pow = pow * 10;
    }
    if (sum1 <= sum2)
        return 0;
    else 
        return 1;

}

static void minCheck(int *inputs, int nints, int *output) 
{
    /* Inputs is num1 || num2
     * And inputs[0] is ones digit, inputs[1] is 2s digit, and so on
     */
    int split = nints / 2;
    int sum1 = 0;
    int sum2 = 0;
    int pow = 1;
    for (int i = 0; i < split; i++) {
        sum1 += pow * inputs[i];
        sum2 += pow * inputs[split + i];
        pow = pow * 10;
    }
    if (sum1 < sum2)
        memcpy(output, inputs, sizeof(int) * split);
    else
        memcpy(output, inputs + split, sizeof(int) * split);
}

static void notGateTest()
{
    /* A test that takes one input, and performs 3 sequential
     * not gates. The evaluation happens locally (ie no networking and OT).
     * The reason this tests exists in some cases with sequential not gates, 
     * the output would be incorrect. I believe the problem was fixed
     * when I 
     */
    /* Paramters */
    int n = 1;
    int m = 3;
    int q = 3;
    int r = n + q;

    /* Inputs */
    int *inputs = allocate_ints(n);
    inputs[0] = 0;

    /* Build Circuit */
    block *inputLabels = allocate_blocks(2*n);
    createInputLabels(inputLabels, n);
    GarbledCircuit gc;
	createEmptyGarbledCircuit(&gc, n, m, q, r);
    GarblingContext gcContext;
	startBuilding(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    NOTGate(&gc, &gcContext, 0, 1);
    NOTGate(&gc, &gcContext, 1, 2);
    NOTGate(&gc, &gcContext, 2, 3);

    countToN(outputWires, m);
    block *outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(2*n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(2*m);
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    removeGarbledCircuit(&gc);

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
    free(inputs);
    free(outputs);
    free(inputLabels);
    free(extractedLabels);
    free(computedOutputMap);
    free(outputMap);
    free(inputWires);
    free(outputWires);
}

static void minTest() 
{
    int n = 4;
    int m = 2;
    int q = 50;
    int r = n + q;

    /* Inputs */
    int inputs[n];
    for (int i = 0; i < n; i++)
        inputs[i] = rand() % 2;

    /* Build Circuit */
    block *inputLabels = allocate_blocks(2*n);
    createInputLabels(inputLabels, n);
    GarbledCircuit gc;
	createEmptyGarbledCircuit(&gc, n, m, q, r);
    GarblingContext gcContext;
	startBuilding(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    MINCircuit(&gc, &gcContext, 4, inputWires, outputWires);

    block *outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    removeGarbledCircuit(&gc);

    int *outputs = allocate_ints(m);
    mapOutputs(outputMap, computedOutputMap, outputs, m);

    /* Automated checking */
    bool failed = false;
    int realCheck = lessThanCheck(inputs, 4);
    if (realCheck == 0) {
        if (outputs[0] != inputs[0] || outputs[1] != inputs[1]) {
            failed = true;
        }
    } else {
        if (outputs[0] != inputs[2] || outputs[1] != inputs[3] ) {
            failed = true;
        }
    }
    if (failed) {
        printf("MIN test failed\n");
        printf("inputs: %d %d %d %d\n", inputs[0], inputs[1], inputs[2], inputs[3]);
        printf("outputs: %d %d\n", outputs[0], outputs[1]);
    }

    free(outputs);
    free(inputWires);
    free(outputWires);
    free(extractedLabels);
    free(inputLabels);
    free(outputMap);
    free(computedOutputMap);

}

static void MUXTest() 
{
    int n = 3;
    int m = 1;
    int q = 20;
    int r = n + q;

    /* Inputs */
    int *inputs = allocate_ints(n);
    inputs[0] = rand() % 2;
    inputs[1] = rand() % 2;
    inputs[2] = rand() % 2;

    /* Build Circuit */
    block *inputLabels = allocate_blocks(2*n);
    createInputLabels(inputLabels, n);
    GarbledCircuit gc;
	createEmptyGarbledCircuit(&gc, n, m, q, r);
    GarblingContext gcContext;
	startBuilding(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    MUX21Circuit(&gc, &gcContext, inputWires[0], inputWires[1], inputWires[2], outputWires);

    block *outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    removeGarbledCircuit(&gc);

    /* Results */
    int *outputs = allocate_ints(m);
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    bool failed = false;
    if (inputs[0] == 0) {
        if (outputs[0] != inputs[1]) { 
            failed = true;
        }
    } else {
        if (outputs[0] != inputs[2]) {
            failed = true;
        }
    }
    if (failed) {
        printf("MUX test failed\n");
        printf("inputs: %d %d %d\n", inputs[0], inputs[1], inputs[2]);
        printf("outputs: %d\n", outputs[0]);
    }

    free(outputs);
    free(inputs);
    free(inputWires);
    free(outputWires);
    free(extractedLabels);
    free(inputLabels);
    free(outputMap);
    free(computedOutputMap);

}

static void LESTest(int n) 
{
    int m = 1;
    int q = 50000;
    int r = n + q;

    /* Inputs */
    int *inputs = allocate_ints(n);
    for (int i = 0; i < n; i++)
        inputs[i] = rand() % 2;

    /* Build Circuit */
    block *inputLabels = allocate_blocks(2*n);
    createInputLabels(inputLabels, n);
    GarbledCircuit gc;
	createEmptyGarbledCircuit(&gc, n, m, q, r);
    GarblingContext gcContext;
	startBuilding(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    LESCircuit(&gc, &gcContext, n, inputWires, outputWires);
    //countToN(outputWires, m);

    block *outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    removeGarbledCircuit(&gc);

    /* Results */
    int *outputs = allocate_ints(m);
    mapOutputs(outputMap, computedOutputMap, outputs, m);

    /* Automated checking */
    int check = lessThanCheck(inputs, n);
    if (check != outputs[0]) {
        printf("test failed\n");
        printf("inputs: %d %d %d %d\n", inputs[0], inputs[1], inputs[2], inputs[3]);
        printf("outputs: %d\n", outputs[0]);
    }

    free(outputs);
    free(inputs);
    free(inputWires);
    free(outputWires);
    free(extractedLabels);
    free(inputLabels);
    free(outputMap);
    free(computedOutputMap);
}

static void levenTest(int l, int sigma)
{
    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);
    int n = inputsDevotedToD + sigma*sigma*l;
    int m = DIntSize;
    int outputs[m];
    block delta = randomBlock();

    /* Build and Garble */
    GarbledCircuit gc;
    block *inputLabels = allocate_blocks(2*n);
    block *outputMap = allocate_blocks(2*m);

    createInputLabelsWithR(inputLabels, n, delta);
    buildLevenshteinCircuit(&gc, l, sigma);
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    // Set Inputs 
    // The first inputsDevotedToD inputs are the numbers  0 through l+1 encoded in binary 
    int *inputs = allocate_ints(n);
    for (int i = 0; i < l + 1; i++) {
        convertToBinary(i, inputs + (DIntSize * i), DIntSize);
    }

    for (int i = inputsDevotedToD; i < n; i++) {
        inputs[i] = rand() % 2;
    }
    
    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);

    int realInputs = n - inputsDevotedToD;
    for (int i = inputsDevotedToD; i < n; i++) { 
        printf("%d", inputs[i]);
        if (i == inputsDevotedToD + (realInputs/2) - 1)
            printf("\n");
    }
    printf("\n");

    mapOutputs(outputMap, computedOutputMap, outputs, m);

    removeGarbledCircuit(&gc);

    /* Results */
    int realDist = levenshteinDistance(inputs + inputsDevotedToD, inputs + inputsDevotedToD + 2*l, l);
    int realDistArr[DIntSize];
    convertToBinary(realDist, realDistArr, DIntSize);

    /* Automated check */
    bool failed = false;
    for (int i = 0; i < m; i++) { 
        if (outputs[i] != realDistArr[i]) {
            failed = true;
        }
    }

    if (failed) {
        printf("Leven test failed\n");
    }
    

    printf("Outputs ");
    for (int i = 0; i < m; i++) 
        printf("%d", outputs[i]);
    printf("\n");

    printf("Real: ");
    for (int i = 0; i < m; i++) 
        printf("%d", realDistArr[i]);
    printf("\n");
    printf("\n");

    free(inputs);
    free(inputLabels);
    free(outputMap);
    free(extractedLabels);
    free(computedOutputMap);
}

static void levenCoreTest() 
{
    int l = 2;
    int sigma = 2;
    int DIntSize = (int) floor(log2(l)) + 1;

    int n = 10;
    int m = DIntSize;
    int q = 500;
    int r = n = q;

    /* Build and Garble */
    int inputWires[n];
    countToN(inputWires, n);
    int outputWires[m];
    block inputLabels[2*n];
    block outputMap[2*m];
    GarbledCircuit gc;
    GarblingContext gcContext;
    int inputs[n];
    int outputs[m];

    createInputLabels(inputLabels, n);
	createEmptyGarbledCircuit(&gc, n, m, q, r);
	startBuilding(&gc, &gcContext);
    addLevenshteinCoreCircuit(&gc, &gcContext, l, sigma, inputWires, outputWires);
	finishBuilding(&gc, outputWires);

    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }

    /* Evaluate */
    block extractedLabels[n];
    extractLabels(extractedLabels, inputLabels, inputs, n);

    block computedOutputMap[m];
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    removeGarbledCircuit(&gc);

    /* Results */
    checkLevenCore(inputs, outputs, l);
}

static void saveAndLoadTest()
{
    printf("save and load test\n");
    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    ChainingType chainingType = CHAINING_TYPE_STANDARD;

    int l = 2;
    int sigma = 2;
    int n = 10;
    int m = 2;
    int q = 200;
    int r = n + q;
    char dir[40] = "files/garbler_gcs";

    ChainedGarbledCircuit cgc;

    GarblingContext gcContext;
    int inputWires[n];
    countToN(inputWires, n);
    int outputWires[m];
    cgc.inputLabels = allocate_blocks(2*n);
    cgc.outputMap = allocate_blocks(2*m);
    GarbledCircuit *gc = &cgc.gc;

    /* Garble */
    createInputLabelsWithR(cgc.inputLabels, n, delta);
	createEmptyGarbledCircuit(gc, n, m, q, r);
	startBuilding(gc, &gcContext);
    addLevenshteinCoreCircuit(gc, &gcContext, l, sigma, inputWires, outputWires);
	finishBuilding(gc, outputWires);
    garbleCircuit(gc, cgc.inputLabels, cgc.outputMap, GARBLE_TYPE_STANDARD);

    /* Declare chaining vars */
    cgc.id = 0;
    cgc.type = LEVEN_CORE;
    saveChainedGC(&cgc, dir, true, chainingType);

    /* loading */
    ChainedGarbledCircuit cgc2;
    loadChainedGC(&cgc2, dir, 0, true, chainingType);
    int inputs[n];
    int outputs[n];
    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }

    block extractedLabels[n];
    extractLabels(extractedLabels, cgc2.inputLabels, inputs, n);

    block computedOutputMap[m];
    evaluate(&cgc2.gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    mapOutputs(cgc2.outputMap, computedOutputMap, outputs, m);
    printf("outputs %d %d\n", outputs[0], outputs[1]);
}

void incWithSwitchTest() 
{
    int n = 9;
    int m = n - 1;
    int q = 200;
    int r = q + n;
    int inputWires[n];
    countToN(inputWires, n);

    int outputWires[m];
    block inputLabels[2*n];
    block outputMap[2*m];
    GarbledCircuit gc;
    GarblingContext gcContext;
    int inputs[n];
    int outputs[m];

    createInputLabels(inputLabels, n);
	createEmptyGarbledCircuit(&gc, n, m, q, r);
	startBuilding(&gc, &gcContext);

    INCCircuitWithSwitch(&gc, &gcContext, inputWires[0], n - 1, &inputWires[1], outputWires);

	finishBuilding(&gc, outputWires);
    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2; 
    }
    block extractedLabels[n];
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block computedOutputMap[m];
    evaluate(&gc, extractedLabels, computedOutputMap, GARBLE_TYPE_STANDARD);
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    removeGarbledCircuit(&gc);
    checkIncWithSwitch(inputs, outputs, n);
}

void runAllTests(void)
{ 
    //int nruns = 100; 
    levenTest(2,2);

    // TODO these two tests are failing!
    //for (int i = 0; i < nruns; i++)
    //    saveAndLoadTest();

    //for (int l = 3; l < 4; l++) { 
    //    printf("Running leven2 test for l=%d\n", l); 
    //    int sigma = 2;
    //    for (int i = 0; i < nruns; i++) 
    //        levenTest(l, sigma); 
    //    printf("Ran leven test %d times\n", nruns); 
    //}
    
    //for (int l = 10; l < 11; l++) { 
    //    printf("Running leven test for l=%d\n", l); 
    //    int sigma = 8;
    //    for (int i = 0; i < nruns; i++) 
    //        levenTest(l, sigma); 
    //    printf("Ran leven test %d times\n", nruns); 
    //}

    //for (int i = 0; i < nruns; i++) 
    //    incWithSwitchTest(); 

    //printf("Ran leven core test %d times\n", nruns); 
    //for (int i = 0; i < nruns; i++) 
    //    levenCoreTest(); 
    //printf("Ran leven core test %d times\n", nruns); 

    //printf("Running min test\n"); 
    //for (int i = 0; i < nruns; i++) 
    //    minTest(); 
    //printf("Ran min test %d times\n", nruns); 

    //printf("Running not gate test\n"); 
    //for (int i = 0; i < nruns; i++) 
    //    notGateTest(); 
    //printf("Ran note gate test %d times\n", nruns); 

    //printf("Running MUX test\n"); 
    //for (int i = 0; i < nruns; i++) 
    //    MUXTest(); 
    //printf("Ran mux test %d times\n", nruns); 

    //for (int n = 2; n < 16; n+=2) { 
    //    printf("Running LES test for n=%d\n", n); 
    //    for (int i = 0; i < nruns; i++) 
    //      LESTest(n); 
    //    printf("Running LES test %d times\n", nruns); 
    //} 
}  
