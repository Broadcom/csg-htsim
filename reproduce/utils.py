

def find_missing(filename):
    arr = [i for i in range(1, 16001)]
    flow_id_arr = list()
    with open(filename, 'r') as fp:
        for l in fp:
            flow_id = int(l.split(' ')[3])
            flow_id_arr.append(flow_id)
            # print(flow_id)
        for i in range(1, 16001):
            if i not in flow_id_arr:
                print("flow_id {} not finished ".format(i))


find_missing('finished')
