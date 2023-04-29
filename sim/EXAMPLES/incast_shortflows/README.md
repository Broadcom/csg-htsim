# NDP Example: Incast as a Function of Number of Senders

In this example we generate an incast traffic matrix: _n_ hosts send
to one receiver.  We vary the number of senders, _n_.  The file size is
450KB, or 50 9KB packets.  All flows start simultaneously, which is
the worst-case example for an incast.  The topology is a 432-node
10Gb/s FatTree, with 8 packet queues.  The initial window is 23
packets, whcih is excessive for NDP - a smaller window would be
preferred in this case - but 23 packets are required for the full
permutation traffic matrix, so this demonstrates that incast behaviour
is good, even with a suboptimal choice of IW.

## Running the Example

You'll need python and gnuplot.

* To run the example, simply run "./run.sh".
* The data is output to incast_ndp_completion_times_23_450000size_all
* The data is graphed in incast.pdf

This is essentially the NDP curve from Figure 16 of the NDP Sigcomm'17
paper, with slightly reduced variance due to a subsequent
implementation of a fair pull queue.  We only graph the min and max
completion times for each incast, as the two are so close, but the
script calculates and outputs mean and median too.

## Comments

Many packets arrive simultaneously, and even with small incasts, many
packets will be trimmed.  With larger incasts we also see the
behaviour when the header queue overflows - in this simulation,
return-to-sender is enabled.  The behaviour is not greatly different
if these packets are dropped and the sender resends based on timeouts - 
there are enough headers already queued that performance does not
drop significantly if we rely on timeouts rather than
return-to-sender.

To vary the start times, edit main_ndp_incast_shortflows.cpp and change _extrastarttime_.
