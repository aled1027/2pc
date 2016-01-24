from pprint import pprint
import json # json.dumps
from collections import OrderedDict
import argparse


# assumes evaluator is supplying message
# garbler is providing iv and key

# aes takes 128*10 inputs from garbler and 128 bits from evaluator, outputing 128 bits
# requires 10 gcs

def initializeComponents(ret_dict):
    r = OrderedDict()
    r['type'] = 'AES_ROUND'
    r['num'] = 0
    r['circuit_ids'] = []
    ret_dict['components'].append(r)

    r = OrderedDict()
    r['type'] = 'AES_FINAL_ROUND'
    r['num'] = 0
    r['circuit_ids'] = []
    ret_dict['components'].append(r)

    r = OrderedDict()
    r['type'] = 'XOR'
    r['num'] = 0
    r['circuit_ids'] = []
    ret_dict['components'].append(r)

def addIVXOR(ret_dict):
    """ Add XOR IV """
    r = OrderedDict()
    r["inputter"] = "garbler"
    r["start_input_idx"] = 0
    r["end_input_idx"] = 127
    r["gc_id"] = 1
    r["start_wire_idx"] = 0
    r["end_wire_idx"] = 127
    ret_dict['input_mapping'].append(r)
    ret_dict['garbler_input_idx'] += 128

    r = OrderedDict()
    r["inputter"] = "evaluator"
    r["start_input_idx"] = 0
    r["end_input_idx"] = 127
    r["gc_id"] = 1
    r["start_wire_idx"] = 128
    r["end_wire_idx"] = 255
    ret_dict['input_mapping'].append(r)
    ret_dict['evaluator_input_idx'] += 128

    r = OrderedDict()
    r["type"] = "EVAL"
    r["gc_id"] = 1
    ret_dict['instructions'].append(r)

    for component in ret_dict['components']:
        if component['type'] == 'XOR':
            component['circuit_ids'].append(1)
            component['num'] += 1
    ret_dict['gcs_used'] += 1

def addAES(ret_dict, num_rounds=10):
    """AES cannot be the first component -- could that, but not as is"""

    assert(ret_dict['gcs_used'] > 0) # aes cannot be first component

    for i in range(num_rounds):
        # DO NOT REORDER THINGS IN THIS FUNCTION. ORDER MATTERS
        # THE +1 AND -1s WILL BE MESSED UP

        # Update input_mapping
        # plug in garbler's input to AES_ROUND component

        r = OrderedDict()
        r["inputter"] = "garbler"
        r["start_input_idx"] = ret_dict['garbler_input_idx']
        r["end_input_idx"] = ret_dict['garbler_input_idx'] + 127
        r["gc_id"] = ret_dict['gcs_used']
        r["start_wire_idx"] = 128
        r["end_wire_idx"] = 255
        ret_dict['input_mapping'].append(r)
        ret_dict['garbler_input_idx'] += 128

        # Update components
        tgt_component = 'AES_ROUND' if i != num_rounds-1 else 'AES_FINAL_ROUND'
        for component in ret_dict['components']:
            if component['type'] == tgt_component:
                component['circuit_ids'].append(ret_dict['gcs_used'])
                component['num'] += 1
        ret_dict['gcs_used'] += 1

        # Update instructions
        # Add chain into this gc.
        # should be chained from previous function.
        r = OrderedDict()
        r["type"] = "CHAIN"
        r["from_gc_id"] = ret_dict['gcs_used'] - 2
        r["from_wire_id_start"] = 0
        r["from_wire_id_end"] = 127
        r["to_gc_id"] = ret_dict['gcs_used'] - 1
        r["to_wire_id_start"] = 0
        r["to_wire_id_end"] = 127
        ret_dict['instructions'].append(r)

        r = OrderedDict()
        r["type"] = "EVAL"
        r["gc_id"] = ret_dict['gcs_used']-1
        ret_dict['instructions'].append(r)

    r = OrderedDict()
    r['gc_id'] = ret_dict['gcs_used']-1
    r['start_wire_idx'] = 0
    r['end_wire_idx'] = 127
    ret_dict['output'].append(r)

def addMessageBlockXOR(ret_dict):
    """
        ORDER MATTERS: DO NOT CHANGE ORDER OF LINES
        takes the previous component and xors with an input from the evaluator
        At a higher level, xors in the next message block from evaluator
    """
    # always chain to the first 127 wires
    # input_mapping
    r = OrderedDict()
    r["inputter"] = "evaluator"
    r["start_input_idx"] = ret_dict['evaluator_input_idx']
    r["end_input_idx"] = ret_dict['evaluator_input_idx'] + 127
    r["gc_id"] = ret_dict['gcs_used']
    r["start_wire_idx"] = 128
    r["end_wire_idx"] = 255
    ret_dict['input_mapping'].append(r)
    ret_dict['evaluator_input_idx'] += 128

    # components
    for component in ret_dict['components']:
        if component['type'] == 'XOR':
            component['circuit_ids'].append(ret_dict['gcs_used'])
            component['num'] += 1
            ret_dict['gcs_used'] += 1

    # instructions
    r = OrderedDict()
    r["type"] = "CHAIN"
    r["from_gc_id"] = ret_dict['gcs_used'] - 2
    r["from_wire_id_start"] = 0
    r["from_wire_id_end"] = 127
    r["to_gc_id"] = ret_dict['gcs_used'] - 1
    r["to_wire_id_start"] = 0
    r["to_wire_id_end"] = 127
    ret_dict['instructions'].append(r)

    r = OrderedDict()
    r["type"] = "EVAL"
    r["gc_id"] = ret_dict['gcs_used']-1
    ret_dict['instructions'].append(r)

