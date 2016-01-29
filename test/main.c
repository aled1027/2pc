#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "justGarble.h"
/* #include "2pc_and.h" */
#include "2pc_aes.h"
#include "2pc_cbc.h"
#include "2pc_leven.h"
#include "2pc_tests.h"

#include "justGarble.h"
#include "garble.h"
#include "circuits.h"

#define GARBLER_DIR "files/garbler_gcs"
#define EVALUATOR_DIR "files/evaluator_gcs"

typedef enum { AND, AES, CBC, LEVEN } experiment;

struct args {
    bool garb_off;
    bool eval_off;
    bool garb_on;
    bool eval_on;
    bool garb_full;
    bool eval_full;
    experiment type;
    int ntrials;
};

static void
args_init(struct args *args)
{
    args->garb_off = 0;
    args->eval_off = 0;
    args->garb_on = 0;
    args->eval_on = 0;
    args->garb_full = 0;
    args->eval_full = 0;
    args->type = AES;
    args->ntrials = 1;
}

static struct option opts[] =
{
    {"garb-off", no_argument, 0, 'g'},
    {"eval-off", no_argument, 0, 'e'},
    {"garb-on", no_argument, 0, 'G'},
    {"eval-on", no_argument, 0, 'E'},
    {"garb-full", no_argument, 0, 'f'},
    {"eval-full", no_argument, 0, 'F'},
    {"test", no_argument, 0, 'p'},
    {"type", required_argument, 0, 't'},
    {"times", required_argument, 0, 'T'},
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

        //printf("inputs: ");
        //for (int j = 0; j < ninputs; j++) {
        //    inputs[j] = rand() % 2; 
        //    printf("%d", inputs[j]);
        //}
        //printf("\n");
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
eval_on(int ninputs, int nlabels, int nchains, int ntrials) {
    unsigned long sum = 0, *tot_time;
    int *inputs;

    tot_time = malloc(sizeof(unsigned long) * ntrials);
    inputs = malloc(sizeof(int) * ninputs);

    for (int i = 0; i < ntrials; i++) {
        sleep(1); // uncomment this if getting hung up
        //printf("inputs: ");
        //for (int j = 0; j < ninputs; j++) {
        //    inputs[j] = rand() % 2;
        //    printf("%d", inputs[j]);
        //}
        //printf("\n");
        evaluator_online(EVALUATOR_DIR, inputs, ninputs, nchains, &tot_time[i]);
        printf("total: %lu\n", tot_time[i]);
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
    block *outputMap = allocate_blocks(2 * gc->m);

    garbleCircuit(gc, outputMap, GARBLE_TYPE_STANDARD);

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
            garbler_classic_2pc(gc, &imap, outputMap, num_garb_inputs,
                                num_eval_inputs, garb_inputs, &tot_time[i]);
            printf("%lu\n", tot_time[i]);
        }

        qsort(tot_time, ntrials, sizeof(unsigned long), compare);
        printf("%d trials\n", ntrials);
        printf("time (cycles): %lu\n", tot_time[ntrials / 2 + 1]);

        free(garb_inputs);
        free(tot_time);
    }

