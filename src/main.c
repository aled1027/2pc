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

void test_read_write() 
{
    
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);
    createGarbledCircuits(chained_gcs, NUM_GCS);
    ChainedGarbledCircuit* cgc = malloc(sizeof(ChainedGarbledCircuit));

    int i = 0;

    saveChainedGC(&chained_gcs[i], false);
    loadChainedGC(cgc, i, false);

    printf("break here\n");
}

void g_offline() 
{
    //GarbledCircuit gcs[NUM_GCS]; // NUM_GCS defined in 2pc_common.h
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);
    createGarbledCircuits(chained_gcs, NUM_GCS);
    garbler_offline(chained_gcs, NUM_GCS); // 
}

void e_offline() 
{
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);
    evaluator_receive_gcs(chained_gcs, NUM_GCS); // NUM_GCS defined in 2pc_common.h
}

int evaluator() 
{
    int *inputs = malloc(sizeof(int) * 2);
    int num_inputs = 2;

    inputs[0] = rand() % 2;
    inputs[1] = rand() % 2;
    printf("inputs: (%d,%d)\n", inputs[0], inputs[1]);
    
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * NUM_GCS);

    for (int i=0; i<NUM_GCS; i++) {
        loadChainedGC(&chained_gcs[i], i, false);
    }
    evaluator_go(chained_gcs, NUM_GCS, inputs, num_inputs);

    return SUCCESS;
}

//void foo(int ** arr) {
//    *arr = malloc(100);
//    //(*arr)[2] = 12;
//}
//
void test() {
    block a = randomBlock();
    block b;
    memcpy(&a, &b, sizeof(block));
    printf("\n");
    print_block(a);
    printf("\n");
    print_block(b);
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

    if (strcmp(argv[1], "eval_online") == 0) {
        printf("Running eval online\n");
        evaluator();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        printf("Running garb online\n");
        run_garbler();
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        printf("Running garb offline\n");
        g_offline();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        printf("Running val offline\n");
        e_offline();
    } else if (strcmp(argv[1], "tests") == 0) {
        //test_read_write();
        //run_all_tests();
        test();
    } else {
        printf("usage: %s eval\n", argv[0]);
        printf("usage: %s garb\n", argv[0]);
        printf("usage: %s tests\n", argv[0]);
        return -1;
    } 
    //go();
    return 0;
}


