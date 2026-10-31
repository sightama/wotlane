#!/usr/bin/env python3
# ABOUTME: Command-line client for the wotm_server lobby: lists lobbies, creates one or joins by code, starts the match and prints the
# ABOUTME: wotm.exe environment variables (WOTM_NET, WOTM_NETTOKEN, WOTM_MATCH...). Used by tools/net_check.ps1 and by players without the launcher.
"""Usage:
  python tools/net_lobby.py --server <host>:<port> --list
  python tools/net_lobby.py --server <host>:<port> --create <lobby> --name Ana --monster 3 [--password <password>]
                            [--stage <stage>] [--wins <wins>] [--start-with <players>]
      creates the lobby (you are the owner; the code goes to stderr), starts the match once <players> are in it and prints NAME=value
  python tools/net_lobby.py --server <host>:<port> --code <code> --name Bia --monster 1 [--password <password>]
      joins the lobby by code, waits for the owner to start and prints NAME=value, one per line
  python tools/net_lobby.py --server <host>:<port> --group Ana,Bia --monsters 3,1 --stage 1 [--wins N] --env-dir <folder>
      joins with several names at once (testing on one PC: the first creates and starts) and writes <folder>/env_<n>.txt per player

The build (--build) must match the wotm.exe's: the default is the wotm.buildid next to the runtime's exe (tools\\build_runtime.ps1).
"""
import argparse
import socket
import sys
from pathlib import Path

PROTOCOL = 2
REPO = Path(__file__).resolve().parent.parent


class LobbyError(Exception):
    pass


class Lobby:
    """One connection to the lobby (line protocol from native/net/wotm_net_proto.hpp)."""

    def __init__(self, host, port, build, timeout=10.0):
        self.host, self.port = host, port
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b''
        self.code = ''
        self.send(f'HELLO {PROTOCOL} {build}')
        self.lobbies = int(self.expect('WELCOME').split()[1])

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def send(self, line):
        self.sock.sendall((line + '\n').encode())

    def read(self):
        while b'\n' not in self.buf:
            data = self.sock.recv(4096)
            if not data:
                raise LobbyError('the server closed the connection')
            self.buf += data
        line, self.buf = self.buf.split(b'\n', 1)
        return line.decode(errors='replace').rstrip('\r')

    def expect(self, prefix):
        """Reads up to a line that starts with prefix; ERROR becomes LobbyError."""
        while True:
            line = self.read()
            if line.startswith('ERROR'):
                raise LobbyError(line[6:])
            if line.startswith(prefix):
                return line

    def expect_waiting(self, prefix):
        """expect without a timeout: waits on another player (joining the lobby, starting the match), who may be on another PC."""
        timeout = self.sock.gettimeout()
        self.sock.settimeout(None)
        try:
            return self.expect(prefix)
        finally:
            self.sock.settimeout(timeout)

    def list(self):
        """Open lobbies: code, state, players, slots, whether it has a password, name, owner and member names."""
        self.send('LIST')
        out = []
        while True:
            line = self.expect('')
            if line == 'END':
                return out
            w = line.split()
            if w[0] != 'LOBBY' or len(w) < 8:
                continue
            count, maximum = w[3].split('/')
            out.append({'code': w[1], 'state': w[2], 'count': int(count), 'max': int(maximum), 'locked': w[4] == 'locked',
                        'name': w[5], 'host': w[6], 'names': [] if w[7] == '-' else w[7].split(',')})

    def create(self, lobby, name, monster, password=None):
        """Creates a lobby (the creator is the owner) and joins it; returns the code."""
        self.send(f'CREATE {lobby} {name} {monster}' + (f' {password}' if password else ''))
        self.code = self.expect('CREATED').split()[1]
        self._joined()
        return self.code

    def join(self, code, name, monster, password=None):
        self.send(f'JOIN {code} {name} {monster}' + (f' {password}' if password else ''))
        self._joined()

    def _joined(self):
        """JOINED and the SETTINGS the server sends right after it (the lobby's stage and wins)."""
        self.code = self.expect('JOINED').split()[1]
        self.settings = self._settings(self.expect('SETTINGS'))

    @staticmethod
    def _settings(line):
        return dict(kv.split('=', 1) for kv in line.split()[2:] if '=' in kv)

    def set(self, stage=None, wins=None):
        """Owner only: stage and wins for the next match; returns what the server stored."""
        parts = ([f'stage={stage}'] if stage is not None else []) + ([f'wins={wins}'] if wins is not None else [])
        self.send('SET ' + ' '.join(parts))
        self.settings = self._settings(self.expect('SETTINGS'))
        return self.settings

    def wait_members(self, count):
        """Waits until the lobby has `count` members (MEMBERS lines)."""
        while True:
            w = self.expect_waiting('MEMBERS').split()
            if len(w) >= 4 and w[3] != '-' and len(w[3].split(',')) >= count:
                return

    def start(self):
        self.send('START')

    def wait_match(self):
        return dict(kv.split('=', 1) for kv in self.expect_waiting('MATCH').split()[1:] if '=' in kv)


