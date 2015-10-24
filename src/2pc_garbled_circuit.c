#include "2pc_garbled_circuit.h"

#include <malloc.h>
#include <assert.h>
#include <stdint.h>

#include "utils.h"

int 
createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n) 
{
    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < n; i++) {
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        buildAdderCircuit(p_gc);
        chained_gcs[i].id = i;
        chained_gcs[i].type = ADDER22;
        chained_gcs[i].inputLabels = memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.n );
        chained_gcs[i].outputMap = memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.m);
        assert(chained_gcs[i].inputLabels != NULL && chained_gcs[i].outputMap != NULL);
        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap, &delta);
    }
    return 0;
}

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc) 
{
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    free(chained_gc->inputLabels);
    free(chained_gc->outputMap);
    // TODO free other things
    return 0;
}

void
buildAdderCircuit(GarbledCircuit *gc) 
{
    int n = 2; // number of inputs 
    int m = 2; // number of outputs
    int q = 3; // number of gates
	int r = n+q;  // number of wires

	int *inputs = (int *) malloc(sizeof(int) * n);
	countToN(inputs, n);
    int* outputs = (int*) malloc(sizeof(int) * m);
	block *labels = (block*) malloc(sizeof(block) * 2 * n);
	block *outputmap = (block*) malloc(sizeof(block) * 2 * m);

	GarblingContext gc_context;

	createInputLabels(labels, n);
	createEmptyGarbledCircuit(gc, n, m, q, r, labels);
	startBuilding(gc, &gc_context);
	ADD22Circuit(gc, &gc_context, inputs, outputs);
	finishBuilding(gc, &gc_context, outputmap, outputs);

    free(gc_context.fixedWires);
    free(inputs);
    free(outputs);
    free(labels);
    free(outputmap);
}

int 
saveGarbledCircuit(GarbledCircuit* gc, char* fileName) 
{
    char* buffer;
    size_t buffer_size;

    buffer_size = sizeofGarbledCircuit(gc);
    buffer = malloc(buffer_size);

    if (writeGarbledCircuitToBuffer(gc, buffer, buffer_size) == FAILURE)  {
        return FAILURE;
    }

    if (writeBufferToFile(buffer, buffer_size, fileName) == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}

int 
loadGarbledCircuit(GarbledCircuit* gc, char* fileName) 
{
    char* buffer;
    long fs;

    fs = filesize(fileName);
    buffer = malloc(fs);

    readFileIntoBuffer(buffer, fileName);
    readBufferIntoGarbledCircuit(gc, buffer);

    return SUCCESS;
}

static int 
writeGarbledCircuitToBuffer(GarbledCircuit* gc, char* buffer, size_t max_buffer_size) 
{
    /*
     * Save the garbled circuit to fileName.
	    - int n, m, q, r; 
     * The following fields are not saved:
     *  - inputLabels
     *  - outputLabels
     *  - outputs
     *  - id
     *  - globalKey
     */
    /*
    typedef struct {
        block* inputLabels, outputLabels;  // 2*n inputLabels, 2*m outputLabels
	    GarbledGate* garbledGates;
	    GarbledTable* garbledTable;
	    Wire* wires;
	    int *outputs;
	    long id;
	    block globalKey;

	    gc->garbledGates = (GarbledGate *) memalign(128, sizeof(GarbledGate) * q);
	    gc->garbledTable = (GarbledTable *) memalign(128, sizeof(GarbledTable) * q);
	    gc->wires = (Wire *) memalign(128, sizeof(Wire) * r);
	    gc->outputs = (int *) memalign(128, sizeof(int) * m);

    } GarbledCircuit;
    */
    size_t p = 0;
    memcpy(buffer+p, &(gc->n), sizeof(gc->n));
    p += sizeof(gc->n);
    memcpy(buffer+p, &(gc->m), sizeof(gc->m));
    p += sizeof(gc->m);
    memcpy(buffer+p, &(gc->q), sizeof(gc->q));
    p += sizeof(gc->q);
    memcpy(buffer+p, &(gc->r), sizeof(gc->r));
    p += sizeof(gc->r);

    // save garbled gates
    memcpy(buffer+p, gc->garbledGates, sizeof(GarbledGate)*gc->q);
    p += sizeof(GarbledGate) * gc->q;

    // save garbled table
    memcpy(buffer+p, gc->garbledTable, sizeof(GarbledTable)*gc->q);
    p += sizeof(GarbledTable)*gc->q;

    // save wires
    memcpy(buffer+p, gc->wires, sizeof(Wire)*gc->r);
    p += sizeof(Wire)*gc->r;

    // save globalKey
    memcpy(buffer + p, &(gc->globalKey), sizeof(gc->globalKey));
    p += sizeof(gc->globalKey);

    if (p >= max_buffer_size) {
        printf("Buffer overflow error\n"); 
        return FAILURE;
    }

    return SUCCESS;
}

static int
readBufferIntoGarbledCircuit(GarbledCircuit *gc, char* buffer) 
{
    size_t p = 0;
    memcpy(&(gc->n), buffer+p, sizeof(int));
    p += sizeof(int);
    memcpy(&(gc->m), buffer+p, sizeof(int));
    p += sizeof(int);
    memcpy(&(gc->q), buffer+p, sizeof(int));
    p += sizeof(int);
    memcpy(&(gc->r), buffer+p, sizeof(int));
    p += sizeof(int);

    gc->garbledGates = malloc(sizeof(GarbledGate) * gc->q);
    gc->garbledTable = malloc(sizeof(GarbledTable) * gc->q);
    gc->wires = malloc(sizeof(Wire) * gc->r);

    // read garbledGates
    memcpy(gc->garbledGates, buffer+p, sizeof(GarbledGate)*gc->q);
    p += sizeof(GarbledGate) * gc->q;

    // read garbledTable
    memcpy(gc->garbledTable, buffer+p, sizeof(GarbledTable)*gc->q);
    p += sizeof(GarbledTable) * gc->q;

    memcpy(gc->wires, buffer+p, sizeof(Wire)*gc->r);
    p += sizeof(Wire) * gc->r;

    // save globalKey
    memcpy(&(gc->globalKey), buffer+p, sizeof(gc->globalKey));
    p += sizeof(gc->globalKey);

    return SUCCESS;
}


static size_t 
sizeofGarbledCircuit(GarbledCircuit* gc) 
{
    size_t size;
    size = sizeof(int)*4 + sizeof(GarbledGate)*(gc->q + 2) + sizeof(GarbledTable)*gc->q
        + sizeof(Wire)*(gc->r) + sizeof(block);
    return size;
}




