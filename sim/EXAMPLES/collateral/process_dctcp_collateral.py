#!python

from __future__ import print_function
import subprocess
import sys

ifile = open("dctcp_incast_raw", "r")

prevtime = 0
totalrate = 0
for line in ifile:
    data = line.split()
    time = float(data[0])
    ev = data[2];
    if ev == "TCP_SINK":
        r = int(data[10])
        #print(time, r)
        if time != prevtime:
            print(prevtime, totalrate)
            totalrate = 0
            prevtime = time
        totalrate += r
print(prevtime, totalrate)

ifile.close()
