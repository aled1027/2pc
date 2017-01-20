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

typedef enum {
    EXPERIMENT_NONE,
    EXPERIMENT_AES, 
    EXPERIMENT_CBC, 
    EXPERIMENT_LEVEN, 
    EXPERIMENT_WDBC, 
    EXPERIMENT_HP_CREDIT, 
    EXPERIMENT_HYPERPLANE, 
    EXPERIMENT_RANDOM_DT, 
    EXPERIMENT_DT_NURSERY, 
    EXPERIMENT_DT_ECG, 
    EXPERIMENT_NB_WDBC,
    EXPERIMENT_NB_NURSERY,
    EXPERIMENT_NB_AUD
} experiment;

static int getDIntSize(int l) { return (int) floor(log2(l)) + 1; }
static int getInputsDevotedToD(int l) { return getDIntSize(l) * (l+1); }

struct args {
    char *progname;
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
args_init(struct args *args, const char *progname)
{
    args->progname = progname;
    args->chaining_type = CHAINING_TYPE_STANDARD;
    args->garb_off = 0;
    args->eval_off = 0;
    args->garb_on = 0;
    args->eval_on = 0;
    args->garb_full = 0;
    args->eval_full = 0;
    args->type = EXPERIMENT_NONE;
    args->ntrials = 1;
    args->nsymbols = 5;
}

static struct option opts[] =
{
    /* {"chaining", required_argument, 0, 'c'}, */
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
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

static void
usage(const char *prog, int ret)
{
    printf("%s [options]\n"
"Options:\n"
"  --garb-off      Do offline garbling\n"
"  --eval-off      Do offline evaluating\n"
"  --garb-on       Do online garbling\n"
"  --eval-on       Do online evaluating\n"
"  --garb-full     Do standard garbling\n"
"  --eval-full     Do standard evaluating\n"
"  --nsymbols N    Set number of symbols to N\n"
"  --test          Run all tests\n"
"  --type T        Run circuit T\n"
"                  Options: AES, CBC, LEVEN, WDBC, CREDIT, HYPER, RANDOM_DT, "
"NURSERY_DT, ECG_DT, WDBC_NB, NURSERY_NB, AUD_NB\n"
"  --times T       Do T runs\n", prog);
    exit(ret);
}

static void
eval_off(int ninputs, int nchains, ChainingType chainingType)
{
    evaluator_offline(EVALUATOR_DIR, ninputs, nchains, chainingType);
}

static size_t
average(uint64_t *a, size_t n)
{
    uint64_t avg = 0;
    for (size_t i = 0; i < n; ++i) {
        avg += a[i];
    }
    return avg / n;
}

static double
confidence(uint64_t *a, size_t n, uint64_t avg)
{
    uint64_t tmp = 0;
    double sigma;
    for (size_t i = 0; i < n; ++i) {
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
    printf("%s avg: %lu +- %f microsec\n", name, avg / 1000, conf / 1000);
    if (totals_no_load) {
        avg = average(totals_no_load, n);
        conf = confidence(totals_no_load, n, avg);
        printf("%s avg (no load): %lu +- %f microsec\n", name, avg / 1000, conf / 1000);
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

    } else if (EXPERIMENT_NB_WDBC == which_experiment) {
        load_model_into_inputs(inputs, "nb_wdbc");

    // Load nursery nb randomly; the model was invalid
    //} else if (EXPERIMENT_NB_NURSERY == which_experiment) {
    //    load_model_into_inputs(inputs, "nb_nursery");

    } else {
        for (int i = 0; i < ninputs; i++) {
            inputs[i] = rand() % 2;
        }
    }
    tot_time = malloc(sizeof(uint64_t) * ntrials);

    for (size_t i = 0; i < ntrials; i++) {
        sleep(1);
        g_bytes_sent = g_bytes_received = 0;
        garbler_online(function_path, GARBLER_DIR, inputs, ninputs, nchains, 
                       &tot_time[i], chainingType);
        fprintf(stderr, "Total: %lu\n", tot_time[i]);
    }

    results("GARB", tot_time, NULL, ntrials);

    free(inputs);
    free(tot_time);
}

static void
eval_on(int ninputs, int nlabels, int nchains, int ntrials,
        ChainingType chainingType, ChainedGarbledCircuit *cgcs)
{
    (void) nlabels;
    uint64_t *tot_time, *tot_time_no_load;
    int *inputs;

    tot_time = calloc(ntrials, sizeof tot_time[0]);
    tot_time_no_load = calloc(ntrials, sizeof tot_time_no_load[0]);
    inputs = calloc(ninputs, sizeof inputs[0]);

    for (int i = 0; i < ntrials; i++) {
        sleep(2); // uncomment this if getting hung up
        g_bytes_sent = g_bytes_received = 0;
        for (int j = 0; j < ninputs; j++) {
            inputs[j] = rand() % 2;
        }
        evaluator_online(EVALUATOR_DIR, inputs, ninputs, nchains, chainingType,
                         &tot_time[i], &tot_time_no_load[i], cgcs);
        fprintf(stderr, "Total: %lu\n", tot_time[i]);
        fprintf(stderr, "Total (no load): %lu\n", tot_time_no_load[i]);
    }

    results("EVAL", tot_time, tot_time_no_load, ntrials);

    free(inputs);
    free(tot_time);
    free(tot_time_no_load);
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
            fprintf(stderr, "Garble: %llu ns\n", end - start);
            tot_time[i] += end - start;
            garbler_classic_2pc(gc, &imap, outputMap, num_garb_inputs,
                                num_eval_inputs, inputs, &tot_time[i]);
            fprintf(stderr, "Total: %llu ns\n", tot_time[i]);
        }

        results("GARB", tot_time, NULL, ntrials);

        free(inputs);
        free(tot_time);
    }

