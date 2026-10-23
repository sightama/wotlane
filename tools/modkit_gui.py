#!/usr/bin/env python3
"""Interface do kit de mods (tools/modkit.py): escolher no disco o que extrair, ver e abrir o que foi extraido, empacotar no mod.

  python tools/modkit_gui.py        (ou pythonw, sem console; o launcher tem o botao "Mod kit")

Textos da janela em ingles (pedido do dono). Esquerda: o disco por nome (monstros e trajes, fases, menu, bancos de som, musicas);
duplo clique ou "Extract selected" manda para a pasta de trabalho (<mods>/_work). Direita: a pasta de trabalho com miniaturas;
"edited" = mudou desde a extracao (vai no proximo pacote). Duplo clique abre no programa padrao do Windows. Embaixo: o nome do mod,
"Pack edited files into mod" (modkit pack) e "Check mods" (modkit check). As pastas ficam em ~/.wotm_modkit.json. Precisa do
Pillow (pip install pillow) para as miniaturas.
"""
import io
import json
import os
import queue
import subprocess
import sys
import threading
import traceback
import wave
from contextlib import redirect_stderr, redirect_stdout

import tkinter as tk
from tkinter import filedialog, messagebox, ttk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modkit  # noqa: E402

try:
    from PIL import Image, ImageTk
except ImportError:   # sem Pillow: sem miniaturas nem previa de imagem
    Image = ImageTk = None

CONF = os.path.join(os.path.expanduser("~"), ".wotm_modkit.json")

# Monstros: prefixo do arquivo (MON/<PREFIXO><traje>) -> nome no jogo, bancos de som (GLOSSARIO.md)
MONSTERS = [
    ("MONKEY", "Congar", ("APE_S", "APE_LS")), ("ROBOT", "Robo-47", ("ROBOT_S", "ROBOT_LS")),
    ("LIZARD", "Togera", ("LIZARD_S", "LIZRD_LS")), ("SHOGUN", "Ultra V", ("SHOGUN_S", "SHOGN_LS")),
    ("MANTIS", "Preytor", ("MANTIS_S", "MANTS_LS")), ("ENERGY", "Kineticlops", ("ENERGY_S", "ENERG_LS")),
    ("LAVA", "Magmo", ("LAVA_S", "LAVA_LS")), ("DRAGON", "Raptros", ("DRAGON_S", "DRAGN_LS")),
    ("ROCK", "Agamo", ("ROKMAN_S", "ROKMN_LS")), ("ALIEN", "Zorgulon", ("ALIEN_S", "ALIEN_LS")),
]
BOSSES = [("ASSBOSS", "Goliath Prime", ("ASSEMB_S",)), ("JELLY", "Jelly (final boss)", ()), ("FINAL", "Final boss", ("FINAL_S",))]
LEVELS = [("CENTRAL", "Midtown Park"), ("VEGAS", "Gamblers Gulch"), ("CANYON2", "Rosdale Canyon"), ("METRO", "Metro City"),
          ("AIRPORT", "Century Airfield"), ("3-MILE", "Atomic Island"), ("SANFRAN", "Baytown"), ("ISLAND", "Club Caldera"),
          ("TOKYO", "Tsunopolis"), ("UFO", "UFO"), ("CAPITOL", "Capitol"), ("TRAINING", "Training"), ("CANYON1", "Lake Powell"),
          ("MINISF", "Mini Baytown"), ("VOLCANO", "Volcano"), ("TOKYODM", "Tokyo Deathmatch"), ("DBALL", "Dodge Ball"),
          ("BIGSHOT", "Big Shot"), ("CRUSH", "Crush")]
SHELL = [("SHELL", "Main menu"), ("SHELL2", "Menu (2)"), ("UI", "Interface"), ("LOAD", "Loading screen"),
         ("PRESHELL", "Before the menu")]

BG, BG2, FG, DIM, ACC, EDIT = "#1e1e1e", "#2b2b2b", "#e6e6e6", "#9a9a9a", "#2f6fd6", "#f0b030"


class QueueWriter(io.TextIOBase):
    def __init__(self, q):
        self.q = q

    def write(self, s):
        if s:
            self.q.put(s)
        return len(s)


