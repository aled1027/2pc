# 2pc
- offline/online 2PC

## Dependencies
1. [msgpack](http://msgpack.org/index.html)
    - a JustGarble requirement
    - To install, follow instructions on their [github page](https://github.com/msgpack/msgpack-c).
1. [jannson](http://www.digip.org/jansson/)
    - interface for JSON
    - To install, follow instructions in their [docs](https://jansson.readthedocs.org/en/2.7/gettingstarted.html)
1. Once msgpack is installed, compile justGarble
    - be sure that you are using my version of justGarble
    - In particular, you need the function `createInputLabelsWithR`, and `garbleCircuit` needs to use this function

## Setup
1. Install dependencies.
1. Clone the 2pc repo: 
    - `git clone https://github.com/aled1027/2pc.git`
    - `cd 2pc`
1. Be sure that there are directories `files/evaluator_gcs` and `files/garbler_gcs` in the 2pc directory
    - `mkdir files; mkdir files/evaluator_gcs; mkdir files/garbler_gcs`
    - these are where the garbled circuits are saved during the offline phase
1. Clone the JustGarble repo:
    - `cd 2pc; git clone https://github.com/aled1027/JustGarble`

## To run AES:
### Offline
1. Be sure everything is compiled by running `make`
1. In one terminal run `make aes_garb_off`
1. In another terminal run `make aes_eval_off`
1. Now the directores `files/evaluator_gcs` and `files/garbler_gcs` should be populated with files.

### Online
1. In one terminal run `make aes_garb_on`
1. In another terminal run `make aes_eval_on`

## To run CBC
### Offline
1. Generate the json using by running `python extra_scripts/main.py --mode cbc 10 8`
    - 10 indicates 10 rounds for aes
    - 8 indicates that there will be 8 128 bit message blocks
1. Copy the output of previously run script to `functions/cbc.json`
1. In one terminal run `make cbc_garb_off`
1. In another terminal run `make cbc_eval_off`
1. Now the directores `files/evaluator_gcs` and `files/garbler_gcs` should be populated with files.

### Online
1. In one terminal run `make cbc_garb_on`
1. In another terminal run `make cbc_eval_on`

## Miscellaneous Notes
- to view json files: [json online editor](http://www.jsoneditoronline.org/)
- taking about 65 c/g for CBC 10 10 

- Sometimes get the compile error: "Typedef redefinition with different types".
    - It's referring to line 61 JustGarble/include/aes.h, 
        - `typedef struct { __m128i rd_key[15]; int rounds; } AES_KEY;`
    - And to line 84 openssl/aes.h,
        - `typedef struct aes_key_st AES_KEY`
    - My weird solution to the error - not sure why it works right now, but will figure it out - to go back to previous commmit and generate the .o files.
    - So I do:
        - 'make clean`
        - `git checkout 0610e654354982b11fc0537ae93774bdc743b5d6`
        - `make`
        - `git checkout master`
        - `make`
    - And for some reason that works.
    - But I if I do `make clean; make;`, I get the error.
    - It's weird because the file in the 2pc code that includes these files is `src/ot_np.c`, and that file hasn't been updated since the project was started.

## File Organization
- Core 2pc files:
    - `2pc_garbler`
        - functions that garbler would run, including `garbler_init()`
    - `2pc_evaluator`
        - functions that evaluator would run
    - `2pc_garbled_circuit`
        - has `CircuitType`, `ChainedGarbledCircuit`, `GCsMetadata`
        - also save and load methods for garbled circuits
    - `2pc_function_spec`
        - has functions related to `FunctionSpec` and json specification

## FAQ
- json file not working,
    - make sure that everything is on a single line
    - verify format: try [json online editor](http://www.jsoneditoronline.org/)

## The leven core circuit. (notes for me)
I am using the leven core circuit from "Faster two party computation using garbled circuits". 
Let D0, D1, D1, S0, S1 represent the inputs to the circuit.

The output of min2 is the minimum(D0,D1,D2) where the switch, which is plugged into the mux21, should be 0
if the mininimum is either D0,D1, and should be 1 if the minimum is D2. The point here is that when we add, we always add a 1 if D0 or D1 is the min, but if the min is D2, then one is only added if T = 1, in other words, we add T. 
