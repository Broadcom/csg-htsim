LIBS=-L.. -lhtsim
INCLUDE= -I../ -I./
LIBDEP=../libhtsim.a
CC=g++
CFLAGS = -Wall -std=c++11 -g -Wsign-compare
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += -O2

all:	htsim_dumbell_ndp htsim_dumbell_ndptunnel htsim_dumbell_tcp htsim_dumbell_swift htsim_multihop_swift htsim_multihop_swift2 htsim_bidir_swift htsim_bidir_ndp htsim_multipath_swift htsim_trigger_test htsim_dumbell_roce htsim_dumbell_hpcc htsim_dumbell_strack

htsim_dumbell_ndp:  main_dumbell_ndp.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_ndp.o -o htsim_dumbell_ndp $(LIBS)

htsim_dumbell_roce:	main_dumbell_roce.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_roce.o -o htsim_dumbell_roce $(LIBS)

htsim_dumbell_hpcc:	main_dumbell_hpcc.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_hpcc.o -o htsim_dumbell_hpcc $(LIBS)

htsim_dumbell_tcp:	main_dumbell_tcp.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_tcp.o -o htsim_dumbell_tcp $(LIBS)

htsim_dumbell_swift:	main_dumbell_swift.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_swift.o -o htsim_dumbell_swift $(LIBS)

htsim_multihop_swift:	main_multihop_swift.o $(LIBDEP)
	$(CC) $(CFLAGS) main_multihop_swift.o -o htsim_multihop_swift $(LIBS)

htsim_multihop_swift2:	main_multihop_swift2.o $(LIBDEP)
	$(CC) $(CFLAGS) main_multihop_swift2.o -o htsim_multihop_swift2 $(LIBS)

htsim_multipath_swift:	main_multipath_swift.o $(LIBDEP)
	$(CC) $(CFLAGS) main_multipath_swift.o -o htsim_multipath_swift $(LIBS)

htsim_bidir_swift:	main_bidir_swift.o $(LIBDEP)
	$(CC) $(CFLAGS) main_bidir_swift.o -o htsim_bidir_swift $(LIBS)

htsim_mpswift:	main_mpswift.o $(LIBDEP)
	$(CC) $(CFLAGS) main_mpswift.o -o htsim_mpswift $(LIBS)

htsim_dumbell_strack:	main_dumbell_strack.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_strack.o -o htsim_dumbell_strack $(LIBS)

htsim_bidir_ndp:	main_bidir_ndp.o $(LIBDEP)
	$(CC) $(CFLAGS) main_bidir_ndp.o -o htsim_bidir_ndp $(LIBS)

htsim_dumbell_ndptunnel:	main_dumbell_ndptunnel.o $(LIBDEP)
	$(CC) $(CFLAGS) main_dumbell_ndptunnel.o -o htsim_dumbell_ndptunnel $(LIBS)

htsim_trigger_test:	main_trigger_test.o $(LIBDEP)
	$(CC) $(CFLAGS) main_trigger_test.o -o htsim_trigger_test $(LIBS)

RUN_TESTS_0 = ./tests.py
RUN_TESTS_1 = ./tests.py -v
V ?= 0

.PHONY: run
run: all
	$(RUN_TESTS_$(V))

.PHONY: update_refs
update_refs: all
	$(RUN_TESTS_$(V)) -u


clean:	
	rm -f *.o htsim_*

.cpp.o:
	source='$<' object='$@' libtool=no depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' $(CXXDEPMODE) $(depcomp) $(CC) $(CFLAGS) $(INCLUDE) -Wuninitialized  -c -o $@ `test -f $< || echo '$(srcdir)/'`$<
