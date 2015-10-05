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
mySave(struct Alex *gc, char* fileName) 
{
    FILE *f = fopen(fileName, "w");
    int fs = filesize(fileName);
    char *buffer = malloc(sizeof(int)*4);

    if (f == NULL) {
        printf("Write: Error in opening file.\n");
        return FAILURE;
    }

    memcpy(buffer, &gc->n, sizeof(int));
    memcpy(buffer + sizeof(int)*1, &gc->m, sizeof(int));
    memcpy(buffer + sizeof(int)*2, &gc->q, sizeof(int));
    memcpy(buffer + sizeof(int)*3, &gc->r, sizeof(int));

    int a,b,c,d;
    memcpy(&a, buffer + sizeof(int)*0, sizeof(int));
    memcpy(&b, buffer + sizeof(int)*1, sizeof(int));
    memcpy(&c, buffer + sizeof(int)*2, sizeof(int));
    memcpy(&d, buffer + sizeof(int)*3, sizeof(int));
    printf("\n %d %d %d %d\n", a, b, c, d);

    fwrite(buffer, sizeof(char), sizeof(int)*4, f);
    fclose(f);
    int fs2 = filesize(fileName);
    printf("asdf %d\n", fs2);

    return SUCCESS;

}

int myLoad(struct Alex *gc, char* fileName)
{
    FILE *f = fopen(fileName, "r");
    int fs = filesize(fileName);
    printf("asdf %d\n", fs);
    char *buffer = malloc(sizeof(int)*4);

    if (f == NULL) {
        printf("Write: Error in opening file.\n");
        return FAILURE;
    }

    //fread(buffer, 1, sizeof(int)*4, f);
    fread(buffer, sizeof(char), sizeof(int)*4, f);
    memcpy(&gc->n, buffer + sizeof(int)*0, sizeof(int));
    memcpy(&gc->m, buffer + sizeof(int)*1, sizeof(int));
    memcpy(&gc->q, buffer + sizeof(int)*2, sizeof(int));
    memcpy(&gc->r, buffer + sizeof(int)*3, sizeof(int));
    fclose(f);

    return SUCCESS;
}

