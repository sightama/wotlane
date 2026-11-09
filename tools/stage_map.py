#!/usr/bin/env python3
# ABOUTME: Map creator tool: lists a stage's pieces, makes a map folder (stage.json with baseHash) and checks the collision
# ABOUTME: grid rule (every path listed in exactly the cells its box covers) on a stock .NGP or a RAM dump of an edited stage.
"""Map creator tool (docs/superpowers/specs/2026-11-09-map-creator-design.md, issue #10).

  stage_map.py check <NGP or dump>...      grid rule; a dump is the raw image (bytes from 0xA00000), and a sibling
                                           <dump>.reserve, if present, is mapped at --reserve (default 0x3EFF000)
  stage_map.py pieces <NGP or dump>        one line per piece: node, kind, world position, collision paths, riders,
                                           destructible; node is the offset from 0xA00000, or @<absolute address> outside
                                           the image
  stage_map.py make --base CENTRAL --id <id> --name "<NAME>" --out <stages folder> [--remove 0x..]...
                    [--move 0x..=x,y,z[,turn]]... [--turn 0x..=degrees]... [--no-riders]
                                           writes <stages folder>/<id>/stage.json (--turn and --move on the same node make
                                           one edit), plus an edit per rider of an edited piece (rider_map(); the user's own
                                           edit of a rider stands), printed as it is added
  stage_map.py inflate <NGP> <out>         writes the inflated stage image (what the game has at 0xA00000), e.g. for
                                           the native test's WOTM_TEST_STAGE
  stage_map.py ram <ram_<frame>.bin> [--monster I:ZMIN:ZMAX]... [--pickup X,Y,Z]... [--no-pickup X,Y,Z]...
                                           a fight's RAM (PS2X_DUMP_RAM): the monsters' positions and the level pickups'
                                           placed positions (what createPickup was given); exits 1 when monster I's height is
                                           outside [ZMIN, ZMAX], or a pickup is missing at (--pickup) or present at
                                           (--no-pickup) a place, within 1 unit (the map creator's live checks)
  stage_map.py selftest

The native loader mirrors these rules exactly (keep them in step):
- children(): the child slots of each opcode (table below); null and unmapped pointers are skipped, and a node whose
  count or slots are not all mapped has no children.
- walk_all(): depth-first preorder from the header's roots in order, children in slot order; a node reached twice keeps
  its first parent.
- Piece: a TRANSLATE (3) or ROTATE (4) with a parent, whose subtree holds an OBJECT (0) or POLY_COLL (42), with no
  transforming node (TRANSLATE 3, ROTATE 4, SCALE 30, ANIM_XFORM 17, ANIM_SCALE 38) on its first-parent chain, and
  (a) whose first-parent chain ends at a root with id1 1 (the world root, the one the game draws), or
  (b) whose first node with a nonzero id1, in preorder of its subtree (itself first), has a level pickup id
      (0x12C0..0x13EB; LevelPickups::createPickup places it with the traversal transform).
  So pieces never nest; root-level transforms (pedestrians, id1 3000) and what is under them, animated parts (under an
  ANIM_XFORM) and the template roots (HUD, weapons, unreferenced models) are not pieces; and a piece's own translation
  (a ROTATE's fourth row) is its world position, which is what `move` writes. CHAR_INSTANCE (25) and SKEL_BONE (35) do
  not transform (native Scene::walk) and do not block a piece below them.
- Grid coverage: cols floor((boxMin.x - minX) / cellSize) .. floor((boxMax.x - minX) / cellSize) inclusive, rows the
  same with Y, clamped to [0, nCols - 1] and [0, nRows - 1]; cell index row * nCols + col.
- baseHash: FNV-1a 64 over the whole inflated image.
Riders (rider_map(), make only; the loader applies them as ordinary edits): a piece whose origin (a pickup: its position; a
world piece: the bottom of its lowest OBJECT) lies in X and Y inside one of another piece's solid collision boxes at most 700
wide, at z from the box top - 20 to the box top + 25; on two pieces, the one whose box top is nearest, then the lower node.
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
WORLD_ID = 1
PICKUP_IDS = (0x12C0, 0x13EB)       # level pickups, inclusive
NAME_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-"   # the creator's NAME letters (native/runtime/wotm_creator.inc)
NAME_LEN = 17                       # GameLevelLongNames[][18]


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

    def in_image(self, a):
        return BASE <= a < BASE + len(self.regions[0][1])

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
        if not self.ok(BASE, 4):
            return []
        n = min(self.u32(BASE), 4096)
        return [self.u32(BASE + 4 + 4 * i) for i in range(n) if self.ok(BASE + 4 + 4 * i, 4)]


def addr(m, a):
    """Offset from BASE for a node in the image, @absolute address elsewhere (the reserve)."""
    return "0x%x" % (a - BASE) if m.in_image(a) else "@0x%08x" % a


def op(m, n):
    return m.u32(n) & 63


def children(m, n):
    """Child pointers of node n (layouts in include/hieri_types.h); nulls and unmapped pointers dropped."""
    def arr(a, k, stride=4, off=0):
        return [m.u32(a + stride * i + off) for i in range(min(k, 4096))]
    try:
        o = op(m, n)
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
    except IndexError:
        return []
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
    if not m.ok(grids[0], 0x20):
        raise ValueError("collision grid not mapped")
    return grids[0]


def grid_dims(m, g):
    return m.s16(g + 4), m.s16(g + 6), m.f32(g + 8), m.f32(g + 0xC), m.f32(g + 0x10)


def grid_cells(m, g):
    """[(cell index, [(list node, path)], error or None)] for every cell; a list stops at a loop or an unmapped node."""
    nc, nr = m.s16(g + 4), m.s16(g + 6)
    out = []
    for i in range(max(nc, 0) * max(nr, 0)):
        if not m.ok(g + 0x20 + 4 * i, 4):
            out.append((i, [], "cell %d: head not mapped" % i))
            continue
        lst, seen, err, p = [], set(), None, m.u32(g + 0x20 + 4 * i)
        while p:
            if p in seen:
                err = "cell %d: list loops at %s" % (i, addr(m, p))
                break
            if not m.ok(p, 8):
                err = "cell %d: list node %s not mapped" % (i, addr(m, p))
                break
            seen.add(p)
            lst.append((p, m.u32(p + 4)))
            p = m.u32(p)
        out.append((i, lst, err))
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
    if not m.ok(path, 0x24):
        return []
    k = min(max(m.u32(path + 0x1C), 0), 256)
    return [m.u32(path + 0x24 + 4 * i) for i in range(k) if m.ok(path + 0x24 + 4 * i, 4)]


def all_paths(m, g):
    seen = {}
    for i, lst, _err in grid_cells(m, g):
        for _node, p in lst:
            seen.setdefault(p, set()).add(i)
    return seen


def check(m, parent=None):
    """(paths, list nodes, problems) for the grid rule."""
    if parent is None:
        parent = walk_all(m)
    g = find_grid(m, parent)
    problems, owner, listed, nnodes = [], {}, {}, 0
    for i, lst, err in grid_cells(m, g):
        if err:
            problems.append(err)
        here = set()
        for node, p in lst:
            nnodes += 1
            if node in owner:
                problems.append("list node %s in cells %d and %d" % (addr(m, node), owner[node], i))
            owner[node] = i
            if p in here:
                problems.append("path %s twice in cell %d" % (addr(m, p), i))
            here.add(p)
            listed.setdefault(p, set()).add(i)
    for p, cells in sorted(listed.items()):
        if not m.ok(p, 0x24):
            problems.append("path %s not mapped" % addr(m, p))
            continue
        want = covered(m, g, p)
        if cells != want:
            problems.append("path %s: listed in %s, box covers %s" % (addr(m, p), sorted(cells), sorted(want)))
    return len(listed), nnodes, problems


# ---------------------------------------------------------------- pieces

def position(m, n):
    """A piece's translation: TRANSLATE trans (+0x10), ROTATE matrix fourth row (+0x40). With no transform above a
    piece (the piece rule), this is its world position."""
    o = 0x10 if op(m, n) == OP_TRANSLATE else 0x40
    return (m.f32(n + o), m.f32(n + o + 4), m.f32(n + o + 8))


def subtree(m, n):
    """Nodes under n (itself first), preorder, slot order, each once."""
    out, seen, st = [], set(), [n]
    while st:
        k = st.pop()
        if k in seen:
            continue
        seen.add(k)
        out.append(k)
        st.extend(reversed(children(m, k)))
    return out


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
        if not m.ok(n, 0x50 if op(m, n) == OP_ROTATE else 0x1C):
            continue
        root = p
        while parent[root] is not None and op(m, root) not in TRANSFORMING:
            root = parent[root]
        if op(m, root) in TRANSFORMING:
            continue
        nodes = subtree(m, n)
        ops = {op(m, k) for k in nodes}
        if OP_OBJECT not in ops and OP_POLY_COLL not in ops:
            continue
        first_id = next((m.u32(k) >> 18 for k in nodes if m.u32(k) >> 18), 0)
        if (m.u32(root) >> 18) != WORLD_ID and not PICKUP_IDS[0] <= first_id <= PICKUP_IDS[1]:
            continue
        out.append((n, op(m, n), position(m, n), through.get(n, 0), OP_DESTRUCTIBLE in ops))
    return out


# ---------------------------------------------------------------- riders

RIDER_BELOW, RIDER_ABOVE = 20.0, 25.0   # a rider's origin is within [box top - 20, box top + 25]
RIDER_MAX_BOX = 700.0                   # boxes wider than this (in X or Y) carry nothing (CENTRAL's tower: an animated 1000 x 1000)


def is_pickup(m, parent, n):
    """A piece under a root other than the world root: a level pickup (the piece rule's case b)."""
    root = n
    while parent[root] is not None:
        root = parent[root]
    return (m.u32(root) >> 18) != WORLD_ID


def lowest_bottom(m, n):
    """(x, y, z) of the bottom of piece n's lowest OBJECT sphere (center z - radius), in world space; first in preorder on a
    tie; None with no OBJECT. TRANSLATE, ROTATE and SCALE apply (row vectors, p * R + t), ANIM_XFORM and ANIM_SCALE count as
    identity, a NULL ends its branch (the native loader's spheres() walk)."""
    best, seen, st = None, set(), [(n, ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)), (0.0, 0.0, 0.0))]

    def app(p, R, t):
        return tuple(p[0] * R[0][j] + p[1] * R[1][j] + p[2] * R[2][j] + t[j] for j in range(3))
    while st:
        k, R, t = st.pop()
        if k in seen:
            continue
        seen.add(k)
        try:
            o = op(m, k)
            if o == 9:   # NULL
                continue
            if o == OP_OBJECT:
                c = app((m.f32(k + 0x10), m.f32(k + 0x14), m.f32(k + 0x18)), R, t)
                s = max(math.sqrt(sum(v * v for v in row)) for row in R)
                pt = (c[0], c[1], c[2] - m.f32(k + 0x1C) * s)
                if best is None or pt[2] < best[2]:
                    best = pt
                continue
            if o == OP_TRANSLATE:
                t = app((m.f32(k + 0x10), m.f32(k + 0x14), m.f32(k + 0x18)), R, t)
            elif o == OP_ROTATE:
                L = [[m.f32(k + 0x10 + 16 * i + 4 * j) for j in range(3)] for i in range(3)]
                t = app((m.f32(k + 0x40), m.f32(k + 0x44), m.f32(k + 0x48)), R, t)
                R = tuple(tuple(sum(L[i][q] * R[q][j] for q in range(3)) for j in range(3)) for i in range(3))
            elif o == 30:   # SCALE: p * scale, then + trans
                sc = (m.f32(k + 0x20), m.f32(k + 0x24), m.f32(k + 0x28))
                t = app((m.f32(k + 0x10), m.f32(k + 0x14), m.f32(k + 0x18)), R, t)
                R = tuple(tuple(sc[i] * R[i][j] for j in range(3)) for i in range(3))
        except IndexError:
            continue
        for c in reversed(children(m, k)):
            st.append((c, R, t))
    return best


