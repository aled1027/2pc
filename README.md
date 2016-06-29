# CompGC
- offline/online 2PC

## Dependencies
1. [libgarble](https://github.com/amaloz/libgarble)
1. [jannson](http://www.digip.org/jansson/)
    - interface for JSON
    - To install, follow instructions in their [docs](https://jansson.readthedocs.org/en/2.7/gettingstarted.html)

## Setup
1. Install dependencies.
1. Clone this repo: 
   - `git clone https://github.com/aled1027/CompGC.git`
   - `cd CompGC`
1. Be sure that there are directories `files/evaluator_gcs` and `files/garbler_gcs` in the CompGC directory
    - `mkdir files; mkdir files/evaluator_gcs; mkdir files/garbler_gcs`
    - these are where the garbled circuits are saved during the offline phase

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

## File Organization
- Core CompGC files:
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

- bits use little-endian, where least signficant bit is on the left. 

- changed or gate
- now getting some nondeterminism in mux21
    - run valgrind
    - check if or gate
    - check if fixed one wire

## TODO
- parse ml models
    - load ml model via json
    - make a new file/module
    - preferably a path to the model, and then load model through json.
    - model tells what type it is, and size

- Looks like they multiply by 1e13
    - but, values look like they need 1e14 or 1e15.
    - use `10**17` as multipler for wdbc
- make sure operations are signed?
    - little-endian
    - 2s complement
