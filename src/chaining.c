#include "chaining.h"

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
createInstructions(Instruction* instr, ChainedGarbledCircuit* chained_gcs) {
    // Input normally would require OT, but in our case only requires extractLabels
    // on other end, call extractLabels(extractedLabels[0], chained_gcs[0].inputLabels, inputs[0], n);
    instr[0].instruction = INPUT;
    instr[0].inCircId = 0;
    // malloc space
    instr[0].inInputLabels = chained_gcs[0].inputLabels;
    
    instr[1].instruction = INPUT;
    instr[1].inCircId = 1;
    // malloc space
    instr[1].inInputLabels = chained_gcs[1].inputLabels;

    instr[2].instruction = EVAL;
    instr[2].evCircId = 0;
    
    instr[3].instruction = EVAL;
    instr[3].evCircId = 1;

    instr[4].instruction = CHAIN;
    instr[4].chFromCircId = 0;
    instr[4].chFromWireId = 0;
    instr[4].chToCircId = 2;
    instr[4].chToWireId = 0;
    instr[4].chOffset = xorBlocks(chained_gcs[0].outputMap[0], chained_gcs[2].inputLabels[0]);

    instr[5].instruction = CHAIN;
    instr[5].chFromCircId = 1;
    instr[5].chFromWireId = 0;
    instr[5].chToCircId = 2;
    instr[5].chToWireId = 1;
    instr[5].chOffset = xorBlocks(chained_gcs[1].outputMap[0], chained_gcs[2].inputLabels[2]);

    instr[6].instruction = EVAL;
    instr[6].evCircId = 2;
    return 0;
}

int 
chainedEvaluate(GarbledCircuit *gcs, int num_gcs, Instruction* instructions, int num_instr, 
        InputLabels* inputLabels, block* receivedOutputMap, 
        int* inputs[], int* output) {

    block** labels = malloc(sizeof(block*) * num_gcs);
    block** computedOutputMap = malloc(sizeof(block*) * num_gcs);

    for (int i=0; i<num_gcs; i++) {
        labels[i] = memalign(128, sizeof(block)* gcs[i].n);
        computedOutputMap[i] = memalign(128, sizeof(block) * gcs[i].m);
        assert(labels[i] && computedOutputMap[i]);
    }

    for (int i=0; i<num_instr; i++) {
        Instruction* cur = &instructions[i];
        switch(cur->instruction) {
            case INPUT:
                // TODO figure this out later
                printf("instruction %d is INPUTting into circuit %d\n", i, cur->inCircId);
                if (cur->inCircId == 0) {
                    extractLabels(labels[0], inputLabels[0], inputs[0], gcs[0].n);

                } else if (cur->inCircId == 1) {
                    extractLabels(labels[1], inputLabels[1], inputs[1], gcs[1].n);
                }
                break;

            case REQUEST_INPUT:
                printf("instruction %d is REQUEST_INPUT\n", i);
                break;
            case EVAL:
                printf("instruction %d is EVALuating circuit %d\n", i, cur->evCircId);
                evaluate(&gcs[cur->evCircId], labels[cur->evCircId], computedOutputMap[cur->evCircId]);

                if (i == num_instr - 1) {
                    printf("mapping\n");
	                mapOutputs(receivedOutputMap, computedOutputMap[2], output, gcs[2].m);
                }

                break;
            case CHAIN:
                // problem in here
                printf("instruction %d is CHAINing circuit %d to circuit %d\n", i, 
                        cur->chFromCircId, cur->chToCircId);

                labels[cur->chToCircId][cur->chToWireId] = xorBlocks(
                        computedOutputMap[cur->chFromCircId][cur->chFromWireId], 
                        cur->chOffset);
                break;
            default:
                printf("Error\n");
                printf("Instruction %d not a valid instruction\n", i);
                return FAILURE;
        }
    }

    for (int i=0; i<num_gcs; i++) {
        free(computedOutputMap[i]);
        free(labels[i]);
    } 
    free(computedOutputMap);
    free(labels);

    return SUCCESS;
}

int
createChainingMap(ChainingMap* c_map, ChainedGarbledCircuit* chained_gcs) {
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
chainedEvaluateOld(GarbledCircuit *gcs, int num_gcs, ChainingMap* c_map, int c_map_size, InputLabels* labels, block* receivedOutputMap, int* output) {
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
