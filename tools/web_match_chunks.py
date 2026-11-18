#!/usr/bin/env python3
# ABOUTME: Writes web/match_chunks.json, the 1 MiB game.iso chunks a fight on each stage and with each monster (every costume) loads,
# ABOUTME: from fight logs of the game (tools/web_match_logs.mjs) and the ISO's directory; the page prefetches them for a lobby's match.
import argparse
import json
import re
import struct
import sys
from pathlib import Path

MIB = 1 << 20
SECTOR = 2048
READ = re.compile(r'(?:disc|sound): LSN (\d+), (\d+) sectors \(ISO\) in [\d.]+ ms(?! FAILED)')
FIGHT = re.compile(r'\[match\] 1: monster (.*), stage (\d+)')
LOADED = 'ready: stage on screen'
COSTUME = re.compile(r'^((?:MON|RST)/[A-Z]+)(\d)(\..*)$')   # MON/LIZARD0.NGP, RST/LIZARD0.AIC: one costume's file


def iso_files(iso):
    """Every file of an ISO9660 image as (first sector, bytes, 'DIR/NAME.EXT')."""
    out = []
    with open(iso, 'rb') as f:
        def walk(lsn, size, prefix):
            f.seek(lsn * SECTOR)
            data = f.read(size)
            i = 0
            while i < len(data):
                n = data[i]
                if n == 0:   # records don't cross a sector: the rest of this one is padding
                    i = (i // SECTOR + 1) * SECTOR
                    continue
                rec = data[i:i + n]
                i += n
                name = rec[33:33 + rec[32]]
                if name in (b'\x00', b'\x01'):   # the directory itself and its parent
                    continue
                ext, length = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0]
                path = prefix + name.decode('ascii').split(';')[0]
                if rec[25] & 2:
                    walk(ext, length, path + '/')
                else:
                    out.append((ext, length, path))
        f.seek(16 * SECTOR)
        root = f.read(SECTOR)[156:156 + 34]
        walk(struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0], '')
    return sorted(out)


def fight_of(lines):
    """(stage, [monster index per player]) from the log's '[match] 1: monster 3:0 (...) and 1:0 (...), stage 1' line."""
    for line in lines:
        m = FIGHT.search(line)
        if m:
            return int(m.group(2)), [int(x) for x in re.findall(r'(\d+):\d+', m.group(1))]
    raise ValueError('no "[match] 1: monster ..." line in the log')


def fight_files(lines, files):
    """The disc files the fight's load read: successful reads from the match line to the stage on screen."""
    reading, read = False, set()
    for line in lines:
        if FIGHT.search(line):
            reading = True
        elif LOADED in line:
            break
        m = READ.search(line) if reading else None
        if m and int(m.group(2)) > 0:
            first, last = int(m.group(1)), int(m.group(1)) + int(m.group(2)) - 1
            read.update(name for lsn, size, name in files if lsn <= last and first <= lsn + max(size - 1, 0) // SECTOR)
    return read


def chunks_of(names, files):
    """The 1 MiB chunks that hold the named files, sorted."""
    wanted = set(names)
    out = set()
    for lsn, size, name in files:
        if name in wanted:
            out.update(range(lsn * SECTOR // MIB, (lsn * SECTOR + max(size, 1) - 1) // MIB + 1))
    return sorted(out)


def table(monster_logs, stage_logs, files):
    """match_chunks.json's content. `monster_logs`: one fight per monster with only that monster in it, all on one stage (what every
    one of them reads is the stage's, not a monster's); `stage_logs`: one fight per stage, with monsters that have a monster log."""
    names = {name for _, _, name in files}
    runs = {}
    for log in monster_logs:
        stage, monsters = fight_of(log)
        if len(set(monsters)) != 1:
            raise ValueError(f'a monster log must have a single monster, not {monsters}')
        runs[monsters[0]] = fight_files(log, files)
    shared = set.intersection(*runs.values()) if runs else set()
    monsters, owned = {}, {}
    for m, read in sorted(runs.items()):
        own = read - shared
        base = sorted(n for n in own if not COSTUME.match(n))
        per = {COSTUME.match(n).group(1, 3) for n in own if COSTUME.match(n)}
        costumes = []
        for c in range(10):
            have = [f'{a}{c}{b}' for a, b in sorted(per) if f'{a}{c}{b}' in names]
            if not have:
                break
            costumes.append(have)
        owned[m] = set(base).union(*costumes)
        monsters[str(m)] = {'all': chunks_of(base, files), 'costumes': [chunks_of(c, files) for c in costumes]}
    stages = {}
    for log in stage_logs:
        stage, players = fight_of(log)
        missing = [m for m in players if m not in owned]
        if missing:
            raise ValueError(f'stage {stage}: no monster log for {missing}')
        stages[str(stage)] = chunks_of(fight_files(log, files) - set().union(*(owned[m] for m in players)), files)
    return {'stages': stages, 'monsters': monsters}


def main():
    ap = argparse.ArgumentParser(description='web/match_chunks.json from fight logs of the browser or native game')
    ap.add_argument('iso', help='game.iso')
    ap.add_argument('logs', help='folder of tools/web_match_logs.mjs: monster-<n>.log (one monster, both seats, one stage) and stage-<n>.log')
    ap.add_argument('--out', default=str(Path(__file__).resolve().parent.parent / 'web' / 'match_chunks.json'))
    a = ap.parse_args()
    files = iso_files(a.iso)
    read = lambda pattern: [p.read_text(errors='replace').splitlines() for p in sorted(Path(a.logs).glob(pattern))]
    t = table(read('monster-*.log'), read('stage-*.log'), files)
    if not t['stages'] or not t['monsters']:
        sys.exit(f'{a.logs}: needs monster-<n>.log and stage-<n>.log files')
    Path(a.out).write_text(json.dumps(t, separators=(',', ':')) + '\n')
    print(f"{a.out}: {len(t['stages'])} stages, {len(t['monsters'])} monsters")


if __name__ == '__main__':
    main()
