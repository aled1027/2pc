#include "2pc_function_spec.h"

#include <assert.h>
#include <string.h>

#include "utils.h"

InstructionType 
get_instruction_type_from_string(const char* type) 
{
    if (strcmp(type, "EVAL") == 0) {
        return EVAL;
    } else if (strcmp(type, "CHAIN") == 0) {
        return CHAIN;
    } else {
        return INSTR_ERR;
    }
}

CircuitType 
get_circuit_type_from_string(const char* type) 
{
    if (strcmp(type, "22Adder") == 0) {
        return ADDER22;
    } else if (strcmp(type, "23Adder") == 0) {
        return ADDER23;
    } else if (strcmp(type, "AES_ROUND") == 0) {
        return AES_ROUND;
    } else if (strcmp(type, "AES_FINAL_ROUND") == 0) {
        return AES_FINAL_ROUND;
    } else if (strcmp(type, "XOR") == 0) {
        return XOR;
    } else if (strcmp(type, "FULL_CBC") == 0) {
        return FULL_CBC;
    } else if (strcmp(type, "LEVEN_CORE") == 0) {
        return LEVEN_CORE;
    } else if (strcmp(type, "INNER_PRODUCT") == 0) {
        return INNER_PRODUCT;
    } else if (strcmp(type, "SIGNED_COMPARISON") == 0) {
        return SIGNED_COMPARISON;
    } else if (strcmp(type, "AND") == 0) {
        return AND;
    } else if (strcmp(type, "GR0") == 0) {
        return GR0;
    } else if (strcmp(type, "NOT") == 0) {
        return NOT;
    } else {
        fprintf(stderr, "circuit type error when loading json: can't detect %s\n", type);
        return CIRCUIT_TYPE_ERR;
    }
}

int 
freeFunctionSpec(FunctionSpec* function) 
{
    /* Free components */
    for (int i = 0; i < function->components.numComponentTypes; i++)
        free(function->components.circuitIds[i]);
    free(function->components.circuitType);
    free(function->components.nCircuits);
    free(function->components.circuitIds);
    
    /* Free input_mapping */
    free(function->input_mapping.imap_instr);

    /* Free instructions */
    free(function->instructions.instr);

    free(function->output_instructions.output_instruction);

    return SUCCESS;
}
void 
print_metadata(FunctionSpec *function)
{
    printf("n: %d\n", function->n);
    printf("m: %d\n", function->m);
    printf("num_garb_inputs: %d\n", function->num_garb_inputs);
    printf("num_eval_inputs: %d\n", function->num_eval_inputs);
}

void
print_components(FunctionComponent* components) 
{
    printf("numComponentsTypes: %d\n", components->numComponentTypes);
    printf("totComponents: %d\n", components->totComponents);
    for (int i = 0; i < components->numComponentTypes; i++) {
        printf("component type: %d\t", components->circuitType[i]);
        printf("nCircuits: %d\t", components->nCircuits[i]);
        printf("circuitIds: ");
        for (int j = 0; j < components->nCircuits[i]; j++) {
            printf("%d, ", components->circuitIds[i][j]);
        }
        printf("\n");
    }
}

void 
print_input_mapping(InputMapping* inputMapping) 
{
    printf("InputMapping size: %d\n", inputMapping->size);
    for (int i = 0; i < inputMapping->size; i++) {
        char person[40] = "Garbler  ";
        InputMappingInstruction *cur = &inputMapping->imap_instr[i];
        if (cur->inputter == PERSON_EVALUATOR) 
            strcpy(person, "Evaluator");


        printf("%s input %d -> (gc %d, wire %d, dist %d)\n", 
                person, 
                cur->input_idx, 
                cur->gc_id, 
                cur->wire_id,
                cur->dist);
    }
}

