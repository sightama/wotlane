#!/usr/bin/env python3
# ABOUTME: Re-encodes the game's two boot movies (INTRO_M and INT_MAIN, .PSS from the disc) to VP9/Opus WebM files the browser page plays,
# ABOUTME: and writes movies.json beside them. Needs ffmpeg. The output is game data: it goes next to the disc in data/, never into the repo.
"""Boot movies for the browser version.

    python3 tools/web_movies.py --iso <game.iso> --out <data folder>/movies
    python3 tools/web_movies.py --disc <extracted disc folder> --out <data folder>/movies

The page (web/movies.js) fetches <data>/movies/movies.json and plays the files it lists, in order, while the game loads. FFmpeg reads the
picture of a .PSS (MPEG-2) but not its audio (SCE's private stream, docs/FORMATOS.md), so the audio is taken apart here: 16-bit PCM in
512-byte blocks that alternate left and right (native/wotm_fmv.hpp reads it the same way).
"""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SECTOR = 2048
BOOT_MOVIES = ['INTRO_M', 'INT_MAIN']   # the two movies the game plays at boot, in order
MAX_TOTAL = 15 << 20                    # all the files together


def iso_find(iso, path):
    """(first sector, size) of `path` (e.g. 'MOV/INTRO_M.PSS') in an ISO 9660 image at 2048 bytes per sector, or None."""
    with open(iso, 'rb') as f:
        f.seek(16 * SECTOR)
        pvd = f.read(SECTOR)
        if pvd[1:6] != b'CD001':
            raise ValueError(f'{iso}: not an ISO 9660 image')
        lba, size = struct.unpack_from('<I', pvd, 156 + 2)[0], struct.unpack_from('<I', pvd, 156 + 10)[0]
        for part in path.upper().split('/'):
            found = None
            f.seek(lba * SECTOR)
            data = f.read(size)
            i = 0
            while i < len(data):
                n = data[i]
                if n == 0:   # records do not cross a sector: the rest of it is padding
                    i = (i // SECTOR + 1) * SECTOR
                    continue
                name = data[i + 33:i + 33 + data[i + 32]].decode('latin-1').split(';')[0].upper()
                if name == part:
                    found = (struct.unpack_from('<I', data, i + 2)[0], struct.unpack_from('<I', data, i + 10)[0])
                    break
                i += n
            if not found:
                return None
            lba, size = found
        return lba, size


def read_movie(name, iso=None, disc=None):
    """The bytes of MOV/<name>.PSS from the image or the extracted folder."""
    if iso:
        hit = iso_find(iso, f'MOV/{name}.PSS')
        if not hit:
            raise FileNotFoundError(f'MOV/{name}.PSS is not in {iso}')
        with open(iso, 'rb') as f:
            f.seek(hit[0] * SECTOR)
            return f.read(hit[1])
    for p in Path(disc, 'MOV').glob('*'):
        if p.name.upper() == f'{name}.PSS':
            return p.read_bytes()
    raise FileNotFoundError(f'MOV/{name}.PSS is not in {disc}')


def pss_audio(data):
    """(rate, left, right) of a .PSS's audio: the sample rate and the two channels as raw 16-bit little-endian PCM; None without audio."""
    stream = bytearray()
    i = 0
    n = len(data)
    while True:
        i = data.find(b'\x00\x00\x01', i)
        if i < 0 or i + 4 > n:
            break
        sid = data[i + 3]
        if sid == 0xBA:   # pack header: 14 bytes and the stuffing for MPEG-2, 12 for MPEG-1
            i += 14 + (data[i + 13] & 7) if (data[i + 4] >> 6) == 1 else 12
        elif sid == 0xB9:
            break
        elif sid < 0xBB:
            i += 4
        else:
            ln = struct.unpack_from('>H', data, i + 4)[0]
            pes = data[i + 6:i + 6 + ln]
            if sid == 0xBD and len(pes) >= 3:
                payload = pes[3 + pes[2]:]
                stream += payload[4:]   # the first 4 bytes name the sub-stream
            i += 6 + ln
    if stream[:4] != b'SShd' or len(stream) < 24:
        return None
    header = struct.unpack_from('<I', stream, 4)[0]
    kind, rate, channels, interleave = struct.unpack_from('<4I', stream, 8)
    body = 8 + header
    if kind != 1 or channels not in (1, 2) or interleave == 0 or stream[body:body + 4] != b'SSbd':
        raise ValueError(f'unsupported movie audio (type {kind}, {channels} channels, interleave {interleave})')
    pcm = bytes(stream[body + 8:])
    left, right = bytearray(), bytearray()
    for at in range(0, len(pcm) - interleave * channels + 1, interleave * channels):
        left += pcm[at:at + interleave]
        right += pcm[at + interleave:at + 2 * interleave] if channels == 2 else pcm[at:at + interleave]
    return rate, bytes(left), bytes(right)


def encode(name, pss, out, crf=32, kbps=500, ffmpeg='ffmpeg'):
    """Writes the WebM of one movie: VP9 picture (656x368, the shape the PS2 showed) and Opus audio. Constrained quality: `crf` sets the
    quality and `kbps` caps the picture's average rate, which is what keeps a busy movie small."""
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp, f'{name}.pss')
        src.write_bytes(pss)
        cmd = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', str(src)]
        audio = pss_audio(pss)
        if audio:
            rate, left, right = audio
            Path(tmp, 'l.raw').write_bytes(left)
            Path(tmp, 'r.raw').write_bytes(right)
            for side in ('l', 'r'):
                cmd += ['-f', 's16le', '-ar', str(rate), '-ac', '1', '-i', str(Path(tmp, side + '.raw'))]
            cmd += ['-filter_complex', '[1:a][2:a]amerge=inputs=2[a]', '-map', '0:v:0', '-map', '[a]', '-c:a', 'libopus', '-b:a', '64k']
        else:
            cmd += ['-map', '0:v:0', '-an']
        cmd += ['-vf', 'scale=656:368,setsar=1', '-pix_fmt', 'yuv420p', '-c:v', 'libvpx-vp9', '-crf', str(crf), '-b:v', f'{kbps}k',
                '-row-mt', '1', '-deadline', 'good', '-cpu-used', '2', '-shortest', str(out)]
        subprocess.run(cmd, check=True)


