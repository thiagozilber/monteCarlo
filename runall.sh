#!/bin/bash

FIXED_ARGS="--naive 8000 300 5 --serial"
OUTPUT="results.txt"
BENCH="./benchmark.sh"

run() {
    local label=$1
    local args=$2
    echo "========================================" >> $OUTPUT
    echo " $label" >> $OUTPUT
    echo "========================================" >> $OUTPUT
    $BENCH $FIXED_ARGS $args >> $OUTPUT 2>&1
    echo "" >> $OUTPUT
}

# Clear previous results
> $OUTPUT

# Test git connection
echo "Testing git..."
git ls-remote --exit-code origin > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "Git is bust. Check your remote and credentials."
    exit 1
fi
echo "Git is good to go. Starting..."
echo ""

# Static
run "static | default chunk"       "--schedule=static"
run "static | chunk=10"            "--schedule=static --chunk=10"
run "static | chunk=50"            "--schedule=static --chunk=50"
run "static | chunk=100"           "--schedule=static --chunk=100"
run "static | chunk=500"           "--schedule=static --chunk=500"
run "static | chunk=1000"          "--schedule=static --chunk=1000"

# Dynamic
run "dynamic | default chunk"      "--schedule=dynamic"
run "dynamic | chunk=10"           "--schedule=dynamic --chunk=10"
run "dynamic | chunk=50"           "--schedule=dynamic --chunk=50"
run "dynamic | chunk=100"          "--schedule=dynamic --chunk=100"
run "dynamic | chunk=500"          "--schedule=dynamic --chunk=500"
run "dynamic | chunk=1000"         "--schedule=dynamic --chunk=1000"

# Guided
run "guided | default chunk"       "--schedule=guided"
run "guided | chunk=10"            "--schedule=guided --chunk=10"
run "guided | chunk=50"            "--schedule=guided --chunk=50"
run "guided | chunk=100"           "--schedule=guided --chunk=100"
run "guided | chunk=500"           "--schedule=guided --chunk=500"
run "guided | chunk=1000"          "--schedule=guided --chunk=1000"

git add results.txt
git commit -m "benchmark results: schedule/chunk sweep"
git push

echo "All runs complete." >> $OUTPUT
echo "All runs complete."