void
print_instruction(const Instruction *in)
{
    switch(in->type) {
        case EVAL:
            printf("EVAL %d\n", in->ev.circId);
            break;
        case CHAIN:
            printf("CHAIN (%d, %d) -> (%d, %d) with offset (%d) and dist (%d)\n", 
                    in->ch.fromCircId, 
                    in->ch.fromWireId,
                    in->ch.toCircId,
                    in->ch.toWireId,
                    in->ch.offsetIdx,
                    in->ch.wireDist);
            break;
        default:
            printf("Not printing command\n");
    }
}

void 
print_instructions(const Instructions* instr) 
{
    printf("Instructions:\n");
    for (int i = 0; i < instr->size; i++) {
        print_instruction(&instr->instr[i]);
    }
}

void
print_output_instructions(const OutputInstructions *ois)
{
    printf("Num output instructions: %d\n", ois->size);
    OutputInstruction *oi;
    for (int i = 0; i < ois->size; i++) {
        oi = &ois->output_instruction[i];
        printf("oi[%d] = (gc_id: %d, wire_id: %d ",
                i,
                oi->gc_id,
                oi->wire_id);
        /* print_block(fp, oi->labels[0]); */
        /* printf(" "); */
        /* print_block(fp, oi->labels[1]); */
        printf("\n");
    }
}

int 
json_load_metadata(json_t *root, FunctionSpec *function)
{
    json_t *j_metadata, *j_ptr;
    j_metadata = json_object_get(root, "metadata");

    j_ptr = json_object_get(j_metadata, "n");
    assert(json_is_integer(j_ptr));
    function->n = json_integer_value(j_ptr);

    j_ptr = json_object_get(j_metadata, "m");
    assert(json_is_integer(j_ptr));
    function->m = json_integer_value(j_ptr);

    j_ptr = json_object_get(j_metadata, "num_garb_inputs");
    assert(json_is_integer(j_ptr));
    function->num_garb_inputs = json_integer_value(j_ptr);

    j_ptr = json_object_get(j_metadata, "num_eval_inputs");
    assert(json_is_integer(j_ptr));
    function->num_eval_inputs = json_integer_value(j_ptr);

    return SUCCESS;
}

int 
json_load_components(json_t *root, FunctionSpec *function) 
{
    json_t *jComponents, *jComponent, *jPtr, *jCircuitIds;

    FunctionComponent *components = &function->components;

    jComponents = json_object_get(root, "components");
    
    int size = json_array_size(jComponents);
    components->circuitType = (CircuitType*) malloc(sizeof(CircuitType) * size);
    components->circuitIds = (int**) malloc(sizeof(int*) * size);
    components->nCircuits = (int*) allocate_ints(size);
    components->numComponentTypes = size;
    components->totComponents = 0;

    for (int i = 0; i < size; i++) {
        jComponent = json_array_get(jComponents, i);
        assert(json_is_object(jComponent));

        /*get type*/
        jPtr = json_object_get(jComponent, "type");
        assert(json_is_string(jPtr));
        const char* type = json_string_value(jPtr);
        components->circuitType[i] = get_circuit_type_from_string(type);

        /*get number of circuits of type needed*/
        jPtr = json_object_get(jComponent, "num");
        assert (json_is_integer(jPtr));
        components->nCircuits[i] = json_integer_value(jPtr);
        components->totComponents += components->nCircuits[i];

        /*get circuit ids*/
        jCircuitIds = json_object_get(jComponent, "circuit_ids");
        assert(json_is_array(jCircuitIds));

        components->circuitIds[i] = allocate_ints(components->nCircuits[i]);
        for (int j = 0; j < components->nCircuits[i]; j++) {
            jPtr = json_array_get(jCircuitIds, j);
            components->circuitIds[i][j] = json_integer_value(jPtr);
        }
    }
    return SUCCESS;
}

