# ABOUTME: Builds and runs tools/timesync_sim and checks the findings it backs (docs/investigacoes/2026-11-24/time-sync): with a peer at
# ABOUTME: ~55 fps the proportional stretch leaves the leader's waits as they are, and the PI stretch cuts them but slows the match.
import os
import re
import shutil
import subprocess
import unittest

RUN = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'timesync_sim', 'run.sh')
ROW = re.compile(r'gfps\s+([\d.]+)\s+waits/5s P1\s+([\d.]+) P2\s+([\d.]+)')


def scenario(*args):
    out = subprocess.run(['sh', RUN] + list(args), capture_output=True, text=True, check=True).stdout
    m = ROW.search(out)
    if not m:
        raise AssertionError('unexpected output: %r' % out)
    return float(m.group(1)), float(m.group(2)), float(m.group(3))


@unittest.skipUnless(shutil.which(os.environ.get('CXX', 'c++')), 'no C++ compiler')
class TimesyncSimTest(unittest.TestCase):
    SLOW_PEER = ['--work', '7,17.5', '--secs', '60']

    def test_proportional_stretch_leaves_the_waits(self):
        _, off, _ = scenario('--sync', 'off', *self.SLOW_PEER)
        _, p, _ = scenario('--sync', 'p', *self.SLOW_PEER)
        self.assertGreater(off, 40)
        self.assertGreater(p, 0.85 * off)

    def test_pi_stretch_trades_waits_for_a_slower_match(self):
        gfps_off, off, _ = scenario('--sync', 'off', *self.SLOW_PEER)
        gfps_pi, pi, _ = scenario('--sync', 'pi', *self.SLOW_PEER)
        self.assertLess(pi, 0.6 * off)
        self.assertLess(gfps_pi, gfps_off)

    def test_a_bad_option_is_refused(self):
        r = subprocess.run(['sh', RUN, '--sync', 'fast'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 2)
        self.assertIn('--sync: off, p or pi', r.stderr)

    def test_out_of_range_values_are_refused(self):
        for args in (['--secs', '0'], ['--delay', '-1'], ['--jitter', '-5'], ['--secs', 'abc']):
            r = subprocess.run(['sh', RUN] + args, capture_output=True, text=True)
            self.assertEqual(r.returncode, 2, args)
            self.assertIn(args[0], r.stderr)
            self.assertIn('usage:', r.stderr)

    def test_one_scenario_prints_the_lead_the_leader_saw(self):
        out = subprocess.run(['sh', RUN, '--sync', 'p', '--work', '7,17.5', '--secs', '60'], capture_output=True, text=True,
                             check=True).stdout
        m = re.search(r'P1 lead mean ([\d.]+) max ([\d.]+)', out)
        self.assertTrue(m, out)
        self.assertLess(float(m.group(1)), 1.5)   # the barrier holds the lead near one frame on average


if __name__ == '__main__':
    unittest.main()
