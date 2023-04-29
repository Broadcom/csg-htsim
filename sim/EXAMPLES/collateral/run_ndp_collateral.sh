#!/bin/sh
../../datacenter/old_tests/htsim_ndp_incast_collateral -strat perm -nodes 432 -conns 64 -q 8 -cwnd 15
../../parse_output logout.dat -ascii | grep NDP_SINK > ndp_output

# 5744 is sensitive to build changes - just find the first ID in ndp_output that is non-zero
grep 5744 ndp_output > ndp_longflow
grep -v 5744 ndp_output > ndp_incast_raw
python process_ndp_collateral.py > ndp_incast
gnuplot collateral_ndp.plot
