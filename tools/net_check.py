#!/usr/bin/env python3
# ABOUTME: Network match test on one computer (macOS, Linux or Windows; same as tools\net_check.ps1): starts wotm_server, forms the match
# ABOUTME: through the lobby (net_lobby.py), opens one game per player with its own pad script and checks every frame's state is equal.
"""Usage: python3 tools/net_check.py [--players 2] [--monsters 3,1] [--stage 1] [--wins 0] [--mods turbo] [--secs 70] [--port 7811]
                                [--exe a,b] [--drop S] [--extra-env "A=1;B=2"] [--player-env "2:A=1|||3:B=2"]
                                [--expect "re|||2:re"] [--reject "re"] [--expect-refused "reason"]
  --exe         one game executable per player (comma list; the last one covers the rest). Default: the runtime's build_dev.
  --drop S      closes the last player after S seconds and checks that the others carry on.
  --extra-env   variables for every player.   --player-env  variables for player N only (what differs on just one PC).
  --expect      regexes every player's log must have ("2:re": player 2's only).   --reject  regexes no log may have.
  --expect-refused  the server must reject the match with this reason, and nobody may start it.
Logs and hashes go to <runtime>/net/ (server.log, player<N>.err.log, player<N>.hash). Exit 0 when everything matches.
"""
import argparse
import re
import shutil
import subprocess
import sys
import time

import gamecheck

# each player walks, hits and jumps at different times (seconds since launch; the fight starts at about 15 s)
SCRIPTS = [
    '18:6:up;20:1:square;22:1:cross;24:3:left;25:1:triangle;33:5:down;34:1:square;45:2:right;46:1:circle',
    '19:5:up;21:1:triangle;23:2:right;26:1:square;31:1:circle;36:4:left;40:1:cross;47:1:square',
    '18:3:down;21:1:square;24:1:cross;28:4:right;30:1:triangle;38:2:up;43:1:circle',
    '20:4:left;22:1:circle;25:1:square;29:3:up;32:1:cross;41:2:down;44:1:triangle',
]
MIN_FRAMES = 1200   # fewer compared frames than this means the match barely ran


def build_server():
    server = gamecheck.REPO / 'native' / 'net' / 'build' / ('wotm_server.exe' if gamecheck.WINDOWS else 'wotm_server')
    sources = list((gamecheck.REPO / 'native' / 'net').glob('*.?pp'))
    if not server.exists() or max(s.stat().st_mtime for s in sources) > server.stat().st_mtime:
        cmd = ['cmd', '/c', str(gamecheck.REPO / 'tools' / 'build_net.bat'), 'notest'] if gamecheck.WINDOWS else \
              ['sh', str(gamecheck.REPO / 'tools' / 'test_net.sh'), 'notest']
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    if not server.exists():
        raise SystemExit(f'error: {server} did not build')
    return server


