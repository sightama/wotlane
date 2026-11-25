#!/usr/bin/env python3
# ABOUTME: Tests for tools/build_id.py (the build identity the netcode compares): the same input in different folders gives the same id,
# ABOUTME: any change of code changes it, and the compiler doesn't. Run: python tools/test_build_id.py
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_id  # noqa: E402

CACHE = """# CMake cache
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_CXX_COMPILER:FILEPATH=C:/VS/VC/Tools/MSVC/{ver}/bin/Hostx64/x64/cl.exe
CMAKE_CXX_FLAGS_RELEASE:STRING=/O2 /Ob2 /DNDEBUG
WOTM_NATIVE_DIR:PATH={repo}/native
"""


def make_tree(root, ver='14.44.35207', native_text='int a;\n', crlf=False):
    """Minimal repo + runtime: native/, patches, recomp/retail, PS2Recomp (HEAD in a file) and the build_dev CMakeCache."""
    repo, runtime = root / 'repo', root / 'repo' / 'runtime'
    nl = '\r\n' if crlf else '\n'
    files = {
        repo / 'native' / 'a.hpp': native_text,
        repo / 'native' / 'runtime' / 'b.inc': 'void f();\n',
        repo / 'native' / 'net' / 'build' / 'junk.obj': 'ignored\n',   # build output: not part of the id
        repo / 'tools' / 'runtime_patch' / 'wotm-runtime.patch': '+x\n',
        repo / 'recomp' / 'retail' / 'f_100.cpp': 'void f_100() {}\n',
        runtime / 'build_dev' / 'CMakeCache.txt': CACHE.format(ver=ver, repo=str(repo).replace('\\', '/')),
    }
    for p, text in files.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(text.replace('\n', nl).encode())
    return repo, runtime


