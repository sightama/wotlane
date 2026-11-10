#!/usr/bin/env python3
# ABOUTME: Map creator tool: lists a stage's pieces, makes a map folder (stage.json with baseHash) and checks the collision
# ABOUTME: grid rule (every path listed in exactly the cells its box covers) on a stock .NGP or a RAM dump of an edited stage.
"""Map creator tool (docs/superpowers/specs/2026-11-09-map-creator-design.md, issue #10).

  stage_map.py check <NGP or dump>...      grid rule, path entries mapped and the copy checks (copy_checks()); a dump is
                                           the raw image (bytes from 0xA00000), and a sibling <dump>.reserve, if present,
                                           is mapped at --reserve (default 0x3BFF000); prints "N paths, M list nodes,
                                           K copies, OK" per file
  stage_map.py pieces <NGP or dump>        one line per piece: node, kind, world position, collision paths, riders,
                                           power-ups (indices into <disc>/PTS/<STAGE>.PUP, for an .NGP in <disc>/LVL),
                                           destructible, and the copy column: "copy <bytes> B i<interactives>
                                           d<destructibles> p<pickups>" plus " debris <n> (<bytes> B)" when its
                                           destructibles drop pickup debris, or "no copy (<reason>)" (copy_set()); node is
                                           the offset from 0xA00000, or @<absolute address> outside the image
  stage_map.py make --base CENTRAL --id <id> --name "<NAME>" --out <stages folder> [--remove 0x..]...
                    [--move 0x..=x,y,z[,turn]]... [--turn 0x..=degrees]... [--copy 0x..=x,y,z[,turn]]... [--no-riders]
                                           writes <stages folder>/<id>/stage.json (--turn and --move on the same node make
                                           one edit), plus an edit per rider of an edited piece (rider_map(),
                                           rider_edits()) and per power-up place on it (powerup_riders(), from the disc's
                                           PTS/<BASE>.PUP when there is one), printed as it is added; per --copy the copy
                                           and its riders' copies, and the power-up places on the copied pieces added
                                           (make_copies()), and the loader's cap estimates (copy_estimate()). The file
                                           lists them in the loader's order (file_order()): node edits and their riders',
                                           then the copies (per --copy: the copy, its riders'), then the power-up moves,
                                           then the power-up additions
  stage_map.py inflate <NGP> <out>         writes the inflated stage image (what the game has at 0xA00000), e.g. for
                                           the native test's WOTM_TEST_STAGE
  stage_map.py ram <ram_<frame>.bin> [--monster I:ZMIN:ZMAX]... [--pickup X,Y,Z]... [--no-pickup X,Y,Z]...
                   [--powerup X,Y,Z]... [--no-powerup X,Y,Z]...
                                           a fight's RAM (PS2X_DUMP_RAM): the monsters' positions and the level pickups'
                                           placed positions (what createPickup was given); exits 1 when monster I's height is
                                           outside [ZMIN, ZMAX], or a pickup is missing at (--pickup) or present at
                                           (--no-pickup) a place, or a power-up (X, Y and its rest height) likewise, within 1
                                           unit (the map creator's live checks)
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
Copies (copy_set(), stage_counts(); native copySet(), stageCounts(); spec 2026-11-09-map-copy-design.md):
- A piece is refused, first reason in this order: on its first-parent chain a DESTRUCTIBLE or INTERACTIVE_STATE ("in a
  destructible's state"), a GROUP with flags & 0xB0 ("mode-masked"), an ANIM_XFORM, ANIM_SCALE or SKEL_BONE ("animated");
  in its subtree an ANIM_XFORM, CHAR_INSTANCE, SKEL_BONE or ANIM_SCALE; a CHAR_INSTANCE above it while an OBJECT of it reads
  an animation channel (an s16 >= 0 at +0x30..+0x3C); in its DESTRUCTIBLEs' state actions an ACT_ANIM, a script action
  (ACT_CINEMA, ACT_PLAYERPOS, ACT_RUNSCRIPT, ACT_AINAVIGATION, ACT_GENERICEVENT) or a tree past 20 levels or 256 actions in a
  group; then per node in preorder: an INTERACTIVE_STATE or DESTRUCTIBLE with a fixed-table id1, a dispatched kind (ops 0,
  1, 2, 3, 4, 6, 11, 17, 30, 40, 41) with a special id1, an emitter with a locator id (0x1D4C..0x1D6B); a shared node
  inside it holding a copied one; a copied node whose size node_size() cannot give.
- Copy set: the root, the per-instance nodes (INTERACTIVE_STATE, DESTRUCTIBLE, SWITCH, PARTICLE_EMITTER, SCRIPT_ACTION, a
  dispatched kind with a level pickup id1) and their first-parent ancestors inside the piece, in preorder; bytes = their
  16-aligned sizes. Counts per copy, each node once per path from the root to it (path_counts()): Interactives
  (INTERACTIVE_STATE, level pickup nodes), Destructibles (DESTRUCTIBLE, dispatched nodes with id1 0x1450..0x1B57), level
  pickups, and debris pickups (ACT_DEBRIS chunks with a pickup type, per state reaching them, 32 chunks at most) with their
  memory (debris_pickup_bytes()).
- Stage counts: the same over every node reachable from the header roots, plus HomingBugs (0x11F8, 0x1203) as Interactives.
Riders (rider_map(), make only; the loader applies them as ordinary edits): a piece whose origin (a pickup: its position; a
world piece: the bottom of its lowest OBJECT) lies in X and Y inside one of another piece's solid collision boxes at most 700
wide, at z from the box top - 20 to the box top + 25; on two pieces, the one whose box top is nearest, then the lower node.
Power-up places ride the same way with z up to the box top + 100 (they hover), as {"powerup": i, "from", "at"} edits that the
runtime writes over PowerUpTool's points (native/runtime/wotm_stagemap.inc); of a removed piece they drop to its ground.
"""
import argparse
import glob
import json
import math
import os
import re
import shutil
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ngp  # noqa: E402

BASE = 0xA00000
RESERVE = 0x4000000 - 0x401000   # kReserve: PS2_RAM_SIZE - 0x401000, 4 MB
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


def wrappers(m):
    """[(header root, wrapper)]: the loader's copy wrappers, a GROUP outside the image in a header GROUP root's last child slot
    (a step 1 turn may leave a reserve ROTATE there: not a wrapper)."""
    out, seen = [], set()
    for r in m.roots():
        if not r or r in seen or not m.ok(r, 0x20) or op(m, r) != OP_GROUP:
            continue
        seen.add(r)
        k = m.u16(r + 8)
        if not k or not m.ok(r + 0x20 + 4 * (k - 1), 4):
            continue
        w = m.u32(r + 0x20 + 4 * (k - 1))
        if w and not m.in_image(w) and m.ok(w, 0x20) and op(m, w) == OP_GROUP:
            out.append((r, w))
    return out


def copy_checks(m, parent):
    """(copies, problems): every per-instance node (per_instance()) reachable from the header roots has one holder slot, and
    nothing in a copy (a wrapper's children after its first, the slot's old child) reaches an image per-instance node or is a
    shared image node holding a node outside the image (review focus 1). copies = the wrappers' numKids - 1, summed."""
    problems, holders = [], {}
    for n in parent:
        for k in children(m, n):
            if per_instance(m, k):
                holders[k] = holders.get(k, 0) + 1
    for k, c in sorted(holders.items()):
        if c > 1:
            problems.append("per-instance node %s held by %d child slots" % (addr(m, k), c))
    ncopies = 0
    for _r, w in wrappers(m):
        k = m.u16(w + 8)
        ncopies += max(k - 1, 0)
        for i in range(1, min(k, 4096)):
            if not m.ok(w + 0x20 + 4 * i, 4):
                problems.append("wrapper %s: slot %d not mapped" % (addr(m, w), i))
                break
            root = m.u32(w + 0x20 + 4 * i)
            for x in subtree(m, root) if root and m.ok(root, 4) else ():
                for kid in children(m, x):
                    if m.in_image(kid) and per_instance(m, kid):
                        problems.append("copy at %s points at per-instance node %s" % (addr(m, x), addr(m, kid)))
                    elif m.in_image(x) and not m.in_image(kid):
                        problems.append("shared node %s in copy %s holds copied node %s" % (addr(m, x), addr(m, root),
                                                                                          addr(m, kid)))
    return ncopies, problems


def check(m, parent=None):
    """(paths, list nodes, problems, copies): the grid rule, every listed path's entries mapped, and copy_checks()."""
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
        k = min(m.u32(p + 0x1C), 256)
        for i in range(k):
            e = p + 0x24 + 4 * i
            if not m.ok(e, 4) or not m.ok(m.u32(e), 4):
                problems.append("path %s: entry %d (%s) not mapped" % (addr(m, p), i,
                                                                       addr(m, m.u32(e)) if m.ok(e, 4) else "slot"))
    ncopies, more = copy_checks(m, parent)
    return len(listed), nnodes, problems + more, ncopies


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
    for n in parent:
        if is_piece(m, parent, n):
            ops = {op(m, k) for k in subtree(m, n)}
            out.append((n, op(m, n), position(m, n), through.get(n, 0), OP_DESTRUCTIBLE in ops))
    return out


def is_piece(m, parent, n):
    """The piece rule for one node (the module docstring's "Piece"; native isPiece())."""
    if parent.get(n) is None or not m.ok(n, 4):
        return False
    o = op(m, n)
    if o not in (OP_TRANSLATE, OP_ROTATE) or not m.ok(n, 0x50 if o == OP_ROTATE else 0x1C):
        return False
    root = parent[n]
    while parent[root] is not None and op(m, root) not in TRANSFORMING:
        root = parent[root]
    if op(m, root) in TRANSFORMING:
        return False
    nodes = subtree(m, n)
    if not any(op(m, k) in (OP_OBJECT, OP_POLY_COLL) for k in nodes):
        return False
    first_id = next((m.u32(k) >> 18 for k in nodes if m.u32(k) >> 18), 0)
    return (m.u32(root) >> 18) == WORLD_ID or PICKUP_IDS[0] <= first_id <= PICKUP_IDS[1]


# ---------------------------------------------------------------- copies (native copySet(), stageCounts())

OP_GROUP, OP_LOD, OP_SWITCH, OP_CONTROL, OP_NULL = 1, 2, 6, 8, 9
OP_ANIM_XFORM, OP_EMITTER, OP_ACTION_DATA, OP_CHAR_INSTANCE, OP_SCALE = 17, 22, 23, 25, 30
OP_SKEL_BONE, OP_ANIM_SCALE, OP_INTERACTIVE, OP_SCRIPT_ACTION = 35, 38, 39, 41
ACT_GROUP, ACT_TRANSFORM, ACT_ANIM, ACT_DEBRIS = 0, 1, 2, 3
SCRIPT_ACTIONS = {9: "ACT_CINEMA", 11: "ACT_PLAYERPOS", 12: "ACT_RUNSCRIPT", 13: "ACT_AINAVIGATION", 14: "ACT_GENERICEVENT"}
DISPATCHED = (0, 1, 2, 3, 4, 6, 11, 17, 30, 40, 41)   # kinds dbsTraverse hands to dbProcInteractive with a nonzero id1
DEBRIS_CHUNKS = 32
INTERACTIVES_CAP, DESTRUCTIBLES_CAP, MONSTER_SLOTS = 1000, 1400, 8   # kInteractivesCap, kDestructiblesCap, kMonsterSlots
PICKUP_BYTES = 0x500                                                  # kPickupBytes: MemoryStack::global per level pickup
MAX_POINTS, MAX_POWER_UPS = 256, 100                                  # PointToolKit::points[256]; PowerUps::m_powerUps[100]
MAX_EDITS = 4096                                                      # stage.json's "edits", all kinds together
COUNT_MAX = 0x7FFFFFFF                                # addCount() saturates here
OP_NAMES = {0: "OBJECT", 1: "GROUP", 2: "LOD", 3: "TRANSLATE", 4: "ROTATE", 6: "SWITCH", 8: "CONTROL", 9: "NULL", 15: "GRID",
            17: "ANIM_XFORM", 22: "PARTICLE_EMITTER", 23: "ACTION_DATA", 25: "CHAR_INSTANCE", 30: "SCALE", 31: "DESTRUCTIBLE",
            35: "SKEL_BONE", 38: "ANIM_SCALE", 39: "INTERACTIVE_STATE", 41: "SCRIPT_ACTION", 42: "POLY_COLL"}


