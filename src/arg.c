#include "arg.h"

#include <stdio.h>

void
arg_defaults(arg_t *arg)
{
    arg->debug = 0;
    arg->verbose = 0;
    arg->type = GENERATOR;
    arg->circuit = NULL;
}
