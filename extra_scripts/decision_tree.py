from collections import OrderedDict
import json
import pprint

def go(num_len):

    depth = 2
    num_nodes = 3
    num_eval_inputs = 2 * num_len
    num_garb_inputs = num_eval_inputs
    n = num_eval_inputs + num_garb_inputs
    m = 2
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
        "input_mapping_size": 2,
    })

    ret_dict['components'] = [
        OrderedDict({
            "type": "SIGNED_LESS_THAN",
            "num": 3,
            "circuit_ids": [1,2,3]
        }),
        OrderedDict({
            "type": "AND",
            "num": 2,
            "circuit_ids": [4,5]
        }),
        OrderedDict({
            "type": "NOT",
            "num": 1,
            "circuit_ids": [6]
        })
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
            "start_input_idx": 0,
            "end_input_idx": ret_dict['metadata']['num_eval_inputs'] - 1,
            "gc_id": 1,
            "start_wire_idx": int(n/2),
            "end_wire_idx": n - 1,
        })
    ]

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
        # Feed into not gate
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 6,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 6
        }),
        # plug outputs of comparisons into and gates
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 2,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 4,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 3,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 5,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        # Plug output of circuit 1 into and gate
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 4,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),
        # Plug output of not circuit 1 into and gate
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 6,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 5,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 3
        }),
    ]

    # Output
    ret_dict["output"] = [OrderedDict({
        "start_wire_idx": 0,
        "end_wire_idx": 0,
        "gc_id": 2,
    })]

    # Output
    ret_dict["output"] = [
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 4,
        }),
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 5,
        })
    ]

    s = json.dumps(ret_dict)
    print(s)

if __name__ == '__main__':
    num_len = 3
    go(num_len)
