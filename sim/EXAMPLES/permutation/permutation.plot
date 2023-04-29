set term pdfcairo font "Helvetica,24"
set output "permutation.pdf"

set xlabel "Flow rank"
set ylabel "Throughput (Gbps)" offset 1.5
#set grid

set yrange [0:10]
set xrange [0:432]
set ytics 2
set key right bottom

#plot "rates.ndp" using 0:($1/1000) w l lc rgb "red" lw 3 t "NDP",\
#     "rates.mptcp" using 0:($1/1000) w l lc rgb "blue" lw 3 t "MPTCP",\
#     "rates.dctcp" using 0:($1/1000) w l lc rgb "black" lw 3 t "DCTCP",\
#     "rates.dcqcn" using 0:($1/1000) w l lc rgb "green" lw 3 t "DCQCN"

plot "logfile.rates" using 0:1 w l lc rgb "red" lw 3 t "NDP",\
     "mptcp_logfile.rates" using 0:1 w l lc rgb "blue" lw 3 t "MPTCP",\
     "dctcp_logfile.rates" using 0:1 w l lc rgb "black" lw 3 t "DCTCP"

    
