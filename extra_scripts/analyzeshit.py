import pandas as pd
import numpy as np
from pprint import pprint

import csv
from collections import OrderedDict

# add "type:time" to top of csv file
# this is the header

def toMS(x):
    return x / 1000000

#f = "eval_simd.csv"
f = "out.csv"
#f = "eval_norm.csv"
#f = "garb_simd.csv"
#f = "garb_norm.csv"

df = pd.read_csv(f, sep=':')

d = OrderedDict()
for i,row in df.iterrows():
    if row['type'] in d:
        d[row['type']].append(row['time'])
    else:
        d[row['type']] = [row['time']]

avg_d = OrderedDict()
for key in d.keys():
    avg_d[key] = np.mean(d[key])

#total = d['total']
total = avg_d['total_without_loading']

perc_d = OrderedDict()
for key in avg_d.keys():
    perc_d[key] = avg_d[key] / float(total)

#actual_avg_time = 0
#actual_avg_time += avg_d['evaluate']
#actual_avg_time += avg_d['map outputs']
#actual_avg_time += avg_d['ot correction']
#actual_avg_time += avg_d['receive labels']
#actual_avg_time += avg_d['receive circmap']
#actual_avg_time += avg_d['recvInstructions total']
#actual_avg_time += avg_d['receive output/outputmap']
#print("real avg time:", actual_avg_time)
#print("real avg time:", toMS(actual_avg_time))
#
#pprint(avg_d)
#pprint(perc_d)

#n = avg_d['total'] - avg_d['waiting'] - avg_d['loading']
#pprint(n)

#with open('eval_data.csv', 'w') as csvfile:
#    writer = csv.writer(csvfile, delimiter=',')
#    writer.writerow(['loading', 'wait', 'recvinstructions', 'eval', 'total'])
#    l = zip(d['loading'], d['waiting'], d['recvInstructions total'], d['evaluate'], d['total'])
#    for a in l:
#        writer.writerow(a)
