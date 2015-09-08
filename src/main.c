#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "arg.h"
#include "gc_comm.h"
#include "net.h"

static void
print_garbled_gate(GarbledGate *gg)
{
    printf("%ld %ld %ld %d %d\n", gg->input0, gg->input1, gg->output, gg->id,
           gg->type);
}

static void
print_garbled_table(GarbledTable *gt)
{
    print_block(gt->table[0]);
    printf(" ");
    print_block(gt->table[1]);
    printf(" ");
    print_block(gt->table[2]);
    printf(" ");
    print_block(gt->table[3]);
    printf("\n");
}

static void
print_wire(Wire *w)
{
    printf("%ld %ld ", w->value, w->id);
    print_block(w->label);
    printf(" ");
    print_block(w->label0);
    printf(" ");
    print_block(w->label1);
    printf("\n");
}

static void
print_gc(GarbledCircuit *gc)
{
    printf("n = %d\n", gc->n);
    printf("m = %d\n", gc->m);
    printf("q = %d\n", gc->q);
    printf("r = %d\n", gc->r);
    for (int i = 0; i < gc->q; ++i) {
        print_garbled_gate(&gc->garbledGates[i]);
        print_garbled_table(&gc->garbledTable[i]);
    }
    for (int i = 0; i < gc->r; ++i) {
        print_wire(&gc->wires[i]);
    }
    for (int i = 0; i < gc->m; ++i) {
        printf("%d\n", gc->outputs[i]);
    }
}

static void
print_blocks(const char *str, block *blks, int length)
{
    for (int i = 0; i < length; ++i) {
        printf("%s ", str);
        print_block(blks[i]);
        printf("\n");
    }
}

static int
checkfn(int *a, int *outputs, int n)
{
	outputs[0] = a[0];
	int i = 0;
	for (i = 0; i < n - 1; i++) {
		if (i % 3 == 2)
			outputs[0] = outputs[0] | a[i + 1];
		if (i % 3 == 1)
			outputs[0] = outputs[0] & a[i + 1];
		if (i % 3 == 0)
			outputs[0] = outputs[0] ^ a[i + 1];
	}
	return outputs[0];
}

