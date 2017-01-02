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
    /* Evaluates the chained garbled circuits using the array of chained_gcs, the instructions,
     * input labels, and other information provided. Ignoring abstractions, this 
     * function populates the array computedOutputMap which holds
     * the output blocks of each garbled circuit. With these blocks, 
     * and extra information on the semantic value of each block 
     * (i.e. does it represent 0 or 1),
     * the evaluator can determine the real output of the component-based gc system.
     * Note: the indices provided by CircuitMapping should only used 
     * when evaluating. In all other places, the indices used to designate the
     * chained garbled circuits are consistent with the indices stated in 
     * the instructions instructions. circuitMapping maps the indice of the instruction
     * to the correct index of the chainedGarbledCircuit as saved on disk.
     *
     * Inputs:
     * @param chained_gcs an array of chained garbled circuits that will be evaluated
     * @param num_chained_gcs the length of the chained_gcs array
     * @param instructions a list of instructions on how to evaluate and chain
     * @param labels a 2-d array of the input labels to each garbled circuit
     *        lables[i] is an array of input labels for chained_gcs[i].
     * @param circuitMapping a mapping of garbled circuit indices from those 
     *        used in the instructions to those used to save the garbled circuits to disk.
     * @param computedOutputMap a 2-d array of the output labels of each garbled circuit, 
     *        where computedOutputMap[i] is the list of output labels of gc i
     * @param offsets otherwise known as the chaining mask, the offsets are the
     *        blocks needed to map output labels of one gc to input labels of another gc.
     *
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

                // if mapping inputs
                if (cur->ch.fromCircId == 0) {
                    int offsetIdx = cur->ch.offsetIdx;
                    for (int j = cur->ch.fromWireId, k = cur->ch.toWireId; 
                         j < cur->ch.fromWireId + cur->ch.wireDist;
                         ++j, ++k) {

                        labels[cur->ch.toCircId][k] = garble_xor(
                            computedOutputMap[cur->ch.fromCircId][j],
                            offsets[offsetIdx]);
                    }

                

                } else {
                    // if not inputs:

                    block b = garble_xor(
                        computedOutputMap[cur->ch.fromCircId][cur->ch.fromWireId], 
                        offsets[cur->ch.offsetIdx]);
                    memcpy(&labels[cur->ch.toCircId][cur->ch.toWireId], &b, sizeof(b));
                }

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
evaluator_classic_2pc(garble_circuit *gc, const int *input, bool *output,
                      int num_garb_inputs, int num_eval_inputs,
                      uint64_t *tot_time)
{
    /* Does the "full" garbled circuit protocol, wherein there is no online
     * phase.  The garbled circuit and all input labels are communicated during
     * the online phase, although we do use OT-processing
     */

    int sockfd;
    int *selections = NULL;
    OldInputMapping map;
    block *garb_labels = NULL, *eval_labels = NULL;
    block *labels, *output_map;
    uint64_t start, end, _start, _end;
    size_t tmp;

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
        gc_comm_recv(sockfd, gc);
    }
    _end = current_time_();
    fprintf(stderr, "Receive GC: %llu\n", _end - _start);
    fprintf(stderr, "\tBytes: %lu\n", g_bytes_received - tmp);
    fflush(stderr);

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
        output_map = garble_allocate_blocks(2 * gc->m);
        net_recv(sockfd, output_map, sizeof(block) * 2 * gc->m, 0);
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
        labels = garble_allocate_blocks(gc->n);
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
        bool *outputs = calloc(gc->m, sizeof(bool));
        garble_eval(gc, labels, NULL, outputs);
        free(outputs);
    }
    _end = current_time_();
    fprintf(stderr, "Evaluate: %llu\n", _end - _start);

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
    /* Does the offline stage for the evaluator.
     * This includes receiving pre-exchanged garbled circuits,
     * performing the offline phase of OT-preprocessing, and
     * saving relevant information to disk.
     *
     * @param dir the directory to save information
     * @param num_eval_inputs the number of evaluator inputs
     * @param nchains the number of garbled circuits to be pre-exchanged
     * @param chainingType indicates whether we are using SIMD or standard chaining.
     *        WARNING: SIMD-style chaining is now deprecated.
     */
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
    /* Performs the online stage of the evaluator.
     * The first part of the function loads data from disk
     * that was saved during the online phase. 
     * Next, perform OT correction to acquire input labels
     * corresponding to the evaluators' inputs.
     * Next, receive instructions on how to evaluate and chain 
     * the garbled circuits.
     * Next, receive garbler labels and the output instructions.
     * The output instructions are unary gates mapping output labels to 0 or 1.
     * Next, we call evaluator_evaluate to evaluate the garbled circuits using 
     * the information acquired.
     * Finally, use the output instructions to process the output of the 
     * evaluation subprocedure to acquire actual bits of the output.
     *
     * @param dir the director which OT and gc data are saved during the offline phase
     * @param eval_inputs an array of the evaluator's inputs; either 0 or 1
     * @param num_eval_inputs the length of the eval_inputs array
     * @param num_chained_gcs the number of garbled circuits saved to disk.
     * @param chainingType indicates whether to do standard or SIMD-style chaining.
     *        WARNING: SIMD-style chaining is deprecated.
     * @param tot_time an unpopulated int* (of length 1). evaluator_online populates
     *        value with the total amount of time it took to evaluate.
     * @param tot_time_no_load an unpopulated int* (of length 1). evaluator_online populates
     *        value with the total amount of time it took to evaluate, not including 
     *        the time to load data from disk.
     */
    ChainedGarbledCircuit* chained_gcs;
    FunctionSpec function;
    block *garb_labels = NULL, *eval_labels = NULL,
        *outputmap = NULL, *offsets = NULL;
    int *corrections = NULL, *circuitMapping, sockfd;
    uint64_t start, end, _start, _end, loading_time;
    int num_garb_inputs = 0; /* later received from garbler */
    size_t tmp;

    block *labels[num_chained_gcs + 1];

    /* start timing after socket connection */
    _start = current_time_();
    {
        /* Load things from disk */
        chained_gcs = calloc(num_chained_gcs, sizeof(ChainedGarbledCircuit));
        loadChainedGarbledCircuits(chained_gcs, num_chained_gcs, dir, chainingType);
        loadOTPreprocessing(&eval_labels, &corrections, dir);
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
        for (int i = 0; i < num_eval_inputs; ++i) {
            assert(corrections[i] == 0 || corrections[i] == 1);
            assert(eval_inputs[i] == 0 || eval_inputs[i] == 1);
            corrections[i] ^= eval_inputs[i];
        }
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
        computedOutputMap[i] = NULL;
    }
    //free(labels[num_chained_gcs]);

    free(computedOutputMap[num_chained_gcs]);
    free(chained_gcs);
    free(computedOutputMap);
    free(outputmap);
    free(output);
    free(garb_labels);
    free(eval_labels);

    if (function.instructions.instr) {
        free(function.instructions.instr);
        function.instructions.instr = NULL;
    }

    free(offsets);

    end = current_time_();
    if (tot_time)
        *tot_time = end - start + loading_time;
    if (tot_time_no_load)
        *tot_time_no_load = end - start;
    return SUCCESS;
}