def verdict(a, out, server_log):
    """The checks on the logs and hashes; returns the list of failures."""
    fails = []
    logs = {i: gamecheck.read_log(out / f'player{i}.err.log') for i in range(1, a.players + 1)}
    if a.expect_refused:
        refused = False
        for i, log in logs.items():
            m = re.search(r'\[net\] stopped: [^\r\n]*' + a.expect_refused + r'[^\r\n]*', log)
            if m:
                refused = True
                gamecheck.say(True, f'player {i}: {m.group(0)}')
            if '[net] start: player' in log:
                fails.append(f'player {i} started the match')
        if not refused:
            fails.append(f"nobody was rejected for '{a.expect_refused}'")
        return fails
    m = re.search(r'DESYNC[^\r\n]*', server_log)
    if m:
        fails.append(f'server: {m.group(0)}')
    m = re.search(r'start, delay (\d+)', server_log)
    if m:
        gamecheck.say(True, f'server started the match (delay {m.group(1)})')
    else:
        fails.append('server never started the match')
    for i, log in logs.items():
        if f'[net] start: player {i} of {a.players}' in log:
            gamecheck.say(True, f'player {i} started')
        else:
            fails.append(f'player {i} did not start the match')
        m = re.search(r'\[net\] stopped: [^\r\n]*', log)
        if m and not (a.drop and i == a.players):
            fails.append(f'player {i}: {m.group(0)}')
        if '[sync] first non-neutral input applied at frame' not in log:
            fails.append(f'player {i}: no input reached the game')
        reject = gamecheck.DEFAULT_REJECT + gamecheck.patterns(a.reject)
        fails += [f'player {i}: {p}' for p in gamecheck.log_problems(log, gamecheck.patterns_for(a.expect, i), reject)]
    if a.drop and not re.search(rf'J{a.players} left the match \(neutral from frame \d+\)', server_log):
        fails.append(f'server did not log player {a.players} leaving')
    alive = a.players - 1 if a.drop else a.players
    for i in range(2, alive + 1):
        r = subprocess.run([sys.executable, str(gamecheck.REPO / 'tools' / 'statediff.py'), str(out / 'player1.hash'), str(out / f'player{i}.hash')],
                           capture_output=True, text=True)
        for line in r.stdout.splitlines():
            print(f'        {line}')
        m = re.search(r'identical for (\d+) frames', r.stdout)
        if r.returncode != 0:
            fails.append(f'player {i} state differs from player 1')
        elif m and int(m.group(1)) < MIN_FRAMES:
            fails.append(f'only {m.group(1)} frames compared with player {i}')
    return fails


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--players', type=int, default=2)
    ap.add_argument('--monsters', default='3,1,2,5,4,7,8,9')
    ap.add_argument('--stage', type=int, default=1)
    ap.add_argument('--wins', type=int, default=0)
    ap.add_argument('--mods', default='')
    ap.add_argument('--secs', type=int, default=70)
    ap.add_argument('--port', type=int, default=7811)
    ap.add_argument('--exe', default='')
    ap.add_argument('--drop', type=int, default=0)
    ap.add_argument('--extra-env', default='')
    ap.add_argument('--player-env', default='')
    ap.add_argument('--expect', default='')
    ap.add_argument('--reject', default='')
    ap.add_argument('--expect-refused', default='')
    a = ap.parse_args()

    rt = gamecheck.runtime_dir()
    server = build_server()
    exes = a.exe.split(',') if a.exe else [str(gamecheck.game_exe(rt))]
    elf = rt / 'discroot' / 'SCUS_971.97'
    out = rt / 'net'
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    base = gamecheck.apply_pairs(gamecheck.check_env(rt, res='360'), [('WOTM_NOINPUT', '1')] + gamecheck.env_pairs(a.extra_env))
    gamecheck.sync_mods(rt)

    procs = []
    with open(out / 'server.log', 'w') as slog:
        srv = subprocess.Popen([str(server), '--port', str(a.port)], stdout=slog, stderr=subprocess.STDOUT)
    try:
        time.sleep(0.5)
        names = ','.join(f'J{i}' for i in range(1, a.players + 1))
        mons = ','.join(a.monsters.split(',')[:a.players])
        lobby = [sys.executable, str(gamecheck.REPO / 'tools' / 'net_lobby.py'), '--server', f'127.0.0.1:{a.port}', '--group', names,
                 '--monsters', mons, '--stage', str(a.stage), '--wins', str(a.wins), '--env-dir', str(out)]
        if a.mods:
            lobby += ['--mods', a.mods]
        subprocess.run(lobby, check=True)
        for i in range(1, a.players + 1):
            env = dict(base)
            gamecheck.apply_pairs(env, gamecheck.env_pairs((out / f'env_{i}.txt').read_text().replace('\n', ';')))
            env['WOTM_STATEHASH'] = str(out / f'player{i}.hash')
            env['PS2X_PAD'] = SCRIPTS[(i - 1) % len(SCRIPTS)]
            gamecheck.apply_pairs(env, gamecheck.player_env(a.player_env, i))   # may replace this player's pad script
            # each player in their own folder, as on different PCs: own ELF, memory card and save.json (the rest comes from the ISO).
            # From player 2 on, the save has other options: the network match must not read it
            pdir = out / f'pc{i}'
            pdir.mkdir()
            shutil.copy(elf, pdir)
            if i >= 2:
                (pdir / 'save.json').write_text('{"options": {"difficulty": 2, "sfxVolume": %d, "musicVolume": 30, "mono": 1, '
                                                '"controllerConfig": [1, 1], "vibration": [0, 0]}, "battleTokens": 1234}' % (40 + i))
            exe = exes[min(i - 1, len(exes) - 1)]
            procs.append(gamecheck.launch(exe, pdir / 'SCUS_971.97', pdir, env, out / f'player{i}.err.log', out / f'player{i}.out.log'))
        if a.drop:
            time.sleep(a.drop)
            gamecheck.stop([procs[-1]], grace=2)
            time.sleep(max(1, a.secs - a.drop))
        else:
            time.sleep(a.secs)
    finally:
        gamecheck.stop(procs)
        srv.terminate()
        srv.wait()

    fails = verdict(a, out, gamecheck.read_log(out / 'server.log'))
    for f in fails:
        gamecheck.say(False, f)
    print('PASSED' if not fails else 'FAILED')
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
