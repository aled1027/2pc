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

void pcrun_local() 
{
    // source from AM's original 2pc
    printf("Running 2pcrun_local()\n");
    srand(time(NULL));
    srand_sse(time(NULL));

    block delta;
    delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    GarbledCircuit gc; 
    block *inputLabels, *extractedLabels;
    block *outputMap, *computedOutputMap;
    int *inputs;
    int *outputVals;
    pcbuildCircuit(&gc, &delta);

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

void test() 
{
    GarbledCircuit gc;
    block delta;
    block *inputLabels, *extractedLabels, *outputMap, *computedOutputMap;
    int *input, *output;

    delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    buildAESRoundComponentCircuit(&gc, false); // doesn't work
    //pcbuildCircuit(&gc, &delta); // works

    inputLabels = (block *) memalign(128, sizeof(block) * 2 * gc.n);
    extractedLabels = (block *) memalign(128, sizeof(block) * gc.n);
    outputMap = (block *) memalign(128, sizeof(block) * 2 * gc.m);
    computedOutputMap = (block *) memalign(128, sizeof(block) * gc.m);
    input = (int *) malloc(sizeof(int) * gc.n);
    output = (int *) malloc(sizeof(int) * gc.m);

    for (int j=0; j<256; j++) {
        input[j] = rand() % 2;
    }

    // TODO add constant delta!
    garbleCircuit(&gc, inputLabels, outputMap);
    extractLabels(extractedLabels, inputLabels, input, gc.n);
    evaluate(&gc, extractedLabels, computedOutputMap);
    mapOutputs(outputMap, computedOutputMap, output, gc.m);

    printf("(");
    for (int j=0; j<128; j++) {
        printf("%d, ", output[j]);
    }
    printf(")\n");
}

int 
main(int argc, char* argv[]) 
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

    if (strcmp(argv[1], "eval_online") == 0) {
        printf("Running eval online\n");
        evaluator_run();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        printf("Running garb online\n");
        garbler_run(function_path);
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        printf("Running garb offline\n");
        garbler_offline();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        printf("Running val offline\n");
        evaluator_offline();
    } else if (strcmp(argv[1], "tests") == 0) {
        //pcrun_local();
        //jgmain();
        test();
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


