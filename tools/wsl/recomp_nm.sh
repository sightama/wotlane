#!/bin/sh
# Generates recomp/nm/: the game translated by PS2Recomp from the ELF with ALL equivalent functions compiled in place of the
# originals (build_nm.sh nmall). It is the base of the "guinea pig" runtime (build_runtime.bat nm on Windows): same runtime, same hooks,
# only the game code changes; if it behaves differently from the normal one (recomp/retail) in the same batch, a rewritten function is to blame.
#   sh tools/wsl/recomp_nm.sh            (WSL; needs the recompiler in ~/wotm-recomp, see tools/recomp_oracle/README.md)
# Output: recomp/nm/ (in .gitignore: it is game code), with register_functions.cpp, one .cpp per function and the ps2_recompiled_*.h.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$HOME/wotm-recomp/work
mkdir -p "$work"
bin=$HOME/wotm-recomp/PS2Recomp/out/build
[ -x "$bin/ps2xRecomp/ps2_recomp" ] && [ -x "$bin/ps2xAnalyzer/ps2_analyzer" ] || {
    echo "recompiler not found in $bin (follow tools/recomp_oracle/README.md, 'Preparar')"; exit 1; }
cd "$root"
# build_nm spec: every TU with a NON_MATCHING block, minus the symbols in config/nm_all_exclude.txt (tu:symbol reason).
# NM_ALL_EXCLUDE=<file> swaps the list (bisection: does the guinea pig hang? exclude half the TUs with "tu:*" and see if it still does).
excl=${NM_ALL_EXCLUDE:-config/nm_all_exclude.txt}
specs=""
for f in $(grep -l 'ifdef NON_MATCHING' src/*/*.cpp | sort); do
    tu=${f#src/}; tu=${tu%.cpp}
    ex=$(grep -v "^#" "$excl" 2>/dev/null | awk -v t="$tu" -F'[: ]' '$1 == t {print $2}' | paste -sd, -)
    case ",$ex," in *,\*,*) continue ;; esac   # "tu:*": the whole TU is left out
    specs="$specs $tu${ex:+:-$ex}"
done
echo "== ELF with the equivalents (build_nm.sh nmall, $(echo $specs | wc -w) TUs; excluded: $(grep -vc "^#" "$excl" 2>/dev/null || echo 0))"
sh tools/wsl/build_nm.sh nmall $specs > "$work/build_nm.log" 2>&1 || { tail -30 "$work/build_nm.log"; exit 1; }
tail -3 "$work/build_nm.log"
elf=build/pcsx2_test/SCUS_971.97_nmall.elf
[ -f "$elf" ] || { echo "build_nm.sh did not produce $elf"; exit 1; }
mkdir -p "$work"
cp "$elf" "$work/nm_all.elf"
cd "$work"
if [ ! -f config_nm_all.toml ]; then
    echo "== analyzer (config_nm_all.toml)"
    "$bin/ps2xAnalyzer/ps2_analyzer" nm_all.elf config_nm_all.toml
    sed -i 's|^output = .*|output = "./output_nm_all/"|' config_nm_all.toml
fi
# The analyzer puts in [patches] the instructions it thinks are "self-modifying code" and the recompiler turns them into nops: in the
# nmall ELF there are ~90, among them the sw of McBootPage's vtable in MonsterMc's static constructor (0x16C5C4), and boot hangs on the
# memory card screen. The retail config.toml has no such section; the game doesn't modify itself (the "code" being written is data).
sed -i '/^\[patches\]/,/^\[performance\]/{/^\[performance\]/!d}' config_nm_all.toml
echo "== recompiling (ps2_recomp config_nm_all.toml)"
rm -rf output_nm_all
"$bin/ps2xRecomp/ps2_recomp" config_nm_all.toml 2>&1 | tail -3
n=$(ls output_nm_all/*.cpp | wc -l)
[ "$n" -gt 1000 ] || { echo "suspicious output: $n .cpp in output_nm_all"; exit 1; }
# The analyzer mistakes chunks of data in the nmall ELF for code (sub_0040C7FC: 3 MB of .data turning into 90 MB of C++ that MSVC never
# finishes compiling). An unnamed function ("sub_") outside .text (0x100000..0x251020) and outside .nm_extra (where build_nm puts the
# equivalents that grew) is data: it is removed, along with the lines that register it.
extra=$(mips-linux-gnu-nm nm_all.elf | awk '$3 == "nm_EXTRA_START" { print $1 }')
drop=""
for f in output_nm_all/sub_*_0x*.cpp; do
    [ -f "$f" ] || continue
    h=${f##*_0x}; a=$(printf '%d' "0x${h%.cpp}")
    if [ "$a" -ge $((0x251020)) ] && [ "$a" -lt $((0x$extra)) ]; then g=${f##*/}; drop="$drop ${g%.cpp}"; rm -f "$f"; fi
done
if [ -n "$drop" ]; then
    expr=""
    for d in $drop; do expr="$expr -e /$d/d"; done
    sed -i $expr output_nm_all/register_functions.cpp output_nm_all/ps2_recompiled_functions.h
    echo "== removed $(echo $drop | wc -w) 'functions' that were data:$drop"
fi
# The runtime loads the data (and .ctors, which points into the code) from the ELF it gets on the command line: the guinea pig must get
# this one, not the retail one from the disc (run_live.ps1 -Build build_dev_nm copies it into discroot).
cp nm_all.elf output_nm_all/SCUS_971.97_nmall.elf
# Native-layer hooks and reads by name (wotm::fn / wotm::sym) with this ELF's addresses: the equivalents that grew live in
# .nm_extra. build_runtime.ps1 copies the header to build_dev_nm\wotm_gen, where native/wotm_syms.h finds it.
mips-linux-gnu-nm nm_all.elf > nm_all.syms
python3 "$root/tools/native_syms.py" --nm nm_all.syms output_nm_all/wotm_syms_nm.h
# Only the files that changed go to recomp/nm: ninja rebuilds by timestamp, and swapping one TU changes few functions (the rest stay
# at their retail address). So a bisection step (NM_ALL_EXCLUDE) recompiles for minutes instead of the whole game.
mkdir -p "$root/recomp/nm"
changed=0
for f in output_nm_all/*; do
    g="$root/recomp/nm/${f##*/}"
    cmp -s "$f" "$g" || { cp "$f" "$g"; changed=$((changed + 1)); }
done
for g in "$root"/recomp/nm/*; do [ -e "output_nm_all/${g##*/}" ] || rm -f "$g"; done
echo "done: recomp/nm has $(ls "$root/recomp/nm" | wc -l) files ($changed changed). On Windows: build_runtime.bat nm; run_live.bat -Build build_dev_nm"
