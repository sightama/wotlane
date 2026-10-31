#!/usr/bin/env python3
# ABOUTME: Tests for tools/interp_smooth.py with synthetic logs in wotm_live.inc's format: game time shown with a constant delay (smooth)
# ABOUTME: versus the last game frame held until the next one (game at 60 on a 144 Hz screen). Run: python tools/test_interp_smooth.py
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import interp_smooth  # noqa: E402

HZ = 144.0


def log(content_fields):
    """One [interp] line per presentation (t in s); the game time shown (fields) becomes fA, fB = fA + 1 and the alpha between them."""
    lines = []
    for i, ct in enumerate(content_fields):
        fa = math.floor(ct)
        lines.append(f'[interp] t={i / HZ:.6f} g={fa} fA={fa} fB={fa + 1} alpha={ct - fa:.4f}')
    return '\n'.join(lines)


class LagTest(unittest.TestCase):
    def test_interpolated_image_has_constant_lag(self):
        # what is shown is the game 1.15 fields behind the screen's clock
        cts = [10.0 + i / HZ * 60.0 - 1.15 for i in range(1000)]
        spread, n = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertEqual(n, 900)   # the first tenth is left out (clock settling)
        self.assertLess(spread, 0.5)

    def test_held_frames_give_a_sawtooth_lag(self):
        # no interpolation: the newest (whole) game frame stays up until the next one
        cts = [float(int(i / HZ * 60.0)) for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertGreater(spread, 12.0)   # sawtooth of ~16.7 ms

    def test_every_other_presentation_redrawn_is_still_even(self):
        # interpolation at 72 redraws/s on a 144 Hz screen: each image stays up 2 vblanks, the delay swings by half the screen's period
        cts = [10.0 + (i - i % 2) / HZ * 60.0 - 1.15 for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertLess(spread, 8.0)

    def test_a_pause_is_a_step_not_judder(self):
        # the game stopped for 0.3 s in the middle (a load): the delay jumps once and stays; the image is smooth before and after
        cts = [30.0 + i / HZ * 60.0 - 1.15 - (18.0 if i >= 500 else 0.0) for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertLess(spread, 0.5)

    def test_live_lines_give_the_blended_game_time(self):
        # the game time shown is fA + alpha * (fB - fA), and without blending (off) the image is B's
        text = '\n'.join([
            '[interp] t=1.0000 g=100 fA=200 fB=201 alpha=0.250 new',
            '[interp] t=1.0070 g=100 fA=200 fB=202 alpha=0.500 (P-A)',
            '[interp] t=1.0140 g=101 fA=201 fB=202 alpha=0.000 (off)',
        ])
        self.assertEqual(interp_smooth.read_presentations(text), [(1.0, 200.25), (1.007, 201.0), (1.014, 202.0)])

    def test_held_images_without_blending_show_the_sawtooth(self):
        held = []
        for i in range(1000):
            newest = int(i / HZ * 60.0)
            held.append(f'[interp] t={i / HZ:.6f} g={newest} fA={newest - 1} fB={newest} alpha=0.000 (off)')
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations('\n'.join(held)))
        self.assertGreater(spread, 12.0)

    def test_other_interp_lines_are_ignored(self):
        text = '[interp] on: 144 Hz screen\n[interp] redraw of 9.00 ms for a period of 6.94 ms: blending suspended'
        self.assertEqual(interp_smooth.read_presentations(text), [])


if __name__ == '__main__':
    unittest.main()
