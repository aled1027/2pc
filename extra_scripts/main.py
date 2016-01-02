from pprint import pprint
import json # json.dumps

# assumes evaluator is supplying message
# garbler is providing iv and key

# aes takes 128*10 inputs from garbler and 128 bits from evaluator, outputing 128 bits
# requires 10 gcs
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
