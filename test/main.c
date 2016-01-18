
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "justGarble.h"
#include "2pc_and.h"
#include "2pc_aes.h"
#include "2pc_cbc.h"

static int NUM_GCS = 10;
static int NUM_TRIALS = 20;

/* Stuff for CBC */
static int NUM_AES_ROUNDS = 10;
static int NUM_CBC_BLOCKS = 10;
static int getNumGarbInputs() { return (NUM_AES_ROUNDS * NUM_CBC_BLOCKS * 128) + 128; }
static int getNumEvalInputs() { return NUM_CBC_BLOCKS * 128; }
static int getNumXORCircs() { return NUM_CBC_BLOCKS; }
static int getNumAESCircs() { return (NUM_AES_ROUNDS-1) * NUM_CBC_BLOCKS; }
static int getNumFinalAESCircs() { return NUM_CBC_BLOCKS; }
static int getNumCircs() { return getNumXORCircs() + getNumAESCircs() + getNumFinalAESCircs();}
static int getM() { return NUM_CBC_BLOCKS * 128; }

#define GARBLER_DIR "files/garbler_gcs"
#define EVALUATOR_DIR "files/evaluator_gcs"


/* XXX: not great, but OK for now */

static struct option opts[] =
{
    {"and_garb_off", no_argument, 0, 'a'},
    {"and_eval_off", no_argument, 0, 'A'},
    {"and_garb_on", no_argument, 0, 'b'},
    {"and_eval_on", no_argument, 0, 'B'},
    {"and_garb_full", no_argument, 0, 'c'},
    {"and_eval_full", no_argument, 0, 'C'},

    {"aes_garb_off", no_argument, 0, 'd'},
    {"aes_eval_off", no_argument, 0, 'D'},
    {"aes_garb_on", no_argument, 0, 'e'},
    {"aes_eval_on", no_argument, 0, 'E'},
    {"aes_garb_full", no_argument, 0, 'f'},
    {"aes_eval_full", no_argument, 0, 'F'},

    {"cbc_garb_off", no_argument, 0, 'g'},
    {"cbc_eval_off", no_argument, 0, 'G'},
    {"cbc_garb_on", no_argument, 0, 'i'},
    {"cbc_eval_on", no_argument, 0, 'I'},
    {"cbc_garb_full", no_argument, 0, 'j'},
    {"cbc_eval_full", no_argument, 0, 'J'},

    {"garb_on", no_argument, 0, 'g'},
    {"eval_on", no_argument, 0, 'e'},

    {"time", no_argument, 0, 't'},

    {0, 0, 0, 0}
};

static void
eval_off(int ninputs, int nchains)
{
    evaluator_offline(EVALUATOR_DIR, ninputs, nchains);
}

static int
compare(const void * a, const void * b)
{
	return (int) (*(unsigned long*) a - *(unsigned long*) b);
}

static void
garb_on(char* function_path, int ninputs, int nchains, int ntrials)
{
    unsigned long sum = 0, *tot_time;
    int *inputs;

    inputs = malloc(sizeof(int) * ninputs);
    tot_time = malloc(sizeof(unsigned long) * ntrials);

    for (int i = 0; i < ntrials; i++) {
        for (int j = 0; j < ninputs; j++) {
            inputs[j] = rand() % 2; 
        }
        garbler_online(function_path, GARBLER_DIR, inputs, ninputs, nchains, 
                       &tot_time[i]);
        printf("%lu\n", tot_time[i]);
        sum += tot_time[i];
    }

    printf("%d trials\n", ntrials);
    printf("avg time: %f\n", (float) sum / (float) ntrials);

    free(inputs);
    free(tot_time);
}

static void
eval_on(int ninputs, int nchains, int ntrials)
{
    unsigned long sum = 0, *tot_time;
    int *inputs;

    tot_time = malloc(sizeof(unsigned long) * ntrials);
    inputs = malloc(sizeof(int) * ninputs);

    for (int i = 0; i < ntrials; i++) {
        sleep(1); // uncomment this if getting hung up
        for (int j = 0; j < ninputs; j++) {
            inputs[j] = rand() % 2;
        }
        evaluator_online(EVALUATOR_DIR, inputs, ninputs, nchains, &tot_time[i]);
        printf("%lu\n", tot_time[i]);
        sum += tot_time[i];
    }

    printf("%d trials\n", ntrials);
    printf("avg time: %f\n", (float) sum / (float) ntrials);

    free(inputs);
    free(tot_time);
}

