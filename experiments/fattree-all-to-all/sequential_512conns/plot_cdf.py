nodes = 1024
conns = 512
cwnd = 50
filename = "a2a-" + str(nodes) + "-" + str(conns) + "_prio.cm"
file = open(filename, "r")
last_ids = set()
for line in file:
    if "->" in line and "send_done" not in line:
        parts = line.split()
        id = parts[2]
        #print(id)
        last_ids.add(id)
file.close()
    
for strat in ["dnx", "perm", "ecmphost1", "ecmphost100"]:
    filename = "out_" + str(nodes) + "_" + str(conns) + "_" + strat + "_" + str(cwnd) + "iw_prio.tmp"
    file = open(filename, "r")
    fin_times = []
    for line in file:
        if "finished" in line:
            parts = line.split()
            id = parts[3]
            t = parts[6]
            if id in last_ids:
                #print(id, t)
                fin_times.append(t)
    file.close()
    filename = "cdf_" + str(nodes) + "_" + str(conns) + "_" + strat + "_" + str(cwnd) + "iw_prio.tmp"
    ofile = open(filename, "w")
    i = 0
    sz = len(fin_times)
    for t in fin_times:
        print(t, 100*i/sz, file=ofile)
        i += 1
    ofile.close()
