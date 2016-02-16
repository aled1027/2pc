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
    free(function->input_mapping.input_idx);
    free(function->input_mapping.gc_id);
    free(function->input_mapping.wire_id);
    free(function->input_mapping.inputter);

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
        if (inputMapping->inputter[i] == PERSON_EVALUATOR) 
            strcpy(person, "Evaluator");
        printf("%s input %d -> (gc %d, wire %d)\n", person, inputMapping->input_idx[i], inputMapping->gc_id[i], 
                inputMapping->wire_id[i]);
    }
}

void
print_instruction(Instruction *in)
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
print_instructions(Instructions* instr) 
{
    printf("Instructions:\n");
    for (int i = 0; i < instr->size; i++) {
        switch(instr->instr[i].type) {
            case EVAL:
                printf("EVAL %d\n", instr->instr[i].ev.circId);
                break;
            case CHAIN:
                printf("CHAIN (%d, %d) -> (%d, %d) with offset (%d)\n", 
                        instr->instr[i].ch.fromCircId, 
                        instr->instr[i].ch.fromWireId,
                        instr->instr[i].ch.toCircId,
                        instr->instr[i].ch.toWireId,
                        instr->instr[i].ch.offsetIdx);

                break;
            default:
                printf("Not printing command\n");
        }
    }
}

void
print_output_instructions(OutputInstructions *ois)
{
    FILE* fp = stdout;
    printf("Num output instructions: %d\n", ois->size);
    OutputInstruction *oi;
    for (int i = 0; i < ois->size; i++) {
        oi = &ois->output_instruction[i];
        printf("oi[%d] = (gc_id: %d, wire_id: %d ",
                i,
                oi->gc_id,
                oi->wire_id);
        print_block(fp, oi->labels[0]);
        printf(" ");
        print_block(fp, oi->labels[1]);
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
    return SUCCESS;

}


int 
json_load_input_mapping(json_t *root, FunctionSpec* function) 
{
    /* "InputMapping: [
     *      { "gc_id": x, "wire_id: y", "inputter": "garbler"},
     *      { "gc_id": a, "wire_id: b", "inputter": "evaluator"}
     * ]
     * 
     * where the ith element in the array is where the ith input is mapped to.
     * 
     */
    InputMapping* inputMapping = &(function->input_mapping);
    json_t *jInputMapping, *jMap, *jGcId, *jWireIdx, *jInputter, *jInputIdx, *jPtr, *jMetadata;

    jMetadata = json_object_get(root, "metadata");
    jPtr = json_object_get(jMetadata, "input_mapping_size");
    assert(json_is_integer(jPtr));
    int size = json_integer_value(jPtr);
    inputMapping->size = size;

    jInputMapping = json_object_get(root, "input_mapping");
    assert(json_is_array(jInputMapping));
    int loopSize = json_array_size(jInputMapping); 

    // input_idx[l] maps to (gc_id[l], wire_id[l])
    inputMapping->input_idx = malloc(sizeof(int) * size);
    inputMapping->gc_id = malloc(sizeof(int) * size);
    inputMapping->wire_id = malloc(sizeof(int) * size);
    inputMapping->inputter = malloc(sizeof(Person) * size);
    assert(inputMapping->input_idx && inputMapping->gc_id && inputMapping->wire_id);

    int l = 0;
    for (int i = 0; i < loopSize; i++) {

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
            inputter = PERSON_ERR;
        }

        // process and save the info to inputMapping
        for (int j = start_input_idx, k = start_wire_idx; j <= end_input_idx; j++, k++, l++) {
            inputMapping->input_idx[l] = j;
            inputMapping->wire_id[l] = k;
            inputMapping->inputter[l] = inputter;
            inputMapping->gc_id[l] = gc_id;
        }
    }
    assert(size == l);
    return SUCCESS;
}

int 
json_load_instructions(json_t *root, FunctionSpec *function, ChainingType chainingType) 
{
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

    /* printf("mallocing %zu for instructions\n", instructions->size * sizeof(Instruction)); */
    instructions->instr = malloc(instructions->size * sizeof(Instruction));
    assert(instructions->instr);


    /* Add chaining for "InputComponent" as specified by InputMapping,
     * which shold be already loaded from the json file */
    for (int i = 0; i < imap->size; ++i) {
        instructions->instr[i].type = CHAIN;
        instructions->instr[i].ch.fromCircId = 0;
        instructions->instr[i].ch.fromWireId = (imap->inputter[i] == PERSON_GARBLER) ? 
                            imap->input_idx[i] : imap->input_idx[i] + function->num_garb_inputs;
        instructions->instr[i].ch.toCircId = imap->gc_id[i];
        instructions->instr[i].ch.toWireId = imap->wire_id[i];
        instructions->instr[i].ch.wireDist = 1;
        /*printf("chaining from circId 0 wire_id %d\n", instructions->instr[i].chFromWireId);*/
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
inputMappingBufferSize(const InputMapping *map)
{
    size_t size = sizeof(int);
    size += (3 * sizeof(int) + sizeof(Person)) * map->size;
    return size;
}

int 
writeInputMappingToBuffer(const InputMapping* map, char* buffer)
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
readBufferIntoInputMapping(InputMapping *input_mapping, const char *buffer) 
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
newInputMapping(InputMapping *map, int num_garb_inputs, int num_eval_inputs)
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
deleteInputMapping(InputMapping *map)
{
    free(map->input_idx);
    free(map->gc_id);
    free(map->wire_id);
    free(map->inputter);
}

int
load_function_via_json(char* path, FunctionSpec* function, ChainingType chainingType)
{
    /* Loading in path
     * uses jansson.h. See jansson docs for more details
     * there exists a function to load directly from file, 
     * but I was getting runtime memory errors when using it.
     */

    //printf("loading %s\n", path);
    long fs = filesize(path); 
    FILE *f = fopen(path, "r"); 
    if (f == NULL) {
        printf("Error in opening file %s.\n", path);
        return -1;
    }   
    char *buffer = malloc(fs); 
    assert(buffer);
    fread(buffer, sizeof(char), fs, f);
    buffer[fs-1] = '\0';

    json_t *jRoot;
    json_error_t error; 
    jRoot = json_loads(buffer, 0, &error);
    if (!jRoot) {
        fprintf(stderr, "error load json on line %d: %s\n", error.line, error.text);
        return FAILURE;
    }
    fclose(f);
    free(buffer);


    // Grab things from the json_t object
    if (json_load_metadata(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading metadata");
        return FAILURE;
    }

    if (json_load_components(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json components");
        return FAILURE;
    }
    
    if (json_load_input_mapping(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json components");
        return FAILURE;
    }

    if (json_load_instructions(jRoot, function, chainingType) == FAILURE) {
        fprintf(stderr, "error loading json instructions");
        return FAILURE;
    }

    if (json_load_output(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json output");
        return FAILURE;
    }

    json_decref(jRoot); // frees all of the other json_t* objects, everywhere.
    return SUCCESS;
}


