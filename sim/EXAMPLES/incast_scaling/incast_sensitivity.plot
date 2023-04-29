set terminal pdfcairo size 3.5in,2in
#set terminal png size 600,400
#set size 1.0, 0.5
set output "incast_sensitivity.pdf"
set yrange [-0.5:5]
#set yrange [-0.5:0.5]
#set xrange [900:1100]
set logscale x
set ylabel "Percent overhead"
set xlabel "Size of incast (flows)"
set grid
#set title "Overhead of incast above theoretical best completion time, 8192 node FatTree, 270000 byte flows"
plot "incast_ndp_completion_times_23_270000size_max" using 1:(100*$2/($1*271920.0*8.0/10000000000.0 + 0.000042256)-100) w l lc rgbcolor "red" lw 1.3 title "NDP, IW=23", \
"incast_ndp_completion_times_10_270000size_max" using 1:(100*$2/($1*271920.0*8.0/10000000000.0 + 0.000042256)-100) w l lc rgbcolor "black" lw 1.3 title "NDP, IW=10", \
"incast_ndp_completion_times_1_270000size_max" using 1:(100*$2/($1*271920.0*8.0/10000000000.0 + 0.000042256)-100) w l lc rgbcolor "blue" lw 1.3 title "NDP, IW=1"

