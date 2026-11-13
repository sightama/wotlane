#!/usr/bin/env python3
# ABOUTME: Network match checks on one computer (macOS, Linux or Windows; same table as tools\net_checks.ps1): each runs net_check.py
# ABOUTME: with 2 players (3 where it says so) and checks identical state on every frame plus the log. Exit 1 if any fail.
"""Usage: python3 tools/net_checks.py [--only <name>] [--list]"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
# map creator: map t1 (stage_map.py, CENTRAL: one removed, three moved, one turned) in a folder every PC uses, and an empty mods
# folder for the PC that lacks it
MAP_MODS = Path(tempfile.gettempdir()) / 'wotm_map_net'
NO_MAP = Path(tempfile.gettempdir()) / 'wotm_map_net_none'


def make_map():
    for d in (MAP_MODS, NO_MAP):
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
    subprocess.run([sys.executable, str(TOOLS / 'stage_map.py'), 'make', '--base', 'CENTRAL', '--id', 't1', '--name', 'TEST MAP',
                    '--out', str(MAP_MODS / 'Maps' / 'stages'), '--remove', '0x18a410', '--move', '0x165a70=-185.7,475.8,0',
                    '--move', '0x2d85a0=768.1,499.6,74.1', '--turn', '0x4da6b0=90', '--move', '0x5c95a0=400,-100,11.9'], check=True)


# map copies (docs/superpowers/specs/2026-11-09-map-copy-design.md): live_checks.ps1's c1 (the Apcomm tower 0x165a70 removed, the
# low building 0x2d85a0 copied onto player 1's start point and 0x18a410 into the park, with their riders and power-ups 5 and 15)
# plus 50 copies of 0x2d85a0 without riders on a 10 x 5 lattice from (-1300, -1100), 260 and 440 apart, at its disc z 74.1.
# make's --no-riders applies to a whole map, so the lattice is made as a second map and its copies are spliced into c1's edits
# after c1's own copies (the loader's file order: piece edits, copies, power-up moves, power-up additions).
COPY_DUMPS = Path(tempfile.gettempdir()) / 'wotm_net_c1'


def make_copy_map():
    for f in COPY_DUMPS.parent.glob(COPY_DUMPS.name + '_*.bin*'):   # no stale dumps from an earlier run
        f.unlink()
    lattice = Path(tempfile.gettempdir()) / 'wotm_map_net_lattice'
    for d in (MAP_MODS, lattice):
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
    stage_map = [sys.executable, str(TOOLS / 'stage_map.py'), 'make', '--base', 'CENTRAL']
    subprocess.run(stage_map + ['--id', 'c1', '--name', 'COPY TEST', '--out', str(MAP_MODS / 'Maps' / 'stages'), '--remove', '0x165a70',
                                '--copy', '0x2d85a0=768.1,499.6,74.1', '--copy', '0x18a410=400,-100,-0.8'], check=True)
    copies = []
    for i in range(50):
        copies += ['--copy', f'0x2d85a0={-1300 + 260 * (i % 10)},{-1100 + 440 * (i // 10)},74.1']
    subprocess.run(stage_map + ['--id', 'lattice', '--name', 'LATTICE', '--out', str(lattice), '--no-riders'] + copies, check=True,
                   stdout=subprocess.DEVNULL)
    file = MAP_MODS / 'Maps' / 'stages' / 'c1' / 'stage.json'
    doc = json.loads(file.read_text(encoding='utf-8'))
    extra = json.loads((lattice / 'lattice' / 'stage.json').read_text(encoding='utf-8'))['edits']
    edits = doc['edits']
    last = max((i for i, e in enumerate(edits) if 'copy' in e), default=-1)
    if last < 0 or len(extra) != 50:
        raise RuntimeError(f'unexpected maps: c1 copies end at {last}, lattice has {len(extra)} edits')
    doc['edits'] = edits[:last + 1] + extra + edits[last + 1:]
    file.write_text(json.dumps(doc, indent=2), encoding='utf-8')
    shutil.rmtree(lattice)
    print(f"        c1: {len(doc['edits'])} edits ({len(extra)} lattice copies after edit {last})")


def same_copy_dumps():
    """Both PCs' stage image and map reserve dumps match byte for byte."""
    same = True
    for ext in ('.bin', '.bin.reserve'):
        a, b = (Path(f'{COPY_DUMPS}_{n}{ext}') for n in (1, 2))
        ha, hb = (hashlib.sha256(p.read_bytes()).hexdigest() for p in (a, b))
        print(f"        {ext} : {a.stat().st_size} / {b.stat().st_size} bytes, {'same SHA256 ' + ha if ha == hb else 'DIFFERENT'}")
        same = same and a.stat().st_size > 0 and ha == hb
    return same


