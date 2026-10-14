#!/bin/sh
# Score every C++ game TU (scoreall.sh lines, sorted), N at a time, each with its own ccmatch work dir. Diff two runs to see what a
# shared-header change or a merge did to the other TUs:
#   sh tools/wsl/scoreall_game.sh > ~/before.txt; (merge / edit); sh tools/wsl/scoreall_game.sh > ~/after.txt; diff ~/before.txt ~/after.txt
# Usage: sh tools/wsl/scoreall_game.sh [jobs, default 8]
cd "$(dirname "$0")/../.."
base=$HOME/.cache/ccmatch/$(basename "$PWD")-par
ls src/game/*.cpp | sed 's|src/game/||; s|\.cpp$||' |
    xargs -P "${1:-8}" -I{} env CCMATCH_WORK="$base/{}" sh tools/wsl/scoreall.sh {} | sort
