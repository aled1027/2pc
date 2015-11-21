#include "2pc_function_spec.h"

#include <assert.h>

#include "utils.h"

int 
freeFunctionSpec(FunctionSpec* function) 
{
    for (int i=0; i<function->num_component_types; i++) {
        free(function->components[i].circuit_ids);
    }
    free(function->components);

    free(function->input_mapping.input_idx);
    free(function->input_mapping.gc_id);
    free(function->input_mapping.wire_id);

    free(function->instructions.instr);
    return 0;
}

void
print_function(FunctionSpec* function) 
{
    print_components(function->components, function->num_component_types);
    print_input_mapping(&(function->input_mapping));
    print_instructions(&(function->instructions));
}

int 
load_function_via_json(char* path, FunctionSpec* function) 
{
    /* Loading in path
     * uses jansson.h. See jansson docs for more details
     * there exists a function to load directly from file, 
     * but I was getting runtime memory errors when using it.
     */

    long fs = filesize(path); 
    FILE *f = fopen(path, "r"); 
    if (f == NULL) {
        printf("Write: Error in opening file.\n");
        return -1;
    }   
    char *buffer = malloc(fs); 
    assert(buffer);
    fread(buffer, sizeof(char), fs, f);
    buffer[fs-1] = '\0';

    json_t *jRoot, *jN, *jM;
    json_error_t error; 
    if (!(jRoot = json_loads(buffer, 0, &error))) {
        fprintf(stderr, "error load json on line %d: %s\n", error.line, error.text);
        return FAILURE;
    }
    free(buffer);


    jN = json_object_get(jRoot, "n");
    assert(json_is_integer(jN));
    function->n = json_integer_value(jN);

    jM = json_object_get(jRoot, "m");
    assert(json_is_integer(jM));
    function->m = json_integer_value(jM);

    // Grab things from the json_t object
    if (json_load_components(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json components");
        return FAILURE;
    }
    
    if (json_load_input_mapping(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json components");
        return FAILURE;
    }

    if (json_load_instructions(jRoot, function) == FAILURE) {
        fprintf(stderr, "error loading json instructions");
        return FAILURE;
    }
    free(jRoot);
    return SUCCESS;
}

int 
json_load_components(json_t *root, FunctionSpec *function) 
{
    const char* type;
    int size, num;
    json_t *jComponents, *jComponent, *jType, *jNum, *jCircuit_ids, *jId;

    jComponents = json_object_get(root, "components");
    size = json_array_size(jComponents);
    function->num_components = 0;
    function->num_component_types = size;
    function->components = malloc(sizeof(FunctionComponent) * size);

    for (int i=0; i<size; i++) {
        jComponent = json_array_get(jComponents, i);
        assert(json_is_object(jComponent));

        // get type
        jType = json_object_get(jComponent, "type");
        assert(json_is_string(jType));
        type = json_string_value(jType);
        function->components[i].circuit_type = get_circuit_type_from_string(type);

        // get number of circuits of type needed
        jNum = json_object_get(jComponent, "num");
        assert (json_is_integer(jNum));
        num = json_integer_value(jNum);
        function->components[i].num = num;
        function->num_components += num;

        // get circuit ids
        function->components[i].circuit_ids = malloc(sizeof(int) * num);
        jCircuit_ids = json_object_get(jComponent, "circuit_ids");
        assert (json_is_array(jCircuit_ids));

        for (int k=0; k<num; k++) {
            jId = json_array_get(jCircuit_ids, k);
            function->components[i].circuit_ids[k] = json_integer_value(jId);
        }
    }
    return SUCCESS;
}

void
print_components(FunctionComponent* components, int num_component_types) 
{
    printf("print_components not yet implemented\n");
}

void 
print_input_mapping(InputMapping* inputMapping) 
{
    printf("InputMapping size: %d\n", inputMapping->size);
    for (int i=0; i<inputMapping->size; i++) {
        printf("%d -> (%d, %d)\n", inputMapping->input_idx[i], inputMapping->gc_id[i], inputMapping->wire_id[i]);
    }
}

void 
print_instructions(Instructions* instr) 
{
    printf("Instructions:\n");
    for (int i=0; i<instr->size; i++) {
        switch(instr->instr[i].type) {
            case EVAL:
                printf("EVAL %d\n", instr->instr[i].evCircId);
                break;
            case CHAIN:
                printf("CHAIN (%d, %d) -> (%d, %d) with offset (%llu, %llu)\n", 
                        instr->instr[i].chFromCircId, 
                        instr->instr[i].chFromWireId,
                        instr->instr[i].chToCircId,
                        instr->instr[i].chToWireId,
                        instr->instr[i].chOffset[0],
                        instr->instr[i].chOffset[1]);

                break;
            default:
                printf("Not printing command\n");
        }
    }
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
    json_t *jInputMapping, *jMap, *jGcId, *jWireIdx, *jInputter, *jInputIdx, *jN;

    jN = json_object_get(root, "n");
    int n = json_integer_value(jN); // aka tot num inputs

    jInputMapping = json_object_get(root, "InputMapping");
    assert(json_is_array(jInputMapping));
    int loop_size = json_array_size(jInputMapping); 

    // input_idx[l] maps to (gc_id[l], wire_id[l])
    inputMapping->input_idx = malloc(sizeof(int) * n);
    inputMapping->gc_id = malloc(sizeof(int) * n);
    inputMapping->wire_id = malloc(sizeof(int) * n);
    inputMapping->inputter = malloc(sizeof(Person) * n);
    assert(inputMapping->input_idx && inputMapping->gc_id && inputMapping->wire_id);

    int l = 0;
    inputMapping->size = n;
    for (int i=0; i<loop_size; i++) {
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

        // Double check with line 95-106 of 2pc_evaluator.c
        // process and save the info to inputMapping
        assert(end_input_idx - start_input_idx == end_wire_idx - start_wire_idx);
        for (int j = start_input_idx, k = start_wire_idx; j <= end_input_idx; j++, k++, l++) {
            inputMapping->input_idx[l] = j;
            inputMapping->wire_id[l] = k;
            inputMapping->inputter[l] = inputter;
            inputMapping->gc_id[l] = gc_id;
        }
    }
    return SUCCESS;
}

CircuitType 
get_circuit_type_from_string(const char* type) 
{
    if (strcmp(type, "22Adder") == 0) {
        return ADDER22;
    } else if (strcmp(type, "23Adder") == 0) {
        return ADDER23;
    } else {
        fprintf(stderr, "circuit type error when loading json");
        return CIRCUIT_TYPE_ERR;
    }
}

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

int 
json_load_instructions(json_t *root, FunctionSpec *function) 
{
    Instructions* instructions = &(function->instructions);
    json_t *jInstructions, *jInstr, *jGcId, *jPtr;
    int size;
    const char* sType;

    jInstructions = json_object_get(root, "instructions");
    assert(json_is_array(jInstructions));

    size = json_array_size(jInstructions); 
    instructions->size = size;
    instructions->instr = malloc(sizeof(Instruction) * size);

    for (int i=0; i<size; i++) {
        jInstr = json_array_get(jInstructions, i);
        assert(json_is_object(jInstr));

        jPtr = json_object_get(jInstr, "type");
        assert(json_is_string(jPtr));
        sType = json_string_value(jPtr);

        instructions->instr[i].type = get_instruction_type_from_string(sType);
        switch (instructions->instr[i].type) {
            case EVAL:
                jPtr = json_object_get(jInstr, "gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].evCircId = json_integer_value(jPtr);
                break;
            case CHAIN:
                jPtr = json_object_get(jInstr, "from_gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chFromCircId = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "from_wire_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chFromWireId = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_gc_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chToCircId = json_integer_value(jPtr);

                jPtr = json_object_get(jInstr, "to_wire_id");
                assert(json_is_integer(jPtr));
                instructions->instr[i].chToWireId = json_integer_value(jPtr);
                break;
            default:
                fprintf(stderr, "Instruction %d was invalid: %s\n", i, sType);
                return FAILURE;
        }
    }
    return SUCCESS;
}


int 
writeInstructionsToBuffer(Instructions* instructions, char* buffer) 
{
    size_t p = 0;
    memcpy(buffer+p, &(instructions->size), sizeof(int));
    p += sizeof(int);

    for (int i=0; i<instructions->size; i++) {
        Instruction* instr = &(instructions->instr[i]);
        memcpy(buffer+p, &(instr->type), sizeof(InstructionType));
        p += sizeof(InstructionType);

        switch(instr->type) {
            case CHAIN:
                memcpy(buffer+p, &(instr->chFromCircId), sizeof(int));
                p += sizeof(int);
            
                memcpy(buffer+p, &(instr->chFromWireId), sizeof(int));
                p += sizeof(int);
            
                memcpy(buffer+p, &(instr->chToCircId), sizeof(int));
                p += sizeof(int);
                
                memcpy(buffer+p, &(instr->chToWireId), sizeof(int));
                p += sizeof(int);
            
                memcpy(buffer+p, &(instr->chOffset), sizeof(block));
                p += sizeof(block);
                break;

            case EVAL:
                memcpy(buffer+p, &(instr->evCircId), sizeof(int));
                p += sizeof(int);
                break;
            default:
                // do nothing
                break;
        }

    }
    return p;
}

int 
readBufferIntoInstructions(Instructions* instructions, char* buffer) 
{
    size_t p = 0;
    memcpy(&(instructions->size), buffer+p, sizeof(int));
    p += sizeof(int);

    int size = instructions->size;
    instructions->instr = (Instruction *) malloc(sizeof(Instruction) * instructions->size);
    assert(instructions->instr);

    for (int i=0; i< instructions->size; i++) {
        Instruction* instr = &(instructions->instr[i]);
        memcpy(&(instr->type), buffer+p, sizeof(InstructionType));
        p += sizeof(InstructionType);

        switch(instr->type) {
            case CHAIN:
                memcpy(&(instr->chFromCircId), buffer+p, sizeof(int));
                p += sizeof(int);
            
                memcpy(&(instr->chFromWireId), buffer+p, sizeof(int));
                p += sizeof(int);
            
                memcpy(&(instr->chToCircId), buffer+p, sizeof(int));
                p += sizeof(int);
                
                memcpy(&(instr->chToWireId), buffer+p, sizeof(int));
                p += sizeof(int);
            
                memcpy(&(instr->chOffset), buffer+p, sizeof(block));
                p += sizeof(block);
                break;

            case EVAL:
                memcpy(&(instr->evCircId), buffer+p, sizeof(int));
                p += sizeof(int);
                break;
            default:
                // do nothing
                break;
        }
    }
    return p;
}

int 
writeInputMappingToBuffer(InputMapping* input_mapping, char* buffer) 
{
    size_t p =0;

    memcpy(buffer+p, &(input_mapping->size), sizeof(int));
    p += sizeof(int);

    for (int i=0; i<input_mapping->size; i++) {
        memcpy(buffer+p, &(input_mapping->input_idx[i]), sizeof(int));
        p += sizeof(int);

        memcpy(buffer+p, &(input_mapping->gc_id[i]), sizeof(int));
        p += sizeof(int);

        memcpy(buffer+p, &(input_mapping->wire_id[i]), sizeof(int));
        p += sizeof(int);

        memcpy(buffer+p, &(input_mapping->inputter[i]), sizeof(Person));
        p += sizeof(Person);
    }
    return p;
}

int 
readBufferIntoInputMapping(InputMapping* input_mapping, char* buffer) 
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

