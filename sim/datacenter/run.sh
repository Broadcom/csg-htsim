cd ../
make 

cd datacenter/
#./htsim_strack -nodes 16 -tm connection_matrices/perm_16n_16c.cm -cwnd 50 -strat ecmp_host_ecn -paths 100 -log sink -q 50 -end 1001 >tmp
./htsim_strack -nodes 18 -tm connection_matrices/perm_18n_4c.cm -cwnd 50 -strat ecmp_host_ecn -paths 100  -q 50 -end 1001 -tiers 2 -log sink >tmp
