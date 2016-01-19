#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include "justGarble.h"

#define SUCCESS 0
#define FAILURE -1

#define MIN(a, b)                               \
    (a) < (b) ? (a) : (b)
#define MAX(a, b)                               \
    (a) > (b) ? (a) : (b)

#define MIN3(a, b, c) \
    ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

void 
convertToBinary(int x, int *arr, int narr);

void
reverse_array(int *arr, size_t nints);

double
current_time(void);

void *
ot_malloc(size_t size);

void
ot_free(void *p);

void
arrayPopulateRange(int *arr, int start, int end);

int *
allocate_ints(size_t nints);

long 
filesize(const char *filename);

int 
writeBufferToFile(char* buffer, size_t buf_size, char* fileName);

int
readFileIntoBuffer(char* buffer, char* fileName);

void
debug(char *s);

#endif
