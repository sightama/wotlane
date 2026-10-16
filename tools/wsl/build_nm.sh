#!/bin/sh
# Build an ELF with NON_MATCHING (equivalent) functions compiled in, in a scratch copy (~/wotm_nm_<name>) so the real build/ stays
# byte-identical to retail. Everything that did not change stays at its retail address (tools/gen_nm_ld.py); pieces that grew are
# moved behind the image. Output (program headers patched so p_paddr == p_vaddr, PCSX2 loads by p_paddr):
#   build/pcsx2_test/SCUS_971.97_<name>.elf             equivalents compiled in
#   build/pcsx2_test/SCUS_971.97_control_matching.elf   the normal (retail-identical) build, as a control
# Usage (WSL):  sh tools/wsl/build_nm.sh [name [spec ...]]
#   no spec            every NON_MATCHING block in the project
#   game/Monster       every NON_MATCHING block of that TU
#   game/Monster:symA,symB     only those mangled symbols of the TU
#   game/Monster:-symA,symB    every block of the TU except those
# PCSX2:  pcsx2-qt.exe -elf <elf> -- "<iso>"
set -e
name=${1:-halfcpp}
[ $# -gt 0 ] && shift
src=$(cd "$(dirname "$0")/../.." && pwd)
dst=$HOME/wotm_nm_$name
mkdir -p "$dst"
# .ninja_log/.ninja_deps ficam os da copia: com os do repo, o ninja compara os comandos com os do build matching e uma TU que sai
# da selecao (perde o -DNON_MATCHING) mantem o .o velho com as equivalentes
(cd "$src" && tar cf - --exclude=./ISO --exclude=./.git --exclude='./disc/[0-9A-Z]*' --exclude=./build \
    --exclude=./.ninja_log --exclude=./.ninja_deps .) | (cd "$dst" && tar xf -)
cd "$dst"
export PATH=$HOME/.venvs/wotm/bin:$PATH
# code pointers that splat wrote as raw .short pairs (pointer-to-member tables) must follow their function here (tools/nm_fix_data.py)
python3 "$src/tools/nm_fix_data.py"
# CrushLevel's NM code brings its own .sdata strings; -G0 keeps them out of the gp-relative area (which has no free room)
sed -i 's/\r$//' config/tu_flags.txt
echo 'game/CrushLevel -G0' >> config/tu_flags.txt
# TUs whose retail image has no .sdata of their own (or no room left in it): float literals of the equivalent code must not land in the
# small-data area either (".sdata referenced in .text ... defined in discarded section" at link time). NM_G0="game/X game/Y" adds more.
for t in game/AiAction game/Monster $NM_G0; do
    if grep -q "^$t " config/tu_flags.txt; then sed -i "s|^$t \(.*\)|$t \1 -G0|" config/tu_flags.txt; else echo "$t -G0" >> config/tu_flags.txt; fi
done
if [ $# -gt 0 ]; then
    python3 "$src/tools/nm_select.py" "$@"
    unset WOTM_EXTRA_CFLAGS
else
    export WOTM_EXTRA_CFLAGS=-DNON_MATCHING
fi
python configure.py > /dev/null
# the plain link may fail (equivalents that grew overflow the retail layout; gen_nm_ld.py links on its own), but a failed compile
# must stop here: the link below would silently take the stale .o of the previous selection
ninja build/SCUS_971.97.elf -j8 > ninja_nm.log 2>&1 || true
grep -v '^\[' ninja_nm.log | grep -v warning | head -30
if grep -q '^FAILED: .*\.o' ninja_nm.log; then echo "build_nm: compile failed ($dst/ninja_nm.log)"; exit 1; fi
python3 tools/gen_nm_ld.py "$src" "$dst"
mips-linux-gnu-ld -EL --no-check-sections -T SCUS_971.97.nm.ld -T undefined_syms_nm.txt -T undefined_funcs_nm.txt \
    -T linker_script_extra.ld -Map nm.map -o build/SCUS_971.97.nm.elf 2>&1 | grep -v RWX || true
mkdir -p "$src/build/pcsx2_test"
# NM_HEAP=0x8E0000 fixes the heap start for every variant (and for the control), so RAM dumps of two builds can be compared word by word
heap=${NM_HEAP:-0x$(mips-linux-gnu-nm build/SCUS_971.97.nm.elf | awk '$3 == "nm_EXTRA_END" { print $1 }')}
python3 "$src/tools/patch_paddr.py" "$dst/build/SCUS_971.97.nm.elf" "$src/build/pcsx2_test/SCUS_971.97_$name.elf" --heap "$heap"
python3 "$src/tools/patch_paddr.py" "$src/build/SCUS_971.97.elf" "$src/build/pcsx2_test/SCUS_971.97_control_matching.elf" ${NM_HEAP:+--heap "$NM_HEAP"}
