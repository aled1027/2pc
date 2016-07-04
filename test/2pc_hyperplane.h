#ifndef TWOPC_HYPERPLANE_H
#define TWOPC_HYPERPLANE_H

typedef enum { WDBC} HYPERPLANE_TYPE;
typedef enum { DT_RANDOM} DECISION_TREE_TYPE;

void hyperplane_garb_off(char *dir, uint32_t n, uint32_t num_len, HYPERPLANE_TYPE type);
void dt_garb_off(char *dir, uint32_t n, uint32_t num_len, DECISION_TREE_TYPE type);

#endif