    deleteOldInputMapping(&imap);
    free(outputMap);
}

static void
eval_full(garble_circuit *gc, int n_garb_inputs, int n_eval_inputs, int noutputs, int ntrials)
{
    uint64_t *tot_time = calloc(ntrials, sizeof(uint64_t));
    int *eval_inputs = malloc(sizeof(int) * n_eval_inputs);
    bool *output = malloc(sizeof(bool) * noutputs);

    for (int i = 0; i < ntrials; ++i) {
        g_bytes_sent = g_bytes_received = 0;
        for (int i = 0; i < n_eval_inputs; i++) {
            eval_inputs[i] = rand() % 2;
        }
        evaluator_classic_2pc(gc, eval_inputs, output, n_garb_inputs, 
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
    uint64_t n_garb_inputs = 0, n_eval_inputs = 0, n_eval_labels = 0, noutputs = 0, ncircs = 0, sigma = 0;
    uint64_t n = 0, l = 0, num_len = 0;

    char *fn, *type;

    // these are for naive bayes only
    int num_classes = 0, vector_size = 0, domain_size = 0, client_input_size = 0;
    int C_size = 0, T_size = 0;


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
    case EXPERIMENT_DT_NURSERY:
        printf("Experiment nursery dt\n");
        num_len = 52;
        n = 4 * 2 * num_len;
        ncircs = 7;
        n_garb_inputs = n / 2;
        n_eval_inputs = n / 2;
        n_eval_labels = n_eval_inputs;
        noutputs = 4;
        type = "DT";
        fn = "functions/nursery_dt.json";
        break;
    case EXPERIMENT_DT_ECG:
        printf("Experiment cg dt\n");
        num_len = 52;
        n = 6 * 2 * num_len;
        ncircs = 13;
        n_garb_inputs = n / 2;
        n_eval_inputs = n / 2;
        n_eval_labels = n_eval_inputs;
        noutputs = 1;
        type = "DT";
        fn = "functions/ecg_dt.json";
        break;
    case EXPERIMENT_NB_WDBC:
        printf("Experiment nursery naive bayes\n");
        num_len = 52;
        num_classes = 2;
        vector_size = 9;
        domain_size = 10;
        client_input_size = vector_size * num_len; 
        C_size = num_classes * num_len;
        T_size = num_classes * vector_size * domain_size * num_len;
        n = client_input_size + C_size + T_size;

        ncircs = (num_classes * vector_size) + (num_classes * vector_size) + 1;
        n_eval_inputs = client_input_size;
        n_garb_inputs = n - client_input_size;
        type = "Naive bayes";
        fn = "functions/wdbc_nb.json";
        break;
    case EXPERIMENT_NB_NURSERY:
        printf("Experiment nursery naive bayes\n");
        num_len = 52;
        num_classes = 5;
        vector_size = 9;
        domain_size = 5;
        client_input_size = vector_size * num_len; 
        C_size = num_classes * num_len;
        T_size = num_classes * vector_size * domain_size * num_len;
        n = client_input_size + C_size + T_size;

        ncircs = (num_classes * vector_size) + (num_classes * vector_size) + 1;
        n_eval_inputs = client_input_size;
        n_garb_inputs = n - client_input_size;
        type = "Naive bayes";
        fn = "functions/nursery_nb.json";
        break; 
    case EXPERIMENT_NB_AUD:
        printf("Experiment nursery naive bayes\n");
        num_len = 52;
        num_classes = 5;
        vector_size = 70;
        domain_size = 5;
        client_input_size = vector_size * num_len; 
        C_size = num_classes * num_len;
        T_size = num_classes * vector_size * domain_size * num_len;
        n = client_input_size + C_size + T_size;

        ncircs = (num_classes * vector_size) + (num_classes * vector_size) + 1;
        n_eval_inputs = client_input_size;
        n_garb_inputs = n - client_input_size;
        type = "Naive bayes";
        fn = "functions/nursery_nb.json";
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
        fprintf(stderr, "error: no type specified\n");
        usage(args->progname, EXIT_FAILURE);
    }

    if (args->chaining_type != CHAINING_TYPE_STANDARD)
        abort();

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
        case EXPERIMENT_DT_NURSERY:
            dt_garb_off(GARBLER_DIR, n, num_len, DT_NURSERY);
            break;
        case EXPERIMENT_DT_ECG:
            dt_garb_off(GARBLER_DIR, n, num_len, DT_ECG);
            break;
        case EXPERIMENT_NB_WDBC:
            nb_garb_off(GARBLER_DIR, num_len, num_classes, vector_size, domain_size, NB_WDBC);
            break;
        case EXPERIMENT_NB_NURSERY:
            nb_garb_off(GARBLER_DIR, num_len, num_classes, vector_size, domain_size, NB_NURSERY);
            break;
        case EXPERIMENT_NB_AUD:
            nb_garb_off(GARBLER_DIR, num_len, num_classes, vector_size, domain_size, NB_AUD);
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

        // TODO UPDATE THIS
        printf("Online evaluating\n");
        ChainedGarbledCircuit *cgcs;
        switch (args->type) {
        case EXPERIMENT_AES:
            cgcs = aes_circuits(10, args->chaining_type);
            break;
        case EXPERIMENT_CBC:
            cgcs = cbc_circuits(args->chaining_type);
            break;
        case EXPERIMENT_LEVEN:
            cgcs = leven_circuits(l, sigma);
            break;
        case EXPERIMENT_WDBC:
            cgcs = hyperplane_circuits(n, num_len);
            break;
        case EXPERIMENT_HP_CREDIT:
            cgcs = hyperplane_circuits(n, num_len);
            break;
        case EXPERIMENT_RANDOM_DT:
            cgcs = dt_circuits(n, num_len, DT_RANDOM);
            break;
        case EXPERIMENT_DT_NURSERY:
            cgcs = dt_circuits(n, num_len, DT_NURSERY);
            break;
        case EXPERIMENT_DT_ECG:
            cgcs = dt_circuits(n, num_len, DT_ECG);
            break;
        case EXPERIMENT_NB_WDBC:
            cgcs = nb_circuits(num_len, num_classes, vector_size, domain_size, NB_WDBC);
            break;
        case EXPERIMENT_NB_NURSERY:
            cgcs = nb_circuits(num_len, num_classes, vector_size, domain_size, NB_NURSERY);
            break;
        case EXPERIMENT_NB_AUD:
            cgcs = nb_circuits(num_len, num_classes, vector_size, domain_size, NB_AUD);
            break;
        case EXPERIMENT_HYPERPLANE:
            printf("EXPERIMENT_HYPERPLANE eval on\n");
            printf("Shouldn't be here -- deprecated\n");
            break;
        default:
            break;
        }
        eval_on(n_eval_inputs, n_eval_labels, ncircs, args->ntrials, args->chaining_type, cgcs);


    } else if (args->garb_full || args->eval_full) {
        garble_circuit gc;
        switch (args->type) {
        case EXPERIMENT_AES:
            buildAESCircuit(&gc);
            break;
        case EXPERIMENT_CBC: {
            block delta = garble_create_delta();
            buildCBCFullCircuit(&gc, NUM_CBC_BLOCKS, NUM_AES_ROUNDS, &delta);
            break;
        }
        case EXPERIMENT_LEVEN:
            buildLevenshteinCircuit(&gc, l, sigma);
            break;
        case EXPERIMENT_WDBC:
            buildLinearCircuit(&gc, n, num_len);
            break;
        case EXPERIMENT_HP_CREDIT:
            buildLinearCircuit(&gc, n, num_len);
            break;
        case EXPERIMENT_RANDOM_DT:
            buildLinearCircuit(&gc, n, num_len);
            break;
        case EXPERIMENT_DT_NURSERY:
            build_decision_tree_nursery_circuit(&gc, num_len);
            break;
        case EXPERIMENT_DT_ECG:
            build_decision_tree_ecg_circuit(&gc, num_len);
            break;
        case EXPERIMENT_NB_WDBC:
            build_naive_bayes_circuit(&gc, num_classes, vector_size, domain_size, num_len);
            break;
        case EXPERIMENT_NB_NURSERY:
            build_naive_bayes_circuit(&gc, num_classes, vector_size, domain_size, num_len);
            break;
        case EXPERIMENT_NB_AUD:
            build_naive_bayes_circuit(&gc, num_classes, vector_size, domain_size, num_len);
            break;
        case EXPERIMENT_HYPERPLANE:
            buildHyperCircuit(&gc);
            break;
        default:
            abort();
        }
        if (args->garb_full)
            garb_full(&gc, n_garb_inputs, n_eval_inputs, args->ntrials, l, sigma, args->type);
        else
            eval_full(&gc, n_garb_inputs, n_eval_inputs, noutputs, args->ntrials);
        garble_delete(&gc);
    } else {
        fprintf(stderr, "error: no role specified\n");
        usage(args->progname, EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
    int c, idx;
    struct args args;

    (void) garble_seed(NULL);

    args_init(&args, argv[0]);

    while ((c = getopt_long(argc, argv, "", opts, &idx)) != -1) {
        switch (c) {
        case 0:
            break;
            /* case 'c': */
            /*     if (strcmp(optarg, "STANDARD") == 0) { */
            /*         args.chaining_type = CHAINING_TYPE_STANDARD; */
            /*     } else if (strcmp(optarg, "SCMC") == 0) { */
            /*         args.chaining_type = CHAINING_TYPE_SIMD; */
            /*     } else { */
            /*         fprintf(stderr, "Unknown chaining type %s\n", optarg); */
            /*         exit(EXIT_FAILURE); */
            /*     } */
            /*     break; */
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
            } else if (strcmp(optarg, "NURSERY_DT") == 0) {
                args.type = EXPERIMENT_DT_NURSERY;
            } else if (strcmp(optarg, "ECG_DT") == 0) {
                args.type = EXPERIMENT_DT_ECG;
            } else if (strcmp(optarg, "WDBC_NB") == 0) {
                args.type = EXPERIMENT_NB_WDBC;
            } else if (strcmp(optarg, "NURSERY_NB") == 0) {
                args.type = EXPERIMENT_NB_NURSERY;
            } else if (strcmp(optarg, "AUD_NB") == 0) {
                args.type = EXPERIMENT_NB_AUD;
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
        case 'h':
        case '?':
            usage(argv[0], EXIT_SUCCESS);
            break;
        default:
            abort();
        }
    }
    return go(&args);
}