def processInputMappingSize(ret_dict):
    num_raw_inputs = 0
    for i, inp in enumerate(ret_dict['input_mapping']):
        num_raw_inputs += (inp['end_wire_idx'] - inp['start_wire_idx'] + 1)
    ret_dict['metadata']['input_mapping_size'] = num_raw_inputs

def processInstructionsSize(ret_dict):
    num_raw_instrs = 0
    for i,instruction in enumerate(ret_dict['instructions']):
        if instruction['type'] == 'EVAL':
            num_raw_instrs += 1
        elif instruction['type'] == 'CHAIN':
            num_raw_instrs += (instruction['to_wire_id_end'] - instruction['to_wire_id_start'] + 1)
        else:
            raise RuntimeError("instruction type not detected")
    ret_dict['metadata']['instructions_size'] = num_raw_instrs

def computeNumGates(ret_dict):
    """
    Does not change state of ret_dict, only returns number of gates

    xor component
        128 xor gates
    aes_round gates:
        num_xor 3072
        num_and 576
        num_zero 128
        num_one 128
        num_xor + num_and 3648
        total 3904
    """
    xor_gates_per_circuit = 128
    aes_round_gates_per_circuit = 3904 # this number seems high
    aes_final_gates_per_circuit = 3904
    num_gates = 0

    for component in ret_dict['components']:
        if component['type'] == 'XOR':
            num_gates += component['num'] * xor_gates_per_circuit
        elif component['type'] == 'AES_ROUND':
            num_gates += component['num'] * aes_round_gates_per_circuit
        elif component['type'] == 'AES_FINAL_ROUND':
            num_gates += component['num'] * aes_final_gates_per_circuit
        else:
            raise RuntimeError("type {} not detected".format(component['type']))
    return num_gates

def generateCBCJSON(num_message_blocks, num_rounds):
    assert(num_message_blocks > 0)
    ret_dict = OrderedDict()
    # mesage bits input + key bits input + iv

    n = (num_message_blocks*128) + (num_message_blocks*num_rounds*128) + 128
    m = num_message_blocks*128
    num_eval_inputs = 128 * num_message_blocks
    num_garb_inputs = n - num_eval_inputs

    ret_dict['metadata'] = OrderedDict({
            "n": n,
            "m": m,
            "num_garb_inputs": num_garb_inputs,
            "num_eval_inputs": num_eval_inputs
            })

    ret_dict['input_mapping'] = []
    ret_dict['output'] = []
    ret_dict['instructions'] = []
    ret_dict['components'] = []

    ret_dict['gcs_used'] = 1
    ret_dict['garbler_input_idx'] = 0
    ret_dict['evaluator_input_idx'] = 0

    initializeComponents(ret_dict)
    addIVXOR(ret_dict)
    addAES(ret_dict, num_rounds=num_rounds)
    for i in range(num_message_blocks-1):
        addMessageBlockXOR(ret_dict)
        addAES(ret_dict, num_rounds=num_rounds)
    processInputMappingSize(ret_dict)
    processInstructionsSize(ret_dict)
    num_gates = computeNumGates(ret_dict)

    assert(ret_dict['garbler_input_idx'] + ret_dict['evaluator_input_idx'] == ret_dict['metadata']['n'])

    del ret_dict['garbler_input_idx']
    del ret_dict['evaluator_input_idx']
    del ret_dict['gcs_used']

    ret_dict['metadata']["num_message_blocks"] = num_message_blocks
    ret_dict['metadata']["num_rounds"] =  num_rounds
    ret_dict['metadata']["num_gates"] = num_gates

    s = json.dumps(ret_dict)
    print(s)

if __name__=='__main__':
    # command line instructions
    parser = argparse.ArgumentParser()
    parser.add_argument('num_rounds', type=int, help='number of rounds when performing aes')
    parser.add_argument('num_message_blocks', type=int, help='number of message blocks')

    # parsing args
    args = parser.parse_args()
    num_message_blocks = args.num_message_blocks
    num_rounds = args.num_rounds

    print("Doing CBC with {} message blocks and {} rounds".format(num_message_blocks, num_rounds))
    generateCBCJSON(num_message_blocks, num_rounds)

