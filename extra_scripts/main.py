from pprint import pprint
import json # json.dumps
from collections import OrderedDict

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
    r["gc_id"] = 0
    r["start_wire_idx"] = 0
    r["end_wire_idx"] = 127
    ret_dict['InputMapping'].append(r)
    ret_dict['garbler_input_idx'] += 128

    r = OrderedDict()
    r["inputter"] = "evaluator"
    r["start_input_idx"] = 0
    r["end_input_idx"] = 127
    r["gc_id"] = 0
    r["start_wire_idx"] = 128
    r["end_wire_idx"] = 255
    ret_dict['InputMapping'].append(r)
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
    """AES cannot be the first component -- could that, but not as is"""

    assert(ret_dict['gcs_used'] > 0) # aes cannot be first component

    for i in range(num_rounds):
        # DO NOT REORDER THINGS IN THIS FUNCTION. ORDER MATTERS
        # THE +1 AND -1s WILL BE MESSED UP

        # Update InputMapping
        # plug in garbler's input to AES_ROUND component

        r = OrderedDict()
        r["inputter"] = "garbler"
        r["start_input_idx"] = ret_dict['garbler_input_idx']
        r["end_input_idx"] = ret_dict['garbler_input_idx'] + 127
        r["gc_id"] = ret_dict['gcs_used']
        r["start_wire_idx"] = 128
        r["end_wire_idx"] = 255
        ret_dict['InputMapping'].append(r)
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
    ret_dict['Output'].append(r)

def addMessageBlockXOR(ret_dict):
    """
        ORDER MATTERS: DO NOT CHANGE ORDER OF LINES
        takes the previous component and xors with an input from the evaluator
        At a higher level, xors in the next message block from evaluator
    """
    # always chain to the first 127 wires
    # InputMapping
    r = OrderedDict()
    r["inputter"] = "evaluator"
    r["start_input_idx"] = ret_dict['evaluator_input_idx']
    r["end_input_idx"] = ret_dict['garbler_input_idx'] + 127
    r["gc_id"] = ret_dict['gcs_used']
    r["start_wire_idx"] = 128
    r["end_wire_idx"] = 255
    ret_dict['InputMapping'].append(r)
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

def processTotRawInstructions(ret_dict):
    num_raw_instrs = 0
    for i,instruction in enumerate(ret_dict['instructions']):
        if instruction['type'] == 'EVAL':
            num_raw_instrs += 1
        elif instruction['type'] == 'CHAIN':
            num_raw_instrs += (instruction['to_wire_id_end'] - instruction['to_wire_id_start'] + 1)
        else:
            RuntimeError("instruction type not detected")
    ret_dict['tot_raw_instructions'] = num_raw_instrs

num_message_blocks = 10
num_rounds = 10
assert(num_message_blocks > 0)
ret_dict = OrderedDict()
ret_dict['meta_data'] = {"num_message_blocks": num_message_blocks,
        "num_rounds": num_rounds}
ret_dict['n'] = (num_message_blocks*num_rounds*128) + 128 # key, m_i + IV
ret_dict['m'] = num_message_blocks*128
ret_dict['InputMapping'] = []
ret_dict['Output'] = []
ret_dict['instructions'] = []
ret_dict['components'] = []
ret_dict['gcs_used'] = 0
ret_dict['garbler_input_idx'] = 0
ret_dict['evaluator_input_idx'] = 0
ret_dict['tot_raw_instructions'] = 0

initializeComponents(ret_dict)
addIVXOR(ret_dict)
addAES(ret_dict, num_rounds=num_rounds)
for i in range(num_message_blocks-1):
    print("in for loop, i=",i)
    addMessageBlockXOR(ret_dict)
    addAES(ret_dict, num_rounds=num_rounds)
processTotRawInstructions(ret_dict)

s = json.dumps(ret_dict)
print(s)
