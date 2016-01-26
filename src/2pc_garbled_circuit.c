#include "2pc_garbled_circuit.h"

#include <malloc.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>  /* log2 */

#include "utils.h"
#include "2pc_common.h"
#include "circuits.h"
#include "gates.h"

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
saveChainedGC(ChainedGarbledCircuit* chained_gc, const char *dir, bool isGarbler)
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
    return SUCCESS;
}

int
loadChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, int id,
              bool isGarbler)
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
