#!/usr/bin/env python3
"""WotM mod kit (docs/passos/MODS.md). Extracts files from the disc into editable formats in the mod's work folder
and turns what changed back into the format the game reads, in mods/<mod>/. One mod = one project: each mod has its own
work folder (<mods>/_work/<mod>/, with its own modkit.json), and packing one mod never takes what was edited for another.

  python tools/modkit.py export my_mod MON/LIZARD0           # Togera's .TEX and .RTX as PNG (my_mod's work folder)
  python tools/modkit.py export my_mod SND/SHELL_S.BNK VAG/CITYAMB.VPK   # the bank's sounds and the music as WAV
  python tools/modkit.py pack my_mod                         # what was edited goes into mods/my_mod/
  python tools/modkit.py check [my_mod]                      # checks the mods (names, sizes, texIds) and conflicts between them
  python tools/modkit.py revert my_mod MON/lizard0_t0004.png # restores a work folder file to the disc original
  python tools/modkit.py skin new my_mod togera green --base 1 --name "GREEN TOGERA"   # new skin: skin.json + base costume PNGs
  python tools/modkit.py skin new my_mod goliath gold        # skin for a custom monster (id from its monster.json)
  python tools/modkit_gui.py                                 # the same with a GUI (the launcher's "Mod kit" button)

Options: --disc <folder> (extracted disc; default WOTM_DISC or the repo's disc/), --mods <folder> (default: <runtime>/discroot/mods,
the runtime from .wotm_recomp_dir; without it, <disc>/mods), --work <folder> (default: <mods>/_work/<mod>; the game skips folders
starting with '_'). Mod name: must not start with '_' or '.', and no \\ / : * ? " < > |.
Export file names can use wildcards (MON/LIZARD*); without an extension they pick up both .TEX and .RTX.

What each type becomes:
- .TEX / .RTX -> <file>_tNNNN.png / <file>_rtx_tNNNN.png (tools/tex.py export). Pack copies the PNG as is: the native renderer
  swaps the texture at draw time (native/wotm_texmods.hpp), at any resolution. With --inject, pack also rewrites the
  .TEX/.RTX (same size, colors from the current palette), for the emulated GS image (the VU1 check reference).
- .VPK / .VAG -> <file>.wav; goes back through tools/snd989.py (same rate and volumes as the original).
- .BNK -> <bank>_sNNN.wav, one per sound that has a sample; the edited ones all go back into a single bank (snd989 bnk-replace).
Pack only takes what changed since the export (sha1 in modkit.json in the work folder). Nothing from the game goes into the repo:
the kit reads the user's disc and writes into the user's folders.
"""
import contextlib
import fnmatch
import io
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
PNG_RE = re.compile(r"^(.+?)(_rtx)?_t(\d+)(?:_sempaleta)?\.png$", re.I)   # _sempaleta: export did not find the palette (grayscale)
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


WORK = "_work"                  # folder holding the work folders, inside <mods> (starts with '_': the game skips it)
BAD_MOD_CHARS = '\\/:*?"<>|'


def mod_name_error(mod):
    """Error message (shown in the GUI) if the name is not valid for a mod; None if it is."""
    if not mod or not mod.strip():
        return "the mod needs a name"
    if mod != mod.strip():
        return "the mod name cannot start or end with spaces"
    if mod[0] in "_.":
        return "the mod name cannot start with '_' or '.' (the game skips those folders)"
    bad = sorted(set(c for c in mod if c in BAD_MOD_CHARS or ord(c) < 32))
    if bad:
        return "the mod name cannot have %s" % " ".join(repr(c) for c in bad)
    return None


NEW_MOD_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$")


def new_mod_name_error(mod):
    """Strict rule for a new mod (the GUI and 'New mod...'): letters, digits, '_' and '-', starting with a letter or digit,
    up to 64. Existing mods with other names are still accepted (mod_name_error)."""
    if not mod:
        return "the mod needs a name"
    if not NEW_MOD_RE.match(mod):
        return "use only letters, digits, '_' and '-', starting with a letter or digit (no spaces), up to 64"
    return None


