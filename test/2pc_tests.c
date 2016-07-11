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
static void multTest()
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

static void innerProductTest()
{
    /* Paramters */
    int num_len = 32; // bits per number
    int num_num = 8; // number of numbers
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
    for (uint32_t i = 0; i < n; i++) {
        inputs[i] = rand() % 2;
    }

    /* Build Circuit */
    garble_create_input_labels(inputLabels, n, NULL, false);
    garble_circuit gc;
    garble_context gcContext;
	garble_new(&gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(&gc, &gcContext);

    countToN(inputWires, n);
    //outputWires[0] = builder_next_wire(&gcContext);
    //circuit_mult_n(&gc, &gcContext, n, inputWires, outputWires);
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

    /* Print Results */
    //printf("Inputs:");
    //for (uint32_t i = 0; i < n; ++i) {
    //    printf(" %d", inputs[i]);
    //}
    //printf("\n");

    //printf("Outputs:");
    //for (uint32_t i = 0; i < m; ++i) {
    //    printf(" %d", outputs[i]);
    //}
    //printf("\n");
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

    int nruns = 1; 


    for (int i = 0; i < nruns; i++)
        test_signed_comparison();
}  
