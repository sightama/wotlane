#!/bin/sh
# Checks that the native VU0 microprograms (tools/vu0native.py -> wotm_vu0_native.gen.inc) give bit-identical results to the VU0
# interpreter on this machine (macOS/Linux). Use it after changing wotm_vu0_native.cpp or vu0native.py, and on any new compiler or
# CPU (arm64 goes through sse2neon). It plays an AI-vs-AI fight in the ps2EntryRunner test build with PS2X_DUMP_VU0=1 (the interpreter
# records up to 60 samples per microprogram once 300k calls have run), replays every sample through the native code with
# tools/vu0bench.cpp, and deletes the dumps (they contain game state). With --time it also times the native code on those samples, and
# with --ref the same code from another commit (e.g. before a change to wotm_vu0_native.cpp), on the same samples.
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/vu0bench.sh [--secs N] [--build <dir>] [--wasm] [--time N] [--ref <commit>]
  --secs   how long the fight records samples (default 150; fewer than ~120 s may not reach 300k VU0 calls)
  --build  build folder in the runtime folder (default build_dev; sh tools/build_runtime.sh first)
  --wasm   also replay the samples through the WebAssembly build of the native VU0 (Emscripten in EMSDK or ~/emsdk, run
           under node), which has no rounding-mode control and must still give the interpreter's bits
  --time   after the check, run every sample N more times and print the time per call of each microprogram
  --ref    with --time: also build that commit's native VU0 code (wotm_vu0_native.cpp and its headers) and time it on the
           same samples, native and (with --wasm) wasm; prints both, so a change's speedup is measured on one machine
Prints one line per microprogram and a total; exits 1 on any mismatch. Game log: <runtime>/live.err.log.
EOF
}
secs=150 build=build_dev wasm= repeats= ref=
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --secs) secs=$2; shift 2 ;;
        --build) build=$2; shift 2 ;;
        --wasm) wasm=1; shift ;;
        --time) repeats=$2; shift 2 ;;
        --ref) ref=$2; shift 2 ;;
        *) usage >&2; exit 2 ;;
    esac
done
[ -z "$ref" ] || [ -n "$repeats" ] || fail "--ref needs --time"
rt=$(runtime_dir)
sse2neon=$rt/$build/_deps/sse2neon-src
need c++ python3

# the microprograms from this checkout's tools/vu0native.py (the build's copy may come from an older one); game code, outside the repo
step 'generate the VU0 microprograms'
gen=$rt/logs/vu0bench_gen
elf=$REPO/disc/SCUS_971.97
[ -f "$elf" ] || elf=$rt/discroot/SCUS_971.97
[ -f "$elf" ] || fail "no ELF in $REPO/disc or $rt/discroot"
mkdir -p "$gen"
run_logged "$rt/logs/vu0bench_gen.log" python3 "$REPO/tools/vu0native.py" "$elf" "$gen/wotm_vu0_native.gen.inc"

step 'build vu0bench'
bench=$rt/logs/vu0bench
# the native VU0 code of a commit: its wotm_vu0_native.cpp and headers in a folder searched before the checkout's
if [ -n "$ref" ]; then
    refsrc=$rt/logs/vu0bench_ref
    rm -rf "$refsrc" && mkdir -p "$refsrc/runtime"
    for f in runtime/wotm_vu0_native.cpp runtime/wotm_vu0_native.h wotm_rtz.hpp wotm_rtz_lanes.hpp; do
        git -C "$REPO" show "$ref:native/$f" >"$refsrc/$f" 2>/dev/null || rm -f "$refsrc/$f"
    done
    [ -f "$refsrc/runtime/wotm_vu0_native.cpp" ] || fail "no native/runtime/wotm_vu0_native.cpp at $ref"
