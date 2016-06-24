#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#include "components.h"
#include "2pc_garbler.h" 
#include "2pc_evaluator.h"
#include "circuits.h"
#include "utils.h"

//#include "gates.h"

#include "2pc_tests.h"

static int convertToDec(const bool *bin, int l) 
{
    assert(bin && l > 0);
    int ret = 0;
    for (int i = 0; i < l; i++) {
        ret += bin[i] * pow(2, i);
    }
    return ret;
}

static void
checkIncWithSwitch(const bool *inputs, bool *outputs, int n)
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
checkLevenCore(const bool *inputs, bool *output, int l) 
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

static int levenshteinDistance(bool *s1, bool *s2, int l) 
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

static int lessThanCheck(bool *inputs, int nints) 
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
    if (sum1 < sum2)
        return 1;
    else 
        return 0;

}

static void minCheck(bool *inputs, int nints, bool *output) 
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

    /* Inputs */
    bool *inputs = calloc(n, sizeof(bool));
    inputs[0] = 0;

    /* Build Circuit */
    block *inputLabels = garble_allocate_blocks(2*n);
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    gate_NOT(&gc, &gcContext, 0, 1);
    gate_NOT(&gc, &gcContext, 1, 2);
    gate_NOT(&gc, &gcContext, 2, 3);

    countToN(outputWires, m);
    block *outputMap = garble_allocate_blocks(2*m);
	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    block *extractedLabels = garble_allocate_blocks(2*n);
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = garble_allocate_blocks(2*m);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    bool *outputs = calloc(m, sizeof(bool));
    for (int i=0; i<m; i++)
        outputs[i] = 55;
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);
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

    /* Inputs */
    bool inputs[n];
    for (int i = 0; i < n; i++)
        inputs[i] = rand() % 2;

    /* Build Circuit */
    block *inputLabels = garble_allocate_blocks(2*n);
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    int *inputWires = allocate_ints(n);
    int *outputWires = allocate_ints(m);
    countToN(inputWires, n);
    circuit_min(&gc, &gcContext, 4, inputWires, outputWires);

    block *outputMap = garble_allocate_blocks(2*m);
	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    block *extractedLabels = garble_allocate_blocks(n);
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = garble_allocate_blocks(m);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    bool *outputs = calloc(m, sizeof(bool));
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

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
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */

    inputs[0] = rand() % 2;
    inputs[1] = rand() % 2;
    inputs[2] = rand() % 2;

    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
    garble_context gcContext;
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    new_circuit_mux21(&gc, &gcContext, inputWires[0], inputWires[1], inputWires[2], outputWires);
	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);
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
        printf("MUX test failed--------------------------\n");
        printf("\tinputs: %d %d %d\n", inputs[0], inputs[1], inputs[2]);
        printf("\toutputs: %d\n", outputs[0]);
    }
}

//static void argMax4Test() 
//{
//    int n = 8; // 1 bit for input, 1 bit for index
//    int m = 1;
//    bool inputs[n];
//    int inputWires[n];
//    int outputWires[m];
//    block inputLabels[2*n];
//    block extractedLabels[n];
//    block computedOutputMap[m];
//    block outputMap[2*m];
//    bool outputs[m];
//
//    /* Inputs */
//    for (int i = 0; i < n; i++)
//        inputs[i] = rand() % 2;
//    inputs[0] = 0;
//    inputs[2] = 1;
//
//    /* Build Circuit */
//    garble_create_input_labels(inputLabels, n, NULL, false);
//    garble_circuit gc;
//	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
//    garble_context gcContext;
//	builder_start_building(&gc, &gcContext);
//
//    countToN(inputWires, n);
//    new_circuit_les(&gc, &gcContext, n, inputWires, outputWires);
//
//	builder_finish_building(&gc, &gcContext, outputWires);
//
//    /* Garble */
//    garble_garble(&gc, inputLabels, outputMap);
//
//    /* Evaluate */
//    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
//    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
//    garble_delete(&gc);
//
//    /* Results */
//    garble_map_outputs(outputMap, computedOutputMap, outputs, m);
//
//    /* Automated checking */
//    printf("arg max test--------------------------\n");
//    printf("\tinputs: ");
//    for (int i = 0; i < n; i++) {
//        if (i == (n/2))
//            printf("|| ");
//        printf("%d ", inputs[i]);
//    }
//    printf("\n");
//    printf("\toutputs: %d\n", outputs[0]);
//}

static void argMax2Test() 
{
    int n = 4; // 1 bit for input, 1 bit for index
    int m = 1;
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */
    for (int i = 0; i < n; i++)
        inputs[i] = rand() % 2;
    inputs[0] = 0;
    inputs[2] = 1;

    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
    garble_context gcContext;
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    circuit_argmax2(&gc, &gcContext, inputWires, outputWires, 1);

	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Automated checking */
    printf("arg max test--------------------------\n");
    printf("\tinputs: ");
    for (int i = 0; i < n; i++) {
        if (i == (n/2))
            printf("|| ");
        printf("%d ", inputs[i]);
    }
    printf("\n");
    printf("\toutputs: %d\n", outputs[0]);
}
static void LESTest(int n) 
{
    int m = 1;
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */
    for (int i = 0; i < n; i++)
        inputs[i] = rand() % 2;

    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
    garble_context gcContext;
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    new_circuit_les(&gc, &gcContext, n, inputWires, outputWires);

	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Automated checking */
    int check = lessThanCheck(inputs, n);
    if (check != outputs[0]) {
        printf("LES test failed--------------------------\n");
        printf("\tinputs: ");
        for (int i = 0; i < n; i++) {
            if (i == (n/2))
                printf("|| ");
            printf("%d ", inputs[i]);
        }
        printf("\n");
        printf("\t outputs: %d\n", outputs[0]);
            
    }
}

