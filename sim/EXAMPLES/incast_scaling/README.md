# NDP Example: Incast Scaling

In this example we generate an incast traffic matrix: _n_ hosts send
to one receiver.  We vary the number of senders, _n_, from 1 to 8000.
We're looking at how incast scales to very large numbers of
senders. The default transfer size used is 270000 bytes, or 30 9KB
packets.  The script generates two graphs - one that examines latency
and one that examines overhead at the senders.

*incast_sensitivity.pdf* displays how flow completion of the last flow
to finish varies as a function of incast size.  If plotted raw, this
appears to be a straight line, so we instead plot flow completion time
normalized by optimal flow completion time, giving the time overhead
as a percentage.  Thus a 1% overhead indicates that the flow
completion time of the last flow is 1% more than an optimal protocol
would achieve.

*incast_overhead.pdf* displays the number of retransmissions during
each scenario, listed by the triggering mechanisms - either a bounce
(return-to-sender packet) or a NACK in response to trimming.

The example also looks at the effects of the NDP initial window -
obviously the more packets sent in the initial window, the more loss
will occur.  The script examines values of 1 (the smallest possible),
10 (close to the smallest possible where a single flow can achieve
minimal completion time), and 23 (indicated as being needed to
completely saturate the network with a permutation traffic matrix).

The script runs 582 separate incast experiments, so expect it to take
most of a day to run.  

## Running the Example

You'll need python and gnuplot.

* To run the example, simply run "./run.sh", and come back the next day.
* The flow completion time data is output to
  incast_ndp_completion_times_1_270000size_max,
  incast_ndp_completion_times_10_270000size_max, and
  incast_ndp_completion_times_23_270000size_max depending on the
  initial window.  Other files are also generated that include min,
  mean, and median figures.
* The overhead data is output to bounces1, bounces10 and bounces23 depending on the initial window.
* The data is graphed in incast_sensitivity.pdf and incast_overhead.pdf

These graphs are Figure 20a and Figure 20b from the NDP Sigcomm'17
paper.  They will differ is some details from the version in the paper
due to different seeding of the random number generator, as each data
point comes from a single simulation run, but should be essentially
the same.

## Comments

Many packets arrive simultaneously, and even with small incasts, many
packets will be trimmed.  With larger incasts we also see the
behaviour when the header queue overflows - in this simulation,
return-to-sender is enabled.  

The small latency overhead with IW=10 and IW=23 is due to headers
consuming some bandwidth on the receiver's link.  As the incast
scales, the header queue overflows, and a smaller fraction of the sent
packets end up as headers (headers are still useful though, because
they allow the receiver to keep the pipe busy during the initial
incast phase).  With really large incasts, almost all packets are
bounced back to the sender.  Although we swallow all but one of these,
we do immediately retransmit one, if no packets from the initial
window get through.  This causes a small echo of the initial incast.
When incasts are larger than 2000 senders, this echo is not completely
negligible, and some packets are bounced more than once.  It would
almost certainly be possible to optimize this behaviour, adding
randomization to the retransmission times, but we have not done so in
this experiment in the interests of simplicity. 

If an application knows it will create an 8000:1 incast, it makes
little sense to choose a 23-packet initial window.  Such applications
should default to a one-packet IW.  However, if the incast is smaller
that 8 senders, choosing an IW of 1 packet does not allow the
aggregate rate to fill the receiver's link, so completion time will be
signifcantly larger than optimal.  In between these two extremes, NDP
is not greatly sensitive to IW choice.