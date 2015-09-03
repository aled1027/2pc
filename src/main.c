#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "arg.h"
#include "gc_comm.h"
#include "net.h"

static int
run_generator(arg_t *args)
{
    int sockfd, fd;
    GarbledCircuit gc;
    block *inputLabels, *extractedLabels;
    block *outputMap;
    int *inputs;

    sockfd = net_init_server("127.0.0.1", "8000");
    fd = net_server_accept(sockfd);

    (void) readCircuitFromFile(&gc, args->circuit);
    printf("n = %d\n", gc.n);
    printf("m = %d\n", gc.m);
    printf("q = %d\n", gc.q);
    printf("r = %d\n", gc.r);

    inputLabels = (block *) memalign(128, sizeof(block) * 2 * gc.n);
    extractedLabels = (block *) memalign(128, sizeof(block) * gc.n);
    outputMap = (block *) memalign(128, sizeof(block) * 2 * gc.m);
    inputs = (int *) malloc(sizeof(int) * gc.n);

    for (int i = 0; i < gc.n; i++) {
		inputs[i] = rand() % 2;
	}
    
    (void) garbleCircuit(&gc, inputLabels, outputMap);
    (void) extractLabels(extractedLabels, inputLabels, inputs, gc.n);

    (void) gc_comm_send(fd, &gc);
    (void) send(sockfd, extractedLabels, sizeof(block) * gc.n, 0);

    return 0;
}

static int
run_evaluator(arg_t *args)
{
    int sockfd;
    GarbledCircuit gc;
    block *input, *output;

    sockfd = net_init_client("127.0.01", "8000");

    (void) gc_comm_recv(sockfd, &gc);

    input = (block *) memalign(128, sizeof(block) * gc.n);
    output = (block *) memalign(128, sizeof(block) * gc.m);

    (void) recv(sockfd, input, sizeof(block) * gc.n, 0);
    (void) recv(sockfd, output, sizeof(block) * gc.m, 0);

    (void) evaluate(&gc, input, output);
    for (int i = 0; i < gc.m; ++i) {
        uint16_t *val = (uint16_t *) &output[i];
        printf("%u", val[0]);
    }
    printf("\n");

    free(input);
    free(output);

    return 0;
}

int
main(int argc, char *argv[])
{
    arg_t args;
    int c;

    arg_defaults(&args);

    opterr = 0;

    while ((c = getopt(argc, argv, "abc:")) != -1) {
        switch (c) {
        case 'a':
            args.type = GENERATOR;
            break;
        case 'b':
            args.type = EVALUATOR;
            break;
        case 'c':
            args.circuit = optarg;
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
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
