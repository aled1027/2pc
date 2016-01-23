#include "2pc_garbled_circuit.h"

#include <malloc.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>  /* log2 */

#include "utils.h"
#include "2pc_common.h"
#include "circuits.h"
#include "gates.h"

void
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc) 
{
    removeGarbledCircuit(&chained_gc->gc);
    if (chained_gc->inputLabels)
        free(chained_gc->inputLabels);
    if (chained_gc->outputMap)
        free(chained_gc->outputMap);
}

int 
saveChainedGC(ChainedGarbledCircuit* chained_gc, const char *dir, bool isGarbler)
{
    char* buffer;
    size_t p, buffer_size = 0;
    char fileName[50];
    int res;

    GarbledCircuit* gc = &chained_gc->gc;

    buffer_size = garbledCircuitSize(gc);
    /* Add in ChainedGC info */
    buffer_size += sizeof(int);
    buffer_size += sizeof(CircuitType); 
    if (isGarbler) {
        buffer_size += sizeof(block) * 2 * gc->n;
        buffer_size += sizeof(block) * 2 * gc->m;
    }

    buffer = calloc(buffer_size, sizeof(char));

    p = copyGarbledCircuitToBuffer(gc, buffer);
    
    // id, type, inputLabels, outputmap
    memcpy(buffer + p, &chained_gc->id, sizeof(int));
    p += sizeof(int);
    memcpy(buffer + p, &chained_gc->type, sizeof(CircuitType));
    p += sizeof(CircuitType);
    if (isGarbler) {
        memcpy(buffer + p, chained_gc->inputLabels, sizeof(block)*2*gc->n);
        p += sizeof(block) * 2 * gc->n;
        memcpy(buffer + p, chained_gc->outputMap, sizeof(block)*2*gc->m);
        p += sizeof(block) * 2 * gc->m;
    }

    if (isGarbler) {
        sprintf(fileName, "%s/chained_gc_%d", dir, chained_gc->id); /* XXX: Security hole */
    } else {
        sprintf(fileName, "%s/chained_gc_%d", dir, chained_gc->id); /* XXX: Security hole */
    }

    res = writeBufferToFile(buffer, buffer_size, fileName);
    free(buffer);
    return res;
}

int 
loadChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, int id,
              bool isGarbler) 
{
    char* buffer, fileName[50];
    long fs;
    size_t p;

    if (isGarbler) {
        sprintf(fileName, "%s/chained_gc_%d", dir, id);
    } else { 
        sprintf(fileName, "%s/chained_gc_%d", dir, id);
    }

    fs = filesize(fileName);
    buffer = malloc(fs);

    if (readFileIntoBuffer(buffer, fileName) == FAILURE) {
        return FAILURE;
    }

    GarbledCircuit* gc = &chained_gc->gc;

    p = copyGarbledCircuitFromBuffer(gc, buffer, isGarbler);
    memcpy(&(chained_gc->id), buffer+p, sizeof(int));
    p += sizeof(int);
    memcpy(&(chained_gc->type), buffer+p, sizeof(CircuitType));
    p += sizeof(CircuitType);
    if (isGarbler) {
        chained_gc->inputLabels = allocate_blocks(2 * gc->n);
        memcpy(chained_gc->inputLabels, buffer+p, sizeof(block)*2*gc->n);
        p += sizeof(block) * 2 * gc->n;

        chained_gc->outputMap = allocate_blocks(2 * gc->m);
        memcpy(chained_gc->outputMap, buffer+p, sizeof(block)*2*gc->m);
        p += sizeof(block) * 2 * gc->m;
    }

    if (p > fs) {
        printf("Buffer overflow error\n");
        free(buffer);
        return FAILURE;
    }
    free(buffer);
    return SUCCESS;
}

void
print_garbled_gate(GarbledGate *gg)
{
    printf("%ld %ld %ld %d\n", gg->input0, gg->input1, gg->output, gg->type);
}

void
print_garbled_table(GarbledTable *gt)
{
    print_block(gt->table[0]);
    printf(" ");
    print_block(gt->table[1]);
    printf(" ");
    print_block(gt->table[2]);
    printf(" ");
    print_block(gt->table[3]);
    printf("\n");
}

void
print_wire(Wire *w)
{
    print_block(w->label0);
    printf(" ");
    print_block(w->label1);
    printf("\n");
}

void
print_gc(GarbledCircuit *gc)
{
    printf("n = %d\n", gc->n);
    printf("m = %d\n", gc->m);
    printf("q = %d\n", gc->q);
    printf("r = %d\n", gc->r);
    for (int i = 0; i < gc->q; ++i) {
        printf("garbled gate %d: ", i);
        print_garbled_gate(&gc->garbledGates[i]);
        printf("garbled table %d: ", i);
        print_garbled_table(&gc->garbledTable[i]);
    }
    for (int i = 0; i < gc->r; ++i) {
        printf("wire %d: ", i);
        print_wire(&gc->wires[i]);
    }
    for (int i = 0; i < gc->m; ++i) {
        printf("%d\n", gc->outputs[i]);
    }
}

int
saveOTLabels(char *fname, block *labels, int n, bool isSender)
{
    size_t size = sizeof(block) * (isSender ?  2 : 1) * n;

    if (writeBufferToFile((char *) labels, size, fname) == FAILURE)
        return FAILURE;

    return SUCCESS;
}

block *
loadOTLabels(char *fname)
{
    block *buf;

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
