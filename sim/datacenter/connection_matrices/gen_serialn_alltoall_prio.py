#!/usr/bin/env python

# Generate a permutation traffic matrix.
# python gen_pemutation.py <nodes> <conns> <groupsize> <flowsize> <extrastarttime>
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
if len(sys.argv) != 9:
    print("Usage: python gen_serialn_alltoall.py <filename> <nodes> <conns_per_group> <groupsize> <parallel_cons> <flowsize> <extrastarttime> <randseed>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
conns = int(sys.argv[3])
groupsize = int(sys.argv[4])
parallel = int(sys.argv[5])
flowsize = int(sys.argv[6])
extrastarttime = float(sys.argv[7])
randseed = int(sys.argv[8])

if conns % groupsize != 0:
    print("conns must be a multiple of groupsize\n");
    sys.exit()

print("Nodes: ", nodes)
print("Connections: ", conns)
print("All-to-all group size: ", groupsize)
print("Flowsize: ", flowsize, "bytes")
print("ExtraStartTime: ", extrastarttime, "us")
print("Random Seed ", randseed)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections", conns*(groupsize-1), file=f)

if (conns-1) % parallel == 0:
    print("Triggers", groupsize*((conns-1)//parallel-1), file=f)
else:
    print("Triggers", groupsize*((conns-1)//parallel), file=f)

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
trig_id = 0

for group in range(groups):
    print("group: ", group)
    groupsrcs = []

    for n in range(groupsize):
        groupsrcs.append(srcs[group * groupsize + n])

    print(groupsrcs)

    half = (groupsize-1) // parallel
    left = (groupsize-1) % parallel
    print("Left is ",str(left),"parallel",parallel,"Conns per node",groupsize-1)
    for s in range(groupsize):
        prio = 0
        for d in range(1, half+1):
            prio += 1
            st_trigger = trig_id

            if d != half or left>0:
                trig_id += 1
            
            for crt in range(parallel):
                id += 1
                dst = (s+d+crt*half)%groupsize
                out = str(groupsrcs[s]) + "->" + str(groupsrcs[dst]) + " id " + str(id)

                if d == 1:
                    out = out + " start " + str(int(extrastarttime * 1000000))
                else:
                    out = out + " trigger " + str(st_trigger)
                
                out = out + " size " + str(flowsize)
            
                if d != half or left>0:
                    out = out + " send_done_trigger " + str(trig_id)
                out = out + " prio " + str(prio)
                    
                print(out, file=f)
                print(groupsrcs[s], "->", groupsrcs[dst])

        prio += 1
        if left>0:
            st_trigger = trig_id
            
            for crt in range(left):
                id += 1
                dst = (s+parallel*half+crt+1)%groupsize
                out = str(groupsrcs[s]) + "->" + str(groupsrcs[dst]) + " id " + str(id)

                out = out + " trigger " + str(st_trigger)
                out = out + " size " + str(flowsize)
                out = out + " prio " + str(prio)
                print(out, file=f)
                print(groupsrcs[s], "->", groupsrcs[dst])


for t in range(1, trig_id+1):
    out = "trigger id " + str(t) + " multishot"
    print(out, file=f)

f.close()
