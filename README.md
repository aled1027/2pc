# 2pc
- offline/online 2PC

## To run:
```
make
make run_garb_off
make run_eval_off
make run_garb
make run_eval
```

## Alex's notes
- `__m128i`
    - used for inputLabels, outputLabels, and wires
    - keys for DKC
- to look at json files
    - http://www.jsoneditoronline.org/

### The Gist
1. Init Phase
    1. Garbler generates a bunch of GCs
    2. labels them
    3. saves them to disk
    4. sends them to evaluator
2. Online Phase - Garbler Side
    1. garbler chooses fn from json files
    2. creates instructions from json function spec
    3. sends instructions to evaluator

3. Online Phase - Evaluator Side
    1. Receive instructions from garbler.
    2. Follow instructions. This step involves OT.
    3. Send output of function back to garbler.

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

