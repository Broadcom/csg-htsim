set terminal pdf color
set output "comparison.pdf"
set ylabel "Flow Completion Time"
set xlabel "Network size (hosts)"
set title "Flow Completion Time, Permutation TM, 100% load demand, 2MB flows, 100G FatTree"
set key top right
set logscale x
plot "data/fct_paths_1.dat" using 1:3 w lp t "ECMP 1 path",\
 "data/fct_paths_8.dat" using 1:3 w lp t "ECMP 8 paths",\
 "data/fct_paths_16.dat" using 1:3 w lp t "ECMP 16 paths",\
 "data/fct_paths_32.dat" using 1:3 w lp t "ECMP 32 paths",\
 "data/fct_paths_64.dat" using 1:3 w lp t "ECMP 64 paths",\
 "data/fct_paths_128.dat" using 1:3 w lp t "ECMP 128 paths",\
 "data/fct_paths_256.dat" using 1:3 w lp t "ECMP 256 paths"



