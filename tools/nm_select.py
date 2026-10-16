#!/usr/bin/env python3
"""Pick which NON_MATCHING blocks get compiled in (run inside the scratch copy made by tools/wsl/build_nm.sh).

    nm_select.py game/Monster                     every block of the TU
    nm_select.py game/Monster:symA,symB           only those mangled symbols
    nm_select.py game/Monster:-symA,symB          every block except those

Other TUs keep their INCLUDE_ASM (stay retail-identical). Adds -DNON_MATCHING to the TU in config/tu_flags.txt; for a symbol list the
unselected blocks are turned into `#if 0`.
"""
import re
import sys

spec = {}
for a in sys.argv[1:]:
    tu, _, fl = a.partition(':')
    spec[tu] = fl
lines = open('config/tu_flags.txt').read().splitlines()
out, seen = [], set()
for ln in lines:
    p = ln.split(None, 1)
    if p and not ln.startswith('#') and p[0] in spec:
        ln += ' -DNON_MATCHING'
        seen.add(p[0])
    out.append(ln)
out += [f'{t} -DNON_MATCHING' for t in sorted(set(spec) - seen)]
open('config/tu_flags.txt', 'w').write('\n'.join(out) + '\n')

# the body may not cross an #endif or another #ifdef NON_MATCHING: a helper block without #else (static inlines) would otherwise be
# swallowed by the next function's match and disabled with it
pat = re.compile(r'#ifdef NON_MATCHING\n((?:(?!#endif\n|#ifdef NON_MATCHING\n).)*?#else\nINCLUDE_ASM\("[^"]+", (\S+)\);\n#endif)', re.S)
for tu, fl in spec.items():
    if not fl:
        continue
    neg = fl.startswith('-')
    names = set(fl.lstrip('-').split(','))
    path = f'src/{tu}.cpp'
    text = open(path).read()

    def sub(m):
        on = (m.group(2) not in names) if neg else (m.group(2) in names)
        return ('#ifdef NON_MATCHING\n' if on else '#if 0\n') + m.group(1)

    open(path, 'w').write(pat.sub(sub, text))
