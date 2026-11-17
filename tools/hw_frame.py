"""Per-frame calls to the hardware (and SDK) functions, from the runtime's coverage output.

With PS2X_COVERAGE=<prefix>, the runtime writes a file <prefix>.<frame> every 300 screen frames, holding the cumulative
call count per address ("0xADDRESS count"). The difference between two files from the same run gives the calls made
in that stretch; this script divides by the number of frames and groups by TU, with the state from config/status.csv.

Usage:
  python tools/hw_frame.py <cov.A> <cov.B> [--modo hw|sce|tudo]
    hw   (default) functions in config/hw_funcs.txt or with state hw
    sce  Sony's SDK (sce/*), lib989snd, newlib and libgcc
    tudo every function called in the stretch
  Example (AI vs AI fight, one 85 s run): WOTM_MATCH=3,1,1 WOTM_AIVSAI=1 PS2X_COVERAGE=<folder>\\cov, then
  python tools/hw_frame.py <folder>\\cov.1800 <folder>\\cov.4800
Map of the boundary and how to read the result: docs/passos/FRONTEIRA.md.
"""
import argparse
import csv
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_cov(path):
    m = {}
    if os.path.getsize(path) == 0:
        return m
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) == 2:
                m[int(p[0], 16)] = int(p[1])
    return m


def frame_of(path):
    g = re.search(r'\.(\d+)$', path)
    if not g:
        raise SystemExit(f'{path}: the name must end in .<frame>')
    return int(g.group(1))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--modo', choices=('hw', 'sce', 'tudo'), default='hw')
    args = ap.parse_args()

    A, B = load_cov(args.a), load_cov(args.b)
    frames = frame_of(args.b) - frame_of(args.a)
    if frames <= 0:
        raise SystemExit('the second file must be from a later frame than the first')

    status = {}
    with open(os.path.join(REPO, 'config', 'status.csv')) as f:
        for r in csv.DictReader(f):
            status[int(r['address'], 16)] = r
    hw_tus, hw_syms = set(), set()
    with open(os.path.join(REPO, 'config', 'hw_funcs.txt')) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            (hw_tus if line.startswith('tu:') else hw_syms).add(line[3:] if line.startswith('tu:') else line)

    lib_prefixes = ('sce/', 'lib989snd/', 'newlib/', 'gcc/')
    rows = []
    for addr, cb in B.items():
        n = cb - A.get(addr, 0)
        if n <= 0:
            continue
        r = status.get(addr)
        if r is None:
            tu, fn, st = '?', hex(addr), '?'
        else:
            tu, fn, st = r['tu'], r['function'], r['state']
        is_hw = st == 'hw' or tu in hw_tus or fn in hw_syms
        if args.modo == 'hw' and not is_hw:
            continue
        if args.modo == 'sce' and not tu.startswith(lib_prefixes):
            continue
        rows.append((tu, fn, n, st))

    print(f'{frames} screen frames; {len(rows)} functions in mode {args.modo}')
    by_tu = {}
    for r in rows:
        by_tu.setdefault(r[0], []).append(r)
    for tu in sorted(by_tu, key=lambda t: -sum(x[2] for x in by_tu[t])):
        lst = by_tu[tu]
        print(f'\n## {tu}  ({sum(x[2] for x in lst) / frames:.1f} calls/frame, {len(lst)} functions)')
        for r in sorted(lst, key=lambda x: -x[2]):
            print(f'   {r[2] / frames:9.2f}/f  {r[1]}  [{r[3]}]')


if __name__ == '__main__':
    main()