fi
# compiled like the runtime: no FMA contraction; sse2neon on ARM (the runtime build fetched it), the runtime's x86 flags otherwise
set -- -std=c++20 -O2 -ffp-contract=off -fno-fast-math
if [ -f "$sse2neon/sse2neon.h" ]; then set -- "$@" -DUSE_SSE2NEON -DSSE2NEON_PRECISE_MINMAX=1 "-I$sse2neon"   # the runtime's definitions (wotm-runtime.patch)
else set -- "$@" -mavx2 -mbmi2 -mlzcnt -mpopcnt; fi
# build_bench <compiler> <log> <source folder: native/ layout> <output> [flags]
build_bench() {
    cc=$1 log=$2 src=$3 out=$4
    shift 4
    run_logged "$log" "$cc" "$@" -I"$src" -I"$src/runtime" -I"$REPO/native" -I"$REPO/native/runtime" \
        -I"$rt/PS2Recomp/ps2xRuntime/include" -I"$gen" "$REPO/tools/vu0bench.cpp" "$src/runtime/wotm_vu0_native.cpp" -o "$out"
}
build_bench c++ "$rt/logs/vu0bench_build.log" "$REPO/native" "$bench" "$@"
[ -z "$ref" ] || build_bench c++ "$rt/logs/vu0bench_ref_build.log" "$refsrc" "$bench.ref" "$@"
if [ -n "$wasm" ]; then
    step 'build vu0bench (wasm)'
    use_emsdk "${EMSDK:-$HOME/emsdk}" || fail "no Emscripten (set EMSDK)"
    need em++ node
    # the browser build's flags for this file (tools/build_web.sh), and node reading the dumps straight from disk
    set -- -std=c++20 -O2 -ffp-contract=off -fno-fast-math -msimd128 -msse4.1 -sNODERAWFS=1 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1
    build_bench em++ "$rt/logs/vu0bench_wasm_build.log" "$REPO/native" "$bench.js" "$@"
    [ -z "$ref" ] || build_bench em++ "$rt/logs/vu0bench_ref_wasm_build.log" "$refsrc" "$bench.ref.js" "$@"
fi

step "record samples (${secs}s AI fight in ps2EntryRunner)"
disc=$rt/discroot
game=
# stop the game and delete the dumps (game state) on every exit, Ctrl-C included
stop_game() {   # every step may find nothing to stop (|| true: set -e would abort the trap before the dumps are deleted)
    [ -z "$game" ] || kill "$game" 2>/dev/null || true
    pkill -f "$rt/$build/ps2xRuntime/ps2EntryRunner" 2>/dev/null || true
    [ -z "$game" ] || wait "$game" 2>/dev/null || true
    game=
}
trap 'stop_game; rm -f "$disc/vu0_samples.bin" "$disc/vu0_code.bin"' EXIT
trap 'exit 130' INT TERM
rm -f "$disc/vu0_samples.bin" "$disc/vu0_code.bin"
WOTM_MATCH=3,1,1 WOTM_AIVSAI=1 WOTM_MUTE=1 PS2X_DUMP_VU0=1 sh "$rt/run_live.sh" --auto 0 --exe test --build "$build" >/dev/null 2>&1 &
game=$!
sleep "$secs"
stop_game
[ -s "$disc/vu0_samples.bin" ] || fail "no samples recorded (fewer than 300k VU0 calls? try --secs 240); log: $rt/live.err.log"

step replay
status=0
# replay <label> <command...>: the check (and the timing) of one build on the samples
replay() {
    label=$1
    shift
    step "$label"
    "$@" "$disc/vu0_code.bin" "$disc/vu0_samples.bin" $repeats || status=$?
}
# with --ref the two versions alternate over 3 rounds, so a change in the machine's load hits both alike
rounds=1
[ -z "$ref" ] || rounds=3
round=0
while [ $round -lt $rounds ]; do
    round=$((round + 1))
    replay "replay (round $round)" "$bench"
    [ -z "$ref" ] || replay "replay at $ref (round $round)" "$bench.ref"
    if [ -n "$wasm" ]; then
        replay "replay, wasm (round $round)" node "$bench.js"
        [ -z "$ref" ] || replay "replay at $ref, wasm (round $round)" node "$bench.ref.js"
    fi
done
exit $status   # the EXIT trap deletes the dumps
