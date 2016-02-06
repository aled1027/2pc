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
        block **computedOutputMap, const block *offsets, ChainingType chainingType)
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
    int savedCircId, offsetIdx;

    for (int i = 0; i < instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        switch(cur->type) {
            case EVAL:
                savedCircId = circuitMapping[cur->evCircId];
                evaluate(&chained_gcs[savedCircId].gc, labels[cur->evCircId], 
                         computedOutputMap[cur->evCircId], GARBLE_TYPE_STANDARD);
                break;
            case CHAIN:

                if (chainingType == CHAINING_TYPE_STANDARD || cur->chFromCircId == 0) {
                    labels[cur->chToCircId][cur->chToWireId] = xorBlocks(
                            computedOutputMap[cur->chFromCircId][cur->chFromWireId], 
                            offsets[cur->chOffsetIdx]);
                } else { /* CHAINING_TYPE_SIMD */
                    /* TODO if this is slow, there are probably ways to optimize */

                    savedCircId = circuitMapping[cur->chFromCircId];
                    offsetIdx = cur->chOffsetIdx;

                    /* correct computedOutputMap offlineChainingOffsets */
                    /* i.e. correct to enable SIMD trick */

                    for (int j = 0; j < chained_gcs[savedCircId].gc.m; ++j) {
                        /* offsets from offline phase */
                        computedOutputMap[cur->chFromCircId][j] = xorBlocks(
                                computedOutputMap[cur->chFromCircId][j],
                                chained_gcs[savedCircId].offlineChainingOffsets[j]);

                        /* offsets from online phase */
                        labels[cur->chToCircId][j] = xorBlocks(
                                computedOutputMap[cur->chFromCircId][j], 
                                offsets[offsetIdx]);
                    }
                }
                break;
            default:
                printf("Error: Instruction %d is not of a valid type\n", i);
                return;
        }
    }
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
        freeChainedGarbledCircuit(&cgc, false, chainingType);
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

    
    /* So we know to start counting this */

    uint64_t s, e;
    s = current_time();

    int noffsets;
    net_recv(fd, &insts->size, sizeof(int), 0);
    net_recv(fd, &noffsets, sizeof(int), 0);

    insts->instr = malloc(insts->size * sizeof(Instruction));
    *offsets = allocate_blocks(noffsets);

    net_recv(fd, insts->instr, sizeof(Instruction) * insts->size, 0);
    net_recv(fd, *offsets, sizeof(block) * noffsets, 0);

    e = current_time();
    fprintf(stderr, "recvInstructions total: %llu\n", e - s);
}

static void 
loadChainedGarbledCircuits(ChainedGarbledCircuit *cgc, int ncgcs, char *dir, ChainingType chainingType) 
{
    /* Loads chained garbled circuits from disk, assuming the loader is the evaluator */
    for (int i = 0; i < ncgcs; ++i) {
        loadChainedGC(&cgc[i], dir, i, false, chainingType);
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

static int
computeOutputs(const OutputInstructions *ois, int *output, block ** computed_outputmap)
{
    assert(output && "output's memory should be allocated");

    for (uint16_t i = 0; i < ois->size; ++i) {
        OutputInstruction *oi = &ois->output_instruction[i];

        // decrypt using comp_block as key
        block comp_block = computed_outputmap[oi->gc_id][oi->wire_id];
        //block dec_block;

        block out0 = our_decrypt(&oi->labels[0], &comp_block);
        block out1 = our_decrypt(&oi->labels[1], &comp_block);

        block b_zero = zero_block();
        block b_one = makeBlock((uint64_t) 0, (uint64_t) 1); // 000...00001

        if (blockEqual(b_zero, out0)) {
            output[i] = 0;
        } else if (blockEqual(b_one, out0)) {
            output[i] = 1;
        } else if (blockEqual(b_zero, out1)) {
            output[i] = 0;
        } else if (blockEqual(b_one, out1)) {
            output[i] = 1;
        } else {
            fprintf(stderr, "Could not compute output[%d] from (gc_id: %d, wire_id %d\n",
                    i, oi->gc_id, oi->wire_id);
            assert(false);
            return FAILURE;
        }
    }
    return SUCCESS;
}

void
evaluator_online(char *dir, const int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, uint64_t *tot_time, ChainingType chainingType)
{
    ChainedGarbledCircuit* chained_gcs;
    FunctionSpec function;
    block *garb_labels = NULL, *eval_labels = NULL, *recvLabels = NULL, *outputmap = NULL, **labels = NULL, *offsets = NULL;
    int *corrections = NULL, *circuitMapping, sockfd;
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
    }
    loadOTPreprocessing(&eval_labels, &corrections, dir);

    _end = current_time();
    fprintf(stderr, "loading: %llu\n", _end - _start);

    _start = current_time();
    uint8_t a_byte;
    net_recv(sockfd, &a_byte, sizeof(a_byte), 0);
    _end = current_time();
    fprintf(stderr, "waiting: %llu\n", _end - _start);

    {
        /* timed inside of function */
        recvInstructions(&function.instructions, sockfd, &offsets);
    }

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

        for (int i = 0; i < num_eval_inputs; ++i)
            corrections[i] ^= eval_inputs[i];

        recvLabels = malloc(sizeof(block) * 2 * num_eval_inputs);
        /* valgrind is saying corrections is unitialized, but it seems to be */
        net_send(sockfd, corrections, sizeof(int) * num_eval_inputs, 0);
        net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_inputs, 0);

        /* TODO check if faster if we put these directly into labels[0] */
        for (int i = 0; i < num_eval_inputs; ++i) {
            assert(eval_inputs[i] == 0 || eval_inputs[i] == 1);
            eval_labels[i] = xorBlocks(eval_labels[i],
                                       recvLabels[2 * i + eval_inputs[i]]);
        }

        memcpy(&labels[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);

        free(recvLabels);
        free(corrections);
    }
    _end = current_time();
    fprintf(stderr, "ot correction: %llu\n", _end - _start);

    /* Receive output instructions */
    OutputInstructions output_instructions;
    _start = current_time();
    {
        net_recv(sockfd, &output_instructions.size, 
                sizeof(output_instructions.size), 0);
        output_instructions.output_instruction = 
            malloc(output_instructions.size * sizeof(OutputInstruction));

        net_recv(sockfd, output_instructions.output_instruction, 
                output_instructions.size * sizeof(OutputInstruction), 0);
    }
    _end = current_time();
    fprintf(stderr, "recv_output_instructions: %llu\n", _end - _start);

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
                           labels, circuitMapping, computedOutputMap, offsets, chainingType);
    }
    _end = current_time();
    fprintf(stderr, "evaluate: %llu\n", _end - _start);

    _start = current_time();
    int *output = calloc(sizeof(int), output_instructions.size);
    {
        int res = computeOutputs(&output_instructions, output, computedOutputMap);
        if (res == FAILURE) {
            fprintf(stderr, "computeOutputs failed\n");
        }

    }
    _end = current_time();
    fprintf(stderr, "map outputs: %llu\n", _end - _start);

    // 12. clean up
    free(circuitMapping);
    for (int i = 0; i < num_chained_gcs; ++i) {
        freeChainedGarbledCircuit(&chained_gcs[i], false, chainingType);
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
    free(offsets);

    close(sockfd);

    end = current_time();
    if (tot_time) {
        *tot_time = end - start;
    }
}


