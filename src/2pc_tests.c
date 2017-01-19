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
#include "ml_models.h"
#include "circuits.h"
#include "utils.h"

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

static int convertSignedToDec(const bool *bin, int l) 
{
    // convert signed binary to dec
    int ret = convertToDec(bin, l - 1);
    if (bin[l-1] == 0) {
        return ret;
    } else {
        return ret * -1;
    }
}

static void test_convert_to_signed_binary()
{

    int number = -1;
    int narr = 5;
    bool arr[narr];

    convertToSignedBinary(number, arr, narr);
    printf("number = %d, arr = [", number);
    for (uint32_t i = 0; i < narr; i++) {
        printf("%d, ", arr[i]);
    }
    printf("]\n");

}

static void test_mux() 
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
    if (inputs[0] == 0 && outputs[0] != inputs[1]) { 
            failed = true;
    } else if (inputs[0] == 1 && outputs[0] != inputs[2] ) {
            failed = true;
    }
    
    if (failed) {
        printf("MUX test failed--------------------------\n");
        printf("\tinputs: %d %d %d\n", inputs[0], inputs[1], inputs[2]);
        printf("\toutputs: %d\n", outputs[0]);
    }
}

static void test_argmax()
{
    printf("test argmax circuit\n");
    int num_len = 3;
    int array_size = 2; 
    int n = array_size * num_len;
    int m = 2 * num_len;

    bool inputs[n];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];
    int inputWires[n];
    int outputWires[m];

    // idx then value
    /* Inputs */
    for (int i = 0; i < n; ++i) {
        inputs[i] = 1;
    }
                
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    /* Add circuits */
    countToN(inputWires, n);
    circuit_argmax(&gc, &gcContext, inputWires, outputWires, array_size, num_len);

    /* Garble */
	builder_finish_building(&gc, &gcContext, outputWires);
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Print Results */
    printf("Inputs:");
    for (uint32_t i = 0; i < n; ++i) {
        if (0 == i % num_len) {
            printf(" |");
        }
        printf(" %d", inputs[i]);
    }
    printf("\n");

    for (uint32_t i = 0; i < m; ++i) {
        printf("outputs[%d] = %d\n", i, outputs[i]);
    }
    printf("\n");
}

static void test_select_circuit()
{
    printf("test select circuit\n");
    int num_len = 4;
    int array_size = 12; 
    int index_size = num_len;
    int n = (array_size * num_len) + index_size;
    int m = num_len;

    bool inputs[n];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];
    int inputWires[n];
    int outputWires[m];

    /* Inputs */
    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }
    inputs[n-4] = 0; // doesn't matter, sign bit
    inputs[n-3] = 0;
    inputs[n-2] = 1;
    inputs[n-1] = 0;
        
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    /* Add circuits */
    countToN(inputWires, n);
    circuit_select(&gc, &gcContext, num_len, array_size, index_size, inputWires, outputWires);

    /* Garble */
	builder_finish_building(&gc, &gcContext, outputWires);
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Print Results */
    printf("Inputs:");
    for (uint32_t i = 0; i < n; ++i) {
        if (0 == i % num_len) {
            printf(" |");
        }
        printf(" %d", inputs[i]);
    }
    printf("\n");

    for (uint32_t i = 0; i < m; ++i) {
        printf("outputs[%d] = %d\n", i, outputs[i]);
    }
    printf("\n");
}

static void test_naive_bayes() 
{
    printf("test decision tree\n");
    int num_len = 52;
    int num_classes = 6;
    int vector_size = 4;
    int domain_size = 4;

    int client_input_size = vector_size * num_len; 
    int C_size = num_classes * num_len;
    int T_size = num_classes * vector_size * domain_size * num_len;
    int n = client_input_size + C_size + T_size;
    printf("n = %d\n", n);
    
    if (n > 8000) {
        printf("not running\n");
        return;
    }

    int m = num_len;
    

    bool inputs[n];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */
    for (int i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }
        
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    build_naive_bayes_circuit(&gc, num_classes, vector_size, domain_size, num_len);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Print Results */
    printf("\nC, T, client inputs\n");
    printf("Inputs:");
    for (uint32_t i = 0; i < n; ++i) {
        if (i == C_size ||
            i == (C_size + T_size)) {
            printf(" | ");
        }
        printf("%d", inputs[i]);
    }
    printf("\n");

    printf("Outputs:");
    for (uint32_t i = 0; i < m; ++i) {
        printf(" %d", outputs[i]);
    }
    printf("\n");

}
static void test_decision_tree() 
{
    printf("test decision tree\n");
    int num_len = 3;
    int n = 4 * num_len;
    int m = 2;
    uint32_t depth = 2;
    uint32_t num_nodes = 3;

    bool inputs[n];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */
    for (int i = 0; i < n; i++) {
        if (i < n / 2) 
            inputs[i] = rand() % 2;
        else
            inputs[i] = rand() % 2;
    }
        
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    build_decision_tree_circuit(&gc, num_nodes, depth, num_len);


    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Print Results */
    printf("Inputs:");
    for (uint32_t i = 0; i < n; ++i) {
        if (i == n / 2) {
            printf(" |");
        }
        printf(" %d", inputs[i]);
    }
    printf("\n");

    for (uint32_t i = 0; i < m; ++i) {
        printf("outputs[%d] = %d\n", i, outputs[i]);
    }
    printf("\n");

}

