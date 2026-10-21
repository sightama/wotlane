#!/usr/bin/env python3
# ABOUTME: Mede o "judder" da imagem nas linhas [interp] do log (WOTM_INTERPLOG=1): a variacao do atraso entre o instante de exibicao e o
# ABOUTME: tempo do jogo mostrado. Com interpolacao o atraso fica quase constante; sem ela, dente de serra de um quadro do jogo.
"""Uso: python tools/interp_smooth.py <live.err.log> [--max-ms 8 | --min-ms 12]

Le as linhas "[interp] t=<s> a=<alfa> r=<redesenhou> ct=<fields> cam=..." (t = instante em que a apresentacao aparece; ct = tempo do
jogo da imagem, em fields de 1/60 s) e imprime a faixa (percentil 95 menos 5) do atraso t - ct, em ms. Sai com 1 se passar do limite.
"""
import argparse
import re
import sys

LINE = re.compile(r'\[interp\] t=([\d.]+) a=[\d.]+ r=\d ct=([-\d.]+)(?: n=(\d+) hz=(\d+))?')


def read_presentations(text):
    """(instante em s, tempo do jogo em fields) por apresentacao; com n= e hz= o instante e o vblank n / hz."""
    out = []
    for m in LINE.finditer(text):
        t = int(m.group(3)) / int(m.group(4)) if m.group(3) else float(m.group(1))
        out.append((t, float(m.group(2))))
    return out


def lag_spread_ms(pres, window=61):
    """Faixa p5..p95 do atraso (ms) em volta da mediana local (janela de `window` apresentacoes: uma pausa do jogo e um degrau, nao
    tremor) e quantas apresentacoes; a primeira decima parte (relogio assentando) fica de fora."""
    lags = [t * 1000.0 - ct * 1000.0 / 60.0 for t, ct in pres[len(pres) // 10:]]
    n = len(lags)
    if n == 0:
        return None, 0
    half = window // 2
    dev = []
    for i, lag in enumerate(lags):
        local = sorted(lags[max(0, i - half):i + half + 1])
        dev.append(lag - local[len(local) // 2])
    dev.sort()
    return dev[n * 95 // 100] - dev[n * 5 // 100], n


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('log')
    ap.add_argument('--max-ms', type=float)
    ap.add_argument('--min-ms', type=float)
    a = ap.parse_args()
    with open(a.log, encoding='utf-8', errors='replace') as f:
        pres = read_presentations(f.read())
    spread, n = lag_spread_ms(pres)
    if spread is None:
        print('nenhuma linha [interp] com ct=')
        return 1
    print(f'atraso da imagem varia {spread:.2f} ms (p5..p95 de {n} apresentacoes)')
    if a.max_ms is not None and spread > a.max_ms:
        return 1
    if a.min_ms is not None and spread < a.min_ms:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
