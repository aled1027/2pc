
#include <malloc.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>  /* log2 */

#include "crypto.h"
#include "utils.h"
#include "2pc_common.h"
#include "circuits.h"
//#include "gates.h"

#include "2pc_garbled_circuit.h"

void 
createSIMDInputLabelsWithR(ChainedGarbledCircuit *cgc, block R)
{
    block hashBlock;
    //cgc->inputSIMDBlock = randomBlock();
    SimdInformation *si = &cgc->simd_info;

    si->num_iblocks = 1;
    si->input_blocks = garble_allocate_blocks(si->num_iblocks);
    si->input_blocks[0] = garble_random_block();

    assert(cgc->gc.n > 0);
    si->iblock_map = malloc(cgc->gc.n * sizeof(int));

    int n = cgc->gc.n;
    for (int i = 0; i < n; i++) {
        si->iblock_map[i] = 0;
        hashBlock = garble_zero_block();
        sha1_hash((char *) &hashBlock, sizeof(block), i, 
            (unsigned char *) &i, sizeof i);

        cgc->inputLabels[2*i] = garble_xor(
                //cgc->inputSIMDBlock, 
                si->input_blocks[0], 
                hashBlock);
		cgc->inputLabels[2*i + 1] = garble_xor(R, cgc->inputLabels[2*i]);
    }
}

void 
createSIMDInputLabelsWithRForLeven(ChainedGarbledCircuit *cgc, block R, int l)
{

    int d_int_size = (int) floor(log2(l)) + 1;
    SimdInformation *si  = &cgc->simd_info;

    si->num_iblocks = 3;
    si->input_blocks = garble_allocate_blocks(si->num_iblocks);

    assert(cgc->gc.n > 0);
    si->iblock_map = malloc(cgc->gc.n * sizeof(int));

    int idx = 0;
    for (int i = 0; i < si->num_iblocks; i++) {
        si->input_blocks[i] = garble_random_block();
        for (int j = 0; j < d_int_size; j++) {
            si->iblock_map[idx] = i;
            block hashBlock = garble_zero_block();
            sha1_hash((char *) &hashBlock, sizeof(block), j, 
                (unsigned char *) &j, sizeof j);

            cgc->inputLabels[2*idx] = garble_xor(
                    si->input_blocks[i], 
                    hashBlock);
		    cgc->inputLabels[2*idx + 1] = garble_xor(R, cgc->inputLabels[2*idx]);
            ++idx;
        }
    }

    for (; idx < cgc->gc.n; idx++) {
        cgc->inputLabels[2*idx] = garble_random_block();
        cgc->inputLabels[2*idx + 1] = garble_xor(R, cgc->inputLabels[2*idx]);
        si->iblock_map[idx] = 0;
    }
}

int 
generateOfflineChainingOffsets(ChainedGarbledCircuit *cgc)
{
    block hashBlock;
    int m = cgc->gc.m;

    cgc->offlineChainingOffsets = garble_allocate_blocks(m);
    cgc->simd_info.output_block = garble_random_block();

    for (int i = 0; i < m; ++i) {
        /* TODO check with Ask Alex M to see about hash function */
        hashBlock = garble_zero_block();

        sha1_hash((char *) &hashBlock, sizeof(block), i, 
                (unsigned char *) &i, sizeof i);

        cgc->offlineChainingOffsets[i] = garble_xor(
                garble_xor(cgc->simd_info.output_block, cgc->outputMap[2*i]),
                hashBlock);
    }
    return 0;
}

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc, bool isGarb, ChainingType chainingType) 
{
    /* TODO will need to remove offlineChainingOffsets */
    garble_delete(&chained_gc->gc); // frees memory in gc
    if (isGarb) {
        free(chained_gc->inputLabels);
        free(chained_gc->outputMap);
    }

    if (chainingType == CHAINING_TYPE_SIMD) {
        free(chained_gc->offlineChainingOffsets);
        if (isGarb) {
            free(chained_gc->simd_info.input_blocks);
            free(chained_gc->simd_info.iblock_map);
        }
    }

    return 0;
}

int
saveChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, bool isGarbler,
              ChainingType chainingType)
{
    char fname[50];
    FILE *f;
    garble_circuit *gc = &chained_gc->gc;

    sprintf(fname, "%s/chained_gc_%d", dir, chained_gc->id); /* XXX: Security hole */
    if ((f = fopen(fname, "w")) == NULL) {
        return FAILURE;
    }
    garble_save(gc, f, isGarbler);

    /* ChainedGarbleCircuit fields */
    fwrite(&chained_gc->id, sizeof(int), 1, f);
    fwrite(&chained_gc->type, sizeof(CircuitType), 1, f);
    printf("Saving circuit of type %d\n", chained_gc->type);
    if (isGarbler) {
        fwrite(chained_gc->inputLabels, sizeof(block), 2 * gc->n, f);
        fwrite(chained_gc->outputMap, sizeof(block), 2 * gc->m, f);
        if (chainingType == CHAINING_TYPE_SIMD) {
            fwrite(&chained_gc->simd_info.output_block, sizeof(block), 1, f);
            fwrite(&chained_gc->simd_info.num_iblocks, sizeof(int), 1, f);
            fwrite(chained_gc->simd_info.input_blocks, 
                    sizeof(block), chained_gc->simd_info.num_iblocks, f);
            fwrite(chained_gc->simd_info.iblock_map, sizeof(int), chained_gc->gc.n, f);
        }
        printf("inside of here\n");
    }

    if (!isGarbler && chainingType == CHAINING_TYPE_SIMD) {
        fwrite(chained_gc->offlineChainingOffsets, sizeof(block), gc->m, f);
    }
    fclose(f);
    return SUCCESS;
}

int
loadChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, int id,
              bool isGarbler, ChainingType chainingType)
{
    char fname[50];
    FILE *f;
    garble_circuit *gc = &chained_gc->gc;

    sprintf(fname, "%s/chained_gc_%d", dir, id); /* XXX: security hole */
    printf("about to load %s\n", fname);
    if ((f = fopen(fname, "r")) == NULL) {
        perror("fopen");
        return FAILURE;
    }
    garble_load(gc, f, isGarbler);

    fread(&chained_gc->id, sizeof(int), 1, f);
    fread(&chained_gc->type, sizeof(CircuitType), 1, f);
    if (isGarbler) {
        chained_gc->inputLabels = garble_allocate_blocks(2 * gc->n);
        chained_gc->outputMap = garble_allocate_blocks(2 * gc->m);
        fread(chained_gc->inputLabels, sizeof(block), 2 * gc->n, f);
        fread(chained_gc->outputMap, sizeof(block), 2 * gc->m, f);

        if (chainingType == CHAINING_TYPE_SIMD) {
            fread(&chained_gc->simd_info.output_block, sizeof(block), 1, f);
            fread(&chained_gc->simd_info.num_iblocks, sizeof(int), 1, f);
            chained_gc->simd_info.input_blocks = garble_allocate_blocks(chained_gc->simd_info.num_iblocks);
            fread(chained_gc->simd_info.input_blocks, sizeof(block),
                    chained_gc->simd_info.num_iblocks, f);
            chained_gc->simd_info.iblock_map = allocate_ints(chained_gc->gc.n);
            fread(chained_gc->simd_info.iblock_map, sizeof(int), chained_gc->gc.n, f);
        }
    }

    if (!isGarbler && chainingType == CHAINING_TYPE_SIMD) {
        chained_gc->offlineChainingOffsets = garble_allocate_blocks(gc->m);
        fread(chained_gc->offlineChainingOffsets, sizeof(block), gc->m, f);
    }
    fclose(f);
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


