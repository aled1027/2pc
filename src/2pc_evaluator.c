#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h" // USE_TO

static void
array_slice(int* dest, int* src, int a, int b) {
    // inclusive on both sides
    for (int i=a; i<=b; i++) {
        dest[i-a] = src[i];
    }
}

int 
chainedEvaluate(GarbledCircuit *gcs, int num_gcs, Instruction* instructions, int num_instr, 
        block* inputLabels, block* receivedOutputMap, 
        int* inputs, int* output, InputMapping *input_mapping) {

    //printf("------------------starting chained evaluate------------------\n");
    // TODO improve this. can reduce number of input parameters
    Instructions iss;
    iss.instr = instructions;
    iss.size = num_instr;

    block** labels = malloc(sizeof(block*) * num_gcs);
    block** computedOutputMap = malloc(sizeof(block*) * num_gcs);

    // TODO won't work if ids are all weird.
    for (int i=0; i<num_gcs; i++) {
        labels[i] = memalign(128, sizeof(block)* gcs[i].n);
        computedOutputMap[i] = memalign(128, sizeof(block) * gcs[i].m);
        assert(labels[i] && computedOutputMap[i]);
    }

    // TODO next time: hook up input grab with OT (goto getLabels function)
    // TODO next time: clean up chainedEvaluate parmaters
    getLabels(labels, inputs, 4, inputLabels, input_mapping); // uses OT if necessary

    for (int i=0; i<num_instr; i++) {
        Instruction* cur = &instructions[i];
        switch(cur->type) {
            case EVAL:
                evaluate(&gcs[cur->evCircId], labels[cur->evCircId], computedOutputMap[cur->evCircId]);
                if (i == num_instr - 1) {
	                mapOutputs(receivedOutputMap, computedOutputMap[2], output, gcs[2].m);
                }
                break;
            case CHAIN:
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
getLabels(block** labels, int* eval_inputs, int eval_num_inputs, 
        block* input_labels, InputMapping *input_mapping) 
{
    /* labels: destination of grabbed labels. Memory should already be allocated 
     * eval_inputs: evaluator's inputs.
     * eval_num_inputs: number of evaluators inputs.
     * inputLabels: only used if not using OT. Garbler sends over all labels, 
     *  and we just grab the ones we need
     *
     *  add OT side
     */

    block* raw_labels = memalign(128, sizeof(block) * eval_num_inputs);

#ifdef USE_OT
    printf("break here\n");
    int sockfd, len;
    struct state state;
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    ot_np_recv(&state, sockfd, eval_inputs, eval_num_inputs, sizeof(block), 2, raw_labels,
               new_choice_reader, new_msg_writer);

    // get outputmap
    //net_recv(sockfd, outputMap, sizeof(block) * 2 * gc.m, 0);
#else
    printf("We are entering new get labels\n");
    assert(raw_labels && input_labels); // input_labels should be malloced and filled before

    // fill raw_labels will all relevant labels.
    extractLabels(raw_labels, input_labels, eval_inputs, eval_num_inputs);

    // plug raw_labels into labels using input_mapping
    for (int i=0; i<input_mapping->size; i++) {
        // TODO double check this is correct.
        labels[input_mapping->gc_id[i]][input_mapping->wire_id[i]] = raw_labels[i]; 
    } // also get outputLabels?
#endif
    return SUCCESS;
}

int
new_choice_reader(void *choices, int idx)
{
    int *c = (int *) choices;
    return c[idx];
}

int
new_msg_writer(void *array, int idx, void *msg, size_t msglength)
{
    block *a = (block *) array;
    a[idx] = *((block *) msg);
    return 0;
}
