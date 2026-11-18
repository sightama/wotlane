#!/usr/bin/env python3
# ABOUTME: Tests for the latency proxy (tools/net_lagproxy.py): round trip near the asked one, jitter within its spread, order kept, UDP and TCP, spikes.
# ABOUTME: Run: python tools/test_net_lagproxy.py
import socket
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import net_lagproxy  # noqa: E402

PROXY = Path(__file__).resolve().parent / 'net_lagproxy.py'


class LinkSpecTest(unittest.TestCase):
    def test_port_rtt_and_jitter(self):
        self.assertEqual(net_lagproxy.parse_link('9001:60:10'), (9001, 60.0, 10.0))

    def test_jitter_defaults_to_zero(self):
        self.assertEqual(net_lagproxy.parse_link('9001:20'), (9001, 20.0, 0.0))

    def test_one_way_delay_stays_within_the_spread(self):
        delays = [net_lagproxy.one_way_ms(60, 20, x) for x in (0.0, 0.5, 0.999)]
        self.assertAlmostEqual(delays[0], 20.0)
        self.assertAlmostEqual(delays[1], 30.0)
        self.assertLess(delays[2], 40.0)

    def test_one_way_delay_is_never_negative(self):
        self.assertEqual(net_lagproxy.one_way_ms(2, 20, 0.0), 0.0)


class ProxyTest(unittest.TestCase):
    """A UDP echo server behind the proxy: what goes through comes back after the asked round trip, in order."""

    def setUp(self):
        self.echo = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.echo.bind(('127.0.0.1', 0))
        self.echo.settimeout(0.2)
        self.running = True
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()
        free = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        free.bind(('127.0.0.1', 0))
        self.listen = free.getsockname()[1]   # a port the system just handed out, for the proxy to listen on
        free.close()
        self.proxies = []

    def tearDown(self):
        self.running = False
        for p in self.proxies:
            p.terminate()
            p.wait()
        self.thread.join()
        self.echo.close()

    def serve(self):
        while self.running:
            try:
                data, addr = self.echo.recvfrom(2048)
            except socket.timeout:
                continue
            self.echo.sendto(data, addr)

    def start_proxy(self, link):
        p = subprocess.Popen([sys.executable, str(PROXY), '--to', f'127.0.0.1:{self.echo.getsockname()[1]}', '--link', link],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.proxies.append(p)
        self.assertIn('listening', p.stdout.readline())

    def client(self):
        c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        c.settimeout(2)
        self.addCleanup(c.close)
        return c

    def test_round_trip_is_the_asked_one(self):
        self.start_proxy(f'{self.listen}:80:0')
        c = self.client()
        times = []
        for i in range(7):
            t0 = time.monotonic()
            c.sendto(b'x%d' % i, ('127.0.0.1', self.listen))
            data, _ = c.recvfrom(2048)
            times.append((time.monotonic() - t0) * 1000)
            self.assertEqual(data, b'x%d' % i)
        # never early; late only by scheduling (the median, so a loaded machine does not fail it)
        self.assertGreaterEqual(min(times), 78)
        self.assertLess(sorted(times)[len(times) // 2], 120)

    def test_jitter_keeps_order(self):
        self.start_proxy(f'{self.listen}:40:30')
        c = self.client()
        for i in range(60):
            c.sendto(b'%d' % i, ('127.0.0.1', self.listen))
            time.sleep(0.002)
        got = [int(c.recvfrom(2048)[0]) for _ in range(60)]
        self.assertEqual(got, list(range(60)))



class SpikeSpecTest(unittest.TestCase):
    def test_port_period_extra_and_length(self):
        self.assertEqual(net_lagproxy.parse_spike('9001:5:150:400'), (9001, 5.0, 150.0, 400.0))

    def test_extra_round_trip_only_inside_each_spike(self):
        spike = (5.0, 150.0, 400.0)   # every 5 s, 400 ms at +150 ms
        self.assertEqual(net_lagproxy.spike_extra_ms(spike, 4.9), 0.0)
        self.assertEqual(net_lagproxy.spike_extra_ms(spike, 5.1), 150.0)
        self.assertEqual(net_lagproxy.spike_extra_ms(spike, 5.5), 0.0)
        self.assertEqual(net_lagproxy.spike_extra_ms(None, 5.1), 0.0)


class TcpProxyTest(unittest.TestCase):
    """A TCP echo server behind the proxy (--tcp, the browsers' WebSocket path): bytes come back after the round trip, in order."""

    def setUp(self):
        self.echo = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.echo.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.echo.bind(('127.0.0.1', 0))
        self.echo.listen()
        self.echo.settimeout(0.2)
        self.running = True
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()
        free = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        free.bind(('127.0.0.1', 0))
        self.listen = free.getsockname()[1]
        free.close()
        self.proxies = []

    def tearDown(self):
        self.running = False
        for p in self.proxies:
            p.terminate()
            p.wait()
        self.thread.join()
        self.echo.close()

    def serve(self):
        while self.running:
            try:
                conn, _ = self.echo.accept()
            except socket.timeout:
                continue
            threading.Thread(target=self.pump, args=(conn,), daemon=True).start()

    def pump(self, conn):
        with conn:
            while True:
                data = conn.recv(4096)
                if not data:
                    return
                conn.sendall(data)

    def test_round_trip_and_order(self):
        p = subprocess.Popen([sys.executable, str(PROXY), '--tcp', '--to', f'127.0.0.1:{self.echo.getsockname()[1]}',
                              '--link', f'{self.listen}:80:20'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.proxies.append(p)
        self.assertIn('listening', p.stdout.readline())
        c = socket.create_connection(('127.0.0.1', self.listen), timeout=3)
        self.addCleanup(c.close)
        t0 = time.monotonic()
        c.sendall(b'a')
        self.assertEqual(c.recv(16), b'a')
        self.assertGreaterEqual((time.monotonic() - t0) * 1000, 68)   # 80 less the jitter's half on each way, at the least
        for i in range(50):
            c.sendall(b'%03d' % i)
        got = b''
        while len(got) < 150:
            got += c.recv(4096)
        self.assertEqual(got, b''.join(b'%03d' % i for i in range(50)))


if __name__ == '__main__':
    unittest.main()
