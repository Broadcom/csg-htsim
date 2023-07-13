set terminal pdf color
set xlabel "FCT (us)"
set ylabel "CDF (%)"
set yrange [0:105]
set key bottom right
set title "FCT NDP 8192 node 100Gb/s 3-tier fattree, 8192 flow permutation, cwnd=50 2MB flows"
set output "cdf_fct.pdf"
plot "3tier_notrim_256/finished" using ($1):($0*100/8192) w l lw 2 lc "black" t "Oblivious, large queue, 256 paths", \
"3tier_trim_256/finished" using ($1):($0*100/8192) w l lw 2 lc "blue" t "Oblivious, Trim at 1 BDP, 256 paths",\
"3tier_notrim_ecn_256/finished" using ($1):($0*100/8192) w l lw 2 lc "red" t "ECN-LB, ECN at 0.5 BDP, 256 paths",\
"3tier_notrim_32/finished" using ($1):($0*100/8192) w l lw 2 lc "green" t "Oblivious, large queue, 32 paths",\
"3tier_trim_32/finished" using ($1):($0*100/8192) w l lw 2 lc "orange" t "Oblivious, Trim at 1 BDP, 32 paths",\
"3tier_notrim_ecn_32/finished" using ($1):($0*100/8192) w l lw 2 lc "purple" t "ECN-LB, ECN at 0.5 BDP, 32 paths"
