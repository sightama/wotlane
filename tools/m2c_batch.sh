#!/bin/sh
# Batch m2c drafts, outside the repo (build/m2c/<tu>/<function>.c), so you don't wait for m2c on every function. Runs in WSL.
#   sh tools/m2c_batch.sh                       the whole queue in config/boot_coverage_asm.csv (functions still in asm that the game calls)
#   sh tools/m2c_batch.sh config/other.csv      another queue (columns tu,function,...)
#   sh tools/m2c_batch.sh game/Monster [tu...]  every INCLUDE_ASM function of those TUs (asm/nonmatchings/<tu>/*.s)
# Variables: JOBS (default 8), M2C/PYTHON (as in m2c.sh). Only generates what is missing or older than the .s; failures become a .c with
# m2c's message at the end ("m2c failed"). The draft is only a starting point: read the asm (CLAUDE.md, "Per-function workflow").
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
out=build/m2c
mkdir -p "$out"
list=$(mktemp)
if [ $# -eq 0 ]; then
    set -- config/boot_coverage_asm.csv
fi
for a in "$@"; do
    case "$a" in
        *.csv) tail -n +2 "$a" | awk -F, '{print $1 "/" $2}' >> "$list" ;;
        *)     ls "asm/nonmatchings/$a"/*.s 2>/dev/null | sed "s|^asm/nonmatchings/||; s|\.s$||" >> "$list" ;;
    esac
done
total=$(wc -l < "$list")
echo "batch m2c: $total functions -> $out/ (JOBS=${JOBS:-8})"
sort -u "$list" | xargs -P "${JOBS:-8}" -I{} sh -c '
    p="{}"; tu=${p%/*}; fn=${p##*/}
    s="asm/nonmatchings/$tu/$fn.s"; c="'"$out"'/$tu/$fn.c"
    [ -f "$s" ] || exit 0
    [ -f "$c" ] && [ "$c" -nt "$s" ] && exit 0
    mkdir -p "'"$out"'/$tu"
    if ! sh tools/m2c.sh "$tu" "$fn" > "$c.tmp" 2>&1; then
        printf "\n/* m2c failed: see above */\n" >> "$c.tmp"
    fi
    mv "$c.tmp" "$c"
'
rm -f "$list"
echo "done: $(find "$out" -name '*.c' | wc -l) drafts in $out/"
