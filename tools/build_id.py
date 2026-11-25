#!/usr/bin/env python3
# ABOUTME: Build identity for the netcode: the simulation id (what decides the game's states: native/ less its presentation files,
# ABOUTME: runtime patches, PS2Recomp commit, recompiled game, build folder) that lobbies compare, and the full id of the whole build.
"""Usage: python tools/build_id.py [--runtime <folder>] [--build build_dev] [--full] [--write]

Prints the simulation id (--full: the full id). --write writes both next to the exe: wotm.buildid (the simulation id, which wotm.exe
and the page send to the server and lobbies compare) and wotm.fullid (which picks the build a page loads).

Two machines only simulate identically with the same code. The simulation id leaves out PRESENTATION, the files that only draw or
present (a drawing-only change keeps it, so players on either build share a lobby); every other file counts, and a file that mixes
drawing with game hooks stays in. If a file is ever misfiled, the server's per-frame state hash still stops the match (desync).

The compiler is left out: game code builds with strict FP, so MSVC on Windows and clang on the Mac produce the same states, and
the server's per-frame state hash stops any match where they don't. Paths, dates and file line endings (CRLF or LF) are left out,
so two clones in different folders with the same code give the same id.
Called by tools/build_runtime.ps1 after every build; wotm.exe reads wotm.buildid and sends it to the server (wotm_sync.inc).
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Files and folders (relative to the repo) that only draw, present or measure, or are not part of the game: the simulation id leaves
# them out. tools/test_build_id.py checks each exists, that none matches the ways code here changes the game (the EE's memory or
# context, a hook it registers, a g_wotm* global it sets: a pattern check, not a proof) and that the core simulation files are not here.
PRESENTATION = (
    # drawing: Scene (reads guest RAM through the read-only Ram) and what only it includes
    'native/wotm_native.hpp', 'native/wotm_fx.hpp', 'native/wotm_gs.hpp', 'native/wotm_glsl.hpp', 'native/wotm_interp.hpp',
    'native/wotm_msaa.hpp', 'native/wotm_preview.hpp', 'native/wotm_flame.hpp', 'native/wotm_texmods.hpp', 'native/wotm_glad_web.cpp',
    'native/wotm_silhouette.hpp',
    # (not wotm_model.hpp or wotm_tint.hpp: the creator makes the skins' PNGs with them, and mods are in the config hash)
    # presenting and measuring: the present phase, the window, the pause view, the latency probes. The present phase slews when this
    # machine's vblanks fire (g_wotmVblankSlewUs): wall-clock pacing only, since lockstep keys every input by frame index
    'native/wotm_framephase.hpp', 'native/wotm_windowfit.hpp', 'native/wotm_pauseview.hpp', 'native/wotm_latency.hpp',
    # drawn screens: the menu font, the legend and prompts (not the map editor's view, which hands the pad to the free camera, nor the
    # training view, which resets the training state it shares with the hooks)
    'native/runtime/wotm_menufont.inc', 'native/runtime/wotm_legend.inc', 'native/runtime/wotm_prompts.inc',
    # not in the game: the standalone viewers and players, tests, art, the Windows resources and build script
    'native/fmvplay.cpp', 'native/monview.cpp', 'native/sndplay.cpp', 'native/viewer.cpp', 'native/tests', 'native/assets',
    'native/build.bat', 'native/wotm.ico', 'native/wotm.rc',
)


def text_digest(path):
    """sha256 of the contents with CRLF turned into LF (git may check out either)."""
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def tree_digest(root, skip_dirs=(), skip=()):
    """sha256 of the files in a folder: each one's relative path and contents, in order; folders named in skip_dirs (at any
    level) are left out, and so are the paths in skip (relative to root: a file, or a folder and everything in it). Files go in
    the order of their relative path strings: Path objects sort differently on Windows (case-insensitive) and POSIX (by parts),
    which would give the same files a different id on each OS."""
    h = hashlib.sha256()
    if root.exists():
        for rel, p in sorted((p.relative_to(root).as_posix(), p) for p in root.rglob('*')):
            if not p.is_file() or any(part in skip_dirs for part in rel.split('/')[:-1]):
                continue
            if any(rel == s or rel.startswith(s + '/') for s in skip):
                continue
            h.update(rel.encode() + b'\0' + text_digest(p).encode() + b'\n')
    return h.hexdigest()


def recomp_digest(retail, runtime):
    """The recompiled game (~90 MB): the hash is cached in <runtime>/.wotm_recomp_hash as long as the folder doesn't change."""
    files = [p for p in retail.rglob('*') if p.is_file()] if retail.exists() else []
    key = [len(files), sum(p.stat().st_size for p in files), max((p.stat().st_mtime_ns for p in files), default=0)]
    cache = runtime / '.wotm_recomp_hash'
    try:
        saved = json.loads(cache.read_text())
        if saved['key'] == key:
            return saved['hash']
    except (OSError, ValueError, KeyError):
        pass
    digest = tree_digest(retail)
    try:
        cache.write_text(json.dumps({'key': key, 'hash': digest}))
    except OSError:
        pass
    return digest


def ps2recomp_head(runtime):
    try:
        return subprocess.run(['git', '-C', str(runtime / 'PS2Recomp'), 'rev-parse', 'HEAD'], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return 'unknown'


def compute(repo, runtime, build, ps2recomp_commit=None, simulation=False):
    """The full id, or with simulation=True the simulation id (native/ without PRESENTATION)."""
    skip = tuple(p[len('native/'):] for p in PRESENTATION) if simulation else ()
    parts = {
        'id': 'simulation' if simulation else 'full',   # never the same value for the two kinds
        'native': tree_digest(repo / 'native', skip_dirs=('build', '__pycache__'), skip=skip),   # build outputs (net, tests)
        'patches': tree_digest(repo / 'tools' / 'runtime_patch'),
        'ps2recomp': ps2recomp_commit or ps2recomp_head(runtime),
        'recomp': recomp_digest(repo / 'recomp' / 'retail', runtime),
        # build_dev (no LTCG) and build (LTCG) generate different code; build_web is build_dev's code for the browser (VU0 rounds to
        # nearest in both) so the states are the same, and its players share lobbies with build_dev's
        'build': 'build_dev' if build == 'build_web' else build,
    }
    return 'wl-' + hashlib.sha256(json.dumps(parts, sort_keys=True).encode()).hexdigest()[:16]


def write_ids(runtime, build, sim, full):
    """wotm.buildid (the simulation id, which the exe sends and lobbies compare) and wotm.fullid next to the build's exe."""
    out = runtime / build / 'ps2xRuntime'
    (out / 'wotm.buildid').write_text(sim + '\n')
    (out / 'wotm.fullid').write_text(full + '\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--runtime', default=None)
    ap.add_argument('--build', default='build_dev')
    ap.add_argument('--full', action='store_true', help='print the full id instead of the simulation id')
    ap.add_argument('--write', action='store_true', help='write <runtime>/<build>/ps2xRuntime/wotm.buildid and wotm.fullid')
    a = ap.parse_args()
    saved = REPO / '.wotm_recomp_dir'
    runtime = Path(a.runtime) if a.runtime else (Path(saved.read_text().splitlines()[0].strip()) if saved.exists() else REPO / 'runtime')
    sim, full = compute(REPO, runtime, a.build, simulation=True), compute(REPO, runtime, a.build)
    if a.write:
        write_ids(runtime, a.build, sim, full)
    print(full if a.full else sim)
    return 0


if __name__ == '__main__':
    sys.exit(main())
