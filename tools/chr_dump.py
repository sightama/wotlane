#!/usr/bin/env python3
"""Tabela dos golpes de um monstro a partir do RST/<MONSTRO>.CHR do disco do usuario (so le; nada vai ao repo).

Uso:
    python tools/chr_dump.py disc/RST/MONKEY.CHR            # tabela dos socos (chave 0xFCC)
    python tools/chr_dump.py disc/RST/MONKEY.CHR --keys     # lista as chaves do arquivo
    python tools/chr_dump.py disc/RST/MONKEY.CHR --csv      # a mesma tabela em CSV
    python tools/chr_dump.py disc/RST/*.CHR                 # varios monstros

Formato (docs/FORMATOS.md, "Personagem (.CHR)"): cabecalho de 0xC bytes e depois chaves {u32 id; u32 tamanho com o cabecalho;
dados}, lidas pelo charreset::readKey. A chave 0xFCC guarda as configs do StatePunch: 10 posicoes no combo (o 1o golpe, o 2o...)
x 6 tipos de golpe, cada uma com 0xA0 bytes a partir de key + 0x10 + pos*0x3C0 + tipo*0xA0. O jogo as copia para o
StatePunch do Monster (Monster + 0x8470 + 0x20 + pos*0x420 + tipo*0xB0, a PunchConfig), que o modo treino le na RAM.
"""
import argparse
import os
import struct
import sys

PUNCH_KEY = 0xFCC
PUNCH_DEFAULTS_KEY = 0xFA1   # +0x30: raio padrao do punho (StatePunch +0x2980), quando o do golpe e 0
TIERS, TYPES = 10, 6

# localizadores (ossos nomeados): Monster::handleLocator, ids 0x802.. (treino_hitboxes.md)
LOCATORS = {0x802: 'MAO E', 0x803: 'MAO D', 0x804: 'PE E', 0x805: 'PE D'}


def read_keys(data):
    keys, off = [], 0xC
    while off + 8 <= len(data):
        kid, size = struct.unpack_from('<II', data, off)
        if kid == 0 or size < 8 or off + size > len(data):
            break
        keys.append((kid, off, size))
        off += size
    return keys


def f32(d, o):
    return struct.unpack_from('<f', d, o)[0]


def i32(d, o):
    return struct.unpack_from('<i', d, o)[0]


def loc_name(v):
    if v < 0x7D0:
        return '-'
    return LOCATORS.get(v, '0x%X' % v)


def punch_table(data, keys):
    """Linhas da tabela dos socos (dicts), na ordem posicao, tipo; [] sem a chave 0xFCC."""
    by_id = {k: (o, s) for k, o, s in keys}
    if PUNCH_KEY not in by_id:
        return [], None
    o, _ = by_id[PUNCH_KEY]
    base = o + 0x10
    default_r = None
    if PUNCH_DEFAULTS_KEY in by_id:
        fo, fs = by_id[PUNCH_DEFAULTS_KEY]
        if fs >= 0x34:
            default_r = f32(data, fo + 0x30)
    rows = []
    for t in range(TIERS):
        for k in range(TYPES):
            b = base + t * 0x3C0 + k * 0xA0
            anim = i32(data, b + 0x28)
            dmg = f32(data, b + 0x20)
            locs = [i32(data, b + 0x10), i32(data, b + 0x14)]
            if anim <= 0 and dmg == 0:
                continue
            r = f32(data, b + 0xA8)
            rows.append({
                'pos': t, 'tipo': k, 'anim': anim,
                'loc': '+'.join(loc_name(v) for v in locs if v >= 0x7D0) or '-',
                'ini': f32(data, b + 0x2C), 'fim': f32(data, b + 0x30),
                'cont_ini': f32(data, b + 0x34), 'cont_fim': f32(data, b + 0x38),
                'dano': dmg, 'raio': r if r > 0 or default_r is None else default_r, 'raio_padrao': r <= 0,
                'kb_h': f32(data, b + 0x8C), 'kb_v': f32(data, b + 0x90),
                'atord': f32(data, b + 0x94), 'pausa': f32(data, b + 0x98),
            })
    return rows, default_r


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='+')
    ap.add_argument('--keys', action='store_true', help='lista as chaves do arquivo')
    ap.add_argument('--csv', action='store_true', help='tabela em CSV')
    a = ap.parse_args()
    if a.csv:
        print('monstro,pos,tipo,anim,loc,ini,fim,cont_ini,cont_fim,dano,raio,kb_h,kb_v,atord,pausa')
    for path in a.files:
        data = open(path, 'rb').read()
        keys = read_keys(data)
        name = os.path.splitext(os.path.basename(path))[0]
        if a.keys:
            print('%s: %s' % (name, ' '.join('%X(%X)' % (k, s) for k, _, s in keys)))
            continue
        rows, default_r = punch_table(data, keys)
        if a.csv:
            for r in rows:
                print('%s,%d,%d,%d,%s,%.3f,%.3f,%.3f,%.3f,%.1f,%.2f,%.0f,%.0f,%.0f,%.0f' % (
                    name, r['pos'], r['tipo'], r['anim'], r['loc'], r['ini'], r['fim'], r['cont_ini'], r['cont_fim'],
                    r['dano'], r['raio'], r['kb_h'], r['kb_v'], r['atord'], r['pausa']))
            continue
        if not rows:
            print('%s: sem a chave 0x%X (socos)' % (name, PUNCH_KEY))
            continue
        print('%s: socos (chave 0x%X), raio padrao %s' % (name, PUNCH_KEY, '%.2f' % default_r if default_r is not None else '?'))
        print('  pos tipo anim  janela      dano  raio   atord  knockback   pausa  localizadores')
        for r in rows:
            print('  %3d %4d %4d  %4.0f-%3.0f%%  %5.1f  %5.1f%s %5.0f  %4.0f/%-4.0f  %5.0f  %s' % (
                r['pos'] + 1, r['tipo'], r['anim'], r['ini'] * 100, r['fim'] * 100, r['dano'], r['raio'],
                '*' if r['raio_padrao'] else ' ', r['atord'], r['kb_h'], r['kb_v'], r['pausa'], r['loc']))
        print('  (pos: golpe do combo, 1 = o primeiro; janela: % da animacao com teste de golpe; * raio padrao; atord > 100 atordoa)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
