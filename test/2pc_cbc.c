#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 

#include "arg.h"

int NUM_XOR_GCS = 10;
int NUM_AES_GCS = 90;
int NUM_FINAL_AES_GCS = 10;
int NUM_GCS = 110;
int NUM_GATES = 351360;

int NUM_GARB_INPUTS = 12928;
int NUM_EVAL_INPUTS = 1280;
char *FUNCTION_PATH = "functions/cbc_10_10.json"; /* 10,90,10,garb=12928,eval=1280,gates=351360 */
//char* FUNCTION_PATH = "functions/cbc_2_2.json"; /* 2,2,2,garb=640,eval=256, gates=7808 */

void cbc_garb_off() {
    printf("Running cbc garb offline\n");
    int num_chained_gcs = NUM_GCS; 
    ChainedGarbledCircuit *chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < num_chained_gcs; i++) {
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        if (i < NUM_XOR_GCS) {
            buildXORCircuit(p_gc, &delta);
            chained_gcs[i].type = XOR;
        } else if (i < NUM_XOR_GCS + NUM_AES_GCS) {
            buildAESRoundComponentCircuit(p_gc, false, &delta);
            chained_gcs[i].type = AES_ROUND;
        } else {
            buildAESRoundComponentCircuit(p_gc, true, &delta);
            chained_gcs[i].type = AES_FINAL_ROUND;

        }
        chained_gcs[i].id = i;

        (void) posix_memalign((void **) &chained_gcs[i].inputLabels, 128, sizeof(block) * 2 * chained_gcs[i].gc.n);
        (void) posix_memalign((void **) &chained_gcs[i].outputMap, 128, sizeof(block) * 2 * chained_gcs[i].gc.m);
        assert(chained_gcs[i].inputLabels != NULL && chained_gcs[i].outputMap != NULL);
        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap);
    }
    garbler_offline(chained_gcs, NUM_EVAL_INPUTS, num_chained_gcs);
    free(chained_gcs);
}

void cbc_eval_off() {
    int num_chained_gcs = NUM_GCS; 
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    evaluator_offline(chained_gcs, NUM_EVAL_INPUTS, num_chained_gcs);
}

void cbc_garb_on(char* function_path) {
    printf("Running cbc garb online\n");
    int num_chained_gcs = NUM_GCS;
    int num_garb_inputs, *garb_inputs;

    num_garb_inputs = NUM_GARB_INPUTS;
    garb_inputs = malloc(sizeof(int) * num_garb_inputs);
    assert(garb_inputs);
    printf("input: ");
    for (int i = 0; i < num_garb_inputs; i++) {
        if ((i % 128) == 0) 
            printf("\n");
        garb_inputs[i] = rand() % 2; 
        printf("%d",garb_inputs[i]);
    }
    printf("\n");
    unsigned long *ot_time = malloc(sizeof(unsigned long));
    unsigned long *tot_time = malloc(sizeof(unsigned long));
    garbler_online(function_path, garb_inputs, num_garb_inputs, num_chained_gcs, ot_time, tot_time);
    printf("ot_time: %lu\n", *ot_time / NUM_GATES);
    printf("tot_time: %lu\n", *tot_time / NUM_GATES);
    printf("time without ot: %lu\n", (*tot_time - *ot_time) / NUM_GATES);
}

void cbc_eval_on() {
    printf("Running cbc eval online\n");
    int num_eval_inputs = NUM_EVAL_INPUTS;
    int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
    assert(eval_inputs);
    printf("input: ");
    for (int i = 0; i < num_eval_inputs; i++) {
        if ((i % 128) == 0) 
            printf("\n");
        eval_inputs[i] = rand() % 2;
        printf("%d",eval_inputs[i]);
    }
    printf("\n");

    int num_chained_gcs = NUM_GCS;
    unsigned long *ot_time = malloc(sizeof(unsigned long));
    unsigned long *tot_time = malloc(sizeof(unsigned long));
    evaluator_online(eval_inputs, num_eval_inputs, num_chained_gcs, ot_time, tot_time);
    printf("ot_time: %lu\n", *ot_time / NUM_GATES);
    printf("tot_time: %lu\n", *tot_time / NUM_GATES);
    printf("time without ot: %lu\n", (*tot_time - *ot_time) / NUM_GATES);

}

int main(int argc, char *argv[]) {
    // TODO add in arg.h stuff
    
	srand(time(NULL));
    srand_sse(time(NULL));
    assert(argc == 2);
    char *function_path = FUNCTION_PATH;
    printf("Using function %s\n", function_path);
    if (strcmp(argv[1], "eval_online") == 0) {
        cbc_eval_on();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        cbc_garb_on(function_path);
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        cbc_garb_off();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        cbc_eval_off();
    } else {
        printf("See test/2pc_cbc.c:main for usage\n");
    }
    
    return 0;
} 
