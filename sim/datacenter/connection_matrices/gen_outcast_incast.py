#!/usr/bin/env python

# Generate an outcast incast traffic matrix.
# python gen_outcast_incast.py <nodes> <conns1> <conns2> <flowsize> <randseed>
# Parameters:
# <nodes>   number of nodes in the topology
# <conns1>    number of active connections to the incast destination
# <conns2>    number of active connections from the outcast sources
# <flowsize>   size of the flows in bytes
# <randseed>   Seed for random number generator, or set to 0 for random seed

import os
import sys
from random import seed, shuffle
#print(sys.argv)
if len(sys.argv) != 7:
    print("Usage: python gen_outcast_incast.py <filename> <nodes> <conns_incast> <conns_outcast> <flowsize> <randseed>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
conns1 = int(sys.argv[3])
conns2 = int(sys.argv[4])
flowsize = int(sys.argv[5])
randseed = int(sys.argv[6])

print("Nodes: ", nodes)
print("Connections incast: ", conns1, "outcast:",conns2)
print("Flowsize: ", flowsize, "bytes")
print("Random Seed ", randseed)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections", conns1+(conns1-1)*(conns2-1), file=f)

if (((conns1-1)*conns2+1+conns1)>=nodes):
    print ("Too many connections for target topology")
    exit (1)

crttarget = conns1+1
id = 1

for n in range(conns1):
    out = str(n+1) + "->" + str(0) + " id " + str(id) + " start " + str(0) + " size " + str(flowsize)
    print(out, file=f)
    id = id+1

    if (n!=0):
        for m in range(conns2-1):
            out = str(n+1) + "->" + str(crttarget) + " id " + str(id) + " start " + str(0) + " size " + str(flowsize)
            crttarget = crttarget+1
            id = id+1
            print(out, file=f)

f.close()
