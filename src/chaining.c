#include "chaining.h"

int
go() {
    ChainedGarbledCircuit cgc;
    cgc.id = 10;
    return 2;
}

int 
createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n) {

    block delta = randomBlock();

    for (int i = 0; i < n; i++) {
        buildAdderCircuit(&(chained_gcs[i].gc));
        chained_gcs[i].id = i;
        chained_gcs[i].type = ADDER22;

        // TODO this memory needs to be freed
        // probably want to restructure so it's not a weird, nested malloc
        chained_gcs[i].inputLabels = (block *) memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.n );
        chained_gcs[i].outputMap = (block *) memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.m);

        // TODO check that memory was allocated
        garbleCircuit(&(chained_gcs[i].gc), chained_gcs[i].inputLabels, chained_gcs[i].outputMap, &delta);
    }

    return 0;
}

int
createChainingMap(ChainingMap* c_map, ChainedGarbledCircuit* chained_gcs, int num_maps) {
    /// num_maps = number of wire pairs that need to be connected
    /// num_maps should be considered an upper bound: is used for memory allocation.
    ///
    /// We are assuming the funciton has been chosen by the garbler at this point.
    
    // Hardcode for now: assume doing same old computation
    c_map[0].from_gc_id = 0;
    c_map[0].from_output_idx = 0;
    c_map[0].to_gc_id = 2;
    c_map[0].to_input_idx = 0;
    c_map[0].offset = xorBlocks(chained_gcs[0].outputMap[   c_map[0].from_output_idx ],
                                chained_gcs[2].inputLabels[ c_map[0].to_input_idx ]);

    c_map[1].from_gc_id = 1;
    c_map[1].from_output_idx = 0;
    c_map[1].to_gc_id = 2;
    c_map[1].to_input_idx = 2;
    c_map[1].offset = xorBlocks(chained_gcs[1].outputMap[   c_map[1].from_output_idx ],
                                chained_gcs[2].inputLabels[ c_map[1].to_input_idx ]);

    return 0;
}

int 
chainedEvaluate(GarbledCircuit *gcs, ChainingMap* c_map, int c_map_size, InputLabels* labels) {
    // would need to communicate which circuits to complete first
    // send some directions - like cooking
    
    int m = 2;
    block outputMap[10][m];
    
    // 1. compute necessary circuits
    // assume relevant circuits to compute fully right now are 0,1
    evaluate(&gcs[0], labels[0], outputMap[0]);
    evaluate(&gcs[1], labels[1], outputMap[1]);

    // 2. now follow directions from garbler and compute other circuits

    return 0;
}