def rider_map(m, parent=None, ps=None):
    """{base piece: [rider pieces, walk order]}. A rider is another piece whose origin (a pickup: its position; a world piece:
    the bottom of its lowest OBJECT, lowest_bottom()) lies inside, in X and Y, one of the base's solid (non-trigger) collision
    path boxes at most RIDER_MAX_BOX wide, with its z in [box top - RIDER_BELOW, box top + RIDER_ABOVE]. A rider on two bases
    goes to the one whose box top is closest to its z (then the lower node). One level only: a rider's riders are not carried."""
    if parent is None:
        parent = walk_all(m)
    if ps is None:
        ps = pieces(m, parent)
    g = find_grid(m, parent)
    boxes = {}
    for p in sorted(all_paths(m, g)):
        if not m.ok(p, 0x24) or m.u32(p + 0x20) & 1:
            continue
        b = [m.f32(p + 4 * i) for i in range(6)]
        if b[3] - b[0] > RIDER_MAX_BOX or b[4] - b[1] > RIDER_MAX_BOX:
            continue
        for n in set(path_nodes(m, p)):
            boxes.setdefault(n, []).append(b)
    origin = {}
    for n, _o, pos, _k, _d in ps:
        origin[n] = pos if is_pickup(m, parent, n) else lowest_bottom(m, n)
    claim = {}
    for base, *_ in ps:
        for b in boxes.get(base, ()):
            for n, pt in origin.items():
                if n == base or pt is None:
                    continue
                if b[0] <= pt[0] <= b[3] and b[1] <= pt[1] <= b[4] and b[5] - RIDER_BELOW <= pt[2] <= b[5] + RIDER_ABOVE:
                    key = (abs(pt[2] - b[5]), base)
                    if n not in claim or key < claim[n]:
                        claim[n] = key
    out = {}
    for n, *_ in ps:
        if n in claim:
            out.setdefault(claim[n][1], []).append(n)
    return out


