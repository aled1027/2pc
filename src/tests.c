#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "gc_comm.h"
#include "arg.h"
#include "common.h"

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "util.h"
#include "tests.h"
#include "2pc_common.h"

#include <stdbool.h>
#include <stdint.h>


int 
run_all_tests() 
{
    bool failed = false;
    printf("------------------\n");
    printf("Running all tests\n");
    printf("------------------\n");

    if (test2() == FAILURE) {
        printf("---test2() failed\n");
        failed = true;
    }

    if (simple_test() == FAILURE) {
        printf("---simple_test() failed\n");
        failed = true;
    }

    if (json_test() == FAILURE) {
        printf("---json_test() failed\n");
        failed = true;
    }

    //if (test_saving_reading() == FAILURE) {
    //    printf("---test_saving_reading() failed\n");
    //    failed = true;
    //}
    
    if (function_spec_test() == FAILURE) {
        printf("--- function_spec_test() failed\n");
        failed = true;
    }

    if (input_mapping_serialize_test() == FAILURE) {
        printf("--- input_mapping_serialize_test() failed\n");
        failed = true;
    } else {
        printf("--- input_mapping_serialize_test() passed\n");
    }

    if (failed) {
        printf("----------------\n");
        printf("At least one test failed\n");
        printf("----------------\n");
        return FAILURE;
    } else {
        printf("----------------\n");
        printf("All tests passed\n");
        printf("----------------\n");
        return SUCCESS;
    }
}

//int 
//test_saving_reading() 
//{
// ======REMOVING THESE FUNCTIONS, SO DONT NEED TEST ==========
//    GarbledCircuit gc, gc2;
//    buildAdderCircuit(&gc);
//    block* inputLabels = memalign(128, sizeof(block) * 2 * gc.n);
//    block* extractedLabels = memalign(128, sizeof(block) * gc.n);
//    block* outputMap = memalign(128, sizeof(block) * 2 * gc.m);
//    //block* computedOutputMap = memalign(128, sizeof(block) * gc.m);
//    block delta = randomBlock();
//    *((uint16_t *) (&delta)) |= 1;
//
//    garbleCircuit(&gc, inputLabels, outputMap, &delta);
//
//    saveGarbledCircuit(&gc,  "garbledCircuit.gc");
//    loadGarbledCircuit(&gc2, "garbledCircuit.gc");
//
//    bool failed = false;
//    if ((gc.n == gc2.n &&
//        gc.m == gc2.m &&
//        gc.q == gc2.q &&
//        gc.r == gc2.r) == false) {
//        printf("metadata failed in test_saving_reading\n");
//        failed = true;
//    }
//    if (gc.garbledGates[0].id != gc2.garbledGates[0].id) {
//        printf("garbledGates failed in test_saving_reading\n");
//        failed = true;
//    }
//
//    if (gc.garbledTable[0].table[0][0] != gc2.garbledTable[0].table[0][0]) {
//        printf("garbledTables failed in test_saving_reading\n");
//        failed = true;
//    }
//
//    if (gc.wires[0].id != gc2.wires[0].id) {
//        printf("wires failed in test_saving_reading\n");
//        failed = true;
//    }
//
//    free(inputLabels);
//    free(extractedLabels);
//    free(outputMap);
//
//    if (failed) {
//        return FAILURE;
//    } else {
//        return SUCCESS;
//    }
//}

int 
simple_test() 
{
    // Doesn't use function spec or instructions
    GarbledCircuit gc;
    buildAdderCircuit(&gc);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    block* inputLabels = memalign(128, sizeof(block) * 2 * gc.n);
    block* extractedLabels = memalign(128, sizeof(block) * gc.n);
    block* outputMap = memalign(128, sizeof(block) * 2 * gc.m);
    block* computedOutputMap = memalign(128, sizeof(block) * gc.m);
    int inputs[] = {1,1};
    int outputs[] = {-1,-1};

    garbleCircuit(&gc, inputLabels, outputMap, &delta);
	extractLabels(extractedLabels, inputLabels, inputs, gc.n);
    evaluate(&gc, extractedLabels, computedOutputMap);
	mapOutputs(outputMap, computedOutputMap, outputs, gc.m);

    bool failed = false;
    if (((inputs[0] ^ inputs[1]) == outputs[0] &&
            inputs[0] && inputs[1] == outputs[1]) == false) {
        failed = true;
    }

    free(inputLabels);
    free(extractedLabels);
    free(outputMap);
    free(computedOutputMap);
    removeGarbledCircuit(&gc); // frees memory in gc

    if (failed) {
        return FAILURE;
    } else {
        return SUCCESS;
    }
}

