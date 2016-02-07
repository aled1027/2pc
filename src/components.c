#include "components.h"
#include "utils.h"

#include "garble.h"
#include "circuits.h"
#include "gates.h"

#include <assert.h>
#include <math.h>

bool isFinalCircuitType(CircuitType type) 
{
    if (type == AES_FINAL_ROUND) 
        return true;
    return false;
}

void
buildLevenshteinCircuit(GarbledCircuit *gc, block *inputLabels, block *outputMap,
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
    int q = 100000; /* number of gates */ 
    if (l > 20)
        q = 5000000;
    int r = n + q; /* number of wires */

    GarblingContext gcContext;
	createEmptyGarbledCircuit(gc, n, m, q, r);
	startBuilding(gc, &gcContext);

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
            memcpy(coreInputWires + p, &a[(i-1)*2], sizeof(int) * 2);
            p += 2;
            memcpy(coreInputWires + p, &b[(j-1)*2], sizeof(int) * 2);
            p += 2;
            assert(p == core_n);

            addLevenshteinCoreCircuit(gc, &gcContext, l, coreInputWires, coreOutputWires);
            /*printf("coreInputWires: (i=%d,j=%d) (%d %d) (%d %d) (%d %d) (%d %d) (%d %d) -> (%d %d)\n",*/
                    /*i,*/
                    /*j,*/
                    /*coreInputWires[0],*/
                    /*coreInputWires[1],*/
                    /*coreInputWires[2],*/
                    /*coreInputWires[3],*/
                    /*coreInputWires[4],*/
                    /*coreInputWires[5],*/
                    /*coreInputWires[6],*/
                    /*coreInputWires[7],*/
                    /*coreInputWires[8],*/
                    /*coreInputWires[9],*/
                    /*coreOutputWires[0],*/
                    /*coreOutputWires[1]);*/

            /* Save coreOutputWires to D[i][j] */
            memcpy(D[i][j], coreOutputWires, sizeof(int) * DIntSize);
        }
    }
    memcpy(outputWires, D[l][l], sizeof(int) * DIntSize);
    finishBuilding(gc, outputWires);
    for (int i = 0; i < l+1; i++)
        for (int j = 0; j < l+1; j++)
            free(D[i][j]);
    free(inputWires);
    free(coreInputWires);
    free(coreOutputWires);
    /* removeGarblingContext(&gcContext); */
}