def riders(m, piece):
    """The riders of one piece (rider_map())."""
    return rider_map(m).get(piece, [])


def kind_of(m, parent, n):
    """'pickup type N' (createPickup's type: the pickup id - 0x1388) or 'world piece'."""
    if not is_pickup(m, parent, n):
        return "world piece"
    first = next((m.u32(k) >> 18 for k in subtree(m, n) if m.u32(k) >> 18), 0)
    return "pickup type %d" % (first - 0x1388)


def rider_edits(m, edits, valid_riders, notes):
    """Edits for the riders of make's edits (stage.json form, offsets): a removed base removes its riders; a moved or turned
    base moves each rider rigidly with it: new = base new + turn(rider - base old), and the rider gets the same turn. A rider
    edited by the user keeps the user's edit (noted). valid_riders: {base offset: [(rider offset, kind)]}."""
    user = {int(e["node"], 16) for e in edits}
    out = []
    for e in edits:
        base = int(e["node"], 16)
        for r, kind in valid_riders.get(base, ()):
            if r in user:
                notes.append("  note: rider 0x%x (%s) of 0x%x keeps your own edit" % (r, kind, base))
                continue
            if any(int(x["node"], 16) == r for x in out):
                continue
            notes.append("  rider 0x%x (%s) follows 0x%x" % (r, kind, base))
            if e.get("remove"):
                out.append({"node": "0x%x" % r, "remove": True})
                continue
            old = position(m, BASE + base)
            new = e.get("move", old)
            deg = e.get("turn", 0)
            a = math.fmod(deg, 360.0) % 360.0
            c, s = {0.0: (1.0, 0.0), 90.0: (0.0, 1.0), 180.0: (-1.0, 0.0), 270.0: (0.0, -1.0)}.get(
                a, (math.cos(math.radians(a)), math.sin(math.radians(a))))
            rp = position(m, BASE + r)
            d = [rp[k] - old[k] for k in range(3)]
            to = [d[0] * c - d[1] * s + new[0], d[0] * s + d[1] * c + new[1], d[2] + new[2]]
            ed = {"node": "0x%x" % r, "move": [round(v, 3) for v in to]}
            if deg:
                ed["turn"] = deg
            out.append(ed)
    return out


