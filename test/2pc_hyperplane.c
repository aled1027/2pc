
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "2pc_garbler.h" 
#include "2pc_evaluator.h"
#include "components.h"
#include "utils.h"

#include <garble.h>

#include "2pc_hyperplane.h"

typedef struct {
    CircuitType circuit_type;
    int n;
    int m;
    int num_len; // if relevant
} cgc_information;

void generate_cgcs(ChainedGarbledCircuit *cgcs, cgc_information *cgc_info, int ncircuits) 
{
    block delta = garble_create_delta();

    for (uint32_t i = 0; i < ncircuits; ++i) {
        int n = cgc_info[i].n;
        int m = cgc_info[i].m;
        ChainedGarbledCircuit *cgc = &cgcs[i];
        CircuitType circuit_type = cgc_info[i].circuit_type;

        if (circuit_type == INNER_PRODUCT) {
            build_inner_product_circuit(&cgc->gc, n, cgc_info[i].num_len);
        } else if (circuit_type == AND) {
            build_and_circuit(&cgc->gc, n);
        }

        cgc->inputLabels = garble_allocate_blocks(2 * n);
        cgc->outputMap = garble_allocate_blocks(2 * m);
        garble_create_input_labels(cgc->inputLabels, n, &delta, false);
        garble_garble(&cgc->gc, cgc->inputLabels, cgc->outputMap);

        cgc->id = i;
        cgc->type = circuit_type;
    }
}

void hyperplane_garb_off(char *dir, uint32_t n, uint32_t num_len, HYPERPLANE_TYPE type) {
    if (type == WDBC) {
        block delta = garble_create_delta();
        ChainedGarbledCircuit cgc[2];
        build_inner_product_circuit(&cgc[0].gc, n, num_len);
        cgc[0].inputLabels = garble_allocate_blocks(2 * n);
        cgc[0].outputMap = garble_allocate_blocks(2 * num_len);
        garble_create_input_labels(cgc[0].inputLabels, n, &delta, false);
        garble_garble(&cgc[0].gc, cgc[0].inputLabels, cgc[0].outputMap);

        cgc[0].id = 0;
        cgc[0].type = INNER_PRODUCT;

        build_gr0_circuit(&cgc[1].gc, num_len);
        cgc[1].inputLabels = garble_allocate_blocks(2 * num_len);
        cgc[1].outputMap = garble_allocate_blocks(2);
        garble_create_input_labels(cgc[1].inputLabels, num_len, &delta, false);
        garble_garble(&cgc[1].gc, cgc[1].inputLabels, cgc[1].outputMap);

        cgc[1].id = 1;
        cgc[1].type = GR0;

        int num_eval_inputs = n / 2;
        int num_circuits = 2;

        garbler_offline(dir, cgc, num_eval_inputs, num_circuits, CHAINING_TYPE_STANDARD);
    }
}

void dt_garb_off(char *dir, uint32_t n, uint32_t num_len, DECISION_TREE_TYPE type) 
{
    if (type == DT_RANDOM) {

        // generate a bunch of chained garbled comparators
        uint32_t ncircuits = 10;
        int num_eval_inputs = n / 2;

        cgc_information cgc_info[ncircuits];
        for (uint32_t i = 0; i < ncircuits; i++) {
            cgc_info[i].circuit_type = SIGNED_COMPARISON;
            cgc_info[i].n = 110;
            cgc_info[i].m = 1;
        }

        ChainedGarbledCircuit cgcs[ncircuits];
        generate_cgcs(cgcs, cgc_info, ncircuits);
                

        garbler_offline(dir, cgcs, num_eval_inputs, ncircuits, CHAINING_TYPE_STANDARD);
    }
}
