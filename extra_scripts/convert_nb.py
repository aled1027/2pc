
import json

with open('d.json') as the_file:
    data = json.load(the_file)

priors = data['prior']
conds = data['conditionals']

scale = 10 ** 17
def mapper(li):
    return list(map(lambda x: '{:.0f}'.format(x * scale), li))

priors = mapper(priors)

new_conds = []
for i, outer in enumerate(conds):
    print("outer: ", len(outer))
    new = []
    for j, middle in enumerate(outer):
        print("middle: ", len(middle))
        new_vals = mapper(middle)
        new.extend(new_vals)
    new_conds.extend(new)

all_data = priors + new_conds

data = {
    'name': 'wdbc_nb',
    'type': 'nb',
    'data': all_data,
    'num_len': 52
    }
s = json.dumps(data)
#print(s)






