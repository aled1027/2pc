from collections import OrderedDict
import json
import pprint
import sys
from pprint import pprint

def wdbc(num_len, num_classes, vector_size, domain_size):
    """
    Pseudocode for naive bayes algorithm:

    for i in range(num_classes):
        t_val[i] = C_inputs[i]
        for j in range(vector_size):
            int t_val[num_len];
            v = client_input[j] # is this correct?
            t_val[i] += circuit_select(T_inputs, index=i*j*v)
    the_argmax = argmax(t_val)
    return the_argmax
    """


    client_input_size = vector_size * num_len
    C_size = num_classes * num_len
    T_size = num_classes * vector_size * domain_size * num_len
    n = client_input_size + C_size + T_size
    num_eval_inputs = client_input_size
    num_garb_inputs = n - client_input_size
    m = num_len

    ret_dict = OrderedDict()
    ret_dict['metadata'] =  OrderedDict({
        "n": n,
        "m": m,
        "num_len": num_len,
        "num_classes": num_classes,
        "vector_size": vector_size,
        "domain_size": domain_size,
        "num_garb_inputs": num_garb_inputs,
        "num_eval_inputs": num_eval_inputs,
    })

    ret_dict['components'] = [
        OrderedDict({
            "type": "SELECT",
            "num": 0,
            "circuit_ids": []
        }),
        OrderedDict({
            "type": "ADD",
            "num": 0,
            "circuit_ids": []
        }),
        OrderedDict({
            "type": "ARGMAX",
            "num": 0,
            "circuit_ids": []
        }),
    ]

    # Some useful constants for the upcoming computation
    t_input_start_idx = C_size
    t_input_end_idx = C_size + T_size - 1
    ret_dict['input_mapping'] = []
    ret_dict['instructions'] = []

    num_select_gc_id = num_classes * vector_size
    ret_dict['next_select_gc_id'] = 1
    ret_dict['next_add_gc_id'] = num_select_gc_id + 1

    def circuit_select(which_client_input):
        """Adds select circuit to input mapping, and instructions

        Alters state
        """
        to_gc_id = ret_dict['next_select_gc_id']
        ret_dict['next_select_gc_id'] += 1

        # Add to components
        ret_dict['components'][0]['num'] += 1
        ret_dict['components'][0]['circuit_ids'].append(to_gc_id)

        # Add to input mapping
        # Get T_inputs put here
        o = OrderedDict({
            "inputter": "garbler", # aka server
            "start_input_idx": t_input_start_idx,
            "end_input_idx": t_input_end_idx,
            "gc_id": to_gc_id,
            "start_wire_idx": 0,
            "end_wire_idx": T_size - 1
        })
        ret_dict['input_mapping'].append(o)

        client_input_start_idx = which_client_input * num_len
        client_input_end_idx = (which_client_input * num_len) + num_len - 1
        o = OrderedDict({
            "inputter": "evaluator", # aka client
            "start_input_idx": client_input_start_idx,
            "end_input_idx": client_input_end_idx,
            "gc_id": to_gc_id,
            "start_wire_idx": T_size,
            "end_wire_idx": T_size + num_len - 1
        })
        ret_dict['input_mapping'].append(o)
        # Add to instructions
        o = OrderedDict({
            "type": "EVAL",
            "gc_id": to_gc_id
        })
        ret_dict['instructions'].append(o)
        return to_gc_id

    def circuit_add(select_gc_id, prev_gc_id, c_input_idx=0):
        """Adds output of select_gc_id and prev_gc_id"""

        to_gc_id = ret_dict['next_add_gc_id']
        ret_dict['next_add_gc_id'] += 1

        # update components
        ret_dict['components'][1]['num'] += 1
        ret_dict['components'][1]['circuit_ids'].append(to_gc_id)

        # update instructions
        o1 = OrderedDict({
            "type": "CHAIN",
            "from_gc_id": select_gc_id,
            "from_wire_id_start": 0,
            "from_wire_id_end": num_len - 1,
            "to_gc_id": to_gc_id,
            "to_wire_id_start": 0,
            "to_wire_id_end": num_len - 1,
        })
        ret_dict['instructions'].append(o1)

        if prev_gc_id == 0:
            # grab val from input_mapping
            o2 = OrderedDict({
                "inputter": "garbler", # aka server
                "start_input_idx": num_len *  c_input_idx,
                "end_input_idx": (num_len * c_input_idx) + num_len - 1,
                "gc_id": to_gc_id,
                "start_wire_idx": num_len,
                "end_wire_idx": num_len + num_len - 1
            })
            ret_dict['input_mapping'].append(o2)
        else:
            # chain from prev_gc_id
            o2 = OrderedDict({
                "type": "CHAIN",
                "from_gc_id": prev_gc_id,
                "from_wire_id_start": 0,
                "from_wire_id_end": num_len - 1,
                "to_gc_id": to_gc_id,
                "to_wire_id_start": num_len,
                "to_wire_id_end": num_len + num_len - 1,
            })
            ret_dict['instructions'].append(o2)

        o3 = OrderedDict({
            "type": "EVAL",
            "gc_id": to_gc_id
        })
        ret_dict['instructions'].append(o3)

        return to_gc_id

    def circuit_argmax(probs_gc_ids):
        # take argmax of probs
        argmax_gc_id = ret_dict['next_add_gc_id']
        ret_dict['next_add_gc_id'] += 1
        ret_dict['components'][2]['num'] += 1
        ret_dict['components'][2]['circuit_ids'].append(argmax_gc_id)

        for i, prob_gc_id in enumerate(probs_gc_ids):
            # chain from gc_id to argmax
            o = OrderedDict({
                "type": "CHAIN",
                "from_gc_id": prob_gc_id,
                "from_wire_id_start": 0,
                "from_wire_id_end": num_len - 1,
                "to_gc_id": argmax_gc_id,
                "to_wire_id_start": i * num_len,
                "to_wire_id_end": (i * num_len) + num_len - 1,
            })
            ret_dict['instructions'].append(o)

        o = OrderedDict({
            "type": "EVAL",
            "gc_id": argmax_gc_id
        })
        ret_dict['instructions'].append(o)

        ret_dict["output"] = [
            OrderedDict({
                "start_wire_idx": 0,
                "end_wire_idx": num_len - 1,
                "gc_id": argmax_gc_id,
            }),
        ]

    probs_gc_ids = []
    for i in range(num_classes):
        prev = 0
        for j in range(vector_size):
            select_gc_id = circuit_select(j)
            prev = circuit_add(select_gc_id, prev, c_input_idx=i)
        probs_gc_ids.append(prev)
    circuit_argmax(probs_gc_ids)


    ret_dict['metadata']["instructions_size"] = 425 + 2756 - 1272
    ret_dict['metadata']["input_mapping_size"] = len(ret_dict['input_mapping'])
    del ret_dict['next_select_gc_id']
    del ret_dict['next_add_gc_id']
    s = json.dumps(ret_dict)
    print(s)

if __name__ == '__main__':
    num_len = 52
    num_classes = 5
    vector_size = 9
    domain_size = 5
    wdbc(num_len, num_classes, vector_size, domain_size)
