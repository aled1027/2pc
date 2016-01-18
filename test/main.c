
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "justGarble.h"
#include "2pc_aes.h"
#include "2pc_cbc.h"

/* XXX: not great, but OK for now */

static struct option opts[] =
{
    {"and_garb_off", no_argument, 0, 'a'},
    {"and_eval_off", no_argument, 0, 'A'},
    {"and_garb_on", no_argument, 0, 'b'},
    {"and_eval_on", no_argument, 0, 'B'},
    {"and_garb_full", no_argument, 0, 'c'},
    {"and_eval_full", no_argument, 0, 'c'},

    {"aes_garb_off", no_argument, 0, 'd'},
    {"aes_eval_off", no_argument, 0, 'D'},
    {"aes_garb_on", no_argument, 0, 'e'},
    {"aes_eval_on", no_argument, 0, 'E'},
    {"aes_garb_full", no_argument, 0, 'f'},
    {"aes_eval_full", no_argument, 0, 'F'},

    {"cbc_garb_off", no_argument, 0, 'g'},
    {"cbc_eval_off", no_argument, 0, 'G'},
    {"cbc_garb_on", no_argument, 0, 'i'},
    {"cbc_eval_on", no_argument, 0, 'I'},
    {"cbc_garb_full", no_argument, 0, 'j'},
    {"cbc_eval_full", no_argument, 0, 'J'},

    {"time", no_argument, 0, 't'},

    {0, 0, 0, 0}
};

int
main(int argc, char *argv[])
{
    int c, idx;
    bool is_timing = false;

    seedRandom();

    while ((c = getopt_long(argc, argv, "", opts, &idx)) != -1) {
        switch (c) {
        case 0:
            break;
        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'c':
        case 'C':
            assert(false);
            exit(1);
        case 'd':
            aes_garb_off();
            exit(1);
        case 'D':
            aes_eval_off();
            exit(1);
        case 'e':
            aes_garb_on("functions/aes.json", is_timing);
            exit(1);
        case 'E':
            aes_eval_on(is_timing);
            exit(1);
        case 'f':
            full_aes_garb();
            exit(1);
        case 'F':
            full_aes_eval();
            exit(1);
        case 'g':
            cbc_garb_off();
            exit(1);
        case 'G':
            cbc_eval_off();
            exit(1);
        case 'i':
            cbc_garb_on();
            exit(1);
        case 'I':
            cbc_eval_on();
            exit(1);
        case 'j':
            full_cbc_garb();
            exit(1);
        case 'J':
            full_cbc_eval();
            exit(1);
        case 't':
            is_timing = true;
            break;
        case '?':
            break;
        default:
            assert(false);
            abort();
        }
    }
    return EXIT_SUCCESS;
}
