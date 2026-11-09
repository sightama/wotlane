#!/usr/bin/env python3
# ABOUTME: Map creator tool: lists a stage's pieces, makes a map folder (stage.json with baseHash) and checks the collision
# ABOUTME: grid rule (every path listed in exactly the cells its box covers) on a stock .NGP or a RAM dump of an edited stage.
"""Map creator tool (docs/superpowers/specs/2026-11-09-map-creator-design.md, issue #10).

  stage_map.py check <NGP or dump>...      grid rule; a dump is the raw image (bytes from 0xA00000), and a sibling
                                           <dump>.reserve, if present, is mapped at --reserve (default 0x3EFF000)
  stage_map.py pieces <NGP or dump>        one line per piece: offset, kind, world position, collision paths, destructible
  stage_map.py make --base CENTRAL --id <id> --name "<NAME>" --out <folder> [--remove 0x..]... [--move 0x..=x,y,z[,turn]]...
  stage_map.py selftest

The native loader mirrors these rules exactly (keep them in step):
- children(): the child slots of each opcode (table below); null and unmapped pointers are skipped.
- walk_all(): depth-first preorder from the header's roots in order, children in slot order; a node reached twice keeps
  its first parent.
- Piece: a TRANSLATE (3) or ROTATE (4) with a parent, whose subtree holds an OBJECT (0) or POLY_COLL (42), and with no
  transforming node (TRANSLATE 3, ROTATE 4, SCALE 30, ANIM_XFORM 17, ANIM_SCALE 38) on its first-parent chain to the root.
  So pieces never nest, root-level transforms (the pedestrian TRANSLATEs with id1 3000) and everything under them are not
  pieces, animated parts (under an ANIM_XFORM of a CHAR_INSTANCE: vehicles, flags) are not pieces, and a piece's local
  translation (a ROTATE's fourth row) is its world position, which is what `move` writes. CHAR_INSTANCE (25) and
  SKEL_BONE (35) themselves do not transform (native Scene::walk) and do not block a piece below them.
- Grid coverage: cols floor((boxMin.x - minX) / cellSize) .. floor((boxMax.x - minX) / cellSize) inclusive, rows the
  same with Y, clamped to [0, nCols - 1] and [0, nRows - 1]; cell index row * nCols + col.
- baseHash: FNV-1a 64 over the whole inflated image.
"""
import argparse
import glob
import json
import math
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ngp  # noqa: E402

BASE = 0xA00000
RESERVE = 0x4000000 - 0x101000   # kReserve: PS2_RAM_SIZE - 0x101000, 1 MB
FNV_OFF, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

OP_OBJECT, OP_TRANSLATE, OP_ROTATE = 0, 3, 4
OP_GRID, OP_DESTRUCTIBLE, OP_POLY_COLL = 15, 31, 42
TRANSFORMING = (3, 4, 30, 17, 38)   # TRANSLATE, ROTATE, SCALE, ANIM_XFORM, ANIM_SCALE


