#!/usr/bin/env python3
"""Compile a source file with each ee-gcc in ~/compilers and score each function against the retail binary.

    python3 tools/ccmatch.py tools/cctest/crc32.cpp "-O2 -G0" [compiler ...]
    python3 tools/ccmatch.py src/common/zip.cpp "" project     # real build pipeline (tools/cc.sh)

Relocated fields (HI16/LO16/GPREL16 immediates, 26-bit jump targets) are masked before comparing.
Run inside WSL. Needs disc/SCUS_971.97 (for the retail symbol table) and SCUS_971.97.rom.
"""
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ROOT = Path(__file__).resolve().parent.parent
COMPILERS = Path.home() / 'compilers'
# outside /tmp: WSL may clear it between sessions; one directory per checkout, so worktrees can score at the same time (tools/wsl/fdiff.sh reads it)
WORK = Path(os.environ['CCMATCH_WORK']) if os.environ.get('CCMATCH_WORK') else Path.home() / '.cache/ccmatch' / ROOT.name
ROM_VRAM = 0x100000

R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16 = 4, 5, 6, 7


def retail_funcs():
    with open(ROOT / 'disc/SCUS_971.97', 'rb') as f:
        return {s.name: (s['st_value'], s['st_size'])
                for s in ELFFile(f).get_section_by_name('.symtab').iter_symbols()
                if s['st_info']['type'] == 'STT_FUNC'}


def compile_with(compiler, src, flags):
    if compiler == 'project':  # the real build pipeline (tools/cc.sh); flags are extra cflags
        WORK.mkdir(parents=True, exist_ok=True)
        out = WORK / 'project.o'
        out.unlink(missing_ok=True)
        r = subprocess.run(['sh', str(ROOT / 'tools/cc.sh'), str(src), str(out), *shlex.split(flags)],
                           capture_output=True, text=True, env=dict(os.environ, PYTHON=sys.executable))
        return (out, None) if out.exists() else (None, (r.stderr or r.stdout).strip().splitlines()[-3:])
    d = COMPILERS / compiler
    if (d / 'cc').is_dir():  # SN ProDG package layout: cc/bin, cc/ee/bin, cc/lib/gcc-lib
        d = d / 'cc'
    exe = d / 'bin/ee-gcc'
    cmd = [str(exe)] if exe.exists() else ['wine', str(d / 'bin/ee-gcc.exe')]
    libexec = next(d.glob('lib/gcc-lib/ee/*/'), None)
    if libexec is not None:
        cmd.append(f'-B{libexec}/')
    if (d / 'ee/bin').is_dir():
        cmd.append(f'-B{d}/ee/bin/')
    out = WORK / f'{compiler}.o'
    out.unlink(missing_ok=True)
    env = dict(os.environ, COMPILER_PATH=str(d / 'bin'), WINEDEBUG='-all')
    # 32-bit Linux builds can't stat files on /mnt/c (64-bit inodes -> EOVERFLOW): compile from /tmp.
    work = WORK / 'src'
    shutil.rmtree(work, ignore_errors=True)
    WORK.mkdir(parents=True, exist_ok=True)
    shutil.copytree(src.parent, work)
    sn_as = d / 'ee/bin/Ps2EeAs.exe'
    if sn_as.exists():  # SN ProDG: compile to .s, assemble with SN's assembler (its macro expansion differs)
        asm = work / (src.stem + '.s')
        r = subprocess.run(cmd + ['-S', *shlex.split(flags), src.name, '-o', str(asm)],
                           cwd=work, env=env, capture_output=True, text=True)
        if asm.exists():
            r = subprocess.run(['wine', str(sn_as), '-o', str(out), str(asm)],
                               cwd=work, env=env, capture_output=True, text=True)
    else:
        r = subprocess.run(cmd + ['-c', *shlex.split(flags), src.name, '-o', str(out)],
                           cwd=work, env=env, capture_output=True, text=True)
    if not out.exists():
        return None, r.stderr.strip().splitlines()[-3:]
    return out, None


def obj_funcs(path):
    with open(path, 'rb') as f:
        elf = ELFFile(f)
        text = elf.get_section_by_name('.text')
        data = text.data()
        masks = {}
        for sec in elf.iter_sections():
            if isinstance(sec, RelocationSection) and sec.name in ('.rel.text', '.rela.text'):
                for rel in sec.iter_relocations():
                    t = rel['r_info_type']
                    masks[rel['r_offset']] = 0xFC000000 if t == R_MIPS_26 else 0xFFFF0000
        funcs = {}
        for s in elf.get_section_by_name('.symtab').iter_symbols():
            if s['st_info']['type'] == 'STT_FUNC' and s['st_size']:
                funcs[s.name] = (s['st_value'], s['st_size'])
    return data, masks, funcs


def score(compiler, src, flags, retail, rom):
    obj, err = compile_with(compiler, src, flags)
    if obj is None:
        return f'{compiler:24} compile failed: {" | ".join(err)}'
    data, masks, funcs = obj_funcs(obj)
    parts = []
    for name, (off, size) in sorted(funcs.items()):
        if name not in retail:
            parts.append(f'{name}: not in retail')
            continue
        raddr, rsize = retail[name]
        r = rom[raddr - ROM_VRAM:raddr - ROM_VRAM + rsize]
        o = data[off:off + size]
        n = max(len(r), len(o)) // 4
        same = 0
        for i in range(0, min(len(r), len(o)), 4):
            m = masks.get(off + i, 0xFFFFFFFF)
            a = int.from_bytes(r[i:i + 4], 'little') & m
            b = int.from_bytes(o[i:i + 4], 'little') & m
            same += a == b
        tag = 'MATCH' if same == n and len(r) == len(o) else f'{same}/{n} words, size {size:#x} vs {rsize:#x}'
        parts.append(f'{name}: {tag}')
    return f'{compiler:24} ' + '; '.join(parts)


def main():
    src = Path(sys.argv[1]).resolve()
    flags = sys.argv[2] if len(sys.argv) > 2 else '-O2'
    compilers = sys.argv[3:] or sorted(p.name for p in COMPILERS.iterdir() if p.is_dir())
    retail = retail_funcs()
    rom = (ROOT / 'SCUS_971.97.rom').read_bytes()
    for c in compilers:
        print(score(c, src, flags, retail, rom), flush=True)


if __name__ == '__main__':
    main()
