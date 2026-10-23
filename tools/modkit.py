#!/usr/bin/env python3
"""Kit de mods do WotM (POS_FRONTEIRA.md, item 4; docs/passos/MODS.md). Extrai arquivos do disco para formatos editaveis numa pasta
de trabalho e devolve o que mudou, ja no formato que o jogo le, para mods/<mod>/.

  python tools/modkit.py export MON/LIZARD0                  # .TEX e .RTX do Togera em PNG (pasta de trabalho)
  python tools/modkit.py export SND/SHELL_S.BNK VAG/CITYAMB.VPK   # sons do banco e a musica em WAV
  python tools/modkit.py pack meu_mod                        # o que foi editado vai para mods/meu_mod/
  python tools/modkit.py check [meu_mod]                     # confere os mods (nomes, tamanhos, texIds)

Opcoes: --disc <pasta> (disco extraido; padrao WOTM_DISC ou disc/ do repo), --mods <pasta> (padrao: <runtime>/discroot/mods, o
runtime de .wotm_recomp_dir; sem ele, <disco>/mods), --work <pasta> (padrao: <mods>/_trabalho; pasta com '_' o jogo nao le).
Arquivos do export podem ter curinga (MON/LIZARD*) e sem extensao pegam .TEX e .RTX.

O que cada tipo vira:
- .TEX / .RTX -> <arquivo>_tNNNN.png / <arquivo>_rtx_tNNNN.png (tools/tex.py export). No pack o PNG vai como esta: o desenho nativo
  troca a textura na hora de desenhar (native/wotm_texmods.hpp), em qualquer resolucao. Com --inject o pack tambem regrava o
  .TEX/.RTX (mesmo tamanho e cores da paleta atual), para a imagem do GS emulado (referencia da checagem VU1).
- .VPK / .VAG -> <arquivo>.wav; volta pelo tools/snd989.py (mesma taxa e volumes do original).
- .BNK -> <banco>_sNNN.wav, um por som com amostra; os editados voltam todos num banco so (snd989 bnk-replace).
O pack so leva o que mudou desde o export (sha1 em modkit.json na pasta de trabalho). Nada do jogo vai para o repo: o kit le o
disco do usuario e grava nas pastas dele.
"""
import fnmatch
import glob
import hashlib
import json
import os
import re
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tex      # noqa: E402
import snd989   # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX_DIRS = ("LVL", "MON", "SHELL")
PNG_RE = re.compile(r"^(.+?)(_rtx)?_t(\d+)\.png$", re.I)
BNK_RE = re.compile(r"^(.+)_s(\d+)\.wav$", re.I)


