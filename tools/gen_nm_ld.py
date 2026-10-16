#!/usr/bin/env python3
"""Linker script for the NON_MATCHING build that keeps every unchanged piece of the image at its retail address.

    python3 tools/gen_nm_ld.py <repo> <scratch>      # run in WSL; <scratch> holds the NM objects (build/**.o) and a copy of the repo files

The normal script lays the pieces (one `build/x.o(.sec)` line each) out back to back, so an equivalent function that is a few bytes
bigger shifts everything behind it, which breaks anything that depends on the retail layout (alignment of DMA buffers, SUBALIGN(4)
had already dropped the original alignments). Here:
  1. both builds are linked with start/end markers around each piece,
  2. a piece that grew in the NM build moves to an area after the end of the image,
  3. every other piece is pinned to `. = <retail address>`.
Writes <scratch>/SCUS_971.97.nm.ld and prints the pieces that moved; also <scratch>/undefined_{funcs,syms}_nm.txt (see the end).
"""
import bisect
import re
import subprocess
import sys
from pathlib import Path

repo = Path(sys.argv[1]).resolve()
scratch = Path(sys.argv[2]).resolve()
LD = 'mips-linux-gnu-ld'
NM = 'mips-linux-gnu-nm'
text = (scratch / 'SCUS_971.97.ld').read_text()
lines = text.split('\n')
piece_re = re.compile(r'^\s+(build/\S+\.o)\((.*)\);\s*$')
sym_re = re.compile(r'^\s+(cod_\w+)\s*=\s*\.;\s*$')


def marker_script(path):
    out = []
    k = 0
    for ln in lines:
        if piece_re.match(ln):
            out += [f'        __ps_{k} = .;', ln, f'        __pe_{k} = .;']
            k += 1
        else:
            out.append(ln)
    path.write_text('\n'.join(out))
    return k


def link_and_read(cwd, ldscript, out):
    cmd = [LD, '-EL', '--no-check-sections', '-T', str(ldscript), '-T', 'undefined_syms_auto.txt', '-T', 'undefined_funcs_auto.txt',
           '-T', 'linker_script_extra.ld', '-o', str(out)]
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'link failed in {cwd}:\n{r.stderr[-3000:]}')
    syms = {}
    for ln in subprocess.run([NM, str(out)], capture_output=True, text=True).stdout.splitlines():
        p = ln.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return syms


n = marker_script(scratch / 'ld_markers.ld')
ctrl = link_and_read(repo, scratch / 'ld_markers.ld', scratch / 'ctrl_markers.elf')
new = link_and_read(scratch, scratch / 'ld_markers.ld', scratch / 'nm_markers.elf')

pieces = [piece_re.match(ln) for ln in lines if piece_re.match(ln)]
grown = {}
for k in range(n):
    cs, ce = ctrl[f'__ps_{k}'], ctrl[f'__pe_{k}']
    ns, ne = new[f'__ps_{k}'], new[f'__pe_{k}']
    # up to 3 bytes is only the 4-byte input alignment landing differently, but only if retail leaves that much room before the next
    # piece (a piece that ends exactly where the next one starts cannot grow by even one byte)
    room = 0
    if k + 1 < n:
        room = max(0, min(3, ctrl[f'__ps_{k + 1}'] - ce))
    if ne - ns > ce - cs + room:
        grown[k] = (ne - ns, ce - cs)

out = []
k = 0
extra = []
sec_re = re.compile(r'^    (\.\w+)( 0x[0-9A-Fa-f]+)?( \(NOLOAD\))? ?:')
base = None
cur = None
for ln in lines:
    sm = sec_re.match(ln)
    if sm:
        cur = sm.group(1)
        if cur == '.cod':
            base = 0x100000
        elif cur == '.cod_bss':
            base = ctrl['cod_bss_VRAM']   # pinned: NOLOAD sections would otherwise start wherever .cod happens to end
            ln = ln.replace('.cod_bss (NOLOAD)', f'.cod_bss 0x{base:X} (NOLOAD)')
        out.append(ln)
        continue
    if cur == '.cod_bss' and ln == '    }':
        out.append(ln)
        out.append('    .nm_extra ALIGN(64) :')
        out.append('    {')
        out.append('        nm_EXTRA_START = .;')
        out += [e[1] for e in extra]
        out.append('        nm_EXTRA_END = .;')
        out.append('    }')
        cur = None
        continue
    m = piece_re.match(ln)
    if m:
        if k in grown:
            extra.append((k, ln))
        else:
            out.append(f'        . = 0x{ctrl[f"__ps_{k}"] - base:X};')
            out.append(ln)
        k += 1
        continue
    sy = sym_re.match(ln)
    if sy and sy.group(1) in ctrl and base is not None:
        out.append(f'        . = 0x{ctrl[sy.group(1)] - base:X};')
        out.append(ln)
        continue
    out.append(ln)
(scratch / 'SCUS_971.97.nm.ld').write_text('\n'.join(out))

# Only the start of each piece is pinned: an equivalent of another size shifts the rest of its piece. The undefined_*_auto.txt entries
# are absolute addresses, and a few point inside a function (splat names a jump into the middle of handwritten asm "func_XXXXXXXX":
# hierTraverseAsm's `j func_00207668`, its loop head). Left absolute, they keep the retail address while the code moved, and the jump
# lands elsewhere (the boot froze in hierTraverseAsm once texmActivateTexture, before it in common/texm, came out 8 bytes smaller).
# The NM link gets copies where such an entry is relative to the retail symbol that contains it.
sized = []
for ln in (repo / 'symbol_addrs.txt').read_text(errors='replace').splitlines():
    m = re.match(r'\s*(\S+)\s*=\s*0x([0-9A-Fa-f]+);.*size:0x([0-9A-Fa-f]+)', ln)
    if m:
        sized.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))
sized.sort()
starts = [s[0] for s in sized]
for name in ('undefined_funcs_auto.txt', 'undefined_syms_auto.txt'):
    rel = []
    lines_out = []
    for ln in (scratch / name).read_text().splitlines():
        m = re.match(r'\s*(\S+)\s*=\s*0x([0-9A-Fa-f]+);', ln)
        if m:
            addr = int(m.group(2), 16)
            i = bisect.bisect_right(starts, addr) - 1
            if i >= 0 and sized[i][0] < addr < sized[i][0] + sized[i][1]:
                ln = f'{m.group(1)} = {sized[i][2]} + 0x{addr - sized[i][0]:X};'
                rel.append(ln)
        lines_out.append(ln)
    (scratch / name.replace('_auto', '_nm')).write_text('\n'.join(lines_out) + '\n')
    for r in rel:
        print(f'  {name}: {r}')
print(f'{n} pieces, {len(grown)} grown and moved behind the image:')
for k, (a, b) in sorted(grown.items()):
    m = pieces[k]
    print(f'  {m.group(1)}({m.group(2)}) {b:#x} -> {a:#x}')
