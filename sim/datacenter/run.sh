cd ../
make -j8 

cd datacenter/
#./htsim_strack -nodes 16 -tm connection_matrices/perm_16n_16c.cm -cwnd 50 -strat ecmp_host_ecn -paths 100 -log sink -q 1000 -end 1000001 >tmp

#valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./htsim_strack -nodes 18 -tm connection_matrices/perm_18n_4c.cm -cwnd 50 -strat ecmp_host_ecn -paths 8  -q 1000 -end 1000001 -tiers 2 -log sink >tmp
#./htsim_strack -nodes 18 -tm connection_matrices/perm_18n_4c.cm -cwnd 50 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > tmp 


#./htsim_strack -nodes 32 -tm connection_matrices/perm_32n_32c.cm -cwnd 50 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > ecn_spray
#./htsim_strack -nodes 32 -tm connection_matrices/perm_32n_32c.cm -cwnd 50 -strat ecmp_host -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > even_spray
#grep tput even_spray > tput_even_spray.txt
#awk -F' ' '{sum+=$9;} END{print sum;}' tput_even_spray.txt

#####./htsim_strack -nodes 32 -tm connection_matrices/bi_32n_32c_2M.cm -cwnd 60 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > ecn_spray
#####
./htsim_strack -nodes 512 -tm connection_matrices/bi_512n_512c.cm -cwnd 60 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -ecn_thresh 0.25 -log sink > ecn_spray
#####grep tput ecn_spray > tput_ecn_spray.txt
#####awk -F' ' '{sum+=$9; n++;} END{ if(n>0) print sum/n;}' tput_ecn_spray.txt

#./htsim_strack -nodes 32 -tm connection_matrices/bi_omnet_32n_32c.cm -cwnd 60 -strat ecmp_host -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > even_spray
##./htsim_strack -nodes 32 -tm connection_matrices/bi_omnet_32n_32c.cm -cwnd 60 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > even_spray

#./htsim_strack -nodes 4 -tm connection_matrices/bi_omnet_4n_4c.tm -cwnd 60 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > even_spray 

#./htsim_strack -nodes 8 -tm connection_matrices/bi_omnet_8n_8c.tm -cwnd 60 -strat ecmp_host_ecn -paths 64  -q 1000 -end 1000001 -tiers 2 -log sink > even_spray 
grep "tput:" ecn_spray > tput_even_spray.txt
awk -F' ' '{sum+=$9; n++;} END{ if(n>0) print sum/n;}' tput_even_spray.txt

grep -e outgoingPort -e receiveData -e trace -e core  even_spray > tmp
grep -e receiveData -e "src 4" -e "dst 4" -V trace  tmp > node4