int
json_load_output(json_t *root, FunctionSpec *function) 
{

    json_t *jOutputs, *jOutput, *jPtr;
    OutputInstructions *output_instructions = &function->output_instructions;

    jOutputs = json_object_get(root, "output");
    assert(json_is_array(jOutputs));

    int array_size = json_array_size(jOutputs);
    output_instructions->size = function->m;

    output_instructions->output_instruction = 
        malloc(output_instructions->size * sizeof(OutputInstruction));

    int idx = 0;
    for (int i = 0; i < array_size; i++) {
        jOutput = json_array_get(jOutputs, i);
        assert(json_is_object(jOutput));

        jPtr = json_object_get(jOutput, "gc_id");
        assert(json_is_integer(jPtr));
        int gc_id = json_integer_value(jPtr);

        jPtr = json_object_get(jOutput, "start_wire_idx");
        assert(json_is_integer(jPtr));
        int start_wire_idx = json_integer_value(jPtr);

        jPtr = json_object_get(jOutput, "end_wire_idx");
        assert(json_is_integer(jPtr));
        int end_wire_idx = json_integer_value(jPtr);
        /* printf("start_wire_idx: %d\n", start_wire_idx); */
        /* printf("end_wire_idx: %d\n", end_wire_idx); */

        /* plus one because inclusive */
        for (int j = start_wire_idx; j < end_wire_idx + 1; ++j) {
            output_instructions->output_instruction[idx].gc_id = gc_id;
            output_instructions->output_instruction[idx].wire_id = j;
            ++idx;
        }
    }
    assert(idx == output_instructions->size);
    return SUCCESS;

}

int 
json_load_input_mapping(json_t *root, FunctionSpec* function) 
{
    /* Loads in the input mapping section from the json into
     * the function
     */
    InputMapping* imap = &(function->input_mapping);
    json_t *jInputMapping, *jMap, *jGcId, *jWireIdx, *jInputter, *jInputIdx, *jPtr, *jMetadata;

    jMetadata = json_object_get(root, "metadata");
    jPtr = json_object_get(jMetadata, "input_mapping_size");
    assert(json_is_integer(jPtr));
    imap->size = json_integer_value(jPtr);

    jInputMapping = json_object_get(root, "input_mapping");
    assert(json_is_array(jInputMapping));
    int loop_size = json_array_size(jInputMapping);  // size is subject to change in for loop
    assert(loop_size == imap->size);  // don't need metadata:input_mapping_size, (maybe used in loading instructions)
    imap->imap_instr = malloc(imap->size * sizeof(InputMappingInstruction));
    assert(imap->imap_instr);
    bool *hasInputBeenUsed[2];
    hasInputBeenUsed[0]= calloc(function->n, sizeof(bool));
    hasInputBeenUsed[1]= calloc(function->n, sizeof(bool));

    int idx = 0;
    for (int i = 0; i < loop_size; i++) {
        // Get info from the json pointers
        jMap = json_array_get(jInputMapping, i);
        assert(json_is_object(jMap));
        
        jInputIdx = json_object_get(jMap, "start_input_idx");
        assert(json_is_integer(jInputIdx));
        int start_input_idx = json_integer_value(jInputIdx);
        
        jInputIdx = json_object_get(jMap, "end_input_idx");
        assert(json_is_integer(jInputIdx));
        int end_input_idx = json_integer_value(jInputIdx);

        jGcId = json_object_get(jMap, "gc_id");
        assert(json_is_integer(jGcId));
        int gc_id = json_integer_value(jGcId);
        
        jWireIdx = json_object_get(jMap, "start_wire_idx");
        assert(json_is_integer(jWireIdx));
        int start_wire_idx = json_integer_value(jWireIdx);
        
        jWireIdx = json_object_get(jMap, "end_wire_idx");
        assert(json_is_integer(jWireIdx));
        int end_wire_idx = json_integer_value(jWireIdx);
        assert(end_input_idx - start_input_idx == end_wire_idx - start_wire_idx);
        
        jInputter = json_object_get(jMap, "inputter");
        assert(json_is_string(jInputter));
        const char* the_inputter = json_string_value(jInputter);
        
        Person inputter;
        if (strcmp(the_inputter, "garbler") == 0) {
            inputter = PERSON_GARBLER;
        } else if (strcmp(the_inputter, "evaluator") == 0) {
            inputter = PERSON_EVALUATOR;
        } else { 
            fprintf(stderr, "person error\n");
            return FAILURE;
        }

        if (hasInputBeenUsed[inputter][start_input_idx] == true) {
            // then we need to make separate instructions for each input
            // because each input requires a unique offset
            int new_memory = end_input_idx - start_input_idx;
            imap->size += new_memory;
            imap->imap_instr = realloc(imap->imap_instr, imap->size *  sizeof(InputMappingInstruction));
            for (int j = 0; j < new_memory + 1; j++) {
                imap->imap_instr[idx].input_idx = start_input_idx + j;
                imap->imap_instr[idx].gc_id = gc_id;
                imap->imap_instr[idx].inputter = inputter;
                imap->imap_instr[idx].dist = 1;
                imap->imap_instr[idx].wire_id = start_wire_idx + j;
                ++idx;
            }
        } else {
            imap->imap_instr[idx].input_idx = start_input_idx;
            imap->imap_instr[idx].gc_id = gc_id;
            imap->imap_instr[idx].inputter = inputter;
            imap->imap_instr[idx].dist = end_input_idx - start_input_idx + 1;
            imap->imap_instr[idx].wire_id = start_wire_idx;
            ++idx;
            hasInputBeenUsed[inputter][start_input_idx] = true;
        }
    }
    free(hasInputBeenUsed[0]);
    free(hasInputBeenUsed[1]);
    return SUCCESS;
}

