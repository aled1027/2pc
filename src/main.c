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


//int 
//go() 
//{
//    int num_circs = 3; // can be an upper bound
//    int num_maps = 3; // can be an upper bound
//    int n = 2; // number of inputs to each gc
//    int num_instr = 7;
//    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_circs);
//    Instruction* instructions = malloc(sizeof(Instruction) *  8);
//    
//    block* receivedOutputMap; // = (block *) memalign(128, sizeof(block) * 2 * 2);
//    GarbledCircuit gcs[num_circs];
//    InputLabels extractedLabels[num_circs];
//    assert(chained_gcs && instructions && receivedOutputMap);
//
//    for (int i=0; i<num_circs; i++) {
//        extractedLabels[i] = memalign(128, sizeof(block)*2);
//        assert(extractedLabels[i]);
//    }
//
//    int* inputs[2];
//    inputs[0] = malloc(sizeof(int*)); inputs[1] = malloc(sizeof(int*));
//
//    inputs[0][0] = 1;
//    inputs[0][1] = 1;
//    inputs[1][0] = 1;
//    inputs[1][1] = 1;
//    printf("inputs: (%d,%d), (%d,%d)\n", inputs[0][0], inputs[0][1], inputs[1][0], inputs[1][1]);
//
//    createGarbledCircuits(chained_gcs, num_circs); createInstructions(instructions, chained_gcs);
//
//    // send garbled circuits over
//    for (int i=0; i<num_circs; i++) {
//        gcs[i] = chained_gcs[i].gc; // shallow copy; pointers are not copied.
//    }
//
//    receivedOutputMap = chained_gcs[2].outputMap; // would be sent over by garbler
//
//	//extractLabels(extractedLabels[0], chained_gcs[0].inputLabels, inputs[0], n);
//	//extractLabels(extractedLabels[1], chained_gcs[1].inputLabels, inputs[1], n);
//    block** inputLabels = malloc(sizeof(block*) * 2);
//    inputLabels[0] = chained_gcs[0].inputLabels;
//    inputLabels[1] = chained_gcs[1].inputLabels;
//
//    int* output = malloc(sizeof(int) * chained_gcs[0].gc.m);
//    assert(output);
//    chainedEvaluate(gcs, 3, instructions, num_instr, inputLabels, receivedOutputMap, inputs, output);
//    printf("%d, %d\n", output[0], output[1]);
//
//    for (int i=0; i<num_circs; i++) {
//        freeChainedGarbledCircuit(&chained_gcs[i]);
//        free(extractedLabels[i]);
//    }
//    free(chained_gcs);
//    free(instructions);
//    free(output);
//    free(inputLabels);
//    free(inputs[0]);
//    free(inputs[1]);
//    
//    return 0;
//}
//

void
garbler()
{
    // test reading function from file, 
    // chaining some circuits and evaluating
    
    FunctionSpec function;

    GarbledCircuit gcs[NUM_GCS]; // NUM_GCS defined in 2pc_common.h
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);
    block* receivedOutputMap; 
    int* inputs = malloc(sizeof(int) * 4);

    createGarbledCircuits(chained_gcs, NUM_GCS);

    garbler_send_gcs(chained_gcs, NUM_GCS);
    garbler_init(&function, chained_gcs, NUM_GCS);

    // fill inputLabels
    for (int i=0; i<NUM_GCS; i++) {
        gcs[i] = chained_gcs[i].gc; // shallow copy; pointers are not copied.
    }


    garbler_go(&function, chained_gcs, NUM_GCS, inputs);

    //freeFunctionSpec(&function);
}

int evaluator() 
{
    // make a new function, evaluator_go()... which receives everything and then calls chained evaluate
    // get labels should probably be a part of evaluator_go(), not chained evaluate, so it really happens before
    // evaluation happens.
    int *inputs = malloc(sizeof(int) * 4);
    inputs[0] = 0;
    inputs[1] = 0;
    inputs[2] = 0;
    inputs[3] = 0;
    printf("inputs: (%d,%d), (%d,%d)\n", inputs[0], inputs[1], inputs[2], inputs[3]);
    
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);
    evaluator_receive_gcs(chained_gcs, NUM_GCS); // NUM_GCS defined in 2pc_common.h
    evaluator_go(chained_gcs, inputs);
    printf("here\n");

    //chainedEvaluate(gcs, num_gcs, function.instructions.instr, function.instructions.size, 
    //        inputLabels, receivedOutputMap, inputs, output, &function.input_mapping);

    //freeFunctionSpec(&function);

    //printf("outputs: (%d, %d)\n", output[0], output[1]);

    //if ((((inputs[0] ^ inputs[1]) ^ (inputs[2] ^ inputs[3])) != output[0]) || 
    //    (((inputs[0] ^ inputs[1]) && (inputs[2] ^ inputs[3])) != output[1]))
    //{
    //    return FAILURE;
    //}
    return SUCCESS;
}

int 
main(int argc, char* argv[]) 
{
	srand(time(NULL));
    srand_sse(time(NULL));

    garbler();
    //evaluator();
    
    //run_all_tests();
    //go();
    return 0;
}


