#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h> // sleep

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h" // USE_TO

void 
evaluator_receive_gcs(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) 
{
    int sockfd, len;
    struct state state;
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    for (int i=0; i<num_chained_gcs; i++) {
        chained_gc_comm_recv(sockfd, &chained_gcs[i]);
    }

    close(sockfd);
    state_cleanup(&state);
}

int
evaluator_go(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs, int* eval_inputs, int num_eval_inputs) 
{
    sleep(1);

    printf("Starting evaluator_g()\n");
    // 1. setup connection
    int sockfd, len;
    struct state state;
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    // 2. initialize things
    int num_gcs = NUM_GCS;
    FunctionSpec function;
    block** labels = malloc(sizeof(block*) * num_gcs);
    block** computedOutputMap = malloc(sizeof(block*) * num_gcs);

    // I think that some metadata needs to be transferred, like size of instructions, 
    // size of inputmapping, which garbled circuits are going to be used.
    // make this for loop more robust.
    for (int i=0; i<num_gcs; i++) {
        labels[i] = memalign(128, sizeof(block) * 2);
        computedOutputMap[i] = memalign(128, sizeof(block) * 2);
        assert(labels[i] && computedOutputMap[i]);
    }


    // 2. receive input_mapping, instructions
    // popualtes instructions and input_mapping field of function
    // allocates memory for them as needed.
    recv_instructions_and_input_mapping(&function, sockfd);

    // -------- not done past here--------------

    // 3. received labels based on evaluator's inputs

    block* eval_labels = memalign(128, sizeof(block) * num_eval_inputs);
    ot_np_recv(&state, sockfd, eval_inputs, num_eval_inputs, sizeof(block), 2, eval_labels,
               new_choice_reader, new_msg_writer);

    // 4. receive labels based on garbler's inputs
    // none so ignoring for now
    
    // 5. process eval_labels and garb_labels into labels
    // TODO improve to include garbler inputs
    InputMapping* input_mapping = &function.input_mapping;
    for (int i=0; i<input_mapping->size; i++) {
        labels[input_mapping->gc_id[i]][input_mapping->wire_id[i]] = eval_labels[i]; 
    }

    // 5. receive outputmap
    block* outputmap = memalign(128, sizeof(block) * 2 * 2);
    net_recv(sockfd, outputmap, sizeof(block)*2*2, 0); 

    // 6. evaluate
    
    int *output = malloc(sizeof(int) * 2);
    chainedEvaluate(chained_gcs, num_chained_gcs, &function.instructions, labels, outputmap, output);
    printf("output: (%d, %d)\n", output[0], output[1]);
    
    // 7. clean up
    close(sockfd);
    state_cleanup(&state);
    for (int i=0; i<num_gcs; i++) {
        free(computedOutputMap[i]);
        free(labels[i]);
    } 
    free(computedOutputMap);
    free(labels);

    return SUCCESS;
}

int chainedEvaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, block* outputmap, int* output)
{

    printf("------------------starting chained evaluate------------------\n");

    block** computedOutputMap = malloc(sizeof(block*) * num_chained_gcs);

    for (int i=0; i<num_chained_gcs; i++) {
        computedOutputMap[i] = memalign(128, sizeof(block) * 2);
        assert(computedOutputMap[i]);
    }

    for (int i=0; i<instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                evaluate(&chained_gcs[cur->evCircId].gc, labels[cur->evCircId], computedOutputMap[cur->evCircId]);
                if (i == instructions->size - 1) {
	                mapOutputs(outputmap, computedOutputMap[2], output, chained_gcs[2].gc.m);
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
    return SUCCESS;
}

int 
getLabels(block** labels, int* eval_inputs, int eval_num_inputs, 
        block* input_labels, InputMapping *input_mapping) 
{
    /* Manages online networking for evaluator
     *
     * labels: destination of grabbed labels. Memory should already be allocated 
     * eval_inputs: evaluator's inputs.
     * eval_num_inputs: number of evaluators inputs.
     * inputLabels: only used if not using OT. Garbler sends over all labels, 
     *  and we just grab the ones we need
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


void 
recv_instructions_and_input_mapping(FunctionSpec *function, int sockfd) {
    char *buffer1, *buffer2;
    size_t buf_size1, buf_size2;

    net_recv(sockfd, &buf_size1, sizeof(size_t), 0);
    net_recv(sockfd, &buf_size2, sizeof(size_t), 0);

    buffer1 = malloc(buf_size1);
    buffer2 = malloc(buf_size2);

    net_recv(sockfd, buffer1, buf_size1, 0);
    net_recv(sockfd, buffer2, buf_size2, 0);

    readBufferIntoInstructions(&function->instructions, buffer1);
    readBufferIntoInputMapping(&function->input_mapping, buffer2);
}
