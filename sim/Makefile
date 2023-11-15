SUBDIRS=tests datacenter
OBJS=eventlist.o tcppacket.o pipe.o queue.o meter.o queue_lossless.o queue_lossless_input.o queue_lossless_output.o ecnqueue.o tcp.o dctcp.o mtcp.o loggers.o logfile.o clock.o config.o network.o qcn.o exoqueue.o randomqueue.o cbr.o cbrpacket.o sent_packets.o ndp.o ndptunnel.o ndppacket.o roce.o rocepacket.o eth_pause_packet.o tcp_transfer.o tcp_periodic.o compositequeue.o prioqueue.o cpqueue.o ndp_transfer.o compositeprioqueue.o switch.o dctcp_transfer.o fairpullqueue.o route.o callback_pipe.o ndptunnelpacket.o swiftpacket.o swift.o swift_scheduler.o routetable.o trigger.o hpccpacket.o hpcc.o strackpacket.o strack.o priopullqueue.o rng.o ecnprioqueue.o eqdspacket.o eqds.o eqds_logger.o aeolusqueue.o
HDRS=network.h ndp.h ndptunnel.h queue_lossless.h queue_lossless_input.h queue_lossless_output.h compositequeue.h prioqueue.h cpqueue.h queue.h loggers.h loggertypes.h pipe.h eventlist.h config.h tcp.h dctcp.h mtcp.h sent_packets.h tcppacket.h ndppacket.h rocepacket.h eth_pause_packet.h ndp_transfer.h compositeprioqueue.h ecnqueue.h switch.h dctcp_transfer.h callback_pipe.h meter.h ndptunnelpacket.h swiftpacket.h swift.h swift_scheduler.h routetable.h circular_buffer.h trigger.h hpccpacket.h hpcc.h strackpacket.h strack.h priopullqueue.h ecnprioqueue.h eqdspacket.h eqds.h eqds_logger.h aeolusqueue.h

CC=g++
CFLAGS = -Wall -std=c++11 -g -Wsign-compare -Wuninitialized -fPIE
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += -O3

all:	libhtsim.a parse_output $(SUBDIRS)

$(SUBDIRS):	libhtsim.a
	$(MAKE) -C $@

.PHONY: all $(SUBDIRS)

libhtsim.a:	$(OBJS) $(HDRS)
	ar -rvu libhtsim.a $(OBJS)

parse_output: parse_output.o $(OBJS)
	$(CC) $(CFLAGS) parse_output.o libhtsim.a -o parse_output 

htsim:	$(OBJS) main.o $(HDRS)
	$(CC) $(CFLAGS) $(OBJS) main.o -o htsim

htsim_dumbell_roce:	$(OBJS) main_dumbell_roce.o
	$(CC) $(CFLAGS) $(OBJS) main_dumbell_roce.o -o htsim_dumbell_roce

htsim_dumbell_hpcc:	$(OBJS) main_dumbell_hpcc.o
	$(CC) $(CFLAGS) $(OBJS) main_dumbell_hpcc.o -o htsim_dumbell_hpcc

htsim_dumbell_tcp:	$(OBJS) main_dumbell_tcp.o
	$(CC) $(CFLAGS) $(OBJS) main_dumbell_tcp.o -o htsim_dumbell_tcp

htsim_dumbell_swift:	$(OBJS) main_dumbell_swift.o
	$(CC) $(CFLAGS) $(OBJS) main_dumbell_swift.o -o htsim_dumbell_swift

htsim_multihop_swift:	$(OBJS) main_multihop_swift.o
	$(CC) $(CFLAGS) $(OBJS) main_multihop_swift.o -o htsim_multihop_swift

htsim_multihop_swift2:	$(OBJS) main_multihop_swift2.o
	$(CC) $(CFLAGS) $(OBJS) main_multihop_swift2.o -o htsim_multihop_swift2

htsim_multipath_swift:	$(OBJS) main_multipath_swift.o
	$(CC) $(CFLAGS) $(OBJS) main_multipath_swift.o -o htsim_multipath_swift

htsim_bidir_swift:	$(OBJS) main_bidir_swift.o
	$(CC) $(CFLAGS) $(OBJS) main_bidir_swift.o -o htsim_bidir_swift

