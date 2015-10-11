#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


#include "gc_comm.h"
#include "arg.h"
#include "circuit_builder.h"
#include "common.h"
#include "chaining.h"

#include "util.h"

#include "tests.h"


int run_all_tests() {
    bool failed = false;

    if (test2() == FAILURE) {
        printf("test2() failed\n");
        failed = true;
    }

    if (simple_test() == FAILURE) {
        printf("simple_test() failed\n");
        failed = true;
    }
    

    if (failed) {
        return FAILURE;
    } else {
        printf("All tests passed\n");
        return SUCCESS;
    }
}


int 
test_saving_reading() {
    printf("running test_saving\n");


    GarbledCircuit gc, gc2;
    block* inputLabels = memalign(128, sizeof(block) * 2 * gc.n);
    block* extractedLabels = memalign(128, sizeof(block) * gc.n);
    block* outputMap = memalign(128, sizeof(block) * 2 * gc.m);
    block* computedOutputMap = memalign(128, sizeof(block) * gc.m);
    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    buildAdderCircuit(&gc);
    garbleCircuit(&gc, inputLabels, outputMap, &delta);

    saveGarbledCircuit(&gc, "garbledCircuit.gc");
    readGarbledCircuit(&gc2, "garbledCircuit.gc");

    bool failed = false;
    if ((gc.n == gc2.n &&
        gc.m == gc2.m &&
        gc.q == gc2.q &&
        gc.r == gc2.r) == false) {
        printf("metadata failed in test_saving_reading\n");
        failed = true;
    }
    if (gc.garbledGates[0].id != gc2.garbledGates[0].id) {
        printf("garbledGates failed in test_saving_reading\n");
        failed = true;
    }

    if (gc.garbledTable[0].table[0][0] != gc2.garbledTable[0].table[0][0]) {
        printf("garbledTables failed in test_saving_reading\n");
        failed = true;
    }

    if (gc.wires[0].id != gc2.wires[0].id) {
        printf("wires failed in test_saving_reading\n");
        failed = true;
    }

    if (failed) {
        printf("test_saving_reading() failed\n");
        return FAILURE;
    } else {
        return SUCCESS;
    }
}

