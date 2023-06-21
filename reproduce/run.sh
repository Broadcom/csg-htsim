#python3 ../sim/datacenter/connection_matrices/gen_permutation.py gen_permutation_16000n_16000c_2Ms.tm 16000 16000 2000000 0 0  
../sim/datacenter/htsim_ndp -nodes 16000 -tm gen_permutation_16000n_16000c_2Ms.tm -cwnd 50 -strat ecmp_host -paths 1 -q 35 -end 1000001 -mtu 4000 -tiers 3 -ecn_thresh 0.25 -log sink -o ./logout.dat > tmp.txt