static void
buildCircuit(GarbledCircuit *gc)
{
	srand(time(NULL));
	GarblingContext gctxt;

	int roundLimit = 10;
	int n = 128 * (roundLimit + 1);
	int m = 128;
	int q = 50000; //Just an upper bound
	int r = 50000;
	int inp[n];
	countToN(inp, n);
	int addKeyInputs[n * (roundLimit + 1)];
	int addKeyOutputs[n];
	int subBytesOutputs[n];
	int shiftRowsOutputs[n];
	int mixColumnOutputs[n];
	block labels[2 * n];
	block outputbs[m];
    int outputs[1];

    /* int n = 8; */
	/* int m = 1; */
	/* int q = n; */
	/* int r = q+n; */

	/* //Setup input and output tokens/labels. */
	/* block *labels = (block*) malloc(sizeof(block) * 2 * n); */
	/* block *exlabels = (block*) malloc(sizeof(block) * n); */
	/* block *outputbs2 = (block*) malloc(sizeof(block) * m); */
	/* block *outputbs = (block*) malloc(sizeof(block) * m); */
	/* int *inp = (int *) malloc(sizeof(int) * n); */
	/* countToN(inp, n); */
    /* int *outputs = (int *) malloc(sizeof(int) * m); */


	/* createInputLabels(labels, n); */
	/* createEmptyGarbledCircuit(gc, n, m, q, r, labels); */
	/* startBuilding(gc, &gctxt); */
	/* MIXEDCircuit(gc, &gctxt, n, inp, outputs); */
	/* finishBuilding(gc, &gctxt, outputbs, outputs); */
    
	createInputLabels(labels, n);
	createEmptyGarbledCircuit(gc, n, m, q, r, labels);
	startBuilding(gc, &gctxt);

	countToN(addKeyInputs, 256);

	for (int round = 0; round < roundLimit; round++) {

		AddRoundKey(gc, &gctxt, addKeyInputs, addKeyOutputs);

		for (int i = 0; i < 16; i++) {
			SubBytes(gc, &gctxt, addKeyOutputs + 8 * i, subBytesOutputs + 8 * i);
		}

		ShiftRows(gc, &gctxt, subBytesOutputs, shiftRowsOutputs);

		for (int i = 0; i < 4; i++) {
			if (round == roundLimit - 1)
				MixColumns(gc, &gctxt,
                           shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
		}
		for (int i = 0; i < 128; i++) {
			addKeyInputs[i] = mixColumnOutputs[i];
			addKeyInputs[i + 128] = (round + 2) * 128 + i;
		}
	}

	finishBuilding(gc, &gctxt, outputbs, mixColumnOutputs);
}

static int
run_generator(arg_t *args)
{
    int sockfd, fd, res;
    GarbledCircuit gc;
    block *inputLabels, *extractedLabels;
    block *outputMap;
    int *inputs;

    /* sockfd = net_init_server("127.0.0.1", "8000"); */
    /* fd = net_server_accept(sockfd); */

    /* buildCircuit(&gc); */
    (void) readCircuitFromFile(&gc, "aesCircuit");

    inputLabels = (block *) memalign(128, sizeof(block) * 2 * gc.n);
    extractedLabels = (block *) memalign(128, sizeof(block) * gc.n);
    outputMap = (block *) memalign(128, sizeof(block) * 2 * gc.m);

    inputs = (int *) malloc(sizeof(int) * gc.n);
    for (int i = 0; i < gc.n; ++i) {
		inputs[i] = rand() % 2;
	}
    
    (void) garbleCircuit(&gc, inputLabels, outputMap);
    print_gc(&gc);
    (void) extractLabels(extractedLabels, inputLabels, inputs, gc.n);

    /* (void) gc_comm_send(fd, &gc); */
    /* net_send(fd, extractedLabels, sizeof(block) * gc.n, 0); */
    /* net_send(fd, outputMap, sizeof(block) * 2 * gc.m, 0); */

    {
        int *outputVals = (int *) malloc(sizeof(int) * gc.m);
        block *computedOutputMap = (block *) memalign(128, sizeof(block) * gc.m);
        evaluate(&gc, extractedLabels, computedOutputMap);
        print_blocks("computedOutputMap", computedOutputMap, gc.m);
        if (mapOutputs(outputMap, computedOutputMap, outputVals, gc.m) == FAILURE) {
            fprintf(stderr, "mapOutputs failed!\n");
            return -1;
        }

        printf("Output: ");
        for (int i = 0; i < gc.m; ++i) {
            printf("%d", outputVals[i]);
        }
        printf("\n");

    }


    checkCircuit(&gc, inputLabels, outputMap, &checkfn);

    /* close(fd); */
    /* close(sockfd); */
    return 0;
}

static int
run_evaluator(arg_t *args)
{
    int sockfd, len;
    GarbledCircuit gc;
    block *inputLabels, *outputMap, *computedOutputMap;
    int *outputVals;

    sockfd = net_init_client("127.0.01", "8000");

    (void) gc_comm_recv(sockfd, &gc);

    inputLabels = (block *) memalign(128, sizeof(block) * gc.n);
    outputMap = (block *) memalign(128, sizeof(block) * 2 * gc.m);
    computedOutputMap = (block *) memalign(128, sizeof(block) * gc.m);
    outputVals = (int *) malloc(sizeof(int) * gc.m);

    net_recv(sockfd, inputLabels, sizeof(block) * gc.n, 0);
    net_recv(sockfd, outputMap, sizeof(block) * 2 * gc.m, 0);

    evaluate(&gc, inputLabels, computedOutputMap);
    if (mapOutputs(outputMap, computedOutputMap, outputVals, gc.m) == FAILURE) {
        fprintf(stderr, "mapOutputs failed!\n");
        return -1;
    }

    printf("Output: ");
    for (int i = 0; i < gc.m; ++i) {
        printf("%d", outputVals[i]);
    }
    printf("\n");

    free(inputLabels);
    free(outputMap);
    free(outputVals);

    close(sockfd);

    return 0;
}

int
main(int argc, char *argv[])
{
    arg_t args;
    int c;

    arg_defaults(&args);

    opterr = 0;

    while ((c = getopt(argc, argv, "ab")) != -1) {
        switch (c) {
        case 'a':
            args.type = GENERATOR;
            break;
        case 'b':
            args.type = EVALUATOR;
            break;
        /* case 'c': */
        /*     args.circuit = optarg; */
        /*     break; */
        case '?':
            /* if (optopt == 'c') */
            /*     fprintf(stderr, "Option -%c requires an argument.\n", optopt); */
            if (isprint (optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return EXIT_FAILURE;
        default:
            break;
        }
    }

    switch (args.type) {
    case GENERATOR:
        run_generator(&args);
        break;
    case EVALUATOR:
        run_evaluator(&args);
        break;
    default:
        assert(0);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
