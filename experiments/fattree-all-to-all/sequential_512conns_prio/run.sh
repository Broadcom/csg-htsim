SIMPATH=../../../sim/datacenter
CWND=50
FLOWSIZE=1000000
MTU=4000
XMAX=300000 # for flowsize = 512
LINKSPEED=100000
QUEUESIZE=35
SEED=13
EXTRA=0.0
PARALLEL=1
SIM=perm
N=1024
FLOWS=512
TMFILE=a2a-${N}-${FLOWS}-${PARALLEL}.cm
echo "Running all-to-all experiment with $N nodes, $CONNS flows, flowsize $FLOWSIZE, spray strategy ${SIM}, parallel conns ${PARALLEL}, recv priority"
echo "flowsize $FLOWSIZE"
CMD="python $SIMPATH/connection_matrices/gen_serialn_alltoall_prio.py ${TMFILE} $N $FLOWS $FLOWS $PARALLEL $FLOWSIZE ${EXTRA} ${SEED} > /dev/null"
echo ${CMD}
eval ${CMD}
STRATPATHS="undefined"
if [ ${SIM} = "rr" ]
then
    STRATPATHS="-strat ecmp_rr"
elif [ ${SIM} = "ecmp1" ]
then
    STRATPATHS="-strat ecmp -paths 1"
elif [ ${SIM} = "ecmp100" ]
then
    STRATPATHS="-strat ecmp -paths 100"
elif [ ${SIM} = "ecmphost1" ]
then
    STRATPATHS="-strat ecmp_host -paths 1"
elif [ ${SIM} = "ecmphost100" ]
then
    STRATPATHS="-strat ecmp_host -paths 100"
elif [ ${SIM} = "perm" ]
then
    STRATPATHS="-strat perm"
elif [ ${SIM} = "perm1" ]
then
    STRATPATHS="-strat perm -paths 1"
fi
echo "$FLOWS Flows"
CMD="$SIMPATH/htsim_ndp -tm $TMFILE -linkspeed ${LINKSPEED} ${STRATPATHS} -nodes $N -conns $FLOWS -q $QUEUESIZE -cwnd $CWND -mtu $MTU -end ${XMAX} > out_${N}_${FLOWS}_${SIM}_${CWND}iw_${PARALLEL}par.tmp"
echo ${CMD}
eval ${CMD}

CMD="python plot_overlap.py ${N} ${FLOWS} ${PARALLEL} ${CWND}"
echo ${CMD}
eval ${CMD}

CMD="python proc_incast_color.py incast_${N}_${FLOWS}_${SIM}_${CWND}iw_${PARALLEL}par.tmp ${PARALLEL}"
echo ${CMD}
eval ${CMD}

CMD="gnuplot incast_${N}_${FLOWS}_${SIM}_${CWND}iw_${PARALLEL}par_prio.gp"
echo ${CMD}
eval ${CMD}
