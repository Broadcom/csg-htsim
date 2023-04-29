#!/bin/sh
flowsize=270000
cwnd=1
bin=../../datacenter/old_tests
for ((conns=1;conns<=20;conns=conns+1)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=20;conns<=100;conns=conns+2)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=110;conns<=400;conns=conns+10)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=425;conns<=1000;conns=conns+25)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=1000;conns<=2000;conns=conns+50)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=2100;conns<=8000;conns=conns+100)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

cwnd=10
for ((conns=1;conns<=20;conns=conns+1)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=20;conns<=100;conns=conns+2)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=110;conns<=400;conns=conns+10)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=425;conns<=1000;conns=conns+25)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=1000;conns<=2000;conns=conns+50)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=2100;conns<=8000;conns=conns+100)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

cwnd=23
for ((conns=1;conns<=20;conns=conns+1)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=20;conns<=100;conns=conns+2)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=110;conns<=400;conns=conns+10)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=425;conns<=1000;conns=conns+25)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=1000;conns<=2000;conns=conns+50)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done

for ((conns=2100;conns<=8000;conns=conns+100)) ;
do
    echo ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize
    ${bin}/htsim_ndp_incast_shortflows -o incast${cwnd}_q8_c${conns}_f${flowsize} -conns $conns -nodes 8192 -cwnd ${cwnd} -q 8 -strat perm -flowsize $flowsize > ts_incast${cwnd}_q8_c${conns}_f${flowsize}
	python ./process_data_incast_conns.py incast${cwnd}_q8_c${conns}_f${flowsize} $conns $flowsize $cwnd ndp
	rm incast${cwnd}_q8_c${conns}_f${flowsize}*
done


gnuplot incast_sensitivity.plot

grep New: ts_incast1_* | awk '{print $2, $10, $8}' | sort -n > bounces1
grep New: ts_incast10_* | awk '{print $2, $10, $8}' | sort -n > bounces10
grep New: ts_incast23_* | awk '{print $2, $10, $8}' | sort -n > bounces23
gnuplot incast_overhead.plot
