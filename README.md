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

## Miscellaneous Notes
- to view json files: [json online editor](http://www.jsoneditoronline.org/)

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