static void test_signed_comparison()
{
    printf("Signed comparison test\n");
    /* Paramters */
    int n = 6; // bits per number
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
    inputs[0] = 1;
    inputs[1] = 1;
    inputs[2] = 1;
    inputs[3] = 1;
    inputs[4] = 1;
    inputs[5] = 0;
    
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    // build
    countToN(inputWires, n);

    circuit_signed_less_than(&gc, &gcContext, n, inputWires, inputWires + (n/2), outputWires);


	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Print Results */
    printf("Inputs:");
    for (uint32_t i = 0; i < n; ++i) {
        if (i == n / 2) {
            printf(" |");
        }
        printf(" %d", inputs[i]);
    }
    printf("\n");

    for (uint32_t i = 0; i < m; ++i) {
        printf("outputs[%d] = %d\n", i, outputs[i]);
    }
    printf("\n");
}

static void signedMultTest()
{
    printf("Signed mult test\n");
    /* Paramters */
    int n = 6; // bits per number
    int m = 3;
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */
    for (uint32_t i = 0; i < n; i++) {
        inputs[i] = i % 2;
    }
    
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    // build
    countToN(inputWires, n);
    circuit_signed_mult_n(&gc, &gcContext, n, inputWires, outputWires);
	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Print Results */
    printf("Inputs:");
    for (uint32_t i = 0; i < n; ++i) {
        if (i == n / 2) {
            printf(" |");
        }
        printf(" %d", inputs[i]);
    }
    printf("\n");

    for (uint32_t i = 0; i < m; ++i) {
        printf("outputs[%d] = %d\n", i, outputs[i]);
    }
    printf("\n");
}

static void test_mult()
{
    /* Paramters */
    int n = 20;
    int m = n / 2;
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    int in0 = 0;
    int in1 = 0;
    int max = pow(2, n / 2) - 1;
    do {
        for (uint32_t i = 0; i < n; i++) {
            inputs[i] = rand() % 2;
        }
        in0 = convertToDec(inputs, n / 2);
        in1 = convertToDec(&inputs[n / 2], n / 2);
    } while (in0 * in1 >= max);

   /* Inputs */
    
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    // build
    countToN(inputWires, n);
    circuit_mult_n(&gc, &gcContext, n, inputWires, outputWires);
	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Check Results */
    int out = convertToDec(outputs, n / 2);
    if (in0 * in1 != out) {
        printf("FAILURE: ");
        printf("x, y, z: %d %d %d\n", in0, in1, out);
    }
}

static void test_add()
{
    /* Paramters */
    int n = 60; 
    int m = n / 2;
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    int in0 = 0;
    int in1 = 0;
    int max = pow(2, n / 2) - 1;
    do {
        for (uint32_t i = 0; i < n; i++) {
            inputs[i] = rand() % 2;
        }
        in0 = convertToDec(inputs, n / 2);
        in1 = convertToDec(&inputs[n / 2], n / 2);
    } while (in0 + in1 >= max);

   /* Inputs */
    
    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    // build
    countToN(inputWires, n);
    int carry;
    circuit_add(&gc, &gcContext, n, inputWires, outputWires, &carry);
	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Check Results */
    int out = convertToDec(outputs, n / 2);
    if (in0 + in1 != out) {
        printf("FAILURE: ");
        printf("x, y, z: %d %d %d\n", in0, in1, out);
    }
}

