#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "components.h"
#include "utils.h"

int NUM_AES_ROUNDS = 10;
int NUM_CBC_BLOCKS = 10;

static int getNumXORCircs() { return NUM_CBC_BLOCKS; }
static int getNumAESCircs() { return (NUM_AES_ROUNDS-1) * NUM_CBC_BLOCKS; }
static int getNumFinalAESCircs() { return NUM_CBC_BLOCKS; }

int cbcNumGarbInputs() { return (NUM_AES_ROUNDS * NUM_CBC_BLOCKS * 128) + 128; }
int cbcNumEvalInputs() { return NUM_CBC_BLOCKS * 128; }
int cbcNumCircs() { return getNumXORCircs() + getNumAESCircs() + getNumFinalAESCircs();}
int cbcNumOutputs() { return NUM_CBC_BLOCKS * 128; }

static int cbcNumGates() {
    int gates_per_xor = 128;
    int gates_per_aes = 3904;
    int gates_per_aes_final = 3904; // TODO this number is off. should be less than aes round

    return getNumXORCircs() * gates_per_xor + getNumAESCircs() * gates_per_aes + 
        getNumFinalAESCircs() * gates_per_aes_final;
}

void cbc_garb_off(char *dir, ChainingType chainingType)
{
    printf("Running cbc garb offline\n");
    int num_chained_gcs = cbcNumCircs(); 
    int num_xor_circs = getNumXORCircs();
    int num_aes_circs = getNumAESCircs();
    ChainedGarbledCircuit *chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < num_chained_gcs; i++) {
        GarbledCircuit* gc = &(chained_gcs[i].gc);
        if (i < num_xor_circs) {
            buildXORCircuit(gc, &delta);
            chained_gcs[i].type = XOR;
        } else if (i < num_xor_circs + num_aes_circs) {
            buildAESRoundComponentCircuit(gc, false, &delta);
            chained_gcs[i].type = AES_ROUND;
        } else {
            buildAESRoundComponentCircuit(gc, true, &delta);
            chained_gcs[i].type = AES_FINAL_ROUND;

        }
        chained_gcs[i].id = i;
        chained_gcs[i].inputLabels = allocate_blocks(2 * gc->n);
        chained_gcs[i].outputMap = allocate_blocks(2 * gc->m);

        if (chainingType == CHAINING_TYPE_SIMD) {
            createSIMDInputLabelsWithR(&chained_gcs[i], delta);
        } else { 
            createInputLabelsWithR(chained_gcs[i].inputLabels, gc->n, delta);
        }
        garbleCircuit(gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap,
                      GARBLE_TYPE_STANDARD);
    }

    if (chainingType == CHAINING_TYPE_SIMD) {
        for (int i = 0; i < num_chained_gcs; i++)
            generateOfflineChainingOffsets(&chained_gcs[i]);
    }

    garbler_offline(dir, chained_gcs, cbcNumEvalInputs(), num_chained_gcs, chainingType);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], true, chainingType);
    }
    free(chained_gcs);
}
