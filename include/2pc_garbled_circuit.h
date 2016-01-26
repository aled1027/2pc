#ifndef MPC_GARBLED_CIRCUIT_H
#define MPC_GARBLED_CIRCUIT_H

#include "components.h"

#include <stdbool.h>

/* Our abstraction/layer on top of GarbledCircuit */
typedef struct {
    GarbledCircuit gc;
    int id;
    CircuitType type;
    block *inputLabels;
    block *outputMap;
} ChainedGarbledCircuit; 

int
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc, bool isGarb);

int
saveChainedGC(ChainedGarbledCircuit* chained_gc, const char *dir, bool isGarbler);
int loadChainedGC(ChainedGarbledCircuit* chained_gc, char *dir, int id,
                  bool isGarbler);

void freeChainedGcs(ChainedGarbledCircuit* chained_gcs, int num);

int saveOTLabels(char *fname, block *labels, int n, bool isSender);
block *loadOTLabels(char *fname);
int saveOTSelections(char *fname, int *selections, int n);
int *loadOTSelections(char *fname);

void print_garbled_gate(GarbledGate *gg);
void print_garbled_table(GarbledTable *gt);
void print_wire(Wire *w);
void print_gc(GarbledCircuit *gc);

#endif
