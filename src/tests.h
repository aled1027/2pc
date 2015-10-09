#ifndef MPC_TESTS_H
#define MPC_TESTS_H

#include <jansson.h>

int run_all_tests();
int simple_test();
int test2();
int test_saving_reading();
int json_test();
int json_get_components(json_t *root);

#endif
