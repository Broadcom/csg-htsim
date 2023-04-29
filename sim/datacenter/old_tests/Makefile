CC = g++
CFLAGS = -Wall -std=c++11 -g -Wsign-compare 
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += -O2
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) -I$(CRT)/../..
LIB=-L.. -L../..
LIBDEP=../../libhtsim.a

all:	htsim_ndp_alltoall htsim_ndp_permutation htsim_ndptunnel_permutation htsim_ndp_permutation_tor htsim_ndp_random htsim_ndp_permutation_lossless htsim_ndp_permutation_fail htsim_ndp_incast htsim_ndp_outcast htsim_ndprts_outcast htsim_ndprts_outcast_incast htsim_ndp_outcast_incast htsim_ndprts_alltoall htsim_ndp_incast_tor htsim_ndp_incast_shortflows htsim_ndp_incast_collateral htsim_ndp_outcast htsim_ndp_outcast_shortflows htsim_tcp_permutation htsim_ndptunnel_incast_shortflows htsim_tcp_perm_shortflows htsim_ndp_oversubscribed htsim_ndp_in_out htsim_ndp_random_shortflows_tor htsim_swift_incast htsim_swift_incast2 htsim_swift_incast2b htsim_ndp_incast2 htsim_ndp_incast2b htsim_swift_incast3 htsim_tcp_dragonfly_permutation htsim_ndp_dragonfly_permutation htsim_dctcp_permutation htsim_dctcp_incast_collateral htsim_dctcp_incast_collateral_lossless


htsim_tcp_permutation: main_tcp_permutation.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_tcp_permutation.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_tcp_permutation

htsim_tcp_dragonfly_permutation: ../dragon_fly_topology.o main_tcp_dragonfly_permutation.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) main_tcp_dragonfly_permutation.o ../dragon_fly_topology.o ../connection_matrix.o ../shortflows.o $(LIB) -lhtsim -o htsim_tcp_dragonfly_permutation

htsim_dctcp_permutation: main_dctcp_permutation.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_permutation.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_permutation

htsim_dctcp_random_shortflows: main_dctcp_random_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_random_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_random_shortflows

htsim_ndp_random_shortflows: main_ndp_random_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_random_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_random_shortflows

htsim_ndp_alltoall:  main_ndp_alltoall.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_alltoall.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_alltoall

htsim_ndp_random_shortflows_tor: main_ndp_random_shortflows_tor.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_random_shortflows_tor.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_random_shortflows_tor

htsim_dctcp_random_shortflows_lossless: main_dctcp_random_shortflows_lossless.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_random_shortflows_lossless.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_random_shortflows_lossless


htsim_tcp_incast_shortflows: main_tcp_incast_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_tcp_incast_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_tcp_incast_shortflows

htsim_ndptunnel_incast_shortflows: main_ndptunnel_incast_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndptunnel_incast_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndptunnel_incast_shortflows

htsim_dctcp_incast_shortflows: main_dctcp_incast_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_incast_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_incast_shortflows

htsim_ndp_permutation: main_ndp_permutation.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_permutation.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_permutation

htsim_ndp_dragonfly_permutation: ../dragon_fly_topology.o main_ndp_dragonfly_permutation.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o  ../dragon_fly_topology.o
	$(CC) $(CFLAGS) main_ndp_dragonfly_permutation.o ../dragon_fly_topology.o ../connection_matrix.o ../shortflows.o $(LIB) -lhtsim -o htsim_ndp_dragonfly_permutation

htsim_ndp_permutation_incast: main_ndp_permutation_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_permutation_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_permutation_incast

htsim_ndprts_permutation_incast: main_ndprts_permutation_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndprts_permutation_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndprts_permutation_incast

htsim_ndptunnel_permutation: main_ndptunnel_permutation.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndptunnel_permutation.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndptunnel_permutation

htsim_ndp_permutation_tor: main_ndp_permutation_tor.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_permutation_tor.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_permutation_tor

htsim_ndp_random: main_ndp_random.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_random.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_random

htsim_ndp_permutation_lossless: main_ndp_permutation_lossless.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_permutation_lossless.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_permutation_lossless

htsim_dctcp_permutation_lossless: main_dctcp_permutation_lossless.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_permutation_lossless.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_permutation_lossless

htsim_ndp_oversubscribed_shortflows:  main_ndp_oversubscribed_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_oversubscribed_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o  htsim_ndp_oversubscribed_shortflows

htsim_ndp_permutation_fail: main_ndp_permutation_fail.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_permutation_fail.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_permutation_fail

htsim_ndp_incast: main_ndp_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast

htsim_ndp_incast2: main_ndp_incast2.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast2.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast2

htsim_ndp_incast2b: main_ndp_incast2b.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast2b.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast2b

htsim_ndp_outcast: main_ndp_outcast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_outcast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_outcast

