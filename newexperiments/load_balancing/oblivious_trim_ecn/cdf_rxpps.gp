set terminal pdf color
set xlabel "RX (MPPS)"
set ylabel "CDF (%)"
set yrange [0:105]
set key bottom right
set title "FCT NDP 8192 node 100Gb/s 3-tier fattree, 8192 flow permutation, cwnd=50 2MB flows"
set output "cdf_rxpps.pdf"
plot "3tier_strack_ecn_256/rxpps" using ($1):($0*100/8192) w l lw 2 linecolor rgb "black" t "strack, ecn, 256 paths", \
"3tier_trim_256/rxpps" using ($1):($0*100/8192) w l lw 2 linecolor rgb "blue" t "Oblivious, Trim at 1 BDP, 256 paths"
