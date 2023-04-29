set term pdfcairo size 3,1
set output "collateral_dctcp.pdf"
set ylabel "Goodput\n(Gb/s)" offset 0,0
set xlabel "Time (s)" offset 0,0.5
set key font "Helvetica,12"
set key right bottom
set yrange [0:15]
set xrange [0.05:0.4]
set ytics 5
plot "dctcp_longflow" using 1:($11*8/1000000000) w filledcurves x1 fs solid 0.1 notitle lc "black" lw 2, \
"dctcp_longflow" using 1:($11*8/1000000000) w l title "Long DCTCP Flow" lc "black" lw 2, \
"dctcp_incast" using 1:($2*8/1000000000) w l title "64 Flows Incast" lc "red" lw 2 dt (3,3), 


