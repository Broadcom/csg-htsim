#!/usr/bin/env python

# Generate a ring allreduce traffic matrix.
# python gen_allreduce.py <nodes> <conns> <groupsize> <flowsize>
# Parameters:
# <nodes>   number of nodes in the topology
# <conns>    number of active connections
# <groupsize>    number of connections in an all-to-all group
# <flowsize>   size of the flows in bytes
# <extrastarttime>   How long in microseconds to space the start times over (start time will be random in between 0 and this time).  Can be a float.
# <randseed>   Seed for random number generator, or set to 0 for random seed

import os
import sys
from random import seed, shuffle
#print(sys.argv)
if len(sys.argv) != 8:
    print("Usage: python gen_allreduce.py <filename> <nodes> <conns> <groupsize> <flowsize> <locality> <randseed>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
conns = int(sys.argv[3])
groupsize = int(sys.argv[4])
flowsize = int(sys.argv[5])
locality = int(sys.argv[6])
randseed = int(sys.argv[7])


print("Connections: ", conns)
print("All-reduce group size: ", groupsize)
print("Flowsize: ", flowsize, "bytes")
print("Random Seed ", randseed)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections", conns*(2*groupsize-1), file=f)
print("Triggers", conns*(2*groupsize-2), file=f)

srcs = []
dsts = []
groups = conns // groupsize;

print("Groups ", groups)

for n in range(nodes):
    srcs.append(n)

if randseed != 0:
    seed(randseed)

shuffle(srcs)

id = 0
trig_id = 1
for group in range(groups):
    print("group: ", group)
    groupsrcs = []
    for n in range(groupsize):
        groupsrcs.append(srcs[group * groupsize + n])

    if (locality==1):
        groupsrcs.sort()

    print(groupsrcs)
    for s in range(groupsize):
        print (" ")
        for d in range(1, 2*groupsize):
            id += 1
            src = (s+d-1)%groupsize
            dst = (s+d)%groupsize
            out = str(groupsrcs[src]) + "->" + str(groupsrcs[dst]) + " id " + str(id)

            if d == 1:
                out = out + " start 0"
            else:
                out = out + " trigger " + str(trig_id)
                trig_id += 1

            out = out + " size " + str(flowsize)
            if d != 2 * groupsize - 1:
                out = out + " send_done_trigger " + str(trig_id)
            print(out, file=f)
            print(groupsrcs[src], "->", groupsrcs[dst])

for t in range(1, trig_id):
    out = "trigger id " + str(t) + " oneshot"
    print(out, file=f)

f.close()
