from pprint import pprint
import json # json.dumps
from collections import OrderedDict
import argparse
import math

def initializeComponents(ret_dict):
    l = ret_dict['meta_data']['l']
    r = OrderedDict()
    r['type'] = 'LEVEN_CORE'
    r['num'] = l*l
    r['circuit_ids'] = list(range(l*l))
    ret_dict['components'].append(r)

def printD(D):
    for r in D:
        pprint(r)

def fillD(ret_dict, D):

    l = ret_dict['meta_data']['l']
    D_int_size = ret_dict['meta_data']['D_int_size']
    core_n = ret_dict['meta_data']['core_n']
    inputs_devoted_to_D = ret_dict['meta_data']['inputs_devoted_to_D']

    D = [[None] * (l+1) for _ in range(l+1)]

    """
    D[i][j] = [start,end]list of input wires if there is no circuit
    D[i][j] = gc_id of circuit whihc outputs distance[i][j]
    """
    initialInputs = list(range(inputs_devoted_to_D))

    # Fill D[0][:] and D[:][0]
    p = 0
    for i in range(l+1):
        # minus 1 because inclusive
        D[0][i] = [p, p + D_int_size - 1]
        D[i][0] = [p, p + D_int_size - 1]
        p += D_int_size

    for x in range(1,l+1):
        for y in range(1,l+1):
            this_gc_id = ret_dict['gcs_used']
            D[x][y] = this_gc_id
            ret_dict['gcs_used'] += 1

            # D[x-1[y-1]
            if isinstance(D[x-1][y-1], list):
                # use input mapping
                r = OrderedDict()
                r["inputter"] = "garbler"
                r["start_input_idx"] = D[x-1][y-1][0]
                r["end_input_idx"] = D[x-1][y-1][1]
                r["gc_id"] = this_gc_id
                r["start_wire_idx"] = 0
                r["end_wire_idx"] = D_int_size - 1
                ret_dict['InputMapping'].append(r)
            else:
                # use chaining
                r = OrderedDict()
                r["type"] = "CHAIN"
                r["from_gc_id"] = D[x-1][y-1]
                r["from_wire_id_start"] = 0
                r["from_wire_id_end"] = D_int_size - 1
                r["to_gc_id"] = this_gc_id
                r["to_wire_id_start"] = 0
                r["to_wire_id_end"] = D_int_size - 1
                ret_dict['instructions'].append(r)

            # D[x-1][y]
            if isinstance(D[x-1][y], list):
                r = OrderedDict()
                r["inputter"] = "garbler"
                r["start_input_idx"] = D[x-1][y][0]
                r["end_input_idx"] = D[x-1][y][1]
                r["gc_id"] = this_gc_id
                r["start_wire_idx"] = D_int_size
                r["end_wire_idx"] = (2*D_int_size) - 1
                ret_dict['InputMapping'].append(r)
            else:
                # use chaining
                r = OrderedDict()
                r["type"] = "CHAIN"
                r["from_gc_id"] = D[x-1][y]
                r["from_wire_id_start"] = 0
                r["from_wire_id_end"] = D_int_size - 1
                r["to_gc_id"] = this_gc_id
                r["to_wire_id_start"] = D_int_size
                r["to_wire_id_end"] = (2*D_int_size) - 1
                ret_dict['instructions'].append(r)
            # D[x][y-1]
            if isinstance(D[x][y-1], list):
                r = OrderedDict()
                r["inputter"] = "garbler"
                r["start_input_idx"] = D[x][y-1][0]
                r["end_input_idx"] = D[x][y-1][1]
                r["gc_id"] = this_gc_id
                r["start_wire_idx"] = 2*D_int_size
                r["end_wire_idx"] = (3*D_int_size) - 1
                ret_dict['InputMapping'].append(r)
            else:
                # use chaining
                r = OrderedDict()
                r["type"] = "CHAIN"
                r["from_gc_id"] = D[x][y-1]
                r["from_wire_id_start"] = 0
                r["from_wire_id_end"] = D_int_size - 1
                r["to_gc_id"] = this_gc_id
                r["to_wire_id_start"] = 2*D_int_size
                r["to_wire_id_end"] = (3*D_int_size) - 1
                ret_dict['instructions'].append(r)
            # symbol0
            start_symbol0 = inputs_devoted_to_D + ((x-1) * 2)
            r = OrderedDict()
            r["inputter"] = "garbler"
            r["start_input_idx"] = start_symbol0
            r["end_input_idx"] = start_symbol0 + 1
            r["gc_id"] = this_gc_id
            r["start_wire_idx"] = 3*D_int_size
            r["end_wire_idx"] = (3*D_int_size) + 1
            ret_dict['InputMapping'].append(r)

            # symbol1
            start_symbol1 = inputs_devoted_to_D + (2*l) + ((x-1) * 2)
            r = OrderedDict()
            r["inputter"] = "evaluator"
            r["start_input_idx"] = start_symbol1
            r["end_input_idx"] = start_symbol1 + 1
            r["gc_id"] = this_gc_id
            r["start_wire_idx"] = 3*D_int_size + 2
            r["end_wire_idx"] = (3*D_int_size) + 3
            ret_dict['InputMapping'].append(r)

            # evaluate this circuit
            r = {}
            r["type"] = "EVAL"
            r["gc_id"] = this_gc_id
            ret_dict['instructions'].append(r)