# Entry keys: net_check.py's options, setup (optional: a function run before the match; an error fails the entry) and after
# (optional: a function run after a passing match; a false result or an error fails the entry)
CHECKS = {
    # mouse on one PC only (WOTM_ORBITMOUSE: mouse-like movement from input frame 300 on), modern camera on both: the motion travels in
    # player 2's input with player 2's own sensitivity and inversion, so both PCs turn that camera alike
    'camera_mouse': dict(player_env='1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1;WOTM_CAMSENS=8;WOTM_CAMINVERT=1;WOTM_ORBITMOUSE=300:600:4:1', secs=50,
                         expect='network match: fixed profile'),
    # different game options on each PC: the camera options are each player's own and travel with their input, lock-on is the fixed
    # profile's on both
    'pc_options': dict(player_env='1:WOTM_CAMERA=0;WOTM_CAMSENS=10;WOTM_CAMINVERT=1|||2:WOTM_CAMERA=1;WOTM_LOCKTOGGLE=1', secs=50),
    # photo mode on one PC only (WOTM_PHOTO: turns itself on 25 s after the first scene): on the network it stays off
    'photo': dict(player_env='2:WOTM_PHOTO=25', secs=50, reject=r'\[photo\] on|||\[photo\] automatic test'),
    # pause by player 1, X on RECOMPILED OPTIONS: on the network that line does not open the panel and the pause stays in lockstep
    'pause_panel': dict(player_env='1:PS2X_PAD=24:0.3:start;26:0.2:down;27:0.2:down;28:0.3:cross;31:0.3:start;33:3:up;37:1:square', secs=50,
                        reject=r'\[pause\] panel opened'),
    # pause by player 1, screen while paused: both PCs must write the photo taken while paused (it is written only when a frame is drawn)
    'pause_screen': dict(extra_env='WOTM_SHOTS=30', player_env='1:PS2X_PAD=24:0.3:start', secs=40,
                         expect=r'\[dialog\] 0 opened|||\[shot\] shot_30\.png'),
    # 1-win target: "vencer" knocks out player 2, dialog 5 opens on both PCs, player 1's X picks PLAY AGAIN; round 2 plays the same on both
    'wins_target': dict(wins=1, extra_env='WOTM_CHEATS=vencer=8', player_env='1:PS2X_PAD=34:0.3:cross|||2:PS2X_PAD=12:0.3:square', secs=60,
                        expect=r'\[dialog\] 5 opened|||round 2 ended'),
    # REBUILD LEVEL in dialog 5 (down, X): ResetLevel rebuilds the stage without reloading; round 2 plays and ends the same on both
    'rebuild_level': dict(wins=1, extra_env='WOTM_CHEATS=vencer=8', player_env='1:PS2X_PAD=34:0.2:down;35:0.3:cross|||2:PS2X_PAD=12:0.3:square',
                          secs=60, expect=r'\[dialog\] 5 opened|||round 2 ended'),
    # QUIT in dialog 5: on the network the match ends and the game closes on every PC (no second stage load, which would desync)
    'quit_match': dict(wins=1, extra_env='WOTM_CHEATS=vencer=8',
                       player_env='1:PS2X_PAD=34:0.2:down;35:0.2:down;36:0.3:cross;38:0.2:up;39:0.3:cross|||2:PS2X_PAD=12:0.3:square', secs=55,
                       expect=r'\[net\] match ended by QUIT', reject='second level load'),
    # different mods (player 2 without the repo's mods): the mods digest is part of the configuration hash and the server rejects it
    'different_mods': dict(player_env='2:WOTM_MODS=0', secs=30, expect_refused='game configuration differs'),
    # training on one PC only: outside the configuration hash and it changes the game state, so online it is ignored
    'training_online': dict(player_env='1:WOTM_TRAINING=1;PS2X_PAD=24:0.3:start', secs=40,
                            expect=r'1:\[training\] WOTM_TRAINING ignored|||\[dialog\] 0 opened'),
    # 3 players (the game's 30 Hz mode, quadrant views), modern camera on everyone, player 2 pauses
    'pause_3p': dict(players=3, player_env='1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1;PS2X_PAD=24:0.3:start|||3:WOTM_CAMERA=1', secs=50,
                     expect=r'\[dialog\] 0 opened'),
    # 3 players, player 3 (camera 2, special-cased by the orbit code) with the mouse orbit, its own sensitivity and inversion, and the pause
    'pause_3p_mouse': dict(players=3, player_env='1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1|||3:WOTM_CAMERA=1;WOTM_CAMSENS=8;WOTM_CAMINVERT=1;'
                           'WOTM_ORBITMOUSE=300:900:4:1;PS2X_PAD=24:0.3:start', secs=50, expect=r'\[dialog\] 0 opened'),
    # 3 players with Turbo (2 fields per frame on top of the 30 Hz mode) and a pause by player 1
    'turbo_3p': dict(players=3, mods='turbo', player_env='1:PS2X_PAD=24:0.3:start', secs=45, expect=r'\[dialog\] 0 opened'),
    # a skin made by the character creator in every PC's mods folder (the same folder) does not break the mods digest or the lockstep;
    # the skin itself is not picked online (the lobby carries disc costumes only). Run the WOTM_CREATOR live check first
    # (tools\live_checks.ps1 -Only creator_script): it creates the folder
    'creator_skin_folder': dict(extra_env='WOTM_MODS=' + str(Path(tempfile.gettempdir()) / 'wotm_creator_mods'), secs=60,
                                expect=r'\[mods\] skin togera/green .* = costume 4'),
    # a saved map online (WOTM_MATCH's stage field is the map id on both PCs, the same mods folder): applied on both, same state
    'map_online': dict(setup=make_map, extra_env=f'WOTM_MODS={MAP_MODS}', stage='t1', secs=60,
                       expect=r'\[stages\] map t1 |||\[stages\] t1: applied 13 edits',
                       reject='does not match|||nothing applied|||not found in mods'),
    # the second PC without the map's folder: the mods digest differs and the server refuses the match (as different_mods)
    'map_online_missing': dict(setup=make_map, extra_env=f'WOTM_MODS={MAP_MODS}', player_env=f'2:WOTM_MODS={NO_MAP}', stage='t1',
                               secs=30, expect_refused='game configuration differs'),
    # map copies online: c1 plus 50 lattice copies applied on both PCs, same state on every frame. The per-frame state hash covers
    # only the low 32 MB of guest RAM and the copies' nodes live in the map reserve above it, so each PC also dumps its stage image
    # and reserve (WOTM_STAGE_DUMP) and both pairs must match byte for byte
    'map_copy_online': dict(setup=make_copy_map, after=same_copy_dumps, extra_env=f'WOTM_MODS={MAP_MODS}', stage='c1', secs=60,
                            player_env=f'1:WOTM_STAGE_DUMP={COPY_DUMPS}_1.bin|||2:WOTM_STAGE_DUMP={COPY_DUMPS}_2.bin',
                            expect=r'\[stages\] c1: copies: 58 applied, 0 refused',
                            reject=r'does not match|||nothing applied|||not found in mods|||Interactives table full|||'
                                   r'MemoryStack::global has \d+ bytes free'),
}


