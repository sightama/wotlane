#!/usr/bin/env python3
"""Interface do kit de mods (tools/modkit.py): escolher no disco o que extrair, ver e abrir o que foi extraido, empacotar no mod.

  python tools/modkit_gui.py        (ou pythonw, sem console; o launcher tem o botao "Mod kit")

Textos da janela em ingles (pedido do dono). Esquerda: o disco por nome (monstros e trajes, fases, menu, bancos de som, musicas);
duplo clique ou "Extract selected" manda para a pasta de trabalho (<mods>/_work). Direita: a pasta de trabalho com miniaturas;
"edited" = mudou desde a extracao (vai no proximo pacote). Duplo clique abre a textura no editor escolhido em "Editors..." (ou no
programa padrao do Windows) e toca o som na propria janela; botao direito: Play, Open with..., Save to folder..., Remove. Embaixo:
o nome do mod, "Pack edited files into mod" (modkit pack) e "Check mods" (modkit check); o log tem divisoria arrastavel. Pastas,
editores e altura do log ficam em ~/.wotm_modkit.json. Precisa do Pillow (pip install pillow) para as miniaturas.
"""
import io
import json
import os
import queue
import subprocess
import sys
import shutil
import threading
import traceback
import wave
from array import array
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
        self.autoplay = tk.BooleanVar(value=self.conf.get("autoplay", True))
        self.editors = dict(self.conf.get("editors") or {})   # ".png" -> executavel; sem entrada: programa padrao do Windows
        self.last_played = None
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

        # divisoria vertical: em cima o trabalho, embaixo o log (arrastar a borda muda a altura do log)
        vsplit = ttk.PanedWindow(self.root, orient="vertical")
        vsplit.pack(fill="both", expand=True)
        upper = ttk.Frame(vsplit)
        vsplit.add(upper, weight=5)
        self.vsplit = vsplit
        panes = ttk.PanedWindow(upper, orient="horizontal")
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
        b.pack(fill="x", pady=(6, 0), side="bottom", before=self.disc_tree)
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
        self.work_tree.bind("<<TreeviewSelect>>", lambda e: self.on_select())
        self.work_tree.bind("<Double-1>", self.on_double)
        self.work_tree.bind("<Button-3>", self.context_menu)
        self.work_tree.bind("<Delete>", lambda e: self.remove_selected())
        self.menu = tk.Menu(self.root, tearoff=0, bg=BG2, fg=FG, activebackground=ACC, activeforeground="white", bd=0)

        pv = ttk.Frame(body, padding=(8, 0, 0, 0))
        body.add(pv, weight=2)
        self.canvas = tk.Canvas(pv, bg=BG2, highlightthickness=0, height=300)
        self.canvas.bind("<Configure>", lambda e: self.show_preview())
        foot = ttk.Frame(pv)   # legenda e botoes presos embaixo; a previa fica com o que sobra
        foot.pack(side="bottom", fill="x")
        self.pv_label = ttk.Label(foot, text="", style="Dim.TLabel", wraplength=360, justify="left")
        self.pv_label.pack(fill="x", pady=4)
        pb = ttk.Frame(foot)
        pb.pack(fill="x")
        ttk.Button(pb, text="Open in editor", command=self.open_selected).pack(side="left")
        ttk.Button(pb, text="Show in folder", command=self.reveal_selected).pack(side="left", padx=4)
        self.btn_play = ttk.Button(pb, text="Play", command=self.play_selected)
        self.btn_play.pack(side="left")
        ttk.Button(pb, text="Stop", command=self.stop_sound).pack(side="left", padx=4)
        ttk.Button(pb, text="Remove", command=self.remove_selected).pack(side="right")
        pb2 = ttk.Frame(foot)
        pb2.pack(fill="x", pady=(4, 0))
        ttk.Checkbutton(pb2, text="play sounds on click", variable=self.autoplay).pack(side="left")
        ttk.Button(pb2, text="Editors...", command=self.editors_dialog).pack(side="right")
        # a previa por ultimo: com pouca altura ela encolhe e os botoes ficam
        self.canvas.pack(fill="both", expand=True)

        # embaixo: mod + log
        bot = ttk.Frame(upper, padding=(10, 4, 10, 8))
        bot.pack(fill="x", side="bottom", before=panes)   # antes da area elastica: nao some com o log alto
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

        lf = ttk.Frame(vsplit, padding=(10, 0, 10, 10))
        vsplit.add(lf, weight=1)
        self.log = tk.Text(lf, height=4, bg="#141414", fg="#cfcfcf", insertbackground=FG, relief="flat", font=("Consolas", 9),
                           wrap="none")
        lsb = ttk.Scrollbar(lf, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=lsb.set)
        self.log.pack(side="left", fill="both", expand=True)
        lsb.pack(side="right", fill="y")
        vsplit.bind("<ButtonRelease-1>", lambda e: self.clamp_log())
        # altura do log da ultima vez (ou ~9 linhas)
        self.root.after(50, lambda: (self.vsplit.sashpos(0, self.vsplit.winfo_height() - int(self.conf.get("log_height", 170))), self.clamp_log()))
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

    def clamp_log(self):
        """Divisoria do log: no minimo ~3 linhas de log e espaco para as listas e os botoes em cima."""
        try:
            h = self.vsplit.winfo_height()
            self.vsplit.sashpos(0, min(max(self.vsplit.sashpos(0), 360), h - 60))
        except tk.TclError:
            pass

    def save_conf(self):
        c = self.conf
        c.update(disc=self.disc.get(), mods=self.mods.get(), mod=self.mod.get(), autoplay=self.autoplay.get(), editors=self.editors)
        try:
            c["log_height"] = max(60, self.vsplit.winfo_height() - self.vsplit.sashpos(0))
        except tk.TclError:
            pass
        try:
            with open(CONF, "w", encoding="utf-8") as f:
                json.dump(c, f, indent=1)
        except OSError:
            pass

    def close(self):
        self.save_conf()
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

    def selected_files(self, groups=False):
        """Arquivos escolhidos na lista. groups=True: um grupo selecionado vale por todos os arquivos dele (Save to folder,
        Remove); senao so as linhas de arquivo (abrir, tocar, previa: clicar numa secao nao abre tudo dela)."""
        t, out = self.work_tree, []
        for i in t.selection():
            if i.startswith("grp:") and not groups:
                continue
            for r in (t.get_children(i) if i.startswith("grp:") else (i,)):
                p = os.path.join(self.work(), *r.split("/"))
                if p not in out:
                    out.append(p)
        return out

    def show_preview(self):
        c = self.canvas
        c.delete("all")
        files = self.selected_files()
        allf = self.selected_files(groups=True)
        if not files and allf:
            self.pv_label.configure(text="%d file(s) in the selected section. Open it (arrow or double-click) and pick a file; "
                                         "right-click saves or removes the whole section." % len(allf))
            return
        if not files:
            self.pv_label.configure(text="Select a file in the work folder. Double-click opens a texture in its editor (\"Editors...\") "
                                         "and plays a sound; right-click for more (Save to folder...).")
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
            if not self.waveform(p, w, h):
                c.create_text(w // 2, h // 2, text="sound", fill=DIM, font=("Segoe UI", 20))
            mb = modkit.BNK_RE.match(os.path.basename(p))
            text += "\n%s" % ("Sound %d of bank %s.BNK" % (int(mb.group(2)), mb.group(1).upper()) if mb else "Music or voice (.VPK/.VAG)")
        if len(files) > 1:
            text += "\n(%d selected)" % len(files)
        self.pv_label.configure(text=text)

    def on_select(self):
        self.show_preview()
        files = self.selected_files()
        if self.autoplay.get() and len(files) == 1 and files[0].lower().endswith(".wav") and files[0] != self.last_played:
            self.play_selected()

    def on_double(self, e):
        row = self.work_tree.identify_row(e.y)
        if not row or row.startswith("grp:"):
            return   # duplo clique no grupo so abre/fecha
        if row.lower().endswith(".wav") and not self.editors.get(".wav"):
            self.play_selected()   # som toca aqui; com editor de .wav definido, abre nele
        else:
            self.open_selected()

    @staticmethod
    def ext_of(p):
        return os.path.splitext(p)[1].lower()

    def waveform(self, p, w, h):
        """Desenha a forma de onda do WAV (PCM 8/16 bits) na previa. False se nao deu."""
        try:
            with wave.open(p) as wf:
                sw, ch, n = wf.getsampwidth(), wf.getnchannels(), wf.getnframes()
                raw = wf.readframes(min(n, 4_000_000))
        except Exception:
            return False
        if sw == 2:
            a = array("h")
            a.frombytes(raw[:len(raw) // 2 * 2])
            if sys.byteorder == "big":
                a.byteswap()
            full = 32768.0
        elif sw == 1:
            a = [b - 128 for b in raw]
            full = 128.0
        else:
            return False
        a = a[::ch]   # so o primeiro canal
        if not len(a):
            return False
        cols = max(1, w - 16)
        step = max(1, len(a) // cols)
        mid, amp = h / 2, (h - 24) / 2
        c = self.canvas
        c.create_line(8, mid, w - 8, mid, fill="#444")
        for x in range(cols):
            seg = a[x * step:(x + 1) * step]
            if not len(seg):
                break
            lo, hi = min(seg), max(seg)
            c.create_line(8 + x, mid - hi / full * amp, 8 + x, mid - lo / full * amp + 1, fill=ACC)
        return True

    def open_selected(self):
        for p in self.selected_files()[:8]:
            exe = self.editors.get(self.ext_of(p))
            if exe:
                self.open_with(p, exe)
            else:
                self.open_path(p)

    def open_with(self, p, exe):
        try:
            subprocess.Popen([exe, os.path.normpath(p)])
        except OSError as ex:
            self.say("Could not start %s: %s\n" % (exe, ex), "err")

    def pick_exe(self, ext):
        exe = filedialog.askopenfilename(parent=self.root, title="Program to open %s files" % ext,
                                         filetypes=[("Programs", "*.exe"), ("All files", "*.*")],
                                         initialdir=os.path.dirname(self.editors.get(ext, "")) or os.environ.get("ProgramFiles", "C:\\"))
        return os.path.normpath(exe) if exe else None

    def open_with_dialog(self):
        files = self.selected_files()
        if not files:
            return
        ext = self.ext_of(files[0])
        exe = self.pick_exe(ext)
        if not exe:
            return
        if messagebox.askyesno("Open with", "Always open %s files with %s?" % (ext, os.path.basename(exe)), parent=self.root):
            self.editors[ext] = exe
            self.save_conf()
        for p in files[:8]:
            self.open_with(p, exe)

    def editors_dialog(self):
        d = tk.Toplevel(self.root, bg=BG)
        d.title("Editors")
        d.transient(self.root)
        d.resizable(True, False)
        fr = ttk.Frame(d, padding=12)
        fr.pack(fill="both", expand=True)
        ttk.Label(fr, text="Program used by \"Open in editor\" and double-click, per file type.\nEmpty = the Windows default "
                           "(for sounds: play here in the kit).", style="Dim.TLabel", justify="left").grid(row=0, column=0, columnspan=4,
                                                                                                         sticky="w", pady=(0, 8))
        rows = {}
        exts = [".png", ".wav"] + sorted(e for e in self.editors if e not in (".png", ".wav"))
        for r, ext in enumerate(exts, 1):
            v = tk.StringVar(value=self.editors.get(ext, ""))
            rows[ext] = v
            ttk.Label(fr, text={".png": "Textures (.png)", ".wav": "Sounds (.wav)"}.get(ext, ext)).grid(row=r, column=0, sticky="w", padx=(0, 8))
            ttk.Entry(fr, textvariable=v, width=60).grid(row=r, column=1, sticky="ew", pady=2)
            ttk.Button(fr, text="...", width=3, command=lambda e=ext, v=v: v.set(self.pick_exe(e) or v.get())).grid(row=r, column=2, padx=4)
            ttk.Button(fr, text="Clear", command=lambda v=v: v.set("")).grid(row=r, column=3)
        fr.columnconfigure(1, weight=1)

        def ok():
            for ext, v in rows.items():
                exe = v.get().strip().strip('"')
                if exe:
                    self.editors[ext] = exe
                else:
                    self.editors.pop(ext, None)
            self.save_conf()
            d.destroy()

        b = ttk.Frame(fr)
        b.grid(row=len(exts) + 1, column=0, columnspan=4, sticky="e", pady=(10, 0))
        ttk.Button(b, text="OK", style="Accent.TButton", command=ok).pack(side="left")
        ttk.Button(b, text="Cancel", command=d.destroy).pack(side="left", padx=(6, 0))
        d.grab_set()

    def context_menu(self, e):
        t = self.work_tree
        row = t.identify_row(e.y)
        if row and row not in t.selection():
            t.selection_set(row)
            t.focus(row)
        files = self.selected_files()
        allf = self.selected_files(groups=True)
        m = self.menu
        m.delete(0, "end")
        if not allf:
            m.add_command(label="Open work folder", command=lambda: self.open_path(self.work()))
        elif not files:   # so secoes: nada de abrir/tocar tudo de uma vez
            n = len(allf)
            m.add_command(label="Show in folder", command=self.reveal_selected)
            m.add_separator()
            m.add_command(label="Save to folder... (%d file%s)" % (n, "" if n == 1 else "s"), command=self.save_to_folder)
            m.add_separator()
            m.add_command(label="Remove from work folder (%d file%s)" % (n, "" if n == 1 else "s"), command=self.remove_selected)
        else:
            n = len(allf)
            wav = any(p.lower().endswith(".wav") for p in files)
            if wav:
                m.add_command(label="Play", command=self.play_selected)
            exe = self.editors.get(self.ext_of(files[0]))
            m.add_command(label="Open in %s" % (os.path.splitext(os.path.basename(exe))[0] if exe else "default program"),
                          command=self.open_selected)
            m.add_command(label="Open with...", command=self.open_with_dialog)
            if len(files) == 1:
                m.add_command(label="Replace with...", command=self.replace_selected)
            m.add_command(label="Show in folder", command=self.reveal_selected)
            m.add_separator()
            m.add_command(label="Save to folder... (%d file%s)" % (n, "" if n == 1 else "s"), command=self.save_to_folder)
            m.add_separator()
            m.add_command(label="Remove from work folder", command=self.remove_selected)
        m.tk_popup(e.x_root, e.y_root)
        m.grab_release()

    IMG_EXT = (".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".tif", ".tiff", ".webp", ".dds")

    def replace_selected(self):
        """Troca um asset da pasta de trabalho por um arquivo escolhido, gravado com o nome certo. Imagem de outro formato vira PNG."""
        files = self.selected_files()
        if len(files) != 1:
            return
        dst = files[0]
        ext = self.ext_of(dst)
        if ext == ".png":
            types = [("Images", " ".join("*" + e for e in self.IMG_EXT)), ("All files", "*.*")]
        elif ext == ".wav":
            types = [("WAV sound", "*.wav"), ("All files", "*.*")]
        else:
            types = [("All files", "*.*")]
        src = filedialog.askopenfilename(parent=self.root, title="Replace %s with" % os.path.basename(dst), filetypes=types,
                                         initialdir=self.conf.get("replace_dir") or self.conf.get("save_dir") or os.path.expanduser("~"))
        if not src:
            return
        src = os.path.normpath(src)
        self.conf["replace_dir"] = os.path.dirname(src)
        sext = self.ext_of(src)
        if os.path.normcase(src) == os.path.normcase(os.path.normpath(dst)):
            return
        orig = self.orig_size(dst)
        try:
            if ext == ".png" and sext != ".png":
                if Image is None:
                    raise OSError("converting %s to PNG needs Pillow (pip install pillow)" % sext)
                with Image.open(src) as im:
                    im.convert("RGBA").save(dst, "PNG")
            elif ext == ".wav" and sext != ".wav":
                raise OSError("pick a .wav file (the kit converts WAV to the game's format when packing)")
            elif ext not in (".png", ".wav") and sext != ext:
                raise OSError("pick a %s file" % ext)
            else:
                if ext == ".wav":
                    with wave.open(src):   # confere que e um WAV que o pack consegue ler
                        pass
                shutil.copyfile(src, dst)   # copyfile: mtime novo, a miniatura atualiza
        except Exception as ex:
            self.say("Could not replace %s: %s\n" % (os.path.basename(dst), ex), "err")
            return
        self.stop_sound()
        msg = "Replaced %s with %s" % (os.path.basename(dst), os.path.basename(src))
        new = modkit.png_size(dst) if ext == ".png" else None
        if new:
            msg += " (%dx%d)" % new
        self.say(msg + "\n")
        if new and orig and new[0] * orig[1] != new[1] * orig[0]:
            self.say("WARNING: the game texture is %dx%d; a different aspect ratio looks stretched in the game\n" % orig, "warn")
        self.save_conf()
        self.refresh_work()
        rel = os.path.relpath(dst, self.work()).replace("\\", "/")
        if self.work_tree.exists(rel):
            self.work_tree.selection_set([rel])
            self.work_tree.see(rel)
        self.show_preview()

    def orig_size(self, p):
        """Tamanho da textura no jogo (largura, altura): o que o export guardou ou, em export antigo, o do proprio arquivo enquanto
        ele nao foi editado. None se nao der para saber."""
        st = modkit.load_state(self.work())["files"].get(os.path.relpath(p, self.work()).replace("\\", "/")) or {}
        if st.get("size"):
            return tuple(st["size"])
        if st.get("sha1") and os.path.exists(p) and modkit.sha1(p) == st["sha1"]:
            return modkit.png_size(p)
        return None

    def save_to_folder(self):
        files = self.selected_files(groups=True)
        if not files:
            return
        d = filedialog.askdirectory(parent=self.root, title="Save %d file(s) to" % len(files),
                                    initialdir=self.conf.get("save_dir") or os.path.expanduser("~"))
        if not d:
            return
        d = os.path.normpath(d)
        self.conf["save_dir"] = d
        exist = [p for p in files if os.path.exists(os.path.join(d, os.path.basename(p)))]
        if exist and not messagebox.askyesno("Save to folder", "%d file(s) already exist in %s. Overwrite?" % (len(exist), d),
                                             parent=self.root):
            files = [p for p in files if p not in exist]
        n = 0
        for p in files:
            try:
                shutil.copy2(p, os.path.join(d, os.path.basename(p)))
                n += 1
            except OSError as ex:
                self.say("%s\n" % ex, "err")
        self.say("Saved %d file(s) to %s\n" % (n, d))
        self.save_conf()

    def reveal_selected(self):
        files = self.selected_files(groups=True)
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
        self.last_played = wavs[0]
        try:
            import winsound
            winsound.PlaySound(wavs[0], winsound.SND_FILENAME | winsound.SND_ASYNC | winsound.SND_NODEFAULT)
        except Exception as ex:
            self.say("Could not play: %s\n" % ex, "err")

    def stop_sound(self):
        self.last_played = None
        try:
            import winsound
            winsound.PlaySound(None, 0)
        except Exception:
            pass

    def remove_selected(self):
        files = self.selected_files(groups=True)
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