def manifest(files):
    """movies.json's content for the encoded files (paths in play order)."""
    movies = []
    for p in files:
        data = Path(p).read_bytes()
        movies.append({'name': Path(p).stem, 'file': Path(p).name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    return {'version': 1, 'codec': 'vp9+opus', 'movies': movies}


def main(argv=None):
    ap = argparse.ArgumentParser(description='Re-encodes the boot movies for the browser page.')
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--iso', help="the game's disc image")
    src.add_argument('--disc', help='the extracted disc folder (it has MOV/)')
    ap.add_argument('--out', required=True, help='folder for the WebM files and movies.json (next to the disc, in data/movies)')
    ap.add_argument('--crf', type=int, default=32, help='VP9 quality, lower is better and larger (default 32)')
    ap.add_argument('--kbps', type=int, default=500, help='cap on the picture\'s average rate (default 500)')
    ap.add_argument('--ffmpeg', default=shutil.which('ffmpeg') or 'ffmpeg')
    a = ap.parse_args(argv)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    files = []
    for name in BOOT_MOVIES:
        target = out / f'{name.lower()}.webm'
        print(f'{name}: encoding', flush=True)
        encode(name.lower(), read_movie(name, a.iso, a.disc), target, a.crf, a.kbps, a.ffmpeg)
        files.append(target)
    m = manifest(files)
    total = sum(x['bytes'] for x in m['movies'])
    (out / 'movies.json').write_text(json.dumps(m, indent=2) + '\n')
    print(f'{len(files)} movies, {total / (1 << 20):.1f} MB in {out}')
    if total > MAX_TOTAL:
        sys.exit(f'the movies come to {total >> 20} MB, over the {MAX_TOTAL >> 20} MB budget: lower --kbps or raise --crf')


if __name__ == '__main__':
    main()
