#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 

#include "arg.h"

int NUM_GCS = 3;

void cbc_garb_off() {
    printf("Running cbc garb offline\n");
    int num_chained_gcs = NUM_GCS; 
    ChainedGarbledCircuit *chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < num_chained_gcs; i++) {
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        if (i == 0) {
            buildXORCircuit(p_gc, &delta);
            chained_gcs[i].type = XOR;
        } else {
            buildAESRoundComponentCircuit(p_gc, false, &delta);
            chained_gcs[i].type = AES_ROUND;
        }
        chained_gcs[i].id = i;

        (void) posix_memalign((void **) &chained_gcs[i].inputLabels, 128, sizeof(block) * 2 * chained_gcs[i].gc.n);
        (void) posix_memalign((void **) &chained_gcs[i].outputMap, 128, sizeof(block) * 2 * chained_gcs[i].gc.m);
        assert(chained_gcs[i].inputLabels != NULL && chained_gcs[i].outputMap != NULL);
        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap);
    }
    printf("%d circuits generated\n", num_chained_gcs);
    garbler_offline(chained_gcs, num_chained_gcs);
}

void cbc_eval_off() {
    int num_chained_gcs = NUM_GCS; // defined in common
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    evaluator_offline(chained_gcs, num_chained_gcs);
}

void cbc_garb_on(char* function_path) {
    printf("Running cbc garb online\n");
    int num_chained_gcs = NUM_GCS;
    int num_garb_inputs, *garb_inputs;

    num_garb_inputs = 384;
    garb_inputs = malloc(sizeof(int) * num_garb_inputs);
    assert(garb_inputs);
    for (int i=0; i<num_garb_inputs; i++) {
        garb_inputs[i] = rand() % 2; 
        garb_inputs[i] = 0;
    }
    garbler_run(function_path, garb_inputs, num_garb_inputs, num_chained_gcs);
}

void cbc_eval_on() {
    printf("Running cbc eval online\n");
    int num_eval_inputs = 256;
    int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
    assert(eval_inputs);
    for (int i=0; i<num_eval_inputs; i++) {
        eval_inputs[i] = rand() % 2;
        eval_inputs[i] = 0;
    }

    int num_chained_gcs = NUM_GCS;
    evaluator_run(eval_inputs, num_eval_inputs, num_chained_gcs);
}

int main(int argc, char *argv[]) {
    // TODO add in arg.h stuff
    
	srand(time(NULL));
    srand_sse(time(NULL));
    char* function_path = "functions/cbc.json";
    assert(argc == 2);
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
