#!/usr/bin/env python3
"""Report decompilation progress, per area, in four states:

  matched     C/C++ whose object code is byte-identical to the retail binary
  equivalent  C/C++ that is believed equivalent but does not match yet (wrapped in #ifdef NON_MATCHING with the
              original assembly as the INCLUDE_ASM fallback); not verified beyond reading the assembly
  hw          code that talks to PS2 hardware / SDK services and will be replaced by the port (config/hw_funcs.txt)
  asm         everything else still only in assembly

Library code (gcc, newlib, sce, lib989snd, crt0) is reported under `libs`: it doesn't need decompiling.
Also writes config/status.csv (one row per function) unless -n is given.
    python3 tools/progress.py [-v] [-n]
"""
import collections
import csv
import re
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parent.parent
INCLUDE_ASM = re.compile(r'INCLUDE_ASM\(\s*"[^"]*"\s*,\s*([^)\s]+)\s*\)')
EQUIV = re.compile(r'#ifdef NON_MATCHING\b.*?#else\s*\n\s*INCLUDE_ASM\(\s*"[^"]*"\s*,\s*([^)\s]+)\s*\)', re.S)
EQUIV_BLOCK = re.compile(r'#ifdef NON_MATCHING\b(?:(?!#ifdef NON_MATCHING|#endif).)*?#else\s*\n((?:\s*INCLUDE_ASM\([^)]*\);\s*\n)+)#endif', re.S)
STATES = ('matched', 'equivalent', 'hw', 'asm')


def load_hw():
    tus, syms = set(), set()
    for ln in (ROOT / 'config/hw_funcs.txt').read_text().splitlines():
        ln = ln.split('#')[0].strip()
        if ln.startswith('tu:'):
            tus.add(ln[3:].strip())
        elif ln:
            syms.add(ln)
    return tus, syms


def main():
    tus = list(csv.DictReader(open(ROOT / 'config/tus.csv')))
    starts = [int(r['start'], 16) for r in tus]
    ends = [int(r['end'], 16) for r in tus]
    hw_tus, hw_syms = load_hw()
    with open(ROOT / 'disc/SCUS_971.97', 'rb') as f:
        elf = ELFFile(f)
        funcs = sorted({(s['st_value'], s['st_size'], s.name) for s in elf.get_section_by_name('.symtab').iter_symbols()
                        if s['st_info']['type'] == 'STT_FUNC' and s['st_shndx'] == 1 and s['st_size']})

    stats = collections.defaultdict(lambda: collections.Counter())  # (area, state) -> funcs / bytes
    rows = []
    for t, r in enumerate(tus):
        name = r['name']
        area = name.split('/')[0] if '/' in name else name
        area = area if area in ('game', 'common') else 'libs'
        src = next((ROOT / f'src/{name}{e}' for e in ('.cpp', '.c') if (ROOT / f'src/{name}{e}').exists()), None)
        text = src.read_text() if src else ''
        all_asm = set(INCLUDE_ASM.findall(text)) if src else None
        equiv = set(EQUIV.findall(text))
        # a NON_MATCHING block may stand for several functions (e.g. template instances): every INCLUDE_ASM of its #else
        for blk in EQUIV_BLOCK.findall(text):
            equiv |= set(INCLUDE_ASM.findall(blk))

        def listed(fname, names):
            return any(fname == n or n.startswith(fname + '_') for n in names)

        for addr, size, fname in funcs:
            if not (starts[t] <= addr < ends[t]):
                continue
            if area == 'libs':
                state = 'asm'
            elif fname in hw_syms or name in hw_tus:
                state = 'hw'
            elif src is None or listed(fname, all_asm - equiv):
                state = 'asm'
            elif listed(fname, equiv):
                state = 'equivalent'
            else:
                state = 'matched'
            # code in hardware TUs that already matches still counts as matched
            if state == 'hw' and src and all_asm is not None and not listed(fname, all_asm):
                state = 'matched'
            stats[area][state + '_f'] += 1
            stats[area][state + '_b'] += size
            rows.append((name, fname, f'{addr:#x}', size, state))

    print(f"{'area':8} {'matched':>14} {'equivalent':>14} {'hw':>14} {'asm':>14}   (functions; bytes in -v)")
    for area in ('game', 'common', 'libs'):
        s = stats[area]
        tot = sum(s[x + '_f'] for x in STATES)
        cells = [f"{s[x + '_f']:5} {100 * s[x + '_f'] / max(tot, 1):5.1f}%" for x in STATES]
        print(f'{area:8} ' + ' '.join(f'{c:>14}' for c in cells) + f'   of {tot}')
        if '-v' in sys.argv:
            totb = sum(s[x + '_b'] for x in STATES)
            print(f"{'':8} " + ' '.join(f"{s[x + '_b']:>8} {100 * s[x + '_b'] / max(totb, 1):4.1f}%" for x in STATES) + '   bytes')
    # One row per area in the plain "done/total" shape (the wotm-hud mod parses this): done = matched + equivalent.
    print()
    print('decompiled (matched + equivalent), with bytes:')
    for area in ('game', 'common', 'libs'):
        s = stats[area]
        tot_f = sum(s[x + '_f'] for x in STATES)
        tot_b = sum(s[x + '_b'] for x in STATES)
        df = s['matched_f'] + s['equivalent_f']
        db = s['matched_b'] + s['equivalent_b']
        print(f'{area:8} {df:6}/{tot_f:<6} {100 * df / max(tot_f, 1):5.1f}%  {db:9}/{tot_b:<9} {100 * db / max(tot_b, 1):5.2f}%')
    if '-n' not in sys.argv:
        with open(ROOT / 'config/status.csv', 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['tu', 'function', 'address', 'size', 'state'])
            w.writerows(rows)


if __name__ == '__main__':
    main()
