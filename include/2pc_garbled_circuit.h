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
    GarbledCircuit gc;
    int id;
    CircuitType type;
    block *inputLabels;
    block *outputMap;
    block *offlineChainingOffsets; /* for SIMD chaining operation */
} ChainedGarbledCircuit; 

int generateOfflineChainingOffsets(ChainedGarbledCircuit *cgc);

int
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc, bool isGarb);

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

#endif
