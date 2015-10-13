

#include "2pc_garbler.h"

#include <assert.h>
#include <stdio.h>
#include "chaining.h"

int 
garbler_init(FunctionSpec *function, ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) {
    // load function from file
    char* function_path = "functions/22Adder.json";

    if (load_function_via_json(function_path, function) == FAILURE) {
        fprintf(stderr, "Could not load function %s\n", function_path);
        return FAILURE;
    }

    CircuitType saved_gcs_type[] = {
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER22,
        ADDER23,
        ADDER23,
        ADDER23,
        ADDER23};
    int num_saved_gcs = 10;
    garbler_make_real_instructions(function, saved_gcs_type, num_saved_gcs, chained_gcs, num_chained_gcs);
    return SUCCESS;
}

int
garbler_make_real_instructions(FunctionSpec *function, CircuitType *saved_gcs_type, int num_saved_gcs,
        ChainedGarbledCircuit* chained_gcs, int num_chained_gcs) {
    // suppose we have N garbled circuits total
    // 6 ADDER22 and 4 ADDER23
    bool* is_circuit_used = malloc(sizeof(bool) * num_saved_gcs);
    assert(is_circuit_used);
    memset(is_circuit_used, 0, sizeof(bool) * num_saved_gcs);

    // create mapping functionCircuitToRealCircuit: FunctionCircuit int id -> Real Circuit id
    int num_components = function->num_components;
    int* functionCircuitToRealCircuit = malloc(sizeof(int) * num_components);

    // loop over type of circuit
    for (int i=0; i<num_components; i++) {
        CircuitType type = function->components[i].circuit_type;
        int num = function->components[i].num;
        int* circuit_ids = function->components[i].circuit_ids; // = {0,1,2};
        
        // j indexes circuit_ids, k indexes is_circuit_used and saved_gcs
        int j = 0, k = 0;

        // find an available circuit
        for (int k=0; k<num_saved_gcs; k++) {
            if (!is_circuit_used[k] && saved_gcs_type[k] == type) {
                // map it, and increment j
                functionCircuitToRealCircuit[circuit_ids[j]] = k;
                is_circuit_used[k] = true;
                j++;
            }
            if (num == j) break;
        }
        if (j < num) {
            fprintf(stderr, "Not enough circuits of type %d available\n", type);
            return FAILURE;
        }
    }
    
    // Apply FunctionCircuitToRealCircuit to update instructions to reflect real circuit ids
    int num_instructions = function->instructions.size;
    Instruction *cur;
    for (int i=0; i<num_instructions; i++) {
        cur = &(function->instructions.instr[i]);
        switch(cur->type) {
            case EVAL:
                cur->evCircId = functionCircuitToRealCircuit[cur->evCircId];
                break;
            case CHAIN:
                cur->chFromCircId = functionCircuitToRealCircuit[cur->chFromCircId];
                cur->chToCircId = functionCircuitToRealCircuit[cur->chToCircId];
                cur->chOffset = xorBlocks(
                        chained_gcs[cur->chFromCircId].outputMap[cur->chFromWireId],
                        chained_gcs[cur->chToCircId].outputMap[cur->chToWireId]);
                break;
            default:
                fprintf(stderr, "Instruction type not recognized\n");
        }
    }
    print_instructions(&function->instructions);
    return SUCCESS;
}

void
print_function(FunctionSpec* function) {
    print_components(function->components, function->num_components);
    print_input_mapping(&(function->input_mapping));
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

int 
json_load_components(json_t *root, FunctionSpec *function) {
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

void
print_components(FunctionComponent* components, int num_components) {
    printf("print_components not yet implemented\n");
}

void 
print_input_mapping(InputMapping* inputMapping) {
    printf("InputMapping size: %d\n", inputMapping->size);
    for (int i=0; i<inputMapping->size; i++) {
        printf("%d -> (%d, %d)\n", inputMapping->input_idx[i], inputMapping->gc_id[i], inputMapping->wire_id[i]);
    }
}

void 
print_instructions(Instructions* instr) {
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

CircuitType 
get_circuit_type_from_string(const char* type) {
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
get_instruction_type_from_string(const char* type) {
    if (strcmp(type, "EVAL") == 0) {
        return EVAL;
    } else if (strcmp(type, "CHAIN") == 0) {
        return CHAIN;
    } else {
        return INSTR_ERR;
    }
}

int 
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