# ---------------------------------------------------------------- fight RAM (PS2X_DUMP_RAM)

MONSTERS, NUM_MONSTERS = 0x3D3920, 0x3D3978   # TheGame::m_monsters[], m_numMonsters (native/wotm_syms.h)
PICKUP_LISTS = (0x2AECA0, 0x2AECB0, 0x2AECC0, 0x2AECD0, 0x2AECE0)   # LevelPickups::s_{active,inUse,inFlight,regen,dead}List


def ram_state(ram):
    """([(x, y, z)] per monster, [(x, y, z)] per level pickup) from a guest RAM image. A monster's _cs is at +0xC (trans at
    +0x10); a pickup is a ListNode {data, next, prev} of LevelPickups' lists, and Pickup+0x20 is the position createPickup
    gave Pickup::init (its _cs moves when it is carried; this one does not)."""
    def u32(a):
        return struct.unpack_from("<I", ram, a & 0x3FFFFFF)[0]

    def vec(a):
        return struct.unpack_from("<3f", ram, a & 0x3FFFFFF)
    mons = []
    for i in range(min(u32(NUM_MONSTERS), 8)):
        mon = u32(MONSTERS + 4 * i)
        if not mon:
            raise SystemExit("monster %d: m_monsters[%d] is empty (no fight in this RAM image?)" % (i, i))
        pos = vec(u32(mon + 0xC) + 0x10)
        if pos == (0.0, 0.0, 0.0):
            raise SystemExit("monster %d: position is all zero (fight not set up in this RAM image?)" % i)
        mons.append(pos)
    picks, seen = [], set()
    for head in PICKUP_LISTS:
        node = u32(head + 4)
        while node and node != head and node not in seen and len(seen) < 4096:
            seen.add(node)
            p = u32(node)
            if p:
                picks.append(vec(p + 0x20))
            node = u32(node + 4)
    return mons, picks


def near(a, b, tol=1.0):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def cmd_ram(a):
    with open(a.file, "rb") as f:
        ram = f.read()
    mons, picks = ram_state(ram)
    for i, m in enumerate(mons):
        print("monster %d at %.1f %.1f %.1f" % ((i,) + m))
    print("%d level pickups" % len(picks))
    bad = 0

    def vec3(s):
        v = [float(x) for x in s.split(",")]
        if len(v) != 3:
            raise SystemExit("bad position %r (expected x,y,z)" % s)
        return v
    for spec in a.monster:
        i, lo, hi = spec.split(":")
        i, lo, hi = int(i), float(lo), float(hi)
        if i < 0:
            raise SystemExit("bad monster index %d in %r" % (i, spec))
        ok = i < len(mons) and lo <= mons[i][2] <= hi
        bad += not ok
        print("%s monster %d height in [%g, %g]" % ("ok     " if ok else "FAILED ", i, lo, hi))
    for s in a.pickup:
        ok = any(near(p, vec3(s)) for p in picks)
        bad += not ok
        print("%s pickup placed at %s" % ("ok     " if ok else "FAILED ", s))
    for s in a.no_pickup:
        ok = not any(near(p, vec3(s)) for p in picks)
        bad += not ok
        print("%s no pickup placed at %s" % ("ok     " if ok else "FAILED ", s))
    return 1 if bad else 0


# ---------------------------------------------------------------- commands

