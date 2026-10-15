#!/bin/sh
# Filtered side-by-side diff (relocation noise removed) of functions in the last ccmatch project build.
for f in "$@"; do
  echo "=== $f"
  sh tools/wsl/objdiff.sh "$f" "${CCMATCH_WORK:-$HOME/.cache/ccmatch/$(basename "$(cd "$(dirname "$0")/../.." && pwd)")}/project.o" | grep -nE '[|<>]' \
    | grep -vE '^[0-9]+:(jal|b[a-z]*)\s+[^|]*\|\s+(jal|b[a-z]*)\s' \
    | grep -vE '^[0-9]+:(lui|addiu|lw|sw|lwu|ld|sd)\s+[a-z0-9]+,(0x[0-9a-f]+|-?[0-9]+(\([a-z0-9]+\))?|[a-z0-9]+,-?[0-9]+)\s+\|\s+\1\s'
done
