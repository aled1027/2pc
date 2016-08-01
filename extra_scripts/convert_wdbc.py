
from pprint import pprint
import json

the_dict = json.load(open("old_wdbc.json", "r"))
data = the_dict['data']

multiple = 10 ** 17
new_data  = list(map(lambda x: x * multiple, data))
new_data = list(map(lambda x: int('{:.0f}'.format(x)), new_data))


the_dict['data'] = new_data
json.dump(the_dict, open("wdbc.json", 'w'))
