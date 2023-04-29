set term pdfcairo size 4,2 font "Helvetica,18"
set output "failure.pdf"
set yrange [0:10]
set xlabel "Flow number" offset 0,0.5
set ylabel "Throughput (Gbps)" offset 2.0,0
#set key font "Helvetica,20"
set key right bottom
set ytics 2
#set title "Permulation, 128 node FatTree, cwnd=20"
plot "ndp_penalty.rates" using 0:1 w l lw 2 lc rgb "red" t "NDP",\
     "mptcp.rates" using 0:1 w l lw 3 lc rgb "blue" t "MPTCP",\
     "ndp_perm.rates" using 0:1 w l lw 4 dt 2 lc rgb "red" t "NDP without path penalty",\
     "dctcp.rates" using 0:1 w l lw 3 lc rgb "black" t "DCTCP"


     
     