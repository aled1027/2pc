#include "2pc_garbled_circuit.h"

#include <malloc.h>
#include <assert.h>
#include <stdint.h>

#include "utils.h"
#include "2pc_common.h"
#include "gates.h"
#include <math.h>  /* log2 */

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc) 
{
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    free(chained_gc->inputLabels);
    free(chained_gc->outputMap);
    return 0;
}

void buildLevenshteinCircuit(GarbledCircuit *gc, InputLabels inputLabels, OutputMap outputMap,
        int *outputWires, int l, int m)
{
    /* The goal of the levenshtein algorithm is
     * populate the l+1 by l+1 D matrix. 
     * D[i][j] points to an integer address. 
     * At that address is an DIntSize-bit number representing the distance between
     * a[0:i] and b[0:j] (python syntax)
     *
     * If l = 2
     * e.g. D[1][2] = 0x6212e0
     * *0x6212e0 = {0,1} = the wire indices
     *
     * For a concrete understanding of the algorithm implemented, 
     * see extra_scripts/leven.py
     */

    /* get number of bits needed to represent our numbers.
     * The input strings can be at most l distance apart,
     * since they are of length l, so we need floor(log_2(l)+1) bits
     * to express the number
     */
    int DIntSize = (int) floor(log2(l)) + 1;
    int inputsDevotedToD = DIntSize * (l+1);

    int n = inputsDevotedToD + 2*2*l;
    int core_n = (3 * DIntSize) + 4;
    int q = 10000; /* number of gates */ 
    int r = n + q; /* number of wires */

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;
    createInputLabelsWithR(inputLabels, n, &delta);

    GarblingContext gcContext;
	createEmptyGarbledCircuit(gc, n, m, q, r, inputLabels);
	startBuilding(gc, &gcContext);

    int fixedOneWire = 0;
    int *inputWires = allocate_ints(n);
    countToN(inputWires, n);

    ///* Populate D's 0th row and column with wire indices from inputs*/
    int *D[l+1][l+1];
    for (int i = 0; i < l+1; i++) {
        for (int j = 0; j < l+1; j++) {
            D[i][j] = allocate_ints(l);
            if (i == 0) {
                memcpy(D[0][j], inputWires + (j*DIntSize), sizeof(int) * DIntSize);
                //printf("Giving D[0][%d] inputWires: %d\n", j, D[0][j][0]);
            } else if (j == 0) {
                memcpy(D[i][0], inputWires + (i*DIntSize), sizeof(int) * DIntSize);
                //printf("Giving D[%d][0] inputWires: %d\n", i, D[i][0][0]);
            }
        }
    }

    /* Populate a and b (the input strings) wire indices */
    int a[l*2];
    int b[l*2];
    memcpy(a, inputWires + inputsDevotedToD, sizeof(int)*l*2);
    memcpy(b, inputWires + inputsDevotedToD + l*2, sizeof(int)*l*2);

    ///* add the core circuits */
    int *coreInputWires = allocate_ints(core_n);
    int *coreOutputWires = allocate_ints(DIntSize);
    for (int i = 1; i < l + 1; i++) {
        for (int j = 1; j < l + 1; j++) {
            /* Set inputs for levenshtein core */
            int p = 0;
            memcpy(coreInputWires + p, D[i-1][j-1], sizeof(int) * DIntSize);
            p += DIntSize;
            memcpy(coreInputWires + p, D[i][j-1], sizeof(int) * DIntSize);
            p += DIntSize;
            memcpy(coreInputWires + p, D[i-1][j], sizeof(int) * DIntSize);
            p += DIntSize;
            memcpy(coreInputWires + 3*l, &a[(i-1)*2], sizeof(int) * 2);
            p += 2;
            memcpy(coreInputWires + 3*l + 2, &b[(j-1)*2], sizeof(int) * 2);
            p += 2;
            assert(p == core_n);

            //printf("coreInputWires: (i=%d,j=%d) (%d %d) (%d %d) (%d %d) (%d %d) (%d %d)\n",
            //        i,
            //        j,
            //        coreInputWires[0],
            //        coreInputWires[1],
            //        coreInputWires[2],
            //        coreInputWires[3],
            //        coreInputWires[4],
            //        coreInputWires[5],
            //        coreInputWires[6],
            //        coreInputWires[7],
            //        coreInputWires[8],
            //        coreInputWires[9]);
            addLevenshteinCoreCircuit(gc, &gcContext, l, coreInputWires, coreOutputWires);
            printf("coreOutputWires: (%d)\n", coreOutputWires[0]);

            /* Save coreOutputWires to D[i][j] */
            memcpy(D[i][j], coreOutputWires, sizeof(int) * DIntSize);
        }
    }
    //printf("D[0][0] = %d\n", D[0][0][0]);
    //printf("D[0][0] = %d\n", D[0][0][1]);
    //printf("D[1][0] = %d\n", D[1][0][0]);
    //printf("D[1][0] = %d\n", D[1][0][1]);
    //printf("D[2][0] = %d\n", D[2][0][0]);
    //printf("D[2][0] = %d\n", D[2][0][1]);
    //printf("D[0][1] = %d\n", D[0][1][0]);
    //printf("D[0][1] = %d\n", D[0][1][1]);
    //printf("D[1][1] = %d\n", D[1][1][0]);
    //printf("D[1][1] = %d\n", D[1][1][1]);
    //printf("D[2][1] = %d\n", D[2][1][0]);
    //printf("D[2][1] = %d\n", D[2][1][1]);
    //printf("D[1][2] = %d\n", D[1][2][0]);
    //printf("D[1][2] = %d\n", D[1][2][1]);
    //printf("D[2][2] = %d\n", D[2][2][0]);
    //printf("D[2][2] = %d\n", D[2][2][1]);
    ////memcpy(outputWires, D[l][l], sizeof(int) * DIntSize); 
    //printf("D[l][l] = %d\n", D[l][l][0]);
    //printf("D[l][l] = %d\n", D[l][l][1]);

    countToN(outputWires, m);
    finishBuilding(gc, &gcContext, outputMap, outputWires);
}

