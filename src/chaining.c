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
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < n; i++) {
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        buildAdderCircuit(p_gc);

        chained_gcs[i].id = i;
        chained_gcs[i].type = ADDER22;

        // TODO this memory needs to be freed
        // probably want to restructure so it's not a weird, nested malloc
        chained_gcs[i].inputLabels = memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.n );
        chained_gcs[i].outputMap = memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.m);

        assert(chained_gcs[i].inputLabels != NULL && chained_gcs[i].outputMap != NULL);

        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap, &delta);
    }
    //printf("break here\n");

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
    c_map[1].to_input_idx = 1;
    c_map[1].offset = xorBlocks(chained_gcs[1].outputMap[0],
                                chained_gcs[2].inputLabels[2]);

    return 0;
}

int 
chainedEvaluate(GarbledCircuit *gcs, int num_gcs, ChainingMap* c_map, int c_map_size, InputLabels* labels, block* receivedOutputMap, int* output) {
    /*
     * GarbledCircuit* gcs is an array of GarbledCircuits
     * ChainingMap* c_map is an array of ChainingMaps
     * InputLabels* labels is an array of InputLabels. 
     *      These labels are received via OT, if working over network.
     * block* receivedOutputMap is the outputMap needed to interpret the output 
     *      of the evaluation of the final block.
     */
    //block* computedOutputMap[3];
    block** computedOutputMap = malloc(sizeof(block*) * num_gcs);
    for (int i=0; i<num_gcs; i++) {
        computedOutputMap[i] = memalign(128, sizeof(block) * gcs[i].m);
    }
    //computedOutputMap[0] = memalign(128, sizeof(block)*2);
    //computedOutputMap[1] = memalign(128, sizeof(block)*2);
    //computedOutputMap[2] = memalign(128, sizeof(block)*2);

    // 1. evaluate necessary circuits
    // assume relevant circuits to compute fully right now are 0,1
    evaluate(&gcs[0], labels[0], computedOutputMap[0]);
    evaluate(&gcs[1], labels[1], computedOutputMap[1]);

    // 2. now follow directions from garbler (sent via c_map) and compute other circuits
    for (int i=0; i<c_map_size; i++) {
        int from_id = c_map[i].from_gc_id;
        int from_idx = c_map[i].from_output_idx;
        int to_id = c_map[i].to_gc_id;
        int to_idx = c_map[i].to_input_idx;
        block* offset = &(c_map[i].offset);

        labels[to_id][to_idx] = xorBlocks(computedOutputMap[from_id][from_idx], *offset);
    }
    
    // Post processing:
    evaluate(&gcs[2], labels[2], computedOutputMap[2]);
	mapOutputs(receivedOutputMap, computedOutputMap[2], output, gcs[2].m);

    for (int i=0; i<num_gcs; i++) 
        free(computedOutputMap[i]);
    free(computedOutputMap);


    return 0;
}


int freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc) {
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    free(chained_gc->inputLabels);
    free(chained_gc->outputMap);
    // TODO free other things
    return 0;
}
