#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define SUCCESS 0
#define FAILURE -1

#define MIN(a, b)                               \
    (a) < (b) ? (a) : (b)
#define MAX(a, b)                               \
    (a) > (b) ? (a) : (b)

double
current_time(void);

void *
ot_malloc(size_t size);

void
ot_free(void *p);

#endif