def _z32(m, a):
    """u32 at a, 0 when not mapped (the native Mem's reads)."""
    return m.u32(a) if m.ok(a, 4) else 0


def _z16(m, a):
    return m.u16(a) if m.ok(a, 2) else 0


def _off(a):
    """The native loader's hex(a - kBase) in reasons."""
    return "0x%x" % ((a - BASE) & 0xFFFFFFFF)


def with_article(o):
    """"a SKEL_BONE", "an ANIM_XFORM": the kind's name with its article (native withArticle())."""
    name = OP_NAMES.get(o, "unknown")
    return ("an " if name[0] in "AEIOU" else "a ") + name


def node_size(m, n):
    """Bytes of a copy-set node (include/hieri_types.h); 0 for a kind a copy cannot hold, a node not wholly mapped, or a child
    count past what children() reads (4096; a LOD's 16): such a piece is refused, not copied in part."""
    if not m.ok(n, 4):
        return 0

    def kids(base, count, each):
        return base + each * count if count <= 4096 else 0
    o = op(m, n)
    if o == OP_GROUP:
        size = kids(0x20, m.u16(n + 8), 4) if m.ok(n + 8, 2) else 0
    elif o == OP_TRANSLATE:
        size = kids(0x1C, m.u32(n + 8), 4) if m.ok(n + 8) else 0
    elif o == OP_ROTATE:
        size = kids(0x50, m.u32(n + 8), 4) if m.ok(n + 8) else 0
    elif o == OP_SCALE:
        size = kids(0x2C, m.u32(n + 8), 4) if m.ok(n + 8) else 0
    elif o == OP_SWITCH:
        size = 0xC + 4 * m.u8(n + 0xB) if m.ok(n + 0xB, 1) else 0
    elif o == OP_LOD:
        size = 0x20 + 16 * m.u32(n + 4) if m.ok(n + 4) and m.u32(n + 4) < 16 else 0
    elif o == OP_CONTROL:
        size = 0x20 + 4 * m.u8(n + 0xC) if m.ok(n + 0xC, 1) else 0
    elif o == OP_INTERACTIVE:
        size = 0x10
    elif o == OP_DESTRUCTIBLE:
        size = kids(0xC, m.u16(n + 0xA), 12) if m.ok(n + 0xA, 2) else 0
    elif o == OP_EMITTER:
        size = 0xB0
    elif o == OP_SCRIPT_ACTION:
        size = 0x60
    else:
        return 0
    return size if size and m.ok(n, size) else 0


def debris_pickup_bytes(t):
    """MemoryStack::global bytes LevelPickups::createPickup takes for a pickup of type t (id1 t + 0x1388): its object and its
    list node, 16-aligned; 0 for a type that creates nothing (native debrisPickupBytes())."""
    i = t + 0x1388
    if i in (0x1388, 0x138D):
        return 0
    size = {0x1391: 0x2A0, 0x1392: 0x2B0, 0x1393: 0x300, 0x1394: 0x300, 0x139F: 0x300, 0x13A0: 0x1D0, 0x13A1: 0x230,
            0x1395: 0x4C0, 0x1399: 0x4C0, 0x139A: 0x4C0, 0x139B: 0x4C0, 0x139C: 0x4C0, 0x139D: 0x4C0, 0x139E: 0x4C0,
            0x138A: 0x270, 0x138B: 0x270, 0x1397: 0x270, 0x13A2: 0x270, 0x13A3: 0x250}.get(i, 0x170)
    return size + 0x10


def actions_under(m, a, depth, seen, out, cut):
    """Every action under a (ACT_GROUP, ACT_TRANSFORM; 20 levels, 256 per group), each once, depth first in slot order; cut[0]
    set when a limit leaves part of the tree unread (native actionsUnder())."""
    if not a or a in seen:
        return
    if depth > 20:
        cut[0] = True
        return
    if not m.ok(a, 8):
        return
    seen.add(a)
    out.append(a)
    t = m.u16(a)
    if t == ACT_GROUP:
        cut[0] = cut[0] or _z32(m, a + 8) > 256
        for i in range(min(_z32(m, a + 8), 256)):
            if not m.ok(a + 0xC + 4 * i, 4):
                break
            actions_under(m, m.u32(a + 0xC + 4 * i), depth + 1, seen, out, cut)
    elif t == ACT_TRANSFORM:
        actions_under(m, _z32(m, a + 8), depth + 1, seen, out, cut)


def _state_slots(m, n):
    """Addresses of DESTRUCTIBLE n's states' action pointers (+0xC + 12 i + 4), while mapped."""
    out = []
    for i in range(_z16(m, n + 0xA)):
        if not m.ok(n + 0xC + 12 * i + 4, 4):
            break
        out.append(n + 0xC + 12 * i + 4)
    return out


def state_actions(m, n, cut):
    """The actions of DESTRUCTIBLE n's states, each once, in state and slot order (native stateActions())."""
    out = []
    if not m.ok(n, 0xC) or op(m, n) != OP_DESTRUCTIBLE:
        return out
    seen = set()
    for s in _state_slots(m, n):
        actions_under(m, m.u32(s), 0, seen, out, cut)
    return out


def debris_of(m, n):
    """(chunks, bytes): DESTRUCTIBLE n's debris pickups, per state that reaches each ACT_DEBRIS (amendment A4; native
    debrisOf())."""
    count = nbytes = 0
    if not m.ok(n, 0xC) or op(m, n) != OP_DESTRUCTIBLE:
        return 0, 0
    for s in _state_slots(m, n):
        acts = []
        actions_under(m, m.u32(s), 0, set(), acts, [False])
        for a in acts:
            if m.u16(a) != ACT_DEBRIS or not m.ok(a, 0x10):
                continue
            for c in range(min(m.u32(a + 8), DEBRIS_CHUNKS)):
                ch = a + 0x10 + 0x30 * c
                if not m.ok(ch, 0x30):
                    break
                t = (m.u16(ch + 0xE) >> 5) & 31
                if t:
                    count += 1
                    nbytes += debris_pickup_bytes(t)
    return count, nbytes


def path_counts(m, starts):
    """{node: how many traversal paths from `starts` reach it} (dbsTraverse has no visited set), saturating at 2^40; nodes on a
    cycle keep only the paths from outside it (native pathCounts())."""
    kmax = 1 << 40
    count, holders, kids, stack = {}, {}, {}, []
    for s in starts:
        if s and m.ok(s, 4):
            count[s] = count.get(s, 0) + 1
            stack.append(s)
    while stack:
        n = stack.pop()
        if n in kids:
            continue
        kids[n] = children(m, n)
        for k in kids[n]:
            holders[k] = holders.get(k, 0) + 1
            if k not in kids:
                stack.append(k)
    ready = [n for n in kids if n not in holders]
    while ready:
        n = ready.pop()
        c = count.get(n, 0)
        for k in kids[n]:
            count[k] = min(count.get(k, 0) + c, kmax)
            holders[k] -= 1
            if holders[k] == 0:
                ready.append(k)
    return count


def _debris_add(m, n, times, acc):
    """acc [count, bytes] += DESTRUCTIBLE n's debris pickups `times` over, saturating (native addDebris())."""
    c, b = debris_of(m, n)
    acc[0] = min(acc[0] + min(c, 1 << 20) * times, COUNT_MAX)
    acc[1] = min(acc[1] + min(b, 1 << 30) * min(times, 1 << 20), 1 << 40)


def special_range(i):
    """dbProcInteractive's ranges a copy must not register twice (src/game/db.cpp:187-255), None for a free id."""
    if i == 0:
        return None
    for lo, hi, name in ((1, 0x1F, "world, sky or font"), (0x20, 0x400, "monsters"), (0xBB8, 0xBB8, "pedestrians"),
                         (0xFA0, 0x11F7, "weapons"), (0x11F8, 0x12BF, "effects"), (0x13EC, 0x144F, "power-ups"),
                         (0x1B58, 0x1C1F, "particle textures"), (0x1C20, 0x2133, "HUD"), (0x2134, 0x2327, "scripts"),
                         (0x2328, 0x3E7F, "end points")):
        if lo <= i <= hi:
            return name
    return None


def fixed_table(i):
    """id1 tables the game fills from INTERACTIVE_STATE and DESTRUCTIBLE ids (Destructible.cpp:161-169, :287-315)."""
    if 0x275D <= i <= 0x2762:
        return "SanFran saves"
    if 0x2775 <= i <= 0x2777:
        return "CANYON2 pillars"
    if 0x27AE <= i <= 0x27B3:
        return "CAPITOL pillars"
    if i == 0x2712 or 0x2715 <= i <= 0x2718:
        return "the 3-MILE vent and stacks"
    return None


def reads_channel(m, n):
    """An OBJECT that reads an animation channel (an index >= 0 at +0x30..+0x3C); too short to tell counts as reading one."""
    if not m.ok(n, 0x3E):
        return True
    return any(m.s16(n + 0x30 + 2 * k) >= 0 for k in range(7))


def per_instance(m, n):
    """A node each copy gets its own of (decision 4): INTERACTIVE_STATE, DESTRUCTIBLE, SWITCH, PARTICLE_EMITTER,
    SCRIPT_ACTION, and a dispatched kind with a level pickup id1."""
    if not m.ok(n, 4):
        return False
    w = m.u32(n)
    o, i = w & 63, w >> 18
    return o in (OP_INTERACTIVE, OP_DESTRUCTIBLE, OP_SWITCH, OP_EMITTER, OP_SCRIPT_ACTION) or (
        o in DISPATCHED and PICKUP_IDS[0] <= i <= PICKUP_IDS[1])


class CopySet:
    """What copying a piece takes (native CopySet): `why` "" when it can be copied, else the reason; `nodes` the copy set in
    preorder; `bytes` their 16-aligned sizes; per copy: Interactives, Destructibles, level pickups, debris pickups and their
    memory; `option_group` a GROUP above with collision-option flags (0: none)."""
    __slots__ = ("why", "nodes", "bytes", "interactives", "destructibles", "pickups", "debris_pickups", "debris_bytes",
                 "option_group")

    def __init__(self, why=""):
        self.why, self.nodes, self.bytes = why, [], 0
        self.interactives = self.destructibles = self.pickups = self.debris_pickups = self.debris_bytes = 0
        self.option_group = 0

    def column(self):
        """pieces' copy column: "copy B B iI dD pP" (plus " debris N (M B)" with debris pickups), or "no copy (why)"."""
        if self.why:
            return "no copy (%s)" % self.why
        return "copy %d B i%d d%d p%d%s" % (self.bytes, self.interactives, self.destructibles, self.pickups,
                                             " debris %d (%d B)" % (self.debris_pickups, self.debris_bytes)
                                             if self.debris_pickups else "")


