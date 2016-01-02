#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
//#include "justGarble.h"

#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 

#include "arg.h"

int NUM_GCS = 10;

void aes_garb_off() {
    printf("Running garb offline\n");
    int num_chained_gcs = NUM_GCS; // defined in 2pc_common.h
    ChainedGarbledCircuit *chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    // createGarbledCircuits(chained_gcs, num_chained_gcs);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < num_chained_gcs; i++) {
        // for AES:
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        if (i == num_chained_gcs-1) {
            buildAESRoundComponentCircuit(p_gc, true, &delta);
            chained_gcs[i].type = AES_FINAL_ROUND;
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
    garbler_offline(chained_gcs, num_chained_gcs);
}

void aes_garb_on(char* function_path) {
    // CHECK NUM GCS
    int num_chained_gcs = NUM_GCS;
    int num_garb_inputs, *inputs;

    num_garb_inputs = 128*10;
    inputs = malloc(sizeof(int) * num_garb_inputs);
    assert(inputs);
    for (int i=0; i<num_garb_inputs; i++) {
        inputs[i] = rand() % 2; 
    }

    garbler_run(function_path, inputs, num_garb_inputs, num_chained_gcs);
}

void aes_eval_off() {
    int num_chained_gcs = NUM_GCS; // defined in common
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    evaluator_offline(chained_gcs, num_chained_gcs);
}

void aes_eval_on() {
    int num_eval_inputs = 128;
    int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
    assert(eval_inputs);

    for (int i=0; i<num_eval_inputs; i++) {
        eval_inputs[i] = rand() % 2;
    }

    int num_chained_gcs = NUM_GCS;
    evaluator_run(eval_inputs, num_eval_inputs, num_chained_gcs);
}

int main(int argc, char *argv[]) {
    // TODO add in arg.h stuff
    
	srand(time(NULL));
    srand_sse(time(NULL));
    char* function_path = "functions/aes.json";
    if (strcmp(argv[1], "eval_online") == 0) {
        printf("Running eval online\n");
        //printf("EvalOTtime, EvalTOTtime\n");
        //for (int j=0; j<num; j++) {
            aes_eval_on();
            //sleep(1);
        //}
    } else if (strcmp(argv[1], "garb_online") == 0) {
        printf("Running garb online\n");
        //printf("GarbOTTime, GarbTOTtime\n");
        //for (int j=0; j<num; j++) {
            aes_garb_on(function_path);
        //}

    } else if (strcmp(argv[1], "garb_offline") == 0) {
        aes_garb_off();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        printf("Running val offline\n");
        aes_eval_off();
    } else {
        printf("Seeing test/2pc_aes.c:main for usage\n");
    }
    
    return 0;
} 
