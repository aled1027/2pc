from collections import OrderedDict
import json

def hyperplane_json(n, num_len):
    ret_dict = OrderedDict()
    # mesage bits input + key bits input + iv
    assert n > 1 # 1 is a bit of a special case
    assert n % 2 == 0
    assert n % num_len == 0

    m = 1

    ret_dict['metadata'] = OrderedDict({
        "n": n,
        "m": 1,
        "num_len": num_len,
        "num_garb_inputs": n / 2,
        "num_eval_inputs": n / 2
    })

    ret_dict['input_mapping'] = []
    ret_dict['output'] = []
    ret_dict['instructions'] = []
    ret_dict['components'] = []
    ret_dict['gcs_used'] = 1

    # Components
    ret_dict['components'] = [
        OrderedDict({
            "type": "INNER_PRODUCT",
            "num": 1,
            "circuit_ids": [1]
        }),
        OrderedDict({
            "type": "GR0",
            "num": 1,
            "circuit_ids": [2]
            })
    ]

    # Input
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
            "inputter": "evaluator",
            "start_input_idx": 0,
            "end_input_idx": num_len - 1,
            "gc_id": 1,
            "start_wire_idx": num_len,
            "end_wire_idx": (2 * num_len) - 1
        })
    ]

    # Instructions
    ret_dict["instructions"] = [
        OrderedDict({
            "type": "EVAL",
            "gc_id": 1
        }),
        OrderedDict({
            "type": "CHAIN",
            "from_gc_id": 1,
            "from_wire_id_start": 0,
            "from_wire_id_end": num_len,
            "to_gc_id": 2,
            "to_wire_id_start": 0,
            "to_wire_id_end": num_len
        }),
        OrderedDict({
            "type": "EVAL",
            "gc_id": 2
        }),
    ]

    # Output
    ret_dict["output"] = [OrderedDict({
        "start_wire_idx": 0,
        "end_wire_idx": 0,
        "gc_id": 2,
    })]

    s = json.dumps(ret_dict)
    print(s)

if __name__=='__main__':
    hyperplane_json(2*55*31, 55)
