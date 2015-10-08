#include "circuit_builder.h"
#include "utils.h"

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

void
buildAdderCircuitOld(GarbledCircuit *gc)
{
    /*
     * Build the 22 adder circuit from scratch. 
     * prefer buildAdderCircuit which uses ADD22CIRCUIT
     */

    int n = 2; // number of inputs 
    int m = 2; // number of outputs
    int q = 20; // number of gates
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

    int wire1 = getNextWire(&gc_context);
    int wire2 = getNextWire(&gc_context);

    XORGate(gc, &gc_context, inputs[0], inputs[1], wire1);
	ANDGate(gc, &gc_context, inputs[0], inputs[1], wire2);

	outputs[0] = wire1;
	outputs[1] = wire2;

	finishBuilding(gc, &gc_context, outputmap, outputs);
}

int 
saveGarbledCircuit(GarbledCircuit* gc, char* fileName) {
    /*
     * Save the garbled circuit to fileName.
     * The following fields are not saved:
     *  - inputLabels
     *  - outputLabels
     *  - outputs
     *  - id
     *  - globalKey
     */
    /*
    typedef struct {
	    int n, m, q, r; 
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

    FILE *f = fopen(fileName, "w");
    size_t buf_size = sizeof(int)*4 + sizeof(GarbledGate)*(gc->q + 2) + sizeof(GarbledTable)*gc->q
        + sizeof(Wire)*(gc->r) + sizeof(block);
        ;
    char *buffer = malloc(buf_size);

    if (f == NULL) {
        printf("Write: Error in opening file.\n");
        return FAILURE;
    }
    // save metdata
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

    fwrite(buffer, sizeof(char), p, f);
    fclose(f);

    return SUCCESS;
}

int 
readGarbledCircuit(GarbledCircuit* gc, char* fileName) {
    FILE *f = fopen(fileName, "r");

    if (f == NULL) {
        printf("Write: Error in opening file.\n");
        return FAILURE;
    }

    long fs = filesize(fileName);
    char *buffer = malloc(fs);

    fread(buffer, sizeof(char), fs, f);

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

    memcpy(gc->garbledTable, buffer+p, sizeof(GarbledTable)*gc->q);
    p += sizeof(GarbledTable) * gc->q;

    memcpy(gc->wires, buffer+p, sizeof(Wire)*gc->r);
    p += sizeof(Wire) * gc->q;

    // save globalKey
    memcpy(&(gc->globalKey), buffer+p, sizeof(gc->globalKey));
    p += sizeof(gc->globalKey);

    fclose(f);
    return SUCCESS;
}