def copy_set(m, parent, n):
    """The native copySet(), rule for rule and in the same order (so the same first reason): see "Copies" in the module
    docstring."""
    c = CopySet()
    if not is_piece(m, parent, n):
        return CopySet("not a piece")
    char_above = 0
    a = parent[n]
    while a is not None:
        o = op(m, a)
        if o in (OP_DESTRUCTIBLE, OP_INTERACTIVE):
            return CopySet("in a destructible's state (%s %s above it)" % (OP_NAMES[o], _off(a)))
        if o == OP_GROUP:
            flags = _z16(m, a + 0xA)
            if flags & 0xB0:
                return CopySet("mode-masked (GROUP %s above it, flags 0x%x)" % (_off(a), flags))
            if flags & 0xFF00 and not c.option_group:
                c.option_group = a
        if o == OP_CHAR_INSTANCE and not char_above:
            char_above = a
        # ANIM_XFORM and ANIM_SCALE are in TRANSFORMING, so is_piece() already refuses any node below one: kept here as a
        # defence. SKEL_BONE does not transform; this is what refuses it.
        if o in (OP_ANIM_XFORM, OP_ANIM_SCALE, OP_SKEL_BONE):
            return CopySet("animated (%s above it)" % with_article(o))
        a = parent[a]
    # the subtree in preorder with each node's first parent inside it
    order, up, stack = [], {}, [(n, 0)]
    while stack:
        k, p = stack.pop()
        if k in up:
            continue
        up[k] = p
        order.append(k)
        for kid in reversed(children(m, k)):
            stack.append((kid, k))
    channel = False
    for k in order:
        o = op(m, k)
        if o in (OP_ANIM_XFORM, OP_CHAR_INSTANCE, OP_SKEL_BONE, OP_ANIM_SCALE):
            return CopySet("animated (%s in it)" % with_article(o))
        channel = channel or (o == OP_OBJECT and reads_channel(m, k))
    if char_above and channel:
        return CopySet("animated (the CHAR_INSTANCE %s above it drives its OBJECTs' channels)" % _off(char_above))
    for k in order:
        if op(m, k) != OP_DESTRUCTIBLE:
            continue
        cut = [False]
        for act in state_actions(m, k, cut):
            t = m.u16(act)
            if t == ACT_ANIM:
                return CopySet("animated (an ACT_ANIM action)")
            if t in SCRIPT_ACTIONS:
                return CopySet("a script action (%s)" % SCRIPT_ACTIONS[t])
        if cut[0]:
            return CopySet("an action tree past 20 levels or 256 actions in a group (%s)" % _off(k))
    # ids, per-instance nodes and what each copy registers (once per path from the root to a node)
    keep = {n}
    paths = path_counts(m, [n])
    debris = [0, 0]

    def add(field, v):
        setattr(c, field, min(getattr(c, field) + v, COUNT_MAX))
    for k in order:
        w = m.u32(k)
        o, i = w & 63, w >> 18
        times = max(paths.get(k, 0), 1)
        own = o in (OP_SWITCH, OP_EMITTER, OP_SCRIPT_ACTION)
        if o in (OP_INTERACTIVE, OP_DESTRUCTIBLE):
            t = fixed_table(i)
            if t:
                return CopySet("holds id1 0x%x (a fixed id1 table: %s)" % (i, t))
            own = True
            add("interactives" if o == OP_INTERACTIVE else "destructibles", times)
            if o == OP_DESTRUCTIBLE:
                _debris_add(m, k, times, debris)
        elif o in DISPATCHED and i:
            r = special_range(i)
            if r:
                return CopySet("holds special id 0x%x (%s)" % (i, r))
            if PICKUP_IDS[0] <= i <= PICKUP_IDS[1]:
                own = True
                add("interactives", times)
                add("pickups", times)
            elif 0x1450 <= i < 0x1B58:
                add("destructibles", times)
        if o == OP_EMITTER and 0x1D4C <= i <= 0x1D6B:
            return CopySet("an emitter with locator id 0x%x" % i)
        x = k
        while own and x and x not in keep:
            keep.add(x)
            x = up[x]
    # a shared node of the piece holding a copied one would let the copy reach the original's per-instance nodes
    for k in order:
        if k not in keep:
            for kid in children(m, k):
                if kid in keep:
                    return CopySet("a shared node inside it (%s) holds a copied node (%s)" % (_off(k), _off(kid)))
    for k in order:
        if k in keep:
            size = node_size(m, k)
            if not size:
                o = op(m, k)
                return CopySet("its copy would hold a node of kind %d (%s)" % (o, OP_NAMES.get(o, "unknown")))
            c.nodes.append(k)
            c.bytes += (size + 15) & ~15
    c.debris_pickups, c.debris_bytes = debris
    return c


class StageCounts:
    """The stage's own entries (native StageCounts): Interactives (INTERACTIVE_STATE + level pickup + HomingBug nodes),
    Destructibles (DESTRUCTIBLE + dispatched nodes with a destructible id1), level pickups, debris pickups and their memory; each
    node once per traversal path from the header roots."""
    __slots__ = ("interactives", "destructibles", "pickups", "debris_pickups", "debris_bytes")

    def __init__(self):
        self.interactives = self.destructibles = self.pickups = self.debris_pickups = self.debris_bytes = 0

    def as_tuple(self):
        return (self.interactives, self.destructibles, self.pickups, self.debris_pickups, self.debris_bytes)


def stage_counts(m, parent):
    """The native stageCounts() of the image in m (call it before copies are linked in: it would count them too)."""
    s = StageCounts()
    paths = path_counts(m, m.roots())
    debris = [0, 0]

    def add(field, v):
        setattr(s, field, min(getattr(s, field) + v, COUNT_MAX))
    for n in parent:
        w = _z32(m, n)
        o, i = w & 63, w >> 18
        times = max(paths.get(n, 0), 1)
        if o == OP_INTERACTIVE:
            add("interactives", times)
        elif o == OP_DESTRUCTIBLE:
            add("destructibles", times)
            _debris_add(m, n, times, debris)
        elif o in DISPATCHED and i:
            if PICKUP_IDS[0] <= i <= PICKUP_IDS[1]:
                add("interactives", times)
                add("pickups", times)
            elif i in (0x11F8, 0x1203):
                add("interactives", times)
            elif 0x1450 <= i < 0x1B58:
                add("destructibles", times)
    s.debris_pickups, s.debris_bytes = debris
    return s


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


def rider_boxes(m, g):
    """{node: [box [x0, y0, z0, x1, y1, z1]]}: the solid (non-trigger) collision path boxes at most RIDER_MAX_BOX wide through
    each node, in path address order."""
    boxes = {}
    for p in sorted(all_paths(m, g)):
        if not m.ok(p, 0x24) or m.u32(p + 0x20) & 1:
            continue
        b = [m.f32(p + 4 * i) for i in range(6)]
        if b[3] - b[0] > RIDER_MAX_BOX or b[4] - b[1] > RIDER_MAX_BOX:
            continue
        for n in set(path_nodes(m, p)):
            boxes.setdefault(n, []).append(b)
    return boxes


def rider_map(m, parent=None, ps=None):
    """{base piece: [rider pieces, walk order]}. A rider is another piece whose origin (a pickup: its position; a world piece:
    the bottom of its lowest OBJECT, lowest_bottom()) lies inside, in X and Y, one of the base's solid (non-trigger) collision
    path boxes at most RIDER_MAX_BOX wide, with its z in [box top - RIDER_BELOW, box top + RIDER_ABOVE]. A rider on two bases
    goes to the one whose box top is closest to its z (then the lower node). One level only: a rider's riders are not carried."""
    if parent is None:
        parent = walk_all(m)
    if ps is None:
        ps = pieces(m, parent)
    boxes = rider_boxes(m, find_grid(m, parent))
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


# Power-up places (PTS/<STAGE>.PUP; PowerUpTool::loadPoints -> exportPoints -> PowerUps::CreatePowerUp): a power-up hovers at its
# point's z for good (its first placement drops only the shadow, PowerUp::placeShadow), on the stock stages 0 to about 95 above
# the surface under it, most 10 to 90. So a power-up rides with a window above the box top of 100 instead of RIDER_ABOVE.
PUP_VERSION, PUP_SIZE = 0x12282000, 64   # file: version, count, then count 64-byte points (position at +0, type +0x2C, arg +0x2D)
PUP_ABOVE = 100.0


def read_pup(path):
    """[(x, y, z)] of a .PUP's points; None when there is no such file. ValueError on a bad file."""
    if not os.path.isfile(path):
        return None
    with open(path, "rb") as f:
        d = f.read()
    if len(d) < 8:
        raise ValueError("%s: too short for a point file" % path)
    ver, n = struct.unpack_from("<II", d, 0)
    if ver != PUP_VERSION or n > 256 or len(d) < 8 + PUP_SIZE * n:
        raise ValueError("%s: not a point file (version %08x, %d points, %d bytes)" % (path, ver, n, len(d)))
    return [struct.unpack_from("<3f", d, 8 + PUP_SIZE * i) for i in range(n)]


def powerup_riders(m, points, parent=None, ps=None):
    """{base piece: [(index, (x, y, z), box)]} for the power-up places `points` (read_pup()). The piece rider rule (rider_map())
    with the power-up window: a point rides piece P when it lies, in X and Y, inside one of P's solid collision boxes at most
    RIDER_MAX_BOX wide, with its z in [box top - RIDER_BELOW, box top + PUP_ABOVE]; on two pieces, the one whose box top is
    nearest its z, then the lower node. `box` is the box that carries it."""
    if parent is None:
        parent = walk_all(m)
    if ps is None:
        ps = pieces(m, parent)
    boxes = rider_boxes(m, find_grid(m, parent))
    claim = {}
    for base, *_ in ps:
        for b in boxes.get(base, ()):
            for i, pt in enumerate(points):
                if b[0] <= pt[0] <= b[3] and b[1] <= pt[1] <= b[4] and b[5] - RIDER_BELOW <= pt[2] <= b[5] + PUP_ABOVE:
                    key = (abs(pt[2] - b[5]), base)
                    if i not in claim or key < claim[i][0]:
                        claim[i] = (key, b)
    out = {}
    for i in sorted(claim):
        (_d, base), b = claim[i]
        out.setdefault(base, []).append((i, tuple(points[i]), b))
    return out


def kind_of(m, parent, n):
    """'pickup type N' (createPickup's type: the pickup id - 0x1388) or 'world piece'."""
    if not is_pickup(m, parent, n):
        return "world piece"
    first = next((m.u32(k) >> 18 for k in subtree(m, n) if m.u32(k) >> 18), 0)
    return "pickup type %d" % (first - 0x1388)


def f32(v):
    """v rounded to a float, as the loader holds it."""
    return struct.unpack("<f", struct.pack("<f", v))[0]


def f32_text(v):
    """The shortest number that reads back as the same float (what the loader's write() puts in a file)."""
    v = f32(v)
    for p in range(1, 10):
        x = float("%.*g" % (p, v))
        if f32(x) == v:
            return num(x)
    return v


def turn_of(deg):
    """(cos, sin) of a turn in degrees, as the loader's turnOf(): right angles exact; the rest from the float angle in radians."""
    a = math.fmod(deg, 360.0)
    if a < 0:
        a += 360.0
    if a in (0.0, 90.0, 180.0, 270.0):
        return {0.0: (1.0, 0.0), 90.0: (0.0, 1.0), 180.0: (-1.0, 0.0), 270.0: (0.0, -1.0)}[a]
    rad = f32(f32(a) * f32(3.14159265358979 / 180.0))
    return f32(math.cos(rad)), f32(math.sin(rad))


def clamp_to_grid(m, g, node_paths, old, to, c, s, turning, shifts=None):
    """Where the loader's planMove() puts a moved or turned piece: its paths' boxes (the old corners relative to the old
    place, turned, at the new place) are kept inside the grid's stored extent by shifting the place in X and Y. Returns the
    place (floats) and whether it was shifted (as the loader: a shift too small to change the float still counts); `shifts`,
    when given, gets the X and Y shifts."""
    to = [f32(v) for v in to]

    def boxes():
        out = []
        for p in node_paths:
            lo = [m.f32(p + 4 * k) for k in range(3)]
            hi = [m.f32(p + 0xC + 4 * k) for k in range(3)]
            if not turning:
                out.append(([f32(lo[k] + (to[k] - old[k])) for k in range(3)], [f32(hi[k] + (to[k] - old[k])) for k in range(3)]))
                continue
            mn, mx = [math.inf] * 3, [-math.inf] * 3
            for corner in range(8):
                q = [(hi if corner >> k & 1 else lo)[k] - old[k] for k in range(3)]
                w_ = (q[0] * c - q[1] * s + to[0], q[0] * s + q[1] * c + to[1], q[2] + to[2])
                mn = [min(a, b) for a, b in zip(mn, w_)]
                mx = [max(a, b) for a, b in zip(mx, w_)]
            out.append(([f32(v) for v in mn], [f32(v) for v in mx]))
        return out
    nb = boxes()
    if not nb:
        return to, False
    gmin = (m.f32(g + 0xC), m.f32(g + 0x10))
    gmax = (m.f32(g + 0x14), m.f32(g + 0x18))
    clamped = False
    for k in range(2):
        lo = min(b[0][k] for b in nb)
        hi = max(b[1][k] for b in nb)
        shift = gmin[k] - lo if lo < gmin[k] else gmax[k] - hi if hi > gmax[k] else 0
        if shifts is not None:
            shifts.append(shift)
        if shift != 0:
            to[k] = f32(to[k] + shift)
            clamped = True
    return to, clamped


