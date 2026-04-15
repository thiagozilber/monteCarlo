#!/bin/bash

export LC_NUMERIC=C

PROGRAM="./nbody-naive"
THREAD_COUNTS=(1 2 4 8 16)
RUN_SERIAL=false
SCHED="static"
CHUNK=""

positional=()
for arg in "$@"; do
    if [[ "$arg" == "--smart" ]]; then
        PROGRAM="./nbody-smart"
    elif [[ "$arg" == "--naive" ]]; then
        PROGRAM="./nbody-naive"
    elif [[ "$arg" == "--serial" ]]; then
        RUN_SERIAL=true
    elif [[ "$arg" == --schedule=* ]]; then
        SCHED="${arg#--schedule=}"
    elif [[ "$arg" == --chunk=* ]]; then
        CHUNK="${arg#--chunk=}"
    else
        positional+=("$arg")
    fi
done

if [ -z "$CHUNK" ]; then
    SCHED_DEF="-DSCHED_TYPE=$SCHED"
else
    SCHED_DEF="-DSCHED_TYPE=$SCHED -DCHUNK_SIZE=$CHUNK"
fi

#compile
gcc -O2 -Wall -fopenmp $SCHED_DEF -o nbody-naive nbody-naive.c -lm

NUM_BODIES=${positional[0]:-8000}
NUM_STEPS=${positional[1]:-100}
RUNS=${positional[2]:-5}

# Colors
CYAN='\033[0;36m'
BOLD='\033[1m'
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
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

# Run program once, echo "time ke"
run_once() {
    local out=$($@ 2>/dev/null)
    local t=$(echo "$out" | grep "Time" | awk '{print $4}')
    local ke=$(echo "$out" | grep "Final KE" | awk '{print $4}')
    echo "$t $ke"
}

# Average a list of plain decimals (timing values) — bc is fine here
avg_bc() {
    local vals=("$@")
    local total=0
    for v in "${vals[@]}"; do
        total=$(echo "$total + $v" | bc -l)
    done
    echo "scale=10; $total / ${#vals[@]}" | bc -l
}

# Average a list of values that may be scientific notation — use python3
avg_py() {
    local joined=$(IFS=,; echo "$*")
    # Remove any empty elements caused by failed parses
    joined=$(echo "$joined" | tr -s ',' | sed 's/^,//;s/,$//')
    python3 -c "vals=[$joined]; print('%.6e' % (sum(vals)/len(vals)))"
}

# Average absolute diff from a reference — python3 for scientific notation
avg_diff_py() {
    local ref=$1
    shift
    local joined=$(IFS=,; echo "$*")
    joined=$(echo "$joined" | tr -s ',' | sed 's/^,//;s/,$//')
    python3 -c "
ref=$ref
vals=[$joined]
print('%.6e' % (sum(abs(v-ref) for v in vals)/len(vals)))
"
}
echo ""

# ── Serial reference ──────────────────────────────────────────────────────────
if [ "$RUN_SERIAL" = true ]; then
    echo -e "${YELLOW}  Running serial reference...${RESET}"
    serial_times=()
    serial_kes=()
    serial_first_ke=""

    for run in $(seq 1 $RUNS); do
        current_run=$(( current_run + 1 ))
        print_progress "serial  run=$run/$RUNS"
        result=$(run_once ./nbody $NUM_BODIES $NUM_STEPS)
        t=$(echo $result | awk '{print $1}')
        ke=$(echo $result | awk '{print $2}')
        serial_times+=("$t")
        serial_kes+=("$ke")
        if [ $run -eq 1 ]; then serial_first_ke=$ke; fi
    done

    serial_time=$(avg_bc "${serial_times[@]}")
    serial_ke_avg=$(avg_py "${serial_kes[@]}")
    serial_ke_diff=$(avg_diff_py $serial_first_ke "${serial_kes[@]}")

    printf "\r  ✓  Serial reference done\n\n"
    echo -e "${GREEN}  Serial Result Run #1 : $serial_first_ke J${RESET}"
    printf "  Avg Result Diff      : %s J  (across %d runs)\n\n" \
        $serial_ke_diff $RUNS
