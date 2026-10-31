#!/usr/bin/env python3
# ABOUTME: Tests for tools/translation_check.py: comment/literal-only edits pass in every language it reads, any code edit fails,
# ABOUTME: changed literals pair up in order, and the matcher check finds -Expect/grep text broken by a translation. Run: python tools/test_translation_check.py
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import translation_check as tc  # noqa: E402


def same(old, new, lang):
    diff, changed = tc.compare(old, new, lang)
    return diff is None, [(o, n) for _, o, n in changed]


class CodeTest(unittest.TestCase):
    def test_c_comments_and_strings_only(self):
        old = '// quadro\nint a = f("quadro %d\\n", b); /* bloco\n de dois */ char c = \'"\';\n'
        new = '// frame\nint a = f("frame %d\\n", b); /* a two-line\n block */ char c = \'"\';\n'
        ok, changed = same(old, new, 'c')
        self.assertTrue(ok)
        self.assertEqual(changed, [('"quadro %d\\n"', '"frame %d\\n"')])

    def test_c_code_change_fails(self):
        self.assertFalse(same('int a = f("x", b);\n', 'int a = f("x", c);\n', 'c')[0])
        self.assertFalse(same('a(); // x\n', 'a(); b(); // x\n', 'c')[0])

    def test_c_whitespace_and_comment_line_count_ignored(self):
        self.assertTrue(same('// um\n// dois\nint a;\n', '// one\nint  a;\n', 'c')[0])

    def test_c_adjacent_literals_merge(self):
        ok, changed = same('f("a" "b");\n', 'f("ab");\n', 'c')
        self.assertTrue(ok)
        self.assertEqual(len(changed), 1)

    def test_c_string_with_comment_marker(self):
        self.assertTrue(same('f("http://x"); // a\n', 'f("http://y"); // b\n', 'c')[0])

    def test_c_raw_string(self):
        self.assertTrue(same('auto s = R"(a "quoted" )";\n', 'auto s = R"(b "quoted" )";\n', 'c')[0])

    def test_csharp_interpolation_hole_is_code(self):
        self.assertTrue(same('var s = $"jogador {n} de {m}";\n', 'var s = $"player {n} of {m}";\n', 'c')[0])
        self.assertFalse(same('var s = $"jogador {n}";\n', 'var s = $"player {m}";\n', 'c')[0])
        self.assertTrue(same('var s = @"a ""b"" c";\n', 'var s = @"x ""y"" z";\n', 'c')[0])

    def test_python(self):
        old = '# comentario\ndef f(x):\n    """Doc em portugues."""\n    return g("erro: %s" % x)  # fim\n'
        new = '# comment\ndef f(x):\n    """Docstring in English."""\n    return g("error: %s" % x)  # end\n'
        ok, changed = same(old, new, 'py')
        self.assertTrue(ok)
        self.assertEqual(len(changed), 2)
        self.assertFalse(same('def f(x):\n    return x\n', 'def f(x):\n    return -x\n', 'py')[0])

    def test_python_fstring_expression_is_code(self):
        self.assertTrue(same('f"quadro {n}"\n', 'f"frame {n}"\n', 'py')[0])
        if sys.version_info >= (3, 12):
            self.assertFalse(same('f"quadro {n}"\n', 'f"frame {m}"\n', 'py')[0])

    def test_powershell(self):
        old = "# nota\n<# bloco #>\nif ($log -notmatch '\\[sync\\] semente') { Write-Host \"FALHOU: $($Matches[0]) nao\" }\n"
        new = "# note\n<# block #>\nif ($log -notmatch '\\[sync\\] seed') { Write-Host \"FAILED: $($Matches[0]) not\" }\n"
        ok, changed = same(old, new, 'ps1')
        self.assertTrue(ok)
        self.assertEqual(len(changed), 3)   # the $(...) splits the second literal in two
        self.assertFalse(same('Write-Host "a $($x[0])"\n', 'Write-Host "a $($x[1])"\n', 'ps1')[0])
        self.assertTrue(same("$s = @\"\nlinha\n\"@\n$t = 'it''s'\n", "$s = @\"\nline\n\"@\n$t = 'é'\n", 'ps1')[0])

    def test_sh(self):
        old = '#!/bin/sh\n# comentario\nx=$#  # resto\necho "erro: $(basename "$0") falhou" >&2\ncat <<EOF\nuso: x\nEOF\n'
        new = '#!/bin/sh\n# comment\nx=$#  # rest\necho "error: $(basename "$0") failed" >&2\ncat <<EOF\nusage: x\nEOF\n'
        ok, changed = same(old, new, 'sh')
        self.assertTrue(ok)
        self.assertEqual(len(changed), 3)   # the $(...) splits the echo literal in two
        self.assertFalse(same('echo "a $(f 1)"\n', 'echo "a $(f 2)"\n', 'sh')[0])
        self.assertFalse(same('x=$#\n', 'x=$1\n', 'sh')[0])

    def test_bat(self):
        old = '@echo off\nrem comentario\n:: outro\nif errorlevel 1 (echo Erro: falhou & exit /b 1)\nset "A=%~dp0x"\n'
        new = '@echo off\nrem comment\n:: another\nif errorlevel 1 (echo Error: failed & exit /b 1)\nset "A=%~dp0x"\n'
        ok, changed = same(old, new, 'bat')
        self.assertTrue(ok)
        self.assertEqual(changed, [(' Erro: falhou ', ' Error: failed ')])
        self.assertFalse(same('@echo off\n', '@echo on\n', 'bat')[0])
        self.assertFalse(same('echo a & exit /b 1\n', 'echo a & exit /b 2\n', 'bat')[0])

    def test_cmake(self):
        self.assertTrue(same('# nota\nmessage(STATUS "ola")\n', '# note\nmessage(STATUS "hello")\n', 'cmake')[0])


