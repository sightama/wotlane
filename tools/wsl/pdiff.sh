sh tools/wsl/objdiff.sh "$1" "${CCMATCH_WORK:-$HOME/.cache/ccmatch/$(basename "$(cd "$(dirname "$0")/../.." && pwd)")}/project.o"
