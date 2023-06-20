set terminal pdf color
set output "incast_1024_512_perm_50iw_1par_prio.pdf"
set title "512 node sequential all-to-all, 1024 fat tree, src route, recv priority,\n50 pkt IW, 1MB flow size" offset 0,-3
set zlabel "No of incoming flows" rotate parallel offset 1,0
set ylabel "Time (ms)"
set xlabel "Receiver rank"
set ytics 0, 20, 160
#set view 45,330
set view 40,30
set size 1,1.1
set xrange [0:512]
set xyplane 0
set zrange [0:120]
unset colorbox
set palette defined ( 0 "blue", 1 "orange", 2 "magenta", 3 "red")
splot "incast_1024_512_perm_50iw_1par.color.tmp" using 2:($1/1000):3:4 w i lw 1 lc palette notitle