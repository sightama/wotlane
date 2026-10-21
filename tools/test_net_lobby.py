#!/usr/bin/env python3
# ABOUTME: Testes do cliente de lobby (tools/net_lobby.py) contra o wotm_server de verdade (native/net/build, tools/build_net.bat).
# ABOUTME: Rodar: python tools/test_net_lobby.py
import subprocess
import sys
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import net_lobby  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SERVER = REPO / 'native' / 'net' / 'build' / ('wotm_server.exe' if sys.platform == 'win32' else 'wotm_server')


class LobbyTest(unittest.TestCase):
    port = 7861

    @classmethod
    def setUpClass(cls):
        if not SERVER.exists():
            raise unittest.SkipTest(f'{SERVER} nao existe: rode tools\\build_net.bat')

    def setUp(self):
        LobbyTest.port += 1
        self.server = subprocess.Popen([str(SERVER), '--port', str(self.port), '--players', '4'], stdout=subprocess.DEVNULL)
        time.sleep(0.3)

    def tearDown(self):
        self.server.kill()
        self.server.wait()

    def test_group_forms_a_match_with_one_env_per_player(self):
        envs = net_lobby.form_group('127.0.0.1', self.port, ['Ana', 'Bia', 'Cai'], [3, 1, 2], stage=5, build='b1')
        self.assertEqual(len(envs), 3)
        a, b, c = envs
        self.assertEqual(a['WOTM_NET'], f'127.0.0.1:{self.port}')
        self.assertEqual(a['WOTM_MATCH'], '3,1,2,5')
        self.assertEqual(a['WOTM_PLAYERS'], '3')
        self.assertEqual(a['WOTM_FIXEDSTEP'], '2')     # 3+ jogadores: modo de 30 Hz do jogo
        self.assertEqual(a['WOTM_SEED'], c['WOTM_SEED'])
        self.assertEqual([e['WOTM_LOCALPLAYER'] for e in envs], ['1', '2', '3'])
        self.assertEqual([e['WOTM_NETNAME'] for e in envs], ['Ana', 'Bia', 'Cai'])
        self.assertEqual(len({e['WOTM_NETTOKEN'] for e in envs}), 3)

    def test_two_players_run_at_full_rate(self):
        a, b = net_lobby.form_group('127.0.0.1', self.port, ['Ana', 'Bia'], [3, 1], stage=1, build='b1')
        self.assertEqual(a['WOTM_FIXEDSTEP'], '1')
        self.assertEqual(b['WOTM_MATCH'], '3,1,1')

    def test_list_shows_lobbies_and_members(self):
        with net_lobby.Lobby('127.0.0.1', self.port, 'b1') as s, net_lobby.Lobby('127.0.0.1', self.port, 'b1') as watcher:
            s.join(1, 'Ana', 4)
            lobbies = watcher.list()
        self.assertEqual([l['index'] for l in lobbies], [0, 1])
        self.assertEqual(lobbies[1]['state'], 'waiting')
        self.assertEqual(lobbies[1]['names'], ['Ana'])
        self.assertEqual(lobbies[0]['names'], [])

    def test_server_errors_raise(self):
        with net_lobby.Lobby('127.0.0.1', self.port, 'b1') as a, net_lobby.Lobby('127.0.0.1', self.port, 'outro') as b:
            a.join(0, 'Ana', 3)
            with self.assertRaisesRegex(net_lobby.LobbyError, 'build'):
                b.join(0, 'Bia', 1)

    def test_cli_prints_env_lines(self):
        # dois membros pela linha de comando: o segundo comeca a partida
        first = subprocess.Popen([sys.executable, str(REPO / 'tools' / 'net_lobby.py'), '--server', f'127.0.0.1:{self.port}', '--name', 'Ana',
                                  '--monster', '3', '--build', 'b1'], stdout=subprocess.PIPE, text=True)
        time.sleep(0.5)
        second = subprocess.run([sys.executable, str(REPO / 'tools' / 'net_lobby.py'), '--server', f'127.0.0.1:{self.port}', '--name', 'Bia',
                                 '--monster', '1', '--build', 'b1', '--start', '2'], capture_output=True, text=True, timeout=10)
        out1, _ = first.communicate(timeout=10)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertIn('WOTM_MATCH=3,1,2', second.stdout)
        self.assertIn('WOTM_LOCALPLAYER=2', second.stdout)
        self.assertIn('WOTM_LOCALPLAYER=1', out1)


if __name__ == '__main__':
    unittest.main()
