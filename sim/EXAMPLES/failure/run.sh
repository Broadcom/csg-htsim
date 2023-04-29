#!/bin/sh
../../datacenter/htsim_ndp_permutation_fail -o ndp_perm -strat perm -conns 128 -nodes 128 -cwnd 23 -q 8 > debugout_ndp_perm
python process_data.py ndp_perm
../../datacenter/htsim_ndp_permutation_fail -o ndp_penalty -strat pull -conns 128 -nodes 128 -cwnd 23 -q 8 > debugout_ndp_penalty
python process_data.py ndp_penalty
../../datacenter/htsim_ndp_permutation_fail -o ndp_penalty -strat pull -conns 128 -nodes 128 -cwnd 23 -q 8 > debugout_ndp_penalty
python process_data.py ndp_penalty
../../datacenter/htsim_dctcp_permutation -o dctcp -nodes 128 -conns 128 -ssthresh 15 -q 100 -fail 1 > debug_dctcp
python process_dctcp_data.py dctcp
../../datacenter/htsim_tcp_permutation -o mptcp -nodes 128 -conns 128 -ssthresh 15 -q 100 -fail 1 -sub 8 > debug_mptcp
python process_mptcp_data.py mptcp
gnuplot failure.plot
