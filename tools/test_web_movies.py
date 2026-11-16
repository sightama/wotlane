#!/usr/bin/env python3
# ABOUTME: Tests for tools/web_movies.py without ffmpeg: file lookup in a small ISO 9660 image and in a folder, taking the audio of a .PSS
# ABOUTME: apart (left and right blocks), and the manifest. Run: python3 tools/test_web_movies.py
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import web_movies  # noqa: E402

S = web_movies.SECTOR


def record(name, lba, size, directory=False):
    """One ISO 9660 directory record."""
    nm = name.encode()
    body = struct.pack('<I', lba) + struct.pack('>I', lba) + struct.pack('<I', size) + struct.pack('>I', size)
    body += bytes(7) + bytes([2 if directory else 0]) + bytes(2) + bytes(4) + bytes([len(nm)]) + nm
    body = b'\0' + body   # the extended attribute length
    if len(body) % 2 == 0:
        body += b'\0'
    return bytes([len(body) + 1]) + body


def tiny_iso(path, name, content):
    """Sectors: 16 the volume descriptor, 17 the root, 18 MOV/, 19 the file."""
    img = bytearray(S * 20)
    root_rec = record('\0', 17, S, True)
    img[16 * S:16 * S + 6] = b'\x01CD001'
    img[16 * S + 156:16 * S + 156 + len(root_rec)] = root_rec
    img[17 * S:17 * S + 100] = record('MOV', 18, S, True).ljust(100, b'\0')
    img[18 * S:18 * S + 100] = record(name + ';1', 19, len(content)).ljust(100, b'\0')
    img[19 * S:19 * S + len(content)] = content
    Path(path).write_bytes(bytes(img))


def pes(payload, sid=0xBD):
    body = b'\x80\x00\x00' + payload   # flags, no optional header
    return b'\x00\x00\x01' + bytes([sid]) + struct.pack('>H', len(body)) + body


def pack_header():
    return b'\x00\x00\x01\xba' + b'\x44\x00\x04\x00\x04\x01\x00\x00\x03\xf8'   # MPEG-2 pack: 10 bytes after the start code, no stuffing


def pss(rate, left, right, interleave=4, kind=1):
    """A .PSS with only an audio stream: SShd, SSbd, then blocks alternating left and right."""
    pcm = b''.join(left[i:i + interleave] + right[i:i + interleave] for i in range(0, len(left), interleave))
    header = struct.pack('<4I', kind, rate, 2, interleave) + b'\0' * 4
    stream = b'SShd' + struct.pack('<I', len(header)) + header + b'SSbd' + struct.pack('<I', len(pcm)) + pcm
    out = b''
    for i in range(0, len(stream), 20):   # split over several packets, as the disc does
        out += pack_header() + pes(b'\xff\xa0\x00\x00' + stream[i:i + 20])
    return out + b'\x00\x00\x01\xb9'


class WebMoviesTest(unittest.TestCase):
    def test_the_audio_comes_apart_into_left_and_right(self):
        left, right = bytes(range(0, 16)), bytes(range(100, 116))
        rate, l, r = web_movies.pss_audio(pss(48000, left, right))
        self.assertEqual((rate, l, r), (48000, left, right))

    def test_a_movie_without_audio_gives_none(self):
        self.assertIsNone(web_movies.pss_audio(pack_header() + pes(b'\x00\x00\x01\xb3', sid=0xE0)))

    def test_a_format_that_is_not_16_bit_pcm_is_refused(self):
        data = pss(48000, bytes(8), bytes(8), kind=9)
        with self.assertRaises(ValueError):
            web_movies.pss_audio(data)

    def test_a_movie_cut_short_gives_what_it_has_or_none(self):
        full = pss(48000, bytes(range(16)), bytes(range(100, 116)))
        self.assertIsNone(web_movies.pss_audio(full[:30]))   # cut inside the header
        self.assertIsNotNone(web_movies.pss_audio(full[:-3] + b'\x00\x00\x01\xbd'))   # a packet header cut off at the end: no crash
        self.assertIsNotNone(web_movies.pss_audio(full[:-20]))   # cut in the data: the blocks that came whole

    def test_an_absurd_sample_rate_is_refused(self):
        with self.assertRaises(ValueError):
            web_movies.pss_audio(pss(1, bytes(8), bytes(8)))

    def test_a_file_is_found_in_an_iso_by_its_path_whatever_the_case(self):
        with tempfile.TemporaryDirectory() as d:
            iso = Path(d, 'game.iso')
            tiny_iso(iso, 'INTRO_M.PSS', b'movie bytes')
            self.assertEqual(web_movies.iso_find(iso, 'mov/intro_m.pss'), (19, 11))
            self.assertIsNone(web_movies.iso_find(iso, 'MOV/OTHER.PSS'))
            self.assertEqual(web_movies.read_movie('INTRO_M', iso=iso), b'movie bytes')

    def test_something_that_is_not_an_iso_is_refused(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'x.iso').write_bytes(bytes(S * 20))
            with self.assertRaises(ValueError):
                web_movies.iso_find(Path(d, 'x.iso'), 'MOV/A.PSS')

    def test_a_movie_is_read_from_a_folder_whatever_the_case(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'MOV').mkdir()
            Path(d, 'MOV', 'Int_Main.pss').write_bytes(b'abc')
            self.assertEqual(web_movies.read_movie('INT_MAIN', disc=d), b'abc')
            with self.assertRaises(FileNotFoundError):
                web_movies.read_movie('INTRO_M', disc=d)

    def test_the_manifest_lists_the_files_in_order_with_size_and_hash(self):
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d, 'intro_m.webm'), Path(d, 'int_main.webm')
            a.write_bytes(b'12')
            b.write_bytes(b'345')
            m = web_movies.manifest([a, b])
            self.assertEqual([(x['name'], x['file'], x['bytes']) for x in m['movies']], [('intro_m', 'intro_m.webm', 2), ('int_main', 'int_main.webm', 3)])
            self.assertEqual(len(m['movies'][0]['sha256']), 64)
            json.dumps(m)


if __name__ == '__main__':
    unittest.main()
