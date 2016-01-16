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

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

void convertToBinary(int x, int *arr, int narr)
{
    // Add case for 0
    int i = 0;
    while (x > 0) {
        arr[i] = (x % 2);
        x >>= 1;
        i++;
    }
    for (int j = i; j < narr; j++)
        arr[j] = 0;
}

int levenshteinDistance(int *s1, int *s2, int l) 
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

int lessThanCheck(int *inputs, int nints) 
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

void minCheck(int *inputs, int nints, int *output)
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

void notGateTest()
{
    /* A test that takes one input, and performs 3 sequential
     * not gates. The evaluation happens locally (ie no networking and OT).
     * The reason this tests exists in some cases with sequential not gates, 
     * the output would be incorrect. I believe the problem was fixed
     * when I 
     */
    /* Paramters */
    //printf("Running not gate test\n");
    int n = 1;
    int m = 3;
    int q = 3;
    int r = n + q;

    /* Inputs */
    int *inputs = allocate_ints(n);
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

void minTest() 
{
    printf("Running the min test\n");
    int n = 4;
    int m = 2;
    int q = 50;
    int r = n + q;

    /* Inputs */
    int inputs[n];
    inputs[0] = 0;
    inputs[1] = 1;
    inputs[2] = 1;
    inputs[3] = 0;

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
    MINCircuit(&gc, &gcContext, 4, inputWires, outputWires);

    OutputMap outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, &gcContext, outputMap, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap);

    /* Results */
    int *outputs = allocate_ints(m);
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    printf("Outputs: %d %d\n", outputs[0], outputs[1]);

    /* Automated Testing */
    //bool failed = false;
    //if (inputs[0] == 0) {
    //    if (outputs[0] != inputs[1]) { 
    //        failed = true;
    //    }
    //} else {
    //    if (outputs[0] != inputs[2]) {
    //        failed = true;
    //    }
    //}
    //if (failed) {
    //    printf("MUX test failed\n");
    //    printf("inputs: %d %d %d\n", inputs[0], inputs[1], inputs[2]);
    //    printf("outputs: %d\n", outputs[0]);
    //}

    //free(outputs);
    //free(inputs);
    //free(inputWires);
    //free(outputWires);
    //free(extractedLabels);
    //free(inputLabels);
    //free(outputMap);
    //free(computedOutputMap);

}

void MUXTest() 
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
    InputLabels inputLabels = allocate_blocks(2*n);
    createInputLabels(inputLabels, n);
    GarbledCircuit gc;
	createEmptyGarbledCircuit(&gc, n, m, q, r, inputLabels);
    GarblingContext gcContext;
	startBuilding(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    MUX21Circuit(&gc, &gcContext, inputWires[0], inputWires[1], inputWires[2], outputWires);

    OutputMap outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, &gcContext, outputMap, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap);

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

void LESTest(int n) 
{
    int m = 1;
    int q = 30000;
    int r = n + q;

    /* Inputs */
    int *inputs = allocate_ints(n);
    for (int i = 0; i < n; i++)
        inputs[i] = rand() % 2;

    //inputs[0] = 1;
    //inputs[1] = 0;
    //inputs[2] = 0;
    //inputs[3] = 1; 

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
    LESCircuit(&gc, &gcContext, n, inputWires, outputWires);
    //countToN(outputWires, m);

    OutputMap outputMap = allocate_blocks(2*m);
	finishBuilding(&gc, &gcContext, outputMap, outputWires);

    /* Garble */
    garbleCircuit(&gc, inputLabels, outputMap);

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap);

    /* Results */
    int *outputs = allocate_ints(m);
    mapOutputs(outputMap, computedOutputMap, outputs, m);
    /*printf("inputs: %d %d %d %d\n", inputs[0], inputs[1], inputs[2], inputs[3]);*/
    /*printf("outputs: %d\n", outputs[0]);*/

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