htsim_ndp_outcast_incast: main_ndp_outcast_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_outcast_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_outcast_incast


htsim_tcp_outcast_incast: main_tcp_outcast_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_tcp_outcast_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_tcp_outcast_incast

htsim_ndprts_outcast: main_ndprts_outcast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndprts_outcast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndprts_outcast

htsim_ndprts_outcast_incast: main_ndprts_outcast_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndprts_outcast_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndprts_outcast_incast

htsim_ndprts_alltoall: main_ndprts_alltoall.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndprts_alltoall.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndprts_alltoall

htsim_ndp_incast_tor: main_ndp_incast_tor.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast_tor.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast_tor

htsim_ndp_incast_shortflows: main_ndp_incast_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast_shortflows


htsim_ndp_incast_shortflows_lossless: main_ndp_incast_shortflows_lossless.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast_shortflows_lossless.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast_shortflows_lossless

#htsim_ndp_incast_shortflows_demo: main_ndp_incast_shortflows_demo.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
#	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast_shortflows_demo.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast_shortflows_demo

htsim_ndp_incast_collateral: main_ndp_incast_collateral.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_incast_collateral.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_incast_collateral

htsim_ndp_in_out: main_ndp_in_out.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_in_out.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_in_out

htsim_ndp_perm_shortflows: main_ndp_perm_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_perm_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_perm_shortflows

htsim_ndp_perm_shortflows_tor: main_ndp_perm_shortflows_tor.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_perm_shortflows_tor.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_perm_shortflows_tor

htsim_ndplite_perm_shortflows: main_ndplite_perm_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndplite_perm_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndplite_perm_shortflows

htsim_dctcp_perm_shortflows: main_dctcp_perm_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_perm_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_perm_shortflows

htsim_dctcp_perm_shortflows_lossless: main_dctcp_perm_shortflows_lossless.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_perm_shortflows_lossless.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_perm_shortflows_lossless

htsim_dctcp_incast_collateral: main_dctcp_incast_collateral.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_incast_collateral.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_incast_collateral

htsim_dctcp_incast_collateral_lossless: main_dctcp_incast_collateral_lossless.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_incast_collateral_lossless.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_incast_collateral_lossless

htsim_tcp_perm_shortflows: main_tcp_perm_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_tcp_perm_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_tcp_perm_shortflows

htsim_ndp_outcast_shortflows: main_ndp_outcast_shortflows.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_outcast_shortflows.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_outcast_shortflows

htsim_ndp_oversubscribed: main_ndp_oversubscribed.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_ndp_oversubscribed.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_ndp_oversubscribed


htsim_dctcp_oversubscribed: main_dctcp_oversubscribed.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_dctcp_oversubscribed.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_dctcp_oversubscribed

htsim_swift: main_swift.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o generic_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_swift.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o generic_topology.o $(LIB) -lhtsim -o htsim_swift

htsim_swift_incast: main_swift_incast.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_swift_incast.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_swift_incast

htsim_swift_incast2: main_swift_incast2.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_swift_incast2.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_swift_incast2

htsim_swift_incast2b: main_swift_incast2b.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_swift_incast2b.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_swift_incast2b

htsim_swift_incast3: main_swift_incast3.o ../firstfit.o $(LIBDEP) ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../multihomed_fat_tree_topology.o ../star_topology.o
	$(CC) $(CFLAGS) ../firstfit.o main_swift_incast3.o ../vl2_topology.o ../fat_tree_topology.o ../fat_tree_switch.o ../bcube_topology.o ../connection_matrix.o ../oversubscribed_fat_tree_topology.o ../shortflows.o ../star_topology.o ../multihomed_fat_tree_topology.o $(LIB) -lhtsim -o htsim_swift_incast3

main_tcp_permutation.o: main_tcp_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_permutation.cpp

main_tcp_dragonfly_permutation.o: main_tcp_dragonfly_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_dragonfly_permutation.cpp 

main_dctcp_permutation.o: main_dctcp_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_permutation.cpp 

main_dctcp_random_shortflows.o: main_dctcp_random_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_random_shortflows.cpp 

main_ndp_random_shortflows.o: main_ndp_random_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_random_shortflows.cpp 

main_ndp_alltoall.o: main_ndp_alltoall.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_alltoall.cpp 

main_ndp_random_shortflows_tor.o: main_ndp_random_shortflows_tor.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_random_shortflows_tor.cpp 

main_dctcp_random_shortflows_lossless.o: main_dctcp_random_shortflows_lossless.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_random_shortflows_lossless.cpp 

main_tcp_incast_shortflows.o: main_tcp_incast_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_incast_shortflows.cpp

main_ndptunnel_incast_shortflows.o: main_ndptunnel_incast_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndptunnel_incast_shortflows.cpp 

main_dctcp_incast_shortflows.o: main_dctcp_incast_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_incast_shortflows.cpp 

