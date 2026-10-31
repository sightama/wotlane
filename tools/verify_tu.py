#!/usr/bin/env python3
"""Every check for a decompiled TU in one command, with a table per function at the end (runs in WSL, from the repo root):

    ~/.venvs/wotm/bin/python tools/verify_tu.py game/Monster [game/TheGame ...] [--runs 30] [--no-nm] [--nm-name verify]

Per TU: each function's score against retail with -DNON_MATCHING (tools/ccmatch.py, what scoreall.sh does), a difftest of the
equivalent ones (tools/difftest_all.py) and, unless --no-nm, an ELF with those TUs' equivalent functions compiled in
(tools/wsl/build_nm.sh) to compare against retail the call sequence (nm_calls.py), the linked code layout (nm_audit.py) and the
return values the callers consume (nm_retvals.py; that one looks at the source and the retail asm, it doesn't need the ELF).

Table columns: score (MATCH, or differing words/total), difftest (ok/DIFFERS/pulado = skipped), calls (same/DIFFERS/-), layout (ok
or how many words differ), return (ok or "void CONSUMED"). Matching functions (no NON_MATCHING) only have the score column.
Exits with an error if any equivalent function has a difftest that differs, different calls or a consumed return: those are the
bugs that have already slipped through.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PY = sys.executable
NM_BLOCK = re.compile(r'#ifdef NON_MATCHING\n(?:/\*.*?\*/\n)?.*?#else\nINCLUDE_ASM\("[^"]+", (\S+)\);\n#endif', re.S)


def run(cmd, **kw):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, **kw)
    return p.stdout + p.stderr


def tu_flags(tu):
    for line in (ROOT / "config/tu_flags.txt").read_text().split("\n"):
        parts = line.split()
        if parts and parts[0] == tu:
            return " ".join(parts[1:])
    return ""


def nm_functions(tu):
    return NM_BLOCK.findall((ROOT / f"src/{tu}.cpp").read_text(errors="ignore"))


def score(tu):
    """{function: 'MATCH' | 'n/m' | other}: from ccmatch's last line, 'project <f1>: <r1>; <f2>: <r2>; ...'."""
    out = run([PY, "tools/ccmatch.py", f"src/{tu}.cpp", f"-DNON_MATCHING {tu_flags(tu)}".strip(), "project"])
    last = [l for l in out.strip().split("\n") if l.strip()][-1] if out.strip() else ""
    last = re.sub(r"^project\s*", "", last)
    res = {}
    for item in last.split(";"):
        if ":" in item:
            name, r = item.split(":", 1)
            res[name.strip()] = r.strip()
    return res, out


def difftest(tus, runs):
    """{mangled: (status, detail)} from difftest_all."""
    out = run([PY, "tools/difftest_all.py", *tus, "--runs", str(runs)])
    res = {}
    for l in out.split("\n"):
        m = re.match(r"^(\S+): (\S+) \[[^\]]*\]: (\w+) \((\d+) ok, (\d+) differ, (\d+) skipped\)", l)
        if m:
            res[m.group(2)] = (m.group(3), f"{m.group(4)} ok, {m.group(5)} diff, {m.group(6)} skip")
            continue
        m = re.match(r"^(\S+): (\S+): (unsupported signature|not available|.*)$", l)
        if m and not l.startswith("total"):
            res[m.group(2)] = ("pulado", m.group(3))
    return res, out


def build_nm(name, tus):
    out = run(["sh", "tools/wsl/build_nm.sh", name, *tus])
    elf = Path.home() / f"wotm_nm_{name}/build/SCUS_971.97.nm.elf"
    ok = elf.exists() and "undefined reference" not in out and "rror" not in out.replace("mirror", "")
    return (elf if ok else None), out


def nm_calls(elf):
    out = run([PY, "tools/nm_calls.py", str(elf)])
    bad = set(re.findall(r"^\S+: (\S+)$", out, re.M))
    return bad, out


def nm_audit(elf):
    out = run([PY, "tools/nm_audit.py", str(elf)])
    diff = {m.group(1): int(m.group(2)) for m in re.finditer(r"^\S+: (\S+)\s+\(diff (\d+)\)", out, re.M)}
    return diff, out


def nm_retvals():
    out = run([PY, "tools/nm_retvals.py"])
    bad = {m.group(1): m.group(2) for m in re.finditer(r"^(\S+): declared \"(\w+)\" but consumed", out, re.M)}
    return bad, out


def main(argv):
    runs, no_nm, nm_name, tus, verbose = 30, False, "verify", [], False
    a = list(argv)
    while a:
        x = a.pop(0)
        if x == "--runs":
            runs = int(a.pop(0))
        elif x == "--no-nm":
            no_nm = True
        elif x == "--nm-name":
            nm_name = a.pop(0)
        elif x in ("-v", "--verbose"):
            verbose = True
        else:
            tus.append(x if "/" in x else f"game/{x}")
    if not tus:
        print(__doc__)
        return 2
    for tu in tus:
        if not (ROOT / f"src/{tu}.cpp").exists():
            print(f"src/{tu}.cpp does not exist")
            return 2
    logs = []
    scores = {}
    for tu in tus:
        s, log = score(tu)
        scores[tu] = s
        logs.append((f"ccmatch {tu}", log))
    dt, log = difftest(tus, runs)
    logs.append(("difftest_all", log))
    rv, log = nm_retvals()
    logs.append(("nm_retvals", log))
    calls_bad, audit, elf = set(), {}, None
    if not no_nm:
        elf, log = build_nm(nm_name, tus)
        logs.append((f"build_nm {nm_name}", log))
        if elf:
            calls_bad, log = nm_calls(elf)
            logs.append(("nm_calls", log))
            audit, log = nm_audit(elf)
            logs.append(("nm_audit", log))
        else:
            print(f"build_nm failed (see the log with -v); no results for calls and layout")
    failures = 0
    for tu in tus:
        nm = set(nm_functions(tu))
        rows = []
        for name, sc in scores[tu].items():
            if name not in nm:
                rows.append((name, sc, "", "", "", ""))
                continue
            d = dt.get(name, ("-", ""))
            dcol = {"agree": "ok", "differ": "DIFFERS", "skipped": "pulado"}.get(d[0], d[0]) + (f" ({d[1]})" if d[1] and d[0] != "pulado" else "")
            ccol = "-" if elf is None else ("DIFFERS" if name in calls_bad else "same")
            lcol = "-" if elf is None else (f"{audit[name]} words" if name in audit else "ok")
            rcol = f"{rv[name]} CONSUMED" if name in rv else "ok"
            if d[0] == "differ" or name in calls_bad or name in rv:
                failures += 1
            rows.append((name, sc, dcol, ccol, lcol, rcol))
        w = [max(len(r[i]) for r in rows + [("function", "score", "difftest", "calls", "layout", "return")]) for i in range(6)]
        print(f"== {tu}: {len(rows)} functions, {len(nm)} equivalent")
        hdr = ("function", "score", "difftest", "calls", "layout", "return")
        print("  " + "  ".join(h.ljust(w[i]) for i, h in enumerate(hdr)))
        for r in rows:
            print("  " + "  ".join(c.ljust(w[i]) for i, c in enumerate(r)))
    if verbose:
        for title, log in logs:
            print(f"\n----- {title} -----\n{log.rstrip()}")
    if failures:
        print(f"\n{failures} equivalent function(s) with problems (difftest differs, different calls or consumed return)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
