#!/bin/sh
# ABOUTME: Runs the native game in rollback's synctest (WOTM_SYNCTEST: every fight frame goes n frames back and is redone) on an AI
# ABOUTME: vs AI direct fight, then summarizes the checks, mismatches and cost lines from its log.
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/synctest.sh [--back <n>] [--match <spec>] [--step 1|2] [--seconds <s>] [--human] [--log <file>] [--build <dir>] [VAR=value...]
  --back     frames to go back at every frame (WOTM_SYNCTEST, default 2)
  --match    direct fight (WOTM_MATCH, default 3,1,1: 1 vs 1); 8 monsters: 3,1,2,4,5,7,8,9,1
  --step     fixed step in fields (WOTM_FIXEDSTEP, default 1 = 60 Hz; 2 = 30 Hz, what the server uses for 3+ players)
  --seconds  how long the game runs (default 120)
  --human    player 1 is a human who touches nothing (default: the AI plays it, WOTM_AIVSAI=1); the modern camera then follows them
  --log      the game's stderr (default <runtime>/synctest_<pid>.err.log)
  --build    build folder inside the runtime folder (default build_dev)
  VAR=value  more environment for the game (WOTM_SYNCTEST_SKIP=sound, WOTM_SYNCTEST_TRACE=<frame>, WOTM_FRAMESPIN=<ms>...)
The run uses WOTM_SEED=7 and no sound output. Prints the synctest's lines (cost every 300 frames, mismatches) and a
summary; exit status 1 when a mismatch was logged or no check ran. Needs a display (the game opens a window).
EOF
}

back=2 match=3,1,1 step=1 seconds=120 log= build=build_dev ai=1
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --back) back=$2; shift 2 ;;
        --match) match=$2; shift 2 ;;
        --step) step=$2; shift 2 ;;
        --seconds) seconds=$2; shift 2 ;;
        --human) ai=0; shift ;;
        --log) log=$2; shift 2 ;;
        --build) build=$2; shift 2 ;;
        *=*) export "$1"; shift ;;
        *) usage >&2; exit 2 ;;
    esac
done

rt=$(runtime_dir)
exe=$rt/$build/ps2xRuntime/wotm
[ -x "$exe" ] || fail "no $exe (sh tools/build_runtime.sh)"
elf=$rt/discroot/SCUS_971.97
[ -f "$elf" ] || fail "no $elf (sh tools/setup_runtime.sh)"
if [ -z "${PS2X_CD_IMAGE:-}" ]; then
    [ -f "$rt/.wotm_iso" ] || fail "no ISO: set PS2X_CD_IMAGE (or run sh tools/setup_runtime.sh)"
    PS2X_CD_IMAGE=$(cat "$rt/.wotm_iso")
fi
[ -n "$log" ] || log=$rt/synctest_$$.err.log
case $log in /*) ;; *) log=$PWD/$log ;; esac   # the game runs in discroot
need timeout caffeinate

export PS2X_CD_IMAGE PS2X_NO_VU1=1 PS2X_NO_VIF1=1 PS2X_NO_GS=1 PS2X_SCENE=1 PS2X_FMV=1
export WOTM_MATCH=$match WOTM_FIXEDSTEP=$step WOTM_SEED=7 WOTM_SYNCTEST=$back WOTM_MUTE=1
if [ "$ai" -eq 1 ]; then export WOTM_AIVSAI=1; else unset WOTM_AIVSAI; fi
echo "synctest: $back back, match $match, step $step, ${seconds}s; log $log"
(cd "$rt/discroot" && caffeinate -d -u timeout "$seconds" "$exe" "$elf" 2>"$log" >/dev/null) || true

grep -E '^\[synctest\]' "$log" | grep -v '^\[synctest\]   ' | cut -c1-400 || true
checks=$(sed -n 's/.* \([0-9][0-9]*\) checks, .*/\1/p' "$log" | awk '{s += $1} END {print s + 0}')
mismatches=$(grep -c '^\[synctest\] MISMATCH' "$log" || true)
echo "summary: $checks checks in the cost lines, $mismatches mismatch line(s)"
[ "$checks" -gt 0 ] && [ "$mismatches" -eq 0 ]