void
addLevenshteinCoreCircuit(GarbledCircuit *gc, GarblingContext *gcContext, 
        int l, int *inputWires, int *outputWires) 
{
    /* Makes a "LevenshteinCore" circuit as defined in 
     * Faster Secure Two-Party Computation Using Garbled Circuits
     * Page 9, figure 5c. 
     *
     * Note: they use a two-bit alphabet, they call the size of the alphabet sigma,
     * because there are four possible nucleobases. Here sigma is hardcoded as 2
     *
     * Input = D[i-1][j-1] ||D[i-1][j] || D[i][j-1] || a[i] || b[j] 
     * |Input| = l-bits || l-bits || l-bits || 2-bits || 2-bits
     *
     * Wires are ordered such that 1s digit is first, 2s digit second, and so forth.
     * This is way in which JustGarble oriented their adders.
     */

    /* input wires indices for the 5 input objects */

    int DIntSize = (int) floor(log2(l)) + 1;
    int *D_minus_minus = allocate_ints(DIntSize); /*D[i-1][j-1] */
    int *D_minus_same = allocate_ints(DIntSize); /* D[1-1][j] */
    int *D_same_minus = allocate_ints(DIntSize); /* D[i][j-1] */
    int symbol0[2];
    int symbol1[2];

    /* arrayPopulateRange is inclusive on start and exclusive on end */
    memcpy(D_minus_minus, inputWires, sizeof(int) * DIntSize);
    memcpy(D_minus_same, inputWires + DIntSize, sizeof(int) * DIntSize);
    memcpy(D_same_minus, inputWires + 2*DIntSize, sizeof(int) * DIntSize);
    memcpy(symbol0, inputWires + 3*DIntSize, sizeof(int) * 2);
    memcpy(symbol1, inputWires + 3*DIntSize + 2, sizeof(int) * 2);

    /* First MIN circuit 
     * D_minus_same, D_same_minus MIN circuit */
    int *min_inputs = allocate_ints(2*DIntSize);
    memcpy(min_inputs, D_minus_same, sizeof(int) * DIntSize);
    memcpy(min_inputs + DIntSize, D_same_minus, sizeof(int) * DIntSize);
    int *min_outputs = allocate_ints(DIntSize); /* will be filled by MINCircuit */
    int val = 2*DIntSize;
    MINCircuit(gc, gcContext, val, min_inputs, min_outputs);
    printf("min_inputs1 %d %d %d %d\n", min_inputs[0], min_inputs[1], min_inputs[2], min_inputs[3]);
    //printf("min1 output: %d\n", min_outputs[0]);


    /* Second MIN Circuit 
     * Uses input from first min cricuit and D_minus_minus */
    memcpy(min_inputs, D_minus_minus, sizeof(int) * DIntSize);
    memcpy(min_inputs + DIntSize, min_outputs, sizeof(int) * DIntSize);
    int *min_outputs2 = allocate_ints(DIntSize+1); /* will be filled by MINCircuit */
    MINCircuitWithLEQOutput(gc, gcContext, 2*DIntSize, min_inputs, min_outputs2); 
    printf("min_inputs2 %d %d %d %d\n", min_inputs[0], min_inputs[1], min_inputs[2], min_inputs[3]);
    printf("min2 %d %d\n", min_outputs2[0], min_outputs2[1]);

    /* T */
    int xor_output[2];
    xor_output[0] = getNextWire(gcContext);
    xor_output[1] = getNextWire(gcContext);
    int T_output = getNextWire(gcContext);
    XORGate(gc, gcContext, symbol0[0], symbol1[1], xor_output[0]);
    XORGate(gc, gcContext, symbol0[1], symbol1[0], xor_output[1]);
    ORGate(gc, gcContext, xor_output[0], xor_output[1], T_output);
    printf("T_output: %d\n", T_output);

    /* 2-1 MUX */
    int mux_switch = min_outputs2[DIntSize + 1];
    int mux_input[2];
    mux_input[1] = T_output;
    int fixed_one_wire = fixedOneWire(gc, gcContext);
    mux_input[0] = fixed_one_wire;

    int and_output[2];
    and_output[0] = getNextWire(gcContext);
    and_output[1] = getNextWire(gcContext);
    ANDGate(gc, gcContext, mux_input[0], mux_switch, and_output[0]);

    int not_mux_switch = getNextWire(gcContext);
    NOTGate(gc, gcContext, mux_switch, not_mux_switch);
    ANDGate(gc, gcContext, mux_input[1], not_mux_switch, and_output[1]);

    int mux_output = getNextWire(gcContext);
    ORGate(gc, gcContext, and_output[0], and_output[1], mux_output);
    printf("mux_output: %d\n", mux_output);

    /* AddOneBit */
    /* TODO change name of l_bit_number - not descriptive anymore */

    /* set inputs for AddOneBit */
    /* TODO change to INCCircuit */
    //memcpy(add_inputs + DIntSize, l_bit_number, sizeof(int) * DIntSize);
    //ADDCircuit(gc, gcContext, 2*DIntSize, add_inputs, add_outputs);
    
    int fixed_zero_wire = fixedZeroWire(gc, gcContext);
    int *add_inputs = allocate_ints(DIntSize);
    int *add_outputs = allocate_ints(DIntSize);
    memcpy(add_inputs, min_outputs2, sizeof(int) * DIntSize);
    INCCircuit(gc, gcContext, DIntSize, add_inputs, add_outputs);

    /* Final MUX (not in paper) */
    /* Do a 2-1 mux for each wire */
    int *final = allocate_ints(DIntSize);
    for (int i = 0; i < DIntSize; i++) {
        int theSwitch = mux_output;
        int theMuxInput[2];
        theMuxInput[1] = min_outputs2[i];
        theMuxInput[0] = add_outputs[i];

        int theAndOutput[2];
        theAndOutput[0] = getNextWire(gcContext);
        theAndOutput[1] = getNextWire(gcContext);
        ANDGate(gc, gcContext, theMuxInput[0], theSwitch, theAndOutput[0]);

        int theNotSwitch = getNextWire(gcContext);
        NOTGate(gc, gcContext, theSwitch, theNotSwitch);
        ANDGate(gc, gcContext, theMuxInput[1], theNotSwitch, theAndOutput[1]);

        int theMuxOutput = getNextWire(gcContext);
        ORGate(gc, gcContext, theAndOutput[0], theAndOutput[1], theMuxOutput);
        final[i] = theMuxOutput;
    }

    memcpy(outputWires, final, sizeof(int) * DIntSize);
    
    /* free a ton of pointers */
    free(D_minus_minus);
    free(D_minus_same);
    free(D_same_minus);
    free(min_inputs);
    free(min_outputs);
    free(min_outputs2);
    free(add_inputs);
    free(add_outputs);
    free(final);
}

