
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "justGarble.h"
#include "2pc_aes.h"
#include "2pc_cbc.h"

static int NUM_GCS = 10;
static int NUM_TRIALS = 20;
static int MEDIAN_IDX = 11;

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

void
eval_off(int ninputs, int nchains)
{
    ChainedGarbledCircuit *gcs;

    gcs = malloc(sizeof(ChainedGarbledCircuit) * nchains);
    evaluator_offline(gcs, ninputs, nchains);
    free(gcs);
}

int myCompare(const void * a, const void * b)
{
	return (int) (*(unsigned long*) a - *(unsigned long*) b);
}

void
garb_on(char* function_path, int ninputs, int nchains, bool timing)
{
    unsigned long *ot_time, *tot_time;
    int ntrials = timing ? NUM_TRIALS : 1;

    ot_time = malloc(sizeof(unsigned long) * ntrials);
    tot_time = malloc(sizeof(unsigned long) * ntrials);

    for (int i = 0; i < ntrials; i++) {
        int *inputs;

        inputs = malloc(sizeof(int) * ninputs);
        for (int j = 0; j < ninputs; j++) {
            inputs[j] = rand() % 2; 
        }
        garbler_online(function_path, inputs, ninputs, nchains, 
                       &ot_time[i], &tot_time[i]);
        printf("%lu, %lu\n", ot_time[i], tot_time[i]);
    }

    if (timing) {
	    qsort(ot_time, ntrials, sizeof(unsigned long), myCompare);
	    qsort(tot_time, ntrials, sizeof(unsigned long), myCompare);
        printf("median ot_time: %lu\n", ot_time[MEDIAN_IDX]);
        printf("median tot_time: %lu\n", tot_time[MEDIAN_IDX]);
        printf("median time without ot: %lu\n", (tot_time[MEDIAN_IDX] - ot_time[MEDIAN_IDX]));
    } else {
        printf("ot_time: %lu\n", *ot_time);
        printf("tot_time: %lu\n", *tot_time);
        printf("time without ot: %lu\n", (*tot_time - *ot_time));
    }
}

void
eval_on(int ninputs, int nchains, bool timing)
{
    unsigned long *ot_time, *tot_time;
    int ntrials = timing ? NUM_TRIALS : 1;

    ot_time = malloc(sizeof(unsigned long) * ntrials);
    tot_time = malloc(sizeof(unsigned long) * ntrials);

    for (int i = 0; i < ntrials; i++) {
        int *inputs;

        sleep(1); // uncomment this if getting hung up
        inputs = malloc(sizeof(int) * ninputs);
        for (int j = 0; j < ninputs; j++) {
            inputs[j] = rand() % 2;
        }
        evaluator_online(inputs, ninputs, nchains, &ot_time[i], &tot_time[i]);
    }

    if (timing) {
	    qsort(ot_time, ntrials, sizeof(unsigned long), myCompare);
	    qsort(tot_time, ntrials, sizeof(unsigned long), myCompare);
        printf("median ot_time: %lu\n", ot_time[MEDIAN_IDX]);
        printf("median tot_time: %lu\n", tot_time[MEDIAN_IDX]);
        printf("median time without ot: %lu\n", (tot_time[MEDIAN_IDX] - ot_time[MEDIAN_IDX]));
    } else {
        printf("ot_time: %lu\n", *ot_time);
        printf("tot_time: %lu\n", *tot_time);
        printf("time without ot: %lu\n", (*tot_time - *ot_time));
    }
}

int
main(int argc, char *argv[])
{
    int c, idx;
    bool is_timing = false;

    seedRandom();

    while ((c = getopt_long(argc, argv, "", opts, &idx)) != -1) {
        switch (c) {
        case 0:
            break;
        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'c':
        case 'C':
            assert(false);
            exit(1);
        case 'd':
            aes_garb_off();
            exit(1);
        case 'D':
            eval_off(128, NUM_GCS);
            exit(1);
        case 'e':
            garb_on("functions/aes.json", 1280, NUM_GCS, is_timing);
            exit(1);
        case 'E':
            eval_on(128, NUM_GCS, is_timing);
            exit(1);
        case 'f':
            full_aes_garb();
            exit(1);
        case 'F':
            full_aes_eval();
            exit(1);
        case 'g':
            cbc_garb_off();
            exit(1);
        case 'G':
            eval_off(getNumEvalInputs(), getNumCircs());
            exit(1);
        case 'i':
            garb_on("functions/cbc_10_10.json", getNumGarbInputs(),
                    getNumCircs(), is_timing);
            exit(1);
        case 'I':
            eval_on(getNumEvalInputs(), getNumCircs(), is_timing);
            exit(1);
        case 'j':
            full_cbc_garb();
            exit(1);
        case 'J':
            full_cbc_eval();
            exit(1);
        case 't':
            is_timing = true;
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
