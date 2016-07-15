#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "2pc_aes.h"
#include "2pc_cbc.h"
#include "2pc_leven.h"
#include "2pc_tests.h"
#include "2pc_hyperplane.h"
#include "net.h"
#include "utils.h"
#include "ml_models.h"

#include "garble.h"
#include "circuits.h"

#define GARBLER_DIR "files/garbler_gcs"
#define EVALUATOR_DIR "files/evaluator_gcs"

typedef enum { EXPERIMENT_AES, EXPERIMENT_CBC, EXPERIMENT_LEVEN, EXPERIMENT_WDBC, EXPERIMENT_HP_CREDIT, EXPERIMENT_HYPERPLANE, EXPERIMENT_RANDOM_DT} experiment;

static int getDIntSize(int l) { return (int) floor(log2(l)) + 1; }
static int getInputsDevotedToD(int l) { return getDIntSize(l) * (l+1); }

struct args {
    ChainingType chaining_type;
    bool garb_off;
    bool eval_off;
    bool garb_on;
    bool eval_on;
    bool garb_full;
    bool eval_full;
    uint64_t nsymbols;
    experiment type;
    uint64_t ntrials;
};

static void
args_init(struct args *args)
{
    args->chaining_type = CHAINING_TYPE_STANDARD;
    args->garb_off = 0;
    args->eval_off = 0;
    args->garb_on = 0;
    args->eval_on = 0;
    args->garb_full = 0;
    args->eval_full = 0;
    args->type = EXPERIMENT_AES;
    args->ntrials = 1;
    args->nsymbols = 5;
}

static struct option opts[] =
{
    {"chaining", required_argument, 0, 'c'},
    {"garb-off", no_argument, 0, 'g'},
    {"eval-off", no_argument, 0, 'e'},
    {"garb-on", no_argument, 0, 'G'},
    {"eval-on", no_argument, 0, 'E'},
    {"garb-full", no_argument, 0, 'f'},
    {"eval-full", no_argument, 0, 'F'},
    {"nsymbols", required_argument, 0, 'l'},
    {"test", no_argument, 0, 'p'},
    {"type", required_argument, 0, 't'},
    {"times", required_argument, 0, 'T'},
    {0, 0, 0, 0}
};

static void
eval_off(int ninputs, int nchains, ChainingType chainingType)
{
    evaluator_offline(EVALUATOR_DIR, ninputs, nchains, chainingType);
}

static int
compare(const void * a, const void * b)
{
	return (int) (*(uint64_t*) a - *(uint64_t*) b);
}

static uint64_t
average(uint64_t *a, uint64_t n)
{
    uint64_t avg = 0;
    for (int i = 0; i < n; ++i) {
        avg += a[i];
    }
    return avg / n;
}

static double
confidence(uint64_t *a, uint64_t n, uint64_t avg)
{
    uint64_t tmp = 0;
    double sigma;
    for (int i = 0; i < n; ++i) {
        tmp += (a[i] - avg)*(a[i] - avg);
    }
    tmp /= n;
    sigma = sqrt((double) tmp);
    return 1.96 * sigma / sqrt((double) n);
}

static void
results(const char *name, uint64_t *totals, uint64_t *totals_no_load, uint64_t n)
{
    uint64_t avg;
    double conf;

    avg = average(totals, n);
    conf = confidence(totals, n, avg);
    printf("%s avg: %llu +- %f microsec\n", name, avg / 1000, conf / 1000);
    if (totals_no_load) {
        avg = average(totals_no_load, n);
        conf = confidence(totals_no_load, n, avg);
        printf("%s avg (no load): %llu +- %f microsec\n", name, avg / 1000, conf / 1000);
    }
    printf("%s Kbits sent: %lu\n", name, g_bytes_sent * 8 / 1000);
    printf("%s Kbits received: %lu\n", name, g_bytes_received * 8 / 1000);
}

