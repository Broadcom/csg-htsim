import csv
import pprint
from plumbum import BG, FG, local

message_size = 2000000

ms_to_str ={2000000: '2M'}
if __name__ == "__main__":
    cfg_list = []
    for node_cnt in [8, 32, 512, 1058, 8192, 16562, 32768]:
        for traffic_pattern in ['gen_bipartite', 'gen_permutation']:
            cfg = dict(
                node_cnt=node_cnt,
                connection_cnt = node_cnt,
                message_size = message_size,
                traffic_pattern = traffic_pattern

            )
            cfg_list.append(cfg)
    
    print(f"Generated {len(cfg_list)} configurations.")
    keypress = input("Enter 'Yes' to continue:")
    if keypress != 'Yes':
        print("Aborting.")
        exit(1)

    trace_files = []
    for cfg in cfg_list:
        filename = f"{cfg['traffic_pattern']}_{cfg['node_cnt']}n_{cfg['connection_cnt']}c_{ms_to_str[cfg['message_size']]}s.tm"

        print(f"Filename {filename}")
        cmd = local['python3'][f"{cfg['traffic_pattern']}.py",
                            f"{filename}",
                            f"{cfg['node_cnt']}",
                            f"{cfg['connection_cnt']}",
                            f"{cfg['message_size']}", 
                            "0",
                            "0"]
        print(f"Running generator: '{cmd}'")
        print(cmd())

#        trace = []
#        with open(filename, 'r') as trace_file:
#            trace_reader = csv.reader(trace_file, delimiter=' ')
#            for row in trace_reader:
#                trace.append(row)
#
#        with open(filename, 'w+') as trace_file:
#            trace = sorted(trace, key=lambda r: int(r[5]))
#
#            trace_writer = csv.writer(trace_file, delimiter=' ')
#            trace_writer.writerow([len(trace)])
#            trace_writer.writerows(trace)
#
#        trace_files.append(filename)
#
#    print(f"Created {len(trace_files)} trace files:")
#    print("\n".join(trace_files))


