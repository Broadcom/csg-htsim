# NDP Example: Permutation Traffic Matrix with Failure.

In this example we generate a full permutation traffic matrix, as in
EXAMPLES/permutation - every host sends to one other host and every
host receives from one other host - except that we fail one of the
links between an upper-pod switch and a core switch from 10Gb/s to
1Gb/s.  This sort of failure is typically more problematic than a hard
failure, because that can be detected by routing and avoided.  The
permutation traffic matrix loads the core to capacity, so the failed
core link will be heavily overloaded, resulting in high congestion for
flows that traverse this link, and high loss for a protocol such as
NDP that sprays packets evenly across all paths.

## Running the Example

You'll need python and gnuplot.

* To run the example, simply run "./run.sh".
* The basic NDP results data is in ndp_perm.rates.
* The NDP results data when dynamic path penalties are enabled is in ndp_penalty.rates.
* The DCTCP results data is in dctcp.rates.
* The data is graphed in permutation.pdf

This is essentially the NDP curve from Figure 22 of the NDP Sigcomm'17
paper.  In Figure 22, an NDP initial window of 15 is used, whereas
here we use an initial window of 23, as with the EXAMPLES/permutation.
This slightly reduces the effect of the failure on NDP without path
penalties, as compared to Figure 22.

## Comments

In EXAMPLES/permutation we use a 432-node FatTree.  In this case we
use a 128-node FatTree, as this means more flows traverse the failed
link, making any issues with NDP easier to see.  The behaviour on a
larger FatTree is similar, but a smaller fraction of flows are
affected.  

NDP flows to and from the nodes on the affected pod suffer high
trimming rates.  This, by itself, is not actually a problem - the
packets are detected as lost (trimmed), and retransmitted on other
paths avoiding the failed link.  However, NDP essentially operates
with a fixed receiver-oriented window of data packets and pulls.  The
failed link is very slow, and its priority queue fills up with headers
and pull packets.  Essentially we "store" a lot of headers and pull
packets in this link, reducing the number of pulls that can do useful
work, and so impacting performance.

A naive solution that works fairly well is simply to increase the
initial window.  However, the failed link also impacts latency, so a better
solution is desirable.  NDP implements a path penalty scheme,
that keeps track of the fraction of traffic sent on each path that
results in a NACK.  As it sprays evenly across all paths, this
fraction should be approximately equal across all paths.  If it
substantially worse on a few paths, NDP penalizes those paths,
removing them from use for that flow for a while.

For NDP, we use an 8 packet data queue size.  We also generate results
for DCTCP, using a 100 packet queue size.  DCTCP performs poorly on
the permutation traffic matrix, even without failures, because it only
uses a single path and suffers from flow collisions.  The failure only
slightly worsens DCTCP's behaviour.  Increasing the queue size further
does not improve DCTCP performance.
