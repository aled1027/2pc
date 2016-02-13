#include "components.h"
#include "utils.h"

#include "garble.h"
#include "circuits.h"
#include "gates.h"

#include <string.h>
#include <assert.h>
#include <math.h>

bool isFinalCircuitType(CircuitType type) 
{
    if (type == AES_FINAL_ROUND) 
        return true;
    return false;
}

void
buildLevenshteinCircuit(GarbledCircuit *gc, int l, int sigma)
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

    int n = inputsDevotedToD + (2 * sigma * l);
    int m = DIntSize;
    int core_n = (3 * DIntSize) + (2 * sigma);
    int q = 100000; /* number of gates */ 
    if (l > 20) {
        q = 5000000;
    }
    int r = n + q; /* number of wires */
    int inputWires[n];
    countToN(inputWires, n);

    GarblingContext gctxt;
	createEmptyGarbledCircuit(gc, n, m, q, r);
	startBuilding(gc, &gctxt);

    ///* Populate D's 0th row and column with wire indices from inputs*/
    int D[l+1][l+1][DIntSize];
    for (int i = 0; i < l+1; i++) {
        for (int j = 0; j < l+1; j++) {
            //D[i][j] = allocate_ints(DIntSize);
            if (i == 0) {
                memcpy(D[0][j], inputWires + (j*DIntSize), sizeof(int) * DIntSize);
            } else if (j == 0) {
                memcpy(D[i][0], inputWires + (i*DIntSize), sizeof(int) * DIntSize);
            }
        }
    }

    /* Populate a and b (the input strings) wire indices */
    int a[l * sigma];
    int b[l * sigma];
    memcpy(a, inputWires + inputsDevotedToD, l * sigma * sizeof(int));
    memcpy(b, inputWires + inputsDevotedToD + (l * sigma), l * sigma * sizeof(int));

    /* dynamically add levenshtein core circuits */
    for (int i = 1; i < l + 1; i++) {
        for (int j = 1; j < l + 1; j++) {
            int coreInputWires[core_n];
            int p = 0;
            memcpy(coreInputWires + p, D[i-1][j-1], sizeof(int) * DIntSize);
            p += DIntSize;
            memcpy(coreInputWires + p, D[i][j-1], sizeof(int) * DIntSize);
            p += DIntSize;
            memcpy(coreInputWires + p, D[i-1][j], sizeof(int) * DIntSize);
            p += DIntSize;
            memcpy(coreInputWires + p, &a[(i-1)*sigma], sizeof(int) * sigma);
            p += sigma;
            memcpy(coreInputWires + p, &b[(j-1)*sigma], sizeof(int) * sigma);
            p += sigma;
            assert(p == core_n);

        
            int coreOutputWires[DIntSize];
            addLevenshteinCoreCircuit(gc, &gctxt, l, sigma, coreInputWires, coreOutputWires);
            printf("coreInputWires: (i=%d,j=%d) (%d %d) (%d %d) (%d %d) (%d %d) (%d %d) -> (%d %d)\n",
                  i,
                  j,
                  coreInputWires[0],
                  coreInputWires[1],
                  coreInputWires[2],
                  coreInputWires[3],
                  coreInputWires[4],
                  coreInputWires[5],
                  coreInputWires[6],
                  coreInputWires[7],
                  coreInputWires[8],
                  coreInputWires[9],
                  coreOutputWires[0],
                  coreOutputWires[1]);

            // Save coreOutputWires to D[i][j] 
            memcpy(D[i][j], coreOutputWires, sizeof(int) * DIntSize);
        }
    }
    int output_wires[m];
    memcpy(output_wires, D[l][l], sizeof(int) * DIntSize);
    finishBuilding(gc, output_wires);
}

int 
INCCircuitWithSwitch(GarbledCircuit *gc, GarblingContext *ctxt,
		int the_switch, int n, int *inputs, int *outputs) {
    /* n does not include the switch. The size of the number */

	for (int i = 0; i < n; i++) {
		outputs[i] = getNextWire(ctxt);
    }
    int carry = getNextWire(ctxt);
    XORGate(gc, ctxt, the_switch, inputs[0], outputs[0]);
    ANDGate(gc, ctxt, the_switch, inputs[0], carry);

    /* do first case */
    int not_switch = getNextWire(ctxt);
    NOTGate(gc, ctxt, the_switch, not_switch);
    for (int i = 1; i < n; i++) {
        /* cout and(xor(x,c),s) */
        int xor_out = getNextWire(ctxt);
        int and0 = getNextWire(ctxt);
        int and1 = getNextWire(ctxt);
        XORGate(gc, ctxt, carry, inputs[i], xor_out);
        ANDGate(gc, ctxt, xor_out, the_switch, and0);

        ANDGate(gc, ctxt, not_switch, inputs[i], and1);
        ORGate(gc, ctxt, and0, and1, outputs[i]);
        
        /* carry and(nand(c,x),s)*/
        int and_out = getNextWire(ctxt);
        ANDGate(gc, ctxt, carry, inputs[i], and_out);
        carry = getNextWire(ctxt);
        ANDGate(gc, ctxt, and_out, the_switch, carry);
    }
    return 0;
}


