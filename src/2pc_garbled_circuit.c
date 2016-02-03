
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

int 
generateOfflineChainingOffsets(ChainedGarbledCircuit *cgc)
{
    block rand, hashBlock;
    int m;

    m = cgc->gc.m;
    cgc->offlineChainingOffsets = allocate_blocks(m);
    rand = randomBlock();

    for (int i = 0; i < m; ++i) {
        /* check with Ask Alex M to see about hash function */
        hashBlock = zero_block();
        sha1_hash((char *) &hashBlock, sizeof(block), i, 
                (unsigned char *) &i, sizeof i);

        cgc->offlineChainingOffsets[i] = xorBlocks(
                xorBlocks(rand, cgc->outputMap[2*i]),
                hashBlock);
    }
    return 0;
}

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc, bool isGarb) 
{
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    if (isGarb) {
        free(chained_gc->inputLabels);
        free(chained_gc->outputMap);
    }
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
    saveGarbledCircuit(&chained_gc->gc, f);
    fwrite(&chained_gc->id, sizeof(int), 1, f);
    fwrite(&chained_gc->type, sizeof(CircuitType), 1, f);
    if (isGarbler) {
        fwrite(chained_gc->inputLabels, sizeof(block), 2 * gc->n, f);
        fwrite(chained_gc->outputMap, sizeof(block), 2 * gc->m, f);
    }
    //} else {
    //    fwrite(chained_gc->offlineChainingOffsets, sizeof(block), gc->m, f);
    //}
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
    }
    //} else {
    //    chained_gc->offlineChainingOffsets = allocate_blocks(gc->m);
    //    fread(chained_gc->offlineChainingOffsets, sizeof(block), gc->m, f);
    //}
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
    int *buf;

    buf = malloc(filesize(fname));
    if (readFileIntoBuffer((char *) buf, fname) == FAILURE) {
        free(buf);
        return NULL;
    }
    return buf;
}