class MatcherTest(unittest.TestCase):
    def test_fixed_pieces(self):
        self.assertEqual(tc.fixed_pieces(r'\[sync\] primeira entrada nao neutra aplicada no quadro (\d+)'),
                         ['[sync] primeira entrada nao neutra aplicada no quadro'])
        self.assertEqual(tc.fixed_pieces(r'rodada \d+ terminou'), ['rodada', 'terminou'])
        self.assertEqual(tc.fixed_pieces(r'\[vigia\] CONGELOU'), ['[vigia] congelou'])
        self.assertEqual(tc.fixed_pieces(r'[0-9a-f]{8}|ab'), [])

    def test_flags_log_line_a_script_still_expects(self):
        corpus = [tc.corpus_entry(Path('/x/tools/lote.ps1'), 'ps1', r'rodada \d+ terminou')]
        flags = tc.matcher_flags(Path('/x/native/a.inc'), 'c', '"[partida] rodada %d terminou\\n"', '"[match] round %d over\\n"', corpus)
        self.assertEqual(len(flags), 2)   # "rodada" and "terminou"
        self.assertIn('rodada', flags[0])
        ok = tc.matcher_flags(Path('/x/native/a.inc'), 'c', '"[partida] rodada %d terminou"', '"[match] rodada %d terminou"', corpus)
        self.assertEqual(ok, [])

    def test_flags_pattern_whose_old_text_still_exists(self):
        corpus = [tc.corpus_entry(Path('/x/native/a.inc'), 'c', '[sync] semente %lld\\n')]
        flags = tc.matcher_flags(Path('/x/tools/d.ps1'), 'ps1', r"'\[sync\] semente'", r"'\[sync\] seed'", corpus)
        self.assertEqual(len(flags), 1)

    def test_powershell_match_is_case_insensitive(self):
        corpus = [tc.corpus_entry(Path('/x/tools/net_check.ps1'), 'ps1', 'DESSINCRONIA[^\\r\\n]*')]
        flags = tc.matcher_flags(Path('/x/native/s.cpp'), 'c', '"dessincronia no quadro %u"', '"desync at frame %u"', corpus)
        self.assertEqual(len(flags), 1)


if __name__ == '__main__':
    unittest.main()
