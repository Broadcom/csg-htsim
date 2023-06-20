SIMPATH=../../../../sim/datacenter/
N=8192  # no of nodes
FLOWS=8192 # no of flows in permutation
CWND=50 # 50 packet (1.5 BDP) window
FLOWSIZE=2000000 # flow size in bytes
MTU=4000
#XMAX=80 #for flowsize = 25
XMAX=300 # for flowsize = 512
LINKSPEED=100000 # 100Gb/s network
TIERS=3 # 3 tier fattree
EXTRA=0 # start simultaneously
SEED=13 # random seed for TM
STRAT=ecmp_host_ecn
PATHS=32
QUEUESIZE=1000
ECNTHRESH=0.018
echo "Running permutation experiment with $N nodes, $CONNS flows"
echo "FLOWSIZE $FLOWSIZE"
echo "$FLOWS Flows"
TMFILE=perm_${N}n_${FLOWS}c_0u_${FLOWSIZE}b.cm
CMD="python $SIMPATH/connection_matrices/gen_permutation.py ${TMFILE} $N ${FLOWS} ${FLOWSIZE} ${EXTRA} ${SEED} > /dev/null"
echo ${CMD}
eval ${CMD}

CMD="$SIMPATH/htsim_ndp -tm $TMFILE -log sink -linkspeed ${LINKSPEED} -strat $STRAT -paths $PATHS -nodes $N \
-conns $FLOWS -q $QUEUESIZE -cwnd $CWND -mtu $MTU -end ${XMAX} -logtime 0.01 -ecn_thresh $ECNTHRESH > out.tmp"
echo $CMD
eval $CMD
grep finished out.tmp | awk '{print $7}' > finished
echo `wc finished | awk '{print $1}'` flows finished
