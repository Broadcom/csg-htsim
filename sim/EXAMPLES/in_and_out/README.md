# NDP Example: Simultaneous outcast and incast

In this example we generate simultaneous inbound and outbound traffic
to examine how NDP shares bandwidth.  The topology is a FatTree, but
the core is lightly loaded, so the precise topology is unimportant.
Five hosts are involved: Host A sends to hosts B, C, D and E.
Simultaneously, host F sends to host E.  The purpose of the experiment
is to verify how host F's traffic affects host A's traffic. The
initial window of all flows is 23 packets and switch queues are 8
packets.  

![Experiment Topology](topology.png)

## Running the Example

You'll need python and gnuplot.

* To run the example, simply run "./run.sh".
* The results data is in logfile.rates.

This is the experiment from Figure 21 of the NDP Sigcomm'17
paper.

## Comments

The data rates achieved are:

* A->B: 2.50992
* A->C: 2.51352
* A->D: 2.50056
* A->E: 2.37672
* F->E: 7.55208

Traffic from A to B, C, D an E will start out fair, simply because the
initial windows are the same.  Each receiver will get less than its
link speed in incoming packets, so all incoming packets elicit
immediate pulls for more data.

The link to E is initially overloaded, resulting in trimming.  As a
result, a pull queue builds at E.  Roughly four times more pull
packets from F than from A are queued at E.  The pull queue uses a
round-robin fair queue by default, so pulls are preferentially sent to
A, with the remainder sent to F.

The end result is roughly the same pull rate at A from all four
receivers, and the remaining bandwidth on E's link is filled with
traffic from F.  In the end, both A's outgoing link and E's incoming
link are filled.

The results are insensitive to the choice of initial window - initial
windows as low as 6 give essentially the same results.  Below 6, the
F->E flow cannot fill the pipe.
