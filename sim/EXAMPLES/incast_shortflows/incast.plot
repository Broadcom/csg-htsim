set term pdfcairo font "Helvetica, 22"
set xlabel "Number of backend servers involved in incast"
set ylabel "Incast completion time (ms)" offset 1.5
set output "incast.pdf"

set key at 200,270
set yrange [0:300]
set xrange [0:431]

plot "incast_ndp_completion_times_23_450000size_all" using 1:($2*1000):($2*1000):($4*1000) w yerrorbars lt -1  lw 2 lc rgb "red" t "NDP"

