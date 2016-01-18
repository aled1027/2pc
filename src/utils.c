#include "utils.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <wmmintrin.h>
#include <sys/stat.h>
#include <assert.h>

#include "state.h"

double
current_time(void)
{
    struct timeval t;
    (void) gettimeofday(&t, NULL);
    return (double) (t.tv_sec + (double) (t.tv_usec / 1000000.0));
}

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

long 
filesize(const char *filename) {
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
        printf("Write: Error in opening file.\n");
        return FAILURE;
    }
    (void) fwrite(buffer, sizeof(char), buf_size, f);
    fclose(f);
    return SUCCESS;
}

int
readFileIntoBuffer(char* buffer, char* fileName) {
    /* assume buffer is already malloced */
    assert(buffer);
    FILE *f = fopen(fileName, "r");
    long fs = filesize(fileName);

    if (f == NULL) {
        printf("Write: Error in opening file.\n");
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
