#!/bin/sh
../../datacenter/old_tests/htsim_dctcp_incast_collateral_lossless -nodes 432 -conns 64 -q 100
../../parse_output logout.dat -ascii | grep TCP_SINK > dctcpll_output

grep 10134 dctcpll_output > dctcpll_longflow
grep -v 10134 dctcpll_output > dctcpll_incast_raw
python process_dctcp_collateral_lossless.py > dctcpll_incast
gnuplot collateral_dctcp_lossless.plot