int 
json_load_instructions(json_t *root, FunctionSpec *function, ChainingType chainingType) 
{
    /* preliminary work */
    Instructions* instructions = &(function->instructions);
    InputMapping *imap = &function->input_mapping;
    json_t *jInstructions, *jInstr, *jPtr, *jMetadata;
    const char* sType;
    jMetadata = json_object_get(root, "metadata");
    jPtr = json_object_get(jMetadata, "instructions_size");
    assert(json_is_integer(jPtr));
    int num_instructions = json_integer_value(jPtr);
    jInstructions = json_object_get(root, "instructions");
    assert(json_is_array(jInstructions));
    int loop_size = json_array_size(jInstructions); 

    if (chainingType == CHAINING_TYPE_STANDARD) {
        instructions->size = num_instructions + imap->size;
    } else {
        instructions->size = imap->size + loop_size;
    }
    instructions->instr = malloc(instructions->size * sizeof(Instruction));
    assert(instructions->instr);

    /* Load input mapping instructions */
    for (int i = 0; i < imap->size; ++i) {
        InputMappingInstruction *cur = &imap->imap_instr[i]; 
        instructions->instr[i].type = CHAIN;
        instructions->instr[i].ch.fromCircId = 0;
        instructions->instr[i].ch.fromWireId = (cur->inputter == PERSON_GARBLER) ?  
                            cur->input_idx : cur->input_idx + function->num_garb_inputs;
        instructions->instr[i].ch.toCircId = cur->gc_id;
        instructions->instr[i].ch.toWireId = cur->wire_id;
        instructions->instr[i].ch.wireDist = cur->dist;
        instructions->instr[i].ch.offsetIdx = 0;
    }

    /* Add normal chaining and evaluating instructions as specified by json */
    int idx = imap->size;
    for (int i = 0; i < loop_size; ++i) {
        jInstr = json_array_get(jInstructions, i);
        assert(json_is_object(jInstr));

        jPtr = json_object_get(jInstr, "type");
        assert(json_is_string(jPtr));
        sType = json_string_value(jPtr);
        InstructionType instr_type = get_instruction_type_from_string(sType);
        switch (instr_type) {
            case EVAL:
                instructions->instr[idx].type = instr_type;
                jPtr = json_object_get(jInstr, "gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[idx].ev.circId = json_integer_value(jPtr);
                idx++;
                break;
            case CHAIN:
                // We have a map (from_gc_id, [from_wire_id_start : from_wire_id_end])
                //          ---> (to_gc_id, [to_wire_id_start : to_wire_id_end])
                
                jPtr = json_object_get(jInstr, "from_gc_id");
                assert(json_is_integer(jPtr));
                int from_gc_id = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "from_wire_id_start");
                assert(json_is_integer(jPtr));
                int from_wire_id_start = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "from_wire_id_end");
                assert(json_is_integer(jPtr));
                int from_wire_id_end = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_gc_id");
                assert(json_is_integer(jPtr));
                int to_gc_id = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_wire_id_start");
                assert(json_is_integer(jPtr));
                int to_wire_id_start = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_wire_id_end");
                assert(json_is_integer(jPtr));
                int to_wire_id_end = json_integer_value(jPtr);

                assert(from_wire_id_end - from_wire_id_start == to_wire_id_end - to_wire_id_start);

                if (chainingType == CHAINING_TYPE_STANDARD) {
                    for (int f = from_wire_id_start, t = to_wire_id_start ; f<=from_wire_id_end; f++, t++) {
                        // f for from, t for to
                        instructions->instr[idx].type = CHAIN;
                        instructions->instr[idx].ch.fromCircId = from_gc_id;
                        instructions->instr[idx].ch.fromWireId = f;
                        instructions->instr[idx].ch.toCircId = to_gc_id;
                        instructions->instr[idx].ch.toWireId = t;
                        idx++;
                    }
                } else { /* CHAINING_TYPE_SIMD */
                    instructions->instr[idx].type = CHAIN;
                    instructions->instr[idx].ch.fromCircId = from_gc_id;
                    instructions->instr[idx].ch.fromWireId = from_wire_id_start;
                    instructions->instr[idx].ch.toCircId = to_gc_id;
                    instructions->instr[idx].ch.toWireId = to_wire_id_start;
                    instructions->instr[idx].ch.wireDist = from_wire_id_end - from_wire_id_start + 1;
                    idx++;
                }
                break;
            default:
                fprintf(stderr, "Instruction %d was invalid: %s\n", i, sType);
                return FAILURE;
        }
    }
    assert(idx == instructions->size);
    return SUCCESS;
}

