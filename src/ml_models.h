#ifndef MPC_ML_MODELS_H
#define MPC_ML_MODELS_H
#include <jansson.h>
#include <stdint.h>

typedef struct {
    uint64_t data_size;
    int64_t *data;
    uint32_t num_len;
    char name[128];
    char type[128];

} Model;

void print_model(const Model *model);
Model *get_model(const char *path);
void destroy_model(Model *model);
void load_model_into_inputs(bool *inputs, const char *model_name);
#endif