static void
garb_on(char *function_path, int ninputs, int nchains, uint64_t ntrials,
        ChainingType chainingType, int l, int sigma, experiment which_experiment)
{
    uint64_t *tot_time;
    bool *inputs;

    inputs = calloc(ninputs, sizeof(bool));

    if (EXPERIMENT_LEVEN == which_experiment) {
        printf("l = %d, sigma = %d\n", l, sigma);
        int DIntSize = getDIntSize(l);
        int inputsDevotedToD = getInputsDevotedToD(l);
        int numGarbInputs = levenNumGarbInputs(l, sigma);
        assert(numGarbInputs == ninputs);
        /* The first inputsDevotedToD inputs are 0 to l+1 in binary */
        for (int i = 0; i < l + 1; i++) 
            convertToBinary(i, inputs + (DIntSize) * i, DIntSize);

        for (int i = inputsDevotedToD; i < numGarbInputs; i++) {
            inputs[i] = rand() % 2;
        }
    } else if (EXPERIMENT_WDBC == which_experiment) {
        load_model_into_inputs(inputs, "wdbc");
    } else if (EXPERIMENT_HP_CREDIT == which_experiment) {
        load_model_into_inputs(inputs, "credit");
    } else {
        for (int i = 0; i < ninputs; i++) {
            inputs[i] = rand() % 2;
        }
    }
    tot_time = malloc(sizeof(uint64_t) * ntrials);

    for (int i = 0; i < ntrials; i++) {
        sleep(1);
        g_bytes_sent = g_bytes_received = 0;
        garbler_online(function_path, GARBLER_DIR, inputs, ninputs, nchains, 
                       &tot_time[i], chainingType);
        fprintf(stderr, "Total: %llu\n", tot_time[i]);
    }

    results("GARB", tot_time, NULL, ntrials);

    free(inputs);
    free(tot_time);
}

static void
eval_on(int ninputs, int nlabels, int nchains, int ntrials,
        ChainingType chainingType)
{
    uint64_t *tot_time, *tot_time_no_load;
    int *inputs;

    tot_time = calloc(ntrials, sizeof(uint64_t));
    tot_time_no_load = calloc(ntrials, sizeof(uint64_t));
    inputs = calloc(ninputs, sizeof(int));

    for (int i = 0; i < ntrials; i++) {
        sleep(2); // uncomment this if getting hung up
        g_bytes_sent = g_bytes_received = 0;
        for (int j = 0; j < ninputs; j++) {
            inputs[j] = rand() % 2;
        }
        evaluator_online(EVALUATOR_DIR, inputs, ninputs, nchains, chainingType,
                         &tot_time[i], &tot_time_no_load[i]);
        fprintf(stderr, "Total: %llu\n", tot_time[i]);
        fprintf(stderr, "Total (no load): %llu\n", tot_time_no_load[i]);
    }

    results("EVAL", tot_time, tot_time_no_load, ntrials);

    free(inputs);
    free(tot_time);
}

static void
garb_full(garble_circuit *gc, int num_garb_inputs, int num_eval_inputs,
          int ntrials, int l, int sigma, experiment which_experiment)
{
    OldInputMapping imap;
    uint64_t start, end;
    block *outputMap = garble_allocate_blocks(2 * gc->m);
    newOldInputMapping(&imap, num_garb_inputs, num_eval_inputs);

    {
        bool *inputs = calloc(num_garb_inputs, sizeof(bool));
        uint64_t *tot_time = calloc(ntrials, sizeof(uint64_t));

        for (int i = 0; i < ntrials; ++i) {
            g_bytes_sent = g_bytes_received = 0;
            int DIntSize = (int) floor(log2(l)) + 1;
            int inputsDevotedToD = DIntSize * (l+1);
            if (EXPERIMENT_LEVEN == which_experiment) {
                /* The first inputsDevotedToD inputs are the numbers 0 through
                 * l+1 encoded in binary */
                for (int i = 0; i < l + 1; i++) {
                    convertToBinary(i, inputs + (DIntSize) * i, DIntSize);
                }
                for (int i = inputsDevotedToD; i < num_garb_inputs; i++) {
                    inputs[i] = rand() % 2;
                }
            } else if (EXPERIMENT_WDBC == which_experiment) {
                load_model_into_inputs(inputs, "wdbc");
            } else if (EXPERIMENT_HP_CREDIT == which_experiment) {
                load_model_into_inputs(inputs, "credit");
            } else {
                for (int i = 0; i < num_garb_inputs; i++) {
                    inputs[i] = rand() % 2; 
                }
            }
            start = current_time_();
            garble_garble(gc, NULL, outputMap);
            end = current_time_();
            fprintf(stderr, "Garble: %llu\n", end - start);
            tot_time[i] += end - start;
            garbler_classic_2pc(gc, &imap, outputMap, num_garb_inputs,
                                num_eval_inputs, inputs, &tot_time[i]);
            fprintf(stderr, "Total: %llu\n", tot_time[i]);
        }

        results("GARB", tot_time, NULL, ntrials);

        free(inputs);
        free(tot_time);
    }

    deleteOldInputMapping(&imap);
    free(outputMap);
}