int
test2() 
{
//#ifdef FREE_XOR
//    printf("free xor turned on\n");
//#else
//    printf("free xor turned off\n");
//#endif
//
//#ifdef ROW_REDUCTION
//    printf("row reduction turned on\n");
//#else
//    printf("row reduction turned off\n"):
//#endif
//
//#ifdef DKC2
//    printf("DKC2 turned on\n");
//#else
//    printf("DKC2 turned off\n");
//#endif
//    printf("\n");

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
	int* outputVals3 = malloc(sizeof(int) * gc3.m);

    // 3.2 plug in 2-bit inputs
    inputs1[0] = 0;
    inputs1[1] = 1;

    inputs2[0] = 1;
    inputs2[1] = 1;

    // 3.3 evaluate gc1 and gc2
    // extractLabels replaces Oblivious transfer of labels
	extractLabels(extractedLabels1, inputLabels1, inputs1, gc1.n);
	evaluate(&gc1, extractedLabels1, computedOutputMap1);
	mapOutputs(outputMap1, computedOutputMap1, outputVals1, gc1.m);

	extractLabels(extractedLabels2, inputLabels2, inputs2, gc2.n);
	evaluate(&gc2, extractedLabels2, computedOutputMap2);
	mapOutputs(outputMap2, computedOutputMap2, outputVals2, gc2.m);

    // 3.5 Do the chaining
    block offset1 = xorBlocks(outputMap1[0], inputLabels3[0]); // would be sent by garbler
    extractedLabels3[0] = xorBlocks(computedOutputMap1[0], offset1); // eval-side computation

    block offset2 = xorBlocks(outputMap2[0], inputLabels3[2]); // woudl be sent by garbler
    extractedLabels3[1] = xorBlocks(computedOutputMap2[0], offset2); // eval-side computation

    // 3.6 evaluate the final garbled circuit
	evaluate(&gc3, extractedLabels3, computedOutputMap3);
	mapOutputs(outputMap3, computedOutputMap3, outputVals3, gc3.m);

    // 3.7 look at the output
    bool failed = false;
    if (((inputs1[0] ^ inputs1[1])  == outputVals1[0] && 
        (inputs1[0] && inputs1[1]) == outputVals1[1] && 
        (inputs2[0] ^ inputs2[1])  == outputVals2[0] && 
        (inputs2[0] && inputs2[1]) == outputVals2[1] && 
        (outputVals1[0] ^ outputVals2[0]) == outputVals3[0] && 
        (outputVals1[0] && outputVals2[0]) == outputVals3[1]) == false) {
        failed = true;
    }

    free(inputLabels1);
    free(outputMap1);
    free(inputs1);
    free(outputVals1);
    free(computedOutputMap1);
    free(extractedLabels1);

    free(inputLabels2);
    free(outputMap2);
    free(inputs2);
    free(outputVals2);
    free(computedOutputMap2);
    free(extractedLabels2);

    free(inputLabels3);
    free(outputMap3);
	free(outputVals3);
    free(computedOutputMap3);
    free(extractedLabels3);

    removeGarbledCircuit(&gc1); // frees memory in gc
    removeGarbledCircuit(&gc2); // frees memory in gc
    removeGarbledCircuit(&gc3); // frees memory in gc

    
    if (failed) 
        return FAILURE;
    else 
        return SUCCESS;
}

int
json_test() 
{
    char* path = "functions/22Adder.json"; 
    FunctionSpec function;

    if (load_function_via_json(path, &function) == FAILURE) {
        return FAILURE;
    }

    //print_function(&function);
    return SUCCESS;
}

int 
function_spec_test() 
{
    char *path, *buffer, *buffer2; 
    size_t buf_size;
    FunctionSpec function;
    Instructions* instructions = (Instructions*) malloc(sizeof(Instructions));

    path = "functions/22Adder.json"; 
    if (load_function_via_json(path, &function) == FAILURE) {
        return FAILURE;
    }

    buf_size = sizeof(int) + function.instructions.size*(sizeof(InstructionType) + sizeof(int)*6 + sizeof(block));
    buffer = malloc(buf_size);

    // write and then read from same buffer
    writeInstructionsToBuffer(&(function.instructions), buffer);
    readBufferIntoInstructions(instructions, buffer);

    // check if instructions and function.instructions are the same
    if (instructions->size != function.instructions.size) {
        printf("Instructions are of different size\n");
        return FAILURE;
    }

    for (int i=0; i<instructions->size; i++) {
        if (instructions->instr[i].type != function.instructions.instr[i].type) {
            printf("An instruction is of incorrect type\n");
            return FAILURE;
        } 
    }

    return SUCCESS;
}

int 
input_mapping_serialize_test() 
{
    char *buffer, *path;
    FunctionSpec function;
    InputMapping imap;
    size_t buf_size;

    path = "functions/22Adder.json"; 
    if (load_function_via_json(path, &function) == FAILURE) {
        return FAILURE;
    }

    buffer = malloc(MAX_BUF_SIZE);
    
    writeInputMappingToBuffer(&function.input_mapping, buffer);
    readBufferIntoInputMapping(&imap, buffer);

    if (function.input_mapping.size != imap.size) {
        printf("InputMappings are of a different size\n");
        return FAILURE;
    }

    printf("break here\n");
    for (int i=0; i<function.input_mapping.size; i++) {
        if (function.input_mapping.wire_id[i] != imap.wire_id[i]) {
        printf("InputMappings have a different wire id at index %d\n", i);
        return FAILURE;
        }
    }
    return SUCCESS;

}