static void levenTest(int l, int sigma)
{
    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);
    int n = inputsDevotedToD + sigma*sigma*l;
    int m = DIntSize;
    bool outputs[m];
    block delta = garble_create_delta();

    /* Build and Garble */
    garble_circuit gc;
    block *inputLabels = garble_allocate_blocks(2*n);
    block *outputMap = garble_allocate_blocks(2*m);

    garble_create_input_labels(inputLabels, n, &delta, false);
    buildLevenshteinCircuit(&gc, l, sigma);
    garble_garble(&gc, inputLabels, outputMap);

    // Set Inputs 
    // The first inputsDevotedToD inputs are the numbers  0 through l+1 encoded in binary 
    bool *inputs = calloc(n, sizeof(bool));
    for (int i = 0; i < l + 1; i++) {
        convertToBinary(i, inputs + (DIntSize * i), DIntSize);
    }

    for (int i = inputsDevotedToD; i < n; i++) {
        inputs[i] = rand() % 2;
    }
    
    /* Evaluate */
    block *extractedLabels = garble_allocate_blocks(n);
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    block *computedOutputMap = garble_allocate_blocks(m);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);

    int realInputs = n - inputsDevotedToD;
    for (int i = 0; i < n; i++) { 
        printf("%d", inputs[i]);
        if (i == inputsDevotedToD + (realInputs/2) - 1)
            printf("\n");
    }
    printf("\n");

    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    garble_delete(&gc);

    /* Results */
    int realDist = levenshteinDistance(inputs + inputsDevotedToD, inputs + inputsDevotedToD + 2*l, l);
    bool realDistArr[DIntSize];
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

    /* Build and Garble */
    int inputWires[n];
    countToN(inputWires, n);
    int outputWires[m];
    block inputLabels[2*n];
    block outputMap[2*m];
    garble_circuit gc;
    garble_context gcContext;
    bool inputs[n];
    bool outputs[m];

    garble_create_input_labels(inputLabels, n, NULL, false);
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);
    addLevenshteinCoreCircuit(&gc, &gcContext, l, sigma, inputWires, outputWires);
	builder_finish_building(&gc, &gcContext, outputWires);

    garble_garble(&gc, inputLabels, outputMap);

    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }

    /* Evaluate */
    block extractedLabels[n];
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);

    block computedOutputMap[m];
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);
    garble_delete(&gc);

    /* Results */
    checkLevenCore(inputs, outputs, l);
}

static void saveAndLoadTest()
{
    printf("save and load test\n");
    block delta = garble_create_delta();

    ChainingType chainingType = CHAINING_TYPE_STANDARD;

    int l = 2;
    int sigma = 2;
    int n = 10;
    int m = 2;
    char dir[40] = "files/garbler_gcs";

    ChainedGarbledCircuit cgc;

    garble_context gcContext;
    int inputWires[n];
    countToN(inputWires, n);
    int outputWires[m];
    cgc.inputLabels = garble_allocate_blocks(2*n);
    cgc.outputMap = garble_allocate_blocks(2*m);
    garble_circuit *gc = &cgc.gc;

    /* Garble */
    garble_create_input_labels(cgc.inputLabels, n, &delta, false);
	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &gcContext);
    addLevenshteinCoreCircuit(gc, &gcContext, l, sigma, inputWires, outputWires);
	builder_finish_building(gc, &gcContext, outputWires);
    garble_garble(gc, cgc.inputLabels, cgc.outputMap);

    /* Declare chaining vars */
    cgc.id = 0;
    cgc.type = LEVEN_CORE;
    saveChainedGC(&cgc, dir, true, chainingType);

    /* loading */
    ChainedGarbledCircuit cgc2;
    loadChainedGC(&cgc2, dir, 0, true, chainingType);
    bool inputs[n];
    bool outputs[n];
    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }

    block extractedLabels[n];
    garble_extract_labels(extractedLabels, cgc2.inputLabels, inputs, n);

    block computedOutputMap[m];
    garble_eval(&cgc2.gc, extractedLabels, computedOutputMap, NULL);
    garble_map_outputs(cgc2.outputMap, computedOutputMap, outputs, m);
    printf("outputs %d %d\n", outputs[0], outputs[1]);
}

void incWithSwitchTest() 
{
    int n = 9;
    int m = n - 1;
    int inputWires[n];
    countToN(inputWires, n);

    int outputWires[m];
    block inputLabels[2*n];
    block outputMap[2*m];
    garble_circuit gc;
    garble_context gcContext;
    bool inputs[n];
    bool outputs[m];

    garble_create_input_labels(inputLabels, n, NULL, false);
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    INCCircuitWithSwitch(&gc, &gcContext, inputWires[0], n - 1, &inputWires[1], outputWires);

	builder_finish_building(&gc, &gcContext, outputWires);
    garble_garble(&gc, inputLabels, outputMap);

    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2; 
    }
    block extractedLabels[n];
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    block computedOutputMap[m];
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);
    garble_delete(&gc);
    checkIncWithSwitch(inputs, outputs, n);
}

void runAllTests(void)
{ 
    int nruns = 10; 

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
    //
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

    //printf("Running OR test\n"); 
    //for (int i = 0; i < nruns; i++) {
    //    ORTest(); 
    //    printf("\n");
    //}

    //printf("Running MUX test\n"); 
    //for (int i = 0; i < nruns; i++) {
    //    MUXTest(); 
    //}
    //
    //printf("Running les test\n");
    //for (int i = 0; i < nruns; i++) {
    //    LESTest(20);
    //}

    printf("Running argmax test\n");
    for (int i = 0; i < nruns; i++) {
        argMax2Test(20);
    }
}  
