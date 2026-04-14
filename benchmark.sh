#!/bin/bash

export LC_NUMERIC=C

PROGRAM="./nbody-naive"
THREAD_COUNTS=(1 2 4 8 16)

positional=()
for arg in "$@"; do
    if [[ "$arg" == "--smart" ]]; then
        PROGRAM="./nbody-smart"
    elif [[ "$arg" == "--naive" ]]; then
        PROGRAM="./nbody-naive"
    else
        positional+=("$arg")
    fi
done

NUM_BODIES=${positional[0]:-8000}
NUM_STEPS=${positional[1]:-100}
RUNS=${positional[2]:-5}

# Colors
CYAN='\033[0;36m'
BOLD='\033[1m'
YELLOW='\033[0;33m'
RESET='\033[0m'

total_runs=$(( (${#THREAD_COUNTS[@]} + 1) * RUNS ))
current_run=0

print_progress() {
    local label=$1
    local pct=$(( current_run * 100 / total_runs ))
    local filled=$(( pct / 5 ))
    local empty=$(( 20 - filled ))
    local bar=$(printf '█%.0s' $(seq 1 $filled))$(printf '░%.0s' $(seq 1 $empty))
    printf "\r  ${CYAN}[${bar}]${RESET} %3d%%  %s" $pct "$label"
}

echo ""
echo -e "${BOLD}Benchmarking $PROGRAM${RESET}"
echo "  Bodies: $NUM_BODIES  |  Steps: $NUM_STEPS  |  Runs per config: $RUNS"
echo ""

# Serial reference (always runs)
echo -e "${YELLOW}  Running serial reference...${RESET}"
serial_total=0
for run in $(seq 1 $RUNS); do
    current_run=$(( current_run + 1 ))
    print_progress "serial  run=$run/$RUNS"
    t=$($PROGRAM $NUM_BODIES $NUM_STEPS 1 2>/dev/null \
        | grep "Time" | awk '{print $4}')
    serial_total=$(echo "$serial_total + $t" | bc -l)
done
serial_time=$(echo "$serial_total / $RUNS" | bc -l)
printf "\r  ✓  Serial reference: %.4f s\n\n" $serial_time

# Parallel runs
results=()
for threads in "${THREAD_COUNTS[@]}"; do
    if [ "$threads" -eq 1 ]; then continue; fi  # already covered by serial
    total=0
    for run in $(seq 1 $RUNS); do
        current_run=$(( current_run + 1 ))
        print_progress "threads=$(printf '%-2d' $threads)  run=$run/$RUNS"
        t=$($PROGRAM $NUM_BODIES $NUM_STEPS $threads 2>/dev/null \
            | grep "Time" | awk '{print $4}')
        total=$(echo "$total + $t" | bc -l)
    done
    avg=$(echo "$total / $RUNS" | bc -l)
    results+=("$threads $avg")
done

printf "\r%80s\r" ""
echo ""

# Results table
echo -e "${BOLD}Results:${RESET}"
echo "  ┌─────────┬─────────────┬──────────┬────────────┐"
echo "  │ Threads │  Avg Time   │ Speedup  │ Efficiency │"
echo "  ├─────────┼─────────────┼──────────┼────────────┤"
printf "  │ %7d │ %9.4f s │ %7.2fx  │ %8.1f%% │\n" \
    1 $serial_time 1.00 100.0
for entry in "${results[@]}"; do
    threads=$(echo $entry | awk '{print $1}')
    avg=$(echo $entry | awk '{print $2}')
    speedup=$(echo "$serial_time / $avg" | bc -l)
    efficiency=$(echo "$speedup / $threads * 100" | bc -l)
    printf "  │ %7d │ %9.4f s │ %7.2fx  │ %8.1f%% │\n" \
        $threads $avg $speedup $efficiency
done
echo "  └─────────┴─────────────┴──────────┴────────────┘"
echo ""