int MINCircuitWithLEQOutput(GarbledCircuit *gc, GarblingContext *garblingContext, int n,
		int* inputs, int* outputs) {
    /* Essentially copied from JustGarble/src/circuits.c.
     * Different from their MIN circuit because it has output size equal to n/2 + 1
     * where that last bit indicates the output of the LEQ circuit,
     * e.g. outputs[2/n + 1] = 1 iff input0 <= input1
     */

	int i;
	int leqOutput[1];
	int andOneOutput[n / 2];
	int andTwoOutput[n / 2];
	int notOutput = getNextWire(garblingContext);
	LEQCircuit(gc, garblingContext, n, inputs, leqOutput);
	NOTGate(gc, garblingContext, leqOutput[0], notOutput);
	for (i = 0; i < n / 2; i++) {
		andOneOutput[i] = getNextWire(garblingContext);
		andTwoOutput[i] = getNextWire(garblingContext);
		outputs[i] = getNextWire(garblingContext);
		ANDGate(gc, garblingContext, leqOutput[0], inputs[i], andOneOutput[i]);
		ANDGate(gc, garblingContext, notOutput, inputs[n / 2 + i],
				andTwoOutput[i]);
		XORGate(gc, garblingContext, andOneOutput[i], andTwoOutput[i],
				outputs[i]);
	}
    outputs[n/2] = leqOutput[0];
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

    InputLabels inputLabels = allocate_blocks(2*n); 
    OutputMap outputMap = allocate_blocks(2*m);

	createInputLabelsWithR(inputLabels, n, delta);
    /* check that inputLabels is taken out of this scope */
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
buildXORCircuit(GarbledCircuit *gc, block *delta) 
{
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

	createInputLabelsWithR(inputLabels, n, delta);
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
	createInputLabelsWithR(inputLabels, n, delta);
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
	OutputMap outputMap = outputbs;
	InputLabels inputLabels = labels;
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