static int
TCircuit(GarbledCircuit *gc, GarblingContext *gctxt, int *inp0, int *inp1, int ninputs)
{
    /* Perfroms "T" which equal 1 if and only if inp0 == inp1 */
    /* returns the output wire */
    assert(ninputs = 4 && "doesnt support other alphabet sizes.. yet");
    //int split = ninputs / 2;

    int xor_output[2];
    xor_output[0] = getNextWire(gctxt);
    xor_output[1] = getNextWire(gctxt);

    int T_output = getNextWire(gctxt);
    XORGate(gc, gctxt, inp0[0], inp1[0], xor_output[0]);
    XORGate(gc, gctxt, inp0[1], inp1[1], xor_output[1]);
    ORGate(gc, gctxt, xor_output[0], xor_output[1], T_output);
    return T_output;
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
    memcpy(symbol0, inputWires + (3*DIntSize), sizeof(int) * 2);
    memcpy(symbol1, inputWires + (3*DIntSize) + 2, sizeof(int) * 2);

    /* First MIN circuit 
     * D_minus_same, D_same_minus MIN circuit */
    int *min_inputs = allocate_ints(2*DIntSize);
    // Switching the order of these changes the output of min1
    memcpy(min_inputs + 0, D_minus_same, sizeof(int) * DIntSize);
    memcpy(min_inputs + DIntSize, D_same_minus, sizeof(int) * DIntSize);

    int *min_outputs = allocate_ints(DIntSize+1); /* will be filled by MINCircuit */
    MINCircuit(gc, gcContext, 2*DIntSize, min_inputs, min_outputs);

    /* Second MIN Circuit 
     * Uses input from first min cricuit and D_minus_minus */
    memcpy(min_inputs, D_minus_minus, sizeof(int) * DIntSize);
    memcpy(min_inputs + DIntSize, min_outputs, sizeof(int) * DIntSize);

    int *min_outputs2 = allocate_ints(DIntSize+1); /* will be filled by MINCircuit */
    MINCircuitWithLEQOutput(gc, gcContext, 2*DIntSize, min_inputs, min_outputs2); 

    /* T */
    int T_output = TCircuit(gc, gcContext, symbol0, symbol1, 4);

    //int xor_output[2];
    //xor_output[0] = getNextWire(gcContext);
    //xor_output[1] = getNextWire(gcContext);
    //int T_output = getNextWire(gcContext);
    //XORGate(gc, gcContext, symbol0[0], symbol1[0], xor_output[0]);
    //XORGate(gc, gcContext, symbol0[1], symbol1[1], xor_output[1]);
    //ORGate(gc, gcContext, xor_output[0], xor_output[1], T_output);
    printf("T_output: %d\n", T_output);

    /* 2-1 MUX */
    int mux_switch = getNextWire(gcContext);
    NOTGate(gc, gcContext, min_outputs2[DIntSize], mux_switch);
    int mux_input[2];
    mux_input[1] = T_output;
    int fixed_one_wire = fixedOneWire(gc, gcContext);
    mux_input[0] = fixed_one_wire;
    int mux_output;
    MUX21Circuit(gc, gcContext, mux_switch, mux_input[0], mux_input[1], &mux_output);

    /* AddOneBit AKA Inc*/
    (void) fixedZeroWire(gc, gcContext);
    int *add_inputs = allocate_ints(DIntSize);
    int *add_outputs = allocate_ints(DIntSize);
    memcpy(add_inputs, min_outputs2, sizeof(int) * DIntSize);
    INCCircuit(gc, gcContext, DIntSize, add_inputs, add_outputs);

    /* Final MUX (not in paper) */
    /* Final Mux between INCed value and orig value, with switch 
     * from the mux_output */
    int *final = allocate_ints(DIntSize);
    for (int i = 0; i < DIntSize; i++) {
        /* Do a 2-1 mux for each wire */
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
    printf("final[1] = %d\n", final[1]);

    //memcpy(outputWires, final, sizeof(int) * DIntSize);
    countToN(outputWires, 76);
    
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
	int lesOutput;
	int notOutput = getNextWire(garblingContext);
	LESCircuit(gc, garblingContext, n, inputs, &lesOutput);
	NOTGate(gc, garblingContext, lesOutput, notOutput);
    int split = n / 2;
	for (i = 0; i < split; i++)
        MUX21Circuit(gc, garblingContext, lesOutput, inputs[i], inputs[split + i], outputs+i);

    outputs[split] = lesOutput;
	return 0;
}

void
buildANDCircuit(GarbledCircuit *gc, int n, int nlayers)
{
    GarblingContext ctxt;
    int wire;
    int wires[n];
    int r = n + n / 2 * nlayers;
    int q = n / 2 * nlayers;

    countToN(wires, n);

    createEmptyGarbledCircuit(gc, n, n, q, r);
    startBuilding(gc, &ctxt);

    for (int i = 0; i < nlayers; ++i) {
        for (int j = 0; j < n; j += 2) {
            wire = getNextWire(&ctxt);
            ANDGate(gc, &ctxt, wires[j], wires[j+1], wire);
            wires[j] = wires[j+1] = wire;
        }
    }

    finishBuilding(gc, wires);
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

	createEmptyGarbledCircuit(gc, n, m, q, r);
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
	finishBuilding(gc, outputWires);
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

	GarblingContext gc_context;

	createEmptyGarbledCircuit(gc, n, m, q, r);
	startBuilding(gc, &gc_context);
	ADD22Circuit(gc, &gc_context, inputs, outputs);
	finishBuilding(gc, outputs);

    free(inputs);
    free(outputs);
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

	createEmptyGarbledCircuit(gc, n, m, q, r);
	startBuilding(gc, &garblingContext);
    XORCircuit(gc, &garblingContext, 256, inp, outs);
	finishBuilding(gc, outs);
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

	createEmptyGarbledCircuit(gc, n, m, q, r);
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
	    finishBuilding(gc, mixColumnOutputs);
    } else {
	    finishBuilding(gc, shiftRowsOutputs);
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
	int i;

	createEmptyGarbledCircuit(gc, n, m, q, r);
	startBuilding(gc, &garblingContext);

	countToN(addKeyInputs, 256);

	for (int round = 0; round < roundLimit; round++) {

		AddRoundKey(gc, &garblingContext, addKeyInputs,
                    addKeyOutputs);

		for (i = 0; i < 16; i++) {
			SubBytes(gc, &garblingContext, addKeyOutputs + 8 * i,
                     subBytesOutputs + 8 * i);
		}

		ShiftRows(gc, &garblingContext, subBytesOutputs, shiftRowsOutputs);

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
	finishBuilding(gc, final);
}