htsim_mpswift:	$(OBJS) main_mpswift.o
	$(CC) $(CFLAGS) $(OBJS) main_mpswift.o -o htsim_mpswift

htsim_dumbell_strack:	$(OBJS) main_dumbell_strack.o
	$(CC) $(CFLAGS) $(OBJS) main_dumbell_strack.o -o htsim_dumbell_strack

htsim_bidir_ndp:	$(OBJS) main_bidir_ndp.o
	$(CC) $(CFLAGS) $(OBJS) main_bidir_ndp.o -o htsim_bidir_ndp

htsim_dumbell_ndptunnel:	$(OBJS) main_dumbell_ndptunnel.o
	$(CC) $(CFLAGS) $(OBJS) main_dumbell_ndptunnel.o -o htsim_dumbell_ndptunnel


clean:	
	rm -f *.o htsim htsim_* libhtsim.a parse_output datacenter/*.o datacenter/htsim_* tests/*.o tests/htsim_*
parse_output.o: parse_output.cpp libhtsim.a
config.o:	config.cpp config.h
switch.o: 	switch.cpp switch.h drawable.h
tofino.o: tofino.cpp tofino.h
eventlist.o:    eventlist.cpp eventlist.h config.h
main.o:		main.cpp $(HDRS)
main_dumbell_ndp.o:		main_dumbell_ndp.cpp $(HDRS)
sent_packets.o:		sent_packets.h sent_packets.cpp
queue.o:	queue.cpp  $(HDRS)
meter.o:	meter.cpp  $(HDRS)
queue_lossless.o:	queue_lossless.cpp  $(HDRS)
queue_lossless_input.o:	queue_lossless_input.cpp  $(HDRS)
queue_lossless_output.o:	queue_lossless_output.cpp  $(HDRS)
ecnqueue.o:	ecnqueue.cpp  $(HDRS)
exoqueue.o:	exoqueue.cpp $(HDRS)
pipe.o:		pipe.cpp $(HDRS)
callback_pipe.o:		callback_pipe.cpp $(HDRS)
network.o:	network.cpp  $(HDRS)
fairpullqueue.o:	fairpullqueue.cpp  $(HDRS)
priopullqueue.o:	priopullqueue.cpp  $(HDRS)
route.o:	route.cpp  $(HDRS)
routetable.o:	routetable.cpp  $(HDRS)
tcp.o:		tcp.cpp  $(HDRS)
dctcp.o:		dctcp.cpp  $(HDRS)
ndp.o:		ndp.cpp $(HDRS)
ndplite.o:	ndplite.cpp $(HDRS)
mtcp.o:		mtcp.cpp $(HDRS)
swift.o:	swift.cpp $(HDRS)
strack.o:	strack.cpp $(HDRS)
tcppacket.o:	tcppacket.cpp $(HDRS)
swiftpacket.o:	swiftpacket.cpp $(HDRS)
strackpacket.o:	strackpacket.cpp $(HDRS)
loggers.o:	loggers.cpp $(HDRS)
logfile.o:	logfile.cpp  $(HDRS)
trigger.o:	trigger.cpp  $(HDRS)
hpccpacket.o:	hpccpacket.cpp  $(HDRS)
clock.o:	clock.cpp clock.h eventlist.h config.h
compositequeue.o: compositequeue.cpp $(HDRS)
eqds.o:         eqds.cpp $(HDRS)
eqds_logger.o:	eqds_logger.cpp $(HDRS)
faircompositequeue.o: faircompositequeue.cpp $(HDRS)
prioqueue.o: prioqueue.cpp $(HDRS)
ecnprioqueue.o: ecnprioqueue.cpp $(HDRS)
cpqueue.o: cpqueue.cpp $(HDRS)
compositeprioqueue.o: compositeprioqueue.cpp $(HDRS)
ndp_transfer.o: ndp_transfer.cpp $(HDRS)
roce.o: roce.cpp $(HDRS)
hpcc.o: hpcc.cpp $(HDRS)
qcn.o: qcn.cpp qcn.h loggers.h config.h 
aeolusqueue.o: aeolusqueue.cpp $(HDRS)

.cpp.o:
	source='$<' object='$@' libtool=no depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' $(CXXDEPMODE) $(depcomp) $(CC) $(CFLAGS)  -c -o $@ `test -f $< || echo '$(srcdir)/'`$<