def match_env(match, host):
    """wotm.exe environment variables for a MATCH line."""
    players = int(match['players'])
    env = {
        'WOTM_NET': f"{host}:{match['port']}",
        'WOTM_NETTOKEN': match['token'],
        'WOTM_NETNAME': match['names'].split(',')[int(match['you'])],
        'WOTM_MATCH': f"{match['monsters']},{match['stage']}",
        'WOTM_PLAYERS': str(players),
        'WOTM_FIXEDSTEP': match['step'],
        'WOTM_SEED': match['seed'],
        'WOTM_KILLTARGET': match.get('wins', '0'),
        'WOTM_LOCALPLAYER': str(int(match['you']) + 1),
    }
    return env


def form_group(host, port, names, monsters, stage, build, wins=0):
    """The first name creates a lobby, the others join by code, the first picks stage and wins and starts; returns each
    player's environment, in order."""
    members = []
    try:
        for i, (name, monster) in enumerate(zip(names, monsters)):
            members.append(Lobby(host, port, build))
            if i == 0:
                code = members[0].create('teste', name, monster)
            else:
                members[-1].join(code, name, monster)
        members[0].set(stage=stage, wins=wins)
        members[0].start()
        return [match_env(m.wait_match(), host) for m in members]
    finally:
        for m in members:
            m.close()


def default_build():
    """The wotm.buildid next to the runtime's wotm.exe (the same one the exe sends to the server)."""
    saved = REPO / '.wotm_recomp_dir'
    runtime = Path(saved.read_text().splitlines()[0].strip()) if saved.exists() else REPO / 'runtime'
    f = runtime / 'build_dev' / 'ps2xRuntime' / 'wotm.buildid'
    return f.read_text().strip() if f.exists() else 'sem-buildid'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--server', required=True, help='host:port of the wotm_server')
    ap.add_argument('--build', default=None)
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--create', metavar='LOBBY')
    ap.add_argument('--code')
    ap.add_argument('--password')
    ap.add_argument('--name')
    ap.add_argument('--monster', type=int, default=0)
    ap.add_argument('--start-with', type=int, default=0, metavar='PLAYERS')
    ap.add_argument('--group')
    ap.add_argument('--monsters')
    ap.add_argument('--stage', type=int, default=1)
    ap.add_argument('--wins', type=int, default=0)
    ap.add_argument('--env-dir')
    a = ap.parse_args()
    host, _, port = a.server.rpartition(':')
    if not host:
        host, port = a.server, '7777'
    build = a.build or default_build()
    try:
        if a.list:
            with Lobby(host, int(port), build) as s:
                for l in s.list():
                    print(f"{l['code']} {l['name']} (owner {l['host']}): {l['state']} {l['count']}/{l['max']}"
                          f"{' with password' if l['locked'] else ''} {', '.join(l['names']) or '-'}")
            return 0
        if a.group:
            names = a.group.split(',')
            monsters = [int(m) for m in a.monsters.split(',')]
            envs = form_group(host, int(port), names, monsters, a.stage, build, a.wins)
            out = Path(a.env_dir or '.')
            for i, env in enumerate(envs, 1):
                (out / f'env_{i}.txt').write_text(''.join(f'{k}={v}\n' for k, v in env.items()))
            print(f'match with {len(envs)} players: environments in {out}')
            return 0
        if not a.name or not (a.create or a.code):
            ap.error('--list, --group, or --name with --create or --code')
        with Lobby(host, int(port), build) as m:
            if a.create:
                code = m.create(a.create, a.name, a.monster, a.password)
                print(f'lobby created, code {code}', file=sys.stderr, flush=True)
                m.set(stage=a.stage, wins=a.wins)
                if a.start_with:
                    m.wait_members(a.start_with)
                    m.start()
            else:
                m.join(a.code, a.name, a.monster, a.password)
            for k, v in match_env(m.wait_match(), host).items():
                print(f'{k}={v}')
        return 0
    except (LobbyError, OSError) as e:
        print(f'lobby: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