def load_mem(path, reserve=RESERVE):
    if not os.path.isfile(path):
        raise OSError("no such file")
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
    try:
        m = load_mem(a.file, a.reserve)
        parent = walk_all(m)
        ps = pieces(m, parent)
        rm = rider_map(m, parent, ps)
    except (ValueError, IndexError, OSError) as e:
        print("%s: %s" % (a.file, e))
        return 1
    for n, o, pos, npaths, destr in ps:
        print("%s  %-9s  pos %.1f %.1f %.1f  paths %d  riders %d%s" % (
            addr(m, n), "TRANSLATE" if o == OP_TRANSLATE else "ROTATE", pos[0], pos[1], pos[2], npaths, len(rm.get(n, ())),
            "  destructible" if destr else ""))
    print("%d pieces" % len(ps), file=sys.stderr)
    return 0


def num(v):
    return int(v) if float(v).is_integer() else v


def make_edits(valid, base, moves, turns, removes):
    """stage.json edits from make's --move 0x..=x,y,z[,turn], --turn 0x..=deg and --remove 0x.. (valid: the base's piece
    offsets). A --turn and a --move of the same node merge into one edit; anything else on a node twice is refused."""
    by_node, order = {}, []

    def node_of(s):
        try:
            node = int(s, 16)
        except ValueError:
            raise ValueError("bad node %r (expected 0x<offset>)" % s)
        if node not in valid:
            raise ValueError("node 0x%x is not a piece of %s (see: stage_map.py pieces)" % (node, base))
        return node

    def edit(node, key):
        e = by_node.get(node)
        if e is None:
            e = by_node[node] = {"node": "0x%x" % node}
            order.append(node)
        if key in e or "remove" in e or (key == "remove" and len(e) > 1):
            raise ValueError("node 0x%x edited twice" % node)
        return e

    def numbers(arg, s, n, what):
        try:
            v = [float(x) for x in s.split(",")]
        except ValueError:
            v = []
        if len(v) not in n or not all(math.isfinite(x) for x in v):
            raise ValueError("bad %s %r (expected %s, finite numbers)" % (arg, s, what))
        return v
    for mv in moves:
        node_s, _, vals = mv.partition("=")
        v = numbers("--move", vals, (3, 4), "0x..=x,y,z[,turn]")
        node = node_of(node_s)
        e = edit(node, "move")
        e["move"] = v[:3]
        if len(v) == 4 and v[3] != 0:
            e["turn"] = num(v[3])
    for t in turns:
        node_s, _, vals = t.partition("=")
        v = numbers("--turn", vals, (1,), "0x..=degrees")
        node = node_of(node_s)
        e = edit(node, "turn")
        if v[0] == 0 and "move" not in e:
            raise ValueError("--turn %r does nothing" % t)
        if v[0] != 0:
            e["turn"] = num(v[0])
    for r in removes:
        edit(node_of(r), "remove")["remove"] = True
    # key order as the loader writes it: node, move, turn / node, remove
    return [{k: by_node[n][k] for k in ("node", "move", "turn", "remove") if k in by_node[n]} for n in order]


def cmd_inflate(a):
    try:
        image = ngp.inflate_ngp(a.ngp)
    except (ValueError, OSError) as e:
        print("%s: %s" % (a.ngp, e))
        return 1
    with open(a.out, "wb") as f:
        f.write(image)
    print("%s: %d bytes, baseHash %016x" % (a.out, len(image), fnv1a(image)))
    return 0


