#include "2pc_garbled_circuit.h"

#include <malloc.h>
#include <assert.h>
#include <stdint.h>

#include "utils.h"
#include "2pc_common.h"
#include "circuits.h"

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc) 
{
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    free(chained_gc->inputLabels);
    free(chained_gc->outputMap);
    return 0;
}

void AddAESCircuit(GarbledCircuit *gc, GarblingContext *garblingContext, int numAESRounds, 
        int *inputWires, int *outputWires) 
{
    /* Adds AES to the circuit
     * :param inputWires has size 128 * numAESRounds + 128
     */

    int addKeyInputs[256];
    int addKeyOutputs[128];
    int subBytesOutputs[128];
    int shiftRowsOutputs[128];
    int mixColumnOutputs[128];

    memcpy(addKeyInputs, inputWires, sizeof(addKeyInputs));

    for (int round = 0; round < numAESRounds; round++) {
        AddRoundKey(gc, garblingContext, addKeyInputs, addKeyOutputs);

        for (int j = 0; j < 16; j++) {
            SubBytes(gc, garblingContext, addKeyOutputs + (8*j), subBytesOutputs + (8*j));
        }
        
        ShiftRows(gc, garblingContext, subBytesOutputs, shiftRowsOutputs);

        if (round != numAESRounds-1) { /*if not final round */
            for (int j = 0; j < 4; j++) {
	        	MixColumns(gc, garblingContext, shiftRowsOutputs + j * 32, mixColumnOutputs + 32 * j);
            }

            memcpy(addKeyInputs, mixColumnOutputs, sizeof(mixColumnOutputs));
            memcpy(addKeyInputs + 128, inputWires + (round + 2) * 128, sizeof(int)*128);
        } 
    }

    for (int i = 0; i < 128; i++) {
        outputWires[i] = shiftRowsOutputs[i];
    }
}

