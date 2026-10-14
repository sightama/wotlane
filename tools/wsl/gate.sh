#!/bin/sh
# Build + sha1 check; exits non-zero unless the ROM matches. Use as: sh tools/wsl/gate.sh && git commit ...
# The log lives in this checkout's build/, so gates in two worktrees can run at the same time.
root=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$root/build"
log=$root/build/gate.log
sh "$root/tools/wsl/check.sh" > "$log" 2>&1
if tail -1 "$log" | grep -q "ROM OK"; then
    echo "ROM OK"
else
    grep -n 'rror\|undefined\|FAILED' "$log" | head -10
    exit 1
fi
