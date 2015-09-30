#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "gc_comm.h"
#include "arg.h"
#include "circuit_builder.h" /* buildAdderCircuit */
#include "common.h"

#include "util.h"


int
test() {
    printf("starting...\n");
    printf("\n");

#ifdef FREE_XOR
    printf("free xor turned on\n");
#else
    printf("free xor turned off\n");
#endif

#ifdef ROW_REDUCTION
    printf("row reduction turned on\n");
#else
    printf("row reduction turned off\n"):
#endif

#ifdef DKC2
    printf("DKC2 turned on\n");
#else
    printf("DKC2 turned off\n");
#endif
    printf("\n");

    // 0. Get a delta for everything
    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;
    
    // 1. build circuits
    GarbledCircuit gc1, gc2, gc3;

    buildAdderCircuit(&gc1);
    buildAdderCircuit(&gc2);
    buildAdderCircuit(&gc3);

    // 2. garble the circuits
    block* inputLabels1 = (block *) memalign(128, sizeof(block) * 2 * gc1.n);
    block* outputMap1 = (block *) memalign(128, sizeof(block) * 2 * gc1.m);
    garbleCircuit(&gc1, inputLabels1, outputMap1, &delta);

    block* inputLabels2 = (block *) memalign(128, sizeof(block) * 2 * gc2.n);
    block* outputMap2 = (block *) memalign(128, sizeof(block) * 2 * gc2.m);
    garbleCircuit(&gc2, inputLabels2, outputMap2, &delta);

    block* inputLabels3 = (block *) memalign(128, sizeof(block) * 2 * gc3.n);
    block* outputMap3 = (block *) memalign(128, sizeof(block) * 2 * gc3.m);
    garbleCircuit(&gc3, inputLabels3, outputMap3, &delta);

    // 3. evaluate circuits
    // 3.1 initialize vars
    block* computedOutputMap1 = memalign(128, sizeof(block)*gc1.m);
    block* extractedLabels1 = memalign(128, sizeof(block)*gc1.n);
    int* outputVals1 = malloc(sizeof(int) * gc1.m);
    int* inputs1 = malloc(sizeof(int) * gc1.n);

    block* computedOutputMap2 = memalign(128, sizeof(block)*gc2.m);
    block* extractedLabels2 = memalign(128, sizeof(block)*gc2.n);
    int* inputs2 = malloc(sizeof(int) * gc2.n);
    int* outputVals2 = malloc(sizeof(int) * gc2.m);

    block* computedOutputMap3 = malloc(sizeof(block)*gc3.m);
    block* extractedLabels3 = malloc(sizeof(block)*gc3.n);
	int* inputs3 = malloc(sizeof(int) * gc3.n);
	int* outputVals3 = malloc(sizeof(int) * gc3.m);

    // 3.2 plug in 2-bit inputs
    inputs1[0] = 1;
    inputs1[1] = 1;

    inputs2[0] = 1;
    inputs2[1] = 1;

    // 3.3 evaluate gc1 and gc2
    // extractLabels replaces Oblivious transfer of labels
	extractLabels(extractedLabels1, inputLabels1, inputs1, gc1.n);
	evaluate(&gc1, extractedLabels1, computedOutputMap1);
	mapOutputs(outputMap1, computedOutputMap1, outputVals1, gc1.m);
    printf("check it\n");

	extractLabels(extractedLabels2, inputLabels2, inputs2, gc2.n);
	evaluate(&gc2, extractedLabels2, computedOutputMap2);
	mapOutputs(outputMap2, computedOutputMap2, outputVals2, gc2.m);

    // 3.4 plug in output of gc1 and gc2 into gc3 and evalute
    inputs3[0] = outputVals1[0];
    inputs3[1] = outputVals2[0];
    
    block a_xor_delta = xorBlocks(outputMap1[0], delta);

    // 3.5 Do the chaining
    block offset1 = xorBlocks(outputMap1[0], inputLabels3[0]); // would be sent by garbler
    extractedLabels3[0] = xorBlocks(computedOutputMap1[0], offset1); // eval-side computation

    block offset2 = xorBlocks(outputMap2[0], inputLabels3[2]); // woudl be sent by garbler
    extractedLabels3[1] = xorBlocks(computedOutputMap2[0], offset2); // eval-side computation

    // 3.6 evaluate the final garbled circuit
	evaluate(&gc3, extractedLabels3, computedOutputMap3);
	mapOutputs(outputMap3, computedOutputMap3, outputVals3, gc3.m);

    // 3.7 look at the output
    printf("xor output1: %d\n", outputVals1[0]);
    printf("and output1: %d\n", outputVals1[1]);
    printf("xor output2: %d\n", outputVals2[0]);
    printf("and output2: %d\n", outputVals2[1]);
    printf("xor output3: %d\n", outputVals3[0]);
    printf("and output3: %d\n", outputVals3[1]);

    free(inputLabels1);
    free(outputMap1);
    free(inputLabels2);
    free(outputMap2);
    free(inputLabels3);
    free(outputMap3);
	free(inputs3);
	free(outputVals3);

    printf("...ending\n");
    return 0;
}

int main(int argc, char* argv[])
{
    test();
    return 0;
}


