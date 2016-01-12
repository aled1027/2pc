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

int NUM_AES_ROUNDS = 10;
int NUM_CBC_BLOCKS = 10;

char *FULL_FUNCTION_PATH = "functions/full_cbc.json";
char *COMPONENT_FUNCTION_PATH = "functions/cbc_10_10.json"; 
//char* COMPONENT_FUNCTION_PATH = "functions/cbc_2_2.json";

static int getNumGarbInputs() { return (NUM_AES_ROUNDS * NUM_CBC_BLOCKS * 128) + 128; }
static int getNumEvalInputs() { return NUM_CBC_BLOCKS * 128; }
static int getNumXORCircs() { return NUM_CBC_BLOCKS; }
static int getNumAESCircs() { return (NUM_AES_ROUNDS-1) * NUM_CBC_BLOCKS; }
static int getNumFinalAESCircs() { return NUM_CBC_BLOCKS; }
static int getNumCircs() { return getNumXORCircs() + getNumAESCircs() + getNumFinalAESCircs();}
static int getM() { return NUM_CBC_BLOCKS * 128; }

static int getNumGates() {
    int gates_per_xor = 128;
    int gates_per_aes = 3904;
    int gates_per_aes_final = 3904; // TODO this number is off. should be less than aes round

    return getNumXORCircs() * gates_per_xor + getNumAESCircs() * gates_per_aes + 
        getNumFinalAESCircs() * gates_per_aes_final;
}


void cbc_garb_off() 
{
    printf("Running cbc garb offline\n");
    int num_chained_gcs = getNumCircs(); 
    int num_xor_circs = getNumXORCircs();
    int num_aes_circs = getNumAESCircs();
    ChainedGarbledCircuit *chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < num_chained_gcs; i++) {
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        if (i < num_xor_circs) {
            buildXORCircuit(p_gc, &delta);
            chained_gcs[i].type = XOR;
        } else if (i < num_xor_circs + num_aes_circs) {
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
        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap,
                      GARBLE_TYPE_STANDARD);
    }
    int num_eval_inputs = getNumEvalInputs();
    garbler_offline(chained_gcs, num_eval_inputs, num_chained_gcs);
    free(chained_gcs);
}

void cbc_eval_off() 
{
    int num_chained_gcs = getNumCircs(); 
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    int num_eval_inputs = getNumEvalInputs();
    evaluator_offline(chained_gcs, num_eval_inputs, num_chained_gcs);
}

void cbc_garb_on() 
{
    char *function_path = COMPONENT_FUNCTION_PATH;
    printf("Running cbc garb online\n");
    int num_chained_gcs = getNumCircs();
    int num_garb_inputs, *garb_inputs;

    num_garb_inputs = getNumGarbInputs();
    garb_inputs = malloc(sizeof(int) * num_garb_inputs);
    assert(garb_inputs);
    for (int i = 0; i < num_garb_inputs; i++) {
        garb_inputs[i] = rand() % 2; 
    }
    unsigned long *tot_time = malloc(sizeof(unsigned long));
    garbler_online(function_path, garb_inputs, num_garb_inputs, num_chained_gcs, NULL, tot_time);
    int num_gates = getNumGates();
    printf("tot_time: %lu\n", *tot_time / num_gates);
}

void cbc_eval_on() 
{
    printf("Running cbc eval online\n");
    int num_eval_inputs = getNumEvalInputs();
    int *eval_inputs = malloc(sizeof(int) * num_eval_inputs);
    assert(eval_inputs);
    for (int i = 0; i < num_eval_inputs; i++) {
        eval_inputs[i] = rand() % 2;
    }

    int num_chained_gcs = getNumCircs();
    unsigned long *tot_time = malloc(sizeof(unsigned long));
    evaluator_online(eval_inputs, num_eval_inputs, num_chained_gcs, NULL, tot_time);
    int num_gates = getNumGates();
    printf("tot_time: %lu\n", *tot_time / num_gates);
}

void full_cbc_garb()
{
    /* Garbler offline phase for CBC where no components are used:
     * only a single circuit
     */
    printf("Running full cbc garb offline\n");

    // TODO: this time should be included in tot_time

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    GarbledCircuit gc;
    buildCBCFullCircuit(&gc, NUM_CBC_BLOCKS, NUM_AES_ROUNDS, &delta);

    InputLabels inputLabels = allocate_blocks(2 * gc.n);
    OutputMap outputMap = allocate_blocks(2 * gc.m);
    assert(inputLabels && outputMap);

    garbleCircuit(&gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);
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

void full_cbc_eval()
{
    /* Evaluator offline phase for CBC where no components are used:
     * only a single circuit
     */

    printf("Running full cbc eval offline\n");
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
        cbc_eval_on();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        cbc_garb_on();
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        cbc_garb_off();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        cbc_eval_off();

    } else if (strcmp(argv[1], "full_garb_on") == 0) {
        full_cbc_garb();
    } else if (strcmp(argv[1], "full_eval_on") == 0) {
        full_cbc_eval();

    } else {
        printf("See test/2pc_cbc.c:main for usage\n");
    }
    
    return 0;
} 
