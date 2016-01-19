from pprint import pprint
import json # json.dumps
from collections import OrderedDict
"""
Generates the json for the AND Circuit
"""

def addANDWires(ret_dict, i0, i1):
    this_gc_id = ret_dict['circs_used']
    ret_dict['circs_used'] += 1

    r = OrderedDict()
    r["type"] = "EVAL"
    r["gc_id"] = this_gc_id
    ret_dict['instructions'].append(r)

    # input_mapping
    r = OrderedDict()
    r["inputter"] = "garbler"
    r["start_input_idx"] = ret_dict['garb_input_idx']
    r["end_input_idx"] = ret_dict['garb_input_idx'] + 1
    ret_dict['garb_input_idx'] += 2
    r["gc_id"] = this_gc_id
    r["start_wire_idx"] = 0
    r["end_wire_idx"] = 1
    ret_dict['input_mapping'].append(r)

    return this_gc_id

def addANDChained(ret_dict, circ0, idx0, circ1, idx1):
    """
    Adds an AND gate where both inputs come from a circuit
    """
    this_gc_id = ret_dict['circs_used']
    ret_dict['circs_used'] += 1

    # instructions
    # use chaining
    r = OrderedDict()
    r["type"] = "CHAIN"
    r["from_gc_id"] = circ0
    r["from_wire_id_start"] = idx0
    r["from_wire_id_end"] = idx0
    r["to_gc_id"] = this_gc_id
    r["to_wire_id_start"] = 0
    r["to_wire_id_end"] = 0
    ret_dict['instructions'].append(r)

    r = OrderedDict()
    r["type"] = "CHAIN"
    r["from_gc_id"] = circ1
    r["from_wire_id_start"] = idx1
    r["from_wire_id_end"] = idx1
    r["to_gc_id"] = this_gc_id
    r["to_wire_id_start"] = 1
    r["to_wire_id_end"] = 1
    ret_dict['instructions'].append(r)
    r = OrderedDict()

    r["type"] = "EVAL"
    r["gc_id"] = this_gc_id
    ret_dict['instructions'].append(r)

    return this_gc_id

def addOutput(ret_dict, circs):
    for c in circs:
        r = OrderedDict()
        r['gc_id'] = c
        r['start_wire_idx'] = 0
        r['end_wire_idx'] = 0
        ret_dict['output'].append(r)

def addComponents(ret_dict):
    r = OrderedDict()
    r['type'] = 'AND21'
    r['num'] = ret_dict['circs_used']
    r['circuit_ids'] = list(range(r['num']))
    ret_dict['components'].append(r)

def generateANDJSON(n, nlayers)
    m = n
    ret_dict = OrderedDict()
    ret_dict['input_mapping'] = []
    ret_dict['instructions'] = []
    ret_dict['output'] = []
    ret_dict['components'] = []
    ret_dict['circs_used'] = 0
    ret_dict['garb_input_idx'] = 0

    ret_dict['metadata'] = OrderedDict()
    ret_dict['metadata']['n'] = n
    ret_dict['metadata']['m'] = m

    assert(n >= 2) #otherwise, what is going to be ANDed?
    prev_circ = 0

    circs = list(range(n))
    for i in range(0,n,2):
        circs[i] = circs[i+1] = addANDWires(ret_dict, i, i+1)

    for i in range(1,nlayers):
        for j in range(0, n, 2):
            circs[j] = circs[j+1] = addANDChained(ret_dict, circs[j], 0, circs[j+1], 0)

    addOutput(ret_dict, circs)
    addComponents(ret_dict)

    ret_dict['metadata']['num_garb_inputs'] = n
    ret_dict['metadata']['num_eval_inputs'] = 0
    del ret_dict['circs_used']
    del ret_dict['garb_input_idx']

    s = json.dumps(ret_dict)
    print(s)

if __name__=='__main__':
    n = 4
    nlayers = 4
    generateANDJSON(n, nlayers)