class BuildIdTest(unittest.TestCase):
    def compute(self, repo, runtime, commit='2c5fbb9'):
        return build_id.compute(repo, runtime, 'build_dev', ps2recomp_commit=commit)

    def test_same_sources_in_other_folders_give_the_same_id(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            ida = self.compute(*make_tree(Path(a)))
            idb = self.compute(*make_tree(Path(b) / 'other folder'))
            self.assertEqual(ida, idb)
            self.assertRegex(ida, r'^wl-[0-9a-f]{16}$')

    def test_line_endings_do_not_matter(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            self.assertEqual(self.compute(*make_tree(Path(a))), self.compute(*make_tree(Path(b), crlf=True)))

    def test_code_and_commit_changes_change_the_id(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            base = self.compute(*make_tree(Path(a)))
            self.assertNotEqual(base, self.compute(*make_tree(Path(b), native_text='int b;\n')))
            self.assertNotEqual(base, self.compute(*make_tree(Path(a)), commit='0000000'))

    def test_the_compiler_does_not_change_the_id(self):
        # game code builds with strict FP, so Windows (MSVC) and the Mac (AppleClang, arm64) simulate alike and must be able to
        # play together; if two compilers ever disagree, the server's per-frame state hash stops the match
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            msvc = self.compute(*make_tree(Path(a)))
            self.assertEqual(msvc, self.compute(*make_tree(Path(b), ver='14.44.35208')))
            self.assertEqual(msvc, self.compute(*make_clang_tree(Path(c), '17.0.0.17000013', 'arm64')))

    def test_the_browser_build_plays_with_the_native_dev_build(self):
        # build_web (Emscripten) is build_dev's code, VU0 microprograms included (every build rounds to nearest): the same states, so
        # browser and native players can share a lobby; the LTCG build stays apart
        with tempfile.TemporaryDirectory() as a:
            repo, runtime = make_tree(Path(a))
            dev = build_id.compute(repo, runtime, 'build_dev', ps2recomp_commit='2c5fbb9')
            self.assertEqual(dev, build_id.compute(repo, runtime, 'build_web', ps2recomp_commit='2c5fbb9'))
            self.assertNotEqual(dev, build_id.compute(repo, runtime, 'build', ps2recomp_commit='2c5fbb9'))

    def test_build_output_is_ignored(self):
        with tempfile.TemporaryDirectory() as a:
            repo, runtime = make_tree(Path(a))
            before = self.compute(repo, runtime)
            (repo / 'native' / 'net' / 'build' / 'junk.obj').write_text('changed')
            self.assertEqual(before, self.compute(repo, runtime))
            # any build folder inside native/ (tests, future tools) is build output
            other = repo / 'native' / 'tests' / 'build' / 'x.obj'
            other.parent.mkdir(parents=True)
            other.write_text('output')
            self.assertEqual(before, self.compute(repo, runtime))

    def test_file_order_is_the_same_on_every_os(self):
        # Path objects sort case-insensitively on Windows and by path parts on POSIX: the tree digest must order files by their
        # relative path string (case-sensitive), or the same files give a different id on Windows and the Mac
        with tempfile.TemporaryDirectory() as a:
            root = Path(a)
            for rel in ('README.md', 'ps2recomp.patch', 'a-c.txt', 'a/b.txt'):
                (root / rel).parent.mkdir(parents=True, exist_ok=True)
                (root / rel).write_text(rel)
            h = build_id.hashlib.sha256()
            for rel in sorted(('README.md', 'ps2recomp.patch', 'a-c.txt', 'a/b.txt')):
                h.update(rel.encode() + b'\0' + build_id.text_digest(root / rel).encode() + b'\n')
            self.assertEqual(build_id.tree_digest(root), h.hexdigest())


class SimulationIdTest(unittest.TestCase):
    """The simulation id leaves out the files that only draw or present (build_id.PRESENTATION): two builds that differ only there
    compute the same states and may share a lobby; the full id still tells them apart (it picks the build a page loads)."""

    def ids(self, repo, runtime):
        return (build_id.compute(repo, runtime, 'build_dev', ps2recomp_commit='2c5fbb9', simulation=True),
                build_id.compute(repo, runtime, 'build_dev', ps2recomp_commit='2c5fbb9'))

    def test_a_presentation_change_leaves_the_simulation_id(self):
        with tempfile.TemporaryDirectory() as a:
            repo, runtime = make_tree(Path(a))
            (repo / 'native' / 'wotm_native.hpp').write_text('// draw\n')
            (repo / 'native' / 'assets' / 'font.ttf').parent.mkdir(parents=True)
            (repo / 'native' / 'assets' / 'font.ttf').write_text('glyphs\n')
            sim, full = self.ids(repo, runtime)
            (repo / 'native' / 'wotm_native.hpp').write_text('// draw it another way\n')
            (repo / 'native' / 'assets' / 'font.ttf').write_text('other glyphs\n')
            sim2, full2 = self.ids(repo, runtime)
            self.assertEqual(sim, sim2)
            self.assertNotEqual(full, full2)
            self.assertRegex(sim, r'^wl-[0-9a-f]{16}$')

    def test_a_simulation_change_moves_the_simulation_id(self):
        with tempfile.TemporaryDirectory() as a:
            repo, runtime = make_tree(Path(a))
            sim, _ = self.ids(repo, runtime)
            (repo / 'native' / 'runtime' / 'b.inc').write_text('void g();\n')
            self.assertNotEqual(sim, self.ids(repo, runtime)[0])
            (repo / 'tools' / 'runtime_patch' / 'wotm-runtime.patch').write_text('+y\n')
            self.assertNotEqual(sim, self.ids(repo, runtime)[0])

    def test_the_simulation_id_differs_from_the_full_id(self):
        # a page compares one and loads by the other: they must never be mistaken for each other
        with tempfile.TemporaryDirectory() as a:
            sim, full = self.ids(*make_tree(Path(a)))
            self.assertNotEqual(sim, full)

    def test_write_puts_both_ids_next_to_the_exe(self):
        # wotm.buildid (sent and compared by lobbies) holds the simulation id, wotm.fullid the full one; the CLI prints either
        with tempfile.TemporaryDirectory() as a:
            repo, runtime = make_tree(Path(a))
            out = runtime / 'build_dev' / 'ps2xRuntime'
            out.mkdir(parents=True)
            sim, full = self.ids(repo, runtime)
            build_id.write_ids(runtime, 'build_dev', sim, full)
            self.assertEqual((out / 'wotm.buildid').read_text(), sim + '\n')
            self.assertEqual((out / 'wotm.fullid').read_text(), full + '\n')

    def test_the_core_simulation_files_are_never_presentation(self):
        core = ('native/runtime/wotm_sync.inc', 'native/runtime/wotm_overrides.cpp', 'native/runtime/wotm_live.inc', 'native/net',
                'native/runtime/wotm_snd.inc', 'native/runtime/wotm_rollback.inc', 'native/wotm_detmath.cpp', 'native/wotm_lockon.hpp',
                'native/runtime/wotm_creator.inc', 'native/wotm_model.hpp', 'native/wotm_tint.hpp', 'native/runtime/wotm_mapedit_view.inc',
                'native/runtime/wotm_trainview.inc')
        for rel in core:
            self.assertFalse(any(rel == p or rel.startswith(p + '/') or p.startswith(rel + '/') for p in build_id.PRESENTATION), rel)

    def test_every_presentation_entry_exists(self):
        for rel in build_id.PRESENTATION:
            self.assertTrue((build_id.REPO / rel).exists(), f'{rel}: listed as presentation but not in the repo')

    def test_presentation_files_do_not_write_game_state(self):
        # the ways code here changes what the game computes: the EE's memory or context, a hook it registers, a host global the
        # game's hooks read (g_wotm*); a pattern check, so a listed file that does any of them fails
        writes = re.compile(r'rdram\[|rdram \+|rdram\)|uint8_t \*rdram|const_cast<uint8_t|R5900Context|ctx->|runtime->|PS2Runtime &|'
                            r'registerFunction|hostLeaf|ps2_stubs::|setReturn|setReg|'
                            r'g_wotm\w*\s*(=[^=]|\|=|&=|\+=|-=)|\+\+g_wotm|g_wotm\w*\+\+|\bg_\w+\.(store|fetch_\w+|exchange)\(')
        for rel in build_id.PRESENTATION:
            root = build_id.REPO / rel
            files = [root] if root.is_file() else [p for p in root.rglob('*') if p.suffix in ('.hpp', '.h', '.cpp', '.inc')]
            if rel.startswith('native/tests'):
                continue   # tests are not in the game
            for p in files:
                found = writes.search(p.read_text(errors='replace'))
                self.assertIsNone(found, f'{p.relative_to(build_id.REPO)} looks like simulation code')


def make_clang_tree(root, ver, cpu):
    """make_tree with a clang build: CMakeCache with /usr/bin/c++ and the compiler/system files CMake writes under CMakeFiles/<ver>."""
    repo, runtime = make_tree(root)
    build = runtime / 'build_dev'
    (build / 'CMakeCache.txt').write_text('CMAKE_BUILD_TYPE:STRING=Release\nCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++\n'
                                          'CMAKE_CXX_FLAGS_RELEASE:STRING=-O3 -DNDEBUG\n')
    files = build / 'CMakeFiles' / '4.4.5'
    files.mkdir(parents=True)
    (files / 'CMakeCXXCompiler.cmake').write_text(f'set(CMAKE_CXX_COMPILER "/usr/bin/c++")\nset(CMAKE_CXX_COMPILER_ID "AppleClang")\n'
                                                  f'set(CMAKE_CXX_COMPILER_VERSION "{ver}")\n')
    (files / 'CMakeSystem.cmake').write_text(f'set(CMAKE_SYSTEM_NAME "Darwin")\nset(CMAKE_SYSTEM_PROCESSOR "{cpu}")\n')
    return repo, runtime


if __name__ == '__main__':
    unittest.main()
