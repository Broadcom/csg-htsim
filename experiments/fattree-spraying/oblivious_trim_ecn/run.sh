#!/bin/bash
echo "Running Oblivious, 256 entropy values, no trimming..."
cd 3tier_notrim_256
./run.sh&
echo "Running Oblivious, 32 entropy values, no trimming..."
cd ../3tier_notrim_32
./run.sh&
echo "Running Oblivious, 256 entropy values, trimming at 1 BDP..."
cd ../3tier_trim_256
./run.sh&
echo "Running Oblivious, 32 entropy values, trimming at 1 BDP..."
cd ../3tier_trim_32
./run.sh&
echo "Running ECN load balancing, 256 entropy values, ECN at 0.5 BDP..."
cd ../3tier_notrim_ecn_256
./run.sh&
echo "Running ECN load balancing, 32 entropy values, ECN at 0.5 BDP..."
cd ../3tier_notrim_ecn_32
./run.sh&
cd ..
FAIL=0
for job in `jobs -p`
do
    echo $job
    wait $job || let "FAIL+=1"
done
if [ "$FAIL" == "0" ];
then
echo "All sims completed"
else
echo "Some sims failed! ($FAIL)"
fi
gnuplot cdf_fin.gp
