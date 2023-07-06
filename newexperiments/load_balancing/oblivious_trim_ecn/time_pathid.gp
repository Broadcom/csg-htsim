set terminal pdf color
set xlabel "Time (us)"
set ylabel "path id"
set yrange [0:256]
set key bottom right
set title "FCT NDP 8192 node 100Gb/s 3-tier fattree, 8192 flow permutation, cwnd=50 2MB flows"
set output "time_pathid.pdf"
plot "3tier_strack_ecn_256/path_id" using ($1):($2) w l lw 2 linecolor rgb "black" t "strack, ecn, 256 paths", \
"3tier_strack_rand_256/path_id" using ($1):($2) w l lw 2 linecolor rgb "green" t "strack, random path with ECN, 256 paths"

#"3tier_strack_256/finished" using ($1):($2) w l lw 2 linecolor rgb "red" t "strack, even spray, 256 paths",\
#"3tier_trim_32/finished" using ($1):($0*100/8192) w l lw 2 linecolor rgb "orange" t "Oblivious, Trim at 1 BDP, 32 paths",\
#"3tier_notrim_ecn_32/finished" using ($1):($0*100/8192) w l lw 2 linecolor rgb "purple" t "ECN_LB, ECN at 0.5 BDP, 32 paths"