def adopt_mod(disc, mods, mod, work=None):
    """A mod made before it had a work folder (or made by hand): copies the files of mods/<mod>/ into its work folder, marked
    as edited (empty sha1 in the state), so the window shows them and pack takes them back. skin.json and monster.json
    stay only in the mod. Runs every time the mod is selected: only what the work folder lacks and the state never saw comes in
    (a file removed on purpose does not come back). Returns how many it copied."""
    if work is None:
        work = work_dir(mods, mod)
    src = os.path.join(mods, mod)
    if not os.path.isdir(src):
        return 0
    state = load_state(work)
    n = 0
    for root, dirs, files in os.walk(src):
        dirs[:] = [x for x in dirs if not x.startswith(".")]
        for name in sorted(files):
            if name.lower() in ("skin.json", "monster.json", "modkit.json") or name.startswith("."):
                continue
            p = os.path.join(root, name)
            rel = os.path.relpath(p, src).replace("\\", "/")
            q = os.path.join(work, *rel.split("/"))
            if os.path.exists(q) or rel in state["files"]:
                continue
            os.makedirs(os.path.dirname(q), exist_ok=True)
            shutil.copy2(p, q)
            ent = {"sha1": "", "adopted": True}   # an empty sha1 never matches: counts as edited
            m = PNG_RE.match(name)
            if m:
                trel = find_texfile(disc, m.group(1), ".RTX" if m.group(2) else ".TEX")
                if trel:
                    pk = next((k for k in tex.parse(tex.read_file(disc_file(disc, trel))[0])["packets"]
                               if k.texId == int(m.group(3)) and not (m.group(2) and k.is_palette())), None)
                    if pk is not None:
                        ent["size"] = [pk.w, pk.h]
            state["files"][rel] = ent
            n += 1
    if n:
        save_state(work, state)
        print("%d file(s) of %s copied into its work folder (they count as edited)" % (n, mod))
    return n


def validate_mod_name(mod, what=""):
    """The mod name, or SystemExit with the message (prefixed by what: "pack: ...")."""
    err = mod_name_error(mod)
    if err:
        raise SystemExit("%s%s: %r" % (what + ": " if what else "", err, mod))
    return mod


def work_dir(mods, mod):
    """The mod's work folder: <mods>/_work/<mod>/ (the modkit.json state file lives there)."""
    return os.path.join(mods, WORK, validate_mod_name(mod))


def disc_file(disc, rel):
    """Path on the disc, case-insensitive (the disc extracted on Windows does not care about case, WSL does)."""
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
    """<STEM>.TEX/.RTX on the disc: LVL/, MON/, SHELL/ or <STEM>/ (as the native renderer searches). Returns the relative path."""
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
        raise SystemExit("nothing found on the disc (%s) for %s" % (disc, " ".join(pats)))
    return list(dict.fromkeys(out))


# ---------------------------------------------------------------- export

def export(disc, work, pats, dest=None):
    """dest: subfolder of the work folder to use instead of the disc folder (skin new: skins/<id>)."""
    state = load_state(work)
    for rel in expand(disc, pats):
        src = disc_file(disc, rel)
        d = dest if dest else os.path.dirname(rel)
        final = os.path.join(work, *d.split("/")) if d else work
        out = os.path.join(work, ".export_tmp")   # write here, then move, without clobbering what was already edited
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out)
        ext = os.path.splitext(rel)[1].upper()
        if ext in (".TEX", ".RTX"):
            with contextlib.redirect_stdout(io.StringIO()):   # tex.py would print the temporary folder
                man = tex.cmd_export(src, out)
            n = sum(1 for e in man["textures"] if "png" in e)
            print("%s -> %d texture(s) as PNG (%s)" % (rel, n, (os.path.splitext(os.path.basename(rel))[0].lower() +
                                                                 ("_rtx" if ext == ".RTX" else "") + "_tNNNN.png")))
        elif ext in (".VPK", ".VAG"):
            rate, chans = snd989.read_vpk(src)
            name = os.path.splitext(os.path.basename(rel))[0].lower() + ".wav"
            snd989.write_wav(os.path.join(out, name), rate, chans)
            print("%s -> %s (%d Hz, %d channels)" % (rel, name, rate, len(chans)))
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
            print("%s -> %d sounds as WAV (%s_sNNN.wav)" % (rel, n, stem))
        else:
            print("%s: no converter for this type; copied as is" % rel)
            shutil.copy2(src, os.path.join(out, os.path.basename(rel)))
        os.makedirs(final, exist_ok=True)
        kept = 0
        for n in sorted(os.listdir(out)):
            p, q = os.path.join(out, n), os.path.join(final, n)
            key = os.path.relpath(q, work).replace("\\", "/")
            old = state["files"].get(key, {}).get("sha1")
            if os.path.exists(q) and not n.endswith(".json") and sha1(q) != old:
                kept += 1   # edited after the previous export: keep the user's
                continue
            os.replace(p, q)
            if not n.endswith(".json"):
                ent = state["files"].setdefault(key, {})
                ent["sha1"] = sha1(q)
                sz = png_size(q)
                if sz:
                    ent["size"] = list(sz)   # size in the game (the GUI warns if the replacement changes the aspect ratio)
        shutil.rmtree(out, ignore_errors=True)
        if kept:
            print("  %d file(s) already edited in the work folder were kept" % kept)
        save_state(work, state)
    print("work folder: %s (edit there, then pack into a mod: modkit.py pack <mod>, or the Pack button)" % work)


