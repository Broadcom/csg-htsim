import sys
# nodes = 1024
# conns = 512
# par = 1
# cwnd = 50
nodes = sys.argv[1]
conns = sys.argv[2]
par = sys.argv[3]
cwnd = sys.argv[4]
filename = "a2a-" + str(nodes) + "-" + str(conns) + "-" + str(par) + ".cm"
file = open(filename, "r")
triggers = {} # map of trigger id to flow id
starts = {} # map of flow id to trigger
srcs = {} # flow id to src
dsts = {} # flow id to dst
dstcounts = {} # dsts to refcount
srclist = []
srcset = set()
for line in file:
    if "->" in line:
        parts = line.split()
        p2 = parts[0].split("-")
        src = p2[0]
        dst = p2[1][1:]
        # print(src, "->", dst)
        assert (parts[1] == "id")
        id = parts[2]
        srcs[id] = src
        dsts[id] = dst
        dstcounts[dst] = 0
        if src not in srcset:
            srclist.append(src)
            srcset.add(src)
        if parts[3] == "trigger":
            trigger = parts[4]
            #print(id, "starts from", trigger)
            triggers[trigger] = id
        if "send_done" in line:
            assert (parts[7] == "send_done_trigger")
            sdt = parts[8]
            #print(id, "causes", sdt)
            starts[id] = sdt
            
file.close()
#for id in starts:
#    print(id, triggers[starts[id]])

#for strat in ["dnx", "perm", "ecmphost1", "ecmphost100"]:
for strat in ["perm"]:
    filename = "out_" + str(nodes) + "_" + str(conns) + "_" + strat + "_" + str(cwnd) + "iw_1par.tmp"
    file = open(filename, "r")
    filename = "incast_" + str(nodes) + "_" + str(conns) + "_" + strat + "_" + str(cwnd) + "iw_1par.tmp"
    ofile = open(filename, "w")
    fin_times = []
    prevtime = -4000
    count = 0
    for line in file:
        if "startflow" in line:
            #print(line)
            parts = line.split()
            flowid = parts[1]
            p2 = flowid.split('_')
            assert(p2[0] == "ndp")
            dst = p2[2]
            t = float(parts[7])
            dstcounts[dst] += 1
        if "finished" in line:
            #print(line)
            parts = line.split()
            id = parts[3]
            t = float(parts[6])
            #print(t, "id", id, "finished")
            dst = dsts[id]
            dstcounts[dst] -= 1
            # if id in starts:
            #     newid = triggers[starts[id]]
            #     newdst = dsts[newid]
            #     dstcounts[newdst] += 1
            #     #print(t, "id", newid, "started, dstcount=", dstcounts[newdst])
            #if t - prevtime > 0:
            count += 1
            #if count > 1024:
            if t - prevtime > 4000:
                count = 0
                prevtime = t
                j = 0
                for n in srclist:
                    j += 1
                    print(t, j, dstcounts[n], file=ofile)
            #print("", file=ofile)
    file.close()
    ofile.close()
