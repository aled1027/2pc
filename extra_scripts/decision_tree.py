from collections import OrderedDict
import json
import pprint

def go(num_len):

    depth = 4
    num_nodes = 4
    num_eval_inputs = 4 * num_len
    num_garb_inputs = num_eval_inputs
    n = num_eval_inputs + num_garb_inputs
    m = 4
    ret_dict = OrderedDict()
    ret_dict['metadata'] =  OrderedDict({
        "n": n,
        "m": m,
        "num_len": num_len,
        "num_nodes": num_nodes,
        "depth": depth,
        "num_garb_inputs": num_garb_inputs,
        "num_eval_inputs": num_eval_inputs,
        "instructions_size": 60,
        "input_mapping_size": 8,
    })

    ret_dict['components'] = [
        OrderedDict({
            "type": "SIGNED_LESS_THAN",
            "num": 3,
            "circuit_ids": [1,2,3,4]
        }),
        OrderedDict({
            "type": "AND",
            "num": 2,
            "circuit_ids": [5,6,7]
        }),
    ]

    ret_dict["input_mapping"] = [
        OrderedDict({
            "inputter": "garbler",
            "start_input_idx": 0,
            "end_input_idx": num_len - 1,
            "gc_id": 1,
            "start_wire_idx": 0,
            "end_wire_idx": num_len - 1,
        }),
        OrderedDict({
            "inputter": "garbler",
            "start_input_idx": num_len,
            "end_input_idx": (2 * num_len) - 1,
            "gc_id": 2,
            "start_wire_idx": 0,
            "end_wire_idx": num_len - 1,
        }),
        OrderedDict({
            "inputter": "garbler",
            "start_input_idx": 2 * num_len,
            "end_input_idx": (3 * num_len) - 1,
            "gc_id": 3,
            "start_wire_idx": 0,
            "end_wire_idx": num_len - 1,
        }),
        OrderedDict({
            "inputter": "garbler",
            "start_input_idx": 3 * num_len,
            "end_input_idx": (4 * num_len) - 1,
            "gc_id": 4,
            "start_wire_idx": 0,
            "end_wire_idx": num_len - 1,
        }),
        OrderedDict({
            "inputter": "evaluator",
            "start_input_idx": 0,
            "end_input_idx": num_len - 1,
            "gc_id": 1,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len)- 1,
        }),
        OrderedDict({
            "inputter": "evaluator",
            "start_input_idx": num_len,
            "end_input_idx": (2 * num_len) - 1,
            "gc_id": 2,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len) - 1,
        }),
        OrderedDict({
            "inputter": "evaluator",
            "start_input_idx": 2 * num_len,
            "end_input_idx": (3 * num_len) - 1,
            "gc_id": 3,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len) - 1,
        }),
        OrderedDict({
            "inputter": "evaluator",
            "start_input_idx": 3 * num_len,
            "end_input_idx": (4 * num_len) - 1,
            "gc_id": 4,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len) - 1,
        }),]

    # Instructions
    ret_dict["instructions"] = [
        OrderedDict({
            "type": "EVAL",
            "gc_id": 1
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 2
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 3
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 4
        }),
        # plug outputs of comparisons into AND gates
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 2,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 5,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 3,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 6,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 4,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 7,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        # Plug output of circuit 1 into AND gate
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 5,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),
        # Plug output of not circuit 1 into and gate
        OrderedDict({
            "type": "EVAL",
            "gc_id": 5
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 5,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 6,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 6
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 6,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 7,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 7
        }),
    ]

    # Output
    ret_dict["output"] = [
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 1,
        }),
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 5,
        }),
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 6,
        }),
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 7,
        })
    ]

    s = json.dumps(ret_dict)
    print(s)

if __name__ == '__main__':
    num_len = 3
    go(num_len)
