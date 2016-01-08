#include "crypto.h"

#include <assert.h>
#include <openssl/sha.h>
#include <string.h>

#include <wmmintrin.h>

#include "utils.h"

/*
 * Implementation (heavily) inspired by computePermutation() function found at
 * http://daimi.au.dk/~jot2re/cuda/resources/code2.zip : src/OT/protocols.c,
 * which implements Knuth's "Algorithm P".
 */
int
random_permutation(unsigned int *array, unsigned int size,
                   unsigned int *sorted, unsigned int seed)
{
    unsigned int *tmp;

    tmp = (unsigned int *) malloc(sizeof(unsigned int) * size);
    if (tmp == NULL)
        return FAILURE;

    srand(seed);

    /* Initialize identity permutation */
    for (unsigned int i = 0; i < size; ++i) {
        tmp[i] = i;
    }

    /* Permute the array */
    for (unsigned int i = size - 1; i >= 1; --i) {
        unsigned int j, tmpi, tmpj;

        j = (unsigned int) rand() % (i + 1);
        tmpi = tmp[i];
        tmpj = tmp[j];
        tmp[i] = tmpj;
        tmp[j] = tmpi;
    }

    /* Select pairs */
    for (unsigned int i = 0; i < size; i += 2) {
        array[tmp[i]] = tmp[i + 1];
        array[tmp[i + 1]] = tmp[i];
        if (sorted) {
            if (tmp[i] < tmp[i + 1]) {
                sorted[i / 2] = tmp[i];
            } else {
                sorted[i / 2] = tmp[i + 1];
            }
        }
    }

    free(tmp);

    return SUCCESS;
}

void
sha1_hash(char *out, size_t outlen, int counter,
          const unsigned char *in, size_t inlen)
{
    unsigned int idx = 0;
    unsigned int length = 0;
    unsigned char hash[SHA_DIGEST_LENGTH];

    while (length < outlen) {
        SHA_CTX c;
        int n;

        (void) SHA1_Init(&c);
        (void) SHA1_Update(&c, &counter, sizeof counter);
        (void) SHA1_Update(&c, &idx, sizeof idx);
        (void) SHA1_Update(&c, in, inlen);
        (void) SHA1_Final(hash, &c);
        n = MIN(outlen - length, sizeof hash);
        (void) memcpy(out + length, hash, n);
        length += n;
        idx++;
    }
}

void
xorarray(unsigned char *a, const size_t alen,
         const unsigned char *b, const size_t blen)
{
    unsigned char tmp[16];

    assert(alen >= blen);
    for (size_t i = 0; i < blen; i += 16) {
        __m128i am, bm;
        int length = blen - i < 16 ? blen - i : 16;
        
        am = _mm_loadu_si128((__m128i *) (a + i));
        bm = _mm_loadu_si128((__m128i *) (b + i));
        am = _mm_xor_si128(am, bm);
        // _mm_store_si128((__m128i *) (a + i), am);
        _mm_store_si128((__m128i *) tmp, am);
        (void) memcpy(a + i, tmp, length);
    }
    // for (size_t i = 0; i < blen; ++i) {
    //     a[i] ^= b[i];
    // }
}