else
    echo -e "${YELLOW}  Skipping serial reference...${RESET}"
fi

# ── Parallel runs ─────────────────────────────────────────────────────────────
par_results=()
par_ke_summaries=()

for threads in "${THREAD_COUNTS[@]}"; do
    if [ "$threads" -eq 1 ]; then continue; fi
    times=()
    kes=()
    first_ke=""

    for run in $(seq 1 $RUNS); do
        current_run=$(( current_run + 1 ))
        print_progress "threads=$(printf '%-2d' $threads)  run=$run/$RUNS"
        result=$(run_once $PROGRAM $NUM_BODIES $NUM_STEPS $threads)
        t=$(echo $result | awk '{print $1}')
        ke=$(echo $result | awk '{print $2}')
        times+=("$t")
        kes+=("$ke")
        if [ $run -eq 1 ]; then first_ke=$ke; fi
    done

    avg_t=$(avg_bc "${times[@]}")
    ke_diff=$(avg_diff_py $first_ke "${kes[@]}")
    ke_diff_serial=$(avg_diff_py $serial_ke_avg "${kes[@]}")

    par_results+=("$threads $avg_t")
    par_ke_summaries+=("$threads $first_ke $ke_diff $ke_diff_serial")
done

printf "\r%80s\r" ""
echo ""

# ── Result consistency ────────────────────────────────────────────────────────
echo -e "${BOLD}Result Consistency:${RESET}"
echo ""
printf "  %-10s  %-18s  %-24s  %-24s\n" \
    "Threads" "First Run KE (J)" "Avg Diff within runs" "Avg Diff vs Serial"
printf "  %-10s  %-18s  %-24s  %-24s\n" \
    "----------" "------------------" "------------------------" "------------------------"
printf "  %-10s  %-18s  %-24s  %-24s\n" \
    "serial" "$serial_first_ke" "$serial_ke_diff" "—"

for entry in "${par_ke_summaries[@]}"; do
    threads=$(echo $entry | awk '{print $1}')
    first_ke=$(echo $entry | awk '{print $2}')
    ke_diff=$(echo $entry | awk '{print $3}')
    ke_diff_serial=$(echo $entry | awk '{print $4}')
    printf "  %-10s  %-18s  %-24s  %-24s\n" \
        "$threads" "$first_ke" "$ke_diff" "$ke_diff_serial"
done

echo ""

# ── Performance table ─────────────────────────────────────────────────────────
echo -e "${BOLD}Performance:${RESET}"
if [ "$RUN_SERIAL" = true ]; then
    echo "  ┌─────────┬─────────────┬──────────┬────────────┐"
    echo "  │ Threads │  Avg Time   │ Speedup  │ Efficiency │"
    echo "  ├─────────┼─────────────┼──────────┼────────────┤"
else
    echo "  ┌─────────┬─────────────┐"
    echo "  │ Threads │  Avg Time   │"
    echo "  ├─────────┼─────────────┤"
fi
if [ "$RUN_SERIAL" = true ]; then
    printf "  │ %7d │ %9.4f s │ %7.2fx  │ %8.1f%% │\n" \
        1 $serial_time 1.00 100.0
    for entry in "${par_results[@]}"; do
        threads=$(echo $entry | awk '{print $1}')
        avg=$(echo $entry | awk '{print $2}')
        speedup=$(echo "$serial_time / $avg" | bc -l)
        efficiency=$(echo "$speedup / $threads * 100" | bc -l)
        printf "  │ %7d │ %9.4f s │ %7.2fx  │ %8.1f%% │\n" \
            $threads $avg $speedup $efficiency
    done
else
    for entry in "${par_results[@]}"; do
        threads=$(echo $entry | awk '{print $1}')
        avg=$(echo $entry | awk '{print $2}')
        printf "  │ %7d │ %9.4f s │\n" $threads $avg
    done
fi
if [ "$RUN_SERIAL" = true ]; then
    echo "  └─────────┴─────────────┴──────────┴────────────┘"
else
    echo "  └─────────┴─────────────┘"
fi
echo ""