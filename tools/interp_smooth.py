#!/usr/bin/env python3
# ABOUTME: Mede o "judder" da imagem nas linhas [interp] do log (WOTM_INTERPLOG=1): a variacao do atraso entre o instante de exibicao e o
# ABOUTME: tempo do jogo mostrado. Com interpolacao o atraso fica quase constante; sem ela, dente de serra de um quadro do jogo.
"""Uso: python tools/interp_smooth.py <live.err.log> [--max-ms 8 | --min-ms 12]

Le as linhas "[interp] t=<s> g=<quadro> fA=<fields> fB=<fields> alfa=<a>" do wotm_live.inc (t = instante da apresentacao; o tempo do
jogo mostrado e fA + alfa * (fB - fA), em fields de 1/60 s; sem a mistura, fB) e imprime a faixa (percentil 95 menos 5) do atraso t - ct,
em ms. Sai com 1 se passar do limite.
"""
import argparse
import re
import sys

# linha do wotm_live.inc (Scene::showBlend): fields de A e B e o alfa da mistura; "(desligada)" = sem mistura, a imagem e a de B
LIVE = re.compile(r'\[interp\] t=([\d.]+) g=\d+ fA=(\d+) fB=(\d+) alpha=([\d.]+)( \(off\))?')


def read_presentations(text):
    """(instante em s, tempo do jogo em fields) por apresentacao."""
    out = []
    for m in LIVE.finditer(text):
        fa, fb, alpha = int(m.group(2)), int(m.group(3)), float(m.group(4))
        out.append((float(m.group(1)), float(fb) if m.group(5) else fa + alpha * (fb - fa)))
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
        print('nenhuma linha [interp] t= g= fA= fB= alfa= (WOTM_INTERPLOG=1, interpolacao pedida)')
        return 1
    print(f'atraso da imagem varia {spread:.2f} ms (p5..p95 de {n} apresentacoes)')
    if a.max_ms is not None and spread > a.max_ms:
        return 1
    if a.min_ms is not None and spread < a.min_ms:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
