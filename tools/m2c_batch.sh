#!/bin/sh
# Rascunhos do m2c em lote, fora do repo (build/m2c/<tu>/<funcao>.c), para nao esperar o m2c a cada funcao. Roda no WSL.
#   sh tools/m2c_batch.sh                       toda a fila de config/boot_coverage_asm.csv (funcoes ainda em asm que o jogo chama)
#   sh tools/m2c_batch.sh config/outra.csv      outra fila (colunas tu,function,...)
#   sh tools/m2c_batch.sh game/Monster [tu...]  todas as funcoes INCLUDE_ASM dessas TUs (asm/nonmatchings/<tu>/*.s)
# Variaveis: JOBS (padrao 8), M2C/PYTHON (como no m2c.sh). So gera o que falta ou esta mais velho que o .s; falhas viram .c com a
# mensagem do m2c no fim ("m2c falhou"). O rascunho e so um ponto de partida: leia o asm (CLAUDE.md, "Fluxo por funcao").
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
echo "m2c em lote: $total funcoes -> $out/ (JOBS=${JOBS:-8})"
sort -u "$list" | xargs -P "${JOBS:-8}" -I{} sh -c '
    p="{}"; tu=${p%/*}; fn=${p##*/}
    s="asm/nonmatchings/$tu/$fn.s"; c="'"$out"'/$tu/$fn.c"
    [ -f "$s" ] || exit 0
    [ -f "$c" ] && [ "$c" -nt "$s" ] && exit 0
    mkdir -p "'"$out"'/$tu"
    if ! sh tools/m2c.sh "$tu" "$fn" > "$c.tmp" 2>&1; then
        printf "\n/* m2c falhou: veja acima */\n" >> "$c.tmp"
    fi
    mv "$c.tmp" "$c"
'
rm -f "$list"
echo "pronto: $(find "$out" -name '*.c' | wc -l) rascunhos em $out/"
