#!/usr/bin/env python3
# ABOUTME: Tests for tools/gamecheck.py, the shared part of the cross-platform game checks (live_check.py, net_check.py,
# ABOUTME: determinism_check.py): variable lists, per-player variables, log patterns and verdicts. Run: python3 tools/test_gamecheck.py
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gamecheck  # noqa: E402


class EnvPairsTest(unittest.TestCase):
    def test_pairs_split_on_semicolons_and_keep_commas(self):
        self.assertEqual(gamecheck.env_pairs('WOTM_MATCH=3,1,1; WOTM_AIVSAI=1'), [('WOTM_MATCH', '3,1,1'), ('WOTM_AIVSAI', '1')])

    def test_empty_value_clears(self):
        self.assertEqual(gamecheck.env_pairs('WOTM_RES=;A=1'), [('WOTM_RES', ''), ('A', '1')])

    def test_piece_without_equals_continues_the_previous_value(self):
        # a PS2X_PAD script uses ';' between its steps
        self.assertEqual(gamecheck.env_pairs('PS2X_PAD=24:0.3:start;26:0.2:down;WOTM_CAMERA=1'),
                         [('PS2X_PAD', '24:0.3:start;26:0.2:down'), ('WOTM_CAMERA', '1')])

    def test_a_first_piece_without_equals_is_an_error(self):
        with self.assertRaises(ValueError):
            gamecheck.env_pairs('nonsense;A=1')


class PlayerEnvTest(unittest.TestCase):
    spec = '1:WOTM_CAMERA=1|||2:WOTM_CAMERA=1;WOTM_CAMSENS=8;PS2X_PAD=24:0.3:start;26:1:down|||3:A=1'

    def test_only_that_players_variables(self):
        self.assertEqual(gamecheck.player_env(self.spec, 2),
                         [('WOTM_CAMERA', '1'), ('WOTM_CAMSENS', '8'), ('PS2X_PAD', '24:0.3:start;26:1:down')])
        self.assertEqual(gamecheck.player_env(self.spec, 3), [('A', '1')])
        self.assertEqual(gamecheck.player_env(self.spec, 4), [])

    def test_player_10_is_not_player_1(self):
        self.assertEqual(gamecheck.player_env('10:A=1', 1), [])


class PatternsTest(unittest.TestCase):
    def test_split_drops_empty_patterns(self):
        self.assertEqual(gamecheck.patterns('a|||b||||||'), ['a', 'b'])
        self.assertEqual(gamecheck.patterns(''), [])

    def test_scoped_patterns_apply_to_their_player_only(self):
        self.assertEqual(gamecheck.patterns_for('1:\\[training\\] ignored|||\\[dialog\\] 0 opened', 1),
                         ['\\[training\\] ignored', '\\[dialog\\] 0 opened'])
        self.assertEqual(gamecheck.patterns_for('1:\\[training\\] ignored|||\\[dialog\\] 0 opened', 2), ['\\[dialog\\] 0 opened'])


class LogVerdictTest(unittest.TestCase):
    log = '[net] start: player 1 of 2, delay 4\n[dialog] 0 opened\n[sync] first non-neutral input applied at frame 963\n'

    def test_all_expected_and_none_rejected_passes(self):
        self.assertEqual(gamecheck.log_problems(self.log, ['\\[dialog\\] 0 opened'], gamecheck.DEFAULT_REJECT), [])

    def test_missing_and_rejected_are_reported(self):
        problems = gamecheck.log_problems(self.log + '[CRASH] at 0x1234\n', ['\\[photo\\] on'], gamecheck.DEFAULT_REJECT)
        self.assertEqual(problems, ['missing \\[photo\\] on', 'found [CRASH]'])

    def test_patterns_match_across_lines_like_the_powershell_ones(self):
        # .NET -match: '.' stops at newlines, but [\s\S] and \n cross them (live_checks.ps1 'volta_4')
        two = 'win the fight: 3 a\nmore\nwin the fight: 3 b\n'
        self.assertEqual(gamecheck.log_problems(two, ['win the fight: 3 [^\\n]*\\n(.|\\n)*win the fight: 3 '], []), [])


if __name__ == '__main__':
    unittest.main()
