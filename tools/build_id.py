#!/usr/bin/env python3
# ABOUTME: Identidade do build do wotm.exe para o netcode: hash do que decide o codigo gerado (native/, patches do runtime, commit do
# ABOUTME: PS2Recomp, jogo traduzido, versao exata do compilador e flags). Grava wotm.buildid ao lado do exe; o servidor exige igual.
"""Uso: python tools/build_id.py [--runtime <pasta>] [--build build_dev] [--write]

O jogo e compilado com /fp:fast /arch:AVX2: dois PCs so simulam igual com o mesmo codigo e o mesmo compilador. Caminhos, datas e o
fim de linha dos arquivos (CRLF ou LF) nao entram, entao dois clones em pastas diferentes com o mesmo codigo dao o mesmo id.
Chamado pelo tools/build_runtime.ps1 depois de cada build; o wotm.exe le o wotm.buildid e o manda ao servidor (wotm_sync.inc).
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def text_digest(path):
    """sha256 do conteudo com CRLF virando LF (o git pode fazer checkout com qualquer um)."""
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def tree_digest(root, skip_dirs=()):
    """sha256 dos arquivos de uma pasta: caminho relativo e conteudo de cada um, em ordem; pastas com nome em skip_dirs (em qualquer
    nivel) ficam de fora."""
    h = hashlib.sha256()
    if root.exists():
        for p in sorted(root.rglob('*')):
            rel = p.relative_to(root).as_posix()
            if not p.is_file() or any(part in skip_dirs for part in rel.split('/')[:-1]):
                continue
            h.update(rel.encode() + b'\0' + text_digest(p).encode() + b'\n')
    return h.hexdigest()


def recomp_digest(retail, runtime):
    """O jogo traduzido (~90 MB): o hash fica guardado em <runtime>/.wotm_recomp_hash enquanto a pasta nao mudar."""
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


def compiler(build_dir):
    """Versao do MSVC (pasta do cl.exe) e as flags do CMakeCache.txt do build."""
    out = {'version': 'desconhecido', 'type': '', 'flags': ''}
    cache = build_dir / 'CMakeCache.txt'
    if cache.exists():
        for line in cache.read_text(errors='replace').splitlines():
            if line.startswith('CMAKE_CXX_COMPILER:'):
                m = re.search(r'/MSVC/([0-9.]+)/', line.replace('\\', '/'))
                out['version'] = m.group(1) if m else line.split('=', 1)[1]
            elif line.startswith('CMAKE_BUILD_TYPE:'):
                out['type'] = line.split('=', 1)[1]
            elif line.startswith('CMAKE_CXX_FLAGS_RELEASE:'):
                out['flags'] = line.split('=', 1)[1]
    return out


def ps2recomp_head(runtime):
    try:
        return subprocess.run(['git', '-C', str(runtime / 'PS2Recomp'), 'rev-parse', 'HEAD'], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return 'desconhecido'


def compute(repo, runtime, build, ps2recomp_commit=None):
    parts = {
        'native': tree_digest(repo / 'native', skip_dirs=('build', '__pycache__')),   # saidas de compilacao (net, tests)
        'patches': tree_digest(repo / 'tools' / 'runtime_patch'),
        'ps2recomp': ps2recomp_commit or ps2recomp_head(runtime),
        'recomp': recomp_digest(repo / 'recomp' / 'retail', runtime),
        'compiler': compiler(runtime / build),
        'build': build,   # build_dev (sem LTCG) e build (LTCG) geram codigo diferente
    }
    return 'wl-' + hashlib.sha256(json.dumps(parts, sort_keys=True).encode()).hexdigest()[:16]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--runtime', default=None)
    ap.add_argument('--build', default='build_dev')
    ap.add_argument('--write', action='store_true', help='grava <runtime>/<build>/ps2xRuntime/wotm.buildid')
    a = ap.parse_args()
    saved = REPO / '.wotm_recomp_dir'
    runtime = Path(a.runtime) if a.runtime else (Path(saved.read_text().splitlines()[0].strip()) if saved.exists() else REPO / 'runtime')
    bid = compute(REPO, runtime, a.build)
    if a.write:
        out = runtime / a.build / 'ps2xRuntime' / 'wotm.buildid'
        out.write_text(bid + '\n')
    print(bid)
    return 0


if __name__ == '__main__':
    sys.exit(main())
