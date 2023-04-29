#!/bin/sh
../../datacenter/htsim_ndp_in_out -o logfile -conns 4 -nodes 128 -cwnd 23 -strat perm -q 8 > ts_in_out
python process_data.py logfile