def source_of(disc, rel):
    """The disc file a work folder file came from ("MON/lizard0_t0004.png" -> "MON/LIZARD0.TEX"); None if not found."""
    d, n = os.path.split(rel.replace("\\", "/"))
    if d.startswith("skins/"):   # skin PNG (skins/<id>/shogun3_t0010.png): came from the base costume in MON/
        d = "MON"
    pre = d + "/" if d else ""
    m = PNG_RE.match(n)
    stem, ext = os.path.splitext(n)
    if m:
        cands = [m.group(1).upper() + (".RTX" if m.group(2) else ".TEX")]
    elif ext.lower() == ".wav":
        mb = BNK_RE.match(n)
        cands = [mb.group(1).upper() + ".BNK"] if mb else [stem.upper() + ".VPK", stem.upper() + ".VAG"]
    else:
        cands = [n]
    for c in cands:
        if disc_file(disc, pre + c):
            return pre + c
    return None


def revert(disc, work, rels):
    """Restores work folder files to the original: re-extracts the disc file into a temporary folder and copies only those."""
    state = load_state(work)
    by_src = {}
    for rel in rels:
        rel = rel.replace("\\", "/")
        src = source_of(disc, rel)
        if not src:
            print("%s: source file not found on the disc" % rel)
            continue
        by_src.setdefault(src, []).append(rel)
    tmp = os.path.join(work, ".revert_tmp")
    n = 0
    for src, items in sorted(by_src.items()):
        shutil.rmtree(tmp, ignore_errors=True)
        with contextlib.redirect_stdout(io.StringIO()):
            export(disc, tmp, [src])
        for rel in items:
            fresh = os.path.join(tmp, *(os.path.dirname(src).split("/") + [os.path.basename(rel)]))
            dst = os.path.join(work, *rel.split("/"))
            if not os.path.exists(fresh):
                print("%s: not produced by a new export of %s" % (rel, src))
                continue
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(fresh, dst)
            ent = state["files"].setdefault(rel, {})
            ent["sha1"] = sha1(dst)
            sz = png_size(dst)
            if sz:
                ent["size"] = list(sz)
            print("reverted %s (from %s)" % (rel, src))
            n += 1
    shutil.rmtree(tmp, ignore_errors=True)
    save_state(work, state)
    print("%d file(s) back to the original" % n)
    return n


def png_size(p):
    """(width, height) from a PNG's IHDR header; None if it is not a PNG."""
    try:
        with open(p, "rb") as f:
            h = f.read(24)
    except OSError:
        return None
    if len(h) < 24 or h[:8] != b"\x89PNG\r\n\x1a\n" or h[12:16] != b"IHDR":
        return None
    return struct.unpack(">II", h[16:24])


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
    """Takes what changed in the work folder (work None = work_dir(mods, mod)) into mods/<mod>/."""
    validate_mod_name(mod, "pack")
    if work is None:
        work = work_dir(mods, mod)
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
        print("nothing changed in %s since the export" % work)
        return
    banks = {}
    for rel in sorted(changed):
        p = os.path.join(work, *rel.split("/"))
        d, n = os.path.split(rel)
        m = PNG_RE.match(n)
        if m:
            stem, rtx, tid = m.group(1), bool(m.group(2)), int(m.group(3))
            os.makedirs(os.path.join(dst, *d.split("/")) if d else dst, exist_ok=True)
            name = "%s%s_t%04d.png" % (stem, "_rtx" if rtx else "", tid)   # without _sempaleta: the name the game looks for
            out_rel = (d + "/" if d else "") + name
            shutil.copy2(p, os.path.join(dst, *out_rel.split("/")))
            print("texture %s -> %s/%s" % (n, mod, out_rel))
            if inject and not d.startswith("skins/"):   # a skin PNG applies only to that skin: nothing to rewrite in the disc .TEX
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
                    print("sound %s -> %s/%s" % (n, mod, os.path.relpath(out, dst).replace("\\", "/")))
                    break
            else:
                print("WARNING: %s does not match any .VPK/.VAG/.BNK on the disc; skipped" % rel)
            continue
        out = os.path.join(dst, *rel.split("/"))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copy2(p, out)
        print("file %s -> %s/%s (as is)" % (rel, mod, rel))
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
        print("bank %s.BNK: %d sound(s) replaced -> %s" % (stem.upper(), len(sounds), os.path.relpath(out, mods)))
    print("mod in %s" % dst)


