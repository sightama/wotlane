# ABOUTME: Tests tools/diag_waits.py on a small synthetic match log: which windows count as steady, the per-player figures it prints,
# ABOUTME: and the error when no match log is found.
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diag_waits  # noqa: E402


def client(seq, player, gfps, waits, wait_ms, hz=60):
    return ('2026-11-24T10:00:%02dZ src=client lobby=TEST player=%s v=1 why=periodic seq=%d hz=%d fps=60.0 gfps=%s ee_busy_ms=8.0 '
            'fr_len=290,10,0,0 waits=%d wait_ms=%d raf_p95_ms=16.7 lat_ms=110.0\n' % (seq, player, seq, hz, gfps, waits, wait_ms))


class DiagWaitsTest(unittest.TestCase):
    def run_tool(self, *args):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            status = diag_waits.main(['diag_waits.py'] + list(args))
        return status, out.getvalue(), err.getvalue()

    def write_run(self, lines):
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, 'diag'))
        with open(os.path.join(d, 'diag', '20261124-100000-TEST-match.log'), 'w') as f:
            f.writelines(lines)
        return d

    def test_steady_windows_skip_the_first_ones_and_slow_ones(self):
        d = self.write_run([
            client(1, 'P1', 7.0, 90, 900),     # boot: skipped (seq <= 2)
            client(2, 'P1', 50.0, 80, 800),    # skipped (seq <= 2)
            client(3, 'P1', 58.0, 40, 500),
            client(4, 'P1', 30.0, 99, 3000),   # below 75% of 60 Hz: skipped
            client(5, 'P1', 58.0, 60, 700),
            '2026-11-24T10:00:30Z src=server lobby=TEST player=P1 slot=0 behind_avg=0.5\n',
            '2026-11-24T10:00:35Z src=server lobby=TEST player=P1 slot=0 behind_avg=1.5\n',
        ])
        status, out, _ = self.run_tool(d)
        self.assertEqual(status, 0)
        self.assertIn('P1: 2 windows, waits/5s median 50 mean 50.0, wait share 12.0%', out)
        self.assertIn('fr_len 580,20,0,0', out)
        self.assertIn('behind_avg 1.00', out)

    def test_the_rate_floor_follows_a_30_hz_match(self):
        d = self.write_run([client(3, 'P2', 27.0, 10, 100, hz=30), client(4, 'P2', 20.0, 10, 100, hz=30)])
        _, out, _ = self.run_tool(d)
        self.assertIn('P2: 1 windows', out)

    def test_a_player_with_no_steady_window_says_so(self):
        d = self.write_run([client(1, 'P1', 58.0, 1, 1)])
        _, out, _ = self.run_tool(d)
        self.assertIn('P1: no steady window (of 1)', out)

    def test_no_match_log_is_an_error(self):
        status, _, err = self.run_tool(tempfile.mkdtemp())
        self.assertEqual(status, 1)
        self.assertIn('no *-match.log', err)

    def test_bad_options_are_refused_with_the_usage(self):
        for args, why in ((['--skip'], '--skip needs a value'), (['--min-gfps', 'fast'], '--min-gfps: not a number'),
                          (['--frobnicate'], 'unknown option --frobnicate')):
            status, _, err = self.run_tool(*args)
            self.assertEqual(status, 2, args)
            self.assertIn(why, err)
            self.assertIn('Usage:', err)


if __name__ == '__main__':
    unittest.main()
