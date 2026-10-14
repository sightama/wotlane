#!/usr/bin/env python3
"""Leitor dos formatos de som do War of the Monsters (989snd): .BNK (SBlk v1), .VPK e .VAG. Documentado em docs/FORMATOS.md ("Som").

  python tools/snd989.py bnk disc/SND/SHELL_S.BNK            # sons e grains de um banco
  python tools/snd989.py stats disc/SND                       # tipos de grain/flags usados em todos os bancos
  python tools/snd989.py wav disc/SND/SHELLMUS.BNK 0 out.wav  # som N do banco (só os grains TONE, sem envelope) em WAV
  python tools/snd989.py wav disc/VAG/CITYAMB.VPK - out.wav   # stream .VPK/.VAG inteiro em WAV
"""
import collections
import os
import struct
import sys
import wave

GRAIN_NAMES = {0: "NULL", 1: "TONE", 2: "XREF_ID", 3: "XREF_NUM", 4: "LFO", 5: "STARTCHILD", 6: "STOPCHILD", 7: "PLUGIN",
               8: "BRANCH", 9: "TONE2", 20: "CONTROL_NULL", 21: "LOOP_START", 22: "LOOP_END", 23: "LOOP_CONTINUE", 24: "STOP",
               25: "RAND_PLAY", 26: "RAND_DELAY", 27: "RAND_PB", 28: "PB", 29: "ADD_PB", 30: "SET_REG", 31: "SET_REG_RAND",
               32: "INC_REG", 33: "DEC_REG", 34: "TEST_REG", 35: "MARKER", 36: "GOTO_MARKER", 37: "GOTO_RANDOM_MARKER",
               38: "WAIT_ALL_VOICES", 39: "PLAY_CYCLE", 40: "ADD_REG", 41: "KEY_OFF", 42: "KILL_VOICES", 43: "ON_STOP_MARKER",
               44: "COPY_REG"}


def read_bnk(path):
    """Arquivo: u32 tipo (3), u32 nChunks (2), {u32 offset, u32 tamanho} x n; chunk 0 = SBlk, chunk 1 = amostras ADPCM."""
    d = open(path, "rb").read()
    typ, n = struct.unpack_from("<II", d, 0)
    chunks = [struct.unpack_from("<II", d, 8 + 8 * i) for i in range(n)]
    blk = d[chunks[0][0]:chunks[0][0] + chunks[0][1]]
    smp = d[chunks[1][0]:chunks[1][0] + chunks[1][1]] if n > 1 else b""
    if blk[:4] != b"SBlk":
        raise ValueError("%s: sem SBlk" % path)
    ver, flags, bank_id = struct.unpack_from("<II4s", blk, 4)
    n_sounds, n_grains, n_vags = struct.unpack_from("<hhh", blk, 0x16)
    first_sound, first_grain, vags_in_sr, vag_data_size, sram_alloc, next_block = struct.unpack_from("<6I", blk, 0x1c)
    block_names, sfx_ud = struct.unpack_from("<II", blk, 0x34)   # v1: sem GrainData
    sounds = []
    for i in range(n_sounds):
        o = first_sound + 12 * i
        vol, vgroup, pan, ng, inst, sflags, fg = struct.unpack_from("<bbhbbHI", blk, o)
        grains = []
        for g in range(ng):
            go = first_grain + fg + 0x28 * g
            gtype, delay = struct.unpack_from("<Ii", blk, go)
            body = blk[go + 8:go + 0x28]
            gr = {"type": gtype, "delay": delay, "raw": body}
            if gtype in (1, 9):
                (pri, tvol, note, fine, tpan, mlo, mhi, pblo, pbhi, adsr1, adsr2, tflags, soff) = struct.unpack_from("<bbbbhbbbbHHHI", body, 0)
                gr.update(pri=pri, vol=tvol, note=note, fine=fine, pan=tpan, adsr1=adsr1, adsr2=adsr2, tflags=tflags, sample=soff)
            elif gtype in (5, 6, 8):
                gr["play"] = struct.unpack_from("<iibbbbi16s", body, 0)
            else:
                gr["params"] = struct.unpack_from("<hhhh", body, 0)
            grains.append(gr)
        sounds.append({"vol": vol, "group": vgroup, "pan": pan, "inst": inst, "flags": sflags, "grains": grains})
    names = {}
    if flags & 0x100 and block_names:
        name_tab = block_names + struct.unpack_from("<I", blk, block_names + 8)[0]
        for h in struct.unpack_from("<32h", blk, block_names + 0x18):
            o = name_tab + 0x14 * h
            while o + 0x14 <= len(blk) and blk[o] != 0:
                nm = blk[o:o + 16].split(b"\0")[0].decode("latin-1")
                names.setdefault(struct.unpack_from("<h", blk, o + 16)[0], nm)
                o += 0x14
    return {"type": typ, "version": ver, "flags": flags, "bank": bank_id[::-1].decode("latin-1"), "sounds": sounds,
            "n_grains": n_grains, "n_vags": n_vags, "samples": smp, "names": names}


# ADPCM do SPU2: blocos de 16 bytes (filtro/shift, flags, 14 bytes = 28 amostras de 4 bits)
FILTERS = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]


