#!/usr/bin/env python3
# ABOUTME: Determinism test for the netcode on macOS, Linux or Windows (same as tools\determinism_check.ps1): runs the same fight twice,
# ABOUTME: the second with slow frames (WOTM_LOGICDELAY), in deterministic mode, and compares the per-frame state hash. Exit 0 if equal.
"""Usage: python3 tools/determinism_check.py [--match 3,1,1] [--players 2] [--secs 60] [--delay 40] [--step 1] [--replay [--input-delay N]]
  --match/--players: the fight (AI on everyone's pad).   --delay: maximum slow-frame delay in ms for the second run.
  --replay: the players stay on the pad: run A plays a pad script and records every frame's input (WOTM_INPUTREC); run B, with slow
    frames, replays the recording (WOTM_INPUTPLAY). --input-delay N: run A applies local input N frames after sampling it, and the
    first input sampled on frame X must be applied on frame X + N in both runs.
The hashes go to <runtime>/det_A.hash and det_B.hash (tools/statediff.py --dumps shows what diverges).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

import gamecheck
import live_check

# walk, punch, jump and grab, for players 1 and 2 (seconds since the window opened; the fight starts at about 15 s)
REPLAY_SCRIPT = ('18:6:up;20:1:square;22:1:cross;24:3:left;25:1:triangle;27:4:up@2;28:1:square@2;30:2:right@2;31:1:circle@2;33:5:down;'
                 '34:1:square;36:1:triangle@2;38:4:left@2')


def first(pattern, text):
    m = re.search(pattern, text)
    return [int(g) for g in m.groups()] if m else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--match', default='3,1,1')
    ap.add_argument('--players', type=int, default=2)
    ap.add_argument('--secs', type=int, default=60)
    ap.add_argument('--delay', type=int, default=40)
    ap.add_argument('--step', type=int, default=1)
    ap.add_argument('--replay', action='store_true')
    ap.add_argument('--input-delay', type=int, default=0)
    a = ap.parse_args()

    rt = gamecheck.runtime_dir()
    base = f'WOTM_MATCH={a.match};WOTM_PLAYERS={a.players};WOTM_NOINPUT=1;WOTM_FIXEDSTEP={a.step};WOTM_SEED=1234'
    if a.replay:
        inputs = rt / 'det_inputs.bin'
        runs = [('A', REPLAY_SCRIPT, f'WOTM_LOGICDELAY=;WOTM_AIVSAI=;WOTM_INPUTREC={inputs};WOTM_INPUTPLAY=;WOTM_INPUTDELAY={a.input_delay}'),
                ('B', '', f'WOTM_LOGICDELAY={a.delay};WOTM_AIVSAI=;WOTM_INPUTREC=;WOTM_INPUTPLAY={inputs};WOTM_INPUTDELAY=')]
    else:
        runs = [('A', '', 'WOTM_LOGICDELAY=;WOTM_AIVSAI=1'), ('B', '', f'WOTM_LOGICDELAY={a.delay};WOTM_AIVSAI=1')]
    logs = {}
    for name, pad, extra in runs:
        h = rt / f'det_{name}.hash'
        h.unlink(missing_ok=True)
        # the pad script uses ';' too: it goes through the environment, not the --env list
        if pad:
            os.environ['PS2X_PAD'] = pad
        else:
            os.environ.pop('PS2X_PAD', None)
        ok, logs[name] = live_check.run(f'{base};{extra};WOTM_STATEHASH={h}', a.secs, r'\[sync\] seed 1234', quiet=True)
        shutil.copyfile(rt / 'live_check.log', rt / f'det_{name}.log')
        if not ok:
            print(f'FAILED  run {name} (see {rt / f"det_{name}.log"})')
            return 1
    if a.replay:
        if not re.search(rf'\[sync\] input delay: {a.input_delay} frame', logs['A']):
            print(f'FAILED  A did not announce delay {a.input_delay}')
            return 1
        sample = first(r'\[sync\] first local input: sample of frame (\d+) applies at frame (\d+)', logs['A'])
        if not sample:
            print('FAILED  A did not sample local input')
            return 1
        sampled, scheduled = sample
        if scheduled != sampled + a.input_delay:
            print(f'FAILED  sample from frame {sampled} scheduled for {scheduled} (delay {a.input_delay})')
            return 1
        applied = [first(r'\[sync\] first non-neutral input applied at frame (\d+)', logs[n]) for n in ('A', 'B')]
        if None in applied:
            print('FAILED  input never applied')
            return 1
        if applied[0][0] != scheduled or applied[1][0] != scheduled:
            print(f'FAILED  scheduled for {scheduled}, applied at A {applied[0][0]} and B {applied[1][0]}')
            return 1
        gamecheck.say(True, f'input sampled on frame {sampled} applied on {scheduled} (delay {a.input_delay}) in A and B')
    r = subprocess.run([sys.executable, str(gamecheck.REPO / 'tools' / 'statediff.py'), str(rt / 'det_A.hash'), str(rt / 'det_B.hash')])
    if r.returncode != 0:
        print('FAILED  the state diverges')
        return 1
    print('PASSED  identical state on every compared frame')
    return 0


if __name__ == '__main__':
    sys.exit(main())
