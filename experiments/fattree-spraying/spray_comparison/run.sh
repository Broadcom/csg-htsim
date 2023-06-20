SIMPATH=../../../sim/datacenter/
CWND=50
FLOWSIZE=2000000
MTU=4000
XMAX=1500 # stop after 1000us
STRAT=ecmp
LINKSPEED=100000
QUEUESIZE=35
echo "Running permutation experiment with flowsize $FLOWSIZE"
for SEED in {13..17}
do

    for N in 432 1024 2000 3456 8192 16000
    do
	FLOWS=${N}
	CMD="python $SIMPATH/connection_matrices/gen_permutation.py perm-${N}-${FLOWS}.cm $N $FLOWS $FLOWSIZE 0.0 ${SEED}"
        echo ${CMD}
        eval ${CMD}
	for PATHS in 1 8 16 32 64 128 256
        do
	    echo "Running seed ${SEED}, ${N} nodes, ${FLOWS} flows, ${PATHS} paths, NDP, ECMP"
	    TMFILE=perm-${N}-${FLOWS}.cm
	    echo "$FLOWS Flows"
	    CMD="$SIMPATH/htsim_ndp -tm $TMFILE -log switch -log sink -linkspeed ${LINKSPEED} -strat $STRAT -paths ${PATHS} -nodes $N -conns $FLOWS -q $QUEUESIZE -cwnd $CWND -mtu $MTU -end ${XMAX} -logtime 0.01 -o data/logout.dat > data/out_${N}.tmp"
	    echo ${CME}
	    eval ${CMD}
	    CMD="$SIMPATH/../parse_output data/logout.dat -ascii > data/logout_$FLOWS_$SEED_${PATHS}.txt"
	    echo ${CME}
	    eval ${CMD}
	    echo -n "${FLOWS} ${SEED} ${PATHS} " >  data/finished_${FLOWS}_${SEED}_${PATHS}.txt
	    CMD="grep finished data/out_${N}.tmp | tail -1 >> data/finished_${FLOWS}_${SEED}_${PATHS}.txt"
	    echo ${CME}
	    eval ${CMD}
	done
    done
done

python makegraph.py
gnuplot comparison.plot
