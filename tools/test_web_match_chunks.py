#!/usr/bin/env python3
# ABOUTME: Tests for tools/web_match_chunks.py: which disc files a fight's log read, split into the stage's and each monster's
# ABOUTME: (with every costume), as the 1 MiB chunk lists web/match_chunks.json holds. Run: python3 tools/test_web_match_chunks.py
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import web_match_chunks as wmc  # noqa: E402

MIB = 1 << 20
SECTORS_PER_MIB = MIB // 2048

# one file per MiB from chunk 10 on: (lsn, size, name)
NAMES = ['LVL/CENTRAL.NGP', 'LVL/METRO.NGP', 'SND/GENAM_LS.BNK', 'MON/MONKEY0.NGP', 'MON/MONKEY1.NGP', 'RST/MONKEY.CHR', 'SND/APE_LS.BNK',
         'MON/LIZARD0.NGP', 'MON/LIZARD1.NGP', 'MON/LIZARD2.NGP', 'RST/LIZARD.CHR', 'SND/LIZRD_LS.BNK', 'VAG/GEN1AMB.VPK']
FILES = [((10 + i) * SECTORS_PER_MIB, MIB, name) for i, name in enumerate(NAMES)]
CHUNK = {name: 10 + i for i, name in enumerate(NAMES)}


def read(name):
    lsn = next(f[0] for f in FILES if f[2] == name)
    return f'5.00s [boot]   2.00 s  disc: LSN {lsn}, 3 sectors (ISO) in 1.0 ms'


def fight(stage, monsters, names, after=()):
    members = ' and '.join(f'{m}:0 (player {i + 1})' for i, m in enumerate(monsters))
    return ([read('LVL/METRO.NGP'), f'3.88s [match] 1: monster {members} no AI, stage {stage}'] + [read(n) for n in names]
            + ['7.11s [boot]   4.59 s  ready: stage on screen'] + [read(n) for n in after])


class MatchChunksTest(unittest.TestCase):
    def test_the_fight_line_names_the_stage_and_the_monsters(self):
        self.assertEqual(wmc.fight_of(fight(4, [3, 1, 2], [])), (4, [3, 1, 2]))

    def test_only_reads_between_the_match_and_the_stage_on_screen_count(self):
        log = fight(1, [1, 1], ['LVL/CENTRAL.NGP'], after=['VAG/GEN1AMB.VPK'])
        self.assertEqual(wmc.fight_files(log, FILES), {'LVL/CENTRAL.NGP'})   # METRO before the match line, the music after the load

    def test_a_failed_read_is_left_out(self):
        log = fight(1, [1, 1], [])
        log.insert(2, read('LVL/CENTRAL.NGP') + ' FAILED')
        self.assertEqual(wmc.fight_files(log, FILES), set())

    def test_stages_and_monsters_split_by_what_differs_between_runs(self):
        monkey = fight(1, [1, 1], ['LVL/CENTRAL.NGP', 'SND/GENAM_LS.BNK', 'MON/MONKEY0.NGP', 'RST/MONKEY.CHR', 'SND/APE_LS.BNK'])
        lizard = fight(1, [3, 3], ['LVL/CENTRAL.NGP', 'SND/GENAM_LS.BNK', 'MON/LIZARD0.NGP', 'RST/LIZARD.CHR', 'SND/LIZRD_LS.BNK'])
        metro = fight(4, [1, 1], ['LVL/METRO.NGP', 'SND/GENAM_LS.BNK', 'MON/MONKEY0.NGP', 'RST/MONKEY.CHR', 'SND/APE_LS.BNK'])
        t = wmc.table([monkey, lizard], [monkey, metro], FILES)
        self.assertEqual(t['stages'], {'1': [CHUNK['LVL/CENTRAL.NGP'], CHUNK['SND/GENAM_LS.BNK']],
                                       '4': [CHUNK['LVL/METRO.NGP'], CHUNK['SND/GENAM_LS.BNK']]})
        # each monster: what every costume needs, then each costume on the disc (MONKEY0..1, LIZARD0..2)
        self.assertEqual(t['monsters']['1'], {'all': [CHUNK['RST/MONKEY.CHR'], CHUNK['SND/APE_LS.BNK']],
                                              'costumes': [[CHUNK['MON/MONKEY0.NGP']], [CHUNK['MON/MONKEY1.NGP']]]})
        self.assertEqual(t['monsters']['3']['costumes'], [[CHUNK[f'MON/LIZARD{c}.NGP']] for c in range(3)])

    def test_a_monster_run_needs_one_monster(self):
        with self.assertRaises(ValueError):
            wmc.table([fight(1, [1, 3], ['LVL/CENTRAL.NGP'])], [], FILES)

    def test_a_file_across_a_chunk_edge_takes_both_chunks(self):
        self.assertEqual(wmc.chunks_of(['A'], [(SECTORS_PER_MIB - 1, 4096, 'A')]), [0, 1])


if __name__ == '__main__':
    unittest.main()
