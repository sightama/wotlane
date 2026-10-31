#!/usr/bin/env python3
# ABOUTME: Tests for the lobby client (tools/net_lobby.py) against the real wotm_server (native/net/build, tools/build_net.bat).
# ABOUTME: Run: python tools/test_net_lobby.py
import re
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
            raise unittest.SkipTest(f'{SERVER} does not exist: run tools\\build_net.bat')

    def setUp(self):
        LobbyTest.port += 1
        self.server = subprocess.Popen([str(SERVER), '--port', str(self.port), '--players', '4'], stdout=subprocess.DEVNULL)
        time.sleep(0.3)

    def tearDown(self):
        self.server.kill()
        self.server.wait()

    def test_group_forms_a_match_with_one_env_per_player(self):
        envs = net_lobby.form_group('127.0.0.1', self.port, ['Ana', 'Bia', 'Cai'], [3, 1, 2], stage=5, build='b1', wins=3)
        self.assertEqual(len(envs), 3)
        a, b, c = envs
        self.assertEqual(a['WOTM_NET'], f'127.0.0.1:{self.port}')
        self.assertEqual(a['WOTM_MATCH'], '3,1,2,5')
        self.assertEqual(a['WOTM_PLAYERS'], '3')
        self.assertEqual(a['WOTM_FIXEDSTEP'], '2')     # 3+ players: the game's 30 Hz mode
        self.assertEqual(a['WOTM_SEED'], c['WOTM_SEED'])
        self.assertEqual([e['WOTM_KILLTARGET'] for e in envs], ['3', '3', '3'])
        self.assertEqual([e['WOTM_LOCALPLAYER'] for e in envs], ['1', '2', '3'])
        self.assertEqual([e['WOTM_NETNAME'] for e in envs], ['Ana', 'Bia', 'Cai'])
        self.assertEqual(len({e['WOTM_NETTOKEN'] for e in envs}), 3)

    def test_two_players_run_at_full_rate_and_endless_by_default(self):
        a, b = net_lobby.form_group('127.0.0.1', self.port, ['Ana', 'Bia'], [3, 1], stage=1, build='b1')
        self.assertEqual(a['WOTM_FIXEDSTEP'], '1')
        self.assertEqual(b['WOTM_MATCH'], '3,1,1')
        self.assertEqual(b['WOTM_KILLTARGET'], '0')

    def test_list_shows_lobbies_with_code_host_and_lock(self):
        with net_lobby.Lobby('127.0.0.1', self.port, 'b1') as s, net_lobby.Lobby('127.0.0.1', self.port, 'b1') as t, \
                net_lobby.Lobby('127.0.0.1', self.port, 'b1') as watcher:
            self.assertEqual(watcher.list(), [])
            code = s.create('Sala', 'Ana', 4)
            locked = t.create('Fechada', 'Bia', 1, password='abc')
            lobbies = {l['code']: l for l in watcher.list()}
        self.assertEqual(len(code), 4)
        self.assertEqual(set(lobbies), {code, locked})
        self.assertEqual(lobbies[code]['state'], 'waiting')
        self.assertEqual(lobbies[code]['names'], ['Ana'])
        self.assertEqual(lobbies[code]['host'], 'Ana')
        self.assertEqual(lobbies[code]['name'], 'Sala')
        self.assertFalse(lobbies[code]['locked'])
        self.assertTrue(lobbies[locked]['locked'])

    def test_join_by_code_with_password_and_host_only_settings(self):
        with net_lobby.Lobby('127.0.0.1', self.port, 'b1') as a, net_lobby.Lobby('127.0.0.1', self.port, 'b1') as b:
            code = a.create('Sala', 'Ana', 3, password='pw')
            with self.assertRaisesRegex(net_lobby.LobbyError, 'password'):
                b.join(code, 'Bia', 1)
            b.join(code.lower(), 'Bia', 1, password='pw')
            with self.assertRaisesRegex(net_lobby.LobbyError, 'owner'):
                b.set(wins=2)
            self.assertEqual(a.set(stage=4, wins=2), {'stage': '4', 'wins': '2'})

    def test_server_errors_raise(self):
        with net_lobby.Lobby('127.0.0.1', self.port, 'b1') as a, net_lobby.Lobby('127.0.0.1', self.port, 'other') as b:
            code = a.create('Sala', 'Ana', 3)
            with self.assertRaisesRegex(net_lobby.LobbyError, 'build'):
                b.join(code, 'Bia', 1)
            with self.assertRaisesRegex(net_lobby.LobbyError, 'no such lobby'):
                b.join('QQQQ' if code != 'QQQQ' else 'RRRR', 'Bia', 1)

    def test_cli_prints_env_lines(self):
        # the owner creates from the command line and starts when the second player joins by code
        tool = [sys.executable, str(REPO / 'tools' / 'net_lobby.py'), '--server', f'127.0.0.1:{self.port}', '--build', 'b1']
        first = subprocess.Popen(tool + ['--create', 'Sala', '--name', 'Ana', '--monster', '3', '--stage', '2', '--wins', '4', '--start-with', '2'],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        code = ''
        deadline = time.time() + 10
        while not code and time.time() < deadline:
            line = first.stderr.readline()
            m = re.search(r'code ([A-Z0-9]{4})', line)
            code = m.group(1) if m else ''
        self.assertEqual(len(code), 4)
        second = subprocess.run(tool + ['--code', code, '--name', 'Bia', '--monster', '1'], capture_output=True, text=True, timeout=10)
        out1, _ = first.communicate(timeout=10)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertIn('WOTM_MATCH=3,1,2', second.stdout)
        self.assertIn('WOTM_KILLTARGET=4', second.stdout)
        self.assertIn('WOTM_LOCALPLAYER=2', second.stdout)
        self.assertIn('WOTM_LOCALPLAYER=1', out1)

    # match between two PCs: the second player joins (or the owner starts) after the Lobby's 10 s read timeout
    def test_host_waits_longer_than_the_read_timeout_for_players(self):
        tool = [sys.executable, str(REPO / 'tools' / 'net_lobby.py'), '--server', f'127.0.0.1:{self.port}', '--build', 'b1']
        first = subprocess.Popen(tool + ['--create', 'Sala', '--name', 'Ana', '--monster', '3', '--start-with', '2'],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        m = re.search(r'code ([A-Z0-9]{4})', first.stderr.readline())
        self.assertIsNotNone(m)
        time.sleep(12)
        second = subprocess.run(tool + ['--code', m.group(1), '--name', 'Bia', '--monster', '1'], capture_output=True, text=True, timeout=10)
        out1, err1 = first.communicate(timeout=10)
        self.assertEqual(first.returncode, 0, err1)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertIn('WOTM_LOCALPLAYER=1', out1)

    def test_joiner_waits_longer_than_the_read_timeout_for_the_start(self):
        tool = [sys.executable, str(REPO / 'tools' / 'net_lobby.py'), '--server', f'127.0.0.1:{self.port}', '--build', 'b1']
        with net_lobby.Lobby('127.0.0.1', self.port, 'b1') as host:
            code = host.create('Sala', 'Ana', 3)
            second = subprocess.Popen(tool + ['--code', code, '--name', 'Bia', '--monster', '1'],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            host.wait_members(2)
            time.sleep(12)
            host.start()
            out2, err2 = second.communicate(timeout=10)
        self.assertEqual(second.returncode, 0, err2)
        self.assertIn('WOTM_LOCALPLAYER=2', out2)


if __name__ == '__main__':
    unittest.main()
