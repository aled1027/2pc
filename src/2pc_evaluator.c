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
    /*  Essence of function: populate computedOutputMap
     *
     * only used circuitMapping when evaluating. 
     * all other labels, outputmap are basedon the indicies of instructions.
     * This is because instruction's circuits are id'ed 0,..,n-1
     * whereas saved gc id'ed arbitrarily.
     */
    assert(computedOutputMap); // memory should already be allocated
    int savedCircId;
    for (int i = 0; i < instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                savedCircId = circuitMapping[cur->evCircId];
                /*printf("evaling %d on instruction %d\n", savedCircId, i);*/
                evaluate(&chained_gcs[savedCircId].gc, labels[cur->evCircId], 
                         computedOutputMap[cur->evCircId], GARBLE_TYPE_STANDARD);
                break;
            case CHAIN:
                /*printf("chaining (%d,%d) -> (%d,%d)\n", 
                 * cur->chFromCircId, cur->chFromWireId, cur->chToCircId, cur->chToWireId);*/
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
        removeGarbledCircuit(&cgc.gc);
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
receive_instructions_and_input_mapping(FunctionSpec *f, int fd)
{
    char *buffer1, *buffer2;
    size_t buf_size1, buf_size2;

    net_recv(fd, &buf_size1, sizeof(size_t), 0);
    net_recv(fd, &buf_size2, sizeof(size_t), 0);

    buffer1 = malloc(buf_size1);
    buffer2 = malloc(buf_size2);

    net_recv(fd, buffer1, buf_size1, 0);
    net_recv(fd, buffer2, buf_size2, 0);

    readBufferIntoInstructions(&f->instructions, buffer1);
    readBufferIntoInputMapping(&f->input_mapping, buffer2);

    free(buffer1);
    free(buffer2);
}

void
evaluator_online(char *dir, int *eval_inputs, int num_eval_labels,
                 int num_chained_gcs, unsigned long *tot_time)
{
    int sockfd;
    struct state state;
    unsigned long start, end, _start, _end;
    ChainedGarbledCircuit* chained_gcs;
    FunctionSpec function;
    block **labels;
    int *circuitMapping;
    int num_garb_labels;
    block *garb_labels = NULL, *eval_labels = NULL;

    state_init(&state);
    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

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
        receive_instructions_and_input_mapping(&function, sockfd);
    }
    _end = RDTSC;
    fprintf(stderr, "receive inst/map: %lu\n", _end - _start);

    // circuitMapping maps instruction-gc-ids --> saved-gc-ids
    _start = RDTSC;
    {
        int circuitMappingSize;
        net_recv(sockfd, &circuitMappingSize, sizeof(int), 0);
        circuitMapping = malloc(sizeof(int) * circuitMappingSize);
        net_recv(sockfd, circuitMapping, sizeof(int) * circuitMappingSize, 0);
    }
    _end = RDTSC;
    fprintf(stderr, "receive circmap: %lu\n", _end - _start);

    // 7. receive labels
    // receieve labels based on garblers inputs
    _start = RDTSC;
    {
        net_recv(sockfd, &num_garb_labels, sizeof(int), 0);
        if (num_garb_labels > 0) {
            garb_labels = allocate_blocks(num_garb_labels);
            net_recv(sockfd, garb_labels, sizeof(block) * num_garb_labels, 0);
        }
    }
    _end = RDTSC;
    fprintf(stderr, "receive labels: %lu\n", _end - _start);


    InputMapping* input_mapping = &function.input_mapping;
    /* OT correction */
    _start = RDTSC;
    if (num_eval_labels > 0) {
        int *corrections;
        block *recvLabels;
        char selName[50], lblName[50];
        int counter;

        (void) sprintf(selName, "%s/%s", dir, "sel"); /* XXX: security hole */
        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */

        recvLabels = malloc(sizeof(block) * 2 * num_eval_labels);

        corrections = loadOTSelections(selName);
        eval_labels = loadOTLabels(lblName);

        // correction based on input
        counter = 0;
        for (int i = 0; i < input_mapping->size; ++i) {
            if (input_mapping->inputter[i] == PERSON_EVALUATOR) {
                int input_idx = input_mapping->input_idx[i];
                corrections[counter] ^= eval_inputs[input_idx];
                counter++;
            }
        }
        assert(counter == num_eval_labels);
        printf("num_eval_labels = %d\n", num_eval_labels);

        net_send(sockfd, corrections, sizeof(int) * num_eval_labels, 0);
        net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_labels, 0);

        counter = 0;
        for (int i = 0; i < input_mapping->size; ++i) {
            if (input_mapping->inputter[i] == PERSON_EVALUATOR) {
                int idx = input_mapping->input_idx[i];
                eval_labels[counter] = xorBlocks(eval_labels[counter],
                                                 recvLabels[2 * counter + eval_inputs[idx]]);
                counter++;
            }
        }

        free(recvLabels);
        free(corrections);
    }
    _end = RDTSC;
    fprintf(stderr, "ot correction: %lu\n", _end - _start);

    // 8. process eval_labels and garb_labels into labels
    _start = RDTSC;
    InputMapping *map = &function.input_mapping;
    labels = malloc(sizeof(block *) * num_chained_gcs);
    for (int i = 0; i < num_chained_gcs; ++i) {
        labels[i] = allocate_blocks(chained_gcs[i].gc.n);
    }
    int garb_p = 0, eval_p = 0;
    for (int i = 0; i < map->size; i++) {
        switch (map->inputter[i]) {
        case PERSON_GARBLER:
            labels[map->gc_id[i]][map->wire_id[i]] = garb_labels[garb_p]; 
            garb_p++;
            break;
        case PERSON_EVALUATOR:
            labels[map->gc_id[i]][map->wire_id[i]] = eval_labels[eval_p]; 
            eval_p++;
            break;
        default:
            assert(0);
            abort();
        }
    }
    _end = RDTSC;
    fprintf(stderr, "process labels: %lu\n", _end - _start);

    // 9a receive "output" 
    //  output is from the json, and tells which components/wires are used for outputs
    // note that size is not size of the output, but length of the arrays in output
    _start = RDTSC;
    int output_arr_size;
    int *output_gc_id;
    int *start_wire_idx;
    int *end_wire_idx;
    net_recv(sockfd, &output_arr_size, sizeof(int), 0);

    output_gc_id = malloc(sizeof(int) * output_arr_size);
    start_wire_idx = malloc(sizeof(int) * output_arr_size);
    end_wire_idx = malloc(sizeof(int) * output_arr_size);

    net_recv(sockfd, output_gc_id, sizeof(int)*output_arr_size, 0);
    net_recv(sockfd, start_wire_idx, sizeof(int)*output_arr_size, 0);
    net_recv(sockfd, end_wire_idx, sizeof(int)*output_arr_size, 0);
    
    // 9b receive outputmap
    int output_size;
    block *outputmap;
    net_recv(sockfd, &output_size, sizeof(int), 0);
    outputmap = allocate_blocks(2 * output_size);
    net_recv(sockfd, outputmap, sizeof(block)*2*output_size, 0);
    int *output = malloc(sizeof(int) * output_size);
    _end = RDTSC;
    fprintf(stderr, "receive output/outputmap: %lu\n", _end - _start);

    // 10. Close state and network
    close(sockfd);
    state_cleanup(&state);

    // 11a. evaluate: follow instructions and evaluate components
    _start = RDTSC;
    block** computedOutputMap = malloc(sizeof(block*) * num_chained_gcs);
    for (int i = 0; i < num_chained_gcs; i++) {
        computedOutputMap[i] = (block*) allocate_blocks(chained_gcs[i].gc.m);
    }
    evaluator_evaluate(chained_gcs, num_chained_gcs, &function.instructions,
                       labels, circuitMapping, computedOutputMap);
    _end = RDTSC;
    fprintf(stderr, "evaluate: %lu\n", _end - _start);

    // 11b. use computedOutputMap and outputMap to get actual outputs
    _start = RDTSC;
    {
        int p_output = 0;
        for (int i = 0; i < output_arr_size; i++) {
            int res, dist, gc_idx;

            dist = end_wire_idx[i] - start_wire_idx[i] + 1;
            // gc_idx is not based on circuitMapping because computedOutputMap 
            // populated based on indices in functionSpec
            gc_idx = output_gc_id[i]; 
            res = mapOutputs(&outputmap[p_output*2],
                             &computedOutputMap[gc_idx][start_wire_idx[i]],
                             &output[p_output], dist);
            assert(res == SUCCESS);
            p_output += dist;
        }
        assert(output_size == p_output);
    }
    _end = RDTSC;
    fprintf(stderr, "map outputs: %lu\n", _end - _start);

    /* printf("Output: "); */
    /* for (int i = 0; i < output_size; i++) { */
    /*     printf("%d", output[i]); */
    /* } */
    /* printf("\n"); */
    
    // 12. clean up
    free(circuitMapping);
    free(output_gc_id);
    free(start_wire_idx);
    free(end_wire_idx);
    // TODO new function freechainedgcs
    for (int i = 0; i < num_chained_gcs; i++) {
        freeChainedGarbledCircuit(&chained_gcs[i]);
        free(labels[i]);
        free(computedOutputMap[i]);
    }
    free(chained_gcs);
    free(labels);
    free(computedOutputMap);
    free(outputmap);
    free(output);
    free(garb_labels);
    free(eval_labels);
    deleteInputMapping(&function.input_mapping);
    free(function.instructions.instr);

    end = RDTSC;
    if (tot_time) {
        *tot_time = end - start;
    }
}