def f32_step(v, up):
    """The next float after float v, up or down."""
    bits = struct.unpack("<i", struct.pack("<f", v))[0]
    if v == 0:
        return f32(1e-45 if up else -1e-45)
    bits += 1 if (v > 0) == up else -1
    return struct.unpack("<f", struct.pack("<i", bits))[0]


def clamp_for_file(m, g, node_paths, old, to, c, s, turning):
    """A place the loader keeps as it is: clamp_to_grid(), then again on its own result while it still shifts. The loader
    clamps the file's place once more, and a turned box shifted to the edge can land a float step outside it again (the
    loader would move the piece a hair and log a second clamp note); one more pass settles every stock case. A shift too
    small to change the float moves the place one float step inward instead. Returns (place, shifted at all)."""
    to, clamped = clamp_to_grid(m, g, node_paths, old, to, c, s, turning)
    for _ in range(8):
        shifts = []
        again_to, again = clamp_to_grid(m, g, node_paths, old, to, c, s, turning, shifts)
        if not again:
            break
        for k in range(2):
            if shifts[k] != 0 and again_to[k] == to[k]:
                again_to[k] = f32_step(to[k], shifts[k] > 0)
        to, clamped = again_to, True
    return to, clamped


def solid_boxes(m, paths):
    """[(path, [x0, y0, z0, x1, y1, z1])] of the solid (non-trigger) paths among `paths`."""
    return [(q, [m.f32(q + 4 * k) for k in range(6)]) for q in paths if m.ok(q, 0x24) and not m.u32(q + 0x20) & 1]


GROUND_SINK = 10.0   # how far a building's boxes may reach below the ground around it (stock: up to 6.4, CENTRAL 0x18a410)
FLAT = 1.0           # a box at most this tall is a floor or ceiling slab


def pup_ground(m, g, p, carry, gone, node_paths):
    """Where a power-up of a removed piece lands (the z of what is under it): the highest top among the stage's solid boxes
    under its X and Y, other than the removed pieces' (`gone`: their paths), at most GROUND_SINK above the bottom of the box
    that carried it (`carry`; a building's boxes often reach a few units into the ground it stands on). With none there, the
    lowest bottom of the removed piece's own solid boxes (`node_paths`) at most RIDER_MAX_BOX wide under its X and Y, or of
    all of them, but no lower than the grid's ground there: the lowest top of the stage's flat solid slabs (at most FLAT
    tall) wider than RIDER_MAX_BOX under it, not above the carrying box (a ceiling is not ground). Neither a roof slab's own
    bottom nor a foundation or animated box reaching below the ground is the ground."""
    def under(b):
        return b[0] <= p[0] <= b[3] and b[1] <= p[1] <= b[4]
    stage = [b for q, b in solid_boxes(m, sorted(all_paths(m, g))) if q not in gone and under(b)]
    tops = [b[5] for b in stage if b[5] <= carry[2] + GROUND_SINK]
    if tops:
        return max(tops)
    own = [b for _q, b in solid_boxes(m, node_paths) if b[3] - b[0] <= RIDER_MAX_BOX and b[4] - b[1] <= RIDER_MAX_BOX]
    z = min([b[2] for b in own if under(b)] or [b[2] for b in own] or [carry[2]])
    ground = [b[5] for b in stage if (b[3] - b[0] > RIDER_MAX_BOX or b[4] - b[1] > RIDER_MAX_BOX) and b[5] - b[2] <= FLAT
              and b[5] <= carry[5]]
    return max(z, min(ground)) if ground else z


def paths_by_node(m, g):
    """{node: [collision paths through it, address order]} over the grid's listed paths (wholly mapped ones)."""
    paths_of = {}
    for p in sorted(all_paths(m, g)):
        nodes = path_nodes(m, p)
        if not m.ok(p, 0x24 + 4 * len(nodes)):
            continue
        for n in set(nodes):
            paths_of.setdefault(n, []).append(p)
    return paths_of


def rider_edits(m, edits, valid_riders, notes, parent=None, pups=None):
    """make's edits (stage.json form, offsets) with their riders' edits added (valid_riders: {base offset: [(rider offset,
    kind)]}; pups: power-up riders, {base offset: [(index, (x, y, z), box)]}, powerup_riders()):
    - a removed base removes its riders;
    - a moved or turned base moves each rider rigidly with it: new = base place + turn(rider - base old place), and the rider
      gets the same turn. The base place is where the loader puts it: a move that would take the base's collision past the
      grid's edge is shifted back inside as the loader does (clamp_for_file()); the base's edit then holds that place (noted);
    - a base edit the loader will refuse (a turn of a TRANSLATE that carries action data) carries nothing (noted);
    - a rider the user edited: a remove or a move of the user's stands as it is (noted); a turn alone (no move) is carried
      with its base like any rider and turned by the user's turn plus the base's (noted). A rider of a removed base that the
      user edited keeps the user's edit.
    - a power-up place goes the same way as a rider piece (moved and turned about the base's place, or held back with a base
      the loader cannot turn), as {"powerup": i, "from": disc place, "at": new place}; a power-up has no turn of its own. Of a
      removed base it drops to the ground at the same X and Y, keeping its hover height: new z = the ground (pup_ground(): the
      highest top of what the map leaves under it, below the carrying box) + (z - the carrying box's top).
    Returns the new list: the user's edits in their order (riders turned by the user updated in place), then the riders',
    then the power-ups'."""
    edits = [dict(e) for e in edits]
    at = {int(e["node"], 16): i for i, e in enumerate(edits) if "node" in e}
    pups = pups or {}
    if parent is None:
        parent = walk_all(m)
    g = find_grid(m, parent)
    paths_of = paths_by_node(m, g)
    out, pup_out, done, drops = [], [], set(), []
    # list(edits): the user's edits as given. A piece the user only turned that is itself a rider of another edited piece gets
    # its edit replaced below by the carried one, but its own riders and power-ups are carried by the user's turn about its disc
    # place, not by the carried place (one level only; no stock stage has a rider that carries power-ups or riders).
    for e in list(edits):
        if "node" not in e:
            continue
        base = int(e["node"], 16)
        rs = [(r, kind) for r, kind in valid_riders.get(base, ()) if r not in done]
        prs = pups.get(base, ())
        if not rs and not prs:
            continue
        deg = e.get("turn", 0)
        c, s = turn_of(deg)
        turning = (c, s) != (1.0, 0.0)
        if not e.get("remove"):
            if turning and op(m, BASE + base) == OP_TRANSLATE and m.u32(BASE + base + 0xC) != 0:
                notes.append("  note: 0x%x is a TRANSLATE with action data, which the loader cannot turn: its riders stay" % base)
                continue
            old = position(m, BASE + base)
            want = e.get("move", list(old))
            new, clamped = clamp_for_file(m, g, paths_of.get(BASE + base, []), old, want, c, s, turning)
            if clamped:
                edits[at[base]]["move"] = [f32_text(v) for v in new]
                edits[at[base]] = {k: edits[at[base]][k] for k in ("node", "move", "turn") if k in edits[at[base]]}
                notes.append("  note: 0x%x kept inside the collision grid's edge, at %.1f %.1f %.1f (as the loader does)"
                             % ((base,) + tuple(new)))
        for r, kind in rs:
            done.add(r)
            mine = edits[at[r]] if r in at else None
            if mine is not None and (e.get("remove") or "remove" in mine or "move" in mine):
                notes.append("  note: rider 0x%x (%s) of 0x%x keeps your own edit" % (r, kind, base))
                continue
            if e.get("remove"):
                notes.append("  rider 0x%x (%s) follows 0x%x" % (r, kind, base))
                out.append({"node": "0x%x" % r, "remove": True})
                continue
            rp = position(m, BASE + r)
            d = [rp[k] - old[k] for k in range(3)]
            to = [round(v, 3) for v in (d[0] * c - d[1] * s + new[0], d[0] * s + d[1] * c + new[1], d[2] + new[2])]
            ed = {"node": "0x%x" % r, "move": to}
            if mine is not None:
                total = num(mine["turn"] + deg)
                if total:
                    ed["turn"] = total
                edits[at[r]] = ed
                notes.append("  rider 0x%x (%s) follows 0x%x, turned by your %g and its %g" % (r, kind, base, mine["turn"], deg))
                continue
            if deg:
                ed["turn"] = deg
            notes.append("  rider 0x%x (%s) follows 0x%x" % (r, kind, base))
            out.append(ed)
        for i, p, box in prs:
            if e.get("remove"):   # its ground needs every remove of the map: filled in below
                drops.append((len(pup_out), len(notes), i, p, box, base))
                notes.append(None)
                to = None
            else:
                d = [p[k] - old[k] for k in range(3)]
                to = [round(v, 3) for v in (d[0] * c - d[1] * s + new[0], d[0] * s + d[1] * c + new[1], d[2] + new[2])]
                notes.append("  power-up %d follows 0x%x, to %.1f %.1f %.1f" % ((i, base) + tuple(to)))
            pup_out.append({"powerup": i, "from": [f32_text(v) for v in p], "at": [num(v) for v in to] if to else None})
    removed = {BASE + int(x["node"], 16) for x in edits + out if "node" in x and x.get("remove")}
    gone = {q for n in removed for q in paths_of.get(n, ())}
    for k, at_note, i, p, box, base in drops:
        ground = pup_ground(m, g, p, box, gone, paths_of.get(BASE + base, []))
        to = [f32_text(p[0]), f32_text(p[1]), round(ground + (p[2] - box[5]), 3)]
        pup_out[k]["at"] = [num(v) for v in to]
        notes[at_note] = "  power-up %d follows 0x%x down to its ground, at %.1f %.1f %.1f" % ((i, base) + tuple(to))
    return edits + out + pup_out


def make_copies(m, args, valid, valid_riders, pups, notes, parent=None):
    """make's copies from --copy 0x..=x,y,z[,turn] (valid: the base's piece offsets; valid_riders and pups as rider_edits()).
    Returns (copies, additions) in stage.json form: per --copy in order, {"copy", "at", "turn"} for the source, then one per
    rider (rider_map(): the riders on its roof on the disc), carried rigidly: at = the copy's place + turn(rider - source's
    disc place), with the copy's turn (on top of the rider's own rotation, as a copy keeps its source's); and for every
    --copy, an {"addpowerup": i, "from": disc place, "at": carried place} per power-up place riding the source. Copies are
    built from the disc state and power-up places are added before any power-up move (applyPointEdits()), so `from` is
    the disc place whatever else the map edits.
    Like a move (decision 9), a copy that would take the source's collision past the grid's edge is shifted back inside as
    the loader does (clamp_for_file(), noted) and its riders follow the clamped place. ValueError for a source that is not a
    piece, that copy_set() refuses, or a turned TRANSLATE with action data (the loader refuses both); a rider copy_set()
    refuses or that the turn cannot turn is skipped with a note. Each line make prints goes to `notes`."""
    if parent is None:
        parent = walk_all(m)
    g = find_grid(m, parent)
    paths_of = paths_by_node(m, g)
    copies, adds = [], []

    def turned_place(p, old, new, c, s):
        d = [p[k] - old[k] for k in range(3)]
        return [round(v, 3) for v in (d[0] * c - d[1] * s + new[0], d[0] * s + d[1] * c + new[1], d[2] + new[2])]

    def cannot_turn(n, turning):
        return turning and op(m, n) == OP_TRANSLATE and m.u32(n + 0xC) != 0
    for arg in args:
        node_s, _, vals = arg.partition("=")
        try:
            v = [float(x) for x in vals.split(",")]
            node = int(node_s, 16)
        except ValueError:
            v, node = [], None
        if node is None or len(v) not in (3, 4) or not all(math.isfinite(x) for x in v):
            raise ValueError("bad --copy %r (expected 0x..=x,y,z[,turn], finite numbers)" % arg)
        if node not in valid:
            raise ValueError("node 0x%x is not a piece of this stage (see: stage_map.py pieces)" % node)
        src = BASE + node
        why = copy_set(m, parent, src).why
        if why:
            raise ValueError("copy 0x%x: the loader would refuse it: %s" % (node, why))
        deg = num(v[3]) if len(v) == 4 else 0
        c, s = turn_of(deg)
        turning = (c, s) != (1.0, 0.0)
        if cannot_turn(src, turning):
            raise ValueError("copy 0x%x: a TRANSLATE with action data, which the loader cannot turn" % node)
        old = position(m, src)
        new, clamped = clamp_for_file(m, g, paths_of.get(src, []), old, v[:3], c, s, turning)
        e = {"copy": "0x%x" % node, "at": [f32_text(x) for x in new] if clamped else v[:3]}
        if deg:
            e["turn"] = deg
        copies.append(e)
        notes.append("  copy 0x%x to %.1f %.1f %.1f" % ((node,) + tuple(new)))
        if clamped:
            notes.append("  note: copy 0x%x kept inside the collision grid's edge, at %.1f %.1f %.1f (as the loader does)"
                         % ((node,) + tuple(new)))
        for r, kind in valid_riders.get(node, ()):
            rn = BASE + r
            why = copy_set(m, parent, rn).why or (cannot_turn(rn, turning) and "a TRANSLATE with action data, which the "
                                                  "loader cannot turn")
            if why:
                notes.append("  note: rider 0x%x of 0x%x is not copied: %s" % (r, node, why))
                continue
            re_ = {"copy": "0x%x" % r, "at": turned_place(position(m, rn), old, new, c, s)}
            if deg:
                re_["turn"] = deg
            copies.append(re_)
            notes.append("  rider 0x%x (%s) copied with 0x%x" % (r, kind, node))
        for i, p, _box in pups.get(node, ()):
            to = turned_place(p, old, new, c, s)
            adds.append({"addpowerup": i, "from": [f32_text(x) for x in p], "at": [num(x) for x in to]})
            notes.append("  power-up %d copied with 0x%x, to %.1f %.1f %.1f" % ((i, node) + tuple(to)))
    return copies, adds


