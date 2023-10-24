#!/usr/bin/env python

# Generate an incast traffic matrix.
# python gen_incast.py <nodes> <conns> <flowsize> <extrastarttime> <randseed>
# Parameters:
# <nodes>   number of nodes in the topology
# <conns>    number of active connections
# <flowsize>   size of the flows in bytes
# <extrastarttime>   How long in microseconds to space the start times over (start time will be random in between 0 and this time).  Can be a float.
# <randseed>   Seed for random number generator, or set to 0 for random seed

import os
import sys
from random import seed, shuffle, randint
#print(sys.argv)
if len(sys.argv) != 7:
    print("Usage: python gen_incast.py <filename> <nodes> <conns> <flowsize> <extrastarttime> <randseed>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
conns = int(sys.argv[3])
flowsize = int(sys.argv[4])
extrastarttime = float(sys.argv[5])
randseed = int(sys.argv[6])

print("Nodes: ", nodes)
print("Connections: ", conns)
print("Flowsize: ", flowsize, "bytes")
print("ExtraStartTime: ", extrastarttime, "us")
print("Random Seed ", randseed)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections", conns, file=f)

srcs = []
for n in range(1,nodes):
    srcs.append(n)
if randseed != 0:
    seed(randseed)
shuffle(srcs)

dst = "0"

for n in range(conns):
    extra = randint(0,int(extrastarttime * 1000000))
    out = str(srcs[n]) + "->" + str(dst) + " id " + str(n+1) + " start " + str(extra) + " size " + str(flowsize)
    print(out, file=f)

f.close()
