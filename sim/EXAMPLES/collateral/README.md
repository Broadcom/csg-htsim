# NDP Example: Collateral damage from an incast

In this example we examine the impact of an incast traffic pattern on
neighbouring traffic.  We generate a long-lived flow to one host, and then
generate an incast of 64 short flows to the next host on the same top
of rack switch.  All flows start simultaneously, which is
the worst-case example for an incast.  

For NDP, the topology is a 432-node 10Gb/s FatTree, with 8 packet
queues.  The initial window is 15 packets, but the results are
essentially the same for initial windows as low as 10, or as large as
100.  Below 10, the long flow cannot fill the pipe.

We also generate results for DCTCP and for DCTCP with lossless
Ethernet, which approximates the behaviour of DCQCN.  DCTCP requires
much more buffering than NDP to perform well, so the DCTCP experiments
use 100 packet queues.  For regular DCTCP, we use output queuing with
DCTCP-style ECN marking.  

For lossless Ethernet, we implemented two versions of PFC, one that
works on output-buffered switches (and that would block when the
output port goes beyond a threshold) and one where we also track
virtual input buffers, although the switch is also output
buffered. PFC is done in the latter case on the size of the virtual
input buffer, which allows stopping only that port. The former suffers
terrible collateral damage, due to pausing all input ports.  The
latter is the version that most switches use, and this is what we used
too in these experiments.

To approximate DCQCN, we used a variant of DCTCP adapted as per the
DCQCN paper, but it is still window-based and not rate-based; it uses
the DCQCN thresholds for ECN, and it in principle it should behave at
least as well as DCQCN because it converges much faster (in practice
DCQCN NICs only send congestion signals every 50us, which limits the
congestion response drastically in the case of incast because it ends
up triggering PFC despite ECN working just fine and marking
packets. Finally, we do not implement RDMA at all - we simply run TCP
over the whole thing. For a simulation, this should not matter since
both TCP and RDMA should be able to keep the pipe full given that PFC
prevents loss.

## Running the Example

You'll need python and gnuplot.

* To run the example, simply run "./run.sh".
* The flow rate data is initially output to ndp_output, dctcp_output and dctcpll_output, and we then slice this data up to generate the graphs.
* The data is graphed in collateral_ndp.pdf, collateral_dctcp.pdf and collateral_dctcp_lossless.pdf

These three curves are Figure 19 of the NDP Sigcomm'17
paper.  

## Comments

In all three cases, the long flow is running at full rate when the
incast arrives.  NDP copes well with the incast.  During the first
RTT, trimming occurs, both for the long flow and the incast flows.
After the first RTT, the incast receiver is clocking data in at its
line rate, and causes no further congestion for the long flow due to
even packet spraying across all the inbound paths.  The long flow
receives a large number of headers during the first RTT of the incast,
but continues sending pulls, and is back to line rate in the following
RTT.

With DCTCP, the incast is large enough that ECN is not sufficient, and
loss occurs.  Timeouts occur, before the flows recover somewhat later.

With lossless Ethernet, loss is avoided, but there are enough incoming
incast flows on all incoming interfaces of the ToR switch that PFC
repeatedly pauses incoming traffic from the lower-pod switches.  The
aggregate traffic rate to the incast receiver is managed nicely by
this pausing and unpausing, so it achieves good aggregate throughput.
However, the lower-pod switch outgoing interface that the long flow
traverses is paused sufficiently that it achieves low throughput.