#!/opt/local/bin/python

from __future__ import print_function
import subprocess
import sys

filename = sys.argv[1]
conns = sys.argv[2]
filesize = sys.argv[3]
cwnd = sys.argv[4]
tag = sys.argv[5]
print("tag:", tag)
subprocess.call("../../parse_output " + filename + " -ascii | grep RCV | grep FULL > " + filename+".asc", shell=True)

ifile = open(filename+".asc", "r")
flowtimes = {}
for line in ifile:
    data = line.split()
    time = float(data[0])
    ev = data[2];
    if len(data) >= 15:
        flag = data[14]
    else:
        flag = "none"
    flowid = data[8]
    if (ev == "NDPTRAFFIC" or ev == "NDPLITETRAFFIC") and flag == "LASTDATA":
        #print(ev, flowid, flag)
        flowtimes[flowid] = time;
    elif ev == "NDPTRAFFIC" or ev == "NDPLITETRAFFIC":
        if flowid in flowtimes:
            #print("update: ", ev, flowid, flag)
            flowtimes[flowid] = time;
#print(flowtimes)
#print("flowtimes");        
lasttimes = []
for id in flowtimes:
    #print(id)
    lasttimes.append(flowtimes[id]);
#print("done")
lasttimes.sort()
numflows = len(lasttimes)
print(lasttimes)
ifile.close()
ofile = open(filename+".last", "w+")
for t in lasttimes:
    print(t, file=ofile);
ofile.close()
namebase = "incast_" + tag + "_completion_times_" + str(cwnd) + "_" + str(filesize) + "size_"
ofile = open(namebase + "median", "a+")
print(conns, lasttimes[numflows//2], file=ofile);
print(conns, lasttimes[numflows//2]);
ofile.close()
ofile = open(namebase + "max", "a+")
print(conns, lasttimes[numflows-1], file=ofile);
print(conns, lasttimes[numflows-1]);
ofile.close()
ofile = open(namebase + "min", "a+")
print(conns, lasttimes[0], file=ofile);
print(conns, lasttimes[0]);
ofile.close()
ofile = open(namebase + "mean", "a+")
total = 0
for i in lasttimes:
    total += i
print(conns, total/numflows, file=ofile);
print(conns, total/numflows);
ofile.close()

    
