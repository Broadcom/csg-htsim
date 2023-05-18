cd ../
make -j8 

cd datacenter/
#./htsim_strack -nodes 16 -tm connection_matrices/perm_16n_16c.cm -cwnd 50 -strat ecmp_host_ecn -paths 100 -log sink -q 1000 -end 1001 >tmp

#valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./htsim_strack -nodes 18 -tm connection_matrices/perm_18n_4c.cm -cwnd 50 -strat ecmp_host_ecn -paths 8  -q 1000 -end 1001 -tiers 2 -log sink >tmp
#./htsim_strack -nodes 18 -tm connection_matrices/perm_18n_4c.cm -cwnd 50 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1001 -tiers 2 -log sink > tmp 
./htsim_strack -nodes 32 -tm connection_matrices/perm_32n_32c.cm -cwnd 50 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1001 -tiers 2 -log sink > tmp 
