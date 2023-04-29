set terminal pdfcairo size 3.5in,2in
#set terminal png size 600,400
#set size 1.0, 0.5
set output "incast_overhead.pdf"
set yrange [0:1.5]
set logscale x
set ylabel "Retransmissions per packet"
set xlabel "Size of incast (flows)"
set key top left
#set title "Traffic overhead of incast, 8192 node FatTree, 270000 byte flows"
plot "bounces23" using 1:($2/($1*30)) w l title "RTX (Bounces), IW=23" lc rgbcolor "red" dashtype (3,3,3,3) lw 1.5, \
"bounces23" using 1:($3/($1*30)) w l title "RTX (Nacks), IW=23" lc rgbcolor "red", \
"bounces10" using 1:($2/($1*30)) w l title "RTX (Bounces), IW=10" lc rgbcolor "black" dashtype (3,3,3,3) lw 1.5, \
"bounces10" using 1:($3/($1*30)) w l title "RTX (Nacks), IW=10" lc rgbcolor "black" ,\
"bounces1" using 1:($2/($1*30)) w l title "RTX (Bounces), IW=1" lc rgbcolor "blue" dashtype (3,3,3,3) lw 1.5, \
"bounces1" using 1:($3/($1*30)) w l title "RTX (Nacks), IW=1" lc rgbcolor "blue" 