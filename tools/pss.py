"""Inspects .PSS movies (PS2 MPEG-2 program streams): streams, the video sequence header and the audio SShd header.

    python3 tools/pss.py disc/MOV/INTRO_M.PSS [more files] [--all]

Reads the first 4 MB (--all: the whole file). Per file: packets per stream (e0 = video, BD/ff = SCE private audio),
video size/aspect/frame rate (MPEG-2 if there is a 0xB5 extension), number of picture headers and the audio format
(type 1 = 16-bit PCM, rate, channels, interleave). The native player (native/wotm_fmv.hpp) does the same demuxing.
"""
import collections
import struct
import sys

RATES = {1: '23.976', 2: '24', 3: '25', 4: '29.97', 5: '30', 6: '50', 7: '59.94', 8: '60'}


def probe(path, read_all=False):
    with open(path, 'rb') as f:
        d = f.read() if read_all else f.read(4 << 20)
    i, streams, vid = 0, collections.Counter(), bytearray()
    aud = collections.defaultdict(bytearray)
    while i + 4 <= len(d):
        if d[i:i + 3] != b'\x00\x00\x01':
            i += 1
            continue
        sid = d[i + 3]
        if sid == 0xBA:  # MPEG-2 pack header (14 bytes + stuffing)
            i += 14 + (d[i + 13] & 7) if (d[i + 4] >> 6) == 1 else 12
            continue
        if sid == 0xB9:
            break
        if sid < 0xBB:
            i += 1
            continue
        ln = struct.unpack('>H', d[i + 4:i + 6])[0]
        p = d[i + 6:i + 6 + ln]
        i += 6 + ln
        if (sid == 0xBD or 0xE0 <= sid <= 0xEF) and len(p) >= 3:
            pay = p[3 + p[2]:]
            if sid == 0xBD:
                streams['BD/%02x' % pay[0]] += 1
                aud[pay[0]] += pay[4:]
            else:
                streams['%02x' % sid] += 1
                vid += pay
        else:
            streams['%02x' % sid] += 1
    print('%s (%s bytes read): packets %s' % (path, len(d), dict(streams)))
    k = vid.find(b'\x00\x00\x01\xb3')
    if k >= 0:
        h = vid[k + 4:k + 8]
        w, hh = (h[0] << 4) | (h[1] >> 4), ((h[1] & 15) << 8) | h[2]
        print('  video %dx%d, aspect %d, %s frames/s, %s, %d pictures' % (
            w, hh, h[3] >> 4, RATES.get(h[3] & 15, '?'), 'MPEG-2' if vid.find(b'\x00\x00\x01\xb5') >= 0 else 'MPEG-1',
            vid.count(b'\x00\x00\x01\x00')))
    for sub, a in sorted(aud.items()):
        if a[:4] == b'SShd' and len(a) >= 32:
            typ, rate, ch, inter = struct.unpack('<4I', a[8:24])
            print('  audio sub %02x: SShd type %d (%s), %d Hz, %d channels, interleave 0x%x; %d bytes' % (
                sub, typ, '16-bit PCM' if typ == 1 else '?', rate, ch, inter, len(a)))
        else:
            print('  audio sub %02x: %d bytes, no SShd' % (sub, len(a)))


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--all']
    if not args:
        print(__doc__)
        sys.exit(1)
    for path in args:
        probe(path, '--all' in sys.argv)
