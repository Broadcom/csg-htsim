#!/usr/bin/env python

# Generate a ring allreduce traffic matrix.
# python gen_allreduce_butterfly.py <nodes> <conns> <groupsize> <flowsize>
# Parameters:
# <nodes>   number of nodes in the topology
# <conns>    number of active connections
# <groupsize>    number of connections in an all-reduce group
# <flowsize>   size of the flows in bytes
# <extrastarttime>   How long in microseconds to space the start times over (start time will be random in between 0 and this time).  Can be a float.
# <randseed>   Seed for random number generator, or set to 0 for random seed

import os
import sys
from random import seed, shuffle
import math

#print(sys.argv)
if len(sys.argv) != 8:
    print("Usage: python gen_allreduce_butterfly.py <filename> <nodes> <groups> <groupsize> <flowsize> <locality> <randseed>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
groups = int(sys.argv[3])
groupsize = int(sys.argv[4])
flowsize = int(sys.argv[5])
locality = int(sys.argv[6])
randseed = int(sys.argv[7])

conns = groups * groupsize * int(math.log(groupsize,2))

print("Connections: ", conns)
print("All-reduce group size: ", groupsize)
print("Flowsize: ", flowsize, "bytes")
print("Random Seed ", randseed)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections",conns, file=f)
print("Triggers", conns-groupsize*groups, file=f)

srcs = []
dsts = []
trigger_ids = [] 

print("Groups ", groups)

for n in range(nodes):
    srcs.append(n)

if randseed != 0:
    seed(randseed)

id = 0

trig_id = 0
for group in range(groups):
    print("group: ", group)
    groupsrcs = []
    for n in range(groupsize):
        groupsrcs.append(srcs[group * groupsize + n])

    if (locality==1):
        groupsrcs.sort()

    print(groupsrcs)

    print (" ")

    for d in range(0, int(math.log(groupsize,2))):
        step = pow(2,d)
        trigger_ids.append([])
        rng = pow(2,d+1)
        direction = 1
        left_direction = step

        for n in range(nodes):
            trigger_ids[d].append(-1)

        last_step = (d==int(math.log(groupsize,2)-1))
            
        print ("Step",str(step))
        for src in range(0,groupsize):
            if ( int(src/step)%2==0 ):
                dst = src + step

                id += 1

                if (not last_step):
                    trig_id += 1
                    trigger_ids[d][dst] = trig_id
                    
                out = str(groupsrcs[src]) + "->" + str(groupsrcs[dst]) + " id " + str(id)

                if (d==0):
                    rest = " start 0"
                else:
                    rest = " trigger " + str(trigger_ids[d-1][src])

                rest = rest + " size " + str(flowsize)

                if (not last_step):
                     rest  = rest + " recv_done_trigger " + str(trig_id)

                out = out + rest
                print(out, file=f)
                print(out)

                id += 1

                if (not last_step):
                    trig_id += 1
                    trigger_ids[d][src] = trig_id


                if (d==0):
                    rest = " start 0"
                else:
                    rest = " trigger " + str(trigger_ids[d-1][dst])

                rest = rest + " size " + str(flowsize)

                if (not last_step):
                     rest  = rest + " recv_done_trigger " + str(trig_id)                     
                    
                out = str(groupsrcs[dst]) + "->" + str(groupsrcs[src]) + " id " + str(id)
                out = out + rest
                
                print(out, file=f)
                print(out)

                print()
                
            else:
                continue
        
for t in range(1, trig_id):
    out = "trigger id " + str(t) + " oneshot"
    print(out, file=f)

f.close()
