#!/usr/bin/env python3
# ABOUTME: Automated live game check on macOS, Linux or Windows (same as tools\live_check.ps1): runs the game with the given variables
# ABOUTME: for a few seconds and checks its log. Exit 0 when every --expect pattern appears and no --reject one does.
"""Usage:
  python3 tools/live_check.py --env "WOTM_MATCH=3,1,1;WOTM_AIVSAI=1" --secs 30 --expect "regex1|||regex2" [--reject "regex"] [--exe test]

  --env     NAME=value pairs separated by ';' (values may contain commas; "NAME=" clears one).
  --expect  regexes separated by '|||', all required.   --reject  regexes that must not appear (default: [CRASH], [watchdog] FROZE).
  --exe test  runs ps2EntryRunner. The log is <runtime>/live.err.log, with a copy in <runtime>/live_check.log.
"""
import argparse
import shutil
import sys
import time

import gamecheck


def run(env_text='', secs=30, expect='', reject=None, exe='', quiet=False):
    """Runs the check; returns (passed, log text). Used by determinism_check.py too."""
    rt = gamecheck.runtime_dir()
    disc = rt / 'discroot'
    path = gamecheck.game_exe(rt)
    if exe == 'test':
        path = path.with_name('ps2EntryRunner.exe' if gamecheck.WINDOWS else 'ps2EntryRunner')
    if not path.exists():
        raise SystemExit(f'error: {path} not found (build the runtime first)')
    env = gamecheck.apply_pairs(gamecheck.check_env(rt), gamecheck.env_pairs(env_text))
    gamecheck.sync_mods(rt)
    log = rt / 'live.err.log'
    proc = gamecheck.launch(path, disc / 'SCUS_971.97', disc, env, log, rt / 'live.out.log')
    try:
        time.sleep(secs)
    finally:
        gamecheck.stop([proc])
    shutil.copyfile(log, rt / 'live_check.log')
    text = gamecheck.read_log(log)
    want = gamecheck.patterns(expect)
    problems = gamecheck.log_problems(text, want, gamecheck.DEFAULT_REJECT if reject is None else gamecheck.patterns(reject))
    if not quiet:
        for p in want:
            if f'missing {p}' not in problems:
                gamecheck.say(True, p)
        for p in problems:
            gamecheck.say(False, p)
        print('PASSED' if not problems else 'FAILED')
    return not problems, text


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--env', default='')
    ap.add_argument('--secs', type=int, default=30)
    ap.add_argument('--expect', default='')
    ap.add_argument('--reject', default=None)
    ap.add_argument('--exe', default='')
    a = ap.parse_args()
    ok, _ = run(a.env, a.secs, a.expect, a.reject, a.exe)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
