#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 
#include "utils.h"

#include "arg.h"

void leven_garb_off() 
{
    printf("Running leven garb offline\n");
}

void leven_eval_off() 
{
    printf("Running leven eval offline\n");
}
void leven_garb_on() 
{
    printf("Running leven garb online\n");
}

void leven_eval_on() 
{
    printf("Running leven eval online\n");
    }

void full_leven_garb()
{
    /* Garbler offline phase for leven where no components are used:
     * only a single circuit
     */
    printf("Running full leven garb offline\n");

    // TODO: this time should be included in tot_time

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    int n = (l * 3) + 2 * 2;
    int m = l;
    int q = 50000; /* number of gates */ 
    int r = n + q; /* number of wires */

    InputLabels inputLabels = allocate_blocks(2*n); 
    OutputMap outputMap = allocate_blocks(2*m);
    createInputLabelsWithR(inputLabels, n, delta);

    GarbledCircuit gc;
	createEmptyGarbledCircuit(gc, n, m, q, r, inputLabels);
	startBuilding(gc, gc_context);

    buildLevenschteinCircuit(&gc, &gc_context);

    InputLabels input_labels = allocate_blocks(2 * gc.n);
    OutputMap output_map = allocate_blocks(2 * gc.m);

    garbleCircuit(&gc, inputLabels, outputMap);
    int num_garb_inputs = getNumGarbInputs();
    int num_eval_inputs = getNumEvalInputs();

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

void full_leven_eval()
{
    /* Evaluator offline phase for leven where no components are used:
     * only a single circuit
     */

    printf("Running full leven eval offline\n");
    int num_garb_inputs = getNumGarbInputs();
    int num_eval_inputs = getNumEvalInputs();

    int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
    for (int i = 0; i < num_eval_inputs; i++) {
        eval_inputs[i] = rand() % 2;
    }
    int *output = malloc(sizeof(int) * getM());
    unsigned long *tot_time = malloc(sizeof(unsigned long));
    evaluator_classic_2pc(eval_inputs, output, num_garb_inputs, 
            num_eval_inputs, tot_time);
    printf("tot_time %lu\n", *tot_time / getNumGates());

    //printf("output: ");
    //for (int i = 0; i < num_eval_inputs; i++) {
    //    if (i % 128 == 0)
    //        printf("\n");
    //    printf("%d", output[i]);
    //}
}

int main(int argc, char *argv[]) {
    // TODO add in arg.h stuff
    
	srand(time(NULL));
    srand_sse(time(NULL));
    assert(argc == 2);
    if (strcmp(argv[1], "eval_online") == 0) {
        leven_eval_on();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        leven_garb_on();
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        leven_garb_off();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        leven_eval_off();

    } else if (strcmp(argv[1], "full_garb_on") == 0) {
        full_leven_garb();
    } else if (strcmp(argv[1], "full_eval_on") == 0) {
        full_leven_eval();

    } else {
        printf("See test/2pc_leven.c:main for usage\n");
    }
    
    return 0;
} 
