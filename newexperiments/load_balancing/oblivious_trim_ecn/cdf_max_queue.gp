set terminal pdf color
set xlabel "Max_Queue_Depth (KB)"
set ylabel "CDF (%)"
set yrange [0:105]
set key bottom right
set title "FCT NDP 8192 node 100Gb/s 3-tier fattree, 8192 flow permutation, cwnd=50 2MB flows"
set output "cdf_queue_depth.pdf"
plot "3tier_strack_ecn_256/max_queue_depth" using ($1/1000):($0*100/32768) w l lw 2 linecolor rgb "black" t "strack, ecn, 256 paths", \
"3tier_strack_256/max_queue_depth" using ($1/1000):($0*100/32768) w l lw 2 linecolor rgb "red" t "strack, even spray, 256 paths", \
"3tier_strack_rand_256/max_queue_depth" using ($1/1000):($0*100/32768) w l lw 2 linecolor rgb "green" t "strack, random path with ECN, 256 paths",\
"3tier_trim_256/max_queue_depth" using ($1/1000):($0*100/32768) w l lw 2 linecolor rgb "blue" t "Oblivious, Trim at 1 BDP, 256 paths"
