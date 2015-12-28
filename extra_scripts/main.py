#from pprint import pprint

import csv

fn = 'outputdata.txt'
f = open(fn, 'r')
li = []
for line in f:
    l = line.split()
    if 'Seed' in l or 'OT:' in l:
        pass
    else:
        li.append(l[0])

l1 = []
l2 = []
for i,e in enumerate(li):
    if i % 2 == 0:
        l1.append(e)
    else:
        l2.append(e)

with open('eggs.csv', 'wb') as csvfile:
    writer = csv.writer(csvfile, delimiter=',')
    writer.writerow(['OT','Total', 'in terms of cycles per gate'])
    for i,j in zip(l1,l2):
        writer.writerow([i,j])

#""" OLD, totally irrelevant things"""
#
#aes_round_output_wires = "3733 3734 3735 3736 3737 3738 3739 3740 3765 3766 3767 3768 3769 3770 3771 3772 3797 3798 3799 3800 3801 3802 3803 3804 3829 3830 3831 3832 3833 3834 3835 3836 3873 3874 3875 3876 3877 3878 3879 3880 3905 3906 3907 3908 3909 3910 3911 3912 3937 3938 3939 3940 3941 3942 3943 3944 3969 3970 3971 3972 3973 3974 3975 3976 4013 4014 4015 4016 4017 4018 4019 4020 4045 4046 4047 4048 4049 4050 4051 4052 4077 4078 4079 4080 4081 4082 4083 4084 4109 4110 4111 4112 4113 4114 4115 4116 4153 4154 4155 4156 4157 4158 4159 4160 4185 4186 4187 4188 4189 4190 4191 4192 4217 4218 4219 4220 4221 4222 4223 4224 4249 4250 4251 4252 4253 4254 4255 4256"
#
#outwires = list(map(int, aes_round_output_wires.split()))
#
## find consecutive wires
#li = []
#isSeq = False
#for i in range(len(outwires)):
#    if i == 0:
#        li.append((outwires[i], outwires[i]))
#    elif outwires[i] == outwires[i-1] + 1:
#        a,b = li[-1]
#        li[-1] = (a,b+1)
#        #li[-1][1] += 1
#        #li[-1] =
#    else:
#        li.append((outwires[i], outwires[i]))
#
#s = 0
#for a,b in li:
#    s += b - a + 1
#assert(s == 128)
#
#st_li = []
#t_start = 0
#t_end = 0
## generate the json:
#for (f_start, f_end) in li:
#    f = 0
#    t = 1
#    num_wires = f_end - f_start
#    t_end = t_start + num_wires
#    string = ["{\"type\" : \"CHAIN\",",
#            "\"from_gc_id\": {},".format(f),
#            "\"from_wire_id_start\": {},".format(f_start),
#            "\"from_wire_id_end\": {},".format(f_end),
#            "\"to_gc_id\": {},".format(t),
#            "\"to_wire_id_start\": {},".format(t_start),
#            "\"to_wire_id_end\": {}".format(t_end)]
#    t_start += num_wires + 1
#
#    st = ''.join(string)
#    st_li.append(st)
#
#
#
#l = '},'.join(st_li)
#l += '}'
#
## l is our string!
#
#
#
#
#"""
#{
#      "type": "CHAIN",
#      "from_gc_id": 0,
#      "from_wire_id_start": 0,
#      "from_wire_id_end": 127,
#      "to_gc_id": 1,
#      "to_wire_id_start": 0,
#      "to_wire_id_end": 127
#    },
#"""