def file_order(edits, copies, adds):
    """stage.json's "edits" in the loader's write() order (decision 14): node edits, copies, power-up moves, power-up additions.
    `edits` is rider_edits()' result (node edits, then power-up moves); the loader numbers a copy's "(edit N)" notes from
    this order."""
    return ([e for e in edits if "node" in e] + list(copies) + [e for e in edits if "powerup" in e] + list(adds))


def copy_estimate(m, edits, copies, parent=None):
    """(interactives, destructibles, pickup bytes): what the loader's caps count once the map's copies are in (native apply(),
    decision 7, amendment A4): slot 0, the 8 monster slots and the stage's own entries (stage_counts() of the image after the
    map's removes, which NULL their pieces; debris pickups included), plus each copy's (copy_set() of its source on the
    disc). Pickup bytes: PICKUP_BYTES per level pickup and debris_pickup_bytes() per debris pickup. Copies copy_set()
    refuses add nothing (the loader refuses them)."""
    if parent is None:
        parent = walk_all(m)
    removed = [BASE + int(e["node"], 16) for e in edits if e.get("remove")]
    after, par_after = m, parent
    if removed:
        image = bytearray(m.regions[0][1])
        for n in removed:
            if m.in_image(n) and m.ok(n, 4):
                struct.pack_into("<I", image, n - BASE, (m.u32(n) & ~63) | OP_NULL)
        after = Mem(bytes(image), m.regions[1:])
        par_after = walk_all(after)
    st = stage_counts(after, par_after)
    inter = 1 + MONSTER_SLOTS + st.interactives + st.debris_pickups
    destr = st.destructibles
    nbytes = st.pickups * PICKUP_BYTES + st.debris_bytes
    sets = {}
    for e in copies:
        n = BASE + int(e["copy"], 16)
        if n not in sets:
            sets[n] = copy_set(m, parent, n)
        cs = sets[n]
        if not cs.why:
            inter += cs.interactives + cs.debris_pickups
            destr += cs.destructibles
            nbytes += cs.pickups * PICKUP_BYTES + cs.debris_bytes
    return inter, destr, nbytes


# ---------------------------------------------------------------- fight RAM (PS2X_DUMP_RAM)

MONSTERS, NUM_MONSTERS = 0x3D3920, 0x3D3978   # TheGame::m_monsters[], m_numMonsters (native/wotm_syms.h)
PICKUP_LISTS = (0x2AECA0, 0x2AECB0, 0x2AECC0, 0x2AECD0, 0x2AECE0)   # LevelPickups::s_{active,inUse,inFlight,regen,dead}List
POWER_UPS = 0x7BDAD0                          # PowerUps::instance: m_powerUps[100] at +0xA0 (16 bytes, _cs * at +8), m_count +0x2670


def ram_powerups(ram):
    """[(x, y, rest z)] per power-up in a guest RAM image: its _cs's trans x and y and trans.w, the height it rests at
    (CreatePowerUp sets it to the point's z; the model eases down to it, and a taken one comes back there)."""
    def u32(a):
        return struct.unpack_from("<I", ram, a & 0x3FFFFFF)[0]
    out = []
    for i in range(min(u32(POWER_UPS + 0x2670), 100)):
        cs = u32(POWER_UPS + 0xA0 + 16 * i + 8)
        if cs:
            x, y, _z, w = struct.unpack_from("<4f", ram, (cs + 0x10) & 0x3FFFFFF)
            out.append((x, y, w))
    return out


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
    if a.powerup or a.no_powerup:
        pups = ram_powerups(ram)
        print("%d power-ups" % len(pups))
        for s in a.powerup:
            ok = any(near(p, vec3(s)) for p in pups)
            bad += not ok
            print("%s power-up at %s" % ("ok     " if ok else "FAILED ", s))
        for s in a.no_powerup:
            ok = not any(near(p, vec3(s)) for p in pups)
            bad += not ok
            print("%s no power-up at %s" % ("ok     " if ok else "FAILED ", s))
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
            npaths, nnodes, problems, ncopies = check(load_mem(path, a.reserve))
        except (ValueError, IndexError, OSError) as e:
            npaths, nnodes, problems, ncopies = 0, 0, [str(e)], 0
        if problems:
            bad += 1
            print("%s: %d problems" % (name, len(problems)))
            for p in problems[:50]:
                print("  " + p)
        else:
            print("%s: %d paths, %d list nodes, %d copies, OK" % (name, npaths, nnodes, ncopies))
    return 1 if bad else 0


def cmd_pieces(a):
    try:
        m = load_mem(a.file, a.reserve)
        parent = walk_all(m)
        ps = pieces(m, parent)
        rm = rider_map(m, parent, ps)
        pm = {}
        if a.file.upper().endswith(".NGP"):   # <disc>/LVL/<STAGE>.NGP: the power-up places are <disc>/PTS/<STAGE>.PUP
            stem = os.path.splitext(os.path.basename(a.file))[0].upper()
            points = read_pup(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(a.file))), "PTS", stem + ".PUP"))
            if points:
                pm = powerup_riders(m, points, parent, ps)
    except (ValueError, IndexError, OSError) as e:
        print("%s: %s" % (a.file, e))
        return 1
    copyable = 0
    for n, o, pos, npaths, destr in ps:
        cs = copy_set(m, parent, n)
        copyable += not cs.why
        print("%s  %-9s  pos %.1f %.1f %.1f  paths %d  riders %d%s%s  %s" % (
            addr(m, n), "TRANSLATE" if o == OP_TRANSLATE else "ROTATE", pos[0], pos[1], pos[2], npaths, len(rm.get(n, ())),
            "  power-ups %s" % ",".join(str(i) for i, *_ in pm[n]) if n in pm else "", "  destructible" if destr else "",
            cs.column()))
    places = ", %d power-up places on %d of them" % (sum(len(v) for v in pm.values()), len(pm)) if pm else ""
    print("%d pieces%s, %d copyable" % (len(ps), places, copyable), file=sys.stderr)
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