void 
buildCBCFullCircuit (GarbledCircuit *gc, int num_message_blocks, int num_aes_rounds, block *delta) 
{
    int n = (num_message_blocks * 128) + (num_message_blocks * num_aes_rounds * 128) + 128;
    int m = num_message_blocks*128;
    // TODO figure out a rough estimate for q based on num_aes_rounds and num_message_blocks 
    int q = 5000000; /* number of gates */ 
    int r = n + q; /* number of wires */
    int num_evaluator_inputs = 128 * num_message_blocks;
    int num_garbler_inputs = n - num_evaluator_inputs;

    /* garbler_inputs and evaluator_inputs are the wires to 
     * which the garbler's and evaluators inputs should go */
    int *garbler_inputs = (int*) malloc(sizeof(int) * num_garbler_inputs);
    int *evaluator_inputs = (int*) malloc(sizeof(int) * num_evaluator_inputs);

    countToN(evaluator_inputs, num_evaluator_inputs);
    for (int i = 0; i < num_garbler_inputs ; i++)
        garbler_inputs[i] = i + num_evaluator_inputs;

    int xorIn[256] = {0};
    int xorOut[128] = {0};
    int aesOut[128] = {0};
    int *aesIn = (int*) malloc(sizeof(int) * (128 * (num_aes_rounds + 1)));
    int *outputWires = (int*) malloc(sizeof(int) * m);
    assert(aesIn && outputWires);

    block *inputLabels = allocate_blocks(2*n); 
    block *outputMap = allocate_blocks(2*m);

	createInputLabelsWithR(inputLabels, n, *delta);
	createEmptyGarbledCircuit(gc, n, m, q, r, inputLabels);
    GarblingContext gc_context;
	startBuilding(gc, &gc_context);

    countToN(xorIn, 256);
    XORCircuit(gc, &gc_context, 256, xorIn, xorOut);

    int output_idx = 0;
    int garbler_input_idx = 128;
    int evaluator_input_idx = 128;

    memcpy(aesIn, xorOut, sizeof(xorOut)); // gets the out wires from xor circuit
    memcpy(aesIn + 128, garbler_inputs + garbler_input_idx, num_aes_rounds * 128);
    garbler_input_idx += 128*num_aes_rounds;

    AddAESCircuit(gc, &gc_context, num_aes_rounds, aesIn, aesOut);
    memcpy(outputWires + output_idx, aesOut, sizeof(aesOut));
    output_idx += 128;

    for (int i = 0; i < num_message_blocks - 1; i++) {
        /* xor previous round output with new message block */
        memcpy(xorIn, aesOut, sizeof(aesOut));
        memcpy(xorIn + 128, evaluator_inputs + evaluator_input_idx, 128);
        evaluator_input_idx += 128;

        XORCircuit(gc, &gc_context, 256, xorIn, xorOut);

        /* AES the output of the xor, and add in the key */
        memcpy(aesIn, xorOut, sizeof(xorOut)); // gets the out wires from xor circuit
        memcpy(aesIn + 128, garbler_inputs + garbler_input_idx, num_aes_rounds * 128);
        garbler_input_idx += 128 * num_aes_rounds;

        AddAESCircuit(gc, &gc_context, num_aes_rounds, aesIn, aesOut);

        memcpy(outputWires + output_idx, aesOut, sizeof(aesOut));
        output_idx += 128;
    }
    assert(garbler_input_idx == num_garbler_inputs);
    assert(evaluator_input_idx == num_evaluator_inputs);
    assert(output_idx == m);
	finishBuilding(gc, &gc_context, outputMap, outputWires);
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

void 
buildXORCircuit(GarbledCircuit *gc, block *delta) {
	GarblingContext garblingContext;
    int n = 256;
    int m = 128;
    int q = 400;
    int r = 400;
    int inp[n];
    int outs[m];
    countToN(inp, n);
    block inputLabels[2*n];
    block outputMap[2*m];

	createInputLabelsWithR(inputLabels, n, *delta);
	createEmptyGarbledCircuit(gc, n, m, q, r, inputLabels);
	startBuilding(gc, &garblingContext);
    XORCircuit(gc, &garblingContext, 256, inp, outs);
    for (int i=0; i<10; i++) {
        printf("%d\n", outs[i]);
    }
	finishBuilding(gc, &garblingContext, outputMap, outs);
}

void buildAESRoundComponentCircuit(GarbledCircuit *gc, bool isFinalRound, block* delta) 
{
	GarblingContext garblingContext;
    int n1 = 128; // size of key
    int n = 256; // tot size of input: prev value is 128 bits + key size 128 bits
    int m = 128;
    int q = 4500; // an upper bound
    int r = 4500; // an upper bound
    int inp[n];
    countToN(inp, n);
    int prevAndKey[n];
	int keyOutputs[n1];
	int subBytesOutputs[n1];
	int shiftRowsOutputs[n1];
	int mixColumnOutputs[n1];
	block inputLabels[2*n];
	block outputMap[2*m];

    // So i'm pretty sure that these labels are ignored
    // in justGarble/src/garble.c line 190ish. 
    // the for loop overwires these values with values that it created.
    // but justGarble's tests are setup like this, where the values are overwritten.
	createInputLabelsWithR(inputLabels, n, *delta);
	createEmptyGarbledCircuit(gc, n, m, q, r, inputLabels);
	startBuilding(gc, &garblingContext);
	countToN(prevAndKey, 256); 

    // first 128 bits of prevAndKey are prev value
    // second 128 bits of prevAndKey are the new key
    // addRoundKey xors them together into keyOutputs
	AddRoundKey(gc, &garblingContext, prevAndKey, keyOutputs);

    for (int i = 0; i < 16; i++) {
        SubBytes(gc, &garblingContext, keyOutputs + (8*i), subBytesOutputs + (8*i));
    }
        
    ShiftRows(gc, &garblingContext, subBytesOutputs, shiftRowsOutputs);

    if (true) {
    // if (!isFinalRound) { // TODO UNCOMMENT
        for (int i = 0; i < 4; i++) { 
            // fixed justGarble bug in their construction
	    	MixColumns(gc, &garblingContext, shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
	    }
        // output wires are stored in mixColumnOutputs
	    finishBuilding(gc, &garblingContext, outputMap, mixColumnOutputs);
    } else {
	    finishBuilding(gc, &garblingContext, outputMap, shiftRowsOutputs);
    }
}

void buildAESCircuit(GarbledCircuit *gc)
{
	GarblingContext garblingContext;

	int roundLimit = 10;
	int n = 128 * (roundLimit + 1);
	int m = 128;
	int q = 50000; //Just an upper bound
	int r = 50000;
    int* final;
	int inp[n];
	countToN(inp, n);
	int addKeyInputs[n * (roundLimit + 1)];
	int addKeyOutputs[n];
	int subBytesOutputs[n];
	int shiftRowsOutputs[n];
	int mixColumnOutputs[n];
	block labels[2 * n];
	block outputbs[m];
	block *outputMap = outputbs;
	block *inputLabels = labels;
	int i;

	createInputLabels(labels, n);
	createEmptyGarbledCircuit(gc, n, m, q, r, inputLabels);
	startBuilding(gc, &garblingContext);

	countToN(addKeyInputs, 256);

	for (int round = 0; round < roundLimit; round++) {

		AddRoundKey(gc, &garblingContext, addKeyInputs,
                    addKeyOutputs);

		for (i = 0; i < 16; i++) {
			SubBytes(gc, &garblingContext, addKeyOutputs + 8 * i,
                     subBytesOutputs + 8 * i);
		}

		ShiftRows(gc, &garblingContext, subBytesOutputs,
                  shiftRowsOutputs);

        // TODO double check this. I don't this coded correctly
        // final is being set to the 9th round's output
        // and addkey inputs shouldn't be happening in final round
        // etc.
		for (i = 0; i < 4; i++) {
			if (round == roundLimit - 1)
				MixColumns(gc, &garblingContext,
                           shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
		}

		for (i = 0; i < 128; i++) {
			addKeyInputs[i] = mixColumnOutputs[i];
			addKeyInputs[i + 128] = (round + 2) * 128 + i;
		}
	}
	final = shiftRowsOutputs;
	finishBuilding(gc, &garblingContext, outputMap, final);
}

int 
saveChainedGC(ChainedGarbledCircuit* chained_gc, bool isGarbler) 
{
    char* buffer;
    size_t buffer_size = 1;

    GarbledCircuit* gc = &chained_gc->gc;

    buffer_size += sizeof(int) * 4;
    buffer_size += sizeof(GarbledGate) * gc->q;
    buffer_size += sizeof(GarbledTable) * gc->q;
    buffer_size += sizeof(Wire) * gc->r; // wires
    buffer_size += sizeof(int) * gc->m; // outputs
    buffer_size += sizeof(block);
    buffer_size += sizeof(int); // id
    buffer_size += sizeof(CircuitType); 
    
    if (isGarbler) {
        buffer_size += sizeof(block)* 2 * gc->n; // inputLabels
        buffer_size += sizeof(block) * 2 *gc->m; // outputMap
    }

    buffer = malloc(buffer_size);

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

    // save outputs
    memcpy(buffer+p, gc->outputs, sizeof(int)*gc->m);
    p += sizeof(int) * gc->m; 

    // save globalKey
    memcpy(buffer + p, &(gc->globalKey), sizeof(block));
    p += sizeof(block);

    // id, type, inputLabels, outputmap
    memcpy(buffer + p, &(chained_gc->id), sizeof(int));
    p += sizeof(int);

    memcpy(buffer + p, &(chained_gc->type), sizeof(CircuitType));
    p += sizeof(CircuitType);

    if (isGarbler) {
        memcpy(buffer + p, chained_gc->inputLabels, sizeof(block)*2*gc->n);
        p += sizeof(block) * 2 * gc->n;

        memcpy(buffer + p, chained_gc->outputMap, sizeof(block)*2*gc->m);
        p += sizeof(block) * 2 * gc->m;
    }

    if (p >= buffer_size) {
        printf("Buffer overflow error in save.\n"); 
        return FAILURE;
    }

    char fileName[50];

    if (isGarbler) {
        sprintf(fileName, GARBLER_GC_PATH, chained_gc->id);
    } else { 
        sprintf(fileName, EVALUATOR_GC_PATH, chained_gc->id);
    }

    if (writeBufferToFile(buffer, buffer_size, fileName) == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}

void freeChainedGcs(ChainedGarbledCircuit* chained_gcs, int num) 
{
    for (int i = 0; i < num; i++) {
        ChainedGarbledCircuit *chained_gc = &chained_gcs[i];
        GarbledCircuit *gc = &chained_gc->gc;

        free(chained_gc->inputLabels);
        free(chained_gc->outputMap);
        free(gc->garbledGates);
        free(gc->garbledTable);
        free(gc->wires);
        free(gc->outputs);
    }
}

int 
loadChainedGC(ChainedGarbledCircuit* chained_gc, int id, bool isGarbler) 
{
    char* buffer, fileName[50];
    long fs;

    if (isGarbler) {
        sprintf(fileName, GARBLER_GC_PATH, id);
    } else { 
        sprintf(fileName, EVALUATOR_GC_PATH, id);
    }

    fs = filesize(fileName);
    buffer=malloc(fs);

    if (readFileIntoBuffer(buffer, fileName) == FAILURE) {
        return FAILURE;
    }

    GarbledCircuit* gc = &chained_gc->gc;

    size_t p = 0;
    memcpy(&(gc->n), buffer+p, sizeof(gc->n));
    p += sizeof(gc->n);
    memcpy(&(gc->m), buffer+p, sizeof(gc->m));
    p += sizeof(gc->m);
    memcpy(&(gc->q), buffer+p, sizeof(gc->q));
    p += sizeof(gc->q);
    memcpy(&(gc->r), buffer+p, sizeof(gc->r));
    p += sizeof(gc->r);

    // load garbled gates
    gc->garbledGates = malloc(sizeof(GarbledGate) * gc->q);
    memcpy(gc->garbledGates, buffer+p, sizeof(GarbledGate)*gc->q);
    p += sizeof(GarbledGate) * gc->q;

    // load garbled table
    gc->garbledTable = malloc(sizeof(GarbledTable) * gc->q);
    memcpy(gc->garbledTable, buffer+p, sizeof(GarbledTable)*gc->q);
    p += sizeof(GarbledTable)*gc->q;

    // load wires
    gc->wires = malloc(sizeof(Wire) * gc->r);
    memcpy(gc->wires, buffer+p, sizeof(Wire)*gc->r);
    p += sizeof(Wire)*gc->r;

    // load outputs
    gc->outputs = malloc(sizeof(int) * gc->m);
    memcpy(gc->outputs, buffer+p, sizeof(int) * gc->m);
    p += sizeof(int) * gc->m;

    // save globalKey
    memcpy(&(gc->globalKey), buffer+p, sizeof(block));
    p += sizeof(block);

    // id
    memcpy(&(chained_gc->id), buffer+p, sizeof(int));
    p += sizeof(int);

    // type
    memcpy(&(chained_gc->type), buffer+p, sizeof(CircuitType));
    p += sizeof(CircuitType);

    if (isGarbler) {
        // inputLabels
        (void) posix_memalign((void **) &chained_gc->inputLabels, 128, sizeof(block) * 2 * gc->n);
        assert(chained_gc->inputLabels);
        memcpy(chained_gc->inputLabels, buffer+p, sizeof(block)*2*gc->n);
        p += sizeof(block) * 2 * gc->n;

        // outputMap
        (void) posix_memalign((void **) &chained_gc->outputMap, 128, sizeof(block) * 2 * gc->m);
        assert(chained_gc->outputMap);
        memcpy(chained_gc->outputMap, buffer+p, sizeof(block)*2*gc->m);
        p += sizeof(block) * 2 * gc->m;
    }

    if (p > fs) {
        printf("Buffer overflow error\n"); 
        return FAILURE;
    }
    free(buffer);

    return SUCCESS;
}

void
print_block(block blk)
{
    uint64_t *val = (uint64_t *) &blk;
    printf("%016lx%016lx", val[1], val[0]);
}

void
print_garbled_gate(GarbledGate *gg)
{
    printf("%ld %ld %ld %d %d\n", gg->input0, gg->input1, gg->output, gg->id,
           gg->type);
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
    printf("%ld ", w->id);
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

void
print_blocks(const char *str, block *blks, int length)
{
    for (int i = 0; i < length; ++i) {
        printf("%s ", str);
        print_block(blks[i]);
        printf("\n");
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