    deleteInputMapping(&imap);

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

static int
go(struct args *args)
{
    int n_garb_inputs, n_eval_inputs, n_eval_labels, noutputs, ncircs;
    char *fn, *type;

    switch (args->type) {
    case AES:
        n_garb_inputs = aesNumGarbInputs();
        n_eval_inputs = aesNumEvalInputs();
        n_eval_labels = n_eval_inputs;
        ncircs = aesNumCircs();
        noutputs = aesNumOutputs();
        fn = "functions/aes.json";
        type = "AES";
        break;
    case CBC:
        n_garb_inputs = cbcNumGarbInputs();
        n_eval_inputs = cbcNumEvalInputs();
        n_eval_labels = n_eval_inputs;
        ncircs = cbcNumCircs();
        noutputs = cbcNumOutputs();
        fn = "functions/cbc_10_10.json";
        type = "CBC";
        break;
    case LEVEN:
        n_garb_inputs = levenNumGarbInputs();
        n_eval_inputs = levenNumEvalInputs();
        n_eval_labels = levenNumEvalLabels();
        ncircs = levenNumCircs();
        noutputs = levenNumOutputs();
        fn = "functions/leven_2.json";
        type = "LEVEN";
        break;
    default:
        fprintf(stderr, "No type specified\n");
        exit(EXIT_FAILURE);
    }

    printf("Running %s with (%d, %d) inputs, %d outputs, %d chains, %d trials\n",
           type, n_garb_inputs, n_eval_inputs, noutputs, ncircs, args->ntrials);

    if (args->garb_off) {
        printf("Offline garbling\n");
        switch (args->type) {
        case AES:
            aes_garb_off(GARBLER_DIR, 10);
            break;
        case CBC:
            cbc_garb_off(GARBLER_DIR);
            break;
        case LEVEN:
            leven_garb_off();
            break;
        default:
            assert(false);
            break;
        }
    } else if (args->eval_off) {
        printf("Offline evaluating\n");
        eval_off(n_eval_labels, ncircs);
    } else if (args->garb_on) {
        printf("Online garbling\n");
        if (args->type == LEVEN) {
            leven_garb_on();
        } else {
            garb_on(fn, n_garb_inputs, ncircs, args->ntrials);
        }
    } else if (args->eval_on) {
        printf("Online evaluating\n");
        eval_on(n_eval_inputs, n_eval_labels, ncircs, args->ntrials);
    } else if (args->garb_full) {
        printf("Full garbling\n");
        GarbledCircuit gc;
        switch (args->type) {
        case AES:
            buildAESCircuit(&gc);
            break;
        case CBC:
        {
            block delta = randomBlock();
            *((uint16_t *) (&delta)) |= 1;
            buildCBCFullCircuit(&gc, NUM_CBC_BLOCKS, NUM_AES_ROUNDS, &delta);
            break;
        }
        case LEVEN:
            break;
        default:
            assert(false);
            break;
        }
        if (args->type == LEVEN) {
            full_leven_garb();
        } else {
            garb_full(&gc, n_garb_inputs, n_eval_inputs, args->ntrials);
            removeGarbledCircuit(&gc);
        }
    } else if (args->eval_full) {
        printf("Full evaluating\n");
        if (args->type == LEVEN) {
            full_leven_eval();
        } else {
            eval_full(n_garb_inputs, n_eval_inputs, noutputs, args->ntrials);
        }
    } else {
        fprintf(stderr, "No role specified\n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
    int c, idx;
    struct args args;

    (void) seedRandom(NULL);

    args_init(&args);


    while ((c = getopt_long(argc, argv, "", opts, &idx)) != -1) {
        switch (c) {
        case 0:
            break;
        case 'g':
            args.garb_off = true;
            break;
        case 'e':
            args.eval_off = true;
            break;
        case 'G':
            args.garb_on = true;
            break;
        case 'E':
            args.eval_on = true;
            break;
        case 'f':
            args.garb_full = true;
            break;
        case 'F':
            args.eval_full = true;
            break;
        case 't':
            if (strcmp(optarg, "AND") == 0) {
                args.type = AND;
            } else if (strcmp(optarg, "AES") == 0) {
                args.type = AES;
            } else if (strcmp(optarg, "CBC") == 0) {
                args.type = CBC;
            } else if (strcmp(optarg, "LEVEN") == 0) {
                args.type = LEVEN;
            } else {
                fprintf(stderr, "Unknown type\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'T':
            args.ntrials = atoi(optarg);
            break;
        case 'p':
            printf("Running tests\n");
            runAllTests();
            return 0;
            break;
        case '?':
            break;
        default:
            assert(false);
            abort();
        }
    }
    return go(&args);
}
