#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

int 
chainedEvaluate(GarbledCircuit *gcs, int num_gcs, Instruction* instructions, int num_instr, 
        InputLabels* inputLabels, block* receivedOutputMap, 
        int* inputs[], int* output) {

    Instructions iss;
    iss.instr = instructions;
    iss.size = num_instr;
    print_instructions(&iss);

    block** labels = malloc(sizeof(block*) * num_gcs);
    block** computedOutputMap = malloc(sizeof(block*) * num_gcs);

    for (int i=0; i<num_gcs; i++) {
        labels[i] = memalign(128, sizeof(block)* gcs[i].n);
        computedOutputMap[i] = memalign(128, sizeof(block) * gcs[i].m);
        assert(labels[i] && computedOutputMap[i]);
    }

    // This step would normally happen with OT
    extractLabels(labels[0], inputLabels[0], inputs[0], gcs[0].n);
    extractLabels(labels[1], inputLabels[1], inputs[1], gcs[1].n);

    for (int i=0; i<num_instr; i++) {
        Instruction* cur = &instructions[i];
        switch(cur->type) {
            case EVAL:
                printf("type %d is EVALuating circuit %d\n", i, cur->evCircId);
                evaluate(&gcs[cur->evCircId], labels[cur->evCircId], computedOutputMap[cur->evCircId]);

                if (i == num_instr - 1) {
                    printf("mapping\n");
	                mapOutputs(receivedOutputMap, computedOutputMap[2], output, gcs[2].m);
                }

                break;
            case CHAIN:
                // problem in here
                printf("type %d is CHAINing circuit %d to circuit %d\n", i, 
                        cur->chFromCircId, cur->chToCircId);

                labels[cur->chToCircId][cur->chToWireId] = xorBlocks(
                        computedOutputMap[cur->chFromCircId][cur->chFromWireId], 
                        cur->chOffset);
                break;
            default:
                printf("Error\n");
                printf("type %d not a valid type\n", i);
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


