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
new_choice_reader(const void *choices, const int idx)
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
        const Instructions* instructions, block** labels, const int* circuitMapping,
        block **computedOutputMap, const block *offsets)
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
    //uint64_t start, end, _start, _end, eval_tot = 0, chain_tot = 0;
    int savedCircId;

    //start = current_time();
    for (int i = 0; i < instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                //_start = current_time();
                savedCircId = circuitMapping[cur->evCircId];
                evaluate(&chained_gcs[savedCircId].gc, labels[cur->evCircId], 
                         computedOutputMap[cur->evCircId], GARBLE_TYPE_STANDARD);
                //_end = current_time();
                //eval_tot += _end - _start;
                break;
            case CHAIN:
                //_start = current_time();
                labels[cur->chToCircId][cur->chToWireId] = xorBlocks(
                       computedOutputMap[cur->chFromCircId][cur->chFromWireId], 
                       offsets[cur->chOffsetIdx]);
                //_end = current_time();
                //chain_tot += _end - _start;
                break;
            default:
                printf("Error: Instruction %d is not of a valid type\n", i);
                return;
        }
    }
    //end = current_time();
    //fprintf(stderr, "real eval time: %llu\n", eval_tot);
    //fprintf(stderr, "real chain time: %llu\n", chain_tot);
    //fprintf(stderr, "real tot: %llu\n", end - start);
}

void evaluator_classic_2pc(const int *input, int *output,
        const const int num_garb_inputs, int num_eval_inputs,
        uint64_t *tot_time) 
{
    int sockfd, res;
    struct state state;
    GarbledCircuit gc;
    InputMapping map;
    block *garb_labels = NULL, *eval_labels = NULL;
    block *labels, *output_map, *computed_output_map;
    uint64_t start, end;

    assert(output && tot_time); /* input could be NULL if num_eval_inputs == 0 */

    state_init(&state);
    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    start = current_time();

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

    end = current_time();
    *tot_time = end - start;
}

