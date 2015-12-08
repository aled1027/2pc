#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// #include "gc_comm.h"
// #include "common.h"
#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 
#include "tests.h"

#include "arg.h"
#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"

void pcbuildCircuit(GarbledCircuit *gc, block *delta)
{
    // source from AM's original 2pc
    srand(time(NULL));
    srand_sse(time(NULL));
    GarblingContext gctxt;

    int roundLimit = 10;
    int n = 128 * (roundLimit + 1);
    int m = 128;
    int q = 50000; //Just an upper bound
    int r = q;
    int inp[n];
    countToN(inp, n);
    int addKeyInputs[n * (roundLimit + 1)];
    int addKeyOutputs[n];
    int subBytesOutputs[n];
    int shiftRowsOutputs[n];
    int mixColumnOutputs[n];
    block labels[2 * n];
    block outputbs[2 * m];
    int outputs[1];

    createInputLabelsWithR(labels, n, delta);
    createEmptyGarbledCircuit(gc, n, m, q, r, labels);
    startBuilding(gc, &gctxt);

    countToN(addKeyInputs, 256);

    for (int round = 0; round < roundLimit; round++) {

        AddRoundKey(gc, &gctxt, addKeyInputs, addKeyOutputs);

        for (int i = 0; i < 16; i++) {
            SubBytes(gc, &gctxt, addKeyOutputs + 8 * i, subBytesOutputs + 8 * i);
        }

        ShiftRows(gc, &gctxt, subBytesOutputs, shiftRowsOutputs);

        for (int i = 0; i < 4; i++) {
            if (round == roundLimit - 1)
                MixColumns(gc, &gctxt,
                           shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
        }
        for (int i = 0; i < 128; i++) {
            addKeyInputs[i] = mixColumnOutputs[i];
            addKeyInputs[i + 128] = (round + 2) * 128 + i;
        }
    }
    finishBuilding(gc, &gctxt, outputbs, mixColumnOutputs);
}

void local_one_round_aes() 
{
    // source from AM's original 2pc
    printf("Running 2pcrun_local()\n");

    block delta;
    delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    GarbledCircuit gc; 
    block *inputLabels, *extractedLabels;
    block *outputMap, *computedOutputMap;
    int *inputs;
    int *outputVals;

    //pcbuildCircuit(&gc, &delta);
    buildAESRoundComponentCircuit(&gc, false,  &delta);

    inputLabels = (block *) memalign(128, sizeof(block) * 2 * gc.n);
    extractedLabels = (block *) memalign(128, sizeof(block) * gc.n);
    outputMap = (block *) memalign(128, sizeof(block) * 2 * gc.m);
    computedOutputMap = (block *) memalign(128, sizeof(block) * gc.m);
    inputs = (int *) malloc(sizeof(int) * gc.n);
    outputVals = (int *) malloc(sizeof(int) * gc.m);
    garbleCircuit(&gc, inputLabels, outputMap);

    for (int i = 0; i < gc.n; ++i) {
        inputs[i] = rand() % 2;
    }   

    extractLabels(extractedLabels, inputLabels, inputs, gc.n);
    evaluate(&gc, extractedLabels, computedOutputMap);

    mapOutputs(outputMap, computedOutputMap, outputVals, gc.m);

    printf("Output: ");
    for (int i = 0; i < gc.m; ++i) {
        printf("%d", outputVals[i]);
    }   
    printf("\n");
}

void local_two_round_aes() 
{
    // If this works, load gcs from disk, and try it. 
    // see where different from computation else where
    // We could call this the no-instructions/no-network test.

    printf("Running test\n");
    GarbledCircuit gc0;
    GarbledCircuit gc1;
    block delta;
    block *inputLabels0, *extractedLabels0, *outputMap0;
    block *inputLabels1, *outputMap1, *computedOutputMap0, *extractedLabels1, *computedOutputMap1;
    int *input, *output;

    delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    buildAESRoundComponentCircuit(&gc0, false, &delta); 
    buildAESRoundComponentCircuit(&gc1, false, &delta); 
    int n = 128*2;
    int m = 128;

    inputLabels0 = (block *) memalign(128, sizeof(block) * 2 * n);
    inputLabels1 = (block *) memalign(128, sizeof(block) * 2 * n);
    extractedLabels0 = (block *) memalign(128, sizeof(block) * n);
    extractedLabels1 = (block *) memalign(128, sizeof(block) * n);
    outputMap0 = (block *) memalign(128, sizeof(block) *  2*m);
    outputMap1 = (block *) memalign(128, sizeof(block) * 2*m);
    computedOutputMap0 = (block *) memalign(128, sizeof(block) * m);
    computedOutputMap1 = (block *) memalign(128, sizeof(block) * m);
    input = (int *) malloc(sizeof(int) * n);
    output = (int *) malloc(sizeof(int) * m);

    garbleCircuit(&gc0, inputLabels0, outputMap0);
    garbleCircuit(&gc1, inputLabels1, outputMap1);

    for (int j=0; j< 128*3; j++) {
        input[j] = rand() % 2;
    }

    // plug in inputs and evaluate circuit 0
    for (int j=0; j< 256; j++) {
        extractedLabels0[j] = inputLabels0[2*j + input[j]];
    }
    evaluate(&gc0, extractedLabels0, computedOutputMap0); 
    mapOutputs(outputMap0, computedOutputMap0, output, m);

    // plug in inputs and evaluate circuit 1
    for (int j=0; j<128; j++) {
        block A, B, offset; // C is offset
        A = outputMap0[2*j];
        B = inputLabels1[2*j];
        offset = xorBlocks(A,B);
        extractedLabels1[j] = xorBlocks(computedOutputMap0[j], offset);
    }

    for (int j=128; j< 256; j++) {
        extractedLabels1[j] = inputLabels1[2*j + input[j+128]];
    }

    evaluate(&gc1, extractedLabels1, computedOutputMap1);
    mapOutputs(outputMap1, computedOutputMap1, output, m);

    printf("output: ");
    for (int j=0; j<128; j++) {
        printf("%d", output[j]);
    }
    printf("\n");
}

int main(int argc, char* argv[]) 
{

    if (argc != 2) {
        printf("usage: %s eval\n", argv[0]);
        printf("usage: %s garb\n", argv[0]);
        printf("usage: %s tests\n", argv[0]);
        return -1;
    }

	srand(time(NULL));
    srand_sse(time(NULL));

    // specifies which function to run
    // char* function_path = "functions/22Adder.json";
    char* function_path = "functions/aes.json";
    int num = 100;

    if (strcmp(argv[1], "eval_online") == 0) {
        //printf("Running eval online\n");
        for (int j=0; j<num; j++) {
            evaluator_run();
            sleep(1);
        }
    } else if (strcmp(argv[1], "garb_online") == 0) {
        printf("Running garb online\n");
        for (int j=0; j<num; j++) {
            garbler_run(function_path);
        }

    } else if (strcmp(argv[1], "garb_offline") == 0) {
        printf("Running garb offline\n");
        garbler_offline();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        printf("Running val offline\n");
        evaluator_offline();
    } else if (strcmp(argv[1], "tests") == 0) {
        //pcrun_local();
        //jgmain();
        //local_one_round_aes();
        local_two_round_aes();
        //run_all_tests();
    } else {
        printf("usage: %s eval\n", argv[0]);
        printf("usage: %s garb\n", argv[0]);
        printf("usage: %s tests\n", argv[0]);
        return -1;
    } 
    //go();
    return 0;
}


