from collections import OrderedDict
import json
import pprint

def ecg(num_len):
    depth = 4
    num_nodes = 6
    num_eval_inputs = 6 * num_len
    num_garb_inputs = num_eval_inputs
    n = num_eval_inputs + num_garb_inputs
    m = 5
    ret_dict = OrderedDict()
    ret_dict['metadata'] =  OrderedDict({
        "n": n,
        "m": m,
        "num_len": num_len,
        "num_nodes": num_nodes,
        "depth": depth,
        "num_garb_inputs": num_garb_inputs,
        "num_eval_inputs": num_eval_inputs,
        "instructions_size": 24,
        "input_mapping_size": 12,
    })

    ret_dict['components'] = [
        OrderedDict({
            "type": "SIGNED_COMPARISON",
            "num": 6,
            "circuit_ids": [1,2,3,4,5,6]
        }),
        OrderedDict({
            "type": "AND",
            "num": 5,
            "circuit_ids": [7,8,9,10,11]
        }),
        OrderedDict({
            "type": "NOT",
            "num": 2,
            "circuit_ids": [12,13]
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
            "inputter": "garbler",
            "start_input_idx": 4 * num_len,
            "end_input_idx": (5 * num_len) - 1,
            "gc_id": 5,
            "start_wire_idx": 0,
            "end_wire_idx": num_len - 1,
        }),
        OrderedDict({
            "inputter": "garbler",
            "start_input_idx": 5 * num_len,
            "end_input_idx": (6 * num_len) - 1,
            "gc_id": 6,
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
        }),
        OrderedDict({
            "inputter": "evaluator",
            "start_input_idx": 4 * num_len,
            "end_input_idx": (5 * num_len) - 1,
            "gc_id": 5,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len) - 1,
        }),
        OrderedDict({
            "inputter": "evaluator",
            "start_input_idx": 5 * num_len,
            "end_input_idx": (6 * num_len) - 1,
            "gc_id": 6,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len) - 1,
        }),
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
        OrderedDict({
            "type": "EVAL",
            "gc_id": 4
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 5
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 6
        }),

        # Not node 1:
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 12,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 12
        }),

        # Node two AND work
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 7,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 2,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 7,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),

        # Node two not
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 7,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 13,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 13
        }),

        # Node three and
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 3,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 8,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 12,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 8,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 8
        }),

        # node four and
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 4,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 9,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 7,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 9,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 9
        }),

        # node five and
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 5,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 10,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 13,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 10,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 10
        }),

        # node six and
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 6,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 11,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 8,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 11,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 11
        }),
    ]

    # Output
    ret_dict["output"] = [
        # In order from left to right
        # node 4
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 9,
        }),
        # node 5
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 10,
        }),
        # node 6
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 11,
        }),
        # node 3
        OrderedDict({
            "start_wire_idx": 0,
            "end_wire_idx": 0,
            "gc_id": 8,
        })
    ]

    s = json.dumps(ret_dict)
    print(s)

def nursery(num_len):

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
        "instructions_size": 13,
        "input_mapping_size": 8,
    })

    ret_dict['components'] = [
        OrderedDict({
            "type": "SIGNED_COMPARISON",
            "num": 4,
            "circuit_ids": [1,2,3,4]
        }),
        OrderedDict({
            "type": "AND",
            "num": 3,
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
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 5,
            "to_wire_id_start": 0,
            "to_wire_id_end": 0,
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 2,
            "from_wire_id_start": 0,
            "from_wire_id_end": 0,
            "to_gc_id": 5,
            "to_wire_id_start": 1,
            "to_wire_id_end": 1,
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 5
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
        # Plug output of circuit 1 into AND gate
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
        })
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
    num_len = 52
    ecg(num_len)