static void test_inner_product()
{
    // Test inner product, which uses signed multiplication of
    // little endian numbers internally.
    
    /* Paramters */
    int num_len = 6; // bits per number
    int num_num = 10; // number of numbers
    int n = num_len * num_num;
    int m = num_len;
    bool inputs[n];
    int inputWires[n];
    int outputWires[m];
    block inputLabels[2*n];
    block extractedLabels[n];
    block computedOutputMap[m];
    block outputMap[2*m];
    bool outputs[m];

    /* Inputs */ 
    int dec_inputs[num_num]; // inputs encoded in decimal ints
    int max_input = pow(2, num_len - 2) - 1;
    int max = pow(2, num_len - 1) - 1;
    int expected_out = 0;
    do {
        for (int i = 0; i < num_num; ++i) {
            dec_inputs[i] = rand() % max_input;
        }
        // compute dot product
        expected_out = 0;
        int split = num_num / 2;
        for (int i = 0; i < split; ++i) {
            expected_out += (dec_inputs[i] * dec_inputs[i + split]);
        }
    } while (expected_out >= max);

    // put input values into inputs encoded in little-endian binary
    for (int i = 0; i < num_num; ++i) {
        convertToSignedBinary(dec_inputs[i], &inputs[num_len * i], num_len);
    }

    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    //circuit_inner_product(&gc, &gcContext, n, num_len, inputWires, outputWires);
    circuit_inner_product(&gc, &gcContext, n, num_len, inputWires, outputWires);

	builder_finish_building(&gc, &gcContext, outputWires);

    /* Garble */
    garble_garble(&gc, inputLabels, outputMap);

    /* Evaluate */
    garble_extract_labels(extractedLabels, inputLabels, inputs, n);
    garble_eval(&gc, extractedLabels, computedOutputMap, NULL);
    garble_delete(&gc);

    /* Results */
    garble_map_outputs(outputMap, computedOutputMap, outputs, m);

    /* Check results */
    int out = convertSignedToDec(outputs, num_len);
    if (out != expected_out) {
        printf("Inner Product Test Failed\n");
        printf("out, expected_out: %d, %d\n", out, expected_out);
        printf("<");
        for (int i = 0; i < num_num; ++i) {
            if (i == num_num / 2) {
                printf("> * <");
            }
            printf("%d,", dec_inputs[i]);
        }
        printf("> = %d\n", out);

        for (int i = 0; i < n; ++i) {
            printf("%d ", inputs[i]);
        }
        printf(" -> ");

        for (int i = 0; i < m; ++i) {
            printf("%d ", outputs[i]);
        }

        printf("\n");
    } else {
        //printf("Passed\n");
    }

}

static void argMax4Test() 
{
    int num_len = 2;
    int n = num_len * 8; // 2 bits for each index, 2 bits for each value
    int m = 4;
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

    convertToBinary(0, &inputs[0], num_len);
    convertToBinary(1, &inputs[4], num_len);
    convertToBinary(2, &inputs[8], num_len);
    convertToBinary(3, &inputs[12], num_len);


    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
    garble_context gcContext;
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    circuit_argmax4(&gc, &gcContext, inputWires, outputWires, num_len);
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
    printf("argmax4 test--------------------------\n");
    printf("\tinputs: ");
    for (int i = 0; i < 4; i++) {
        int j = 4 * i;
        printf("(");
        printf("%d %d, %d %d", inputs[j], inputs[j+1], inputs[j+2], inputs[j+3]);
        printf(") ");
    }
    printf("\n");
    printf("\toutputs: (%d %d, %d %d)\n", outputs[0], outputs[1], outputs[2], outputs[3]);
}

static void argMax2Test() 
{
    int num_len = 2;
    int n = num_len * 4; // 1 bit for input, 1 bit for index
    int m = 2 * num_len;
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
    inputs[1] = 0;
    inputs[2] = 1;
    inputs[3] = 0;

    inputs[4] = 1;
    inputs[5] = 0;
    inputs[6] = 1;
    inputs[7] = 1;

    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
    garble_context gcContext;
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    circuit_argmax2(&gc, &gcContext, inputWires, outputWires, num_len);

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
    printf("\toutputs: (%d %d, %d %d)\n", outputs[0], outputs[1], outputs[2], outputs[3]);
}

static void test_les(int n) 
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
    int x = convertToDec(inputs, n / 2);
    int y = convertToDec(&inputs[n / 2], n / 2);
    bool expected_out = x < y;
    if (expected_out != outputs[0]) {
        printf("LES test failed--------------------------\n");
        printf("Expected: %d < %d => %d\n", x, y, expected_out);
        printf("\tinputs: ");
        for (int i = 0; i < n; i++) {
            if (i == (n/2))
                printf("|| ");
            printf("%d ", inputs[i]);
        }
        printf("\n");
        printf("\toutputs: %d\n", outputs[0]);
            
    }
}

static void test_get_model() 
{
    printf("Testing get_model");
    Model *model;
    model = get_model("models/wdbc.json");
    print_model(model);

    destroy_model(model);
    free(model);
    model = NULL;
}

void runAllTests(void)
{ 
    for (int i = 0; i < 100; ++i) {
        test_mult();
    }

    for (int i = 0; i < 100; ++i) {
        test_add();
    }
    
    for (int i = 0; i < 100; ++i) {
        test_inner_product();
    }

    for (int i = 0; i < 100; ++i) {
        test_les(40);
    }
    
    for (int i = 0; i < 100; ++i) {
        test_mux();
    }
}  
