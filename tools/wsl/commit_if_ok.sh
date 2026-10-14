#!/bin/sh
# Usage: sh tools/wsl/commit_if_ok.sh "message"  (run in WSL from repo root) -- gate, then commit only on ROM OK
cd "$(dirname "$0")/../.."
if sh tools/wsl/gate.sh > build/gate_out.txt 2>&1 && grep -q "ROM OK" build/gate_out.txt; then
    echo "ROM OK"
else
    cat build/gate_out.txt | tail -10; exit 1
fi