# ---------------------------------------------------------------- mod skins (MODS.md, "Skins")

MONSTERS = {"congar": "MONKEY", "robo-47": "ROBOT", "togera": "LIZARD", "ultra-v": "SHOGUN", "preytor": "MANTIS",
            "kineticlops": "ENERGY", "magmo": "LAVA", "raptros": "DRAGON", "agamo": "ROCK", "zorgulon": "ALIEN"}


# Bosses a custom monster can use as its base (kCustomBases in native/wotm_mods.hpp)
CUSTOM_BASES = {"assboss": "ASSBOSS"}


def norm(s):
    return "".join(c for c in s.lower() if c.isalnum())


def game_monster(s):
    """The game's name ("togera") of a playable monster, given its name or prefix ("LIZARD", "Robo-47"); None if it is not one."""
    return next((g for g, p in MONSTERS.items() if norm(s) in (norm(g), norm(p))), None)


def base_prefix(s):
    """MON/ prefix of a custom monster's base (playables and kCustomBases); None if the port does not accept that base."""
    g = game_monster(s)
    if g:
        return MONSTERS[g]
    return next((p for g, p in CUSTOM_BASES.items() if norm(s) in (norm(g), norm(p))), None)


def mod_folders(mods):
    """{name: folder} of the enabled mods: the repo's (REPO/mods, mirrored by sync_mods) and the mods folder's, which wins on the same name."""
    out = {}
    for root in (os.path.join(REPO, "mods"), mods):
        if not root or not os.path.isdir(root):
            continue
        for n in os.listdir(root):
            p = os.path.join(root, n)
            if os.path.isdir(p) and n[0] not in "_.":
                out[n] = p
    return out


def find_customs(mods):
    """Custom monsters (mods/<mod>/monsters/<folder>/monster.json) of the enabled mods, by id: {id: {"id", "name", "base", "prefix",
    "costume", "mod", "path"}}. Same id in two mods: the last in alphabetical order wins, as in the loader."""
    by_id = {}
    for mod, folder in sorted(mod_folders(mods).items()):
        mdir = os.path.join(folder, "monsters")
        if not os.path.isdir(mdir):
            continue
        for sub in sorted(os.listdir(mdir)):
            js = os.path.join(mdir, sub, "monster.json")
            try:
                with open(js, encoding="utf-8") as f:
                    v = json.load(f)
            except (OSError, ValueError):
                continue
            if not isinstance(v, dict):
                continue
            pre = base_prefix(v.get("base")) if isinstance(v.get("base"), str) else None
            cid = (v.get("id") if isinstance(v.get("id"), str) else sub).lower()
            if not pre or not re.fullmatch(r"[a-z][a-z0-9_]{0,31}", cid):
                continue
            co = v.get("costume", 0)
            co = co if isinstance(co, int) and 0 <= co <= 3 else 0
            by_id[cid] = {"id": cid, "name": v.get("name") or cid, "base": v.get("base"), "prefix": pre, "costume": co,
                          "mod": mod, "path": js}
    return by_id


def list_monsters(mods):
    """Monsters for a skin: playables from the disc [(game_name, prefix, None)] and customs [(id, base_prefix, costume)]."""
    out = [(g, p, None) for g, p in MONSTERS.items()]
    out += [(c["id"], c["prefix"], c["costume"]) for c in sorted(find_customs(mods).values(), key=lambda c: c["id"])]
    return out


SKIN_ID_RE = re.compile(r"[a-z0-9_]{1,32}")
CUSTOM_ID_RE = re.compile(r"[a-z][a-z0-9_]{0,31}")


def skin_id_error(sid):
    """Message if the id is not valid for a skin; None if it is."""
    if not SKIN_ID_RE.fullmatch(sid or ""):
        return "the id takes only lowercase letters, digits and _ (1 to 32)"
    return None


def skin_slug(name):
    """Skin id from the name ("GREEN TOGERA" -> "green_togera")."""
    return re.sub(r"_+", "_", re.sub(r"[^a-z0-9_]", "_", (name or "").lower())).strip("_")[:32].strip("_")


def skin_dir(mods, mod, sid):
    return os.path.join(mods, mod, "skins", sid)