static void
bitwiseMUX(GarbledCircuit *gc, GarblingContext *gctxt, int the_switch, const int *inps, 
        int ninputs, int *outputs)
{
    //assert(ninputs % 2 == 0 && "ninputs should be even because we are muxing");
    //assert(outputs && "final should already be malloced");
    int split = ninputs / 2;

    for (int i = 0; i < split; i++) {
        int res = MUX21Circuit(gc, gctxt, the_switch, inps[i], inps[i + split], &outputs[i]);
        if (res == FAILURE) {
            fprintf(stderr, "unable to do mux21\n");
        }
    }
}

static int
TCircuit(GarbledCircuit *gc, GarblingContext *gctxt, const int *inp0, const int *inp1, int ninputs)
{
    /* Perfroms "T" which equal 1 if and only if inp0 == inp1 */
    /* returns the output wire */
    assert(ninputs % 2 == 0 && "doesnt support other alphabet sizes.. yet");
    int split = ninputs / 2;
    int xor_output[split];

    for (int i = 0; i < split; i++) {
        xor_output[i] = getNextWire(gctxt);
        XORGate(gc, gctxt, inp0[i], inp1[i], xor_output[i]);
    }
    int T_output;
    ORCircuit(gc, gctxt, split, xor_output, &T_output);
    return T_output;
}

void
addLevenshteinCoreCircuit(GarbledCircuit *gc, GarblingContext *gctxt, 
        int l, int sigma, int *inputWires, int *outputWires) 
{
    int DIntSize = (int) floor(log2(l)) + 1;
    int D_minus_minus[DIntSize]; /*D[i-1][j-1] */
    int D_minus_same[DIntSize]; /* D[1-1][j] */
    int D_same_minus[DIntSize]; /* D[i][j-1] */
    int symbol0[sigma];
    int symbol1[sigma];

    /* arrayPopulateRange is inclusive on start and exclusive on end */
    memcpy(D_minus_minus, inputWires, sizeof(int) * DIntSize);
    memcpy(D_minus_same, inputWires + DIntSize, sizeof(int) * DIntSize);
    memcpy(D_same_minus, inputWires + 2*DIntSize, sizeof(int) * DIntSize);
    memcpy(symbol0, inputWires + (3*DIntSize), sizeof(int) * sigma);
    memcpy(symbol1, inputWires + (3*DIntSize) + sigma, sizeof(int) * sigma);

    /* First MIN circuit :MIN(D_minus_same, D_same_minus) */
    int min_inputs[2 * DIntSize];
    memcpy(min_inputs + 0, D_minus_same, sizeof(int) * DIntSize);
    memcpy(min_inputs + DIntSize, D_same_minus, sizeof(int) * DIntSize);
    int min_outputs[DIntSize]; 
    MINCircuit(gc, gctxt, 2 * DIntSize, min_inputs, min_outputs);

    /* Second MIN Circuit: uses input from first min cricuit and D_minus_minus */
    memcpy(min_inputs, min_outputs, sizeof(int) * DIntSize);
    memcpy(min_inputs + DIntSize,  D_minus_minus, sizeof(int) * DIntSize);
    int min_outputs2[DIntSize + 1]; 
    MINCircuitWithLEQOutput(gc, gctxt, 2 * DIntSize, min_inputs, min_outputs2); 

    int T_output = TCircuit(gc, gctxt, symbol0, symbol1, 2*sigma);

    /* 2-1 MUX(switch = determined by secon min, 1, T)*/
    int mux_switch = min_outputs2[DIntSize];

    int fixed_one_wire = fixedOneWire(gc, gctxt);

    int mux_output;
    MUX21Circuit(gc, gctxt, mux_switch, fixed_one_wire, T_output, &mux_output);

    int final[DIntSize];
    INCCircuitWithSwitch(gc, gctxt, mux_output, DIntSize, min_outputs2, final);

    memcpy(outputWires, final, sizeof(int) * DIntSize);
}

int myLEQCircuit(GarbledCircuit *gc, GarblingContext *ctxt, int n,
        int *inputs, int *outputs)
{

	int les, eq;
    int ret = getNextWire(ctxt);

	LESCircuit(gc, ctxt, n, inputs, &les);
    EQUCircuit(gc, ctxt, n, inputs, &eq);
    ORGate(gc, ctxt, les, eq, ret);
    outputs[0] = ret;
    return 0;
}

int MINCircuitWithLEQOutput(GarbledCircuit *gc, GarblingContext *garblingContext, int n,
		int* inputs, int* outputs) 
{
    /* Essentially copied from JustGarble/src/circuits.c.
     * Different from their MIN circuit because it has output size equal to n/2 + 1
     * where that last bit indicates the output of the LEQ circuit,
     * e.g. outputs[2/n + 1] = 1 iff input0 <= input1
     */
    int i;
	int lesOutput;
	int notOutput = getNextWire(garblingContext);
    myLEQCircuit(gc, garblingContext, n, inputs, &lesOutput);
	NOTGate(gc, garblingContext, lesOutput, notOutput);
    int split = n / 2;
	for (i = 0; i < split; i++) {
        MUX21Circuit(gc, garblingContext, lesOutput, inputs[i], inputs[split + i], outputs+i);
    }

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

    if (!isFinalRound) { 
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

        if (round != roundLimit - 1) {
		    for (i = 0; i < 4; i++) {
				MixColumns(gc, &garblingContext,
                           shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
            }
		}

		for (i = 0; i < 128; i++) {
			addKeyInputs[i] = mixColumnOutputs[i];
			addKeyInputs[i + 128] = (round + 2) * 128 + i;
		}
	}
	final = shiftRowsOutputs;
	finishBuilding(gc, final);
}

