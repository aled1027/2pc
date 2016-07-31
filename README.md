# CompGC
- offline/online 2PC

- misisng eval 7
- missing ones at end for to_wires
    - 11, 10, 9, 8

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
## Circuits
- libgarble circuit add
    - uses little-endian unsigned values, and passes all tests
    - e.g. 010 + 010 = 001

## Notes and TODO
- check that naive bayes offline is working
- get naive bayes online working
- put component ids in order from least to greatest. 
    - there is a bug where labels[i] and computedouptutmap[i] are allocated memory
under this assumption. It is fixable, but I would need to go back and test all of the other experiments. 

- GR0 component
    - make sure grabbing the correct bit


### The deets
- everything should be signed little-endian. see decision tree node less than circuit reference.
- wdbc
    - multiplied by `10**17`
    - used `num_len = 55`
    - garbler has model
- credit
    - multiplied by `10**18`
    - used `num_len = 58`
- naive bayes
    - v ranges from `0 to num_classes * |x| (number of components in vector x)`
         - v is big for audiology dataset
    - server inputs:
        - C = {log Pr[C=c_1], ..., log Pr[C=c_k]}
            - theoretically, a three dimensional array of shape (k, |x|, | domain(x_i))
        - T = {log Pr[x_j=v | C=c_i] | for all i in [k], j in |x|, v in domain of x_j}
            - in the code, we say:
                - k = `num_classes`
                - |x| = `vector_size`
                - `domain_size` is the size of domain of x_j
                - size of T is k * |x| * domain_size * num_len
                - v is indexed most inner, then j, the i on the outside.
                - e.g., k = |x| = domain_size = 2 
                    - {Pr[x_1=1 | C=c_1, Pr[x_1=2 | C=c_1], Pr[x_1=1 | C=c_2], Pr[x_1=2 | C=c_2], Pr[x_2=1 | C=c_1], Pr[x_2=2 | C=c_1], ...
                    - note that I don't write log.
                    - flatten in "numpy c" style
            - theoretically, we would T is 3-d dimensional array, where
                - T[i][j][v] = log Pr[x_j=v | C=c_i]
        - Algorithm
            ```
            for i in range(num_classes):
                probs[i] = C_inputs[i]
                for j in range(vector_size):
                    v = client_input[j] # is this correct?
                    probs[i] += circuit_select(T_inputs, index=i*j*v)
            the_argmax = argmax(t_val)
            return the_argmax
            ```





