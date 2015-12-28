# 2pc
- offline/online 2PC

# Setup
1. install [msgpack](http://msgpack.org/index.html)
    - a JustGarble requirement
1. Once msgpack is installed, compile justGarble
    - be sure that you are using my version of justGarble
    - In particular, you need the function `createInputLabelsWithR`, and `garbleCircuit` needs to use this function
1. install [jannson](http://www.digip.org/jansson/)
    - interface for JSON
1. Be sure that there are directories `files/evaluator_gcs` and `files/garbler_gcs`
    - these are where the garbled circuits are saved during the offline phase

# To run:
## Offline
1. Be sure everything is compiled by running `make`
1. In one terminal run `make run_garb_off`
1. In another terminal run `make run_eval_off`
1. Now the directores `files/evaluator_gcs` and `files/garbler_gcs` should be populated with files.

## Online
1. In one terminal run `make run_garb`
1. In another terminal run `make run_eval`

# Miscellaneous Notes
- to view json files: [json online editor](http://www.jsoneditoronline.org/)

### File Organization
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