def fnv1a(data):
    h = FNV_OFF
    for b in data:
        h = ((h ^ b) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


class Mem:
    """Guest memory: the image at BASE plus optional extra regions (the reserve of a RAM dump)."""

    def __init__(self, image, extra=()):
        self.regions = [(BASE, image)] + list(extra)

    def _at(self, a, n):
        for base, d in self.regions:
            if base <= a and a - base + n <= len(d):
                return d, a - base
        raise IndexError("address %08x not mapped" % a)

    def ok(self, a, n=4):
        try:
            self._at(a, n)
            return True
        except IndexError:
            return False

    def u32(self, a):
        d, o = self._at(a, 4)
        return struct.unpack_from("<I", d, o)[0]

    def s16(self, a):
        d, o = self._at(a, 2)
        return struct.unpack_from("<h", d, o)[0]

    def u16(self, a):
        d, o = self._at(a, 2)
        return struct.unpack_from("<H", d, o)[0]

    def u8(self, a):
        d, o = self._at(a, 1)
        return d[o]

    def f32(self, a):
        d, o = self._at(a, 4)
        return struct.unpack_from("<f", d, o)[0]

    def roots(self):
        return [self.u32(BASE + 4 + 4 * i) for i in range(self.u32(BASE))]


def op(m, n):
    return m.u32(n) & 63


def children(m, n):
    """Child pointers of node n (layouts in include/hieri_types.h); nulls and unmapped pointers dropped."""
    o = op(m, n)

    def arr(a, k, stride=4, off=0):
        return [m.u32(a + stride * i + off) for i in range(min(k, 4096))]
    if o == 1:      # GROUP: numKids u16 +8, child[] +0x20
        out = arr(n + 0x20, m.u16(n + 8))
    elif o == 3:    # TRANSLATE: numKids +8, child[] +0x1C
        out = arr(n + 0x1C, m.u32(n + 8))
    elif o == 4:    # ROTATE: numKids +8, child[] +0x50
        out = arr(n + 0x50, m.u32(n + 8))
    elif o == 30:   # SCALE: numKids +8, child[] +0x2C
        out = arr(n + 0x2C, m.u32(n + 8))
    elif o == 6:    # SWITCH: numKids u8 +0xB, child[] +0xC
        out = arr(n + 0xC, m.u8(n + 0xB))
    elif o == 2:    # LOD: numLods +4, lods[] +0x20 (16 bytes each, child at +8)
        k = m.u32(n + 4)
        out = arr(n + 0x20, k, 16, 8) if 0 < k < 16 else []
    elif o == 8:    # CONTROL: child1 +4, child2 +8 (may be null)
        out = [m.u32(n + 4), m.u32(n + 8)]
    elif o == 23:   # ACTION_DATA: child +4
        out = [m.u32(n + 4)]
    elif o == 39:   # INTERACTIVE: child +0xC
        out = [m.u32(n + 0xC)]
    elif o == 31:   # DESTRUCTIBLE: numKids u16 +0xA, states[] +0xC (12 bytes each, child at +0)
        out = arr(n + 0xC, m.u16(n + 0xA), 12)
    elif o == 25:   # CHAR_INSTANCE: count +0x28, child[] +0x2C
        out = arr(n + 0x2C, m.u32(n + 0x28))
    elif o == 17:   # ANIM_XFORM: count +4, child[] +0xCC
        out = arr(n + 0xCC, m.u32(n + 4))
    elif o == 35:   # SKEL_BONE (ANIM_XFORM container): count +4, child[] +0xC
        out = arr(n + 0xC, m.u32(n + 4))
    elif o == 38:   # ANIM_SCALE: count +4, child[] +0x60
        out = arr(n + 0x60, m.u32(n + 4))
    else:
        out = []
    return [k for k in out if k and m.ok(k, 4)]


def walk_all(m):
    """{node: first parent} for every node reachable from the roots (roots map to None); preorder, slot order."""
    parent = {}
    stack = [(r, None) for r in reversed(m.roots()) if r and m.ok(r, 4)]
    while stack:
        n, p = stack.pop()
        if n in parent:
            continue
        parent[n] = p
        for k in reversed(children(m, n)):
            if k not in parent:
                stack.append((k, n))
    return parent


def find_grid(m, parent):
    grids = [n for n in parent if op(m, n) == OP_GRID]
    if len(grids) != 1:
        raise ValueError("%d collision grids (expected 1)" % len(grids))
    return grids[0]


def grid_dims(m, g):
    return m.s16(g + 4), m.s16(g + 6), m.f32(g + 8), m.f32(g + 0xC), m.f32(g + 0x10)


def grid_cells(m, g):
    """[(cell index, [(list node, path)])] for every cell."""
    nc, nr = m.s16(g + 4), m.s16(g + 6)
    out = []
    for i in range(nc * nr):
        lst, p, guard = [], m.u32(g + 0x20 + 4 * i), 0
        while p and guard < 1 << 20:
            lst.append((p, m.u32(p + 4)))
            p, guard = m.u32(p), guard + 1
        out.append((i, lst))
    return out


def covered(m, g, path):
    """Cells the path's world box covers: floor-inclusive, clamped, index row * nCols + col."""
    nc, nr, cs, mx, my = grid_dims(m, g)

    def span(lo, hi, origin, k):
        a = math.floor((lo - origin) / cs)
        b = math.floor((hi - origin) / cs)
        return max(a, 0), min(b, k - 1)
    c0, c1 = span(m.f32(path), m.f32(path + 0xC), mx, nc)
    r0, r1 = span(m.f32(path + 4), m.f32(path + 0x10), my, nr)
    return {r * nc + c for r in range(r0, r1 + 1) for c in range(c0, c1 + 1)}


def path_nodes(m, path):
    return [m.u32(path + 0x24 + 4 * i) for i in range(min(m.u32(path + 0x1C), 256))]


def all_paths(m, g):
    seen = {}
    for i, lst in grid_cells(m, g):
        for _node, p in lst:
            seen.setdefault(p, set()).add(i)
    return seen


def check(m, parent=None):
    """(paths, list nodes, problems) for the grid rule."""
    if parent is None:
        parent = walk_all(m)
    g = find_grid(m, parent)
    problems, owner, listed, nnodes = [], {}, {}, 0
    for i, lst in grid_cells(m, g):
        here = set()
        for node, p in lst:
            nnodes += 1
            if node in owner:
                problems.append("list node 0x%x in cells %d and %d" % (node - BASE, owner[node], i))
            owner[node] = i
            if p in here:
                problems.append("path 0x%x twice in cell %d" % (p - BASE, i))
            here.add(p)
            listed.setdefault(p, set()).add(i)
    for p, cells in sorted(listed.items()):
        want = covered(m, g, p)
        if cells != want:
            problems.append("path 0x%x: listed in %s, box covers %s" % (p - BASE, sorted(cells), sorted(want)))
    return len(listed), nnodes, problems


# ---------------------------------------------------------------- pieces

def position(m, n):
    """A piece's translation: TRANSLATE trans (+0x10), ROTATE matrix fourth row (+0x40). With no transform above a
    piece (the piece rule), this is its world position."""
    o = 0x10 if op(m, n) == OP_TRANSLATE else 0x40
    return (m.f32(n + o), m.f32(n + o + 4), m.f32(n + o + 8))


def subtree_ops(m, n):
    ops, seen, st = set(), set(), [n]
    while st:
        k = st.pop()
        if k in seen:
            continue
        seen.add(k)
        ops.add(op(m, k))
        st.extend(children(m, k))
    return ops


def pieces(m, parent=None):
    """[(node, opcode, (x, y, z), paths, destructible)] in walk order."""
    if parent is None:
        parent = walk_all(m)
    g = find_grid(m, parent)
    through = {}
    for p in all_paths(m, g):
        for n in set(path_nodes(m, p)):
            through[n] = through.get(n, 0) + 1
    out = []
    for n, p in parent.items():
        if op(m, n) not in (OP_TRANSLATE, OP_ROTATE) or p is None:
            continue
        above = p
        while above is not None and op(m, above) not in TRANSFORMING:
            above = parent[above]
        if above is not None:
            continue
        ops = subtree_ops(m, n)
        if OP_OBJECT not in ops and OP_POLY_COLL not in ops:
            continue
        out.append((n, op(m, n), position(m, n), through.get(n, 0), OP_DESTRUCTIBLE in ops))
    return out


# ---------------------------------------------------------------- commands

def load_mem(path, reserve=RESERVE):
    if path.upper().endswith(".NGP"):
        return Mem(ngp.inflate_ngp(path))
    with open(path, "rb") as f:
        image = f.read()
    extra = []
    if os.path.exists(path + ".reserve"):
        with open(path + ".reserve", "rb") as f:
            extra.append((reserve, f.read()))
    return Mem(image, extra)


def expand(paths):
    out = []
    for p in paths:
        hits = sorted(glob.glob(p))
        out += hits if hits else [p]
    return out


def cmd_check(a):
    bad = 0
    for path in expand(a.files):
        name = os.path.basename(path)
        try:
            npaths, nnodes, problems = check(load_mem(path, a.reserve))
        except (ValueError, IndexError, OSError) as e:
            npaths, nnodes, problems = 0, 0, [str(e)]
        if problems:
            bad += 1
            print("%s: %d problems" % (name, len(problems)))
            for p in problems[:50]:
                print("  " + p)
        else:
            print("%s: %d paths, %d list nodes, OK" % (name, npaths, nnodes))
    return 1 if bad else 0


def cmd_pieces(a):
    m = load_mem(a.file, a.reserve)
    ps = pieces(m)
    for n, o, pos, npaths, destr in ps:
        print("0x%x  %-9s  pos %.1f %.1f %.1f  paths %d%s" % (
            n - BASE, "TRANSLATE" if o == OP_TRANSLATE else "ROTATE", pos[0], pos[1], pos[2], npaths,
            "  destructible" if destr else ""))
    print("%d pieces" % len(ps), file=sys.stderr)
    return 0


def parse_node(s):
    try:
        return int(s, 16)
    except ValueError:
        raise SystemExit("bad node %r (expected 0x<offset>)" % s)


def num(v):
    return int(v) if float(v).is_integer() else v


def cmd_make(a):
    if not re.fullmatch(r"[a-z][a-z0-9_]{0,15}", a.id):
        print("id %r: a letter first, then [a-z0-9_], at most 16 characters" % a.id)
        return 1
    name = a.name.upper()
    if not name or len(name) > 17:
        print("name %r: 1 to 17 characters" % a.name)
        return 1
    ngp_path = os.path.join(a.disc if os.path.isabs(a.disc) else os.path.join(REPO, a.disc), "LVL", a.base.upper() + ".NGP")
    image = ngp.inflate_ngp(ngp_path)
    m = Mem(image)
    valid = {n - BASE for n, *_ in pieces(m)}
    edits, used = [], set()

    def take(node):
        if node not in valid:
            raise SystemExit("node 0x%x is not a piece of %s (see: stage_map.py pieces)" % (node, a.base.upper()))
        if node in used:
            raise SystemExit("node 0x%x edited twice" % node)
        used.add(node)
    for mv in a.move:
        if "=" not in mv:
            print("bad --move %r (expected 0x..=x,y,z[,turn])" % mv)
            return 1
        node_s, vals = mv.split("=", 1)
        try:
            v = [float(x) for x in vals.split(",")]
        except ValueError:
            v = []
        if len(v) not in (3, 4):
            print("bad --move %r (expected 0x..=x,y,z[,turn])" % mv)
            return 1
        node = parse_node(node_s)
        take(node)
        e = {"node": "0x%x" % node, "move": v[:3]}
        if len(v) == 4 and v[3] != 0:
            e["turn"] = num(v[3])
        edits.append(e)
    for r in a.remove:
        node = parse_node(r)
        take(node)
        edits.append({"node": "0x%x" % node, "remove": True})
    doc = {"name": name, "base": a.base.upper(), "baseHash": "%016x" % fnv1a(image), "edits": edits}
    os.makedirs(a.out, exist_ok=True)
    out = os.path.join(a.out, "stage.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    print("%s: %d edits, baseHash %s" % (out, len(edits), doc["baseHash"]))
    return 0


def cmd_selftest(_a):
    assert fnv1a(b"") == 0xCBF29CE484222325
    assert fnv1a(b"a") == 0xAF63DC4C8601EC8C
    # synthetic image: roots GROUP -> TRANSLATE -> POLY_COLL, and a 2x2 grid of 100-unit cells with one path
    img = bytearray(0x200)

    def w32(o, v):
        struct.pack_into("<I", img, o, v)

    def wf(o, v):
        struct.pack_into("<f", img, o, v)
    w32(0, 2), w32(4, BASE + 0x40), w32(8, BASE + 0x100)
    w32(0x40, 1), struct.pack_into("<H", img, 0x48, 1), w32(0x60, BASE + 0x80)            # GROUP, 1 kid
    w32(0x80, 3), w32(0x88, 1), wf(0x90, 5.0), wf(0x94, 6.0), w32(0x9C, BASE + 0xC0)    # TRANSLATE (5,6,0)
    w32(0xC0, 42)                                                                         # POLY_COLL
    w32(0x100, 15), struct.pack_into("<hh", img, 0x104, 2, 2)                           # grid 2x2
    for k, v in enumerate((100.0, 0.0, 0.0, 200.0, 200.0)):
        wf(0x108 + 4 * k, v)
    for k, v in enumerate((10.0, 10.0, 0.0, 150.0, 50.0, 9.0)):                          # path box: cols 0-1, row 0
        wf(0x140 + 4 * k, v)
    w32(0x15C, 3), w32(0x164, BASE + 0x40), w32(0x168, BASE + 0x80), w32(0x16C, BASE + 0xC0)
    w32(0x180, 0), w32(0x184, BASE + 0x140), w32(0x188, 0), w32(0x18C, BASE + 0x140)   # two list nodes
    w32(0x120, BASE + 0x180), w32(0x124, BASE + 0x188)                                   # cells 0 and 1
    npaths, nnodes, problems = check(Mem(bytes(img)))
    assert (npaths, nnodes, problems) == (1, 2, []), problems
    ps = pieces(Mem(bytes(img)))
    assert [(n, pos, k) for n, _o, pos, k, _d in ps] == [(BASE + 0x80, (5.0, 6.0, 0.0), 1)], ps
    w32(0x124, 0), w32(0x128, BASE + 0x188)                                              # cell 2 instead of 1
    assert check(Mem(bytes(img)))[2], "wrong cell not detected"
    print("selftest OK")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("check", help="grid rule on stock .NGP files or RAM dumps")
    p.add_argument("files", nargs="+")
    p.add_argument("--reserve", type=lambda s: int(s, 0), default=RESERVE)
    p.set_defaults(fn=cmd_check)
    p = sub.add_parser("pieces", help="list a stage's pieces")
    p.add_argument("file")
    p.add_argument("--reserve", type=lambda s: int(s, 0), default=RESERVE)
    p.set_defaults(fn=cmd_pieces)
    p = sub.add_parser("make", help="write <out>/stage.json")
    p.add_argument("--base", required=True)
    p.add_argument("--id", required=True)
    p.add_argument("--name", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--remove", action="append", default=[])
    p.add_argument("--move", action="append", default=[])
    p.add_argument("--disc", default="disc", help="disc folder (relative to the repo root unless absolute)")
    p.set_defaults(fn=cmd_make)
    p = sub.add_parser("selftest")
    p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
