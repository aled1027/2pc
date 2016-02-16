#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h> // sleep
#include <time.h>
#include <string.h>
#include "justGarble.h"

#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"
#include "utils.h"

static int
new_choice_reader(const void *choices, int idx)
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
    uint64_t s,e, eval_time = 0;
    for (int i = 0; i < instructions->size; i++) {
        Instruction* cur = &instructions->instr[i];
        //print_instruction(cur);
        switch(cur->type) {
            case EVAL:
                s = current_time_();
                savedCircId = circuitMapping[cur->ev.circId];

                evaluate(&chained_gcs[savedCircId].gc, labels[cur->ev.circId], 
                         computedOutputMap[cur->ev.circId], GARBLE_TYPE_STANDARD);

                e = current_time_();
                eval_time += e - s;
                break;
            case CHAIN:

                if (chainingType == CHAINING_TYPE_STANDARD) {
                    labels[cur->ch.toCircId][cur->ch.toWireId] = xorBlocks(
                            computedOutputMap[cur->ch.fromCircId][cur->ch.fromWireId], 
                            offsets[cur->ch.offsetIdx]);

                } else { 
                    /* CHAINING_TYPE_SIMD */
                    savedCircId = circuitMapping[cur->ch.fromCircId];
                    offsetIdx = cur->ch.offsetIdx;

                    for (int j = cur->ch.fromWireId, k = cur->ch.toWireId; 
                         j < cur->ch.fromWireId + cur->ch.wireDist;
                         ++j, ++k) {

                        /* correct computedOutputMap offlineChainingOffsets */
                        /* i.e. correct to enable SIMD trick */
                        labels[cur->ch.toCircId][k] = xorBlocks(
                                computedOutputMap[cur->ch.fromCircId][j],
                                offsets[offsetIdx]);

                        if (cur->ch.fromCircId != 0) { /* if not the input component */
                            labels[cur->ch.toCircId][k] = xorBlocks(
                                    labels[cur->ch.toCircId][k],
                                    chained_gcs[savedCircId].offlineChainingOffsets[j]);
                        }

                    }
                }
                break;
            default:
                printf("Error: Instruction %d is not of a valid type\n", i);
                return;
        }
    }
    fprintf(stderr, "evaltime: %llu\n", eval_time);
}

void evaluator_classic_2pc(const int *input, int *output,
        int num_garb_inputs, int num_eval_inputs,
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

    start = current_time_();

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
    assert(false && "not yet updated for new input mapping");
    //labels = allocate_blocks(gc.n);
    //{
    //    int garb_p = 0, eval_p = 0;
    //    for (int i = 0; i < map.size; i++) {
    //        if (map.inputter[i] == PERSON_GARBLER) {
    //            labels[map.wire_id[i]] = garb_labels[garb_p]; 
    //            garb_p++;
    //        } else if (map.inputter[i] == PERSON_EVALUATOR) {
    //            labels[map.wire_id[i]] = eval_labels[eval_p]; 
    //            eval_p++;
    //        }
    //    }
    //}

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

    end = current_time_();
    *tot_time = end - start;
}

void
evaluator_offline(char *dir, int num_eval_inputs, int nchains, ChainingType chainingType)
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

    start = current_time_();

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

    end = current_time_();
    fprintf(stderr, "evaluator offline: %llu\n", (end - start));

    close(sockfd);
    state_cleanup(&state);
}

