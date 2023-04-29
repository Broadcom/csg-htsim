#!/bin/sh
../../datacenter/old_tests/htsim_dctcp_incast_collateral -nodes 432 -conns 64 -q 100
../../parse_output logout.dat -ascii | grep TCP_SINK > dctcp_output
grep 5742 dctcp_output > dctcp_longflow
grep -v 5742 dctcp_output > dctcp_incast_raw
python process_dctcp_collateral.py > dctcp_incast
gnuplot collateral_dctcp.plot