def cmd_make(a):
    def fail(msg):
        print(msg)
        return 1
    if not re.fullmatch(r"[a-z][a-z0-9_]{0,15}", a.id):
        return fail("id %r: a letter first, then [a-z0-9_], at most 16 characters" % a.id)
    name = a.name.upper().strip()
    if not name or len(name) > NAME_LEN or any(c not in NAME_CHARS for c in name):
        return fail("name %r: 1 to %d of A-Z, 0-9, space and -" % (a.name, NAME_LEN))
    base = a.base.upper()
    ngp_path = os.path.join(a.disc if os.path.isabs(a.disc) else os.path.join(REPO, a.disc), "LVL", base + ".NGP")
    if not os.path.isfile(ngp_path):
        return fail("%s: no such file (--base %s, --disc %s)" % (ngp_path, a.base, a.disc))
    image = ngp.inflate_ngp(ngp_path)
    m = Mem(image)
    try:
        parent = walk_all(m)
        ps = pieces(m, parent)
        valid = {n - BASE for n, *_ in ps}
        rm = {} if a.no_riders else rider_map(m, parent, ps)
    except (ValueError, IndexError) as e:
        return fail("%s: %s" % (ngp_path, e))
    try:
        edits = make_edits(valid, base, a.move, a.turn, a.remove)
    except ValueError as e:
        return fail(str(e))
    notes = []
    edits += rider_edits(m, edits, {b - BASE: [(r - BASE, kind_of(m, parent, r)) for r in rs] for b, rs in rm.items()}, notes)
    for line in notes:
        print(line)
    doc = {"name": name, "base": base, "baseHash": "%016x" % fnv1a(image), "edits": edits}
    folder = os.path.join(a.out, a.id)
    os.makedirs(folder, exist_ok=True)
    out = os.path.join(folder, "stage.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, allow_nan=False)
        f.write("\n")
    print("%s: %d edits, baseHash %s" % (out, len(edits), doc["baseHash"]))
    return 0


def cmd_selftest(_a):
    """FNV vectors, then a built image exercising the rules the loader mirrors."""
    assert fnv1a(b"") == 0xCBF29CE484222325
    assert fnv1a(b"a") == 0xAF63DC4C8601EC8C
    img, res = bytearray(0x1000), bytearray(0x200)
    top = {BASE: 0x40, RESERVE: 0x10}

    def at(a):
        return (img, a - BASE) if a < RESERVE else (res, a - RESERVE)

    def w(fmt, a, v):
        b, o = at(a)
        struct.pack_into(fmt, b, o, v)

    def node(o, size, id1=0, where=BASE):
        a = where + top[where]
        top[where] += (size + 15) & ~15
        w("<I", a, id1 << 18 | o)
        return a

    def kids(a, off, ks):
        for i, k in enumerate(ks):
            w("<I", a + off + 4 * i, k)
        return a

    def obj(id1=0):
        return node(0, 4, id1)

    def group(ks, id1=0):
        a = node(1, 0x20 + 4 * len(ks), id1)
        w("<H", a + 8, len(ks))
        return kids(a, 0x20, ks)

    def trans(t, ks, where=BASE):
        a = node(3, 0x1C + 4 * len(ks), where=where)
        w("<I", a + 8, len(ks))
        for i, v in enumerate(t):
            w("<f", a + 0x10 + 4 * i, v)
        return kids(a, 0x1C, ks)

    def rot(t, ks):
        a = node(4, 0x50 + 4 * len(ks))
        w("<I", a + 8, len(ks))
        for i in range(4):
            w("<f", a + 0x10 + 20 * i, 1.0)
        for i, v in enumerate(t):
            w("<f", a + 0x40 + 4 * i, v)
        return kids(a, 0x50, ks)

    def holder(o, count_off, kids_off, ks):   # SCALE (30) / ANIM_XFORM (17) with an identity effect
        a = node(o, kids_off + 4 * len(ks))
        w("<I", a + count_off, len(ks))
        return kids(a, kids_off, ks)

    pc, r1obj = node(OP_POLY_COLL, 4), obj()
    t1 = trans((5, 6, 0), [pc])                                              # piece, on path 1
    r1 = rot((7, 8, 9), [r1obj])                                             # ROTATE piece, on path 2
    scaled = holder(30, 8, 0x2C, [trans((1, 1, 1), [obj()])])                # under a SCALE: not a piece
    animated = holder(17, 4, 0xCC, [trans((2, 2, 2), [obj()])])              # under an ANIM_XFORM: not a piece
    tres = trans((3, 4, 5), [obj()], where=RESERVE)                          # piece in the reserve
    world = group([t1, r1, scaled, animated, tres], WORLD_ID)
    root_t = trans((9, 9, 9), [trans((0, 0, 0), [obj()])])                   # root-level transform and its child: no
    templates = group([trans((0, 35, 0), [obj(18)])])                        # non-world root, not a pickup: no
    pick = trans((50, 60, 70), [group([obj(5001)])])                         # pickup id below: a piece
    pickups = group([pick])
    g = node(OP_GRID, 0x20 + 4 * 8)                                          # 4 x 2 cells of 100, from (-100, 0)
    w("<h", g + 4, 4), w("<h", g + 6, 2)
    for i, v in enumerate((100.0, -100.0, 0.0, 300.0, 200.0)):
        w("<f", g + 8 + 4 * i, v)

    def path(lo, hi, nodes):
        a = node(0, 0x24 + 4 * len(nodes))
        for i, v in enumerate(lo + hi):
            w("<f", a + 4 * i, v)
        w("<I", a + 0x1C, len(nodes))
        return kids(a, 0x24, nodes)
    p1 = path((-160.0, 110.0, 0.0), (450.0, 150.0, 9.0), [world, t1, pc])    # past both X edges: cols 0-3, row 1
    p2 = path((-50.0, 10.0, 0.0), (20.0, 50.0, 9.0), [world, r1, r1obj])     # negative start: cols 0-1, row 0
    for cell, p in ((0, p2), (1, p2), (4, p1), (5, p1), (6, p1), (7, p1)):
        ln = node(0, 8, where=RESERVE if cell == 5 else BASE)                 # cell 5's list node in the reserve
        w("<I", ln + 4, p)
        w("<I", g + 0x20 + 4 * cell, ln)
    roots = [world, root_t, templates, pickups, g]
    w("<I", BASE, len(roots))
    kids(BASE, 4, roots)

    def mem(reserve=True):
        return Mem(bytes(img), [(RESERVE, bytes(res))] if reserve else [])
    m = mem()
    assert covered(m, g, p1) == {4, 5, 6, 7} and covered(m, g, p2) == {0, 1}
    assert check(m) == (2, 6, []), check(m)
    got = [(n, o, pos, k) for n, o, pos, k, _d in pieces(m)]
    assert got == [(t1, 3, (5.0, 6.0, 0.0), 1), (r1, 4, (7.0, 8.0, 9.0), 1), (tres, 3, (3.0, 4.0, 5.0), 0),
                   (pick, 3, (50.0, 60.0, 70.0), 0)], got
    assert addr(m, tres) == "@0x%08x" % tres and addr(m, t1) == "0x%x" % (t1 - BASE)
    assert check(mem(False))[2] and tres not in [p[0] for p in pieces(mem(False))]   # reserve not mapped: no crash
    try:
        pieces(Mem(bytes(img[:0x100])))                                      # truncated dump
    except ValueError:
        pass
    head7 = g + 0x20 + 4 * 7
    b, o = at(head7)
    ln7 = struct.unpack_from("<I", b, o)[0]
    w("<I", head7, 0), w("<I", g + 0x20 + 4 * 3, ln7)                        # cell 3 instead of 7
    assert any("box covers" in p for p in check(mem())[2])
    w("<I", ln7, ln7)                                                        # list node pointing at itself
    assert [p for p in check(mem())[2] if "loops" in p] == ["cell 3: list loops at 0x%x" % (ln7 - BASE)]
    ram = bytearray(0x900000)                                                # fight RAM: 1 monster, 1 pickup
    struct.pack_into("<I", ram, NUM_MONSTERS, 1)
    struct.pack_into("<I", ram, MONSTERS, 0x800000)
    struct.pack_into("<I", ram, 0x80000C, 0x800100)
    struct.pack_into("<3f", ram, 0x800110, 1.0, 2.0, 148.0)
    struct.pack_into("<I", ram, PICKUP_LISTS[0] + 4, 0x800200)                # active list: one node, then null
    struct.pack_into("<I", ram, 0x800200, 0x800300)
    struct.pack_into("<3f", ram, 0x800320, 400.0, -100.0, 11.9)
    for h in PICKUP_LISTS[1:]:
        struct.pack_into("<I", ram, h + 4, h)                                 # empty: points at its own head
    mons, picks = ram_state(bytes(ram))
    assert mons == [(1.0, 2.0, 148.0)] and len(picks) == 1 and near(picks[0], (400, -100, 11.9)), (mons, picks)
    valid = {0x100, 0x200, 0x300}                                             # make's edits
    got = make_edits(valid, "X", ["0x100=1,2,3"], ["0x100=90", "0x200=15"], ["0x300"])
    assert got == [{"node": "0x100", "move": [1.0, 2.0, 3.0], "turn": 90}, {"node": "0x200", "turn": 15},
                   {"node": "0x300", "remove": True}], got
    assert make_edits(valid, "X", ["0x100=1,2,3,45"], [], []) == [{"node": "0x100", "move": [1.0, 2.0, 3.0], "turn": 45}]
    for bad in ((["0x100=1,2,3,45"], ["0x100=90"], []), ([], ["0x100=1", "0x100=2"], []), ([], ["0x100=5"], ["0x100"]),
                ([], ["0x400=5"], []), ([], ["0x100=x"], []), ([], ["0x100=0"], []), (["0x100=1,2"], [], [])):
        try:
            make_edits(valid, "X", *bad)
        except ValueError:
            continue
        raise AssertionError("make_edits accepted %r" % (bad,))

    # riders: a fresh image with two bases (roof tops 100 and 108), a wide box, a trigger box and candidates around them
    img[:] = bytes(len(img))
    res[:] = bytes(len(res))
    top[BASE], top[RESERVE] = 0x40, 0x10

    def sphobj(c, r):
        a = node(OP_OBJECT, 0x20)
        for i, v in enumerate(c + (r,)):
            w("<f", a + 0x10 + 4 * i, v)
        return a
    pcs = [node(OP_POLY_COLL, 4) for _ in range(3)]
    b = trans((100, 50, 0), [sphobj((0, 0, 50), 50), pcs[0]])
    b2 = trans((100, 50, 0), [sphobj((0, 0, 50), 50), pcs[1]])
    tw = trans((250, 100, 0), [sphobj((0, 0, 25), 25), pcs[2]])
    wr = trans((120, 60, 100), [sphobj((0, 0, 10), 10)])                     # world piece, bottom at 100: on b
    wg = trans((60, 30, 0), [sphobj((0, 0, 10), 10)])                        # bottom at 0, under b's roof: no
    world = group([b, b2, tw, wr, wg], WORLD_ID)

    def pk(x, y, z):
        return trans((x, y, z), [group([obj(5001)])])
    pk1, pk2, pk3, pk4, pk5, pk6 = (pk(150, 20, 110), pk(150, 20, 140), pk(170, 20, 100), pk(60, 80, 205),
                                    pk(250, 150, 50), pk(100, 50, 104))
    pickups = group([pk1, pk2, pk3, pk4, pk5, pk6])
    g = node(OP_GRID, 0x20 + 4 * 8)
    w("<h", g + 4, 4), w("<h", g + 6, 2)
    for i, v in enumerate((100.0, -100.0, 0.0, 300.0, 200.0)):
        w("<f", g + 8 + 4 * i, v)
    boxes = [((40.0, 10.0, 0.0), (160.0, 90.0, 100.0), [world, b, pcs[0]], 0),
             ((40.0, 10.0, 0.0), (160.0, 90.0, 108.0), [world, b2, pcs[1]], 0),
             ((40.0, 10.0, 0.0), (160.0, 90.0, 205.0), [world, b, pcs[0]], 1),     # a trigger box: carries nothing
             ((-500.0, 0.0, 0.0), (400.0, 200.0, 50.0), [world, tw, pcs[2]], 0)]   # 900 wide: carries nothing
    head = 0
    for lo, hi, nodes, trig in boxes:
        p = path(lo, hi, nodes)
        w("<I", p + 0x20, trig)
        ln = node(0, 8)
        w("<I", ln, head), w("<I", ln + 4, p)
        head = ln
    w("<I", g + 0x20, head)
    roots = [world, pickups, g]
    w("<I", BASE, len(roots))
    kids(BASE, 4, roots)
    m = mem()
    parent = walk_all(m)
    assert lowest_bottom(m, wr) == (120.0, 60.0, 100.0) and kind_of(m, parent, pk1) == "pickup type 1"
    rm = rider_map(m)
    assert rm == {b: [wr, pk6], b2: [pk1]}, {hex(k): [hex(x) for x in v] for k, v in rm.items()}   # pk6: a tie, the lower node
    assert riders(m, b2) == [pk1] and riders(m, tw) == []
    off = lambda n: n - BASE   # noqa: E731
    valid = {off(k): [(off(r), kind_of(m, parent, r)) for r in v] for k, v in rm.items()}
    notes = []
    got = rider_edits(m, [{"node": "0x%x" % off(b), "move": [110, 50, 5], "turn": 90}, {"node": "0x%x" % off(b2), "remove": True}],
                      valid, notes)
    assert got == [{"node": "0x%x" % off(wr), "move": [100.0, 70.0, 105.0], "turn": 90},
                   {"node": "0x%x" % off(pk6), "move": [110.0, 50.0, 109.0], "turn": 90},
                   {"node": "0x%x" % off(pk1), "remove": True}], got
    assert notes[0] == "  rider 0x%x (world piece) follows 0x%x" % (off(wr), off(b)), notes
    notes = []
    got = rider_edits(m, [{"node": "0x%x" % off(b), "move": [100, 80, 0]}, {"node": "0x%x" % off(pk6), "remove": True}], valid, notes)
    assert got == [{"node": "0x%x" % off(wr), "move": [120.0, 90.0, 100.0]}], got                  # pk6: the user's edit stands
    assert any("keeps your own edit" in n for n in notes), notes
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
    p = sub.add_parser("make", help="write <out>/<id>/stage.json")
    p.add_argument("--base", required=True)
    p.add_argument("--id", required=True)
    p.add_argument("--name", required=True)
    p.add_argument("--out", required=True, help="the stages folder (mods/<mod>/stages); the map goes in <out>/<id>/")
    p.add_argument("--remove", action="append", default=[])
    p.add_argument("--move", action="append", default=[], metavar="0x..=X,Y,Z[,TURN]")
    p.add_argument("--turn", action="append", default=[], metavar="0x..=DEGREES",
                   help="turn about Z without moving; with a --move of the same node, one edit")
    p.add_argument("--no-riders", action="store_true",
                   help="do not carry the riders (pickups and pieces standing on an edited piece's roof) with it")
    p.add_argument("--disc", default="disc", help="disc folder (relative to the repo root unless absolute)")
    p.set_defaults(fn=cmd_make)
    p = sub.add_parser("inflate", help="write a stage .NGP's inflated image (WOTM_TEST_STAGE)")
    p.add_argument("ngp")
    p.add_argument("out")
    p.set_defaults(fn=cmd_inflate)
    p = sub.add_parser("ram", help="monsters and level pickups in a fight's RAM dump (PS2X_DUMP_RAM)")
    p.add_argument("file")
    p.add_argument("--monster", action="append", default=[], metavar="I:ZMIN:ZMAX")
    p.add_argument("--pickup", action="append", default=[], metavar="X,Y,Z")
    p.add_argument("--no-pickup", action="append", default=[], metavar="X,Y,Z")
    p.set_defaults(fn=cmd_ram)
    p = sub.add_parser("selftest")
    p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