void
evaluator_offline(char *dir, const int num_eval_inputs, const int nchains, ChainingType chainingType)
{
    int sockfd;
    struct state state;
    ChainedGarbledCircuit cgc;
    uint64_t start, end;
    
    state_init(&state);

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    start = current_time();

    for (int i = 0; i < nchains; i++) {
        chained_gc_comm_recv(sockfd, &cgc, chainingType);
        saveChainedGC(&cgc, dir, false, chainingType);
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

    end = current_time();
    fprintf(stderr, "evaluator offline: %llu\n", end - start);

    close(sockfd);
    state_cleanup(&state);
}

static void
recvInstructions(Instructions *insts, const int fd, block **offsets)
{
    // Without timings:
    net_recv(fd, &insts->size, sizeof insts->size, 0);
    net_recv(fd, &insts->size, sizeof(int), 0);
    insts->instr = malloc(insts->size * sizeof(Instruction));
    net_recv(fd, insts->instr, sizeof(Instruction) * insts->size, 0);

    int noffsets;
    net_recv(fd, &noffsets, sizeof(int), 0);
    *offsets = allocate_blocks(noffsets);
    net_recv(fd, *offsets, sizeof(block) * noffsets, 0);
}

static void 
loadChainedGarbledCircuits(ChainedGarbledCircuit *cgc, int ncgcs, char *dir, ChainingType chainingType) 
{
    /* Loads chained garbled circuits from disk, assuming the loader is the evaluator */

    for (int i = 0; i < ncgcs; ++i) {
            /* false indicates this is the evaluator */

            loadChainedGC(&cgc[i], dir, i, false, chainingType);

            /* If we want to also load outputmap labels */

            /*if (isFinalCircuitType(&chained_gcs[i].type) == true) {*/
                /*[> This could be sped up if it's turns out to be slow <]*/
                /*[> memory is being copied, and that could be avoided with some finesse <]*/
                /*char fileName[50];*/
                /*(void) sprintf(fileName, "%s/%s_%d", dir, "outputmap", chained_gcs[i].id);*/
                /*outputmap = loadOTLabels(fileName);*/
                /*block *temp = loadOTLabels(fileName);*/
                /*memcpy(outputmap, temp, sizeof(block) * chained_gcs[i].gc.m * 2);*/
                /*free(temp);*/

                /*if (!outputmap)*/
                    /*fprintf(stderr, "Error reading outputmap\n");*/

                /*outputmapIdx += (2 * chained_gcs[i].gc.m);*/
            /*} */
        }
}

static void
loadOTPreprocessing(block **eval_labels, int **corrections, char *dir)
{
        char selName[50], lblName[50];
        (void) sprintf(selName, "%s/%s", dir, "sel"); /* XXX: security hole */
        (void) sprintf(lblName, "%s/%s", dir, "lbl"); /* XXX: security hole */
        *corrections = loadOTSelections(selName);
        *eval_labels = loadOTLabels(lblName);
}

static Output*
recvOutput(int outputArrSize, int sockfd) 
{
    Output *output = malloc(sizeof(Output));

    output->gc_id = malloc(sizeof(int) * outputArrSize);
    output->start_wire_idx = malloc(sizeof(int) * outputArrSize);
    output->end_wire_idx = malloc(sizeof(int) * outputArrSize);

    net_recv(sockfd, output->gc_id, sizeof(int) * outputArrSize, 0);
    net_recv(sockfd, output->start_wire_idx, sizeof(int) * outputArrSize, 0);
    net_recv(sockfd, output->end_wire_idx, sizeof(int) * outputArrSize, 0);

    return output;
}

static void
mapOutputsWithOutputInstructions(const Output *outputInstructions, const int outputInstructionsSize, 
                                 int *output, const int noutputs, block **computedOutputMap,
                                 const block *outputMap)
{
    int output_idx = 0;
    for (int i = 0; i < outputInstructionsSize; ++i) {
        int start = outputInstructions->start_wire_idx[i];
        int end = outputInstructions->end_wire_idx[i];
        int dist = end - start + 1;
        int gc_idx = outputInstructions->gc_id[i];
        int res;

        res = mapOutputs(&outputMap[output_idx * 2],
                         &computedOutputMap[gc_idx][start],
                         &output[output_idx], 
                         dist);

        assert(res == SUCCESS);
        output_idx += dist;
    }
    assert(noutputs == output_idx);
}

void
evaluator_online(char *dir, const int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, uint64_t *tot_time, ChainingType chainingType)
{
    ChainedGarbledCircuit* chained_gcs;
    FunctionSpec function;
    block *garb_labels = NULL, *eval_labels = NULL, *recvLabels = NULL, *outputmap = NULL, **labels = NULL, *offsets = NULL;
    int *corrections = NULL, output_size, *circuitMapping, sockfd;
    uint64_t start, end, _start, _end;
    int num_garb_inputs = 0; /* later received from garbler */

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    /* start timing after socket connection */
    start = current_time();

    _start = current_time();
    {
        /* Load things from disk */
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        loadChainedGarbledCircuits(chained_gcs, num_chained_gcs, dir, chainingType);
        loadOTPreprocessing(&eval_labels, &corrections, dir);
    }

    _end = current_time();
    fprintf(stderr, "loading: %llu\n", _end - _start);

    /* Wait for garbler to load garble circuits, so we know when to start timing */
    int empty;
    net_recv(sockfd, &empty, sizeof(int), 0);

    _start = current_time();
    {
        recvInstructions(&function.instructions, sockfd, &offsets);
    }
    _end = current_time();
    fprintf(stderr, "receive inst: %llu\n", _end - _start);

    _start = current_time();
    {
        /* Receive circuit mapping which maps instructions-gc-id to disk-gc-id */
        int size;
        net_recv(sockfd, &size, sizeof(int), 0);
        circuitMapping = malloc(sizeof(int) * size);
        net_recv(sockfd, circuitMapping, sizeof(int) * size, 0);
    }
    _end = current_time();
    fprintf(stderr, "receive circmap: %llu\n", _end - _start);

    _start = current_time();
    {
        /* receive garb labels */
        net_recv(sockfd, &num_garb_inputs, sizeof(int), 0);
        labels = malloc(sizeof(block *) * (num_chained_gcs + 1));

        /* the first num_garb_inputs of labels[0] are the garb_labels, 
         * the rest are the eval_labels */
        labels[0] = allocate_blocks(num_garb_inputs + num_eval_inputs);
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            labels[i] = allocate_blocks(chained_gcs[i-1].gc.n);
        }

        /* receive garbler labels */
        if (num_garb_inputs > 0) {
            net_recv(sockfd, &labels[0][0], sizeof(block) * num_garb_inputs, 0);
        }
    }
    _end = current_time();
    fprintf(stderr, "receive labels: %llu\n", _end - _start);

    /* Receive eval labels: OT correction */
    _start = current_time();
    if (num_eval_inputs > 0) {
        /* randLabels and eval_labels are loaded during loading phase */

        /* correction based on input */
        for (int i = 0; i < num_eval_inputs; ++i) {
            corrections[i] ^= eval_inputs[i];
        }

        recvLabels = malloc(sizeof(block) * 2 * num_eval_inputs);
        net_send(sockfd, corrections, sizeof(int) * num_eval_inputs, 0);
        net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_inputs, 0);

        /* TODO check if faster if we put these directly into labels[0] */
        for (int i = 0; i < num_eval_inputs; ++i) {
            eval_labels[i] = xorBlocks(eval_labels[i],
                                       recvLabels[2 * i + eval_inputs[i]]);
        }

        memcpy(&labels[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);

        free(recvLabels);
        free(corrections);
    }
    _end = current_time();
    fprintf(stderr, "ot correction: %llu\n", _end - _start);

    /* Receive outputmap and outputInstructions */
    _start = current_time();
    int output_arr_size; /* size of output_arr, not num outputs */
    Output *outputInstructions;
    {
        net_recv(sockfd, &output_arr_size, sizeof(int), 0);
        outputInstructions = recvOutput(output_arr_size, sockfd);

        /* receive outputmap */
        net_recv(sockfd, &output_size, sizeof(int), 0);
        outputmap = allocate_blocks(2 * output_size);
        net_recv(sockfd, outputmap, 2 * sizeof(block) * output_size, 0);
    }
    _end = current_time();
    fprintf(stderr, "receive output/outputmap: %llu\n", _end - _start);

    /* Follow instructions and evaluate */
    _start = current_time();
    block** computedOutputMap = malloc(sizeof(block*) * (num_chained_gcs + 1));
    {
        computedOutputMap[0] = allocate_blocks(num_garb_inputs + num_eval_inputs);
        memcpy(computedOutputMap[0], labels[0], sizeof(block) * (num_garb_inputs + num_eval_inputs));
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            computedOutputMap[i] = allocate_blocks(chained_gcs[i-1].gc.m);
        }
        evaluator_evaluate(chained_gcs, num_chained_gcs, &function.instructions,
                           labels, circuitMapping, computedOutputMap, offsets);
    }
    _end = current_time();
    fprintf(stderr, "evaluate: %llu\n", _end - _start);

    _start = current_time();
    int *output = calloc(sizeof(int), output_size);
    {
        /* Map outputs */
        mapOutputsWithOutputInstructions(outputInstructions, output_arr_size, 
                                         output, output_size, computedOutputMap, outputmap);
    }
    _end = current_time();
    fprintf(stderr, "map outputs: %llu\n", _end - _start);

    // 12. clean up
    free(circuitMapping);
    free(outputInstructions->gc_id);
    free(outputInstructions->start_wire_idx);
    free(outputInstructions->end_wire_idx);
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
    free(function.instructions.instr);

    close(sockfd);

    end = current_time();
    if (tot_time) {
        *tot_time = end - start;
    }
}