static void
recvInstructions(int fd, Instructions *insts, block **offsets)
{
    int noffsets;

    net_recv(fd, &insts->size, sizeof(int), 0);
    net_recv(fd, &noffsets, sizeof(int), 0);

    insts->instr = malloc(insts->size * sizeof(Instruction));
    *offsets = allocate_blocks(noffsets);

/* <<<<<<< Updated upstream */
/*     fprintf(stderr, "instructions size: %d\n", insts->size); */
/*     fprintf(stderr, "instructions tot bytes %d\n", insts->size * sizeof(Instruction)); */


/*     /\* ADDING HERE *\/ */
/*     // for aes, insts->size = 1427 */
    
/*     for (int i = 0; i < 1000; i+=100) { */
/*         start = current_time_(); */
/*         net_recv(fd, &insts->instr[i], 100 * sizeof(Instruction), 0); */
/*         end = current_time_(); */
/*         fprintf(stderr, "\tSecond split time (%d): %llu\n", i, end - start); */
/*     } */

/* ======= */
/* >>>>>>> Stashed changes */
    /* start = current_time_(); */
    /* net_recv(fd, &insts->instr[1000], 427 * sizeof(Instruction), 0); */
    /* end = current_time_(); */
    /* fprintf(stderr, "\tSecond final 427: %llu\n", end - start); */

    /* END ADDING HERE */
    net_recv(fd, insts->instr, sizeof(Instruction) * insts->size, 0);
    net_recv(fd, *offsets, sizeof(block) * noffsets, 0);
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
computeOutputs(const OutputInstructions *ois, int *output,
               block **computed_outputmap)
{
    assert(output && "output's memory should be allocated");

    print_output_instructions(ois);
    for (uint16_t i = 0; i < ois->size; ++i) {
        AES_KEY key;
        block out[2], b_zero, b_one;
        OutputInstruction *oi = &ois->output_instruction[i];

        // decrypt using comp_block as key
        block comp_block = computed_outputmap[oi->gc_id][oi->wire_id];

        printf("comp block: ");
        print_block(stdout, comp_block);
        printf("\n");

        AES_set_decrypt_key(comp_block, &key);
        out[0] = oi->labels[0];
        out[1] = oi->labels[1];
        AES_ecb_decrypt_blks(out, 2, &key);

        b_zero = zero_block();
        b_one = makeBlock((uint64_t) 0, (uint64_t) 1); // 000...00001


        printf("dec block0: ");
        print_block(stdout, out[0]);
        printf("\n");
        printf("dec block1: ");
        print_block(stdout, out[1]);
        printf("\n");


        if (equal_blocks(out[0], b_zero) || equal_blocks(out[1], b_zero)) {
            output[i] = 0;
        } else if (equal_blocks(out[0], b_one) || equal_blocks(out[1], b_one)) {
            output[i] = 1;
        } else {
            fprintf(stderr, "Could not compute output[%d] from (gc_id: %d, wire_id: %d)\n",
                    i, oi->gc_id, oi->wire_id);
            assert(false);
            return FAILURE;
        }
    }
    return SUCCESS;
}

int
evaluator_online(char *dir, const int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, uint64_t *tot_time, ChainingType chainingType)
{
    ChainedGarbledCircuit* chained_gcs;
    FunctionSpec function;
    block *garb_labels = NULL, *eval_labels = NULL,
        *outputmap = NULL, **labels = NULL, *offsets = NULL;
    int *corrections = NULL, *circuitMapping, sockfd;
    uint64_t start, end, _start, _end, loading_time;
    int num_garb_inputs = 0; /* later received from garbler */
    size_t tmp;

    /* start timing after socket connection */
    _start = current_time_();
    {
        /* Load things from disk */
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        loadChainedGarbledCircuits(chained_gcs, num_chained_gcs, dir, chainingType);
        loadOTPreprocessing(&eval_labels, &corrections, dir);
        labels = malloc(sizeof(block *) * (num_chained_gcs + 1));
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            labels[i] = allocate_blocks(chained_gcs[i-1].gc.n);
        }
    }
    _end = current_time_();
    loading_time = _end - _start;
    fprintf(stderr, "Load components: %llu\n", (_end - _start));

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    start = current_time_();

    /* Receive eval labels: OT correction */
    _start = current_time_();
    if (num_eval_inputs > 0) {
        block *recvLabels;
        uint64_t _start, _end;
        for (int i = 0; i < num_eval_inputs; ++i)
            corrections[i] ^= eval_inputs[i];
        recvLabels = allocate_blocks(2 * num_eval_inputs);
        /* valgrind is saying corrections is unitialized, but it seems to be */
        _start = current_time_();
        {
            net_send(sockfd, corrections, sizeof(int) * num_eval_inputs, 0);
        }
        _end = current_time_();
        fprintf(stderr, "OT correction (send): %llu\n", _end - _start);
        _start = current_time_();
        {
            tmp = g_bytes_received;
            net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_inputs, 0);
        }
        _end = current_time_();
        fprintf(stderr, "OT correction (receive): %llu\n", _end - _start);
        fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

        for (int i = 0; i < num_eval_inputs; ++i) {
            eval_labels[i] = xorBlocks(eval_labels[i],
                                       recvLabels[2 * i + eval_inputs[i]]);
        }
        free(recvLabels);
    }
    free(corrections);
    _end = current_time_();
    fprintf(stderr, "OT correction: %llu\n", _end - _start);

    /* Receive circuit mapping which maps instructions-gc-id to disk-gc-id */
    _start = current_time_();
    {
        int size;
        tmp = g_bytes_received;
        net_recv(sockfd, &size, sizeof(int), 0);
        circuitMapping = malloc(sizeof(int) * size);
        net_recv(sockfd, circuitMapping, sizeof(int) * size, 0);
    }
    _end = current_time_();
    fprintf(stderr, "Receive circuit map: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    /* receive garbler labels */
    _start = current_time_();
    {
        tmp = g_bytes_received;
        net_recv(sockfd, &num_garb_inputs, sizeof(int), 0);
        if (num_garb_inputs > 0) {
            garb_labels = allocate_blocks(sizeof(block) * num_garb_inputs);
            net_recv(sockfd, garb_labels, sizeof(block) * num_garb_inputs, 0);
        }
    }
    _end = current_time_();
    fprintf(stderr, "Receive garbler labels: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    /* Receive output instructions */
    OutputInstructions output_instructions;
    _start = current_time_();
    {
        tmp = g_bytes_received;
        net_recv(sockfd, &output_instructions.size, 
                sizeof(output_instructions.size), 0);
        output_instructions.output_instruction = 
            malloc(output_instructions.size * sizeof(OutputInstruction));

        net_recv(sockfd, output_instructions.output_instruction, 
                output_instructions.size * sizeof(OutputInstruction), 0);
    }
    _end = current_time_();
    fprintf(stderr, "Receive output instructions: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    _start = current_time_();
    {
        tmp = g_bytes_received;
        recvInstructions(sockfd, &function.instructions, &offsets);
    }
    _end = current_time_();
    fprintf(stderr, "Receive instructions: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    /* Done with socket, so close */
    close(sockfd);

    /* Follow instructions and evaluate */
    _start = current_time_();
    block **computedOutputMap = malloc(sizeof(block *) * (num_chained_gcs + 1));
    {
        computedOutputMap[0] = allocate_blocks(num_garb_inputs + num_eval_inputs);
        labels[0] = allocate_blocks(num_garb_inputs + num_eval_inputs);
        memcpy(&computedOutputMap[0][0], garb_labels, sizeof(block) * num_garb_inputs);
        memcpy(&computedOutputMap[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);
        memcpy(&labels[0][0], garb_labels, sizeof(block) * num_garb_inputs);
        memcpy(&labels[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            computedOutputMap[i] = allocate_blocks(chained_gcs[i-1].gc.m);
        }
        evaluator_evaluate(chained_gcs, num_chained_gcs, &function.instructions,
                           labels, circuitMapping, computedOutputMap, offsets, chainingType);
    }
    _end = current_time_();
    fprintf(stderr, "Evaluate: %llu\n", _end - _start);

    _start = current_time_();
    int *output = calloc(output_instructions.size, sizeof(int));
    {
        int res = computeOutputs(&output_instructions, output, computedOutputMap);
        if (res == FAILURE) {
            fprintf(stderr, "computeOutputs failed\n");
            return FAILURE;
        }

    }
    _end = current_time_();
    fprintf(stderr, "Map outputs: %llu\n", _end - _start);

    /* cleanup */
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

    end = current_time_();
    fprintf(stderr, "Total (post connection): %llu\n", end - start);
    fprintf(stderr, "Bytes sent: %lu\n", g_bytes_sent);
    fprintf(stderr, "Bytes received: %lu\n", g_bytes_received);
    if (tot_time)
        *tot_time = end - start + loading_time;
    return SUCCESS;
}

