#!/usr/bin/env python3
# ABOUTME: Network match checks on one computer (macOS, Linux or Windows; same table as tools\net_checks.ps1): each runs net_check.py
# ABOUTME: with 2 players (3 where it says so) and checks identical state on every frame plus the log. Exit 1 if any fail.
"""Usage: python3 tools/net_checks.py [--only <name>] [--list]"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

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
}


def args_for(check):
    out = []
    for key, value in check.items():
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
        if subprocess.run([sys.executable, str(Path(__file__).with_name('net_check.py'))] + args_for(check)).returncode != 0:
            failed.append(name)
    print(f"Failed: {', '.join(failed)}" if failed else 'All passed')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
