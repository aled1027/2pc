#ifndef MPC_GARBLED_CIRCUIT_H
#define MPC_GARBLED_CIRCUIT_H

#include "components.h"

#include <stdbool.h>

/* Our abstraction/layer on top of GarbledCircuit */
typedef enum {
    CHAINING_TYPE_STANDARD,
    CHAINING_TYPE_SIMD,
} ChainingType;

typedef struct {
    // the iblock_map is needed so that make_real_instructions knows which 
    // input_block to use. 
    int num_iblocks;
    int *iblock_map; // maps input_idx \to input_blocks_idx. size is equal to gc.n 
    block *input_blocks;
    block output_block;
} SimdInformation;

typedef struct {
    garble_circuit gc;
    int id;
    CircuitType type;
    block *inputLabels;
    block *outputMap;
    block *offlineChainingOffsets; /* for SIMD chaining operation */
    SimdInformation simd_info;
} ChainedGarbledCircuit; 

int generateOfflineChainingOffsets(ChainedGarbledCircuit *cgc);

int
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc, bool isGarb, ChainingType chainingType);

int
saveChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, bool isGarbler,
              ChainingType chainingType);
int loadChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, int id,
                  bool isGarbler, ChainingType chainingType);

void freeChainedGcs(ChainedGarbledCircuit* chained_gcs, int num);

int saveOutputMap(char *fname, block *labels, int nlabels);
int loadOutputMap(char *fname, block* labels);
int saveOTLabels(char *fname, block *labels, int n, bool isSender);
block *loadOTLabels(char *fname);
int saveOTSelections(char *fname, int *selections, int n);
int *loadOTSelections(char *fname);


void createSIMDInputLabelsWithR(ChainedGarbledCircuit *cgc, block R);
void 
createSIMDInputLabelsWithRForLeven(ChainedGarbledCircuit *cgc, block R, int l);
#endif
