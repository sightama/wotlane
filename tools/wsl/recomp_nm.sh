#!/bin/sh
# Gera recomp/nm/: o jogo traduzido pelo PS2Recomp a partir do ELF com TODAS as funcoes equivalentes compiladas no lugar das
# originais (build_nm.sh nmall). E a base do runtime "cobaia" (build_runtime.bat nm no Windows): mesmo runtime, mesmos hooks, so
# o codigo do jogo muda; se ele se comporta diferente do normal (recomp/retail) no mesmo lote, a culpa e de uma funcao reescrita.
#   sh tools/wsl/recomp_nm.sh            (WSL; precisa do recompilador em ~/wotm-recomp, ver tools/recomp_oracle/README.md)
# Saida: recomp/nm/ (no .gitignore: e codigo do jogo), com register_functions.cpp, um .cpp por funcao e os ps2_recompiled_*.h.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$HOME/wotm-recomp/work
mkdir -p "$work"
bin=$HOME/wotm-recomp/PS2Recomp/out/build
[ -x "$bin/ps2xRecomp/ps2_recomp" ] && [ -x "$bin/ps2xAnalyzer/ps2_analyzer" ] || {
    echo "recompilador nao encontrado em $bin (siga tools/recomp_oracle/README.md, 'Preparar')"; exit 1; }
cd "$root"
# Especificacao do build_nm: toda TU com bloco NON_MATCHING, menos os simbolos de config/nm_all_exclude.txt (tu:simbolo motivo).
# NM_ALL_EXCLUDE=<arquivo> troca a lista (bisseccao: o cobaia trava? exclua metade das TUs com "tu:*" e veja se continua).
excl=${NM_ALL_EXCLUDE:-config/nm_all_exclude.txt}
specs=""
for f in $(grep -l 'ifdef NON_MATCHING' src/*/*.cpp | sort); do
    tu=${f#src/}; tu=${tu%.cpp}
    ex=$(grep -v "^#" "$excl" 2>/dev/null | awk -v t="$tu" -F'[: ]' '$1 == t {print $2}' | paste -sd, -)
    case ",$ex," in *,\*,*) continue ;; esac   # "tu:*": a TU inteira fica de fora
    specs="$specs $tu${ex:+:-$ex}"
done
echo "== ELF com as equivalentes (build_nm.sh nmall, $(echo $specs | wc -w) TUs; excluidas: $(grep -vc "^#" "$excl" 2>/dev/null || echo 0))"
sh tools/wsl/build_nm.sh nmall $specs > "$work/build_nm.log" 2>&1 || { tail -30 "$work/build_nm.log"; exit 1; }
tail -3 "$work/build_nm.log"
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
# O analisador poe em [patches] as instrucoes que acha serem "codigo automodificavel" e o recompilador as troca por nop: no ELF
# nmall sao ~90, entre elas o sw da vtable do McBootPage no construtor estatico do MonsterMc (0x16C5C4), e o boot trava na tela
# do cartao de memoria. O config.toml do retail nao tem a secao; o jogo nao se automodifica (o "codigo" escrito e dado).
sed -i '/^\[patches\]/,/^\[performance\]/{/^\[performance\]/!d}' config_nm_all.toml
echo "== recompilando (ps2_recomp config_nm_all.toml)"
rm -rf output_nm_all
"$bin/ps2xRecomp/ps2_recomp" config_nm_all.toml 2>&1 | tail -3
n=$(ls output_nm_all/*.cpp | wc -l)
[ "$n" -gt 1000 ] || { echo "saida suspeita: $n .cpp em output_nm_all"; exit 1; }
# O analisador toma trechos de dados do ELF nmall por codigo (sub_0040C7FC: 3 MB de .data virando 90 MB de C++, que o MSVC nao
# termina de compilar). Funcao sem nome ("sub_") fora do .text (0x100000..0x251020) e fora do .nm_extra (onde o build_nm poe as
# equivalentes que cresceram) e dado: sai, junto com as linhas que a registram.
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
    echo "== removidas $(echo $drop | wc -w) 'funcoes' que eram dados:$drop"
fi
# O runtime carrega os dados (e a .ctors, que aponta o codigo) do ELF que recebe na linha de comando: o cobaia tem de receber
# este, nao o retail do disco (run_live.ps1 -Build build_dev_nm o copia para o discroot).
cp nm_all.elf output_nm_all/SCUS_971.97_nmall.elf
# Hooks e leituras da camada nativa por nome (wotm::fn / wotm::sym) com os enderecos deste ELF: as equivalentes que cresceram moram
# no .nm_extra. O build_runtime.ps1 copia o header para build_dev_nm\wotm_gen, onde o native/wotm_syms.h o acha.
mips-linux-gnu-nm nm_all.elf > nm_all.syms
python3 "$root/tools/native_syms.py" --nm nm_all.syms output_nm_all/wotm_syms_nm.h
# So os arquivos que mudaram vao para recomp/nm: o ninja recompila pelo horario, e trocar uma TU muda poucas funcoes (o resto fica
# no endereco retail). Assim uma bisseccao (NM_ALL_EXCLUDE) recompila minutos em vez do jogo inteiro.
mkdir -p "$root/recomp/nm"
changed=0
for f in output_nm_all/*; do
    g="$root/recomp/nm/${f##*/}"
    cmp -s "$f" "$g" || { cp "$f" "$g"; changed=$((changed + 1)); }
done
for g in "$root"/recomp/nm/*; do [ -e "output_nm_all/${g##*/}" ] || rm -f "$g"; done
echo "pronto: recomp/nm com $(ls "$root/recomp/nm" | wc -l) arquivos ($changed mudaram). No Windows: build_runtime.bat nm; run_live.bat -Build build_dev_nm"
