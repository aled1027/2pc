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
#include "utils.h"

void 
evaluator_offline(ChainedGarbledCircuit *chained_gcs, int num_eval_inputs,
                  int num_chained_gcs)
{
    int sockfd, len;
    struct state state;
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    /* receive GCs */
    for (int i = 0; i < num_chained_gcs; i++) {
        chained_gc_comm_recv(sockfd, &chained_gcs[i]);
        saveChainedGC(&chained_gcs[i], false);
    }

    /* pre-processing OT using random selection bits */
    {
        int *selections;
        block *evalLabels;

        char selName[50];
        char lblName[50];

        (void) sprintf(selName, "%s/%s", EVALUATOR_DIR, "sel");
        (void) sprintf(lblName, "%s/%s", EVALUATOR_DIR, "lbl");

        selections = malloc(sizeof(int) * num_eval_inputs);
        if (selections == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < num_eval_inputs; ++i) {
            selections[i] = rand() % 2;
        }
        evalLabels = allocate_blocks(num_eval_inputs);
        ot_np_recv(&state, sockfd, selections, num_eval_inputs, sizeof(block),
                   2, evalLabels, new_choice_reader, new_msg_writer);
        saveOTSelections(selName, selections, num_eval_inputs);
        saveOTLabels(lblName, evalLabels, num_eval_inputs, false);

        free(selections);
        free(evalLabels);
    }

    close(sockfd);
    state_cleanup(&state);
}

void
evaluator_online(int *eval_inputs, int num_eval_inputs, int num_chained_gcs, 
        unsigned long *ot_time, unsigned long *tot_time)
{
    // 1. I guess there is no 1 now.

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
	unsigned long start_time, ot_start_time;
    start_time = RDTSC;

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

    // 7. receive labels
    // receieve labels based on garblers inputs
    int num_garb_inputs;
    block *garb_labels, *eval_labels;

    net_recv(sockfd, &num_garb_inputs, sizeof(int), 0);
    if (num_garb_inputs > 0) {
        garb_labels = allocate_blocks(num_garb_inputs);
        net_recv(sockfd, garb_labels, sizeof(block) * num_garb_inputs, 0);
    }

    /* OT correction */
    {
        int *corrections;
        block *recvLabels;
        char selName[50], lblName[50];

        (void) sprintf(selName, "%s/%s", EVALUATOR_DIR, "sel");
        (void) sprintf(lblName, "%s/%s", EVALUATOR_DIR, "lbl");

        recvLabels = malloc(sizeof(block) * 2 * num_eval_inputs);

        corrections = loadOTSelections(selName);
        eval_labels = loadOTLabels(lblName);

        for (int i = 0; i < num_eval_inputs; ++i) {
            corrections[i] ^= eval_inputs[i];
        }

        net_send(sockfd, corrections, sizeof(int) * num_eval_inputs, 0);
        net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_inputs, 0);

        for (int i = 0; i < num_eval_inputs; ++i) {
            eval_labels[i] = xorBlocks(eval_labels[i],
                                       recvLabels[2 * i + eval_inputs[i]]);
        }

        free(recvLabels);
        free(corrections);
    }


    // 8. process eval_labels and garb_labels into labels
    InputMapping* input_mapping = &function.input_mapping;
    int garb_p = 0, eval_p = 0;
    for (int i=0; i<input_mapping->size; i++) {
        if (input_mapping->inputter[i] == PERSON_GARBLER) {
            //printf("(gc_id: %d, wire: %d) grabbing garb input: %d\n", input_mapping->gc_id[i], input_mapping->wire_id[i], garb_p);
            labels[input_mapping->gc_id[i]][input_mapping->wire_id[i]] = garb_labels[garb_p]; 
            garb_p++;
        } else if (input_mapping->inputter[i] == PERSON_EVALUATOR) {
            //printf("(gc_id: %d, wire: %d) grabbing eval input: %d\n", input_mapping->gc_id[i], input_mapping->wire_id[i], eval_p);
            labels[input_mapping->gc_id[i]][input_mapping->wire_id[i]] = eval_labels[eval_p]; 
            eval_p++;
        }
    }

    // 9a receive "output" 
    //  output is from the json, and tells which components/wires are used for outputs
    // note that size is not size of the output, but length of the arrays in output
    int output_arr_size;
    net_recv(sockfd, &output_arr_size, sizeof(int), 0);

    int *output_gc_id = malloc(sizeof(int) * output_arr_size);
    int *start_wire_idx = malloc(sizeof(int) * output_arr_size);
    int *end_wire_idx = malloc(sizeof(int) * output_arr_size);
    net_recv(sockfd, output_gc_id, sizeof(int)*output_arr_size, 0);
    net_recv(sockfd, start_wire_idx, sizeof(int)*output_arr_size, 0);
    net_recv(sockfd, end_wire_idx, sizeof(int)*output_arr_size, 0);
    
    // 9b receive outputmap
    int output_size;
    net_recv(sockfd, &output_size, sizeof(int), 0);
    block* outputmap;
    (void) posix_memalign((void **) &outputmap, 128, sizeof(block) * 2 * output_size);
    assert(outputmap);
    net_recv(sockfd, outputmap, sizeof(block)*2*output_size, 0); 
    //print_blocks("outputmap ", outputmap, 4);

    // 10. Close state and network
    close(sockfd);
    state_cleanup(&state);

    // 11a. evaluate: follow instructions and evaluate components
    block** computedOutputMap = malloc(sizeof(block*) * num_chained_gcs);
    for (int i=0; i<num_chained_gcs; i++) {
        (void) posix_memalign((void **) &computedOutputMap[i], 128, sizeof(block) * chained_gcs[i].gc.m);
        assert(computedOutputMap[i]);
    }
    evaluator_evaluate(chained_gcs, num_chained_gcs, &function.instructions, labels, circuitMapping, computedOutputMap);

    // 11b. use computedOutputMap and outputMap to get actual outputs
    int *output = malloc(sizeof(int) * output_size);
    int p_output = 0;
    for (int i=0; i<output_arr_size; i++) {
        int dist = end_wire_idx[i] - start_wire_idx[i] + 1;
        // gc_idx is not based on circuitMapping because computedOutputMap 
        // populated based on indices in functionSpec
        int gc_idx = output_gc_id[i]; 
        mapOutputs(&outputmap[p_output*2], &computedOutputMap[gc_idx][start_wire_idx[i]],
                &output[p_output], dist);
        p_output += dist;
    }
    assert(output_size == p_output);

    printf("Output: ");
    for (int i=0; i<output_size; i++) {
        printf("%d", output[i]);
    }
    printf("\n");
    
    *tot_time = RDTSC - start_time;

    // 12. clean up
    for (int i=0; i<num_chained_gcs; i++) {
        free(labels[i]);
    } 
    free(labels);
}

void evaluator_evaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, int* circuitMapping,
        block **computedOutputMap)
{
    /*  Essence of function: populate computedOutputMap
     *
     * only used circuitMapping when evaluating. 
     * all other labels, outputmap are basedon the indicies of instructions.
     * This is because instruction's circuits are id'ed 0,..,n-1
     * whereas saved gc id'ed arbitrarily.
     */
    assert(computedOutputMap); // memory should already be allocated
    int savedCircId;
    for (int i=0; i<instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                savedCircId = circuitMapping[cur->evCircId];
                // printf("evaling %d on instruction %d\n", savedCircId, i);
                evaluate(&chained_gcs[savedCircId].gc, labels[cur->evCircId], 
                        computedOutputMap[cur->evCircId]);
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

