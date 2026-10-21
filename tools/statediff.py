#!/usr/bin/env python3
# ABOUTME: Compares two determinism runs: per-frame state hashes (WOTM_STATEHASH) and optionally two RAM dumps (WOTM_STATEDUMP).
# ABOUTME: Reports the first frame whose game state differs, the 128 KB blocks that differ, and the differing symbols of the dumps.
"""Usage:
  python tools/statediff.py A.hash B.hash                 first differing frame and its blocks
  python tools/statediff.py A.hash B.hash --dumps a.bin b.bin   also list differing symbols (native/wotm_syms.h) between the dumps

Hash file records (native/runtime/wotm_sync.inc, FrameHash): u32 index, u32 g_frame, u64 total, u64 blocks[256].
"""
import re
import struct
import sys
from pathlib import Path

BLOCKS = 256
BLOCK_SIZE = 32 * 1024 * 1024 // BLOCKS
RECORD = struct.Struct('<IIQ' + 'Q' * BLOCKS)
REPO = Path(__file__).resolve().parent.parent


def read_hashes(path):
    data = Path(path).read_bytes()
    n = len(data) // RECORD.size
    return [RECORD.unpack_from(data, i * RECORD.size) for i in range(n)]


def load_symbols():
    syms = []
    pat = re.compile(r'^\s*\{0x([0-9A-Fa-f]+), 0x([0-9A-Fa-f]+), "([^"]+)"\}')
    for line in (REPO / 'native' / 'wotm_syms.h').read_text(encoding='utf-8', errors='replace').splitlines():
        m = pat.match(line)
        if m:
            syms.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    syms.sort()
    return syms


def symbol_at(syms, addr):
    best = None
    for a, size, name in syms:
        if a > addr:
            break
        if addr < a + max(size, 1):
            best = (a, name)
    return best


def compare_hashes(a, b, ignore=()):
    """Returns (first differing record index or None, number of frames compared). `ignore`: block numbers left out."""
    n = min(len(a), len(b))
    for i in range(n):
        if a[i][2] != b[i][2] and any(k not in ignore for k in differing_blocks(a[i], b[i])):
            return i, n
    return None, n


def differing_blocks(ra, rb):
    return [k for k in range(BLOCKS) if ra[3 + k] != rb[3 + k]]


def compare_dumps(pa, pb, syms, limit=60):
    da, db = Path(pa).read_bytes(), Path(pb).read_bytes()
    groups = {}
    for off in range(0, min(len(da), len(db)), 4):
        if da[off:off + 4] != db[off:off + 4]:
            s = symbol_at(syms, off)
            key = s[1] if s else 'block 0x%06X' % (off & ~0xFFFF)
            g = groups.setdefault(key, [off, 0])
            g[1] += 1
    rows = sorted(groups.items(), key=lambda kv: kv[1][0])
    for name, (first, count) in rows[:limit]:
        print('  0x%07X  %6d words  %s' % (first, count, name))
    if len(rows) > limit:
        print('  ... %d more' % (len(rows) - limit))
    return len(rows)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    a, b = read_hashes(argv[1]), read_hashes(argv[2])
    ignore = set()
    if '--ignore' in argv:   # --ignore 0x440000,0x6E0000: blocks (by start address) that may differ
        ignore = {int(x, 16) // BLOCK_SIZE for x in argv[argv.index('--ignore') + 1].split(',')}
    first, n = compare_hashes(a, b, ignore)
    if first is None:
        print('identical for %d frames (A has %d, B has %d)' % (n, len(a), len(b)))
    else:
        ra, rb = a[first], b[first]
        print('first difference at frame index %d (g_frame A %d, B %d) of %d compared' % (ra[0], ra[1], rb[1], n))
        blocks = differing_blocks(ra, rb)
        print('differing 128 KB blocks (%d): %s' % (len(blocks), ' '.join('0x%07X' % (k * BLOCK_SIZE) for k in blocks)))
    if '--dumps' in argv:
        i = argv.index('--dumps')
        print('differing symbols between %s and %s:' % (argv[i + 1], argv[i + 2]))
        compare_dumps(argv[i + 1], argv[i + 2], load_symbols())
    return 0 if first is None else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
