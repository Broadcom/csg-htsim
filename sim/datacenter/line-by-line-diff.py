import sys

f0 = sys.argv[1]
f1 = sys.argv[2]


def print_diff(dict0, dict1):
    for key in dict0:
        if (dict0[key] != dict1[key]):
            print(key, dict0[key], dict1[key])


file0 = open(f0, "r")
file1 = open(f1, "r")

count = 0
for l0 in file0:
    l1 = file1.readline()
    # print (l0, l1)
    if "outgoingPort" in l0 or "outgoingPort" in l1:
        s0 = ''.join(l0.split(" ")[:12])
        s1 = ''.join(l1.split(" ")[:12])

    if "receiveDataPkt" in l0 or "receiveDataPkt" in l1:
        s0 = ''.join(l0.split(" ")[2:6])
        s1 = ''.join(l1.split(" ")[2:6])
    if "sendDataPkt" in l0 or "sendDataPkt" in l1:
        s0 = ''.join(l0.split(" ")[1:6])
        s1 = ''.join(l1.split(" ")[1:6])            
    else:
        s0 = l0
        s1 = l1 
    if s0 != s1:
        print(s0, s1)
        print(l0, l1)
        print(count)
        break
    count = count + 1                




files_dict = dict()
for filename in [f0, f1]:
    file = open(filename, "r")
    file0_dict = dict()
    for l0 in file:
        if "outgoingPort" not in l0:
            continue
        words = l0.split(" ")
        switch_id = words[1]
        outport = words[3]
        size = words[5]
        if switch_id not in file0_dict:
            file0_dict[switch_id] = dict()
        if outport not in file0_dict[switch_id]:
            file0_dict[switch_id][outport] = dict()
        if size not in file0_dict[switch_id][outport]:
            file0_dict[switch_id][outport][size] = 0

        file0_dict[switch_id][outport][size] = file0_dict[switch_id][outport][size] + 1
    files_dict[filename] = file0_dict

# for filename in files_dict:
    # print(filenam)
filename = f0
for switch in files_dict[filename]:
    for outport in files_dict[filename][switch]:
        print(filename, switch, outport, files_dict[filename][switch][outport])
        print(f1, switch, outport, files_dict[f1][switch][outport])
        # if files_dict[filename][switch][outport] != files_dict[f1][switch][outport]:
        #     print("The two simulators does not generate the same hash\n")
        #     sys.exit(-1)


files_dict = dict()
for filename in [f0, f1]:
    file = open(filename, "r")
    file0_dict = dict()
    for l0 in file:
        if "set_ecn" not in l0 and "queue_ecn " not in l0:
            continue
        words = l0.split(" ")
        pathid = words[2]
        src = words[4]
        dst = words[6]
        if src not in file0_dict:
            file0_dict[src] = dict()
        if dst not in file0_dict[src]:
            file0_dict[src][dst] = dict()
        if pathid not in file0_dict[src][dst]:
            file0_dict[src][dst][pathid] = 0

        file0_dict[src][dst][pathid] = file0_dict[src][dst][pathid] + 1
    files_dict[filename] = file0_dict

filename = f0
for src in files_dict[filename]:
    for dst in files_dict[filename][src]:

        if files_dict[filename][src][dst] != files_dict[f1][src][dst]:
            # print(filename, src, dst, files_dict[filename][src][dst])
            # print(f1, src, dst, files_dict[f1][src][dst])
            print("The two simulators does not generate the same hash\n")
            print(src, dst)
            print_diff(files_dict[filename][src][dst], files_dict[f1][src][dst])

            # sys.exit(-1)