def sha1(path):
    with open(path, "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()


def default_disc():
    return os.environ.get("WOTM_DISC") or os.path.join(REPO, "disc")


def default_mods(disc):
    try:
        with open(os.path.join(REPO, ".wotm_recomp_dir"), encoding="utf-8") as f:
            rt = f.read().strip()
        if os.path.isdir(os.path.join(rt, "discroot")):
            return os.path.join(rt, "discroot", "mods")
    except OSError:
        pass
    return os.path.join(disc, "mods")


def disc_file(disc, rel):
    """Caminho no disco sem depender de maiusculas (o disco extraido no Windows nao liga, o WSL liga)."""
    p = os.path.join(disc, *rel.split("/"))
    if os.path.exists(p):
        return p
    cur = disc
    for part in rel.split("/"):
        if not os.path.isdir(cur):
            return None
        hit = [n for n in os.listdir(cur) if n.upper() == part.upper()]
        if not hit:
            return None
        cur = os.path.join(cur, hit[0])
    return cur


def find_texfile(disc, stem, ext):
    """<STEM>.TEX/.RTX no disco: LVL/, MON/, SHELL/ ou <STEM>/ (como o desenho nativo procura). Devolve o caminho relativo."""
    for d in TEX_DIRS + (stem.upper(),):
        rel = "%s/%s%s" % (d, stem.upper(), ext)
        if disc_file(disc, rel):
            return rel
    return None


def expand(disc, pats):
    out = []
    for pat in pats:
        pat = pat.replace("\\", "/").strip("/")
        if not os.path.splitext(pat)[1]:
            pats2 = [pat + ".TEX", pat + ".RTX"]
        else:
            pats2 = [pat]
        for p in pats2:
            d, base = os.path.split(p)
            dd = disc_file(disc, d) if d else disc
            if not dd:
                continue
            for n in sorted(os.listdir(dd)):
                if fnmatch.fnmatch(n.upper(), base.upper()):
                    out.append((d + "/" if d else "") + n)
    if not out:
        raise SystemExit("nada achado no disco (%s) para %s" % (disc, " ".join(pats)))
    return list(dict.fromkeys(out))


# ---------------------------------------------------------------- export

def export(disc, work, pats):
    state = load_state(work)
    for rel in expand(disc, pats):
        src = disc_file(disc, rel)
        d = os.path.dirname(rel)
        final = os.path.join(work, *d.split("/")) if d else work
        out = os.path.join(work, ".export_tmp")   # grava aqui e depois move, sem atropelar o que ja foi editado
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out)
        ext = os.path.splitext(rel)[1].upper()
        if ext in (".TEX", ".RTX"):
            tex.cmd_export(src, out)
        elif ext in (".VPK", ".VAG"):
            rate, chans = snd989.read_vpk(src)
            name = os.path.splitext(os.path.basename(rel))[0].lower() + ".wav"
            snd989.write_wav(os.path.join(out, name), rate, chans)
            print("%s -> %s (%d Hz, %d canais)" % (rel, name, rate, len(chans)))
        elif ext == ".BNK":
            b = snd989.read_bnk(src)
            stem = os.path.splitext(os.path.basename(rel))[0].lower()
            n = 0
            for i, s in enumerate(b["sounds"]):
                tones = [g for g in s["grains"] if g["type"] in (1, 9)]
                if not tones:
                    continue
                chans = [snd989.adpcm_decode(b["samples"], g["sample"])[0] for g in tones[:2]]
                rate = round(snd989.tone_rate(tones[0]["note"], tones[0]["fine"]))
                snd989.write_wav(os.path.join(out, "%s_s%03d.wav" % (stem, i)), rate, chans)
                n += 1
            print("%s -> %d sons em WAV (%s_sNNN.wav)" % (rel, n, stem))
        else:
            print("%s: tipo sem conversor; copiado como esta" % rel)
            shutil.copy2(src, os.path.join(out, os.path.basename(rel)))
        os.makedirs(final, exist_ok=True)
        kept = 0
        for n in sorted(os.listdir(out)):
            p, q = os.path.join(out, n), os.path.join(final, n)
            key = os.path.relpath(q, work).replace("\\", "/")
            old = state["files"].get(key, {}).get("sha1")
            if os.path.exists(q) and not n.endswith(".json") and sha1(q) != old:
                kept += 1   # editado depois do export anterior: fica o do usuario
                continue
            os.replace(p, q)
            if not n.endswith(".json"):
                state["files"].setdefault(key, {})["sha1"] = sha1(q)
        shutil.rmtree(out, ignore_errors=True)
        if kept:
            print("  %d arquivo(s) ja editado(s) na pasta de trabalho ficaram como estavam" % kept)
        save_state(work, state)
    print("pasta de trabalho: %s (edite e rode: python tools/modkit.py pack <mod>)" % work)