def adpcm_decode(data, off=0, stop_at_end=True, hist=None):
    out = []
    h1, h2 = hist or (0, 0)
    while off + 16 <= len(data):
        sf, fl = data[off], data[off + 1]
        shift, filt = sf & 0xF, min(sf >> 4, 4)
        k0, k1 = FILTERS[filt]
        for i in range(28):
            b = data[off + 2 + i // 2]
            nib = (b >> 4) if i & 1 else (b & 0xF)
            s = (nib << 12) & 0xFFFF
            s = (s - 0x10000 if s & 0x8000 else s) >> shift
            s += (h1 * k0 + h2 * k1) >> 6
            s = max(-32768, min(32767, s))
            out.append(s)
            h2, h1 = h1, s
        off += 16
        if stop_at_end and fl & 1:   # fim (com ou sem laço)
            break
    return out, off


def write_wav(path, rate, channels):
    with wave.open(path, "wb") as w:
        w.setnchannels(len(channels))
        w.setsampwidth(2)
        w.setframerate(rate)
        n = min(len(c) for c in channels)
        frames = bytearray()
        for i in range(n):
            for c in channels:
                frames += struct.pack("<h", c[i])
        w.writeframes(bytes(frames))


def read_vpk(path):
    """' KPV': u32 bytes por canal, u32 início dos dados, u32 intercalação, u32 Hz, u32 canais."""
    d = open(path, "rb").read()
    if d[:4] == b" KPV":
        per_ch, start, inter, rate, nch = struct.unpack_from("<5I", d, 4)
        chans = [[] for _ in range(nch)]
        hist = [(0, 0)] * nch
        o = start
        while o < len(d):
            for c in range(nch):
                blk = d[o:o + inter]
                s, _ = adpcm_decode(blk, 0, stop_at_end=False, hist=hist[c])
                chans[c] += s
                if len(s) >= 2:
                    hist[c] = (s[-1], s[-2])
                o += inter
        return rate, chans
    if d[:4] == b"VAGp":
        size, rate = struct.unpack_from(">I4xI", d, 0xc)
        s, _ = adpcm_decode(d, 0x30, stop_at_end=False)
        return rate, [s]
    raise ValueError("formato desconhecido: %s" % path)


def tone_rate(center_note, center_fine, note=60, fine=0):
    """Taxa em que o 989snd toca a amostra de um TONE (os efeitos sao tocados na nota 60). CenterNote negativa = nota de PS2
    (base 48 kHz); positiva = de PS1 (base 44,1 kHz). Como no sceSdNote2Pitch, o CenterFine soma ao fine."""
    base = 48000 if center_note < 0 else 44100
    return base * 2 ** ((note - abs(center_note) + (fine + center_fine) / 128) / 12)


def main():
    cmd = sys.argv[1]
    if cmd == "bnk":
        b = read_bnk(sys.argv[2])
        print("banco %s v%d flags=%#x sons=%d grains=%d vags=%d amostras=%d bytes" % (
            b["bank"], b["version"], b["flags"], len(b["sounds"]), b["n_grains"], b["n_vags"], len(b["samples"])))
        for i, s in enumerate(b["sounds"]):
            print("som %3d %-16s vol=%d grupo=%d pan=%d inst=%d flags=%#x" % (i, b["names"].get(i, ""), s["vol"], s["group"], s["pan"],
                                                                             s["inst"], s["flags"]))
            for g in s["grains"]:
                extra = ""
                if g["type"] in (1, 9):
                    extra = "vol=%d nota=%d.%d pan=%d adsr=%04x/%04x tflags=%#x amostra=%#x" % (
                        g["vol"], g["note"], g["fine"], g["pan"], g["adsr1"], g["adsr2"], g["tflags"], g["sample"])
                elif "play" in g:
                    extra = str(g["play"][:6])
                else:
                    extra = str(g["params"])
                print("    %-14s atraso=%-5d %s" % (GRAIN_NAMES.get(g["type"], g["type"]), g["delay"], extra))
    elif cmd == "stats":
        types = collections.Counter()
        sflags = collections.Counter()
        bflags = collections.Counter()
        groups = collections.Counter()
        for fn in sorted(os.listdir(sys.argv[2])):
            if not fn.upper().endswith(".BNK"):
                continue
            b = read_bnk(os.path.join(sys.argv[2], fn))
            bflags[hex(b["flags"])] += 1
            print("%-14s %s v%d sons=%4d grains=%4d vags=%4d amostras=%8d nomes=%d" % (
                fn, b["bank"], b["version"], len(b["sounds"]), b["n_grains"], b["n_vags"], len(b["samples"]), len(b["names"])))
            for s in b["sounds"]:
                sflags[hex(s["flags"])] += 1
                groups[s["group"]] += 1
                for g in s["grains"]:
                    types[GRAIN_NAMES.get(g["type"], str(g["type"]))] += 1
        print("tipos de grain:", dict(types.most_common()))
        print("flags de som:", dict(sflags.most_common()))
        print("flags de banco:", dict(bflags))
        print("grupos de volume:", dict(sorted(groups.items())))
    elif cmd == "wav":
        path, which, out = sys.argv[2], sys.argv[3], sys.argv[4]
        if path.upper().endswith(".BNK"):
            b = read_bnk(path)
            tones = [g for g in b["sounds"][int(which)]["grains"] if g["type"] in (1, 9)]
            chans = [adpcm_decode(b["samples"], g["sample"])[0] for g in tones]
            rate = round(tone_rate(tones[0]["note"], tones[0]["fine"]))
            write_wav(out, rate, chans[:2] if len(chans) >= 2 else chans)
        else:
            rate, chans = read_vpk(path)
            write_wav(out, rate, chans)
        print("gravado", out)


if __name__ == "__main__":
    main()
