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

/* typedef struct {  */
/*     /\* index is i \in {0, ... , num_gcs-1}  */
/*      * so gc with index i has properties gc_types[i], gc_valids[i], gc_paths[i].  */
/*      *\/ */

/*     int num_gcs; */
/*     CircuitType* gc_types; */
/*     bool* gc_valids; */
/*     char** gc_paths;  */
/* } GCsMetadata; */

void
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc);

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
