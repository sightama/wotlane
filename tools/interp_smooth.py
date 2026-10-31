#!/usr/bin/env python3
# ABOUTME: Measures image "judder" from the log's [interp] lines (WOTM_INTERPLOG=1): how much the delay between display time and the
# ABOUTME: game time shown varies. With interpolation the delay stays nearly constant; without it, a sawtooth of one game frame.
"""Usage: python tools/interp_smooth.py <live.err.log> [--max-ms 8 | --min-ms 12]

Reads the "[interp] t=<s> g=<frame> fA=<fields> fB=<fields> alpha=<a>" lines from wotm_live.inc (t = presentation time; the game time
shown is fA + alpha * (fB - fA), in fields of 1/60 s; without blending, fB) and prints the spread (95th minus 5th percentile) of the
delay t - ct, in ms. Exits with 1 if it goes past the limit.
"""
import argparse
import re
import sys

# line from wotm_live.inc (Scene::showBlend): the fields of A and B and the blend alpha; "(off)" = no blending, the image is B's
LIVE = re.compile(r'\[interp\] t=([\d.]+) g=\d+ fA=(\d+) fB=(\d+) alpha=([\d.]+)( \(off\))?')


def read_presentations(text):
    """(time in s, game time in fields) per presentation."""
    out = []
    for m in LIVE.finditer(text):
        fa, fb, alpha = int(m.group(2)), int(m.group(3)), float(m.group(4))
        out.append((float(m.group(1)), float(fb) if m.group(5) else fa + alpha * (fb - fa)))
    return out


def lag_spread_ms(pres, window=61):
    """p5..p95 spread of the delay (ms) around the local median (a window of `window` presentations: a game pause is a step, not
    jitter) and how many presentations; the first tenth (clock settling) is left out."""
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
        print('no [interp] t= g= fA= fB= alpha= lines (WOTM_INTERPLOG=1, interpolation requested)')
        return 1
    print(f'image delay varies {spread:.2f} ms (p5..p95 of {n} presentations)')
    if a.max_ms is not None and spread > a.max_ms:
        return 1
    if a.min_ms is not None and spread < a.min_ms:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
