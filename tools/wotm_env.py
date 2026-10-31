#!/usr/bin/env python3
"""Checks the WOTM_* variable table in docs/passos/NATIVO.md against the code.

The code (native/ and the runtime patch in tools/runtime_patch/) is the source of truth:
every variable read by getenv("WOTM_...") must be in the table between the markers
<!-- WOTM_ENV:START --> and <!-- WOTM_ENV:END -->, and every variable in the table must
exist in the code. The descriptions are written by hand in the table itself (one row
may group several variables in the first column).

  python tools/wotm_env.py           # lists what is missing from the table and what is extra in it; exits 1 if any
  python tools/wotm_env.py --write   # appends the missing ones to the end of the table (empty description) and removes the extra ones
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "docs", "passos", "NATIVO.md")
SRC_DIRS = [os.path.join(ROOT, "native"), os.path.join(ROOT, "tools", "runtime_patch")]
SRC_EXT = (".hpp", ".h", ".cpp", ".inc", ".patch")
START, END = "<!-- WOTM_ENV:START -->", "<!-- WOTM_ENV:END -->"
VAR = re.compile(r'"(WOTM_[A-Z0-9_]+)"')
VAR_DOC = re.compile(r"`(WOTM_[A-Z0-9_]+)")


def scan_code():
    found = {}
    for d in SRC_DIRS:
        for dirpath, _, files in os.walk(d):
            for f in files:
                if not f.endswith(SRC_EXT):
                    continue
                p = os.path.join(dirpath, f)
                with open(p, encoding="utf-8", errors="replace") as fh:
                    for n, line in enumerate(fh, 1):
                        for v in VAR.findall(line):
                            found.setdefault(v, "%s:%d" % (os.path.relpath(p, ROOT).replace("\\", "/"), n))
    return found


def read_doc():
    with open(DOC, encoding="utf-8") as fh:
        text = fh.read()
    a, b = text.index(START), text.index(END)
    block = text[a + len(START):b]
    rows = [l for l in block.splitlines() if l.startswith("|")]
    head, body = rows[:2], rows[2:]
    return text, a, b, head, body


def main():
    write = "--write" in sys.argv
    code = scan_code()
    text, a, b, head, body = read_doc()
    in_doc = {}
    for row in body:
        first = row.split("|")[1]
        for v in VAR_DOC.findall(first):
            in_doc.setdefault(v, row)
    missing = sorted(v for v in code if v not in in_doc)
    stale = sorted(v for v in in_doc if v not in code)
    for v in missing:
        print("MISSING from the table: %s (%s)" % (v, code[v]))
    for v in stale:
        print("EXTRA in the table (not in the code): %s" % v)
    if not write:
        print("%d variables in the code, %d in the table" % (len(code), len(in_doc)))
        return 1 if (missing or stale) else 0
    kept = [r for r in body if not any(v in stale for v in VAR_DOC.findall(r.split("|")[1]))]
    for v in missing:
        kept.append("| `%s` | (describe; read in `%s`) |" % (v, code[v]))
    block = "\n" + "\n".join(head + kept) + "\n"
    with open(DOC, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text[:a + len(START)] + block + text[b:])
    print("table rewritten: +%d -%d" % (len(missing), len(stale)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
