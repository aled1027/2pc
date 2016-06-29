#include "ml_models.h"
#include "utils.h"
#include <string.h>
#include <assert.h>

void print_model(const Model *model) 
{
    printf("Model:\n");
    printf("\tname: %s\n", model->name);
    printf("\ttype: %s\n", model->type);
    printf("\tdata_size: %lu\n", model->data_size);
    printf("\tdata: [");
    for (uint32_t i = 0; i < model->data_size; ++i) {
        printf("%lf, ", model->data[i]);
    }
    printf("]\n");

}


Model *get_model(const char *path)
{
    long fs;
    FILE *f;
    json_t *j_root, *j_ptr, *j_data;
    json_error_t error;
    char *buffer = NULL;

    f = fopen(path, "r");
    if (f == NULL) {
        printf("Error in opening file %s.\n", path);
        return NULL;
    }
    fs = filesize(path);

    if (fs == FAILURE)
        goto cleanup;

    buffer = malloc(fs);
    fread(buffer, sizeof(char), fs, f);
    buffer[fs-1] = '\0';

    j_root = json_loads(buffer, 0, &error);

    Model *model = malloc(sizeof(Model));
    char *temp;

    // get name
    j_ptr = json_object_get(j_root, "name");
    temp = (char*) json_string_value(j_ptr);
    assert(strlen(temp) + 1 < 128);
    memcpy(model->name, temp, sizeof(char) * strlen(temp) + 1);
    free(temp);
    temp = NULL;

    // get type
    j_ptr = json_object_get(j_root, "type");
    temp =  (char* ) json_string_value(j_ptr);
    assert(strlen(temp) + 1 < 128);
    memcpy(model->type, temp, sizeof(char) * strlen(temp) + 1);
    free(temp);
    temp = NULL;

    // get data
    j_data = json_object_get(j_root, "data");

    assert(json_is_array(j_data));
    model->data_size = json_array_size(j_data); 
    model->data = malloc(sizeof(double) * model->data_size);

    for (uint32_t i = 0; i < model->data_size; ++i) {
        j_ptr = json_array_get(j_data, i);
        assert(json_is_real(j_ptr));
        model->data[i] = json_integer_value(j_ptr);
    }


    
    return model;


cleanup:
    free(buffer);
    fclose(f);
    return NULL;

}