size_t
inputMappingBufferSize(const OldInputMapping *map)
{
    size_t size = sizeof(int);
    size += (3 * sizeof(int) + sizeof(Person)) * map->size;
    return size;
}

int 
writeInputMappingToBuffer(const OldInputMapping* map, char* buffer)
{
    size_t p =0;

    memcpy(buffer+p, &map->size, sizeof(int));
    p += sizeof(int);

    for (int i=0; i<map->size; i++) {
        memcpy(buffer+p, &map->input_idx[i], sizeof(int));
        p += sizeof(int);
        memcpy(buffer+p, &map->gc_id[i], sizeof(int));
        p += sizeof(int);
        memcpy(buffer+p, &map->wire_id[i], sizeof(int));
        p += sizeof(int);
        memcpy(buffer+p, &map->inputter[i], sizeof(Person));
        p += sizeof(Person);
    }
    return p;
}

int 
readBufferIntoInputMapping(OldInputMapping *input_mapping, const char *buffer) 
{
    size_t p = 0;

    memcpy(&(input_mapping->size), buffer+p, sizeof(int));
    p += sizeof(int);

    int size = input_mapping->size;
    input_mapping->input_idx = malloc(sizeof(int) * size);
    input_mapping->gc_id = malloc(sizeof(int) * size);
    input_mapping->wire_id = malloc(sizeof(int) * size);
    input_mapping->inputter = malloc(sizeof(Person) * size);
    assert(input_mapping->input_idx && input_mapping->gc_id && 
            input_mapping->wire_id && input_mapping->inputter);

    for (int i=0; i<input_mapping->size; i++) {
        memcpy(&(input_mapping->input_idx[i]), buffer+p, sizeof(int));
        p += sizeof(int);

        memcpy(&(input_mapping->gc_id[i]), buffer+p, sizeof(int));
        p += sizeof(int);

        memcpy(&(input_mapping->wire_id[i]), buffer+p, sizeof(int));
        p += sizeof(int);

        memcpy(&(input_mapping->inputter[i]), buffer+p, sizeof(Person));
        p += sizeof(Person);
    }
    return p;
}
    
