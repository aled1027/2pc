#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h> // sleep
#include <time.h>

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"
#include "utils.h"

static int
new_choice_reader(void *choices, int idx)
{
    int *c = (int *) choices;
    return c[idx];
}

static int
new_msg_writer(void *array, int idx, void *msg, size_t msglength)
{
    block *a = (block *) array;
    a[idx] = *((block *) msg);
    return 0;
}

static void
evaluator_evaluate(ChainedGarbledCircuit* chained_gcs, int num_chained_gcs,
        Instructions* instructions, block** labels, int* circuitMapping,
        block **computedOutputMap)
{
    /* Essence of function: populate computedOutputMap
     * computedOutputMap[0] should already be populated garb and eval input labels
     * These inputs are then chained appropriately to where they need to go.
     *
     * only used circuitMapping when evaluating. 
     * all other labels, outputmap are basedon the indicies of instructions.
     * This is because instruction's circuits are id'ed 0,..,n-1
     * whereas saved gc id'ed arbitrarily.
     */
    int savedCircId;
    for (int i = 0; i < instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                savedCircId = circuitMapping[cur->evCircId];
                evaluate(&chained_gcs[savedCircId].gc, labels[cur->evCircId], 
                         computedOutputMap[cur->evCircId], GARBLE_TYPE_STANDARD);
                break;
            case CHAIN:
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

void evaluator_classic_2pc(int *input, int *output,
        int num_garb_inputs, int num_eval_inputs,
        unsigned long *tot_time) 
{
    int sockfd, res;
    struct state state;
    GarbledCircuit gc;
    InputMapping map;
    block *garb_labels = NULL, *eval_labels = NULL;
    block *labels, *output_map, *computed_output_map;
    unsigned long start, end;

    assert(output && tot_time); /* input could be NULL if num_eval_inputs == 0 */

    state_init(&state);
    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    start = RDTSC;

    gc_comm_recv(sockfd, &gc);

    /* Receive input_mapping from garbler */
    {
        size_t buf_size;
        char *buffer;

        net_recv(sockfd, &buf_size, sizeof(size_t), 0);
        buffer = malloc(buf_size);
        net_recv(sockfd, buffer, buf_size, 0);
        readBufferIntoInputMapping(&map, buffer);
        free(buffer);
    }

    /* Receive garb_labels */
    if (num_garb_inputs > 0) {
        garb_labels = allocate_blocks(num_garb_inputs);
        net_recv(sockfd, garb_labels, sizeof(block) * num_garb_inputs, 0);
    }

    /* Receive eval_labels via OT */
    if (num_eval_inputs > 0) {
        eval_labels = allocate_blocks(2 * num_eval_inputs);
        ot_np_recv(&state, sockfd, input, num_eval_inputs, sizeof(block), 2,
                   eval_labels, new_choice_reader, new_msg_writer);
    }

    /* Plug labels in correctly based on input_mapping */
    labels = allocate_blocks(gc.n);
    {
        int garb_p = 0, eval_p = 0;
        for (int i = 0; i < map.size; i++) {
            if (map.inputter[i] == PERSON_GARBLER) {
                labels[map.wire_id[i]] = garb_labels[garb_p]; 
                garb_p++;
            } else if (map.inputter[i] == PERSON_EVALUATOR) {
                labels[map.wire_id[i]] = eval_labels[eval_p]; 
                eval_p++;
            }
        }
    }

    /* Receive output_map */
    output_map = allocate_blocks(2 * gc.m);
    net_recv(sockfd, output_map, sizeof(block) * 2 * gc.m, 0);

    /* Evaluate the circuit */
    computed_output_map = allocate_blocks(gc.m);
    evaluate(&gc, labels, computed_output_map, GARBLE_TYPE_STANDARD);
    res = mapOutputs(output_map, computed_output_map, output, gc.m);
    assert(res == SUCCESS);

    /* Close and clean up network */
    close(sockfd);
    state_cleanup(&state);

    removeGarbledCircuit(&gc);
    deleteInputMapping(&map);

    /* free up memory */
    free(output_map);
    free(computed_output_map);
    free(eval_labels);
    free(garb_labels);
    free(labels);

    end = RDTSC;
    *tot_time = end - start;
}

void
evaluator_offline(char *dir, int num_eval_inputs, int nchains)
{
    int sockfd;
    struct state state;
    ChainedGarbledCircuit cgc;
    unsigned long start, end;
    
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    start = RDTSC;

    for (int i = 0; i < nchains; i++) {
        chained_gc_comm_recv(sockfd, &cgc);
        saveChainedGC(&cgc, dir, false);
        freeChainedGarbledCircuit(&cgc, false);
    }

    /* pre-processing OT using random selection bits */
    if (num_eval_inputs > 0) {
        int *selections;
        block *evalLabels;

        char selName[50];
        char lblName[50];

        (void) sprintf(selName, "%s/%s", dir, "sel"); /* XXX: security hole */
        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */

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

    end = RDTSC;
    fprintf(stderr, "evaluator offline: %lu\n", end - start);

    close(sockfd);
    state_cleanup(&state);
}

static void
recvInstructions(Instructions *insts, int fd)
{
    net_recv(fd, &insts->size, sizeof insts->size, 0);
    insts->instr = malloc(insts->size * sizeof(Instruction));
    for (int i = 0; i < insts->size; ++i) {
        Instruction *inst = &insts->instr[i];
        net_recv(fd, &inst->type, sizeof inst->type, 0);
        switch (inst->type) {
        case CHAIN:
            net_recv(fd, &inst->chFromCircId, sizeof inst->chFromCircId, 0);
            net_recv(fd, &inst->chFromWireId, sizeof inst->chFromWireId, 0);
            net_recv(fd, &inst->chToCircId, sizeof inst->chToCircId, 0);
            net_recv(fd, &inst->chToWireId, sizeof inst->chToWireId, 0);
            net_recv(fd, &inst->chOffset, sizeof inst->chOffset, 0);
            break;
        case EVAL:
            net_recv(fd, &inst->evCircId, sizeof inst->evCircId, 0);
            break;
        default:
            assert(0);
            abort();
        }
    }
}

void
evaluator_online(char *dir, int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, unsigned long *tot_time)
{
    int sockfd;
    unsigned long start, end, _start, _end;
    ChainedGarbledCircuit* chained_gcs;
    FunctionSpec function;
    block **labels;
    int *circuitMapping;
    int num_garb_inputs = 0;
    block *garb_labels = NULL, *eval_labels = NULL;

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    /* start timing after socket connection */
    start = RDTSC;

    _start = RDTSC;
    {
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        for (int i = 0; i < num_chained_gcs; ++i) {
            loadChainedGC(&chained_gcs[i], dir, i, false);
        }
    }
    _end = RDTSC;
    fprintf(stderr, "load gcs: %lu\n", _end - _start);

    _start = RDTSC;
    {
        recvInstructions(&function.instructions, sockfd);
    }
    _end = RDTSC;
    fprintf(stderr, "receive inst: %lu\n", _end - _start);

    // circuitMapping maps instruction-gc-ids --> saved-gc-ids
    _start = RDTSC;
    {
        int size;
        net_recv(sockfd, &size, sizeof(int), 0);
        circuitMapping = malloc(sizeof(int) * size);
        net_recv(sockfd, circuitMapping, sizeof(int) * size, 0);
    }
    _end = RDTSC;
    fprintf(stderr, "receive circmap: %lu\n", _end - _start);

    // 7. receive labels
    // receieve labels based on garblers inputs
    _start = RDTSC;
    {
        net_recv(sockfd, &num_garb_inputs, sizeof(int), 0);

        labels = malloc(sizeof(block *) * (num_chained_gcs + 1));
        /* the first num_garb_inputs of labels[0] are the garb_labels, the rest
         * are the eval_labels */
        labels[0] = allocate_blocks(num_garb_inputs + num_eval_inputs);
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            labels[i] = allocate_blocks(chained_gcs[i-1].gc.n);
        }

        if (num_garb_inputs > 0) {
            net_recv(sockfd, &labels[0][0], sizeof(block) * num_garb_inputs, 0);
        }
    }
    _end = RDTSC;
    fprintf(stderr, "receive labels: %lu\n", _end - _start);

    /* OT correction */
    _start = RDTSC;
    if (num_eval_inputs > 0) {
        int *corrections;
        block *recvLabels;
        char selName[50], lblName[50];

        (void) sprintf(selName, "%s/%s", dir, "sel"); /* XXX: security hole */
        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */

        recvLabels = malloc(sizeof(block) * 2 * num_eval_inputs);

        corrections = loadOTSelections(selName);
        eval_labels = loadOTLabels(lblName);

        // correction based on input
        for (int i = 0; i < num_eval_inputs; ++i) {
            corrections[i] ^= eval_inputs[i];
        }

        net_send(sockfd, corrections, sizeof(int) * num_eval_inputs, 0);
        net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_inputs, 0);

        for (int i = 0; i < num_eval_inputs; ++i) {
            eval_labels[i] = xorBlocks(eval_labels[i],
                                       recvLabels[2 * i + eval_inputs[i]]);
        }

        memcpy(&labels[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);

        free(recvLabels);
        free(corrections);
    }
    _end = RDTSC;
    fprintf(stderr, "ot correction: %lu\n", _end - _start);

    // 9a receive "output" 
    //  output is from the json, and tells which components/wires are used for outputs
    // note that size is not size of the output, but length of the arrays in output
    _start = RDTSC;
    int output_arr_size;
    int output_size;
    int *output_gc_id;
    int *start_wire_idx;
    int *end_wire_idx;
    block *outputmap;
    {
        net_recv(sockfd, &output_arr_size, sizeof(int), 0);

        output_gc_id = malloc(sizeof(int) * output_arr_size);
        start_wire_idx = malloc(sizeof(int) * output_arr_size);
        end_wire_idx = malloc(sizeof(int) * output_arr_size);

        net_recv(sockfd, output_gc_id, sizeof(int)*output_arr_size, 0);
        net_recv(sockfd, start_wire_idx, sizeof(int)*output_arr_size, 0);
        net_recv(sockfd, end_wire_idx, sizeof(int)*output_arr_size, 0);
    
        // 9b receive outputmap
        net_recv(sockfd, &output_size, sizeof(int), 0);
        outputmap = allocate_blocks(2 * output_size);
        net_recv(sockfd, outputmap, sizeof(block)*2*output_size, 0);
    }
    _end = RDTSC;
    fprintf(stderr, "receive output/outputmap: %lu\n", _end - _start);

    // 11a. evaluate: follow instructions and evaluate components
    _start = RDTSC;
    block** computedOutputMap = malloc(sizeof(block*) * (num_chained_gcs + 1));
    {
        computedOutputMap[0] = allocate_blocks(num_garb_inputs + num_eval_inputs);
        memcpy(computedOutputMap[0], labels[0], sizeof(block) * (num_garb_inputs + num_eval_inputs));
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            computedOutputMap[i] = allocate_blocks(chained_gcs[i-1].gc.m);
        }
        evaluator_evaluate(chained_gcs, num_chained_gcs, &function.instructions,
                           labels, circuitMapping, computedOutputMap);
    }
    _end = RDTSC;
    fprintf(stderr, "evaluate: %lu\n", _end - _start);

    // 11b. use computedOutputMap and outputMap to get actual outputs
    _start = RDTSC;
    int *output = calloc(sizeof(int), output_size);
    {
        int p_output = 0;
        for (int i = 0; i < output_arr_size; i++) {
            int start = start_wire_idx[i];
            int end = end_wire_idx[i];
            int dist = end - start + 1;
            int gc_idx = output_gc_id[i];
            int res;

            res = mapOutputs(&outputmap[p_output*2],
                             &computedOutputMap[gc_idx][start],
                             &output[p_output], dist);
            assert(res == SUCCESS);
            p_output += dist;
        }
        assert(output_size == p_output);
    }
    _end = RDTSC;
    fprintf(stderr, "map outputs: %lu\n", _end - _start);

    // 12. clean up
    free(circuitMapping);
    free(output_gc_id);
    free(start_wire_idx);
    free(end_wire_idx);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], false);
        free(labels[i]);
        free(computedOutputMap[i]);
    }
    free(labels[num_chained_gcs]);
    free(computedOutputMap[num_chained_gcs]);
    free(chained_gcs);
    free(labels);
    free(computedOutputMap);
    free(outputmap);
    free(output);
    free(garb_labels);
    free(eval_labels);
    //deleteInputMapping(&function.input_mapping);
    free(function.instructions.instr);

    close(sockfd);

    end = RDTSC;
    if (tot_time) {
        *tot_time = end - start;
    }
}