static void
garb_full(GarbledCircuit *gc, int num_garb_inputs, int num_eval_inputs,
          int ntrials)
{
    InputMapping imap;
    block *inputLabels = allocate_blocks(2 * gc->n);
    block *outputMap = allocate_blocks(2 * gc->m);

    garbleCircuit(gc, inputLabels, outputMap, GARBLE_TYPE_STANDARD);

    newInputMapping(&imap, num_eval_inputs + num_garb_inputs);

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

    {
        int *garb_inputs = malloc(sizeof(int) * num_garb_inputs);
        unsigned long *tot_time = malloc(sizeof(unsigned long) * ntrials);

        for (int i = 0; i < ntrials; ++i) {
            for (int i = 0; i < num_garb_inputs; i++) {
                garb_inputs[i] = rand() % 2; 
            }
            garbler_classic_2pc(gc, inputLabels, &imap, outputMap,
                                num_garb_inputs, num_eval_inputs, garb_inputs,
                                &tot_time[i]);
            printf("%lu\n", tot_time[i]);
        }

        qsort(tot_time, ntrials, sizeof(unsigned long), compare);
        printf("%d trials\n", ntrials);
        printf("time (cycles): %lu\n", tot_time[ntrials / 2 + 1]);

        free(garb_inputs);
        free(tot_time);
    }

    deleteInputMapping(&imap);
    removeGarbledCircuit(gc);

    free(inputLabels);
    free(outputMap);
}

static void
eval_full(int n_garb_inputs, int n_eval_inputs, int noutputs, int ntrials)
{
    unsigned long *tot_time = malloc(sizeof(unsigned long) * ntrials);
    int *eval_inputs = malloc(sizeof(int) * n_eval_inputs);
    int *output = malloc(sizeof(int) * noutputs);

    for (int i = 0; i < ntrials; ++i) {
        sleep(1);
        for (int i = 0; i < n_eval_inputs; i++) {
            eval_inputs[i] = rand() % 2;
        }
        evaluator_classic_2pc(eval_inputs, output, n_garb_inputs, 
                              n_eval_inputs, &tot_time[i]);
        printf("%lu\n", tot_time[i]);
    }

    qsort(tot_time, ntrials, sizeof(unsigned long), compare);
    printf("%d trials\n", ntrials);
    printf("time (cycles): %lu\n", tot_time[ntrials / 2 + 1]);

    free(output);
    free(eval_inputs);
    free(tot_time);
}

typedef enum {
    GARB_OFF_AND,
    GARB_OFF_AES,
    GARB_OFF_CBC,
    GARB_OFF_NONE
} GarbOff;

struct args {
    char *fspec;
    GarbOff garb_type;
};

int
main(int argc, char *argv[])
{
    int c, idx;
    int ntrials = 1;
    block delta;
    GarbledCircuit gc;
    /* struct args args = { NULL, GARB_OFF_NONE, }; */

    seedRandom();

    delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    while ((c = getopt_long(argc, argv, "", opts, &idx)) != -1) {
        switch (c) {
        case 0:
            break;
        case 'a':
            and_garb_off(GARBLER_DIR, 128, 10, NUM_GCS);
            exit(EXIT_SUCCESS);
        case 'A':
            eval_off(0, NUM_GCS);
            exit(EXIT_SUCCESS);
        case 'b':
            garb_on("functions/and.json", 128, NUM_GCS, ntrials);
            exit(EXIT_SUCCESS);
        case 'B':
            eval_on(0, NUM_GCS, ntrials);
            exit(EXIT_SUCCESS);
        case 'c':
            and_garb_full(128, 10);
            exit(EXIT_SUCCESS);
        case 'C':
            and_eval_full(128);
            exit(EXIT_SUCCESS);

            /* AES */
        case 'd':
            aes_garb_off(GARBLER_DIR, NUM_GCS);
            exit(EXIT_SUCCESS);
        case 'D':
            eval_off(128, NUM_GCS);
            exit(EXIT_SUCCESS);
        case 'e':
            garb_on("functions/aes.json", 1280, NUM_GCS, ntrials);
            exit(EXIT_SUCCESS);
        case 'E':
            eval_on(128, NUM_GCS, ntrials);
            exit(EXIT_SUCCESS);
        case 'f':
            buildAESCircuit(&gc);
            garb_full(&gc, 1280, 128, ntrials);
            exit(EXIT_SUCCESS);
        case 'F':
            eval_full(1280, 128, 128, ntrials);
            exit(EXIT_SUCCESS);

            /* CBC */
        case 'g':
            cbc_garb_off(GARBLER_DIR);
            exit(EXIT_SUCCESS);
        case 'G':
            eval_off(getNumEvalInputs(), getNumCircs());
            exit(EXIT_SUCCESS);
        case 'i':
            garb_on("functions/cbc_10_10.json", getNumGarbInputs(),
                    getNumCircs(), ntrials);
            exit(EXIT_SUCCESS);
        case 'I':
            eval_on(getNumEvalInputs(), getNumCircs(), ntrials);
            exit(EXIT_SUCCESS);
        case 'j':
            buildCBCFullCircuit(&gc, NUM_CBC_BLOCKS, NUM_AES_ROUNDS, &delta);
            garb_full(&gc, getNumGarbInputs(), getNumEvalInputs(), ntrials);
            exit(EXIT_SUCCESS);
        case 'J':
            eval_full(getNumGarbInputs(), getNumEvalInputs(), getM(), ntrials);
            exit(EXIT_SUCCESS);
        case 't':
            ntrials = NUM_TRIALS;
            break;
        case '?':
            break;
        default:
            assert(false);
            abort();
        }
    }

    return EXIT_SUCCESS;
}
