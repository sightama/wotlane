#!/bin/sh
# Builds the runtime with the native layer (macOS/Linux counterpart of build_runtime.bat + tools/build_runtime.ps1): wotm (the game)
# and ps2EntryRunner (the test build with the emulated VU1/GS). Run it after every change in native/; ninja doesn't track native/,
# so this script touches ps2_runtime.cpp first. It also (re)generates <build>/wotm_gen/: the version header, the embedded resources
# and the translated VU0 microprograms (game code, outside the repo).
set -eu
. "$(dirname "$0")/posix_env.sh"

usage() {
    cat <<EOF
usage: sh tools/build_runtime.sh [build dir name]
  build dir name  folder inside the runtime folder (default: build_dev, configured by sh tools/setup_runtime.sh)
Full compiler output goes to <runtime>/logs/<build>.log; on failure the first errors are printed.
EOF
}
case ${1:-} in -h|--help) usage; exit 0 ;; esac
build=${1:-build_dev}
rt=$(runtime_dir)
bdir=$rt/$build
[ -f "$bdir/build.ninja" ] || fail "$bdir isn't configured (run sh tools/setup_runtime.sh)"
need cmake ninja python3 git

gen=$bdir/wotm_gen
mkdir -p "$gen" "$rt/logs"
touch "$rt/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp"

# build version (native/wotm_version.hpp: in the log and the window title); rewritten only when it changes, so ninja doesn't
# recompile everything that includes it
ver=$(git -C "$REPO" describe --tags --always --dirty 2>/dev/null || echo dev)
ver_text="#define WOTM_VERSION \"$ver\""
[ -f "$gen/wotm_version.gen.h" ] && [ "$(cat "$gen/wotm_version.gen.h")" = "$ver_text" ] || printf '%s\n' "$ver_text" >"$gen/wotm_version.gen.h"

# VU0 microprograms as C++ (without the .inc the runtime only uses the interpreter)
vu0=$gen/wotm_vu0_native.gen.inc
elf=$REPO/disc/SCUS_971.97
if [ -f "$elf" ] && { [ ! -f "$vu0" ] || [ "$REPO/tools/vu0native.py" -nt "$vu0" ]; }; then
    python3 "$REPO/tools/vu0native.py" "$elf" "$vu0" >"$rt/logs/vu0native.log" 2>&1 || echo "warning: vu0native.py failed, VU0 stays on the interpreter (see $rt/logs/vu0native.log)"
fi

# button atlas and font from native/wotm.rc as byte arrays (Windows reads them from the .exe resources; native/wotm_embedded.hpp)
python3 "$REPO/tools/embed_resources.py" "$REPO/native/wotm.rc" "$gen/wotm_resources.gen.h" || fail "embed_resources.py failed"

log=$rt/logs/$build.log
start=$(date +%s)
if ! cmake --build "$bdir" --target ps2EntryRunner wotm >"$log" 2>&1; then
    grep -m 40 -E 'error:|error [A-Z][0-9]+|undefined reference|Undefined symbols|ld: ' "$log" >&2 || tail -n 30 "$log" >&2
    fail "build failed after $(( $(date +%s) - start ))s; full log: $log"
fi
echo "built in $(( $(date +%s) - start ))s:"
for exe in wotm ps2EntryRunner; do echo "  $bdir/ps2xRuntime/$exe"; done
