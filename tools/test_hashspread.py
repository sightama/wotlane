#!/usr/bin/env python3
# ABOUTME: Tests for tools/hashspread.py (how a state difference between two runs spreads over the 128 KB blocks, frame by frame).
# ABOUTME: Run: python tools/test_hashspread.py
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hashspread  # noqa: E402


def record(index, blocks):
    full = [0] * hashspread.BLOCKS
    for b, v in blocks.items():
        full[b] = v
    return hashspread.RECORD.pack(index, index, 0, *full)


class HashSpread(unittest.TestCase):
    def write(self, d, name, records):
        p = Path(d) / name
        p.write_bytes(b"".join(records))
        return str(p)

    def test_same_runs_never_differ(self):
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, "a.hash", [record(f, {1: f}) for f in range(10)])
            s = hashspread.spread(hashspread.load(a), hashspread.load(a), [5])
            self.assertIsNone(s["first"])
            self.assertEqual(s["most"], 0)
            self.assertEqual(s["ever"], [])
            self.assertEqual(s["at"], {5: 0})

    def test_names_first_frame_worst_count_and_every_block(self):
        with tempfile.TemporaryDirectory() as d:
            ref = self.write(d, "r.hash", [record(f, {}) for f in range(6)])
            run = self.write(d, "x.hash", [record(0, {}), record(1, {}), record(2, {7: 1}), record(3, {7: 1, 9: 1}),
                                           record(4, {9: 1}), record(5, {})])
            s = hashspread.spread(hashspread.load(ref), hashspread.load(run), [2, 3, 5, 99])
            self.assertEqual(s["first"], 2)
            self.assertEqual(s["most"], 2)
            self.assertEqual(s["ever"], [7, 9])
            self.assertEqual(s["at"], {2: 1, 3: 2, 5: 0})   # frames neither run has are left out
            self.assertEqual(s["frames"], 6)

    def test_compares_only_the_frames_both_runs_have(self):
        with tempfile.TemporaryDirectory() as d:
            ref = self.write(d, "r.hash", [record(f, {}) for f in range(3)])
            run = self.write(d, "x.hash", [record(f, {}) for f in range(1, 5)])
            self.assertEqual(hashspread.spread(hashspread.load(ref), hashspread.load(run), [])["frames"], 2)


if __name__ == "__main__":
    unittest.main()