main_ndp_incast_shortflows_lossless.o: main_ndp_incast_shortflows_lossless.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast_shortflows_lossless.cpp 

main_ndp_permutation.o: main_ndp_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_permutation.cpp

main_ndp_dragonfly_permutation.o: main_ndp_dragonfly_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_dragonfly_permutation.cpp

main_ndp_permutation_incast.o: main_ndp_permutation_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_permutation_incast.cpp

main_ndprts_permutation_incast.o: main_ndprts_permutation_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndprts_permutation_incast.cpp

main_ndptunnel_permutation.o: main_ndptunnel_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndptunnel_permutation.cpp 

main_ndp_permutation_tor.o: main_ndp_permutation_tor.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_permutation_tor.cpp 

main_ndplite_permutation.o: main_ndplite_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndplite_permutation.cpp 

main_ndp_random.o: main_ndp_random.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_random.cpp 

main_ndp_permutation_lossless.o: main_ndp_permutation_lossless.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_permutation_lossless.cpp 

main_dctcp_permutation_lossless.o: main_dctcp_permutation_lossless.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_permutation_lossless.cpp 

main_ndp_bcube_permutation.o: main_ndp_bcube_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_bcube_permutation.cpp 

main_ndp_camcube_permutation.o: main_ndp_camcube_permutation.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_camcube_permutation.cpp 

main_ndp_oversubscribed.o: main_ndp_oversubscribed.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_oversubscribed.cpp

main_dctcp_oversubscribed.o: main_dctcp_oversubscribed.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_oversubscribed.cpp 

main_ndp_oversubscribed_shortflows.o: main_ndp_oversubscribed_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c  main_ndp_oversubscribed_shortflows.cpp

main_ndp_permutation_fail.o: main_ndp_permutation_fail.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_permutation_fail.cpp 

main_ndp_incast.o: main_ndp_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast.cpp 

main_ndp_incast2.o: main_ndp_incast2.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast2.cpp 

main_ndp_incast2b.o: main_ndp_incast2b.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast2b.cpp 

main_ndp_incast_tor.o: main_ndp_incast_tor.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast_tor.cpp 

main_ndp_incast_shortflows.o: main_ndp_incast_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast_shortflows.cpp 

main_ndplite_incast_shortflows.o: main_ndplite_incast_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndplite_incast_shortflows.cpp 

#main_ndp_incast_shortflows_demo.o: main_ndp_incast_shortflows_demo.cpp
#	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast_shortflows_demo.cpp 

main_ndp_incast_collateral.o: main_ndp_incast_collateral.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_incast_collateral.cpp 

main_ndplite_incast_collateral.o: main_ndplite_incast_collateral.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndplite_incast_collateral.cpp 

main_ndp_outcast.o: main_ndp_outcast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_outcast.cpp

main_ndp_outcast_incast.o: main_ndp_outcast_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_outcast_incast.cpp

main_tcp_outcast_incast.o: main_tcp_outcast_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_outcast_incast.cpp 

main_ndprts_outcast.o: main_ndprts_outcast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndprts_outcast.cpp

main_ndprts_outcast_incast.o: main_ndprts_outcast_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndprts_outcast_incast.cpp

main_ndprts_alltoall.o: main_ndprts_alltoall.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndprts_alltoall.cpp 

main_ndp_in_out.o: main_ndp_in_out.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_in_out.cpp 

main_ndp_perm_shortflows.o: main_ndp_perm_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_perm_shortflows.cpp 

main_ndp_perm_shortflows_tor.o: main_ndp_perm_shortflows_tor.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_perm_shortflows_tor.cpp 

main_ndplite_perm_shortflows.o: main_ndplite_perm_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndplite_perm_shortflows.cpp 

main_dctcp_perm_shortflows.o: main_dctcp_perm_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_perm_shortflows.cpp 

main_dctcp_perm_shortflows_lossless.o: main_dctcp_perm_shortflows_lossless.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_perm_shortflows_lossless.cpp 

main_tcp_perm_shortflows.o: main_tcp_perm_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_tcp_perm_shortflows.cpp 

main_dctcp_incast_collateral.o: main_dctcp_incast_collateral.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_incast_collateral.cpp 

main_dctcp_incast_collateral_lossless.o: main_dctcp_incast_collateral_lossless.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_dctcp_incast_collateral_lossless.cpp 

main_ndp_outcast_shortflows.o: main_ndp_outcast_shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_ndp_outcast_shortflows.cpp 

main_swift_incast.o: main_swift_incast.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_swift_incast.cpp 

main_swift_incast2.o: main_swift_incast2.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_swift_incast2.cpp 

main_swift_incast2b.o: main_swift_incast2b.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_swift_incast2b.cpp 

main_swift_incast3.o: main_swift_incast3.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_swift_incast3.cpp 

clean:	
	rm -f *.o htsim_ndp* htsim_swift* htsim_tcp* htsim_dctcp* *~
