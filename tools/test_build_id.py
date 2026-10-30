#!/usr/bin/env python3
# ABOUTME: Testes do tools/build_id.py (identidade do build que o netcode compara): mesma entrada em pastas diferentes da o mesmo id,
# ABOUTME: e qualquer mudanca de codigo, compilador ou flags muda. Rodar: python tools/test_build_id.py
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
    """Repo + runtime minimos: native/, patches, recomp/retail, PS2Recomp (HEAD em arquivo) e o CMakeCache do build_dev."""
    repo, runtime = root / 'repo', root / 'repo' / 'runtime'
    nl = '\r\n' if crlf else '\n'
    files = {
        repo / 'native' / 'a.hpp': native_text,
        repo / 'native' / 'runtime' / 'b.inc': 'void f();\n',
        repo / 'native' / 'net' / 'build' / 'junk.obj': 'ignorado\n',   # saida de build: fora do id
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
            idb = self.compute(*make_tree(Path(b) / 'outra pasta'))
            self.assertEqual(ida, idb)
            self.assertRegex(ida, r'^wl-[0-9a-f]{16}$')

    def test_line_endings_do_not_matter(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            self.assertEqual(self.compute(*make_tree(Path(a))), self.compute(*make_tree(Path(b), crlf=True)))

    def test_code_compiler_and_commit_changes_change_the_id(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            base = self.compute(*make_tree(Path(a)))
            self.assertNotEqual(base, self.compute(*make_tree(Path(b), native_text='int b;\n')))
            self.assertNotEqual(base, self.compute(*make_tree(Path(c), ver='14.44.35208')))
            self.assertNotEqual(base, self.compute(*make_tree(Path(a)), commit='0000000'))

    def test_build_output_is_ignored(self):
        with tempfile.TemporaryDirectory() as a:
            repo, runtime = make_tree(Path(a))
            before = self.compute(repo, runtime)
            (repo / 'native' / 'net' / 'build' / 'junk.obj').write_text('mudou')
            self.assertEqual(before, self.compute(repo, runtime))
            # qualquer pasta build dentro de native/ (testes, ferramentas futuras) e saida de compilacao
            other = repo / 'native' / 'tests' / 'build' / 'x.obj'
            other.parent.mkdir(parents=True)
            other.write_text('saida')
            self.assertEqual(before, self.compute(repo, runtime))

    def test_compiler_version_comes_from_the_cmake_cache(self):
        with tempfile.TemporaryDirectory() as a:
            _, runtime = make_tree(Path(a))
            self.assertEqual(build_id.compiler(runtime / 'build_dev')['version'], '14.44.35207')

    def test_clang_version_and_cpu_come_from_cmake_files(self):
        # gcc/clang: the cache only has the compiler's path (the same for every version); CMake's compiler and system files say
        # which compiler and which CPU, and both decide the generated code
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            ids = []
            for root, ver, cpu in ((a, '17.0.0.17000013', 'arm64'), (b, '17.0.0.17000014', 'arm64'), (c, '17.0.0.17000013', 'x86_64')):
                repo, runtime = make_clang_tree(Path(root), ver, cpu)
                ids.append(self.compute(repo, runtime))
                if root == a:
                    self.assertEqual(build_id.compiler(runtime / 'build_dev')['version'], 'AppleClang 17.0.0.17000013 arm64')
            self.assertEqual(len(set(ids)), 3)


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
