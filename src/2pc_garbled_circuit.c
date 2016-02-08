
#include <malloc.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>  /* log2 */

#include "crypto.h"
#include "utils.h"
#include "2pc_common.h"
#include "circuits.h"
#include "gates.h"

#include "2pc_garbled_circuit.h"

void 
createSIMDInputLabelsWithR(ChainedGarbledCircuit *cgc, block R)
{
    block hashBlock;
    cgc->inputSIMDBlock = randomBlock();

    int n = cgc->gc.n;
    for (int i = 0; i < n; i++) {
        hashBlock = zero_block();
        sha1_hash((char *) &hashBlock, sizeof(block), i, 
            (unsigned char *) &i, sizeof i);

        cgc->inputLabels[2*i] = xorBlocks(
                cgc->inputSIMDBlock, 
                hashBlock);
		cgc->inputLabels[2*i + 1] = xorBlocks(R, cgc->inputLabels[i]);
    }
}

    int 
generateOfflineChainingOffsets(ChainedGarbledCircuit *cgc)
{
    block hashBlock;
    int m = cgc->gc.m;

    cgc->offlineChainingOffsets = allocate_blocks(m);
    cgc->outputSIMDBlock = randomBlock();

    for (int i = 0; i < m; ++i) {
        /* TODO check with Ask Alex M to see about hash function */
        hashBlock = zero_block();
        sha1_hash((char *) &hashBlock, sizeof(block), i, 
                (unsigned char *) &i, sizeof i);

        cgc->offlineChainingOffsets[i] = xorBlocks(
                xorBlocks(cgc->outputSIMDBlock, cgc->outputMap[2*i]),
                hashBlock);
        if (i == 0)
            print_block(cgc->offlineChainingOffsets[0]);

    }
    return 0;
}

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc, bool isGarb, ChainingType chainingType) 
{
    /* TODO will need to remove offlineChainingOffsets */
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    if (isGarb) {
        free(chained_gc->inputLabels);
        free(chained_gc->outputMap);
    }
    if (chainingType == CHAINING_TYPE_SIMD)
        free(chained_gc->offlineChainingOffsets);
    return 0;
}

int
saveChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, bool isGarbler,
              ChainingType chainingType)
{
    char fname[50];
    FILE *f;
    GarbledCircuit *gc = &chained_gc->gc;

    sprintf(fname, "%s/chained_gc_%d", dir, chained_gc->id); /* XXX: Security hole */
    if ((f = fopen(fname, "w")) == NULL) {
        return FAILURE;
    }
    saveGarbledCircuit(&chained_gc->gc, f); /* TODO is this suppoed to be false? no wires? */
    fwrite(&chained_gc->id, sizeof(int), 1, f);
    fwrite(&chained_gc->type, sizeof(CircuitType), 1, f);
    if (isGarbler) {
        fwrite(chained_gc->inputLabels, sizeof(block), 2 * gc->n, f);
        fwrite(chained_gc->outputMap, sizeof(block), 2 * gc->m, f);
        if (chainingType == CHAINING_TYPE_SIMD) {
            fwrite(&chained_gc->outputSIMDBlock, sizeof(block), 1, f);
            fwrite(&chained_gc->inputSIMDBlock, sizeof(block), 1, f);
        }

    }
    
    printf("in save\n");
    if (!isGarbler && chainingType == CHAINING_TYPE_SIMD) {
        print_block(chained_gc->offlineChainingOffsets[0]);
        fwrite(chained_gc->offlineChainingOffsets, sizeof(block), gc->m, f);
    }
    return SUCCESS;
}

int
loadChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, int id,
              bool isGarbler, ChainingType chainingType)
{
    char fname[50];
    FILE *f;
    GarbledCircuit *gc = &chained_gc->gc;

    sprintf(fname, "%s/chained_gc_%d", dir, id); /* XXX: security hole */
    if ((f = fopen(fname, "r")) == NULL) {
        perror("fopen");
        return FAILURE;
    }
    loadGarbledCircuit(gc, f, isGarbler);
    fread(&chained_gc->id, sizeof(int), 1, f);
    fread(&chained_gc->type, sizeof(CircuitType), 1, f);
    if (isGarbler) {
        chained_gc->inputLabels = allocate_blocks(2 * gc->n);
        chained_gc->outputMap = allocate_blocks(2 * gc->m);
        fread(chained_gc->inputLabels, sizeof(block), 2 * gc->n, f);
        fread(chained_gc->outputMap, sizeof(block), 2 * gc->m, f);

        if (chainingType == CHAINING_TYPE_SIMD) {
            fread(&chained_gc->outputSIMDBlock, sizeof(block), 1, f);
            fread(&chained_gc->inputSIMDBlock, sizeof(block), 1, f);
        }
    }

    if (!isGarbler && chainingType == CHAINING_TYPE_SIMD) {
        chained_gc->offlineChainingOffsets = allocate_blocks(gc->m);
        fread(chained_gc->offlineChainingOffsets, sizeof(block), gc->m, f);
    }
    
    return SUCCESS;
}

int 
saveOutputMap(char *fname, block *labels, int nlabels)
{
    size_t size = sizeof(block) * nlabels;
    if (writeBufferToFile((char *) labels, size, fname) == FAILURE)
        return FAILURE;
    return SUCCESS;
}

int
saveOTLabels(char *fname, block *labels, int n, bool isSender)
{
    size_t size = sizeof(block) * (isSender ?  2 : 1) * n;

    if (writeBufferToFile((char *) labels, size, fname) == FAILURE)
        return FAILURE;

    return SUCCESS;
}

int
loadOutputMap(char *fname, block* labels)
{
    labels = NULL;
    (void) posix_memalign((void **) &labels, 128, filesize(fname));
    printf("filesize(fname): %zu\n", filesize(fname));
    if (readFileIntoBuffer((char *) labels, fname) == FAILURE) {
        free(labels);
        assert(false);
        return FAILURE;
    }
    return SUCCESS;
}

block *
loadOTLabels(char *fname)
{
    block *buf = NULL;
    (void) posix_memalign((void **) &buf, 128, filesize(fname));
    if (readFileIntoBuffer((char *) buf, fname) == FAILURE) {
        free(buf);
        return NULL;
    }
    return buf;
}

    
int
saveOTSelections(char *fname, int *selections, int n)
{
    size_t size = sizeof(int) / sizeof(char) * n;
    if (writeBufferToFile((char *) selections, size, fname) == FAILURE)
        return FAILURE;

    return SUCCESS;
}

int *
loadOTSelections(char *fname)
{
    int *buf = malloc(filesize(fname));
    if (readFileIntoBuffer((char *) buf, fname) == FAILURE)  {
        free(buf);
        return NULL;
    }
    return buf;
}


