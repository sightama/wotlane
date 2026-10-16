#!/bin/sh
# Gera recomp/nm/: o jogo traduzido pelo PS2Recomp a partir do ELF com TODAS as funcoes equivalentes compiladas no lugar das
# originais (build_nm.sh nmall). E a base do runtime "cobaia" (build_runtime.bat nm no Windows): mesmo runtime, mesmos hooks, so
# o codigo do jogo muda; se ele se comporta diferente do normal (recomp/retail) no mesmo lote, a culpa e de uma funcao reescrita.
#   sh tools/wsl/recomp_nm.sh            (WSL; precisa do recompilador em ~/wotm-recomp, ver tools/recomp_oracle/README.md)
# Saida: recomp/nm/ (no .gitignore: e codigo do jogo), com register_functions.cpp, um .cpp por funcao e os ps2_recompiled_*.h.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$HOME/wotm-recomp/work
bin=$HOME/wotm-recomp/PS2Recomp/out/build
[ -x "$bin/ps2xRecomp/ps2_recomp" ] && [ -x "$bin/ps2xAnalyzer/ps2_analyzer" ] || {
    echo "recompilador nao encontrado em $bin (siga tools/recomp_oracle/README.md, 'Preparar')"; exit 1; }
cd "$root"
# Especificacao do build_nm: toda TU com bloco NON_MATCHING, menos os simbolos de config/nm_all_exclude.txt (tu:simbolo motivo).
specs=""
for f in $(grep -l 'ifdef NON_MATCHING' src/*/*.cpp | sort); do
    tu=${f#src/}; tu=${tu%.cpp}
    ex=$(grep -v '^#' config/nm_all_exclude.txt 2>/dev/null | awk -v t="$tu" -F'[: ]' '$1 == t {print $2}' | paste -sd, -)
    case ",$ex," in *,\*,*) continue ;; esac   # "tu:*": a TU inteira fica de fora
    specs="$specs $tu${ex:+:-$ex}"
done
echo "== ELF com as equivalentes (build_nm.sh nmall, $(echo $specs | wc -w) TUs; excluidas: $(grep -vc '^#' config/nm_all_exclude.txt 2>/dev/null || echo 0))"
sh tools/wsl/build_nm.sh nmall $specs 2>&1 | tail -3
elf=build/pcsx2_test/SCUS_971.97_nmall.elf
[ -f "$elf" ] || { echo "build_nm.sh nao gerou $elf"; exit 1; }
mkdir -p "$work"
cp "$elf" "$work/nm_all.elf"
cd "$work"
if [ ! -f config_nm_all.toml ]; then
    echo "== analisador (config_nm_all.toml)"
    "$bin/ps2xAnalyzer/ps2_analyzer" nm_all.elf config_nm_all.toml
    sed -i 's|^output = .*|output = "./output_nm_all/"|' config_nm_all.toml
fi
echo "== recompilando (ps2_recomp config_nm_all.toml)"
rm -rf output_nm_all
"$bin/ps2xRecomp/ps2_recomp" config_nm_all.toml 2>&1 | tail -3
n=$(ls output_nm_all/*.cpp | wc -l)
[ "$n" -gt 1000 ] || { echo "saida suspeita: $n .cpp em output_nm_all"; exit 1; }
echo "== copiando para recomp/nm ($n .cpp)"
rm -rf "$root/recomp/nm"
mkdir -p "$root/recomp/nm"
cp output_nm_all/*.cpp output_nm_all/*.h "$root/recomp/nm/"
# O analisador toma trechos de dados do ELF nmall por codigo (sub_0040C7FC: 3 MB de .data virando 90 MB de C++, que o MSVC nao
# termina de compilar). Funcao sem nome ("sub_") com endereco fora do .text (0x100000..0x251020) e fora do .nm_extra (>= 0x8C0000,
# onde o build_nm poe as equivalentes que cresceram) e dado: sai, junto com as linhas que a registram.
cd "$root/recomp/nm"
drop=""
for f in sub_*_0x*.cpp; do
    [ -f "$f" ] || continue
    h=${f##*_0x}; a=$(printf '%d' "0x${h%.cpp}")
    if [ "$a" -ge $((0x251020)) ] && [ "$a" -lt $((0x8C0000)) ]; then drop="$drop ${f%.cpp}"; rm -f "$f"; fi
done
if [ -n "$drop" ]; then
    expr=""
    for n in $drop; do expr="$expr -e /$n/d"; done
    sed -i $expr register_functions.cpp ps2_recompiled_functions.h
    echo "== removidas $(echo $drop | wc -w) 'funcoes' que eram dados:$drop"
fi
echo "pronto: recomp/nm com $(ls "$root/recomp/nm" | wc -l) arquivos. No Windows: build_runtime.bat nm; run_live.bat -Build build_dev_nm"