static void
eval_full(int n_garb_inputs, int n_eval_inputs, int noutputs, int ntrials)
{
    uint64_t *tot_time = calloc(ntrials, sizeof(uint64_t));
    int *eval_inputs = malloc(sizeof(int) * n_eval_inputs);
    bool *output = malloc(sizeof(bool) * noutputs);

    for (int i = 0; i < ntrials; ++i) {
        sleep(1);
        g_bytes_sent = g_bytes_received = 0;
        for (int i = 0; i < n_eval_inputs; i++) {
            eval_inputs[i] = rand() % 2;
        }
        evaluator_classic_2pc(eval_inputs, output, n_garb_inputs, 
                              n_eval_inputs, &tot_time[i]);
        fprintf(stderr, "Total: %llu\n", tot_time[i]);
    }

    results("EVAL", tot_time, NULL, ntrials);

    free(output);
    free(eval_inputs);
    free(tot_time);
}

static int
go(struct args *args)
{
    uint64_t n_garb_inputs, n_eval_inputs, n_eval_labels, noutputs, ncircs, sigma;
    uint64_t n = 0, l = 0, num_len = 0;
    char *fn, *type;
    /* ChainingType chainingType; */

    /* chainingType = CHAINING_TYPE_SIMD; */
    switch (args->type) {
    case EXPERIMENT_AES:
        n_garb_inputs = aesNumGarbInputs();
        n_eval_inputs = aesNumEvalInputs();
        n_eval_labels = n_eval_inputs;
        ncircs = aesNumCircs();
        noutputs = aesNumOutputs();
        fn = "functions/aes.json";
        type = "AES";
        break;
    case EXPERIMENT_CBC:
        n_garb_inputs = cbcNumGarbInputs();
        n_eval_inputs = cbcNumEvalInputs();
        n_eval_labels = n_eval_inputs;
        ncircs = cbcNumCircs();
        noutputs = cbcNumOutputs();
        fn = "functions/cbc_10_10.json";
        type = "CBC";
        break;
    case EXPERIMENT_LEVEN:
        l = args->nsymbols;
        sigma = 8;
        n_garb_inputs = levenNumGarbInputs(l, sigma);
        n_eval_inputs = levenNumEvalInputs(l, sigma);
        n_eval_labels = n_eval_inputs;
        ncircs = levenNumCircs(l);
        noutputs = levenNumOutputs(l);
        fn = NULL;              /* set later */
        type = "LEVEN";
        break;
    case EXPERIMENT_WDBC:
        printf("Experiment linear\n");
        num_len = 55;
        n = 31 * 2 * num_len;
        ncircs = 2;

        n_garb_inputs = n / 2;
        n_eval_inputs = n / 2;
        n_eval_labels = n_eval_inputs;
        type = "LINEAR";
        fn = "functions/simple_hyperplane.json";
        break;
    case EXPERIMENT_HP_CREDIT:
        printf("Experiment linear\n");
        // TODO ACTUALLY HYPERPLANE WITH 1 vector
        num_len = 58;
        n = 48 * 2 * num_len;
        ncircs = 2;

        n_garb_inputs = n / 2;
        n_eval_inputs = n / 2;
        n_eval_labels = n_eval_inputs;
        type = "LINEAR";
        fn = "functions/credit.json";
        break;
    case EXPERIMENT_RANDOM_DT:
        printf("Experiment random dt\n");
        n = 31 * 2 * num_len;
        ncircs = 2;
        n_garb_inputs = n / 2;
        n_eval_inputs = n / 2;
        n_eval_labels = n_eval_inputs;
        type = "DT";
        fn = "functions/decision_tree.json";
        break;
    case EXPERIMENT_HYPERPLANE:
        printf("Experiment hyperplane\n");
        fn = NULL; // TODO add function
        n_garb_inputs = 4;
        n_eval_inputs = 4;
        n_eval_labels = n_eval_inputs;
        type = "HYPERPLANE";
        break;
    default:
        fprintf(stderr, "No type specified\n");
        exit(EXIT_FAILURE);
    }

    if (args->chaining_type == CHAINING_TYPE_SIMD)
        printf("Using CHAINING_TYPE_SIMD\n");
    else
        printf("Using CHAINING_TYPE_STANDARD\n");

    printf("Running %s with (%d, %d) inputs, %d outputs, %d chains, %d trials\n",
           type, n_garb_inputs, n_eval_inputs, noutputs, ncircs, args->ntrials);

    if (args->garb_off) {
        printf("Offline garbling\n");
        switch (args->type) {
        case EXPERIMENT_AES:
            aes_garb_off(GARBLER_DIR, 10, args->chaining_type);
            break;
        case EXPERIMENT_CBC:
            cbc_garb_off(GARBLER_DIR, args->chaining_type);
            break;
        case EXPERIMENT_LEVEN:
            leven_garb_off(l, sigma, args->chaining_type);
            break;
        case EXPERIMENT_WDBC:
            hyperplane_garb_off(GARBLER_DIR, n, num_len, WDBC);
            break;
        case EXPERIMENT_HP_CREDIT:
            hyperplane_garb_off(GARBLER_DIR, n, num_len, CREDIT);
            break;
        case EXPERIMENT_RANDOM_DT:
            dt_garb_off(GARBLER_DIR, n, num_len, DT_RANDOM);
            break;
        case EXPERIMENT_HYPERPLANE:
            printf("EXPERIMENT_HYPERPLANE garb off\n");
            break;
        }
    } else if (args->eval_off) {
        printf("Offline evaluating\n");
        eval_off(n_eval_inputs, ncircs, args->chaining_type);
    } else if (args->garb_on) {
        printf("Online garbling\n");
        if (args->type == EXPERIMENT_LEVEN) {
            char *fn;
            size_t size;

            size = strlen("functions/leven_.json")
                + (int) floor(log10((float) l)) + 2;
            fn = malloc(size);
            (void) snprintf(fn, size, "functions/leven_%d.json", l);
            garb_on(fn, n_garb_inputs, ncircs, args->ntrials, args->chaining_type, l, sigma, args->type);
            free(fn);
        } else {
            garb_on(fn, n_garb_inputs, ncircs, args->ntrials, args->chaining_type, 0, 0, args->type);
        }
    } else if (args->eval_on) {
        printf("Online evaluating\n");
        eval_on(n_eval_inputs, n_eval_labels, ncircs, args->ntrials, args->chaining_type);
    } else if (args->garb_full) {
        printf("Full garbling\n");
        garble_circuit gc;
        switch (args->type) {
        case EXPERIMENT_AES:
            buildAESCircuit(&gc);
            break;
        case EXPERIMENT_CBC:
        {
            block delta = garble_create_delta();
            buildCBCFullCircuit(&gc, NUM_CBC_BLOCKS, NUM_AES_ROUNDS, &delta);
            break;
        }
        case EXPERIMENT_LEVEN:
            buildLevenshteinCircuit(&gc, l, sigma);
            break;
        case EXPERIMENT_WDBC:
            printf("experiment wdbc\n");
            buildLinearCircuit(&gc, n, num_len);
            break;
        case EXPERIMENT_HP_CREDIT:
            printf("experiment credit\n");
            buildLinearCircuit(&gc, n, num_len);
            break;
        case EXPERIMENT_RANDOM_DT:
            printf("experiment linear\n");
            // TODO CHANGE THIS
            buildLinearCircuit(&gc, n, num_len);
            break;
        case EXPERIMENT_HYPERPLANE:
            printf("experiment hyperplane\n");
            buildHyperCircuit(&gc);
            break;
        default:
            assert(false);
            return EXIT_FAILURE;
        }
        garb_full(&gc, n_garb_inputs, n_eval_inputs, args->ntrials, l, sigma, args->type);
        //garble_delete(&gc);
    } else if (args->eval_full) {
        printf("Full evaluating\n");
        eval_full(n_garb_inputs, n_eval_inputs, noutputs, args->ntrials);
    } else {
        fprintf(stderr, "No role specified\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
    int c, idx;
    struct args args;

    (void) garble_seed(NULL);

    args_init(&args);

    while ((c = getopt_long(argc, argv, "", opts, &idx)) != -1) {
        switch (c) {
        case 0:
            break;
        case 'c':
            if (strcmp(optarg, "STANDARD") == 0) {
                args.chaining_type = CHAINING_TYPE_STANDARD;
            } else if (strcmp(optarg, "SCMC") == 0) {
                args.chaining_type = CHAINING_TYPE_SIMD;
            } else {
                fprintf(stderr, "Unknown chaining type %s\n", optarg);
                exit(EXIT_FAILURE);
            }
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
        case 'l':
            args.nsymbols = atoi(optarg);
            break;
        case 't':
            if (strcmp(optarg, "AES") == 0) {
                args.type = EXPERIMENT_AES;
            } else if (strcmp(optarg, "CBC") == 0) {
                args.type = EXPERIMENT_CBC;
            } else if (strcmp(optarg, "LEVEN") == 0) {
                args.type = EXPERIMENT_LEVEN;
            } else if (strcmp(optarg, "WDBC") == 0) {
                args.type = EXPERIMENT_WDBC;
            } else if (strcmp(optarg, "CREDIT") == 0) {
                args.type = EXPERIMENT_HP_CREDIT;
            } else if (strcmp(optarg, "HYPER") == 0) {
                args.type = EXPERIMENT_WDBC;
            } else if (strcmp(optarg, "RANDOM_DT") == 0) {
                args.type = EXPERIMENT_RANDOM_DT;
            } else {
                fprintf(stderr, "Unknown circuit type %s\n", optarg);
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
