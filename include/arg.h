#ifndef ARG_H
#define ARG_H

typedef enum { GENERATOR, EVALUATOR } party_type;

typedef struct arg_t_ {
    int debug;
    int verbose;

    party_type type;

    char *circuit;
} arg_t;

void
arg_defaults(arg_t *arg);

#endif