void levenTest(int l)
{
    printf("Running full leven garb\n");

    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);
    int n = inputsDevotedToD + 2*2*l;
    int core_n = (3 * DIntSize) + 4;
    int m = DIntSize;

    /* Build and Garble */
    GarbledCircuit gc;
    int *outputWires = allocate_ints(m);
    InputLabels inputLabels = allocate_blocks(2*n);
    OutputMap outputMap = allocate_blocks(2*m);
    buildLevenshteinCircuit(&gc, inputLabels, outputMap, outputWires, l, m);
    garbleCircuit(&gc, inputLabels, outputMap);

    /* Set Inputs */
    int *inputs = allocate_ints(n);
    // L = 3
    // TODO AUTO set inputsDevotedToD:
    inputs[0] = 0;
    inputs[1] = 0;
    inputs[2] = 1;
    inputs[3] = 0;
    inputs[4] = 0;
    inputs[5] = 1;
    inputs[6] = 1;
    inputs[7] = 1;

    //inputs[8] = 0;
    //inputs[9] = 1;
    //inputs[10] = 0;
    //inputs[11] = 0;
    //inputs[12] = 1;
    //inputs[13] = 1;
    //inputs[14] = 0;
    //inputs[15] = 0;
    //inputs[16] = 0;
    //inputs[17] = 1;
    //inputs[18] = 0;
    //inputs[19] = 0;

    for (int i = inputsDevotedToD; i < n; i++)
        inputs[i] = rand() % 2;

    /* Evaluate */
    block *extractedLabels = allocate_blocks(n);
    extractLabels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = allocate_blocks(m);
    evaluate(&gc, extractedLabels, computedOutputMap);

    /* Results */
    int *outputs = allocate_ints(m);
    mapOutputs(outputMap, computedOutputMap, outputs, m);

    /* Compute what the results should be */
    int realDist = levenshteinDistance(inputs + inputsDevotedToD, inputs + inputsDevotedToD + 2*l, l);
    int realDistArr[DIntSize];
    convertToBinary(realDist, realDistArr, DIntSize);

    /* Automated check */
    bool failed = false;
    for (int i = 0; i < m; i++) {
        if (outputs[i] != realDistArr[i])
            failed = true;
    }
    if (failed) {
        printf("Leven test failed\n");
        int realInputs = n - inputsDevotedToD;
        for (int i = inputsDevotedToD; i < n; i++) { 
            printf("%d", inputs[i]);
            if (i == inputsDevotedToD + (realInputs/2) - 1)
                printf("\n");
        }
        printf("\n");

        printf("Outputs ");
        for (int i = 0; i < m; i++) 
            printf("%d", outputs[i]);
        printf("\n");

        printf("Real: ");
        for (int i = 0; i < m; i++) 
            printf("%d", realDistArr[i]);
        printf("\n");
    }

    free(inputs);
    free(inputLabels);
    free(outputWires);
    free(outputMap);
    free(extractedLabels);
    free(computedOutputMap);
    free(outputs);

    /* Check results (for debugging) */
    //printf("real dist: %d\n", realDist);
    //printf("Inputs: ");
    //printf("\n");
    //int realInputs = n - inputsDevotedToD;
    //for (int i = inputsDevotedToD; i < n; i++) { 
    //    printf("%d", inputs[i]);
    //    if (i == inputsDevotedToD + (realInputs/2) - 1)
    //        printf("\n");
    //}
    //printf("\n");
    //printf("Real: ");
    //for (int i = 0; i < DIntSize; i++) 
    //    printf("%d", realDistArr[i]);
    //printf("\n");
    //printf("computed %d%d\n", outputs[655], outputs[659]);
}

int main(int argc, char *argv[]) 
{
	srand(time(NULL));
    srand_sse(time(NULL));
    assert(argc == 2);
    int nruns = 50;

    if (strcmp(argv[1], "leven") == 0) {
        for (int i = 0; i < nruns; i++)
            levenTest(3);

    } else if (strcmp(argv[1], "min") == 0) {
        printf("Running min test\n");
        minTest();

    } else if (strcmp(argv[1], "not") == 0) {
        for (int i = 0; i < nruns; i++)
            notGateTest();

    } else if (strcmp(argv[1], "mux") == 0) {
        printf("Running MUX test\n");
        for (int i = 0; i < nruns; i++)
            MUXTest();

    } else if (strcmp(argv[1], "les") == 0) {
        printf("Running LES test\n");
        for (int n = 2; n < 16; n+=2) {
            printf("n = %d\n", n);
            for (int i = 0; i < nruns; i++) 
              LESTest(n);
        }
    } else {
        printf("See test/2pc_misc_tests.c:main for usage\n");
    }
    
    return 0;
} 
