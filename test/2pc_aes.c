#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 

#include "arg.h"

int NUM_GCS = 10;
int NUM_TRIALS = 20;
int MEDIAN_IDX = 11; // NUM_TRIALS / 2 - 1
bool is_testing = true;
unsigned long NUM_GATES = 36480;

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
    garbler_offline(chained_gcs, 128, num_chained_gcs);
    free(chained_gcs);
}
int
myCompare(const void * a, const void * b)
{
	return (int) (*(unsigned long*) a - *(unsigned long*) b);
}

void aes_garb_on(char* function_path, bool timing) {
    // crude addition for timings
    if (timing) {
        unsigned long *ot_time = malloc(sizeof(unsigned long) * NUM_TRIALS);
        unsigned long *tot_time = malloc(sizeof(unsigned long) * NUM_TRIALS);
        for (int j=0; j<NUM_TRIALS; j++) {
            int num_chained_gcs = NUM_GCS;
            int num_garb_inputs, *inputs;

            num_garb_inputs = 128*10;
            inputs = malloc(sizeof(int) * num_garb_inputs);
            assert(inputs);
            for (int i=0; i<num_garb_inputs; i++) {
                inputs[i] = rand() % 2; 
            }
            garbler_online(function_path, inputs, num_garb_inputs, num_chained_gcs, 
                    &ot_time[j], &tot_time[j]);
            printf("%lu, %lu\n", ot_time[j], tot_time[j]);
        }

	    qsort(ot_time, NUM_TRIALS, sizeof(unsigned long), myCompare);
	    qsort(tot_time, NUM_TRIALS, sizeof(unsigned long), myCompare);
        printf("median ot_time: %lu\n", ot_time[MEDIAN_IDX] / NUM_GATES);
        printf("median tot_time: %lu\n", tot_time[MEDIAN_IDX] / NUM_GATES);
        printf("median time without ot: %lu\n", (tot_time[MEDIAN_IDX] - ot_time[MEDIAN_IDX]) / NUM_GATES);
    } else {
        // CHECK NUM GCS
        int num_chained_gcs = NUM_GCS;
        int num_garb_inputs, *inputs;

        num_garb_inputs = 128*10;
        inputs = malloc(sizeof(int) * num_garb_inputs);
        assert(inputs);
        for (int i=0; i<num_garb_inputs; i++) {
            inputs[i] = rand() % 2; 
        }
        unsigned long *ot_time = malloc(sizeof(unsigned long));
        unsigned long *tot_time = malloc(sizeof(unsigned long));
        garbler_online(function_path, inputs, num_garb_inputs, num_chained_gcs, ot_time, tot_time);
    }
}

void aes_eval_off() {
    int num_chained_gcs = NUM_GCS; // defined in common
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    evaluator_offline(chained_gcs, 128, num_chained_gcs);
}

void aes_eval_on(bool testing) {
    if (testing) {
        unsigned long *ot_time = malloc(sizeof(unsigned long) * NUM_TRIALS);
        unsigned long *tot_time = malloc(sizeof(unsigned long) * NUM_TRIALS);
        for (int j=0; j<NUM_TRIALS; j++) {
            sleep(1); // uncomment this if getting hung up
            int num_eval_inputs = 128;
            int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
            assert(eval_inputs);

            for (int i=0; i<num_eval_inputs; i++) {
                eval_inputs[i] = rand() % 2;
            }

            int num_chained_gcs = NUM_GCS;
            evaluator_online(eval_inputs, num_eval_inputs, num_chained_gcs, &ot_time[j], &tot_time[j]);
        }
	    qsort(ot_time, NUM_TRIALS, sizeof(unsigned long), myCompare);
	    qsort(tot_time, NUM_TRIALS, sizeof(unsigned long), myCompare);
        printf("median ot_time: %lu\n", ot_time[MEDIAN_IDX] / NUM_GATES);
        printf("median tot_time: %lu\n", tot_time[MEDIAN_IDX] / NUM_GATES);
        printf("median time without ot: %lu\n", (tot_time[MEDIAN_IDX] - ot_time[MEDIAN_IDX]) / NUM_GATES);
    } else {
        int num_eval_inputs = 128;
        int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
        assert(eval_inputs);

        for (int i=0; i<num_eval_inputs; i++) {
            eval_inputs[i] = rand() % 2;
        }

        int num_chained_gcs = NUM_GCS;
        unsigned long *ot_time = malloc(sizeof(unsigned long));
        unsigned long *tot_time = malloc(sizeof(unsigned long));
        evaluator_online(eval_inputs, num_eval_inputs, num_chained_gcs, ot_time, tot_time);
    }
}

int main(int argc, char *argv[]) {
    // TODO add in arg.h stuff
    
	srand(time(NULL));
    srand_sse(time(NULL));
    char* function_path = "functions/aes.json";
    if (strcmp(argv[1], "eval_online") == 0) {
        aes_eval_on(is_testing);
    } else if (strcmp(argv[1], "garb_online") == 0) {
        printf("Running garb online\n");
        aes_garb_on(function_path, is_testing);
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
