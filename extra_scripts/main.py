from pprint import pprint
import json # json.dumps
from collections import OrderedDict

# assumes evaluator is supplying message
# garbler is providing iv and key

# aes takes 128*10 inputs from garbler and 128 bits from evaluator, outputing 128 bits
# requires 10 gcs

def add_aes():
    rounds = 2
    start_gc = 1

    # INPUT MAPPING
    gc_id = start_gc
    input_mapping = []
    for i in range(rounds):
        r = {}
        r["inputter"] = "garbler"
        r["start_input_idx"] = i*128
        r["end_input_idx"] = (i+1)*128 - 1
        r["gc_id"] = gc_id
        r["start_wire_idx"] = 128
        r["end_wire_idx"] = 255
        gc_id += 1
        input_mapping.append(r)

    instructions = []
    gc_id = start_gc
    for j in range(rounds-1):
        r = {}
        r["type"] = "EVAL"
        r["gc_id"] = gc_id
        instructions.append(r)

        r = {}
        r["type"] = "CHAIN"
        r["from_gc_id"] = gc_id
        r["from_wire_id_start"] = 0
        r["from_wire_id_end"] = 127

        r["to_gc_id"] = gc_id+1
        r["to_wire_id_start"] = 0
        r["to_wire_id_end"] = 127

        gc_id += 1
        instructions.append(r)

    r = {}
    r["type"] = "EVAL"
    r["gc_id"] = gc_id
    instructions.append(r)

    s = json.dumps(instructions)
    t = json.dumps(input_mapping)


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
    r["gc_id"] = 0
    r["start_wire_idx"] = 0
    r["end_wire_idx"] = 127
    ret_dict['input_mapping'].append(r)
    ret_dict['garbler_input_idx'] += 128

    r = OrderedDict()
    r["inputter"] = "evaluator"
    r["start_input_idx"] = 0
    r["end_input_idx"] = 127
    r["gc_id"] = 0
    r["start_wire_idx"] = 128
    r["end_wire_idx"] = 255
    ret_dict['input_mapping'].append(r)
    ret_dict['evaluator_input_idx'] += 128

    r = OrderedDict()
    r["type"] = "EVAL"
    r["gc_id"] = 0
    ret_dict['instructions'].append(r)

    for component in ret_dict['components']:
        if component['type'] == 'XOR':
            component['circuit_ids'].append(0)
            component['num'] += 1
            ret_dict['gcs_used'] += 1

def addAES(ret_dict, num_rounds=10):
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
        tgt_component = 'AES_ROUND' if i != num_rounds-1 else 'AES_FINAL_FOUND'
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
    r["end_input_idx"] = ret_dict['garbler_input_idx'] + 127
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

num_message_blocks = 2
num_rounds = 2
assert(num_message_blocks > 0)
ret_dict = OrderedDict()
ret_dict['n'] = (num_message_blocks*128*2) + 128 # key, m_i + IV
ret_dict['m'] = 128 # TODO make so output is more robust
ret_dict['input_mapping'] = []
ret_dict['instructions'] = []
ret_dict['components'] = []
ret_dict['gcs_used'] = 0
ret_dict['garbler_input_idx'] = 0
ret_dict['evaluator_input_idx'] = 0

initializeComponents(ret_dict)
addIVXOR(ret_dict)
addAES(ret_dict, num_rounds=num_rounds)
for i in range(num_message_blocks-1):
    addMessageBlockXOR(ret_dict)
    #addAES(ret_dict, num_rounds=num_rounds)

s = json.dumps(ret_dict)
print(s)
