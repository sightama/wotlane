#!/usr/bin/env python3
"""Make code pointers in splat's data asm follow the code in the NON_MATCHING build (run inside the scratch copy of tools/wsl/build_nm.sh).

    python3 tools/nm_fix_data.py            # rewrites asm/data/**/*.s in the current directory

splat writes some tables of code pointers as raw numbers: the pointer-to-member tables (`_6Camera$UPDATE_FUNK`: delta, index, then the
function as `.short 0xF630` / `.short 0x0011`) come out as pairs of .short. In the retail build that is the same thing; in the NM
build, where an equivalent of another size shifts the rest of its piece (tools/gen_nm_ld.py pins only the start), the number keeps the
retail address and the call lands in the middle of another function (Cameras::Update -> 0x11F630, Camera::normalUpdate, which moved).
Every 4-aligned pair of .short whose value is exactly the start of a function (symbol_addrs.txt, type:func) becomes `.word <function>`.
Never used on the real build: there the bytes are already right and the asm must stay what splat wrote.
"""
import re
from pathlib import Path

funcs = {}
for ln in Path('symbol_addrs.txt').read_text(errors='replace').splitlines():
    m = re.match(r'\s*(\S+)\s*=\s*0x([0-9A-Fa-f]+);.*type:func', ln)
    if m:
        funcs.setdefault(int(m.group(2), 16), m.group(1))

short = re.compile(r'^(\s*/\* \S+ ([0-9A-Fa-f]{8}) \*/ )\.short 0x([0-9A-Fa-f]{1,4})\s*$')
total = 0
for path in sorted(Path('asm/data').rglob('*.s')):
    lines = path.read_text().split('\n')
    out, i, n = [], 0, 0
    while i < len(lines):
        a, b = short.match(lines[i]), short.match(lines[i + 1]) if i + 1 < len(lines) else None
        if a and b and int(a.group(2), 16) % 4 == 0 and int(b.group(2), 16) == int(a.group(2), 16) + 2:
            value = int(a.group(3), 16) | int(b.group(3), 16) << 16
            if value in funcs:
                out.append(f'{a.group(1)}.word {funcs[value]}')
                i += 2
                n += 1
                continue
        out.append(lines[i])
        i += 1
    if n:
        path.write_text('\n'.join(out))
        total += n
        print(f'  {path}: {n}')
print(f'nm_fix_data: {total} code pointers written as .short pairs now follow their function')
