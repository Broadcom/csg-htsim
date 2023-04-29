# NDP Example: Permutation Traffic Matrix

In this example we generate a full permutation traffic matrix: every
host sends to one other host and every host receives from one other
host.  This example uses the full cross-sectional bandwidth of the
FatTree topology, and also illustrates each host simultaneously
sending and receiving data.  It's not intended to be a realistic
example, but illustrates that the network can be run at nearly 100%
utilization without significant unfairness, even though some hosts
send to others that are more local than others within the topology.

We run both NDP and DCTCP for comparison.

## Running the Example

You'll need python and gnuplot.

* To run the example, simply run "./run.sh".
* The NDP results data is in logfile.rates.
* The DCTCP results data is in dctcp_logfile.rates.
* The data is graphed in permutation.pdf

This is essentially the NDP curve from Figure 14 of the NDP Sigcomm'17
paper.

## Comments

We're simulating 432 nodes sending at nearly 10Gb/s, so the aggregate
load is around 4Tb/s.  Even with 9KB packets, it takes a few minutes
to run.  With NDP, queue sizes are 8 packets, and we use a 23 packet initial
window.  23 packets is quite a large IW for NDP, but with full
bidirectional traffic, we get small queues all over, and even PULLs
will see some queuing.  NDP isn't very sensitive to IW choice, but in
this case, smaller IW values don't have enough packets in flight to
quite fill the network.  Under realistic workloads, you would never need an
IW this large.

DCTCP performs poorly on the permutation traffic matrix in a FatTree
topology, because it only uses a single path for each flow, routed
using a random path using ECMP, and suffers from flow collisions as a
result.  DCTCP also needs substantially more queuing than NDP - we use
100 packet queues here, to allow it to perform as well as it can.
Increasing above 100 does not further improve performance.  In
general, single-path schemes are unlikely to be robust at higher
utilizations.