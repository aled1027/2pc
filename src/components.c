#include "components.h"
#include "circuits.h"
#include "utils.h"

#include <assert.h>

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

void
buildAESRoundComponentCircuit(GarbledCircuit *gc, bool isFinalRound, block* delta) 
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

