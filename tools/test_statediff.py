#!/usr/bin/env python3
# ABOUTME: Tests for tools/statediff.py: synthetic hash files and RAM dumps with known differences.
# ABOUTME: Run with: python -m unittest tools/test_statediff.py
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import statediff  # noqa: E402


def record(index, gframe, blocks):
    total = hash(tuple(blocks)) & 0xFFFFFFFFFFFFFFFF
    return statediff.RECORD.pack(index, gframe, total, *blocks)


class StateDiffTest(unittest.TestCase):
    def write(self, d, name, records):
        p = Path(d) / name
        p.write_bytes(b''.join(records))
        return p

    def test_identical_runs_have_no_difference(self):
        blocks = [7] * statediff.BLOCKS
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, 'a', [record(i, 100 + i, blocks) for i in range(5)])
            b = self.write(d, 'b', [record(i, 100 + i, blocks) for i in range(5)])
            first, n = statediff.compare_hashes(statediff.read_hashes(a), statediff.read_hashes(b))
        self.assertIsNone(first)
        self.assertEqual(n, 5)

    def test_first_differing_frame_and_block(self):
        same = [7] * statediff.BLOCKS
        changed = list(same)
        changed[5] = 8
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, 'a', [record(i, i, same) for i in range(4)])
            b = self.write(d, 'b', [record(0, 0, same), record(1, 1, same), record(2, 2, changed), record(3, 3, changed)])
            ha, hb = statediff.read_hashes(a), statediff.read_hashes(b)
            first, _ = statediff.compare_hashes(ha, hb)
            self.assertEqual(first, 2)
            self.assertEqual(statediff.differing_blocks(ha[first], hb[first]), [5])

    def test_ignored_blocks_do_not_count(self):
        same = [7] * statediff.BLOCKS
        noisy, real = list(same), list(same)
        noisy[3] = 9
        real[3], real[10] = 9, 9
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, 'a', [record(i, i, same) for i in range(3)])
            b = self.write(d, 'b', [record(0, 0, noisy), record(1, 1, noisy), record(2, 2, real)])
            first, _ = statediff.compare_hashes(statediff.read_hashes(a), statediff.read_hashes(b), ignore={3})
        self.assertEqual(first, 2)

    def test_symbol_lookup_inside_sized_symbol(self):
        syms = [(0x1000, 0x10, 'alpha'), (0x2000, 0x4, 'beta')]
        self.assertEqual(statediff.symbol_at(syms, 0x1008), (0x1000, 'alpha'))
        self.assertIsNone(statediff.symbol_at(syms, 0x1010))
        self.assertEqual(statediff.symbol_at(syms, 0x2000), (0x2000, 'beta'))


if __name__ == '__main__':
    unittest.main()
