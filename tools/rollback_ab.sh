#!/bin/sh
# ABOUTME: Rollback's proof runs in real browsers (#30): input latency with rollback off and on (Chrome, Firefox, a slow game thread
# ABOUTME: through WOTM_FRAMESPIN), then the desync matrix with rollback on. Every browser is muted (the harness's launch options).
set -eu

usage() {
    cat <<EOF
usage: sh tools/rollback_ab.sh --site <web dist> [--out <dir>] [--window <n>] [--spin <ms>] [--only latency|matrix]
  --site    a web dist folder of its own (not runtime/web_dist while a build may replace it)
  --out     results (default runtime/rollback_ab/<time>): one folder per run, each with the harness's summary; log.txt has every
            run's printed lines
  --window  WOTM_ROLLBACK's cap for the "on" runs (default 2)
  --spin    WOTM_FRAMESPIN for the slow game thread runs, ms per frame (default 9)
  --only    just the latency runs or just the desync matrix
Each latency run: 2 tabs, 60 ms round trip with 10 ms jitter, 90 s; Chrome runs twice each way (its spread between runs). The
matrix: 2, 3 and 4 players x baseline, pause, stall, the last tab of each lobby in Firefox, the window fixed at --window
(WOTM_ROLLBACK_WINDOW: every PC predicts, whatever its costs afford), then 2 players x baseline, pause with every prediction
wrong (WOTM_ROLLBACK_MISPREDICT: dialogs restore through the unwind in wasm). Exit status 1 when any run failed (the others still run).
EOF
}

repo=$(cd "$(dirname "$0")/.." && pwd)
site= data= out= window=2 spin=9 only=
while [ $# -gt 0 ]; do
    case $1 in --site|--out|--window|--spin|--only) [ $# -ge 2 ] || { usage >&2; exit 2; } ;; esac
    case $1 in
        -h|--help) usage; exit 0 ;;
        --site) site=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --window) window=$2; shift 2 ;;
        --spin) spin=$2; shift 2 ;;
        --only) only=$2; shift 2 ;;
        *) usage >&2; exit 2 ;;
    esac
done
[ -n "$site" ] && [ -d "$site" ] || { echo "rollback_ab: --site <web dist folder> is required" >&2; exit 2; }
data=$repo/runtime/web_data
[ -n "$out" ] || out=$repo/runtime/rollback_ab/$(date +%Y%m%d-%H%M%S)
mkdir -p "$out"
cd "$repo/tools/browser_mp_test"
status=0

# run <name> <command...>: its printed lines go to log.txt, the last ones to the screen
run() {
    name=$1; shift
    echo "== $name ($(date +%T))" | tee -a "$out/log.txt"
    if "$@" >"$out/$name.txt" 2>&1; then r=ok; else r="FAILED ($?)"; status=1; fi
    tail -8 "$out/$name.txt" | tee -a "$out/log.txt"
    echo "-- $name: $r" | tee -a "$out/log.txt"
}

latency() {   # latency <name> <harness options...>
    name=$1; shift
    run "$name" node latency.mjs --players 2 --rtt 60 --jitter 10 --secs 90 --site "$site" --data "$data" --out "$out/$name" "$@"
}

if [ "$only" != matrix ]; then
    for i in 1 2; do
        latency lat_chrome_off_$i --browser chrome
        latency lat_chrome_on_$i --browser chrome --env "WOTM_ROLLBACK=$window"
    done
    latency lat_firefox_off --browser chrome --firefox 2
    latency lat_firefox_on --browser chrome --firefox 2 --env "WOTM_ROLLBACK=$window"
    latency lat_slow_off --browser chrome --env "WOTM_FRAMESPIN=$spin"
    latency lat_slow_on --browser chrome --env "WOTM_FRAMESPIN=$spin" --env "WOTM_ROLLBACK=$window"
fi
if [ "$only" != latency ]; then
    run desync node desync.mjs --cases 2,3,4 --scenarios baseline,pause,stall --browser chrome --firefox 1 --rtt 60 --jitter 10 \
        --site "$site" --data "$data" --env "WOTM_ROLLBACK=$window" --env "WOTM_ROLLBACK_WINDOW=$window" --out "$out/desync"
    # every prediction wrong: the pause and the round's end open on predicted frames and restore through the unwind, in wasm
    run desync_mispredict node desync.mjs --cases 2 --scenarios baseline,pause --browser chrome --firefox 1 --rtt 60 --jitter 10 \
        --site "$site" --data "$data" --env "WOTM_ROLLBACK=$window" --env "WOTM_ROLLBACK_WINDOW=$window" \
        --env WOTM_ROLLBACK_MISPREDICT=1 --out "$out/desync_mispredict"
fi
echo "results: $out"
exit "$status"
