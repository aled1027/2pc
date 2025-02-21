#ifndef TWOPC_HYPERPLANE_H
#define TWOPC_HYPERPLANE_H

typedef enum { WDBC, CREDIT} HYPERPLANE_TYPE;
typedef enum { DT_RANDOM, DT_NURSERY, DT_ECG} DECISION_TREE_TYPE;
typedef enum {NB_WDBC, NB_NURSERY, NB_AUD} NAIVE_BAYES_TYPE;

void hyperplane_garb_off(char *dir, uint32_t n, uint32_t num_len, HYPERPLANE_TYPE type);
void dt_garb_off(char *dir, uint32_t n, uint32_t num_len, DECISION_TREE_TYPE type);
void nb_garb_off(char *dir, int num_len, int num_classes, int vector_size, int domain_size, NAIVE_BAYES_TYPE experiment);

ChainedGarbledCircuit* hyperplane_circuits(uint32_t n, uint32_t num_len);
ChainedGarbledCircuit* dt_circuits(uint32_t n, uint32_t num_len, DECISION_TREE_TYPE);
ChainedGarbledCircuit* nb_circuits(int num_len, int num_classes, int vector_size, int domain_size, NAIVE_BAYES_TYPE experiment);

#endif