def list_skins(mods, mod):
    """{folder: skin.json} of the skins in mods/<mod>/skins/ (folder = the id from skin_new)."""
    d = os.path.join(mods, mod, "skins")
    out = {}
    for n in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        js = os.path.join(d, n, "skin.json")
        if os.path.isfile(js):
            out[n] = _json_obj(js)
    return out


def write_skin_json(mods, mod, sid, data):
    with open(os.path.join(skin_dir(mods, mod, sid), "skin.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def portrait_png(disc, prefix, costume, out_png):
    """Writes the costume's portrait to out_png (the last 128x64 texture of MON/<PREFIX><costume>.RTX), decoded the way export
    (tex.cmd_export) does, without printing anything to stdout (it runs on a GUI thread). False if it failed."""
    src = disc_file(disc, "MON/%s%d.RTX" % (prefix, costume))
    if not src:
        return False
    try:
        data, _ = tex.read_file(src)
        t = tex.parse(data)
        pks = [p for p in t["packets"] if not p.is_palette()]
        pks = [p for p in pks if (p.w, p.h) == (128, 64)] or pks
        if not pks:
            return False
        p = pks[-1]
        ngp = tex.sibling(src, ".NGP")
        pair = tex.Pairing(t, data, t, tex.ngp_texrefs(ngp) if ngp else [])
        off = p.data_off(True)
        if p.psm in (tex.T8, tex.T4, tex.T8H, tex.T4HL, tex.T4HH):
            pal, _cbp, csa, _ = pair.palette(p, hi=True)
            if pal is None:
                return False
            if p.psm in (tex.T4, tex.T4HL, tex.T4HH) and len(pal) < 256:
                csa = 0
            psm = p.psm if p.psm not in tex.HIGH else (tex.T8 if p.psm == tex.T8H else tex.T4)
            px = tex.decode(data, off, psm, p.w, p.h, pal, csa)
            fix = not tex.raw_hi(px)
        elif p.psm in (tex.CT32, tex.CT24, tex.CT16, tex.CT16S):
            px = tex.decode(data, off, p.psm, p.w, p.h, hi=True)
            fix = p.psm == tex.CT32 and not tex.raw_hi(px)
        else:
            return False
        if fix:
            px = [(r, g, b, tex.fix_alpha(a)) for (r, g, b, a) in px]
        tex.write_png(out_png, p.w, p.h, px)
        return True
    except Exception:
        return False


def skin_duplicate(mods, mod, work, sid, new):
    """Copies skin sid to new: its folder in the mod (skin.json with the new id), its work folder and the PNGs' state."""
    err = skin_id_error(new)
    if err:
        raise SystemExit("duplicate skin: " + err)
    src, dst = skin_dir(mods, mod, sid), skin_dir(mods, mod, new)
    if os.path.exists(dst) or os.path.exists(os.path.join(work, "skins", new)):
        raise SystemExit("duplicate skin: there is already a skin %r in the mod" % new)
    shutil.copytree(src, dst)
    data = _json_obj(os.path.join(dst, "skin.json"))
    data["id"] = new
    write_skin_json(mods, mod, new, data)
    wsrc = os.path.join(work, "skins", sid)
    if os.path.isdir(wsrc):
        shutil.copytree(wsrc, os.path.join(work, "skins", new))
    st = load_state(work)
    pre = "skins/%s/" % sid
    for k, v in list(st["files"].items()):
        if k.startswith(pre):
            st["files"]["skins/%s/%s" % (new, k[len(pre):])] = dict(v)
    save_state(work, st)
    print("skin %s copied to %s (%s)" % (sid, new, dst))


def skin_delete(mods, mod, work, sid):
    """Deletes the skin: its folder in the mod, its work folder and its keys in the state."""
    shutil.rmtree(skin_dir(mods, mod, sid), ignore_errors=True)
    shutil.rmtree(os.path.join(work, "skins", sid), ignore_errors=True)
    st = load_state(work)
    pre = "skins/%s/" % sid
    st["files"] = {k: v for k, v in st["files"].items() if not k.startswith(pre)}
    save_state(work, st)
    print("skin %s deleted" % sid)


def skin_adopt(work, d, stem, sid):
    """Moves the PNGs (and manifests) in <work>/<d>/ that came from <STEM>.TEX/.RTX into <work>/skins/<sid>/, carrying their
    state sha1 along (edited ones stay edited). The next pack leaves the original costume alone. Returns the PNG count moved."""
    src = os.path.join(work, *d.split("/"))
    dst = os.path.join(work, "skins", sid)
    os.makedirs(dst, exist_ok=True)
    st = load_state(work)
    n = 0
    mans = {(stem + "_manifest.json").lower(), (stem + "_rtx_manifest.json").lower()}
    for name in sorted(os.listdir(src)) if os.path.isdir(src) else []:
        m = PNG_RE.match(name)
        if not (m and m.group(1).lower() == stem.lower()) and name.lower() not in mans:
            continue
        os.replace(os.path.join(src, name), os.path.join(dst, name))
        ent = st["files"].pop("%s/%s" % (d, name), None)
        if ent is not None:
            st["files"]["skins/%s/%s" % (sid, name)] = ent
        n += bool(m)
    save_state(work, st)
    print("%d texture(s) of %s/%s moved to skins/%s" % (n, d, stem.upper(), sid))
    return n


def skin_new(disc, work, mods, mod, monster, sid, base=None, name=None, ai=False, textures=True):
    """Creates mods/<mod>/skins/<id>/skin.json and exports the base costume's textures into <work>/skins/<id>/; pack takes the
    edited PNGs into the skin's folder (they apply only to that skin). monster: a playable (name or prefix) or a custom monster's
    id; base None = costume 0 (playable) or the costume from monster.json (custom). work None = work_dir(mods, mod).
    textures=False: only skin.json (the GUI moves the PNGs of an already extracted costume into the skin, skin_adopt)."""
    validate_mod_name(mod, "skin new")
    if work is None:
        work = work_dir(mods, mod)
    if skin_id_error(sid):
        raise SystemExit("skin new: " + skin_id_error(sid))
    if base is not None and base not in range(4):
        raise SystemExit("skin new: --base is a disc costume, 0 to 3")
    game = game_monster(monster)
    custom = None
    if game:
        key, prefix, costume = game, MONSTERS[game], base if base is not None else 0
    else:
        key = monster.strip().lower()
        if not re.fullmatch(r"[a-z][a-z0-9_]{0,31}", key):
            raise SystemExit("skin new: unknown monster %r (one of: %s, or the id of a custom monster)" % (monster, ", ".join(MONSTERS)))
        custom = find_customs(mods).get(key)
        prefix = custom["prefix"] if custom else None
        costume = base if base is not None else (custom["costume"] if custom else None)
    folder = os.path.join(mods, mod, "skins", sid)
    os.makedirs(folder, exist_ok=True)
    js = os.path.join(folder, "skin.json")
    if os.path.exists(js):
        print("%s already exists; kept" % js)
    else:
        data = {"monster": key, "id": sid, "name": (name or "%s %s" % (sid, key)).upper()}
        if game or base is not None:   # custom monster skin without --base: the game uses the costume from monster.json
            data["base"] = costume
        data["ai"] = ai
        with open(js, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
            f.write("\n")
        print("skin %s/%s: %s" % (key, sid, js))
    if custom:
        print("custom monster %s (%s, mod %s): base %s, costume %d" % (key, custom["name"], custom["mod"], custom["base"], costume))
    elif not game:
        print("WARNING: no monster.json with id %r in the mods (%s, %s); skin.json created, but no textures exported "
              "(the base type is unknown)" % (key, mods, os.path.join(REPO, "mods")))
        return
    if not textures:
        return
    export(disc, work, ["MON/%s%d" % (prefix, costume)], dest="skins/" + sid)
    print("edit the PNGs in %s, then: modkit.py pack %s" % (os.path.join(work, "skins", sid), mod))


def find_disc_ext(disc, d, stem, ext):
    return disc_file(disc, ("%s/%s%s" % (d, stem, ext)) if d else stem + ext)


def inject_png(disc, dst, stem, rtx, tid, png):
    """Rewrites the texture into the mod's (or the disc's) .TEX/.RTX, same size: only for the emulated GS image."""
    ext = ".RTX" if rtx else ".TEX"
    rel = find_texfile(disc, stem, ext)
    if not rel:
        print("  --inject: %s%s not found on the disc" % (stem, ext))
        return
    out = os.path.join(dst, *rel.split("/"))
    src = out if os.path.exists(out) else disc_file(disc, rel)
    w, h, _ = tex.read_image(png)
    pk = next((p for p in tex.parse(tex.read_file(src)[0])["packets"] if p.texId == tid), None)
    if pk is None or (w, h) != (pk.w, pk.h):
        print("  --inject: %s skipped (needs %dx%d, the original size)" % (os.path.basename(png), pk.w if pk else 0,
                                                                                         pk.h if pk else 0))
        return
    os.makedirs(os.path.dirname(out), exist_ok=True)
    rtx_path = None if rtx else disc_file(disc, rel[:-4] + ".RTX")
    ngp = disc_file(disc, rel[:-4] + ".NGP")
    tex.cmd_inject(src, tid, png, out + ".tmp", rtx_path, None, ngp)
    os.replace(out + ".tmp", out)


# ---------------------------------------------------------------- check

def _json_obj(p):
    try:
        with open(p, encoding="utf-8") as f:
            v = json.load(f)
        return v if isinstance(v, dict) else {}
    except (OSError, ValueError):
        return {}


def conflict_key(mod_dir, rel):
    """Key for what the file replaces in the game, to find two mods replacing the same thing; None if it has no key of its own.
    PNG: (stem, rtx, texId); skin: monster/id (the whole skin folder counts as one); custom monster: id; anything else: the path."""
    parts = rel.split("/")
    n = parts[-1]
    if len(parts) >= 3 and parts[0].lower() == "skins":
        if len(parts) == 3 and n.lower() == "skin.json":
            v = _json_obj(os.path.join(mod_dir, *parts))
            mon = v.get("monster") if isinstance(v.get("monster"), str) else ""
            mon = game_monster(mon) or mon.lower()
            sid = (v.get("id") if isinstance(v.get("id"), str) else parts[1]).lower()
            return ("skin", "%s/%s" % (mon, sid))
        return None
    if len(parts) >= 3 and parts[0].lower() == "monsters":
        if len(parts) == 3 and n.lower() == "monster.json":
            v = _json_obj(os.path.join(mod_dir, *parts))
            return ("custom", (v.get("id") if isinstance(v.get("id"), str) else parts[1]).lower())
        return None
    m = PNG_RE.match(n)
    if m:
        return ("png", m.group(1).upper(), bool(m.group(2)), int(m.group(3)))
    if n.lower() == "modkit.json":
        return None
    return ("file", rel.upper())


def key_label(key):
    if key[0] == "png":
        return "texture %s%s t%d" % (key[1], ".RTX" if key[2] else ".TEX", key[3])
    if key[0] == "skin":
        return "skin %s" % key[1]
    if key[0] == "custom":
        return "custom monster %s" % key[1]
    return "file %s" % key[1]


def mod_files(mods, mod):
    """[(rel, path)] of the files in mods/<mod>/, in order."""
    out = []
    base = os.path.join(mods, mod)
    for root, dirs, files in os.walk(base):
        dirs.sort()
        for n in sorted(files):
            p = os.path.join(root, n)
            out.append((os.path.relpath(p, base).replace("\\", "/"), p))
    return out


def check(disc, mods, only=None):
    if not os.path.isdir(mods):
        raise SystemExit("no mods folder at %s" % mods)
    names = sorted(n for n in os.listdir(mods) if os.path.isdir(os.path.join(mods, n)) and n != WORK)
    if only and only not in names:
        raise SystemExit("check: no mod %r in %s" % (only, mods))
    owners = {}   # key -> enabled mods that replace it, in alphabetical order (the last wins)
    for mod in names:
        if mod[0] in "_.":
            continue
        for rel, _p in mod_files(mods, mod):
            key = conflict_key(os.path.join(mods, mod), rel)
            if key and mod not in owners.setdefault(key, []):
                owners[key].append(mod)
    customs = find_customs(mods)
    problems = 0
    for mod in names:
        if only and mod != only:
            continue
        off = mod[0] in "_."
        print("%s%s" % (mod, " (disabled: starts with '_' or '.')" if off else ""))
        if off and not only:
            continue
        for rel, p in mod_files(mods, mod):
            n = os.path.basename(p)
            key = conflict_key(os.path.join(mods, mod), rel)
            if n.lower() == "modkit.json":
                continue
            note = ""
            m = PNG_RE.match(n)
            if key and key[0] in ("skin", "custom"):
                note = key_label(key)
                mon = key[1].split("/")[0]
                if key[0] == "skin" and mon not in MONSTERS and mon not in customs:
                    note += " WARNING: %r is not a playable monster nor the id of a custom monster; the game skips it" % mon
            elif m:
                stem, rtx, tid = m.group(1), bool(m.group(2)), int(m.group(3))
                trel = find_texfile(disc, stem, ".RTX" if rtx else ".TEX")
                if not trel:
                    note, problems = "ERROR: %s%s is not on the disc" % (stem.upper(), ".RTX" if rtx else ".TEX"), problems + 1
                else:
                    modded = os.path.join(mods, mod, *trel.split("/"))
                    pk = next((q for q in tex.parse(tex.read_file(modded if os.path.exists(modded) else disc_file(disc, trel))[0])["packets"]
                               if q.texId == tid and not (rtx and q.is_palette())), None)
                    if pk is None:
                        note, problems = "ERROR: %s has no texId %d" % (trel, tid), problems + 1
                    else:
                        w, h, _ = tex.read_image(p) if n.lower().endswith(".png") else (0, 0, None)
                        ratio = w * pk.h == h * pk.w
                        note = "texture of %s, %dx%d (game %dx%d)%s" % (trel, w, h, pk.w, pk.h,
                                                                        "" if ratio else " WARNING: different aspect ratio, it will look stretched")
            else:
                orig = disc_file(disc, rel)
                size = os.path.getsize(p)
                if not orig:
                    note = "new file (not on the disc; nothing asks for it yet)" if key else "part of the %s" % rel.split("/")[0][:-1]
                else:
                    osz = os.path.getsize(orig)
                    note = "%d bytes (disc %d)" % (size, osz)
                    if size > osz and os.path.splitext(n)[1].upper() in (".TEX", ".RTX", ".NGP", ".BNK"):
                        note += " WARNING: larger than the disc file; the game loads into fixed regions and may overwrite what comes next"
            if not off and key:
                own = owners.get(key, [])
                i = own.index(mod)
                if i > 0:
                    note += " (overrides %s)" % own[i - 1]
                if i < len(own) - 1:
                    note += " (overridden by %s)" % own[-1]
            print("  %-40s %s" % (rel, note))
    clash = sorted((k, own) for k, own in owners.items() if len(own) > 1 and (not only or only in own))
    print()
    if clash:
        print("conflicts between enabled mods (%d; the last in alphabetical order wins, as the game loads them):" % len(clash))
        for k, own in clash:
            print("  %-40s %s -> %s wins" % (key_label(k), ", ".join(own), own[-1]))
    else:
        print("no conflicts between enabled mods")
    print("%d error(s)" % problems)
    return 1 if problems else 0


def main(argv):
    opts, args, i = {}, [], 0
    while i < len(argv):
        if argv[i] in ("--disc", "--mods", "--work", "--base", "--name"):
            opts[argv[i][2:]] = argv[i + 1]
            i += 2
        elif argv[i] in ("--inject", "--ai"):
            opts[argv[i][2:]] = True
            i += 1
        else:
            args.append(argv[i])
            i += 1
    if not args or args[0] not in ("export", "pack", "check", "revert", "skin"):
        print(__doc__)
        return 1
    disc = opts.get("disc") or default_disc()
    mods = opts.get("mods") or default_mods(disc)
    if not os.path.isdir(disc):
        raise SystemExit("extracted disc not found at %s (--disc or WOTM_DISC)" % disc)

    def work_of(cmd, mod):
        validate_mod_name(mod, cmd)
        w = opts.get("work") or work_dir(mods, mod)
        adopt_mod(disc, mods, mod, w)   # a mod made before it had a work folder: comes in as edited
        return w

    if args[0] == "export":
        if len(args) < 3:
            raise SystemExit("export <mod> <files...>: the mod and what to extract (e.g. export my_mod MON/LIZARD0)")
        export(disc, work_of("export", args[1]), args[2:])
    elif args[0] == "pack":
        if len(args) != 2:
            raise SystemExit("pack <mod>: one mod name (not starting with '_' or '.')")
        pack(disc, work_of("pack", args[1]), mods, args[1], opts.get("inject", False))
    elif args[0] == "skin":
        if len(args) != 5 or args[1] != "new":
            raise SystemExit("skin new <mod> <monster> <id> [--base N] [--name \"GREEN TOGERA\"] [--ai]")
        base = opts.get("base")
        if base is not None:
            try:
                base = int(base)
            except ValueError:
                raise SystemExit("skin new: --base is a disc costume, 0 to 3")
        skin_new(disc, work_of("skin new", args[2]), mods, args[2], args[3], args[4], base, opts.get("name"), opts.get("ai", False))
    elif args[0] == "revert":
        if len(args) < 3:
            raise SystemExit("revert <mod> <files...>: the mod and files of its work folder (e.g. revert my_mod MON/lizard0_t0004.png)")
        revert(disc, work_of("revert", args[1]), args[2:])
    else:
        if len(args) > 2:
            raise SystemExit("check [mod]: all mods, or one")
        return check(disc, mods, args[1] if len(args) > 1 else None)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
