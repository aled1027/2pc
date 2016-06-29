
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

void simple_hyperplane_garb_off(char *dir, uint32_t n, uint32_t num_len) {
    printf("Running linear garb offline\n");

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
