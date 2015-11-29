#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// #include "gc_comm.h"
// #include "common.h"
#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 
#include "tests.h"

#include "arg.h"
#include "gc_comm.h"
#include "net.h"
#include "ot_np.h"
#include "2pc_common.h"

int 
main(int argc, char* argv[]) 
{

    if (argc != 2) {
        printf("usage: %s eval\n", argv[0]);
        printf("usage: %s garb\n", argv[0]);
        printf("usage: %s tests\n", argv[0]);
        return -1;
    }

	srand(time(NULL));
    srand_sse(time(NULL));

    // specifies which function to run
    // char* function_path = "functions/22Adder.json";
    char* function_path = "functions/aes.json";

    if (strcmp(argv[1], "eval_online") == 0) {
        printf("Running eval online\n");
        evaluator_run();
    } else if (strcmp(argv[1], "garb_online") == 0) {
        printf("Running garb online\n");
        garbler_run(function_path);
    } else if (strcmp(argv[1], "garb_offline") == 0) {
        printf("Running garb offline\n");
        garbler_offline();
    } else if (strcmp(argv[1], "eval_offline") == 0) {
        printf("Running val offline\n");
        evaluator_offline();
    } else if (strcmp(argv[1], "tests") == 0) {
        run_all_tests();
    } else {
        printf("usage: %s eval\n", argv[0]);
        printf("usage: %s garb\n", argv[0]);
        printf("usage: %s tests\n", argv[0]);
        return -1;
    } 
    //go();
    return 0;
}


