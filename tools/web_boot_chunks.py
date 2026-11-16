#!/usr/bin/env python3
# ABOUTME: Writes web/boot_chunks.json, the 1 MiB disc chunks the browser page prefetches (web/disc_cache.js): every chunk of the ELF and
# ABOUTME: the game.iso chunks a boot read, from the game's log ("disc: LSN ..." and "sound: LSN ..." lines, the page's L log or stderr).
import argparse
import json
import re
import sys
from pathlib import Path

MIB = 1 << 20
SECTOR = 2048
READ = re.compile(r'(?:disc|sound): LSN (\d+), (\d+) sectors \(ISO\) in [\d.]+ ms(?! FAILED)')


def iso_chunks(lines):
    """The game.iso chunks the successful reads in `lines` touch, sorted."""
    chunks = set()
    for line in lines:
        m = READ.search(line)
        if m and int(m.group(2)) > 0:
            start = int(m.group(1)) * SECTOR
            end = start + int(m.group(2)) * SECTOR - 1
            chunks.update(range(start // MIB, end // MIB + 1))
    return sorted(chunks)


def manifest(lines, elf_bytes):
    return {'SCUS_971.97': list(range((elf_bytes + MIB - 1) // MIB)), 'game.iso': iso_chunks(lines)}


def main():
    ap = argparse.ArgumentParser(description='web/boot_chunks.json from a boot log of the browser or native game')
    ap.add_argument('log', help='the game log of a boot (to the title: the front panel\'s Single player)')
    ap.add_argument('elf', help='SCUS_971.97, for its size')
    ap.add_argument('--out', default=str(Path(__file__).resolve().parent.parent / 'web' / 'boot_chunks.json'))
    a = ap.parse_args()
    m = manifest(Path(a.log).read_text(errors='replace').splitlines(), Path(a.elf).stat().st_size)
    if not m['game.iso']:
        sys.exit('no disc reads in the log')
    Path(a.out).write_text(json.dumps(m, separators=(',', ':')) + '\n')
    print(f"{a.out}: {len(m['SCUS_971.97'])} ELF chunks, {len(m['game.iso'])} game.iso chunks")


if __name__ == '__main__':
    main()
