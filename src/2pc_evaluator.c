#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h" // USE_TO


int 
chainedEvaluate(GarbledCircuit *gcs, int num_gcs, Instruction* instructions, int num_instr, 
        InputLabels* inputLabels, block* receivedOutputMap, 
        int* inputs[], int* output) {

    printf("------------------starting chained evaluate------------------\n");
    Instructions iss;
    iss.instr = instructions;
    iss.size = num_instr;
    print_instructions(&iss);

    block** labels = malloc(sizeof(block*) * num_gcs);
    block** computedOutputMap = malloc(sizeof(block*) * num_gcs);

    // won't work if ids are all weird.
    for (int i=0; i<num_gcs; i++) {
        labels[i] = memalign(128, sizeof(block)* gcs[i].n);
        computedOutputMap[i] = memalign(128, sizeof(block) * gcs[i].m);
        assert(labels[i] && computedOutputMap[i]);
    }

    // This step would normally happen with OT
    // (labels_to_fill, all labells, inputs, num_inputs)
    //getLabels(labels[0], inputs[0], gcs[0].n, inputLabels[0]);
    //getLabels(labels[1], inputs[1], gcs[1].n, inputLabels[1]);
    extractLabels(labels[0], inputLabels[0], inputs[0], gcs[0].n);
    extractLabels(labels[1], inputLabels[1], inputs[1], gcs[1].n);
    printf("break\n");

    for (int i=0; i<num_instr; i++) {
        Instruction* cur = &instructions[i];
        switch(cur->type) {
            case EVAL:
                printf("instruction %d is EVALuating circuit %d\n", i, cur->evCircId);
                evaluate(&gcs[cur->evCircId], labels[cur->evCircId], computedOutputMap[cur->evCircId]);

                printf("computed outputmap %lld, %lld\n", computedOutputMap[cur->evCircId][0][0], 
                    computedOutputMap[cur->evCircId][0][1]);

                printf("check it\n");
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
                printf("Instruction %d is not of a valid type\n", i);
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
getLabels(block* labels, int* inputs, int n, block* inputLabels) {
    // inputLabels is only used if not using Oblivious Transfer
#ifdef USE_OT
    printf("not yet implemented\n");
    // requires input, num_inputs, lables_to_fill, 
    //ot_np_recv(&state, sockfd, input, gc.n, sizeof(block), 2, inputLabels,
               //my_choice_reader, my_msg_writer);
#else
    if (! inputLabels) {
        printf("No input labels\n");
        return FAILURE;
    }

    extractLabels(labels, inputLabels, inputs, n);
#endif

    return SUCCESS;
}