void
print_function(FunctionSpec* function) 
{
    print_metadata(function);
    print_components(&function->components);
    print_input_mapping(&function->input_mapping);
    print_instructions(&function->instructions);
    print_output_instructions(&function->output_instructions);
}

void
newOldInputMapping(OldInputMapping *map, int num_garb_inputs, int num_eval_inputs)
{
    map->size = num_garb_inputs + num_eval_inputs;
    map->input_idx = malloc(sizeof(int) * map->size);
    map->gc_id = malloc(sizeof(int) * map->size);
    map->wire_id = malloc(sizeof(int) * map->size);
    map->inputter = malloc(sizeof(Person) * map->size);

    for (int i = 0; i < num_eval_inputs; i++) {
        map->input_idx[i] = i;
        map->gc_id[i] = 0;
        map->wire_id[i] = i;
        map->inputter[i] = PERSON_EVALUATOR;
    }

    for (int i = num_eval_inputs; i < num_eval_inputs + num_garb_inputs; i++) {
        map->input_idx[i] = i;
        map->gc_id[i] = 0;
        map->wire_id[i] = i;
        map->inputter[i] = PERSON_GARBLER;
    }
}

void 
newInputMapping(InputMapping *map, int size)
{
    map->size = size;
    map->imap_instr = malloc(size * sizeof(InputMappingInstruction));
}

void
deleteOldInputMapping(OldInputMapping *map)
{
    free(map->input_idx);
    free(map->gc_id);
    free(map->wire_id);
    free(map->inputter);
}

void
deleteInputMapping(InputMapping *map)
{
    free(map->imap_instr);
}

int
load_function_via_json(char* path, FunctionSpec* function, ChainingType chainingType)
{
    /* Loading in path
     * uses jansson.h. See jansson docs for more details
     * there exists a function to load directly from file, 
     * but I was getting runtime memory errors when using it.
     */
    long fs;
    FILE *f;
    json_t *jRoot;
    json_error_t error;
    char *buffer = NULL;
    int res = FAILURE;

    f = fopen(path, "r");
    if (f == NULL) {
        printf("Error in opening file %s.\n", path);
        return FAILURE;
    }
    fs = filesize(path);
    if (fs == FAILURE)
        goto cleanup;
    buffer = malloc(fs);
    fread(buffer, sizeof(char), fs, f);
    buffer[fs-1] = '\0';

    jRoot = json_loads(buffer, 0, &error);
    if (!jRoot) {
        fprintf(stderr, "error load json on line %d: %s\n", error.line, error.text);
        goto cleanup;
    }

    // Grab things from the json_t object
    if (json_load_metadata(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading metadata");
        goto cleanup;
    }
    if (json_load_components(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json components");
        goto cleanup;
    }
    if (json_load_input_mapping(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json input mapping");
        goto cleanup;
    }
    if (json_load_instructions(jRoot, function, chainingType) == FAILURE) {
        fprintf(stderr, "error loading json instructions");
        goto cleanup;
    }
    if (json_load_output(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json output");
        return FAILURE;
    }

    json_decref(jRoot); // frees all of the other json_t* objects, everywhere.
    res = SUCCESS;
cleanup:
    free(buffer);
    fclose(f);
    return res;
}