def args_for(check):
    out = []
    for key, value in check.items():
        if key in ('setup', 'after'):
            continue
        out += ['--' + key.replace('_', '-'), str(value)]
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--only', default='')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    if a.list:
        print('\n'.join(CHECKS))
        return 0
    if a.only and a.only not in CHECKS:
        print(f'unknown check {a.only!r}; --list shows them', file=sys.stderr)
        return 2
    failed = []
    for name, check in CHECKS.items():
        if a.only and name != a.only:
            continue
        print(f'== {name}', flush=True)
        if 'setup' in check:
            try:
                check['setup']()
            except Exception as e:   # noqa: BLE001 (any setup failure fails the entry)
                print(f'FAILED  setup: {e}')
                failed.append(name)
                continue
        if subprocess.run([sys.executable, str(Path(__file__).with_name('net_check.py'))] + args_for(check)).returncode != 0:
            failed.append(name)
            continue
        if 'after' in check:
            try:
                ok = check['after']()
            except Exception as e:   # noqa: BLE001 (any error after the match fails the entry)
                print(f'FAILED  after the match: {e}')
                ok = False
            if not ok:
                print(f'FAILED  {name} (after the match)')
                failed.append(name)
    print(f"Failed: {', '.join(failed)}" if failed else 'All passed')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