def load_state(work):
    try:
        with open(os.path.join(work, "modkit.json"), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {"files": {}}


def save_state(work, state):
    os.makedirs(work, exist_ok=True)
    with open(os.path.join(work, "modkit.json"), "w", encoding="utf-8") as f:
        json.dump(state, f, indent=1, sort_keys=True)


# ---------------------------------------------------------------- pack

def pack(disc, work, mods, mod, inject=False):
    state = load_state(work)
    dst = os.path.join(mods, mod)
    changed = []
    for root, dirs, files in os.walk(work):
        dirs[:] = [x for x in dirs if not x.startswith(".")]
        for n in files:
            p = os.path.join(root, n)
            rel = os.path.relpath(p, work).replace("\\", "/")
            if n.endswith(".json") or n.startswith("."):
                continue
            old = state["files"].get(rel, {}).get("sha1")
            if old == sha1(p):
                continue
            changed.append(rel)
    if not changed:
        print("nada mudou em %s desde o export" % work)
        return
    banks = {}
    for rel in sorted(changed):
        p = os.path.join(work, *rel.split("/"))
        d, n = os.path.split(rel)
        m = PNG_RE.match(n)
        if m:
            stem, rtx, tid = m.group(1), bool(m.group(2)), int(m.group(3))
            os.makedirs(os.path.join(dst, *d.split("/")) if d else dst, exist_ok=True)
            shutil.copy2(p, os.path.join(dst, *rel.split("/")))
            print("textura %s -> %s/%s" % (n, mod, rel))
            if inject:
                inject_png(disc, dst, stem, rtx, tid, p)
            continue
        if n.lower().endswith(".wav"):
            mb = BNK_RE.match(n)
            if mb and find_disc_ext(disc, d, mb.group(1), ".BNK"):
                banks.setdefault((d, mb.group(1)), []).append((int(mb.group(2)), p))
                continue
            stem = os.path.splitext(n)[0]
            for ext in (".VPK", ".VAG"):
                orig = find_disc_ext(disc, d, stem, ext)
                if orig:
                    out = os.path.join(dst, *d.split("/"), stem.upper() + ext)
                    os.makedirs(os.path.dirname(out), exist_ok=True)
                    rate, chans = snd989.read_wav_any(p)
                    if ext == ".VPK":
                        like = open(orig, "rb").read()
                        _, _, _, orate, onch = struct.unpack_from("<5I", like, 4)
                        vols = [struct.unpack_from("<HH", like, 0x18 + 4 * c) for c in range(onch)]
                        chans = [snd989.resample(c, rate, orate) for c in snd989.to_channels(chans, onch)]
                        snd989.write_vpk(out, orate, chans, vols)
                    else:
                        snd989.write_vag(out, rate, snd989.to_channels(chans, 1)[0])
                    print("som %s -> %s/%s" % (n, mod, os.path.relpath(out, dst).replace("\\", "/")))
                    break
            else:
                print("AVISO: %s nao corresponde a nenhum .VPK/.VAG/.BNK do disco; ignorado" % rel)
            continue
        out = os.path.join(dst, *rel.split("/"))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copy2(p, out)
        print("arquivo %s -> %s/%s (como esta)" % (rel, mod, rel))
    for (d, stem), sounds in sorted(banks.items()):
        src = find_disc_ext(disc, d, stem, ".BNK")
        out = os.path.join(dst, *d.split("/"), stem.upper() + ".BNK")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        tmp = out + ".tmp"
        cur = src
        for i, wav in sorted(sounds):
            snd989.bnk_replace(cur, i, wav, tmp)
            os.replace(tmp, out)
            cur = out
        print("banco %s.BNK: %d som(ns) trocado(s) -> %s" % (stem.upper(), len(sounds), os.path.relpath(out, mods)))
    print("mod em %s" % dst)


def find_disc_ext(disc, d, stem, ext):
    return disc_file(disc, ("%s/%s%s" % (d, stem, ext)) if d else stem + ext)


def inject_png(disc, dst, stem, rtx, tid, png):
    """Regrava a textura no .TEX/.RTX do mod (ou do disco), mesmo tamanho: so para a imagem do GS emulado."""
    ext = ".RTX" if rtx else ".TEX"
    rel = find_texfile(disc, stem, ext)
    if not rel:
        print("  --inject: %s%s nao achado no disco" % (stem, ext))
        return
    out = os.path.join(dst, *rel.split("/"))
    src = out if os.path.exists(out) else disc_file(disc, rel)
    w, h, _ = tex.read_image(png)
    pk = next((p for p in tex.parse(tex.read_file(src)[0])["packets"] if p.texId == tid), None)
    if pk is None or (w, h) != (pk.w, pk.h):
        print("  --inject: %s pulado (precisa de %dx%d, mesmo tamanho da original)" % (os.path.basename(png), pk.w if pk else 0,
                                                                                         pk.h if pk else 0))
        return
    os.makedirs(os.path.dirname(out), exist_ok=True)
    rtx_path = None if rtx else disc_file(disc, rel[:-4] + ".RTX")
    ngp = disc_file(disc, rel[:-4] + ".NGP")
    tex.cmd_inject(src, tid, png, out + ".tmp", rtx_path, None, ngp)
    os.replace(out + ".tmp", out)


# ---------------------------------------------------------------- check

def check(disc, mods, only=None):
    if not os.path.isdir(mods):
        raise SystemExit("sem pasta de mods em %s" % mods)
    names = sorted(n for n in os.listdir(mods) if os.path.isdir(os.path.join(mods, n)))
    seen = {}
    problems = 0
    for mod in names:
        if only and mod != only:
            continue
        off = mod[0] in "_."
        print("%s%s" % (mod, " (desligado: comeca com '_' ou '.')" if off else ""))
        if off and not only:
            continue
        for root, _dirs, files in os.walk(os.path.join(mods, mod)):
            for n in sorted(files):
                p = os.path.join(root, n)
                rel = os.path.relpath(p, os.path.join(mods, mod)).replace("\\", "/")
                note = ""
                m = PNG_RE.match(n)
                if m:
                    stem, rtx, tid = m.group(1), bool(m.group(2)), int(m.group(3))
                    key = ("png", stem.upper(), rtx, tid)
                    trel = find_texfile(disc, stem, ".RTX" if rtx else ".TEX")
                    if not trel:
                        note, problems = "ERRO: %s%s nao existe no disco" % (stem.upper(), ".RTX" if rtx else ".TEX"), problems + 1
                    else:
                        modded = os.path.join(mods, mod, *trel.split("/"))
                        pk = next((q for q in tex.parse(tex.read_file(modded if os.path.exists(modded) else disc_file(disc, trel))[0])["packets"]
                                   if q.texId == tid and not (rtx and q.is_palette())), None)
                        if pk is None:
                            note, problems = "ERRO: %s nao tem texId %d" % (trel, tid), problems + 1
                        else:
                            w, h, _ = tex.read_image(p) if n.lower().endswith(".png") else (0, 0, None)
                            ratio = w * pk.h == h * pk.w
                            note = "textura de %s, %dx%d (jogo %dx%d)%s" % (trel, w, h, pk.w, pk.h,
                                                                            "" if ratio else " AVISO: proporcao diferente, sai esticada")
                else:
                    key = ("file", rel.upper())
                    orig = disc_file(disc, rel)
                    size = os.path.getsize(p)
                    if n.lower() == "modkit.json":
                        continue
                    if not orig:
                        note = "arquivo novo (nao esta no disco; ninguem o pede ainda)"
                    else:
                        osz = os.path.getsize(orig)
                        note = "%d bytes (disco %d)" % (size, osz)
                        if size > osz and os.path.splitext(n)[1].upper() in (".TEX", ".RTX", ".NGP", ".BNK"):
                            note += " AVISO: maior que o do disco, o jogo carrega em regioes fixas e pode sobrescrever o que vem depois"
                if not off:
                    if key in seen:
                        note += " (vence %s)" % seen[key]
                    seen[key] = mod
                print("  %-40s %s" % (rel, note))
    print("%d erro(s)" % problems)
    return 1 if problems else 0


def main(argv):
    opts, args, i = {}, [], 0
    while i < len(argv):
        if argv[i] in ("--disc", "--mods", "--work"):
            opts[argv[i][2:]] = argv[i + 1]
            i += 2
        elif argv[i] == "--inject":
            opts["inject"] = True
            i += 1
        else:
            args.append(argv[i])
            i += 1
    if not args or args[0] not in ("export", "pack", "check"):
        print(__doc__)
        return 1
    disc = opts.get("disc") or default_disc()
    mods = opts.get("mods") or default_mods(disc)
    work = opts.get("work") or os.path.join(mods, "_trabalho")
    if not os.path.isdir(disc):
        raise SystemExit("disco extraido nao achado em %s (--disc ou WOTM_DISC)" % disc)
    if args[0] == "export":
        if len(args) < 2:
            raise SystemExit("export: diga o que extrair (ex.: MON/LIZARD0)")
        export(disc, work, args[1:])
    elif args[0] == "pack":
        if len(args) != 2 or args[1][0] in "_.":
            raise SystemExit("pack: um nome de mod (sem '_' ou '.' no comeco)")
        pack(disc, work, mods, args[1], opts.get("inject", False))
    else:
        return check(disc, mods, args[1] if len(args) > 1 else None)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
