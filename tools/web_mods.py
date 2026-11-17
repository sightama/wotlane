#!/usr/bin/env python3
# ABOUTME: Copies the repo's enabled mods (mods/<mod>/, not starting with _ or .) into the browser dist with mods/manifest.json (the
# ABOUTME: file list and a hash), which web/builtin_mods.js mirrors into the browser's mods folder. Called by tools/build_web.sh.
"""Usage: python3 tools/web_mods.py <repo mods dir> <dist mods dir>"""
import hashlib
import json
import shutil
import sys
from pathlib import Path


def build(repo_mods, out):
    """Replaces `out` with the enabled mods of `repo_mods` and their manifest; returns the manifest."""
    repo_mods, out = Path(repo_mods), Path(out)
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    files = []
    if repo_mods.is_dir():
        for mod in sorted(p for p in repo_mods.iterdir() if p.is_dir() and not p.name.startswith(('_', '.'))):
            for f in sorted(p for p in mod.rglob('*') if p.is_file()):
                files.append(f.relative_to(repo_mods).as_posix())
    digest = hashlib.sha256()
    for rel in files:
        src = repo_mods / rel
        (out / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, out / rel)
        digest.update(rel.encode() + b'\0' + src.read_bytes() + b'\0')
    manifest = {'hash': digest.hexdigest()[:16], 'files': [{'path': rel} for rel in files]}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n')
    return manifest


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    m = build(sys.argv[1], sys.argv[2])
    print(f"web mods: {len(m['files'])} file(s), hash {m['hash']}")
