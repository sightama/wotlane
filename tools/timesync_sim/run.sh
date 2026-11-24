#!/bin/sh
# ABOUTME: Builds and runs tools/timesync_sim (the lockstep pacing simulation behind docs/investigacoes/2026-11-24/time-sync) with the
# ABOUTME: system C++ compiler; every argument goes to the simulation (--help lists them; no arguments prints the scenario table).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${TMPDIR:-/tmp}/timesync_sim
${CXX:-c++} -std=c++17 -O2 -Wall -Wextra -o "$out" "$here/timesync_sim.cpp" || { echo "timesync_sim: build failed" >&2; exit 1; }
exec "$out" "$@"
