#ifndef MPC_ML_MODELS_H
#define MPC_ML_MODELS_H
#include <jansson.h>
#include <stdint.h>

typedef struct {
    char name[128];
    char type[128];
    uint64_t data_size;
    int64_t *data;

} Model;

void print_model(const Model *model);
Model *get_model(const char *path);
void destroy_model(Model *model);
#endif