def make_riders(m, parent, ps, pup_file, no_riders):
    """make's riders, both keyed by base offset: ({base: [(rider offset, kind)]}, {base: [(index, (x, y, z), box)]}); nothing
    with --no-riders, and no power-ups when the stage has no .PUP (pup_file missing)."""
    if no_riders:
        return {}, {}
    rm = {b - BASE: [(r - BASE, kind_of(m, parent, r)) for r in rs] for b, rs in rider_map(m, parent, ps).items()}
    points = read_pup(pup_file)
    pm = {b - BASE: v for b, v in powerup_riders(m, points, parent, ps).items()} if points else {}
    return rm, pm


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
        rm, pm = make_riders(m, parent, ps, os.path.join(os.path.dirname(os.path.dirname(ngp_path)), "PTS", base + ".PUP"),
                             a.no_riders)
    except (ValueError, IndexError) as e:
        return fail("%s: %s" % (ngp_path, e))
    try:
        edits = make_edits(valid, base, a.move, a.turn, a.remove)
    except ValueError as e:
        return fail(str(e))
    notes = []
    edits = rider_edits(m, edits, rm, notes, parent, pm)
    try:
        copies, adds = make_copies(m, a.copy, valid, rm, pm, notes, parent)
    except ValueError as e:
        for line in notes:
            print(line)
        return fail(str(e))
    for line in notes:
        print(line)
    if len(edits) + len(copies) + len(adds) > MAX_EDITS:
        return fail("%d edits: a map holds %d at most" % (len(edits) + len(copies) + len(adds), MAX_EDITS))
    if copies:
        inter, destr, nbytes = copy_estimate(m, edits, copies, parent)
        print("copies: %d (interactives estimate %d of %d, destructibles %d of %d, pickup memory %d KB)"
              % (len(copies), inter, INTERACTIVES_CAP, destr, DESTRUCTIBLES_CAP, (nbytes + 512) // 1024))
        for what, est, cap in (("interactives", inter, INTERACTIVES_CAP), ("destructibles", destr, DESTRUCTIBLES_CAP)):
            if est > cap:
                print("warning: the %s estimate %d passes %d: the loader refuses the copies past it" % (what, est, cap))
    if adds:
        points = read_pup(os.path.join(os.path.dirname(os.path.dirname(ngp_path)), "PTS", base + ".PUP")) or []
        total = len(points) + len(adds)
        if total > MAX_POINTS:
            print("warning: %d power-up places: the loader adds up to %d, the rest are refused" % (total, MAX_POINTS))
        elif total > MAX_POWER_UPS:
            print("note: %d power-up places: the game creates %d power-ups at most" % (total, MAX_POWER_UPS))
    edits = file_order(edits, copies, adds)
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
    assert RESERVE == 0x3BFF000, hex(RESERVE)
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
    assert check(m) == (2, 6, [], 0), check(m)
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
    struct.pack_into("<I", ram, POWER_UPS + 0x2670, 2)                        # 2 power-ups, the second with no _cs
    struct.pack_into("<I", ram, POWER_UPS + 0xA0 + 8, 0x800400)
    struct.pack_into("<4f", ram, 0x800410, -338.5, 471.5, 90.0, 80.6)          # easing down from z 90 to its rest height 80.6
    pups = ram_powerups(bytes(ram))
    assert len(pups) == 1 and near(pups[0], (-338.5, 471.5, 80.6)), pups
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
    pcs = [node(OP_POLY_COLL, 4) for _ in range(4)]
    b = trans((100, 50, 0), [sphobj((0, 0, 50), 50), pcs[0]])
    b2 = trans((100, 50, 0), [sphobj((0, 0, 50), 50), pcs[1]])
    tw = trans((250, 100, 0), [sphobj((0, 0, 25), 25), pcs[2]])
    wr = trans((120, 60, 100), [sphobj((0, 0, 10), 10)])                     # world piece, bottom at 100: on b
    wg = trans((60, 30, 0), [sphobj((0, 0, 10), 10)])                        # bottom at 0, under b's roof: no
    rb = rot((200, 150, 0), [sphobj((0, 0, 25), 25), pcs[3]])                # a ROTATE base (place in the fourth row)
    world = group([b, b2, tw, wr, wg, rb], WORLD_ID)

    def pk(x, y, z):
        return trans((x, y, z), [group([obj(5001)])])
    pk1, pk2, pk3, pk4, pk5, pk6 = (pk(150, 20, 110), pk(150, 20, 140), pk(170, 20, 100), pk(60, 80, 205),
                                    pk(250, 150, 50), pk(100, 50, 104))
    pk7 = pk(210, 160, 60)                                                   # on rb
    pickups = group([pk1, pk2, pk3, pk4, pk5, pk6, pk7])
    g = node(OP_GRID, 0x20 + 4 * 8)
    w("<h", g + 4, 4), w("<h", g + 6, 2)
    for i, v in enumerate((100.0, -100.0, 0.0, 300.0, 200.0)):
        w("<f", g + 8 + 4 * i, v)
    boxes = [((40.0, 10.0, 0.0), (160.0, 90.0, 100.0), [world, b, pcs[0]], 0),
             ((40.0, 10.0, 0.0), (160.0, 90.0, 108.0), [world, b2, pcs[1]], 0),
             ((40.0, 10.0, 0.0), (160.0, 90.0, 205.0), [world, b, pcs[0]], 1),     # a trigger box: carries nothing
             ((-500.0, 0.0, 0.0), (400.0, 200.0, 50.0), [world, tw, pcs[2]], 0),   # 900 wide: carries nothing
             ((180.0, 130.0, 0.0), (220.0, 170.0, 50.0), [world, rb, pcs[3]], 0),
             ((180.0, 130.0, 70.0), (220.0, 170.0, 71.0), [world, rb, pcs[3]], 0)]     # rb's thin roof slab above it
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
    assert rm == {b: [wr, pk6], b2: [pk1], rb: [pk7]}, {hex(k): [hex(x) for x in v] for k, v in rm.items()}   # pk6: a tie
    assert riders(m, b2) == [pk1] and riders(m, tw) == []
    off = lambda n: n - BASE   # noqa: E731
    valid = {off(k): [(off(r), kind_of(m, parent, r)) for r in v] for k, v in rm.items()}

    def E(n, **kw):
        e = {"node": "0x%x" % off(n)}
        e.update(kw)
        return e

    def edits_of(*user):
        notes = []
        return rider_edits(m, list(user), valid, notes, parent), notes
    # moved and turned 90, and a removed base
    got, notes = edits_of(E(b, move=[110, 60, 5], turn=90), E(b2, remove=True))
    assert got == [E(b, move=[110, 60, 5], turn=90), E(b2, remove=True), E(wr, move=[100.0, 80.0, 105.0], turn=90),
                   E(pk6, move=[110.0, 60.0, 109.0], turn=90), E(pk1, remove=True)], got
    assert notes[0] == "  rider 0x%x (world piece) follows 0x%x" % (off(wr), off(b)), notes
    # the user's remove of a rider stands
    got, notes = edits_of(E(b, move=[100, 80, 0]), E(pk6, remove=True))
    assert got == [E(b, move=[100, 80, 0]), E(pk6, remove=True), E(wr, move=[120.0, 90.0, 100.0])], got
    assert any("keeps your own edit" in n for n in notes), notes
    # the user's turn alone of a rider: carried with the base, turned by both
    got, notes = edits_of(E(b, move=[100, 80, 0], turn=90), E(wr, turn=45))
    assert got == [E(b, move=[100, 80, 0], turn=90), E(wr, move=[90.0, 100.0, 100.0], turn=135),
                   E(pk6, move=[100.0, 80.0, 104.0], turn=90)], got
    got, _ = edits_of(E(b, move=[100, 80, 0]), E(wr, turn=45), E(pk6, move=[1, 2, 3]))   # a user move stands
    assert got == [E(b, move=[100, 80, 0]), E(wr, move=[120.0, 90.0, 100.0], turn=45), E(pk6, move=[1, 2, 3])], got
    # past the grid's edge (box y 10..90 at y 50, moved to y 20: y -20 < 0): the base and its riders shifted back inside
    got, notes = edits_of(E(b, move=[100, 20, 0]))
    assert got == [E(b, move=[100, 40, 0]), E(wr, move=[120.0, 50.0, 100.0]), E(pk6, move=[100.0, 40.0, 104.0])], got
    assert any("kept inside the collision grid" in n for n in notes), notes
    # a turn alone, not a right angle, negative: about the base's place
    got, _ = edits_of(E(b, turn=-10))
    c, s = turn_of(-10)
    assert abs(c - math.cos(math.radians(10))) < 1e-6 and abs(s + math.sin(math.radians(10))) < 1e-6
    assert got[0] == E(b, turn=-10) and got[1]["turn"] == -10 and "move" not in got[0], got
    assert all(abs(a - b_) < 0.002 for a, b_ in zip(got[1]["move"], (121.433, 56.375, 100.0))), got
    # a ROTATE base: its place is the matrix's fourth row
    got, _ = edits_of(E(rb, move=[150, 150, 0]))
    assert got == [E(rb, move=[150, 150, 0]), E(pk7, move=[160.0, 160.0, 60.0])], got

    # power-up places: the piece rule with z up to the box top + 100. 0 hovers 52 over b2's roof (60 over b's: b2 is nearer),
    # 1 is 130 over b's roof (only a trigger box is nearer), 2 hovers 70 over rb, 3 is over the 900-wide box only, 4 is 15 under
    # b's roof top (and 23 under b2's: past the 20 below)
    pts = [(150.0, 20.0, 160.0), (60.0, 30.0, 230.0), (200.0, 150.0, 120.0), (300.0, 100.0, 60.0), (120.0, 60.0, 85.0)]
    pr = powerup_riders(m, pts, parent)
    assert {k: [i for i, *_ in v] for k, v in pr.items()} == {b2: [0], rb: [2], b: [4]}, pr
    pv = {off(k): v for k, v in pr.items()}

    def pup_edits(*user):
        notes = []
        got = rider_edits(m, list(user), valid, notes, parent, pv)
        return [e for e in got if "powerup" in e], [e for e in got if "powerup" not in e], notes
    # moved and turned 90 about the base's place, removed (down to the ground, keeping the 52 hover), moved (a ROTATE base)
    pups, rest, notes = pup_edits(E(b, move=[110, 60, 5], turn=90), E(b2, remove=True), E(rb, move=[150, 150, 0]))
    assert pups == [{"powerup": 4, "from": [120, 60, 85], "at": [100, 80, 90]},
                    {"powerup": 0, "from": [150, 20, 160], "at": [150, 20, 52]},
                    {"powerup": 2, "from": [200, 150, 120], "at": [150, 150, 120]}], pups
    assert rest[:3] == [E(b, move=[110, 60, 5], turn=90), E(b2, remove=True), E(rb, move=[150, 150, 0])], rest
    assert any("power-up 0 follows 0x%x down to its ground" % off(b2) in n for n in notes), notes
    # a removed base whose carrying box is a thin roof slab (rb's, 70..71, over its 0..50 box): power-up 2 hovers 49 over the
    # slab and lands 49 over what is left under it, tw's box (top 50), not over the slab's own bottom (70) or rb's (0)
    assert pr[rb][0][2][2] == 70.0 and pr[rb][0][2][5] == 71.0, pr[rb]   # the slab carries it (nearest top)
    pups, _r, _n = pup_edits(E(rb, remove=True))
    assert pups == [{"powerup": 2, "from": [200, 150, 120], "at": [200, 150, 99]}], pups
    selftest_pup_ground()
    # a turn alone, not a right angle: the same angle as the piece riders
    pups, _r, _n = pup_edits(E(b, turn=-10))
    c, s = turn_of(-10)
    assert pups[0]["at"] == [round(v, 3) for v in (20 * c - 10 * s + 100, 20 * s + 10 * c + 50, 85.0)], pups
    # past the grid's edge: the power-up follows the clamped place (box y 10..90 at y 50, moved to y 20: kept at y 40)
    pups, rest, _n = pup_edits(E(b, move=[100, 20, 0]))
    assert rest[0] == E(b, move=[100, 40, 0]) and pups == [{"powerup": 4, "from": [120, 60, 85], "at": [120, 50, 85]}], (rest, pups)
    # an unedited base carries nothing; a non-float-exact place is written as its shortest float text
    assert pup_edits(E(wr, move=[0, 0, 0]))[0] == []
    pv2 = {off(b): [(4, (f32(120.3), f32(60.7), f32(85.1)), pr[b][0][2])]}
    got = rider_edits(m, [E(b, move=[100, 50, 10])], valid, [], parent, pv2)
    assert got[-1] == {"powerup": 4, "from": [120.3, 60.7, 85.1], "at": [120.3, 60.7, 95.1]}, got[-1]
    # make: power-ups from <disc>/PTS/<BASE>.PUP; none without the file; neither riders nor power-ups with --no-riders
    tmp = tempfile.mkdtemp()
    try:
        pup = os.path.join(tmp, "X.PUP")
        with open(pup, "wb") as f:
            f.write(struct.pack("<II", PUP_VERSION, len(pts)))
            for p in pts:
                f.write(struct.pack("<3f", *p) + bytes(PUP_SIZE - 12))
        assert read_pup(pup) == pts and read_pup(os.path.join(tmp, "none.PUP")) is None
        rmk, pmk = make_riders(m, parent, None, pup, False)
        assert rmk == valid and {k: [i for i, *_ in v] for k, v in pmk.items()} == {off(b2): [0], off(rb): [2], off(b): [4]}, pmk
        assert make_riders(m, parent, None, os.path.join(tmp, "none.PUP"), False) == (valid, {})
        assert make_riders(m, parent, None, pup, True) == ({}, {})
        with open(pup, "r+b") as f:
            f.write(struct.pack("<I", 0x1234))
        try:
            read_pup(pup)
            raise AssertionError("read_pup accepted a bad version")
        except ValueError:
            pass
    finally:
        shutil.rmtree(tmp)

    # a turn the loader will refuse (a TRANSLATE with action data): no riders, no power-ups
    w("<I", b2 + 0xC, 0x1000)
    m = mem()
    got, notes = edits_of(E(b2, turn=90))
    assert got == [E(b2, turn=90)] and any("action data" in n for n in notes), (got, notes)
    assert rider_edits(m, [E(b2, turn=90)], valid, [], parent, pv) == [E(b2, turn=90)]

    # make --copy: the copy, its riders' copies (the same turn, about the copy's place) and its power-up places added
    pieces_of = {off(n) for n, *_ in pieces(m, parent)}

    def C(n, at, turn=0):
        e = {"copy": "0x%x" % off(n), "at": at}
        if turn:
            e["turn"] = turn
        return e

    def copies_of(args, riders_=None, pups_=None):
        notes = []
        got = make_copies(m, args, pieces_of, valid if riders_ is None else riders_, pv if pups_ is None else pups_, notes,
                          parent)
        return got, notes
    # pk6 holds its pickup id on an OBJECT here, which a copy cannot hold: the rider is skipped with a note
    (cps, adds), notes = copies_of(["0x%x=150,100,0,90" % off(b)])
    assert cps == [C(b, [150, 100, 0], 90), C(wr, [140, 120, 100], 90)], cps
    assert adds == [{"addpowerup": 4, "from": [120, 60, 85], "at": [140, 120, 85]}], adds
    assert notes == ["  copy 0x%x to 150.0 100.0 0.0" % off(b),
                     "  rider 0x%x (world piece) copied with 0x%x" % (off(wr), off(b)),
                     "  note: rider 0x%x of 0x%x is not copied: its copy would hold a node of kind 0 (OBJECT)"
                     % (off(pk6), off(b)),
                     "  power-up 4 copied with 0x%x, to 140.0 120.0 85.0" % off(b)], notes
    assert copies_of(["0x%x=150,100,0,90" % off(b)], {}, {})[0] == ([C(b, [150, 100, 0], 90)], [])   # --no-riders
    # past the grid's edge (box y 10..90 at y 50, copied to y 20): kept inside like a move, its riders follow the clamped place
    (cps, adds), notes = copies_of(["0x%x=100,20,0" % off(b)])
    assert cps == [C(b, [100, 40, 0]), C(wr, [120, 50, 100])] and adds[0]["at"] == [120, 50, 85], (cps, adds)
    assert any("kept inside the collision grid" in n for n in notes), notes
    # turned past the edge: make's place is one the loader keeps (clamping it again shifts nothing)
    b_paths = paths_by_node(m, find_grid(m, parent))[b]
    for deg in (17, 30, 45, 133, 200, 271, 315, 12.5):
        c_, s_ = turn_of(deg)
        for want in ((100, -500, 0), (900, 100, 0), (-900, 900, 0), (100, 900, 3)):
            to, moved = clamp_for_file(m, find_grid(m, parent), b_paths, position(m, b), want, c_, s_, True)
            assert moved and not clamp_to_grid(m, find_grid(m, parent), b_paths, position(m, b), to, c_, s_, True)[1], (deg, want)
    # the same source twice: two sets
    (cps, adds), _ = copies_of(["0x%x=150,100,0" % off(b), "0x%x=150,120,0" % off(b)])
    assert [e["copy"] for e in cps] == ["0x%x" % off(b), "0x%x" % off(wr)] * 2 and [e["addpowerup"] for e in adds] == [4, 4], cps
    # refused: a source copy_set refuses, a turned TRANSLATE with action data (b2), not a piece, bad numbers
    for args, text in ((["0x%x=150,100,0" % off(pk1)], "kind 0 (OBJECT)"), (["0x%x=150,100,0,90" % off(b2)], "action data"),
                       (["0x%x=150,100,0" % off(pcs[0])], "not a piece"), (["0x%x=150,100" % off(b)], "bad --copy")):
        try:
            copies_of(args)
            raise AssertionError("make_copies accepted %r" % args)
        except ValueError as e:
            assert text in str(e), (str(e), text)
    assert copies_of(["0x%x=150,100,0" % off(b2)])[0][0][0] == C(b2, [150, 100, 0])   # unturned it is fine
    # the file's order (the loader's write(), decision 14): node edits, copies, power-up moves, power-up additions
    (cps, adds), _ = copies_of(["0x%x=150,100,0" % off(b)])
    mixed = rider_edits(m, [E(b2, remove=True)], valid, [], parent, pv)   # b2 and its rider pk1 removed, power-up 0 moved down
    assert [list(e)[0] for e in mixed] == ["node", "node", "powerup"], mixed
    got = file_order(mixed, cps, adds)
    assert got == mixed[:2] + cps + mixed[2:] + adds, got
    assert [list(e)[0] for e in got] == ["node", "node", "copy", "copy", "powerup", "addpowerup"], got
    # with the pickup id on its GROUP (as on the disc), pk6 is copied too
    pk6g = children(m, pk6)[0]
    w("<I", pk6g, 5001 << 18 | OP_GROUP), w("<I", children(m, pk6g)[0], OP_OBJECT)
    m = mem()
    (cps, _a), notes = copies_of(["0x%x=150,100,0,90" % off(b)])
    assert cps == [C(b, [150, 100, 0], 90), C(wr, [140, 120, 100], 90), C(pk6, [150, 100, 104], 90)], cps
    assert "  rider 0x%x (pickup type 1) copied with 0x%x" % (off(pk6), off(b)) in notes, notes
    selftest_copies()
    print("selftest OK")
    return 0


class _Img:
    """A built image and reserve for the copy selftest (the native tests' Img)."""

    def __init__(self, size=0x8000, res=0x4000):
        self.img, self.res, self.top = bytearray(size), bytearray(res), BASE + 0x100

    def at(self, a):
        return (self.res, a - RESERVE) if a >= RESERVE else (self.img, a - BASE)

    def w(self, fmt, a, v):
        b, o = self.at(a)
        struct.pack_into(fmt, b, o, v)

    def r32(self, a):
        b, o = self.at(a)
        return struct.unpack_from("<I", b, o)[0]

    def alloc(self, size):
        a = self.top
        self.top += (size + 15) & ~15
        return a

    def node(self, o, size, id1=0):
        a = self.alloc(size)
        self.w("<I", a, id1 << 18 | o)
        return a

    def set_id(self, n, id1):
        self.w("<I", n, (self.r32(n) & 0x3FFFF) | id1 << 18)

    def kids(self, a, ks):
        for i, k in enumerate(ks):
            self.w("<I", a + 4 * i, k)
        return a

    def still(self):   # a full-size OBJECT whose animation channels are all -1
        a = self.node(OP_OBJECT, 0xA0)
        for k in range(7):
            self.w("<h", a + 0x30 + 2 * k, -1)
        return a

    def polycoll(self):
        return self.node(OP_POLY_COLL, 0x10)

    def istate(self, child, id1=0):
        return self.kids(self.node(OP_INTERACTIVE, 0x10, id1) + 0xC, [child]) - 0xC

    def emitter(self, id1=0):
        return self.node(OP_EMITTER, 0xB0, id1)

    def switch(self, ks):
        a = self.node(OP_SWITCH, 0xC + 4 * len(ks))
        self.w("<B", a + 0xB, len(ks))
        return self.kids(a + 0xC, ks) - 0xC

    def act(self, t, size):
        a = self.alloc(size)
        self.w("<I", a, t)
        return a

    def hat(self):
        return self.act(10, 0x18)

    def domino(self, target, state):
        a = self.act(8, 0x10)
        self.w("<I", a + 8, state), self.w("<I", a + 0xC, target)
        return a

    def act_group(self, acts):
        a = self.act(ACT_GROUP, 0xC + 4 * len(acts))
        self.w("<I", a + 8, len(acts))
        return self.kids(a + 0xC, acts) - 0xC

    def debris(self, types):
        a = self.act(ACT_DEBRIS, 0x10 + 0x30 * len(types))
        self.w("<I", a + 8, len(types))
        for i, t in enumerate(types):
            self.w("<H", a + 0x10 + 0x30 * i + 0xE, 1 | t << 5 | 3 << 10)
        return a

    def destr(self, states):   # [(child, actions)]
        d = self.node(OP_DESTRUCTIBLE, 0xC + 12 * len(states))
        self.w("<H", d + 0xA, len(states))
        for i, (ch, acts) in enumerate(states):
            self.w("<I", d + 0xC + 12 * i, ch), self.w("<I", d + 0x10 + 12 * i, acts)
        return d

    def group(self, ks, id1=0, flags=1):
        a = self.node(OP_GROUP, 0x20 + 4 * len(ks), id1)
        self.w("<H", a + 8, len(ks)), self.w("<H", a + 0xA, flags)
        return self.kids(a + 0x20, ks) - 0x20

    def translate(self, ks, x, y, z):
        a = self.node(OP_TRANSLATE, 0x1C + 4 * len(ks))
        self.w("<I", a + 8, len(ks))
        for i, v in enumerate((x, y, z)):
            self.w("<f", a + 0x10 + 4 * i, v)
        return self.kids(a + 0x1C, ks) - 0x1C

    def holder(self, o, count_off, kids_off, ks):
        a = self.node(o, kids_off + 4 * len(ks))
        self.w("<I", a + count_off, len(ks))
        return self.kids(a + kids_off, ks) - kids_off

    def mem(self):
        return Mem(bytes(self.img), [(RESERVE, bytes(self.res))])


def _copy_stage():
    """The native tests' CopyStage: world {mid {tD}, tE, oldLast}, a pickup root, a 4 x 4 grid of 200 with paths pD and pE."""
    im, s = _Img(), {}
    s["pcE"] = im.polycoll()
    s["gE"] = im.group([im.still(), s["pcE"]])
    s["dE"] = im.destr([(s["gE"], 0)])
    s["iE"] = im.istate(s["dE"])
    s["tE"] = im.translate([s["iE"]], 700, 100, 0)
    s["d2"] = im.destr([(im.still(), 0)])
    s["i2"] = im.istate(s["d2"])
    s["hat1"] = im.hat()
    s["dom"] = im.domino(s["i2"], 1)
    s["domOut"] = im.domino(s["iE"], 0)
    s["a1"] = im.act_group([s["hat1"], s["dom"], s["domOut"]])
    s["obj0"], s["obj1"] = im.still(), im.still()
    s["sw"] = im.switch([s["obj0"], s["obj1"]])
    s["pc"] = im.polycoll()
    s["em"] = im.emitter()
    s["g0"] = im.group([s["sw"], s["pc"], s["em"]])
    s["obj2"] = im.still()
    s["d1"] = im.destr([(s["g0"], s["a1"]), (s["obj2"], s["hat1"])])
    s["i1"] = im.istate(s["d1"])
    s["tD"] = im.translate([s["i1"], s["i2"]], 300, 300, 0)
    s["mid"] = im.group([s["tD"]])
    s["oldLast"] = im.group([im.still()])
    s["world"] = im.group([s["mid"], s["tE"], s["oldLast"]], WORLD_ID, 3)
    s["swP"] = im.switch([im.still()])
    s["gP"] = im.group([s["swP"]], 0x12C1)
    s["tP"] = im.translate([s["gP"]], 500, 500, 10)
    s["holder"] = im.group([s["tP"]])
    s["proot"] = im.group([s["holder"]])
    g = s["g"] = im.node(OP_GRID, 0x20 + 4 * 16)
    im.w("<h", g + 4, 4), im.w("<h", g + 6, 4)
    for i, v in enumerate((200.0, 0.0, 0.0, 800.0, 800.0)):
        im.w("<f", g + 8 + 4 * i, v)

    def path(lo, hi, nodes, cells):
        p = im.alloc(0x24 + 4 * len(nodes))
        for i, v in enumerate(lo + hi):
            im.w("<f", p + 4 * i, v)
        im.w("<I", p + 0x1C, len(nodes))
        im.kids(p + 0x24, nodes)
        for cell in cells:
            ln = im.alloc(8)
            im.w("<I", ln, im.r32(g + 0x20 + 4 * cell)), im.w("<I", ln + 4, p)
            im.w("<I", g + 0x20 + 4 * cell, ln)
        return p
    s["pD"] = path((290.0, 290.0, 0.0), (320.0, 310.0, 40.0), [s[k] for k in ("world", "mid", "tD", "i1", "d1", "g0", "pc")],
                   [5])
    s["pE"] = path((690.0, 90.0, 0.0), (710.0, 110.0, 20.0), [s[k] for k in ("world", "tE", "iE", "dE", "gE", "pcE")], [3])
    im.w("<I", BASE, 3)
    im.kids(BASE + 4, [s["world"], s["proot"], g])
    return im, s


def selftest_copies():
    """copy_set(), stage_counts() and check()'s copy problems on the native tests' CopyStage (stagemap_copy_set,
    stagemap_copy_refusals, stagemap_stage_counts, stagemap_copy_debris_pickups)."""
    im, s = _copy_stage()
    m = im.mem()
    parent = walk_all(m)
    c = copy_set(m, parent, s["tD"])
    want = [s[k] for k in ("tD", "i1", "d1", "g0", "sw", "em", "i2", "d2")]
    nbytes = sum((v + 15) & ~15 for v in (0x24, 0x10, 0x24, 0x2C, 0x14, 0xB0, 0x10, 0x18))
    assert (c.why, c.nodes, c.bytes, c.interactives, c.destructibles, c.pickups) == ("", want, nbytes, 2, 2, 0), c.why
    assert c.column() == "copy %d B i2 d2 p0" % nbytes, c.column()
    p = copy_set(m, parent, s["tP"])
    assert (p.why, p.nodes, p.interactives, p.destructibles, p.pickups) == ("", [s["tP"], s["gP"], s["swP"]], 1, 0, 1)
    assert copy_set(m, parent, s["g0"]).why == "not a piece"
    assert node_size(m, s["obj0"]) == 0 and node_size(m, s["pc"]) == 0 and node_size(m, s["sw"]) == 0x14
    assert stage_counts(m, parent).as_tuple() == (4, 3, 1, 0, 0)
    # make's estimates (the loader's caps): 1 + 8 + the stage's (after the map's removes) + each copy's
    cp_d = {"copy": "0x%x" % (s["tD"] - BASE), "at": [500, 500, 0]}
    cp_p = {"copy": "0x%x" % (s["tP"] - BASE), "at": [600, 600, 10]}
    assert copy_estimate(m, [], [cp_d, cp_p], parent) == (1 + 8 + 4 + 2 + 1, 3 + 2, 2 * 0x500)
    assert copy_estimate(m, [{"node": "0x%x" % (s["tE"] - BASE), "remove": True}], [cp_d, cp_p], parent) == (15, 4, 2 * 0x500)
    assert copy_estimate(m, [], [], parent) == (13, 3, 0x500)

    def why(change):
        im2, s2 = _copy_stage()
        change(im2, s2)
        m2 = im2.mem()
        return copy_set(m2, walk_all(m2), s2["tD"]).why

    def under_char(i, t):
        i.w("<I", t["mid"] + 0x20, i.holder(OP_CHAR_INSTANCE, 0x28, 0x2C, [t["tD"]]))

    def ad_between(i, t):
        ad = i.node(OP_ACTION_DATA, 0x10)
        i.w("<I", ad + 4, t["i1"])
        i.w("<I", t["tD"] + 0x1C, ad)

    def shared_holds(i, t):   # g0 = {sw, gs, em}, d1's state 1 = plain {gs, OBJECT}: plain is shared but holds gs (copied)
        gs = i.group([i.switch([i.still()])])
        i.w("<I", t["g0"] + 0x24, gs)
        i.w("<I", t["d1"] + 0x18, i.group([gs, i.still()]))
    for change, text in (
            (lambda i, t: i.set_id(t["g0"], 0x2134), "holds special id 0x2134 (scripts)"),
            (lambda i, t: i.set_id(t["g0"], 0x20), "holds special id 0x20 (monsters)"),
            (lambda i, t: i.set_id(t["i1"], 0x27AE), "holds id1 0x27ae (a fixed id1 table: CAPITOL pillars)"),
            (lambda i, t: i.set_id(t["i2"], 0x2715), "holds id1 0x2715 (a fixed id1 table: the 3-MILE vent and stacks)"),
            (lambda i, t: i.set_id(t["em"], 0x1D4C), "an emitter with locator id 0x1d4c"),
            (lambda i, t: i.w("<I", t["g0"] + 0x24, i.holder(OP_ANIM_XFORM, 4, 0xCC, [t["pc"]])),
             "animated (an ANIM_XFORM in it)"),
            (lambda i, t: i.w("<I", t["a1"] + 0xC, i.act(ACT_ANIM, 0x10)), "animated (an ACT_ANIM action)"),
            (lambda i, t: i.w("<I", t["a1"] + 0xC, i.act(12, 0x10)), "a script action (ACT_RUNSCRIPT)"),
            (lambda i, t: i.w("<I", t["a1"] + 0xC, i.act(9, 0x10)), "a script action (ACT_CINEMA)"),
            (lambda i, t: (under_char(i, t), i.w("<h", t["obj0"] + 0x30, 0)), "animated (the CHAR_INSTANCE 0x"),
            (lambda i, t: i.w("<I", t["mid"] + 0x20, i.holder(OP_SKEL_BONE, 4, 0xC, [t["tD"]])),
             "animated (a SKEL_BONE above it)"),
            (lambda i, t: i.w("<I", t["mid"] + 0x20, i.holder(OP_ANIM_SCALE, 4, 0x60, [t["tD"]])), "not a piece"),
            (ad_between, "its copy would hold a node of kind 23 (ACTION_DATA)"),
            (shared_holds, "a shared node inside it (0x"),
            (lambda i, t: i.w("<I", t["mid"] + 0x20, i.istate(i.destr([(t["tD"], 0)]))),
             "in a destructible's state (DESTRUCTIBLE 0x"),
            (lambda i, t: i.w("<H", t["mid"] + 0xA, 0x21), "mode-masked (GROUP 0x"),
            (lambda i, t: i.set_id(t["obj2"], 0x12C3), "its copy would hold a node of kind 0 (OBJECT)")):
        got = why(change)
        assert got.startswith(text), (got, text)
    for change in (lambda i, t: i.set_id(t["i1"], 0x2134),            # any other id on a 39 goes to AddDestruct only
                   under_char,                                         # no channel read under the CHAR_INSTANCE
                   lambda i, t: i.w("<h", t["obj0"] + 0x30, 0),        # a channel without a CHAR_INSTANCE above
                   lambda i, t: i.w("<H", t["mid"] + 0xA, 0x4F),       # other low flag bits are not masks
                   lambda i, t: i.w("<I", t["d1"] + 0x18, t["obj0"])):   # a shared OBJECT held twice
        assert why(change) == "", why(change)
    # a destructible-range id on an OBJECT, a pickup id on a GROUP, a pickup id on a SWITCH (copied)
    for key, id1, counts in (("obj0", 0x1500, (2, 3, 0)), ("g0", 0x12C5, (3, 2, 1)), ("sw", 0x12C3, (3, 2, 1))):
        im2, s2 = _copy_stage()
        im2.set_id(s2[key], id1)
        m2 = im2.mem()
        c2 = copy_set(m2, walk_all(m2), s2["tD"])
        assert c2.why == "" and (c2.interactives, c2.destructibles, c2.pickups) == counts, (key, c2.why)
    # debris pickups (amendment A4): a rigid pickup, none and a plain one in d1's state 0 actions
    im2, s2 = _copy_stage()
    im2.w("<I", s2["a1"] + 0x14, im2.debris([0x1395 - 0x1388, 0, 1]))
    m2 = im2.mem()
    par2 = walk_all(m2)
    c2 = copy_set(m2, par2, s2["tD"])
    assert (c2.debris_pickups, c2.debris_bytes, c2.interactives) == (2, 0x4D0 + 0x180, 2), c2.column()
    assert c2.column().endswith(" debris 2 (%d B)" % (0x4D0 + 0x180)), c2.column()
    assert stage_counts(m2, par2).as_tuple() == (4, 3, 1, 2, 0x4D0 + 0x180)
    assert copy_estimate(m2, [], [cp_d], par2) == (1 + 8 + 4 + 2 + 2 + 2, 3 + 2, 0x500 + 2 * (0x4D0 + 0x180))
    # a NULL ends a branch
    im.w("<I", s["tE"], OP_NULL)
    m = im.mem()
    assert stage_counts(m, walk_all(m)).as_tuple()[:3] == (3, 2, 1)

    # check(): a wrapper G = {oldLast, tD'} in the world root's last slot, tD' = {i1'} -> d1' -> {g0' -> {sw', pc, em'}, obj2}
    im, s = _copy_stage()
    assert check(im.mem()) == (2, 2, [], 0), check(im.mem())
    shared = im.group([0])                                            # an image GROUP, linked further down
    im.top = RESERVE
    d1c = im.destr([(0, s["a1"]), (s["obj2"], s["hat1"])])
    g0c = im.group([im.switch([s["obj0"], s["obj1"]]), s["pc"], im.emitter()])
    im.w("<I", d1c + 0xC, g0c)
    i1c = im.istate(d1c)
    tdc = im.translate([i1c], 500, 500, 0)
    wrap = im.group([s["oldLast"], tdc], 0, 3)
    im.w("<I", s["world"] + 0x20 + 8, wrap)
    m = im.mem()
    assert check(m) == (2, 2, [], 1), check(m)
    im.w("<I", tdc + 0x1C, s["i1"])                                   # the copy's slot at the source's INTERACTIVE_STATE
    got = check(im.mem())[2]
    assert "copy at @0x%08x points at per-instance node 0x%x" % (tdc, s["i1"] - BASE) in got, got
    assert "per-instance node 0x%x held by 2 child slots" % (s["i1"] - BASE) in got, got
    im.w("<I", tdc + 0x1C, i1c)
    im.w("<I", s["g0"] + 0x24, i1c)                                   # a source node holding a node of the copy
    got = check(im.mem())[2]
    assert any(p.startswith("per-instance node @0x%08x held by 2" % i1c) for p in got), got
    im.w("<I", s["g0"] + 0x24, s["pc"])
    im.w("<I", s["pD"] + 0x24 + 4 * 6, RESERVE + 0x3FF0 + 0x100)      # a path entry outside every region
    got = check(im.mem())[2]
    assert any("entry 6" in p and "not mapped" in p for p in got), got
    im.w("<I", s["pD"] + 0x24 + 4 * 6, s["pc"])
    im.w("<I", shared + 0x20, im.still())                             # d1' state 1: the image GROUP `shared` holding a
    im.w("<I", d1c + 0x18, shared)                                    # reserve OBJECT
    got = check(im.mem())[2]
    want = "shared node 0x%x in copy @0x%08x holds copied node @0x%08x" % (shared - BASE, tdc, im.r32(shared + 0x20))
    assert got == [want], got
    im.w("<I", d1c + 0x18, s["obj2"])
    im.w("<H", wrap + 8, 0x1000)                                      # numKids past the reserve's end
    got = check(im.mem())
    first_out = (RESERVE + 0x4000 - wrap - 0x20) // 4                 # the first slot past the reserve's end
    assert got[3] == 0xFFF and "wrapper @0x%08x: slot %d not mapped" % (wrap, first_out) in got[2], got


def selftest_pup_ground():
    """pup_ground() on CENTRAL's case (power-up 4 of 0x165a70): a tower whose roof box carries a power-up, its foundation box
    reaching below the ground, another piece's animated 1000 x 1000 box reaching far below it, a flat ground slab, a
    ceiling."""
    img = bytearray(0x1000)
    top = [BASE + 0x100]

    def w(fmt, a, *v):
        struct.pack_into(fmt, img, a - BASE, *v)

    def alloc(size):
        a = top[0]
        top[0] += (size + 15) & ~15
        return a
    g = alloc(0x24)
    w("<I", g, OP_GRID)
    w("<hh5f", g + 4, 1, 1, 4000.0, -2000.0, -2000.0, 2000.0, 2000.0)   # one cell: every path listed there

    def path(lo, hi):
        p = alloc(0x24)
        w("<6f", p, *(lo + hi))
        ln = alloc(8)
        w("<II", ln, struct.unpack_from("<I", img, g + 0x20 - BASE)[0], p)
        w("<I", g + 0x20, ln)
        return p
    carry = path((660.0, 450.0, 0.0), (880.0, 750.0, 579.6))            # the tower: its roof carries the power-up
    found = path((660.0, 450.0, -61.2), (880.0, 790.0, 238.5))          # its foundation, below the ground
    sunk = path((660.0, 450.0, -6.4), (880.0, 750.0, 271.1))            # a building box a few units into the ground
    anim = path((300.0, 100.0, -242.0), (1300.0, 1100.0, 758.0))        # another piece's animated box
    ground = path((-1700.0, -1600.0, 0.0), (1600.0, 1500.0, 0.0))       # the flat ground slab
    path((-1700.0, -1600.0, 1312.3), (1600.0, 1500.0, 1312.3))          # the ceiling
    m = Mem(bytes(img))
    p = (777.0, 600.7, 630.2)

    def box(q):
        return [m.f32(q + 4 * k) for k in range(6)]
    tower = {carry, found, sunk}
    assert pup_ground(m, g, p, box(carry), tower, sorted(tower)) == 0.0                 # the ground slab, not -242 or -61.2
    assert pup_ground(m, g, p, box(sunk), tower, sorted(tower)) == 0.0                  # 6.4 into the ground: still on it
    # nothing at or below the carrying box: the tower's own lowest box under it (-61.2), but no lower than the flat ground
    deep = box(found)
    assert abs(pup_ground(m, g, p, deep, tower | {ground}, sorted(tower)) - f32(-61.2)) < 1e-6    # no ground slab left
    assert pup_ground(m, g, p, deep, tower, sorted(tower)) == 0.0                       # the slab is above -61.2 + 10: clamped
    assert pup_ground(m, g, (1500.0, 1400.0, 0.0), box(carry), tower, sorted(tower)) == 0.0   # outside the tower too


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
    p.add_argument("--copy", action="append", default=[], metavar="0x..=X,Y,Z[,TURN]",
                   help="copy a piece to a place (turned about Z), with copies of its riders and power-up places")
    p.add_argument("--no-riders", action="store_true",
                   help="do not carry the riders (pickups and pieces standing on an edited or copied piece's roof) with it")
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
    p.add_argument("--powerup", action="append", default=[], metavar="X,Y,Z", help="a power-up at X,Y resting at height Z")
    p.add_argument("--no-powerup", action="append", default=[], metavar="X,Y,Z")
    p.set_defaults(fn=cmd_ram)
    p = sub.add_parser("selftest")
    p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
