
#include "2pc_garbler.h"
#include "2pc_evaluator.h"
#include "components.h"

#include "utils.h"

#include <assert.h>

/* void */
/* and_garb_off(char *dir, int ninputs, int nlayers, int nchains) */
/* { */
/*     ChainedGarbledCircuit *gcs; */
/*     block delta; */
    
/*     printf("Running garb offline\n"); */

/*     assert(nlayers % nchains == 0); */

/*     gcs = malloc(sizeof(ChainedGarbledCircuit) * nchains); */

/*     delta = randomBlock(); */
/*     *((uint16_t *) (&delta)) |= 1; */

/*     for (int i = 0; i < nchains; ++i) { */
/*         GarbledCircuit *gc = &gcs[i].gc; */

/*         buildANDCircuit(gc, ninputs, nlayers / nchains); */

/*         /\* gcs[i].inputLabels = allocate_blocks(2 * gc->n); *\/ */
/*         gcs[i].outputMap = allocate_blocks(2 * gc->m); */
/*         garbleCircuit(gc, gcs[i].outputMap, GARBLE_TYPE_STANDARD); */
/*     } */
/*     garbler_offline(dir, gcs, ninputs, nchains); */
/*     for (int i = 0; i < nchains; ++i) { */
/*         removeGarbledCircuit(&gcs[i].gc); */
/*     } */
/*     freeChainedGarbledCircuit(gcs); */
/* } */

/* void */
/* and_garb_full(int ninputs, int nlayers) */
/* { */
/*     GarbledCircuit gc; */
/*     block *inputs, *outputs; */
/*     int n_garb_inputs, n_eval_inputs; */
/*     int *garb_inputs; */
/*     InputMapping map; */

/*     buildANDCircuit(&gc, ninputs, nlayers); */
/*     inputs = allocate_blocks(2 * gc.n); */
/*     outputs = allocate_blocks(2 * gc.m); */

/*     n_garb_inputs = ninputs; */
/*     n_eval_inputs = 0; */

/*     garb_inputs = malloc(sizeof(int) * n_garb_inputs); */
/*     for (int i = 0; i < n_garb_inputs; ++i) { */
/*         garb_inputs[i] = rand() % 2; */
/*     } */

/*     garbleCircuit(&gc, inputs, outputs, GARBLE_TYPE_STANDARD); */
/*     map.size = n_garb_inputs + n_eval_inputs; */
/*     map.input_idx = malloc(sizeof(int) * map.size); */
/*     map.gc_id = malloc(sizeof(int) * map.size); */
/*     map.wire_id = malloc(sizeof(int) * map.size); */
/*     map.inputter = malloc(sizeof(Person) * map.size); */

/*     for (int i = 0; i < n_eval_inputs; ++i) { */
/*         map.input_idx[i] = i; */
/*         map.gc_id[i] = 0; */
/*         map.wire_id[i] = i; */
/*         map.inputter[i] = PERSON_EVALUATOR; */
/*     } */
/*     for (int i = n_eval_inputs; i < map.size; ++i) { */
/*         map.input_idx[i] = i; */
/*         map.gc_id[i] = 0; */
/*         map.wire_id[i] = i; */
/*         map.inputter[i] = PERSON_GARBLER; */
/*     } */

/*     { */
/*         unsigned long time; */

/*         garbler_classic_2pc(&gc, inputs, &map, outputs, n_garb_inputs, */
/*                             n_eval_inputs, garb_inputs, &time); */
/*         printf("tot_time %lu\n", time); */
/*     } */
/*     removeGarbledCircuit(&gc); */
/*     free(inputs); */
/*     free(outputs); */
/* } */

/* void */
/* and_eval_full(int ninputs) */
/* { */
/*     int n_garb_inputs, n_eval_inputs; */
/*     int *output; */
/*     unsigned long time; */

/*     n_garb_inputs = ninputs; */
/*     n_eval_inputs = 0; */

/*     output = malloc(sizeof(int) * ninputs); */

/*     evaluator_classic_2pc(NULL, output, n_garb_inputs, n_eval_inputs, &time); */
/*     printf("tot_time %lu\n", time); */
/* } */