int 
simple_test() {
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
test2() {
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

int json_get_components(json_t *root) {
    // TODO need to populate a struct or something
    const char* type;
    int num, *circuit_ids;
    json_t *jComponents, *jComponent, *jType, *jNum, *jCircuit_ids, *jId;

    jComponents = json_object_get(root, "components");

    for (int i=0; i<json_array_size(jComponents); i++) {
        jComponent = json_array_get(jComponents, i);
        assert(json_is_object(jComponent));

        // get type
        jType = json_object_get(jComponent, "type");
        assert(json_is_string(jType));
        type = json_string_value(jType);

        // get num
        jNum = json_object_get(jComponent, "num");
        assert (json_is_integer(jNum));
        num = json_integer_value(jNum);

        // get circuit ids
        circuit_ids = malloc(sizeof(int) * num);
        jCircuit_ids = json_object_get(jComponent, "circuit_ids");
        assert (json_is_array(jCircuit_ids));
        for (int k=0; k<num; k++) {
            jId = json_array_get(jCircuit_ids, k);
            circuit_ids[k] = json_integer_value(jId);
        }
    }
    return SUCCESS;
}

void 
print_input_mapping(InputMapping* inputMapping) {
    printf("InputMapping:\n");
    for (int i=0; i<inputMapping->size; i++) {
        printf("%d -> (%d, %d)\n", inputMapping->input_idx[i], inputMapping->gc_id[i], inputMapping->wire_id[i]);
    }
}

void 
print_instructions(Instructions* instr) {
    printf("Instructions:\n");
    for (int i=0; i<instr->size; i++) {
        switch(instr->instr[i].type) {
            case EVAL:
                printf("EVAL %d\n", instr->instr[i].evCircId);
                break;
            case CHAIN:
                printf("CHAIN (%d, %d) -> (%d, %d)\n", instr->instr[i].chFromCircId, 
                        instr->instr[i].chFromWireId,
                        instr->instr[i].chToCircId,
                        instr->instr[i].chToWireId);
                break;
            default:
                printf("Not printing command\n");
        }
    }
}


int json_get_input_mapping(json_t *root, InputMapping* inputMapping) {
    /* "InputMapping: [
     *      { "gc_id": x, "wire_id: y"},
     *      { "gc_id": a, "wire_id: b"}
     * ]
     * 
     * where the ith element in the array is where the ith input is mapped to.
     * 
     */
    json_t *jInputMapping, *jMap, *jGcId, *jWireId;
    int size;

    jInputMapping = json_object_get(root, "InputMapping");
    assert(json_is_array(jInputMapping));
    size = json_array_size(jInputMapping); // should be equal to n

    // input_idx[i] maps to (gc_id[i], wire_id[i])
    inputMapping->input_idx = malloc(sizeof(int) * size);
    inputMapping->gc_id = malloc(sizeof(int) * size);
    inputMapping->wire_id = malloc(sizeof(int) * size);
    assert(inputMapping->input_idx && inputMapping->gc_id && inputMapping->wire_id);

    inputMapping->size = size;
    for (int i=0; i<size; i++) {
       jMap = json_array_get(jInputMapping, i);
       assert(json_is_object(jMap));

       inputMapping->input_idx[i] = i;

       jGcId = json_object_get(jMap, "gc_id");
       assert(json_is_integer(jGcId));
       inputMapping->gc_id[i] = json_integer_value(jGcId);

       jWireId = json_object_get(jMap, "wire_id");
       assert(json_is_integer(jWireId));
       inputMapping->wire_id[i] = json_integer_value(jWireId);
    }

    return SUCCESS;
}

static InstructionType 
get_instruction_type_from_string(const char* type) {
    if (strcmp(type, "EVAL") == 0) {
        return EVAL;
    } else if (strcmp(type, "CHAIN") == 0) {
        return CHAIN;
    } else {
        return INSTR_ERR;
    }
}

int json_get_instructions(json_t *root, Instructions *instructions) {
    json_t *jInstructions, *jInstr, *jGcId, *jPtr;
    int size;
    const char* sType;

    jInstructions = json_object_get(root, "instructions");
    assert(json_is_array(jInstructions));

    size = json_array_size(jInstructions); 
    instructions->size = size;
    instructions->instr = malloc(sizeof(Instruction) * size);

    for (int i=0; i<size; i++) {
        jInstr = json_array_get(jInstructions, i);
        assert(json_is_object(jInstr));

        jPtr = json_object_get(jInstr, "type");
        assert(json_is_string(jPtr));
        sType = json_string_value(jPtr);

        instructions->instr[i].type = get_instruction_type_from_string(sType);
        switch (instructions->instr[i].type) {
            case EVAL:
                jPtr = json_object_get(jInstr, "gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].evCircId = json_integer_value(jPtr);
                break;
            case CHAIN:
                jPtr = json_object_get(jInstr, "from_gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chFromCircId = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "from_wire_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chFromWireId = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chToCircId = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_wire_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chToWireId = json_integer_value(jPtr);
                break;
            default:
                fprintf(stderr, "Instruction %d was invalid: %s\n", i, sType);
                return FAILURE;
        }
    }
    return SUCCESS;
}

int
json_test() {
    /* tests use loading in function specification via json
     * uses jansson.h. See jansson docs for more details */

    // there exists a function to load directly from file, 
    // but I was getting runtime memory errors when using it.
    
    // load file into buffer
    //
    InputMapping inputMapping;  // memory allocated in json_get_input_mapping
    Instructions instructions; // memory allocated in json_get_instructions

    char* path = "functions/22Adder.json"; 
    long fs = filesize(path); 
    FILE *f = fopen(path, "r"); 
    if (f == NULL) {
        printf("Write: Error in opening file.\n");
        return -1;
    }   
    char *buffer = malloc(fs); 
    fread(buffer, sizeof(char), fs, f);

    // read buffer into json_t object
    json_t *jRoot;
    json_error_t error; 
    if (!(jRoot = json_loads(buffer, 0, &error))) {
        fprintf(stderr, "error load json on line %d: %s\n", error.line, error.text);
        return FAILURE;
    }
    free(buffer);

    if (json_get_components(jRoot) == FAILURE) {
        fprintf(stderr, "error loading json components");
        return FAILURE;
    }
    
    if (json_get_input_mapping(jRoot, &inputMapping) == FAILURE) {
        fprintf(stderr, "error loading json components");
        return FAILURE;
    }

    if (json_get_instructions(jRoot, &instructions) == FAILURE) {
        fprintf(stderr, "error loading json instructions");
        return FAILURE;
    }

    print_input_mapping(&inputMapping);
    print_instructions(&instructions);

    free(jRoot);
    printf("successful json_test\n");
    return SUCCESS;
}

