#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h> // sleep
#include <time.h>

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h" // USE_TO

void
evaluator_run(int *eval_inputs, int num_eval_inputs, int num_chained_gcs)
{

    // 2. load chained garbled circuits from disk
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_chained_gcs);
    for (int i=0; i<num_chained_gcs; i++) {
        loadChainedGC(&chained_gcs[i], i, false); // false indicates this is evaluator
    }

    // 3. setup connection
    int sockfd, len;
    struct state state;
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    // 4. allocate some memory
	unsigned long startCycles = RDTSC;
	unsigned long origStartTime, startTime, TOTtime, OTtime;
    startTime = origStartTime = RDTSC;

    FunctionSpec function;
    block** labels = malloc(sizeof(block*) * num_chained_gcs);

    for (int i=0; i<num_chained_gcs; i++) {
        (void) posix_memalign((void **) &labels[i], 128, sizeof(block) * chained_gcs[i].gc.n); 
        assert(labels[i]);
    }

    // 5. receive input_mapping, instructions
    // popualtes instructions and input_mapping field of function
    // allocates memory for them as needed.
    char *buffer1, *buffer2;
    size_t buf_size1, buf_size2;

    net_recv(sockfd, &buf_size1, sizeof(size_t), 0);
    net_recv(sockfd, &buf_size2, sizeof(size_t), 0);

    buffer1 = malloc(buf_size1);
    buffer2 = malloc(buf_size2);

    net_recv(sockfd, buffer1, buf_size1, 0);
    net_recv(sockfd, buffer2, buf_size2, 0);

    readBufferIntoInstructions(&function.instructions, buffer1);
    readBufferIntoInputMapping(&function.input_mapping, buffer2);
    free(buffer1);
    free(buffer2);

    // 6. receive circuitMapping
    // circuitMapping maps instruction-gc-ids --> saved-gc-ids 
    int circuitMappingSize, *circuitMapping;
    net_recv(sockfd, &circuitMappingSize, sizeof(int), 0);
    circuitMapping = malloc(sizeof(int) * circuitMappingSize);
    net_recv(sockfd, circuitMapping, sizeof(int)*circuitMappingSize, 0);

    //-----------------------------------------
    //--------START OF SLOWNESS---------------------
    //-----------------------------------------
    // 7. receive labels
    // receieve labels based on garblers inputs
    int num_garb_inputs;
    net_recv(sockfd, &num_garb_inputs, sizeof(int), 0);

    block *garb_labels, *eval_labels;
    (void) posix_memalign((void **) &garb_labels, 128, sizeof(block) * num_garb_inputs);
    assert(garb_labels && num_garb_inputs >= 0);
    if (num_garb_inputs > 0) {
        net_recv(sockfd, garb_labels, sizeof(block)*num_garb_inputs, 0);
    }
    
    // receive labels based on evaluator's inputs
    (void) posix_memalign((void **) &eval_labels, 128, sizeof(block) * num_eval_inputs);
    assert(eval_labels);

    ot_np_recv(&state, sockfd, eval_inputs, num_eval_inputs, sizeof(block), 2, eval_labels,
               new_choice_reader, new_msg_writer);

    OTtime = RDTSC - startTime;
    //-----------------------------------------
    //--------END OF SLOWNESS---------------------
    //-----------------------------------------
    // 8. process eval_labels and garb_labels into labels
    InputMapping* input_mapping = &function.input_mapping;
    int garb_p = 0, eval_p = 0;
    for (int i=0; i<input_mapping->size; i++) {
        if (input_mapping->inputter[i] == PERSON_GARBLER) {
            labels[input_mapping->gc_id[i]][input_mapping->wire_id[i]] = garb_labels[garb_p]; 
            garb_p++;
        } else if (input_mapping->inputter[i] == PERSON_EVALUATOR) {
            // printf("(gc_id: %d, wire: %d) grabbing eval input: %d\n", input_mapping->gc_id[i], input_mapping->wire_id[i], eval_p);
            labels[input_mapping->gc_id[i]][input_mapping->wire_id[i]] = eval_labels[eval_p]; 
            eval_p++;
        }
    }

    // 9. receive outputmap
    int output_size;
    net_recv(sockfd, &output_size, sizeof(int), 0);
    block* outputmap;
    (void) posix_memalign((void **) &outputmap, 128, sizeof(block) * 2 * output_size);
    assert(outputmap);
    net_recv(sockfd, outputmap, sizeof(block)*2*output_size, 0); 

    // 10. evaluate
    int *output = malloc(sizeof(int) * output_size);
    evaluator_evaluate(chained_gcs, num_chained_gcs, &function.instructions, labels, outputmap, output, circuitMapping);

    printf("Output: ");
    for (int i=0; i<output_size; i++) {
        printf("%d", output[i]);
    }
    printf("\n");
    
    TOTtime = RDTSC - origStartTime;
    printf("%lu, %lu \n", OTtime, TOTtime);

    // 11. clean up
    // TODO move state and close up above, before evaluation. Receive everything, then evaluate.
    close(sockfd);
    state_cleanup(&state);
    for (int i=0; i<num_chained_gcs; i++) {
        free(labels[i]);
    } 
    free(labels);
}

void evaluator_evaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, block* outputmap, int* output, int* circuitMapping)
{
    /* only used circuitMapping when evaluating. 
     * all other labels, outputmap are basedon the indicies of instructions.
     * This is because instruction's circuits are id'ed 0,..,n-1
     * whereas saved gc id'ed arbitrarily.
     */

    block** computedOutputMap = malloc(sizeof(block*) * num_chained_gcs);
    for (int i=0; i<num_chained_gcs; i++) {
        (void) posix_memalign((void **) &computedOutputMap[i], 128, sizeof(block) * chained_gcs[i].gc.m);
        assert(computedOutputMap[i]);
    }

    int savedCircId;
    for (int i=0; i<instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                savedCircId = circuitMapping[cur->evCircId];
                // be nice if in debug mode, this printed to a log file. 
                // probably don't need that functionality
                // printf("evaling %d on instruction %d\n", savedCircId, i);
                evaluate(&chained_gcs[savedCircId].gc, labels[cur->evCircId], 
                        computedOutputMap[cur->evCircId]);

                if (i == instructions->size - 1) {
                    //printf("Mapping outputput for savedCircId: %d\n", savedCircId);
	                mapOutputs(outputmap, computedOutputMap[cur->evCircId], 
                            output, chained_gcs[savedCircId].gc.m);
                }
                break;
            case CHAIN:
                // printf("chaining (%d,%d) -> (%d,%d)\n", cur->chFromCircId, cur->chFromWireId, cur->chToCircId, cur->chToWireId);
                labels[cur->chToCircId][cur->chToWireId] = xorBlocks(
                        computedOutputMap[cur->chFromCircId][cur->chFromWireId], 
                        cur->chOffset);
                break;
            default:
                printf("Error: Instruction %d is not of a valid type\n", i);
                return;
        }
    }
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
evaluator_offline(ChainedGarbledCircuit *chained_gcs, int num_chained_gcs) 
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
        saveChainedGC(&chained_gcs[i], false);
    }

    close(sockfd);
    state_cleanup(&state);
}


