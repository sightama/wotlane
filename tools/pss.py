"""Inspeciona filmes .PSS (MPEG-2 program stream do PS2): fluxos, cabecalho de sequencia do video e cabecalho SShd do audio.

    python3 tools/pss.py disc/MOV/INTRO_M.PSS [mais arquivos] [--all]

Le os primeiros 4 MB (--all: o arquivo inteiro). Por arquivo: pacotes por fluxo (e0 = video, BD/ff = audio privado da SCE),
tamanho/proporcao/taxa do video (MPEG-2 se houver extensao 0xB5), numero de cabecalhos de imagem e o formato do audio
(tipo 1 = PCM 16 bits, taxa, canais, intercalacao). O tocador nativo (native/wotm_fmv.hpp) faz a mesma separacao.
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
        if sid == 0xBA:  # cabecalho de pacote MPEG-2 (14 bytes + enchimento)
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
    print('%s (%s bytes lidos): pacotes %s' % (path, len(d), dict(streams)))
    k = vid.find(b'\x00\x00\x01\xb3')
    if k >= 0:
        h = vid[k + 4:k + 8]
        w, hh = (h[0] << 4) | (h[1] >> 4), ((h[1] & 15) << 8) | h[2]
        print('  video %dx%d, aspecto %d, %s quadros/s, %s, %d imagens' % (
            w, hh, h[3] >> 4, RATES.get(h[3] & 15, '?'), 'MPEG-2' if vid.find(b'\x00\x00\x01\xb5') >= 0 else 'MPEG-1',
            vid.count(b'\x00\x00\x01\x00')))
    for sub, a in sorted(aud.items()):
        if a[:4] == b'SShd' and len(a) >= 32:
            typ, rate, ch, inter = struct.unpack('<4I', a[8:24])
            print('  audio sub %02x: SShd tipo %d (%s), %d Hz, %d canais, intercalacao 0x%x; %d bytes' % (
                sub, typ, 'PCM 16 bits' if typ == 1 else '?', rate, ch, inter, len(a)))
        else:
            print('  audio sub %02x: %d bytes, sem SShd' % (sub, len(a)))


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--all']
    if not args:
        print(__doc__)
        sys.exit(1)
    for path in args:
        probe(path, '--all' in sys.argv)
