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
evaluator_go(ChainedGarbledCircuit* chained_gcs, int* eval_inputs) 
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

    //getLabels(labels, eval_inputs, 4, inputLabels, input_mapping); // uses OT if necessary

    // 2. receive input_mapping, instructions
    // popualtes instructions and input_mapping field of function
    // allocates memory for them as needed.
    recv_instructions_and_input_mapping(&function, sockfd);
    printf("break here\n");


    // -------- not done past here--------------


    // 3. received labels based on evaluator's inputs
    //getEvaluatorLabels(block** labels, int* eval_inputs, int eval_num_inputs, 
    //    block* input_labels, InputMapping *input_mapping) 
    // 4. receive labels based on garbler's inputs
    // 5. receive outputmap
    
    // 6. evaluate
    // chainedEvaluate()
    
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
        labels[i] = memalign(128, sizeof(block)* 2);
        computedOutputMap[i] = memalign(128, sizeof(block) * 2);
        assert(labels[i] && computedOutputMap[i]);
    }

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