class App:
    def __init__(self, root):
        self.root = root
        root.title("WotM - Mod kit")
        root.geometry("1280x800")
        root.minsize(900, 560)
        root.configure(bg=BG)
        self.conf = self.load_conf()
        self.disc = tk.StringVar(value=self.conf.get("disc") or modkit.default_disc())
        self.mods = tk.StringVar(value=self.conf.get("mods") or modkit.default_mods(self.disc.get()))
        self.mod = tk.StringVar(value=self.conf.get("mod", "my_mod"))
        self.only_edited = tk.BooleanVar(value=False)
        self.filter = tk.StringVar()
        self.log_q = queue.Queue()
        self.busy = False
        self.thumbs = {}      # caminho -> (mtime, PhotoImage)
        self.preview_img = None
        self.style()
        self.build()
        self.fill_disc()
        self.refresh_work()
        self.refresh_mods()
        root.after(100, self.pump_log)
        root.protocol("WM_DELETE_WINDOW", self.close)

    # ------------------------------------------------------------ aparencia
    def style(self):
        s = ttk.Style(self.root)
        s.theme_use("clam")
        s.configure(".", background=BG, foreground=FG, fieldbackground=BG2, bordercolor="#444", lightcolor=BG2, darkcolor=BG2,
                    troughcolor=BG2, arrowcolor=FG, insertcolor=FG)
        s.configure("TButton", background="#3a3a3a", padding=(10, 4))
        s.map("TButton", background=[("active", "#4a4a4a"), ("disabled", "#2a2a2a")], foreground=[("disabled", DIM)])
        s.configure("Accent.TButton", background=ACC)
        s.map("Accent.TButton", background=[("active", "#3f80e8"), ("disabled", "#2a2a2a")])
        s.configure("Treeview", background=BG2, fieldbackground=BG2, foreground=FG, rowheight=22, borderwidth=0)
        s.configure("Work.Treeview", rowheight=44)
        s.map("Treeview", background=[("selected", ACC)], foreground=[("selected", "white")])
        s.configure("Treeview.Heading", background="#333", foreground=FG, relief="flat")
        s.map("Treeview.Heading", background=[("active", "#3d3d3d")])
        s.configure("TEntry", fieldbackground=BG2, foreground=FG)
        s.configure("TCombobox", fieldbackground=BG2, foreground=FG, background="#3a3a3a")
        s.map("TCombobox", fieldbackground=[("readonly", BG2)], foreground=[("readonly", FG)])
        s.configure("TCheckbutton", background=BG, foreground=FG)
        s.map("TCheckbutton", background=[("active", BG)])
        s.configure("Dim.TLabel", foreground=DIM)
        s.configure("Head.TLabel", font=("Segoe UI", 11, "bold"))
        self.root.option_add("*TCombobox*Listbox.background", BG2)
        self.root.option_add("*TCombobox*Listbox.foreground", FG)

    # ------------------------------------------------------------ montagem
    def build(self):
        top = ttk.Frame(self.root, padding=(10, 8, 10, 4))
        top.pack(fill="x")
        for r, (label, var) in enumerate((("Extracted disc", self.disc), ("Mods folder", self.mods))):
            ttk.Label(top, text=label).grid(row=r, column=0, sticky="w", padx=(0, 8), pady=2)
            ttk.Entry(top, textvariable=var).grid(row=r, column=1, sticky="ew", pady=2)
            ttk.Button(top, text="...", width=3, command=lambda v=var: self.pick_dir(v)).grid(row=r, column=2, padx=(4, 0))
        top.columnconfigure(1, weight=1)
        self.disc.trace_add("write", lambda *_: self.on_paths())
        self.mods.trace_add("write", lambda *_: self.on_paths())

        panes = ttk.PanedWindow(self.root, orient="horizontal")
        panes.pack(fill="both", expand=True, padx=10, pady=4)

        # esquerda: disco
        left = ttk.Frame(panes)
        panes.add(left, weight=3)
        ttk.Label(left, text="1. Pick from the disc", style="Head.TLabel").pack(anchor="w")
        f = ttk.Frame(left)
        f.pack(fill="x", pady=(4, 4))
        ttk.Label(f, text="Search").pack(side="left")
        e = ttk.Entry(f, textvariable=self.filter)
        e.pack(side="left", fill="x", expand=True, padx=(6, 0))
        self.filter.trace_add("write", lambda *_: self.fill_disc())
        ttk.Label(left, text="Ctrl or Shift to pick several; double-click extracts", style="Dim.TLabel", wraplength=230).pack(anchor="w", pady=(0, 2))
        self.disc_tree = ttk.Treeview(left, show="tree", selectmode="extended")
        self.disc_tree.pack(fill="both", expand=True)
        self.disc_tree.bind("<Double-1>", lambda e: self.export_selected())
        b = ttk.Frame(left)
        b.pack(fill="x", pady=(6, 0))
        self.btn_export = ttk.Button(b, text="Extract selected  >>", style="Accent.TButton", command=self.export_selected)
        self.btn_export.pack(side="right")

        # direita: pasta de trabalho + previa
        right = ttk.Frame(panes)
        panes.add(right, weight=5)
        h = ttk.Frame(right)
        h.pack(fill="x")
        ttk.Label(h, text="2. Edit in the work folder", style="Head.TLabel").pack(side="left")
        ttk.Checkbutton(h, text="edited only", variable=self.only_edited, command=self.refresh_work).pack(side="right")
        ttk.Button(h, text="Refresh", command=self.refresh_work).pack(side="right", padx=6)
        body = ttk.PanedWindow(right, orient="horizontal")
        body.pack(fill="both", expand=True, pady=(4, 0))
        wf = ttk.Frame(body)
        body.add(wf, weight=3)
        self.work_tree = ttk.Treeview(wf, style="Work.Treeview", columns=("info", "state"), selectmode="extended")
        self.work_tree.heading("#0", text="File")
        self.work_tree.heading("info", text="Size")
        self.work_tree.heading("state", text="State")
        self.work_tree.column("#0", width=300)
        self.work_tree.column("info", width=110, anchor="center")
        self.work_tree.column("state", width=80, anchor="center")
        self.work_tree.tag_configure("edited", foreground=EDIT)
        sb = ttk.Scrollbar(wf, orient="vertical", command=self.work_tree.yview)
        self.work_tree.configure(yscrollcommand=sb.set)
        self.work_tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        self.work_tree.bind("<<TreeviewSelect>>", lambda e: self.show_preview())
        self.work_tree.bind("<Double-1>", lambda e: self.open_selected())

        pv = ttk.Frame(body, padding=(8, 0, 0, 0))
        body.add(pv, weight=2)
        self.canvas = tk.Canvas(pv, bg=BG2, highlightthickness=0, height=300)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda e: self.show_preview())
        self.pv_label = ttk.Label(pv, text="", style="Dim.TLabel", wraplength=360, justify="left")
        self.pv_label.pack(fill="x", pady=4)
        pb = ttk.Frame(pv)
        pb.pack(fill="x")
        ttk.Button(pb, text="Open in editor", command=self.open_selected).pack(side="left")
        ttk.Button(pb, text="Show in folder", command=self.reveal_selected).pack(side="left", padx=4)
        self.btn_play = ttk.Button(pb, text="Play", command=self.play_selected)
        self.btn_play.pack(side="left")
        ttk.Button(pb, text="Stop", command=self.stop_sound).pack(side="left", padx=4)
        ttk.Button(pb, text="Remove", command=self.remove_selected).pack(side="right")

        # embaixo: mod + log
        bot = ttk.Frame(self.root, padding=(10, 4, 10, 8))
        bot.pack(fill="x")
        ttk.Label(bot, text="3. Mod", style="Head.TLabel").pack(side="left")
        self.mod_box = ttk.Combobox(bot, textvariable=self.mod, width=28)
        self.mod_box.pack(side="left", padx=8)
        self.btn_pack = ttk.Button(bot, text="Pack edited files into mod", style="Accent.TButton", command=self.pack)
        self.btn_pack.pack(side="left")
        self.btn_check = ttk.Button(bot, text="Check mods", command=self.check)
        self.btn_check.pack(side="left", padx=6)
        ttk.Button(bot, text="Open mods folder", command=lambda: self.open_path(self.mods.get())).pack(side="left")
        self.status = ttk.Label(bot, text="", style="Dim.TLabel")
        self.status.pack(side="right")

        lf = ttk.Frame(self.root, padding=(10, 0, 10, 10))
        lf.pack(fill="x")
        self.log = tk.Text(lf, height=9, bg="#141414", fg="#cfcfcf", insertbackground=FG, relief="flat", font=("Consolas", 9),
                           wrap="none")
        self.log.pack(fill="x")
        self.log.tag_configure("warn", foreground=EDIT)
        self.log.tag_configure("err", foreground="#ff6b6b")
        self.say("Mod kit. Extract something from the disc, edit it in the work folder and pack it into a mod. No game files go into the repo.\n")

    # ------------------------------------------------------------ caminhos
    def work(self):
        return os.path.join(self.mods.get(), "_work")

    def pick_dir(self, var):
        d = filedialog.askdirectory(initialdir=var.get() or os.getcwd(), parent=self.root)
        if d:
            var.set(os.path.normpath(d))

    def on_paths(self):
        if getattr(self, "_paths_job", None):
            self.root.after_cancel(self._paths_job)
        self._paths_job = self.root.after(400, lambda: (self.fill_disc(), self.refresh_work(), self.refresh_mods()))

    def load_conf(self):
        try:
            with open(CONF, encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            return {}

    def close(self):
        try:
            with open(CONF, "w", encoding="utf-8") as f:
                json.dump({"disc": self.disc.get(), "mods": self.mods.get(), "mod": self.mod.get()}, f, indent=1)
        except OSError:
            pass
        self.stop_sound()
        self.root.destroy()

    # ------------------------------------------------------------ disco
    def fill_disc(self):
        t = self.disc_tree
        t.delete(*t.get_children())
        disc = self.disc.get()
        if not os.path.isdir(disc):
            t.insert("", "end", text="(extracted disc not found: pick the folder above)")
            return
        q = self.filter.get().strip().lower()

        def has(rel):
            return modkit.disc_file(disc, rel) is not None

        def add(parent, text, pats, open_=False):
            if q and q not in text.lower() and not any(q in p.lower() for p in pats):
                return None
            return t.insert(parent, "end", text=text, values=(json.dumps(pats),), open=open_)

        def group(text):
            return t.insert("", "end", text=text, open=bool(q))

        g = group("Monsters")
        for prefix, name, banks in MONSTERS:
            items = [("Costume %d  (%s%d)" % (k + 1, prefix, k), ["MON/%s%d" % (prefix, k)]) for k in range(4) if has("MON/%s%d.TEX" % (prefix, k))]
            items += [("Sounds  (%s.BNK)" % b, ["SND/%s.BNK" % b]) for b in banks if has("SND/%s.BNK" % b)]
            self.add_node(t, g, name, items, q, add)
        g = group("Bosses")
        for prefix, name, banks in BOSSES:
            items = [("Form %d  (%s%d)" % (k + 1, prefix, k), ["MON/%s%d" % (prefix, k)]) for k in range(4) if has("MON/%s%d.TEX" % (prefix, k))]
            items += [("Sounds  (%s.BNK)" % b, ["SND/%s.BNK" % b]) for b in banks if has("SND/%s.BNK" % b)]
            self.add_node(t, g, name, items, q, add)
        g = group("Levels")
        for f, name in LEVELS:
            if has("LVL/%s.TEX" % f):
                add(g, "%s  (%s)" % (name, f), ["LVL/%s" % f])
        g = group("Menus and screens")
        for f, name in SHELL:
            if has("SHELL/%s.TEX" % f):
                add(g, "%s  (%s)" % (name, f), ["SHELL/%s" % f])
        used = {b for _, _, bs in MONSTERS + BOSSES for b in bs}
        g = group("Sound banks")
        sd = modkit.disc_file(disc, "SND")
        for n in sorted(os.listdir(sd)) if sd else []:
            if n.upper().endswith(".BNK") and os.path.splitext(n)[0].upper() not in used:
                add(g, n.upper(), ["SND/" + n])
        g = group("Music and voices")
        vd = modkit.disc_file(disc, "VAG")
        for n in sorted(os.listdir(vd)) if vd else []:
            if n.upper().endswith((".VPK", ".VAG")):
                add(g, n.upper(), ["VAG/" + n])
        for g in t.get_children():
            if not t.get_children(g):
                t.delete(g)

    @staticmethod
    def add_node(t, g, name, items, q, add):
        if not items:
            return
        if q and q not in name.lower() and not any(q in txt.lower() or any(q in p.lower() for p in ps) for txt, ps in items):
            return
        node = t.insert(g, "end", text=name, values=(json.dumps([p for _, ps in items for p in ps]),), open=bool(q))
        for txt, ps in items:
            t.insert(node, "end", text=txt, values=(json.dumps(ps),))

    def selected_pats(self):
        pats = []
        for iid in self.disc_tree.selection():
            v = self.disc_tree.item(iid, "values")
            if v:
                pats += json.loads(v[0])
        return list(dict.fromkeys(pats))

    def export_selected(self):
        pats = self.selected_pats()
        if not pats:
            self.say("Pick a monster, costume, level or sound in the list on the left.\n", "warn")
            return
        n = len(pats)
        if n > 12 and not messagebox.askyesno("Extract", "Extract %d files? This can take a few minutes." % n, parent=self.root):
            return
        self.run("Extracting %d file(s)..." % n, lambda: modkit.export(self.disc.get(), self.work(), pats),
                 lambda: self.refresh_work(focus=pats))

    # ------------------------------------------------------------ pasta de trabalho
    # Grupo da pasta de trabalho = arquivo do disco de onde o item saiu ("MON/MONKEY0": .TEX e .RTX juntos; "SND/SHELL_S.BNK")
    @staticmethod
    def group_of(rel):
        d, n = os.path.split(rel)
        pre = d + "/" if d else ""
        m = modkit.PNG_RE.match(n)
        if m:
            return pre + m.group(1).upper()
        mb = modkit.BNK_RE.match(n)
        if mb:
            return pre + mb.group(1).upper() + ".BNK"
        return pre + os.path.splitext(n)[0].upper()

    @staticmethod
    def group_of_pat(pat):
        stem, ext = os.path.splitext(pat.replace("\\", "/"))
        d, n = os.path.split(stem)
        return (d + "/" if d else "") + n.upper() + (".BNK" if ext.upper() == ".BNK" else "")

    def group_label(self, key):
        n = key.split("/")[-1]
        for prefix, name, banks in MONSTERS + BOSSES:
            if n[:-1] == prefix and n[-1:].isdigit():
                return "%s  (%s, %s %d)" % (key, name, "costume" if (prefix, name, banks) in MONSTERS else "form", int(n[-1]) + 1)
            if n.endswith(".BNK") and n[:-4] in banks:
                return "%s  (%s sounds)" % (key, name)
        for f, name in LEVELS + SHELL:
            if n == f:
                return "%s  (%s)" % (key, name)
        return key

    def refresh_work(self, focus=None):
        """focus: padroes do export que acabou de rodar; os grupos deles abrem (os outros fecham) e a lista rola ate o primeiro."""
        t = self.work_tree
        sel = set(t.selection())
        was_open = {i for i in t.get_children() if t.item(i, "open")}
        t.delete(*t.get_children())
        work = self.work()
        if not os.path.isdir(work):
            self.status.configure(text="work folder is empty")
            return
        state = modkit.load_state(work)["files"]
        groups = {}   # chave -> [(rel, p, edited)]
        edited_total = total = 0
        for root, ds, files in os.walk(work):
            ds[:] = sorted(d for d in ds if not d.startswith("."))
            for n in sorted(files):
                if n.endswith(".json") or n.startswith("."):
                    continue
                p = os.path.join(root, n)
                rel = os.path.relpath(p, work).replace("\\", "/")
                edited = state.get(rel, {}).get("sha1") != modkit.sha1(p)
                total += 1
                edited_total += edited
                if self.only_edited.get() and not edited:
                    continue
                groups.setdefault(self.group_of(rel), []).append((rel, p, edited))
        want = {self.group_of_pat(p) for p in focus} if focus else None
        first_new = None
        for key in sorted(groups):
            items = groups[key]
            gid = "grp:" + key
            if want is not None:
                is_open = key in want
            elif was_open or sel:
                is_open = gid in was_open
            else:
                is_open = len(groups) == 1
            ne = sum(1 for _, _, e in items if e)
            t.insert("", "end", iid=gid, text="%s   [%d file(s)%s]" % (self.group_label(key), len(items), ", %d edited" % ne if ne else ""),
                     open=is_open, tags=("edited",) if ne else ())
            for rel, p, edited in items:
                img = self.thumb(p)
                kw = {"image": img} if img else {}
                t.insert(gid, "end", iid=rel, text=" " + os.path.basename(rel), values=(self.info(p), "edited" if edited else "original"),
                         tags=("edited",) if edited else (), **kw)
            if want is not None and key in want and first_new is None:
                first_new = gid
        if first_new:   # seleciona a primeira textura, nao o grupo (um Remove logo depois nao pega o grupo inteiro)
            kids = t.get_children(first_new)
            t.selection_set([kids[0]] if kids else [first_new])
            if kids:
                t.see(kids[min(len(kids) - 1, 3)])
            t.see(first_new)
        else:
            keep = [i for i in sel if t.exists(i)]
            if keep:
                t.selection_set(keep)
                t.see(keep[0])
        self.status.configure(text="%d file(s), %d edited" % (total, edited_total))

    def thumb(self, p):
        if Image is None or not p.lower().endswith(".png"):
            return None
        mt = os.path.getmtime(p)
        c = self.thumbs.get(p)
        if c and c[0] == mt:
            return c[1]
        try:
            im = Image.open(p).convert("RGBA")
            im.thumbnail((40, 40))
            bg = Image.new("RGBA", (40, 40), BG2)
            bg.alpha_composite(im, ((40 - im.width) // 2, (40 - im.height) // 2))
            ph = ImageTk.PhotoImage(bg)
        except Exception:
            return None
        self.thumbs[p] = (mt, ph)
        return ph

    @staticmethod
    def info(p):
        n = p.lower()
        try:
            if n.endswith(".png") and Image is not None:
                with Image.open(p) as im:
                    return "%dx%d" % im.size
            if n.endswith(".wav"):
                with wave.open(p) as w:
                    return "%.1f s" % (w.getnframes() / float(w.getframerate()))
        except Exception:
            pass
        return "%d KB" % max(1, os.path.getsize(p) // 1024)

    def selected_files(self):
        t, out = self.work_tree, []
        for i in t.selection():   # um grupo selecionado vale por todos os arquivos dele
            for r in (t.get_children(i) if i.startswith("grp:") else (i,)):
                p = os.path.join(self.work(), *r.split("/"))
                if p not in out:
                    out.append(p)
        return out

    def show_preview(self):
        c = self.canvas
        c.delete("all")
        files = self.selected_files()
        if not files:
            self.pv_label.configure(text="Select a file in the work folder. Double-click opens it in the default Windows editor.")
            return
        p = files[0]
        w, h = max(c.winfo_width(), 50), max(c.winfo_height(), 50)
        text = os.path.basename(p)
        if p.lower().endswith(".png") and Image is not None:
            try:
                im = Image.open(p).convert("RGBA")
                text += "  %dx%d" % im.size
                s = min((w - 16) / im.width, (h - 16) / im.height)
                s = s if s < 1 else max(1, int(s))   # amplia em inteiros (pixels nitidos)
                im2 = im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.NEAREST if s >= 1 else Image.LANCZOS)
                chk = Image.new("RGBA", im2.size, "#3a3a3a")   # xadrez atras do alfa
                light = Image.new("RGBA", (8, 8), "#4a4a4a")
                for y in range(0, im2.height, 16):
                    for x in range(0, im2.width, 16):
                        chk.paste(light, (x, y))
                        chk.paste(light, (x + 8, y + 8))
                chk.alpha_composite(im2)
                self.preview_img = ImageTk.PhotoImage(chk)
                c.create_image(w // 2, h // 2, image=self.preview_img)
            except Exception as ex:
                text += "  (could not open: %s)" % ex
            m = modkit.PNG_RE.match(os.path.basename(p))
            if m:
                text += "\nTexture %d of %s%s. Save it at any resolution, keeping the aspect ratio." % (
                    int(m.group(3)), m.group(1).upper(), ".RTX" if m.group(2) else ".TEX")
        elif p.lower().endswith(".wav"):
            text += "  " + self.info(p)
            c.create_text(w // 2, h // 2, text="sound", fill=DIM, font=("Segoe UI", 20))
            mb = modkit.BNK_RE.match(os.path.basename(p))
            text += "\n%s" % ("Sound %d of bank %s.BNK" % (int(mb.group(2)), mb.group(1).upper()) if mb else "Music or voice (.VPK/.VAG)")
        if len(files) > 1:
            text += "\n(%d selected)" % len(files)
        self.pv_label.configure(text=text)

    def open_selected(self):
        for p in self.selected_files()[:8]:
            self.open_path(p)

    def reveal_selected(self):
        files = self.selected_files()
        if files:
            subprocess.Popen(["explorer", "/select,", os.path.normpath(files[0])])
        else:
            self.open_path(self.work())

    @staticmethod
    def open_path(p):
        if not os.path.exists(p) and not os.path.splitext(p)[1]:
            os.makedirs(p, exist_ok=True)   # pasta de mods ou de trabalho ainda nao criada
        try:
            os.startfile(os.path.normpath(p))
        except (OSError, AttributeError) as ex:
            messagebox.showerror("Open", str(ex))

    def play_selected(self):
        wavs = [p for p in self.selected_files() if p.lower().endswith(".wav")]
        if not wavs:
            return
        try:
            import winsound
            winsound.PlaySound(wavs[0], winsound.SND_FILENAME | winsound.SND_ASYNC)
        except Exception as ex:
            self.say("Could not play: %s\n" % ex, "err")

    @staticmethod
    def stop_sound():
        try:
            import winsound
            winsound.PlaySound(None, 0)
        except Exception:
            pass

    def remove_selected(self):
        files = self.selected_files()
        if not files or not messagebox.askyesno("Remove", "Delete %d file(s) from the work folder?\n(the disc and the mods do not change)"
                                                % len(files), parent=self.root):
            return
        self.stop_sound()
        work = self.work()
        st = modkit.load_state(work)
        for p in files:
            try:
                os.remove(p)
            except OSError as ex:
                self.say("%s\n" % ex, "err")
            st["files"].pop(os.path.relpath(p, work).replace("\\", "/"), None)
        modkit.save_state(work, st)
        self.refresh_work()

    # ------------------------------------------------------------ mod
    def refresh_mods(self):
        m = self.mods.get()
        names = sorted(n for n in os.listdir(m) if os.path.isdir(os.path.join(m, n)) and n[0] not in "_.") if os.path.isdir(m) else []
        self.mod_box["values"] = names

    def pack(self):
        mod = self.mod.get().strip()
        if not mod or mod[0] in "_." or any(ch in mod for ch in '\\/:*?"<>|'):
            messagebox.showwarning("Mod", "Give the mod a name (letters, digits, '_' or '-'; not starting with '_' or '.').", parent=self.root)
            return
        self.run("Packing into %s..." % mod, lambda: modkit.pack(self.disc.get(), self.work(), self.mods.get(), mod),
                 lambda: (self.refresh_mods(), self.say("Done. Start the game: the log shows a [mods] line for each file and texture of the mod.\n")))

    def check(self):
        self.run("Checking...", lambda: modkit.check(self.disc.get(), self.mods.get()))

    # ------------------------------------------------------------ execucao em segundo plano
    def run(self, title, fn, done=None):
        if self.busy:
            self.say("Still working on the previous task; try again when it finishes.\n", "warn")
            return
        self.busy = True
        for b in (self.btn_export, self.btn_pack, self.btn_check):
            b.state(["disabled"])
        self.say("\n== %s\n" % title)
        w = QueueWriter(self.log_q)

        def body():
            try:
                with redirect_stdout(w), redirect_stderr(w):
                    fn()
            except SystemExit as ex:
                self.log_q.put(("err", "%s\n" % ex))
            except Exception:
                self.log_q.put(("err", traceback.format_exc()))
            self.log_q.put(("done", done))

        threading.Thread(target=body, daemon=True).start()

    def pump_log(self):
        try:
            while True:
                item = self.log_q.get_nowait()
                if isinstance(item, tuple):
                    kind, val = item
                    if kind == "done":
                        self.busy = False
                        for b in (self.btn_export, self.btn_pack, self.btn_check):
                            b.state(["!disabled"])
                        if val:
                            val()
                        continue
                    self.say(val, "err")
                else:
                    self.say(item, "warn" if ("WARNING" in item or "ERROR" in item) else None)
        except queue.Empty:
            pass
        self.root.after(100, self.pump_log)

    def say(self, s, tag=None):
        self.log.insert("end", s, tag or ())
        self.log.see("end")


def main():
    root = tk.Tk()
    try:
        ico = os.path.join(modkit.REPO, "native", "wotm.ico")
        if os.path.exists(ico):
            root.iconbitmap(ico)
    except tk.TclError:
        pass
    app = App(root)
    # com pythonw nao ha console: erro dentro da janela vai para o log dela
    root.report_callback_exception = lambda *a: app.say("Error: %s\n" % "".join(traceback.format_exception(*a)), "err")
    root.mainloop()


if __name__ == "__main__":
    main()
