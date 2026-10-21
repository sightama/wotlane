#!/usr/bin/env python3
# ABOUTME: Cliente de linha de comando do lobby do wotm_server: lista lobbies, entra, comeca a partida e imprime as variaveis de
# ABOUTME: ambiente do wotm.exe (WOTM_NET, WOTM_NETTOKEN, WOTM_MATCH...). Usado pelo tools/net_check.ps1 e por quem joga sem o launcher.
"""Uso:
  python tools/net_lobby.py --server <host>:<porta> --list
  python tools/net_lobby.py --server <host>:<porta> --name Ana --monster 3 [--lobby 0] [--start <fase>]
      entra no lobby e espera a partida (quem passa --start a comeca com quem estiver la); imprime NOME=valor, uma por linha
  python tools/net_lobby.py --server <host>:<porta> --group Ana,Bia --monsters 3,1 --stage 1 --env-dir <pasta>
      entra com varios nomes de uma vez (teste num PC so), comeca e grava <pasta>/env_<n>.txt para cada jogador

O build (--build) tem que ser o mesmo do wotm.exe: o padrao e o wotm.buildid ao lado do exe do runtime (tools\\build_runtime.ps1).
"""
import argparse
import socket
import sys
from pathlib import Path

PROTOCOL = 1
REPO = Path(__file__).resolve().parent.parent


class LobbyError(Exception):
    pass


class Lobby:
    """Uma conexao com o lobby (protocolo de linhas de native/net/wotm_net_proto.hpp)."""

    def __init__(self, host, port, build, timeout=10.0):
        self.host, self.port = host, port
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b''
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
                raise LobbyError('o servidor fechou a conexao')
            self.buf += data
        line, self.buf = self.buf.split(b'\n', 1)
        return line.decode(errors='replace').rstrip('\r')

    def expect(self, prefix):
        """Le ate uma linha que comeca com prefix; ERROR vira LobbyError."""
        while True:
            line = self.read()
            if line.startswith('ERROR'):
                raise LobbyError(line[6:])
            if line.startswith(prefix):
                return line

    def list(self):
        self.send('LIST')
        out = []
        while True:
            line = self.expect('')
            if line == 'END':
                return out
            w = line.split()
            if w[0] != 'LOBBY':
                continue
            count, maximum = w[3].split('/')
            out.append({'index': int(w[1]), 'state': w[2], 'count': int(count), 'max': int(maximum),
                        'names': [] if w[4] == '-' else w[4].split(',')})

    def join(self, lobby, name, monster):
        self.send(f'JOIN {lobby} {name} {monster}')
        self.expect('JOINED')

    def start(self, stage):
        self.send(f'START {stage}')

    def wait_match(self):
        return dict(kv.split('=', 1) for kv in self.expect('MATCH').split()[1:] if '=' in kv)


def match_env(match, host):
    """Variaveis de ambiente do wotm.exe para uma linha MATCH."""
    players = int(match['players'])
    env = {
        'WOTM_NET': f"{host}:{match['port']}",
        'WOTM_NETTOKEN': match['token'],
        'WOTM_NETNAME': match['names'].split(',')[int(match['you'])],
        'WOTM_MATCH': f"{match['monsters']},{match['stage']}",
        'WOTM_PLAYERS': str(players),
        'WOTM_FIXEDSTEP': match['step'],
        'WOTM_SEED': match['seed'],
        'WOTM_LOCALPLAYER': str(int(match['you']) + 1),
    }
    return env


def form_group(host, port, names, monsters, stage, build, lobby=0):
    """Entra com todos os nomes no mesmo lobby, o primeiro comeca; devolve o ambiente de cada jogador, na ordem."""
    members = []
    try:
        for name, monster in zip(names, monsters):
            members.append(Lobby(host, port, build))
            members[-1].join(lobby, name, monster)
        members[0].start(stage)
        return [match_env(m.wait_match(), host) for m in members]
    finally:
        for m in members:
            m.close()


def default_build():
    """wotm.buildid ao lado do wotm.exe do runtime (o mesmo que o exe manda ao servidor)."""
    saved = REPO / '.wotm_recomp_dir'
    runtime = Path(saved.read_text().splitlines()[0].strip()) if saved.exists() else REPO / 'runtime'
    f = runtime / 'build_dev' / 'ps2xRuntime' / 'wotm.buildid'
    return f.read_text().strip() if f.exists() else 'sem-buildid'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--server', required=True, help='host:porta do wotm_server')
    ap.add_argument('--build', default=None)
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--lobby', type=int, default=0)
    ap.add_argument('--name')
    ap.add_argument('--monster', type=int, default=0)
    ap.add_argument('--start', type=int, default=None, metavar='FASE')
    ap.add_argument('--group')
    ap.add_argument('--monsters')
    ap.add_argument('--stage', type=int, default=1)
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
                    print(f"lobby {l['index']}: {l['state']} {l['count']}/{l['max']} {', '.join(l['names']) or '-'}")
            return 0
        if a.group:
            names = a.group.split(',')
            monsters = [int(m) for m in a.monsters.split(',')]
            envs = form_group(host, int(port), names, monsters, a.stage, build, a.lobby)
            out = Path(a.env_dir or '.')
            for i, env in enumerate(envs, 1):
                (out / f'env_{i}.txt').write_text(''.join(f'{k}={v}\n' for k, v in env.items()))
            print(f'partida com {len(envs)} jogadores: ambientes em {out}')
            return 0
        if not a.name:
            ap.error('--name, --group ou --list')
        with Lobby(host, int(port), build) as m:
            m.join(a.lobby, a.name, a.monster)
            if a.start is not None:
                m.start(a.start)
            for k, v in match_env(m.wait_match(), host).items():
                print(f'{k}={v}')
        return 0
    except (LobbyError, OSError) as e:
        print(f'lobby: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
