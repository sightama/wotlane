#!/usr/bin/env python3
# ABOUTME: Tests for tools/ws_ping.py: framing, runs against a local wotm_server (a pong per ping, none lost) and against stubs (a refused
# ABOUTME: path, a wrong accept key or a reset is an error, after the summary). Run: python3 tools/test_ws_ping.py
import socket
import struct
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import net_check  # noqa: E402
import ws_ping  # noqa: E402

PROBE = Path(__file__).resolve().parent / 'ws_ping.py'


class FrameTest(unittest.TestCase):
    def test_a_client_frame_is_masked_and_unmasks_to_the_payload(self):
        f = ws_ping.client_frame(0x9, b'\x00\x01\x02\x03\x04\x05\x06\x07')
        self.assertEqual(f[0], 0x89)          # FIN + ping
        self.assertEqual(f[1], 0x80 | 8)      # masked, 8 bytes
        mask = f[2:6]
        self.assertEqual(bytes(b ^ mask[i % 4] for i, b in enumerate(f[6:])), b'\x00\x01\x02\x03\x04\x05\x06\x07')

    def test_server_frames_are_split_and_a_partial_one_is_kept(self):
        pong = bytes([0x8A, 8]) + b'abcdefgh'
        text = bytes([0x81, 126, 0, 3]) + b'hey'
        frames, rest = ws_ping.server_frames(pong + text + pong[:5])
        self.assertEqual(frames, [(0xA, b'abcdefgh'), (0x1, b'hey')])
        self.assertEqual(rest, pong[:5])

    def test_a_long_frame_header_waits_for_its_length(self):
        frames, rest = ws_ping.server_frames(bytes([0x82, 127, 0, 0]))
        self.assertEqual(frames, [])
        self.assertEqual(rest, bytes([0x82, 127, 0, 0]))

    def test_the_accept_header_is_the_rfc_example(self):
        self.assertEqual(ws_ping.accept_key('dGhlIHNhbXBsZSBub25jZQ=='), 's3pPLMBiTxaQ9kYGzzhZRbK+xOo=')


class StubServerTest(unittest.TestCase):
    """A stub that answers the handshake its own way: a wrong accept key, or a reset after the first pong."""

    def serve(self, answer):
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        self.addCleanup(listener.close)

        def run():
            c, _ = listener.accept()
            with c:
                head = b''
                while b'\r\n\r\n' not in head:
                    head += c.recv(4096)
                key = [ln.split(b':', 1)[1].strip().decode() for ln in head.split(b'\r\n') if ln.lower().startswith(b'sec-websocket-key:')][0]
                answer(c, key)

        threading.Thread(target=run, daemon=True).start()
        return listener.getsockname()[1]

    def probe(self, port):
        return subprocess.run([sys.executable, str(PROBE), f'ws://127.0.0.1:{port}/lobby', '--secs', '2', '--every', '50'],
                              capture_output=True, text=True, timeout=20)

    def test_a_wrong_accept_key_is_an_error(self):
        port = self.serve(lambda c, key: c.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                                                   b'Sec-WebSocket-Accept: bm90IGl0\r\n\r\n'))
        r = self.probe(port)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)
        self.assertIn('Sec-WebSocket-Accept', r.stderr)
        self.assertNotIn('Traceback', r.stderr)

    def test_a_reset_mid_run_is_an_error_after_the_summary(self):
        def answer(c, key):
            c.sendall(('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                       f'Sec-WebSocket-Accept: {ws_ping.accept_key(key)}\r\n\r\n').encode())
            ping = c.recv(14)                          # the first ping frame: 2 header bytes, the mask, 8 payload bytes
            mask, body = ping[2:6], ping[6:14]
            c.sendall(bytes([0x8A, 8]) + bytes(b ^ mask[i % 4] for i, b in enumerate(body)))
            time.sleep(0.2)
            c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))   # close with a reset
        r = self.probe(self.serve(answer))
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(len(r.stdout.split('\n')[:-1]), 1)   # the one pong
        self.assertIn('error:', r.stderr)
        self.assertIn('# sent', r.stderr)
        self.assertNotIn('Traceback', r.stderr)


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


class ServerTest(unittest.TestCase):
    """A local wotm_server with a WebSocket port: the probe gets every pong back, quickly."""

    @classmethod
    def setUpClass(cls):
        cls.port = free_port()
        cls.server = subprocess.Popen([str(net_check.build_server()), '--port', str(free_port()), '--ws-port', str(cls.port)],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 10
        while time.time() < deadline:
            try:
                socket.create_connection(('127.0.0.1', cls.port), timeout=0.2).close()
                return
            except OSError:
                time.sleep(0.1)
        raise RuntimeError('wotm_server did not open its WebSocket port')

    @classmethod
    def tearDownClass(cls):
        cls.server.terminate()
        cls.server.wait(5)

    def test_every_ping_gets_a_pong(self):
        r = subprocess.run([sys.executable, str(PROBE), f'ws://127.0.0.1:{self.port}/lobby', '--secs', '1', '--every', '50'],
                           capture_output=True, text=True, timeout=20)
        self.assertEqual(r.returncode, 0, r.stderr)
        lines = r.stdout.split('\n')[:-1]
        self.assertGreaterEqual(len(lines), 10)
        for line in lines:
            t, rtt = line.split()
            self.assertAlmostEqual(float(t), time.time(), delta=30)
            self.assertLess(float(rtt), 100)
        self.assertIn('lost 0', r.stderr)

    def test_pings_still_in_flight_at_the_end_are_waited_for(self):
        r = subprocess.run([sys.executable, str(PROBE), f'ws://127.0.0.1:{self.port}/lobby', '--secs', '1', '--every', '1'],
                           capture_output=True, text=True, timeout=20)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('lost 0', r.stderr)

    def test_no_server_is_an_error(self):
        r = subprocess.run([sys.executable, str(PROBE), f'ws://127.0.0.1:{free_port()}/lobby', '--secs', '1'],
                           capture_output=True, text=True, timeout=20)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)
        self.assertNotIn('Traceback', r.stderr)

    def test_a_refused_path_is_an_error(self):
        r = subprocess.run([sys.executable, str(PROBE), f'ws://127.0.0.1:{self.port}/nope', '--secs', '1'],
                           capture_output=True, text=True, timeout=20)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)


if __name__ == '__main__':
    unittest.main()
