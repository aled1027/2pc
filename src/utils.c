#include "utils.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <wmmintrin.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "state.h"

void convertToBinary(int x, int *arr, int narr)
{
    int i = 0;
    while (x > 0) {
        arr[i] = (x % 2);
        x >>= 1;
        i++;
    }
    for (int j = i; j < narr; j++)
        arr[j] = 0;
}

void
reverse_array(int *arr, size_t nints) 
{
    int *temp = allocate_ints(nints);
    for (int i = 0; i < nints; i++) {
        temp[i] = arr[nints-i-1];
    }
    memcpy(arr, temp, sizeof(int) * nints);
    free(temp);
}

uint64_t 
current_time_ns(void)
{
    struct timespec tp;
    int res;

    res = clock_gettime(CLOCK_MONOTONIC, &tp); /* mark start time */
    if (res == -1) {
        fprintf(stderr, "Failure with clock_gettime");
        return 0;
    }

    return 1000000000 * tp.tv_sec + tp.tv_nsec;
}

/* uint64_t */
/* nanoSecondsToMilliseconds(uint64_t nanoseconds) { */
/*     return nanoseconds / 1000000; */
/* } */

void *
ot_malloc(size_t size)
{
    return _mm_malloc(size, 16);
}

void
ot_free(void *p)
{
    return _mm_free(p);
}


void
arrayPopulateRange(int *arr, int start, int end) 
{
    /* exclusive on end */
    int i = 0;
    for (int j = start; j < end; i++, j++) {
        arr[i] = j;
    }
}

int *
allocate_ints(size_t nints)
{
    int *ret = calloc(nints, sizeof(int));
    if (ret == NULL) {
        perror("allocate_ints");
        exit(EXIT_FAILURE);
    }
    return ret;
}

size_t 
filesize(char *filename) 
{
    /* returns size of file in bytes */
	struct stat st;

	if (stat(filename, &st) == 0)
		return st.st_size;

	return -1;
}

int 
writeBufferToFile(char* buffer, size_t buf_size, char* fileName)
{
    FILE *f;
    f = fopen(fileName, "w");
    if (f == NULL) {
        printf("Write: Error in opening file %s.\n", fileName);
        return FAILURE;
    }
    (void) fwrite(buffer, sizeof(char), buf_size, f);
    fclose(f);
    return SUCCESS;
}

int
readFileIntoBuffer(char* buffer, char* fileName) 
{
    /* assume buffer is already malloced */
    assert(buffer);
    FILE *f = fopen(fileName, "r");
    long fs = filesize(fileName);

    if (f == NULL) {
        printf("Write: Error in opening file %s.\n", fileName);
        return FAILURE;
    }
    fread(buffer, sizeof(char), fs, f);
    fclose(f);
    return SUCCESS;
}

void
debug(char *s)
{
    fprintf(stderr, "DEBUG: %s\n", s);
    fflush(stderr);
}

block
our_encrypt(block *bl, block* key)
{
    /* encrypt bl using key. output is put back into bl */
    block ret;
    ret = xorBlocks(*bl, *key);
    return ret;
    
    // Ideally what it would look like:
    //AES_KEY key;
    //AES_set_encrypt_key((unsigned char *) bl, 128, key);
    //AES_ecb_encrypt_blks(bl, 1, key);
}

block
our_decrypt(block *bl, block* key)
{
    /* encrypt bl using key. output is put back into bl */
    block ret;
    ret = xorBlocks(*bl, *key);
    return ret;
    
    // Ideally what it would look like:
    //AES_KEY key;
    //AES_set_encrypt_key((unsigned char *) bl, 128, key);
    //AES_ecb_encrypt_blks(bl, 1, key);
}

void print_gate(Gate *g) 
{
    printf("Type: %d, inputs %d %d output: %d\n",
            g->type,
            g->input0,
            g->input0,
            g->output);
}

void print_table(GarbledTable *gt)
{
    FILE* fp = stdout;
    print_block(fp, gt->table[0]);
    printf(" ");
    print_block(fp, gt->table[1]);
    printf(" ");
    print_block(fp, gt->table[2]);
    printf("\n");
}

void print_wire(Wire *w)
{
    FILE* fp = stdout;
    print_block(fp, w->label0);
    printf(" ");
    print_block(fp, w->label1);
    printf("\n");
}

void
print_gc(GarbledCircuit *gc)
{
    //for (int i = 0; i < gc->q; i++) {
    //    print_gate(&gc->gates[i]);
    //    print_table(&gc->garbledTable[i]);
    //}
    for (int i = 0; i < 80; i++) {
        print_wire(&gc->wires[i]);
    }

}

