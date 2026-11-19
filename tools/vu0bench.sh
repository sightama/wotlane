#!/bin/sh
# Checks that the native VU0 microprograms (tools/vu0native.py -> wotm_vu0_native.gen.inc) give bit-identical results to the VU0
# interpreter on this machine (macOS/Linux). Use it after changing wotm_vu0_native.cpp or vu0native.py, and on any new compiler or
# CPU (arm64 goes through sse2neon). It plays an AI-vs-AI fight in the ps2EntryRunner test build with PS2X_DUMP_VU0=1 (the interpreter
# records up to 60 samples per microprogram once 300k calls have run), replays every sample through the native code with
# tools/vu0bench.cpp, and deletes the dumps (they contain game state).
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/vu0bench.sh [--secs N] [--build <dir>] [--wasm]
  --secs   how long the fight records samples (default 150; fewer than ~120 s may not reach 300k VU0 calls)
  --build  build folder in the runtime folder (default build_dev; sh tools/build_runtime.sh first)
  --wasm   also replay the samples through the WebAssembly build of the native VU0 (Emscripten in EMSDK or ~/emsdk, run
           under node), which has no rounding-mode control and must still give the interpreter's bits
Prints one line per microprogram and a total; exits 1 on any mismatch. Game log: <runtime>/live.err.log.
EOF
}
secs=150 build=build_dev wasm=
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --secs) secs=$2; shift 2 ;;
        --build) build=$2; shift 2 ;;
        --wasm) wasm=1; shift ;;
        *) usage >&2; exit 2 ;;
    esac
done
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
# compiled like the runtime: no FMA contraction; sse2neon on ARM (the runtime build fetched it), the runtime's x86 flags otherwise
set -- -std=c++20 -O2 -ffp-contract=off -fno-fast-math
if [ -f "$sse2neon/sse2neon.h" ]; then set -- "$@" -DUSE_SSE2NEON -DSSE2NEON_PRECISE_MINMAX=1 "-I$sse2neon"   # the runtime's definitions (wotm-runtime.patch)
else set -- "$@" -mavx2 -mbmi2 -mlzcnt -mpopcnt; fi
run_logged "$rt/logs/vu0bench_build.log" c++ "$@" -I"$REPO/native" -I"$REPO/native/runtime" -I"$rt/PS2Recomp/ps2xRuntime/include" -I"$gen" \
    "$REPO/tools/vu0bench.cpp" "$REPO/native/runtime/wotm_vu0_native.cpp" -o "$bench"
if [ -n "$wasm" ]; then
    step 'build vu0bench (wasm)'
    use_emsdk "${EMSDK:-$HOME/emsdk}" || fail "no Emscripten (set EMSDK)"
    need em++ node
    # the browser build's flags for this file (tools/build_web.sh), and node reading the dumps straight from disk
    run_logged "$rt/logs/vu0bench_wasm_build.log" em++ -std=c++20 -O2 -ffp-contract=off -fno-fast-math -msimd128 -msse4.1 \
        -sNODERAWFS=1 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -I"$REPO/native" -I"$REPO/native/runtime" \
        -I"$rt/PS2Recomp/ps2xRuntime/include" -I"$gen" "$REPO/tools/vu0bench.cpp" "$REPO/native/runtime/wotm_vu0_native.cpp" \
        -o "$bench.js"
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
WOTM_MATCH=3,1,1 WOTM_AIVSAI=1 PS2X_DUMP_VU0=1 sh "$rt/run_live.sh" --auto 0 --exe test --build "$build" >/dev/null 2>&1 &
game=$!
sleep "$secs"
stop_game
[ -s "$disc/vu0_samples.bin" ] || fail "no samples recorded (fewer than 300k VU0 calls? try --secs 240); log: $rt/live.err.log"

step replay
status=0
"$bench" "$disc/vu0_code.bin" "$disc/vu0_samples.bin" || status=$?
if [ -n "$wasm" ]; then
    step 'replay (wasm)'
    node "$bench.js" "$disc/vu0_code.bin" "$disc/vu0_samples.bin" || status=$?
fi
exit $status   # the EXIT trap deletes the dumps
