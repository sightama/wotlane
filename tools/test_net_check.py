#!/usr/bin/env python3
# ABOUTME: Tests for the pure parts of tools/net_check.py: the per-player simulated round trips and the pace summary read from a log.
# ABOUTME: Run: python tools/test_net_check.py
import argparse
import socket
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import net_check  # noqa: E402

LOG = """[net] start: player 1 of 2, delay 5 frames, ping 61 ms
[net] pace: frame 600, delay 5, ping 62 ms, 3 stalls (12.5 ms) in the last 600 frames
[net] pace: frame 1200, delay 4, ping 60 ms, 0 stalls (0.0 ms) in the last 600 frames
[net] pace: frame 1800, delay 4, ping 61 ms, 1 stalls (4.5 ms) in the last 600 frames
"""


class RttTest(unittest.TestCase):
    def test_last_value_covers_the_rest(self):
        self.assertEqual(net_check.rtts('20,60', 4), [20.0, 60.0, 60.0, 60.0])

    def test_none_without_rtt(self):
        self.assertEqual(net_check.rtts('', 3), [])


class PaceTest(unittest.TestCase):
    def test_summary_averages_the_delay_and_sums_the_stalls(self):
        p = net_check.pace(LOG)
        self.assertEqual(p['windows'], 3)
        self.assertAlmostEqual(p['delay'], 13 / 3)
        self.assertEqual(p['delays'], [5, 4, 4])
        self.assertEqual(p['stalls'], 4)
        self.assertAlmostEqual(p['stall_ms'], 17.0)
        self.assertEqual(p['frames'], 1800)

    def test_no_pace_lines(self):
        self.assertIsNone(net_check.pace('[net] start: player 1 of 2, delay 5 frames, ping 61 ms\n'))


class ProxyStartTest(unittest.TestCase):
    def test_a_proxy_that_cannot_listen_stops_the_check(self):
        busy = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        busy.bind(('127.0.0.1', 0))
        self.addCleanup(busy.close)
        a = argparse.Namespace(port=busy.getsockname()[1] - 101, rtt='20', jitter=0.0, players=1)   # player 1 listens on port + 101
        with tempfile.TemporaryDirectory() as out, self.assertRaises(SystemExit) as stop:
            net_check.start_proxy(a, Path(out))
        self.assertIn('lag proxy', str(stop.exception.code))


if __name__ == '__main__':
    unittest.main()
