#include "2pc_evaluator.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <unistd.h> // sleep
#include <time.h>
#include <string.h>
#include <garble.h>
#include <garble/aes.h>

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
        switch(cur->type) {
        case EVAL:
            s = current_time_();
            savedCircId = circuitMapping[cur->ev.circId];

            garble_eval(&chained_gcs[savedCircId].gc, labels[cur->ev.circId],
                        computedOutputMap[cur->ev.circId], NULL);

            e = current_time_();
            eval_time += e - s;
            break;
        case CHAIN:

            if (chainingType == CHAINING_TYPE_STANDARD) {
                labels[cur->ch.toCircId][cur->ch.toWireId] = garble_xor(
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
                    labels[cur->ch.toCircId][k] = garble_xor(
                        computedOutputMap[cur->ch.fromCircId][j],
                        offsets[offsetIdx]);

                    if (cur->ch.fromCircId != 0) { /* if not the input component */
                        labels[cur->ch.toCircId][k] = garble_xor(
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

void
evaluator_classic_2pc(const int *input, bool *output,
                      int num_garb_inputs, int num_eval_inputs,
                      uint64_t *tot_time)
{
    int sockfd;
    int *selections = NULL;
    garble_circuit gc;
    OldInputMapping map;
    block *garb_labels = NULL, *eval_labels = NULL;
    block *labels, *output_map;
    uint64_t start, end, _start, _end;
    size_t tmp;

    gc.type = GARBLE_TYPE_STANDARD;

    if ((sockfd = net_init_client(HOST, PORT)) == FAILURE) {
        perror("net_init_client");
        exit(EXIT_FAILURE);
    }

    /* pre-process OT */
    if (num_eval_inputs > 0) {
        struct state state;
        state_init(&state);
        selections = calloc(num_eval_inputs, sizeof(int));
        eval_labels = garble_allocate_blocks(num_eval_inputs);
        for (int i = 0; i < num_eval_inputs; ++i) {
            selections[i] = rand() % 2;
        }
        ot_np_recv(&state, sockfd, selections, num_eval_inputs, sizeof(block),
                   2, eval_labels, new_choice_reader, new_msg_writer);
        state_cleanup(&state);
    }

    /* Start timing after pre-processing of OT as we only want to record online
     * time */
    start = current_time_();

    _start = current_time_();
    {
        tmp = g_bytes_received;
        gc_comm_recv(sockfd, &gc);
    }
    _end = current_time_();
    fprintf(stderr, "Receive GC: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    _start = current_time_();
    tmp = g_bytes_received;
    if (num_eval_inputs > 0) {
        block *recvLabels;
        for (int i = 0; i < num_eval_inputs; ++i) {
            selections[i] ^= input[i];
        }
        recvLabels = garble_allocate_blocks(2 * num_eval_inputs);
        net_send(sockfd, selections, sizeof(int) * num_eval_inputs, 0);
        net_recv(sockfd, recvLabels, sizeof(block) * 2 * num_eval_inputs, 0);
        for (int i = 0; i < num_eval_inputs; ++i) {
            eval_labels[i] = garble_xor(eval_labels[i],
                                        recvLabels[2 * i + input[i]]);
        }
        free(recvLabels);
        free(selections);
    }
    _end = current_time_();
    fprintf(stderr, "OT correction: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    _start = current_time_();
    tmp = g_bytes_received;
    if (num_garb_inputs > 0) {
        garb_labels = garble_allocate_blocks(num_garb_inputs);
        net_recv(sockfd, garb_labels, sizeof(block) * num_garb_inputs, 0);
    }
    _end = current_time_();
    fprintf(stderr, "Receive garbler labels: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    _start = current_time_();
    {
        tmp = g_bytes_received;
        output_map = garble_allocate_blocks(2 * gc.m);
        net_recv(sockfd, output_map, sizeof(block) * 2 * gc.m, 0);
    }
    _end = current_time_();
    fprintf(stderr, "Receive output map: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    _start = current_time_();
    tmp = g_bytes_received;
    {
        size_t size;
        char *buffer;

        net_recv(sockfd, &size, sizeof size, 0);
        buffer = malloc(size);
        net_recv(sockfd, buffer, size, 0);
        readBufferIntoInputMapping(&map, buffer);
        free(buffer);
    }
    _end = current_time_();
    fprintf(stderr, "Receive garbler labels: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);

    close(sockfd);
    
    /* Plug labels in correctly based on input_mapping */
    {
        labels = garble_allocate_blocks(gc.n);
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

    _start = current_time_();
    {
        bool *outputs = calloc(gc.m, sizeof(bool));
        garble_eval(&gc, labels, NULL, outputs);
        free(outputs);
    }
    _end = current_time_();
    fprintf(stderr, "Evaluate: %llu\n", _end - _start);

    garble_delete(&gc);
    deleteOldInputMapping(&map);
    free(output_map);
    free(eval_labels);
    free(garb_labels);
    free(labels);

    end = current_time_();
    *tot_time = end - start;
}

void
evaluator_offline(char *dir, int num_eval_inputs, int nchains,
                  ChainingType chainingType)
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
        char *fname;
        size_t size;

        selections = malloc(sizeof(int) * num_eval_inputs);

        size = strlen(dir) + strlen("/sel") + 1;
        fname = malloc(size);

        for (int i = 0; i < num_eval_inputs; ++i) {
            selections[i] = rand() % 2;
        }
        evalLabels = garble_allocate_blocks(num_eval_inputs);
        ot_np_recv(&state, sockfd, selections, num_eval_inputs, sizeof(block),
                   2, evalLabels, new_choice_reader, new_msg_writer);
        (void) snprintf(fname, size, "%s/%s", dir, "sel");
        saveOTSelections(fname, selections, num_eval_inputs);
        (void) snprintf(fname, size, "%s/%s", dir, "lbl");
        saveOTLabels(fname, evalLabels, num_eval_inputs, false);

        free(selections);
        free(evalLabels);
        free(fname);
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
    *offsets = garble_allocate_blocks(noffsets);

    net_recv(fd, insts->instr, sizeof(Instruction) * insts->size, 0);
    net_recv(fd, *offsets, sizeof(block) * noffsets, 0);
}

static void 
loadChainedGarbledCircuits(ChainedGarbledCircuit *cgc, int ncgcs, char *dir,
                           ChainingType chainingType) 
{
    /* Loads chained garbled circuits from disk, assuming the loader is the evaluator */
    for (int i = 0; i < ncgcs; ++i) {
        loadChainedGC(&cgc[i], dir, i, false, chainingType);
    }
}

static void
loadOTPreprocessing(block **eval_labels, int **corrections, char *dir)
{
    size_t size;
    char *fname;

    size = strlen(dir) + strlen("/sel") + 1;
    fname = malloc(size);
    (void) snprintf(fname, size, "%s/%s", dir, "sel");
    *corrections = loadOTSelections(fname);
    (void) snprintf(fname, size, "%s/%s", dir, "lbl");
    *eval_labels = loadOTLabels(fname);
    free(fname);
}

static int
computeOutputs(const OutputInstructions *ois, int *output,
               block **computed_outputmap)
{
    assert(output && "output's memory should be allocated");

    for (uint16_t i = 0; i < ois->size; ++i) {
        AES_KEY key;
        block out[2], b_zero, b_one;
        OutputInstruction *oi = &ois->output_instruction[i];

        // decrypt using comp_block as key
        block comp_block = computed_outputmap[oi->gc_id][oi->wire_id];

        /* XXX: huh?  why does calling AES_set_decrypt_key not work!? */
        /* AES_set_decrypt_key(comp_block, &key); */
        {
            AES_KEY temp_key;
            AES_set_encrypt_key(comp_block, &temp_key);
            AES_set_decrypt_key_fast(&key, &temp_key);
        }
        out[0] = oi->labels[0];
        out[1] = oi->labels[1];
        AES_ecb_decrypt_blks(out, 2, &key);

        b_zero = garble_zero_block();
        b_one = garble_make_block((uint64_t) 0, (uint64_t) 1); // 000...00001

        if (garble_equal(out[0], b_zero) || garble_equal(out[1], b_zero)) {
            output[i] = 0;
        } else if (garble_equal(out[0], b_one) || garble_equal(out[1], b_one)) {
            output[i] = 1;
        } else {
            fprintf(stderr, "Could not compute output[%d] from (gc_id: %d, wire_id: %d)\n",
                    i, oi->gc_id, oi->wire_id);
            /* assert(false); */
            return FAILURE;
        }
    }
    return SUCCESS;
}

int
evaluator_online(char *dir, const int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, ChainingType chainingType,
                 uint64_t *tot_time, uint64_t *tot_time_no_load)
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
            labels[i] = garble_allocate_blocks(chained_gcs[i-1].gc.n);
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
        recvLabels = garble_allocate_blocks(2 * num_eval_inputs);
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
            eval_labels[i] = garble_xor(eval_labels[i],
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
            garb_labels = garble_allocate_blocks(sizeof(block) * num_garb_inputs);
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
        computedOutputMap[0] = garble_allocate_blocks(num_garb_inputs + num_eval_inputs);
        labels[0] = garble_allocate_blocks(num_garb_inputs + num_eval_inputs);
        memcpy(&computedOutputMap[0][0], garb_labels, sizeof(block) * num_garb_inputs);
        memcpy(&computedOutputMap[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);
        memcpy(&labels[0][0], garb_labels, sizeof(block) * num_garb_inputs);
        memcpy(&labels[0][num_garb_inputs], eval_labels, sizeof(block) * num_eval_inputs);
        for (int i = 1; i < num_chained_gcs + 1; i++) {
            computedOutputMap[i] = garble_allocate_blocks(chained_gcs[i-1].gc.m);
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
            /* return FAILURE; */
        }
    }
    free(output_instructions.output_instruction);
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
    if (tot_time)
        *tot_time = end - start + loading_time;
    if (tot_time_no_load)
        *tot_time_no_load = end - start;
    return SUCCESS;
}

