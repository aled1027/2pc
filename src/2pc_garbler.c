

#include "2pc_garbler.h"

#include <assert.h>
#include <stdio.h>

int 
garbler_init() {
    // 1. load function
    FunctionSpec function; // Populate ME!
    char* function_path = "functions/22Adder.json";

    if (load_function_via_json(function_path, &function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    // 2. map
    // look at list of constructed garbled circuits, and make the chaining map for the real circuits.
    
    // 3. send chaining map over
    return SUCCESS;
}

void
print_function(FunctionSpec* function) {
    printf("Components:\n");
    print_components(function->components, function->num_components);
    printf("Input Mapping:\n");
    print_input_mapping(&(function->input_mapping));
    printf("Instructions:\n");
    print_instructions(&(function->instructions));
}

int 
load_function_via_json(char* path, FunctionSpec* function) {
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

    json_t *jRoot;
    json_error_t error; 
    if (!(jRoot = json_loads(buffer, 0, &error))) {
        fprintf(stderr, "error load json on line %d: %s\n", error.line, error.text);
        return FAILURE;
    }
    free(buffer);

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

static int 
json_load_components(json_t *root, FunctionSpec *function) {
    // TODO need to populate a struct or something
    const char* type;
    int size, num;
    json_t *jComponents, *jComponent, *jType, *jNum, *jCircuit_ids, *jId;

    jComponents = json_object_get(root, "components");
    size = json_array_size(jComponents);
    function->num_components = size;
    function->components = malloc(sizeof(FunctionComponent) * size);

    for (int i=0; i<size; i++) {
        jComponent = json_array_get(jComponents, i);
        assert(json_is_object(jComponent));

        // get type
        jType = json_object_get(jComponent, "type");
        assert(json_is_string(jType));
        type = json_string_value(jType);
        // TODO write this
        function->components[i].circuit_type = get_circuit_type_from_string(type);

        // get number of circuits of type needed
        jNum = json_object_get(jComponent, "num");
        assert (json_is_integer(jNum));
        num = json_integer_value(jNum);
        function->components[i].num = num;

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

static void
print_components(FunctionComponent* components, int num_components) {
    printf("print_components not yet implemented\n");
}

static void 
print_input_mapping(InputMapping* inputMapping) {
    printf("InputMapping:\n");
    for (int i=0; i<inputMapping->size; i++) {
        printf("%d -> (%d, %d)\n", inputMapping->input_idx[i], inputMapping->gc_id[i], inputMapping->wire_id[i]);
    }
}

static void 
print_instructions(Instructions* instr) {
    printf("Instructions:\n");
    for (int i=0; i<instr->size; i++) {
        switch(instr->instr[i].type) {
            case EVAL:
                printf("EVAL %d\n", instr->instr[i].evCircId);
                break;
            case CHAIN:
                printf("CHAIN (%d, %d) -> (%d, %d)\n", instr->instr[i].chFromCircId, 
                        instr->instr[i].chFromWireId,
                        instr->instr[i].chToCircId,
                        instr->instr[i].chToWireId);
                break;
            default:
                printf("Not printing command\n");
        }
    }
}

static int 
json_load_input_mapping(json_t *root, FunctionSpec* function) {
    /* "InputMapping: [
     *      { "gc_id": x, "wire_id: y"},
     *      { "gc_id": a, "wire_id: b"}
     * ]
     * 
     * where the ith element in the array is where the ith input is mapped to.
     * 
     */
    InputMapping* inputMapping = &(function->input_mapping);
    json_t *jInputMapping, *jMap, *jGcId, *jWireId;
    int size;

    jInputMapping = json_object_get(root, "InputMapping");
    assert(json_is_array(jInputMapping));
    size = json_array_size(jInputMapping); // should be equal to n

    // input_idx[i] maps to (gc_id[i], wire_id[i])
    inputMapping->input_idx = malloc(sizeof(int) * size);
    inputMapping->gc_id = malloc(sizeof(int) * size);
    inputMapping->wire_id = malloc(sizeof(int) * size);
    assert(inputMapping->input_idx && inputMapping->gc_id && inputMapping->wire_id);

    inputMapping->size = size;
    for (int i=0; i<size; i++) {
       jMap = json_array_get(jInputMapping, i);
       assert(json_is_object(jMap));

       inputMapping->input_idx[i] = i;

       jGcId = json_object_get(jMap, "gc_id");
       assert(json_is_integer(jGcId));
       inputMapping->gc_id[i] = json_integer_value(jGcId);

       jWireId = json_object_get(jMap, "wire_id");
       assert(json_is_integer(jWireId));
       inputMapping->wire_id[i] = json_integer_value(jWireId);
    }

    return SUCCESS;
}

static CircuitType 
get_circuit_type_from_string(const char* type) {
    if (strcmp(type, "ADDER22") == 0) {
        return ADDER22;
    } else if (strcmp(type, "ADDER23") == 0) {
        return ADDER23;
    } else {
        return CIRCUIT_TYPE_ERR;
    }
}

static InstructionType 
get_instruction_type_from_string(const char* type) {
    if (strcmp(type, "EVAL") == 0) {
        return EVAL;
    } else if (strcmp(type, "CHAIN") == 0) {
        return CHAIN;
    } else {
        return INSTR_ERR;
    }
}

static int 
json_load_instructions(json_t *root, FunctionSpec *function) {
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



