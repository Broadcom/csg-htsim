#!/bin/bash
# Compare RoCE and Swift performance

echo "=== Building simulators ==="
cd "$(dirname "$0")"
make htsim_roce htsim_swift || exit 1

echo ""
echo "=== Running RoCE simulation ==="
./htsim_roce -o roce_log -nodes 16 -conns 16 -q 15 -end 10000 \
    -strat single -tm simple_permutation.tm > roce_debug.txt 2>&1

echo "RoCE simulation completed"
echo ""

echo "=== Running Swift simulation ==="
./htsim_swift -o swift_log -nodes 16 -conns 16 -q 8 -cwnd 15 \
    -tm simple_permutation.tm > swift_debug.txt 2>&1

echo "Swift simulation completed"
echo ""

echo "=== Results ==="
echo "RoCE output: roce_log (debug: roce_debug.txt)"
echo "Swift output: swift_log (debug: swift_debug.txt)"
echo ""
echo "To parse results, use:"
echo "  ../parse_output roce_log -ascii > roce_log.asc"
echo "  ../parse_output swift_log -ascii > swift_log.asc"
