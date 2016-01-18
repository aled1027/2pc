#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "components.h"

#include "utils.h"

/* static unsigned long NUM_GATES = 34000; */

void
aes_garb_off(char *dir, int nchains)
{
    printf("Running garb offline\n");
    ChainedGarbledCircuit *chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * nchains);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < nchains; i++) {
        // for AES:
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        if (i == nchains - 1) {
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
        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap,
                      GARBLE_TYPE_STANDARD);
    }
    garbler_offline(dir, chained_gcs, 128, nchains);
    freeChainedGarbledCircuit(chained_gcs);
}

void
full_aes_garb(void)
{
    printf("Running full aes garb offline\n");
    GarbledCircuit gc;
    buildAESCircuit(&gc);

    block *inputLabels = allocate_blocks(2 * gc.n);
    block *outputMap = allocate_blocks(2 * gc.m);
    assert(inputLabels && outputMap);

    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);
    int num_garb_inputs = 128 * 10;
    int num_eval_inputs = 128;

    int *garb_inputs = malloc(sizeof(int) * num_garb_inputs);
    for (int i = 0; i < num_garb_inputs; i++) {
        garb_inputs[i] = rand() % 2; 
    }

    InputMapping imap; // TODO Fill this and we are good
    imap.size = num_eval_inputs + num_garb_inputs;
    imap.input_idx = malloc(sizeof(int) * imap.size);
    imap.gc_id = malloc(sizeof(int) * imap.size);
    imap.wire_id = malloc(sizeof(int) * imap.size);
    imap.inputter = malloc(sizeof(Person) * imap.size);

    for (int i = 0; i < num_eval_inputs; i++) {
        imap.input_idx[i] = i;
        imap.gc_id[i] = 0;
        imap.wire_id[i] = i;
        imap.inputter[i] = PERSON_EVALUATOR;
    }

    for (int i = num_eval_inputs; i < num_eval_inputs + num_garb_inputs; i++) {
        imap.input_idx[i] = i;
        imap.gc_id[i] = 0;
        imap.wire_id[i] = i;
        imap.inputter[i] = PERSON_GARBLER;
    }

    unsigned long *tot_time = malloc(sizeof(unsigned long));
    garbler_classic_2pc(&gc, inputLabels, &imap, outputMap,
            num_garb_inputs, num_eval_inputs, garb_inputs, tot_time); 

    free(inputLabels);
    free(outputMap);
}

void
full_aes_eval(void)
{
    /* Evaluator offline phase for CBC where no components are used:
     * only a single circuit
     */

    printf("Running full aes eval offline\n");
    int num_garb_inputs = 128 * 10;
    int num_eval_inputs = 128;
    int m = 128;

    int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
    for (int i = 0; i < num_eval_inputs; i++) {
        eval_inputs[i] = rand() % 2;
    }
    int *output = malloc(sizeof(int) * m);
    unsigned long *tot_time = malloc(sizeof(unsigned long));
    evaluator_classic_2pc(eval_inputs, output, num_garb_inputs, 
            num_eval_inputs, tot_time);
    printf("tot_time %lu\n", *tot_time);
}
