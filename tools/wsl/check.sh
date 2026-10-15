#!/bin/sh
# Build and verify the ROM against the retail checksum; exit non-zero on any failure (safe to gate commits on).
cd "$(dirname "$0")/../.." || exit 1
mkdir -p build
ninja >build/ninja.log 2>&1 || { tail -15 build/ninja.log; echo "BUILD FAILED"; exit 1; }
sha1sum -c SCUS_971.97.sha1 2>&1 | tail -1 | grep -q OK && echo "ROM OK" || { echo "ROM MISMATCH"; exit 1; }
