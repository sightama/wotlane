#!/usr/bin/env python3
# ABOUTME: Shared part of the cross-platform game checks (live_check.py, net_check.py, determinism_check.py): the runtime folder, the
# ABOUTME: check environment, variable lists, launching and stopping game copies, and the log verdicts. Tested by test_gamecheck.py.
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
WINDOWS = sys.platform == 'win32'
DEFAULT_REJECT = [r'\[CRASH\]', r'\[watchdog\] FROZE']


def runtime_dir():
    """The runtime folder, looked up as the other scripts do: WOTM_RECOMP_DIR, then .wotm_recomp_dir, then <repo>/runtime."""
    if os.environ.get('WOTM_RECOMP_DIR'):
        return Path(os.environ['WOTM_RECOMP_DIR'])
    saved = REPO / '.wotm_recomp_dir'
    if saved.exists():
        return Path(saved.read_text().splitlines()[0].strip())
    return REPO / 'runtime'


def game_exe(runtime, build='build_dev'):
    return runtime / build / 'ps2xRuntime' / ('wotm.exe' if WINDOWS else 'wotm')


def iso_path(runtime):
    """PS2X_CD_IMAGE, else the ISO setup_runtime used (<runtime>/.wotm_iso), else the first .iso in <repo>/ISO."""
    if os.environ.get('PS2X_CD_IMAGE'):
        return os.environ['PS2X_CD_IMAGE']
    saved = runtime / '.wotm_iso'
    if saved.exists() and Path(saved.read_text().strip()).exists():
        return saved.read_text().strip()
    isos = sorted((REPO / 'ISO').glob('*.iso'))
    if not isos:
        raise SystemExit(f'error: no .iso in {REPO / "ISO"} (set PS2X_CD_IMAGE)')
    return str(isos[0])


def check_env(runtime, res='720'):
    """The environment every check starts from (as run_live and the .ps1 checks): native drawing, no sound, a small window that does
    not cover the screen, this PC's settings.ini ignored. Variables already set in the caller's environment win."""
    env = dict(os.environ)
    env.update(PS2X_NO_VU1='1', PS2X_NO_VIF1='1', PS2X_NO_GS='1', PS2X_SCENE='1', PS2X_FMV='1', WOTM_REPO=str(REPO),
               PS2X_CD_IMAGE=iso_path(runtime))
    for k, v in (('WOTM_MUTE', '1'), ('WOTM_FULLSCREEN', '0'), ('WOTM_RES', res), ('WOTM_WINDOW', res), ('WOTM_SETTINGS', '0')):
        env.setdefault(k, v)
    return env


def sync_mods(runtime):
    """The repo's mods into <runtime>/discroot/mods, as run_live does before every launch."""
    if WINDOWS:
        cmd = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(REPO / 'tools' / 'sync_mods.ps1'), '-Runtime', str(runtime)]
    else:
        cmd = ['sh', str(REPO / 'tools' / 'sync_mods.sh'), str(runtime), str(REPO)]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


def env_pairs(text):
    """'A=1;B=x,y' -> [('A', '1'), ('B', 'x,y')]. A piece without '=' continues the previous value (a PS2X_PAD script uses ';')."""
    pairs = []
    for piece in text.split(';'):
        if not piece.strip():
            continue
        if re.match(r'^\s*[A-Za-z0-9_]+=', piece):
            name, _, value = piece.partition('=')
            pairs.append((name.strip(), value))
        elif pairs:
            pairs[-1] = (pairs[-1][0], pairs[-1][1] + ';' + piece)
        else:
            raise ValueError(f'not NAME=value: {piece!r}')
    return pairs


def apply_pairs(env, pairs):
    """Sets each pair in env; an empty value removes the variable ("NAME=" clears one, as in the .ps1 checks)."""
    for name, value in pairs:
        if value == '':
            env.pop(name, None)
        else:
            env[name] = value
    return env


def player_env(spec, player):
    """'1:A=1|||2:B=2;C=3' -> the pairs for `player` only."""
    pairs = []
    for part in patterns(spec):
        number, _, rest = part.partition(':')
        if number.strip() == str(player):
            pairs += env_pairs(rest)
    return pairs


def patterns(text):
    """Regexes separated by '|||'."""
    return [p for p in (text or '').split('|||') if p]


def patterns_for(text, player):
    """patterns() with the '<n>:' ones kept only for player n (without the prefix)."""
    out = []
    for p in patterns(text):
        m = re.match(r'^(\d+):(.*)$', p, re.S)
        if m:
            if int(m.group(1)) == player:
                out.append(m.group(2))
        else:
            out.append(p)
    return out


def log_problems(text, expect, reject):
    """'missing <re>' for each expected pattern not in the log, 'found <match>' for each rejected one that is."""
    problems = [f'missing {p}' for p in expect if not re.search(p, text)]
    for p in reject:
        m = re.search(p, text)
        if m:
            problems.append(f'found {m.group(0).strip()}')
    return problems


def read_log(path):
    return Path(path).read_text(encoding='utf-8', errors='replace') if Path(path).exists() else ''


def launch(exe, elf, cwd, env, err_log, out_log):
    """Starts one game copy (its own process group, so stop() also ends anything it started)."""
    with open(err_log, 'w') as err, open(out_log, 'w') as out:
        kw = {'creationflags': subprocess.CREATE_NEW_PROCESS_GROUP} if WINDOWS else {'start_new_session': True}
        return subprocess.Popen([str(exe), str(elf)], cwd=str(cwd), env=env, stderr=err, stdout=out, **kw)


def stop(procs, grace=3.0):
    """Asks each game to close (SIGTERM / CTRL_BREAK), waits `grace` seconds, then kills what is left."""
    for p in procs:
        if p.poll() is None:
            try:
                if WINDOWS:
                    p.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    os.killpg(p.pid, signal.SIGTERM)
            except (OSError, ProcessLookupError):
                pass
    deadline = time.time() + grace
    for p in procs:
        try:
            p.wait(timeout=max(0.1, deadline - time.time()))
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()


def say(ok, text):
    print(f"{'ok     ' if ok else 'FAILED '} {text}", flush=True)