def addOutput(ret_dict):
    r = OrderedDict()
    r['gc_id'] = ret_dict['gcs_used']-1
    r['start_wire_idx'] = 0
    r['end_wire_idx'] = ret_dict['m'] - 1 # minus 1 because inclusive
    ret_dict['Output'].append(r)

def processTotRawInputs(ret_dict):
    num_raw_inputs = 0
    for i, inp in enumerate(ret_dict['InputMapping']):
        num_raw_inputs += (inp['end_wire_idx'] - inp['start_wire_idx'] + 1)
    print("num raw inputs", num_raw_inputs)
    ret_dict['tot_raw_inputs'] = num_raw_inputs

def processTotRawInstructions(ret_dict):
    num_raw_instrs = 0
    for i,instruction in enumerate(ret_dict['instructions']):
        if instruction['type'] == 'EVAL':
            num_raw_instrs += 1
        elif instruction['type'] == 'CHAIN':
            num_raw_instrs += (instruction['to_wire_id_end'] - instruction['to_wire_id_start'] + 1)
        else:
            raise RuntimeError("instruction type not detected")
    ret_dict['tot_raw_instructions'] = num_raw_instrs

ret_dict = OrderedDict()
# mesage bits input + key bits input + iv
l = 2
assert(l > 1) # 1 is a bit of a special case
D_int_size = int(math.floor(math.log2(l)) + 1)
inputs_devoted_to_D = D_int_size * (l+1)
n = inputs_devoted_to_D + (2*2*l)
m = D_int_size
num_eval_inputs = 2*l
num_garb_inputs = n - num_eval_inputs
core_n = (3*D_int_size) + 4

ret_dict['meta_data'] = OrderedDict({
    "l": l,
    "D_int_size": D_int_size,
    "core_n": core_n,
    "core_m": D_int_size,
    "inputs_devoted_to_D": inputs_devoted_to_D})


ret_dict['garbler_input_idx'] = num_garb_inputs
ret_dict['evaluator_input_idx'] = num_eval_inputs
ret_dict['n'] = n
ret_dict['m'] = m
ret_dict['InputMapping'] = []
ret_dict['Output'] = []
ret_dict['instructions'] = []
ret_dict['components'] = []
ret_dict['gcs_used'] = 0
ret_dict['tot_raw_instructions'] = 0
ret_dict['tot_raw_inputs'] = 0

D = []
initializeComponents(ret_dict)
fillD(ret_dict, D)
addOutput(ret_dict)
processTotRawInstructions(ret_dict)
processTotRawInputs(ret_dict)

del ret_dict['gcs_used']
s = json.dumps(ret_dict)
print(s)


