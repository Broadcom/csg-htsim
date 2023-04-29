#!python

from __future__ import print_function
import subprocess
import sys

filename = sys.argv[1]
subprocess.call("../../parse_output " + filename + " -ascii > " + filename+".asc", shell=True)

ifile = open(filename+".asc", "r")
rates = []
for line in ifile:
    data = line.split()
    time = float(data[0])
    ev = data[2];
    if time == 0.2 and ev == "NDP_SINK":
        if data[9] == "Rate":
            rate = int(data[10])
            rate = rate * 8 /1000000000.0;
            rates.append(rate)
            print(rate)
ifile.close()

ofile = open(filename+".urates", "w+")
for r in rates:
    print(r, file=ofile);
ofile.close()

rates.sort()
print(rates)
ofile = open(filename+".rates", "w+")
for r in rates:
    print(r, file=ofile);
ofile.close()
    
