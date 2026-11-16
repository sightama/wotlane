#!/usr/bin/env python3
# ABOUTME: Tests for tools/web_boot_chunks.py: the 1 MiB disc chunks a boot reads, from the game's log lines (disc and sound reads by
# ABOUTME: sector, the whole ELF), as the list web/disc_cache.js prefetches. Run: python3 tools/test_web_boot_chunks.py
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import web_boot_chunks  # noqa: E402

MIB = 1 << 20


class BootChunksTest(unittest.TestCase):
    def test_reads_become_the_chunks_they_touch(self):
        log = [
            '1.47s [boot]   0.77 s  disc: LSN 401001, 63 sectors (ISO) in 0.1 ms',
            '1.46s [boot]   0.77 s  sound: LSN 512, 1 sectors (ISO) in 3.0 ms',
            '2.54s [match] 1: monster 3:0 vs 1:0, stage 1',
            '3.00s [boot]   2.00 s  disc: LSN 1000, 1 sectors (ISO) in 9.0 ms FAILED',
        ]
        chunks = web_boot_chunks.iso_chunks(log)
        first = 401001 * 2048 // MIB
        last = ((401001 + 63) * 2048 - 1) // MIB
        self.assertEqual(chunks, sorted({1} | set(range(first, last + 1))))   # LSN 512 = byte 1 MiB; a failed read is left out

    def test_a_read_across_a_chunk_edge_takes_both(self):
        self.assertEqual(web_boot_chunks.iso_chunks(['disc: LSN 511, 2 sectors (ISO) in 1 ms']), [0, 1])

    def test_the_manifest_has_every_elf_chunk_and_the_iso_chunks(self):
        m = web_boot_chunks.manifest(['disc: LSN 0, 1 sectors (ISO) in 1 ms'], elf_bytes=2 * MIB + 5)
        self.assertEqual(m, {'SCUS_971.97': [0, 1, 2], 'game.iso': [0]})


if __name__ == '__main__':
    unittest.main()
