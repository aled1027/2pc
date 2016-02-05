#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "2pc_garbler.h"
#include "components.h"

#include "utils.h"

int aesNumGarbInputs() { return 1280; }
int aesNumEvalInputs() { return 128; }
int aesNumCircs() { return 10; }
int aesNumOutputs() { return 128; }
/* static unsigned long NUM_GATES = 34000; */

void
aes_garb_off(char *dir, int nchains, ChainingType chainingType)
{
    printf("Running garb offline\n");
    ChainedGarbledCircuit *chained_gcs = calloc(nchains, sizeof(ChainedGarbledCircuit));

    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < nchains; i++) {
        GarbledCircuit* gc = &chained_gcs[i].gc;
        if (i == nchains - 1) {
            buildAESRoundComponentCircuit(gc, true, &delta);
            chained_gcs[i].type = AES_FINAL_ROUND;
        } else {
            buildAESRoundComponentCircuit(gc, false, &delta);
            chained_gcs[i].type = AES_ROUND;
        }
        chained_gcs[i].id = i;
        chained_gcs[i].inputLabels = allocate_blocks(2 * gc->n);
        chained_gcs[i].outputMap = allocate_blocks(2 * gc->m);

        if (chainingType == CHAINING_TYPE_SIMD)
            createSIMDInputLabelsWithR(&chained_gcs[i], delta);
        else 
            createInputLabelsWithR(chained_gcs[i].inputLabels, gc->n, delta);

        garbleCircuit(gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap,
                      GARBLE_TYPE_STANDARD);

    }

    if (chainingType == CHAINING_TYPE_SIMD) {
        for (int i = 0; i < nchains; i++) {
            printf("break here\n");
            generateOfflineChainingOffsets(&chained_gcs[i]);
        }
    }

    garbler_offline(dir, chained_gcs, aesNumEvalInputs(), nchains, chainingType);
    for (int i = 0; i < nchains; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], true, chainingType);
    }
    free(chained_gcs);
}
