# Map Creator PR 1 (loading maps) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A saved map (`mods/<mod>/stages/<id>/stage.json` + `photo.png`) is listed in the stage selection, and a match on it
loads the stock stage with its pieces moved, turned or removed, solid where they now stand, offline and online.

**Architecture:** One pure header, `native/wotm_stagemap.hpp` (JSON, hash, hierarchy walk, pieces, grid relink, spheres,
turn), works on a `Mem` view of guest memory and is unit-tested on synthetic images. `wotm_mods.hpp` scans `stages/` folders.
A new `native/runtime/wotm_stagemap.inc` hooks `LoadLevelDB` (hash) and `dbInitDb` (apply) and dumps the result on request;
`wotm_levelsel.inc` lists maps in slots 18 to 23 and resolves them at confirm. `tools/stage_map.py` lists pieces, makes maps
and checks the grid rule on stock images and RAM dumps.

**Tech Stack:** C++17 (runtime, native tests), Python 3 (`tools/ngp.py`), PowerShell checks (`live_check.ps1`,
`net_check.ps1`).

**Spec:** `docs/superpowers/specs/2026-11-09-map-creator-design.md` (read it first, including "What the research found").

## Global constraints

- English everywhere; every new file starts with two `// ABOUTME:` (or `# ABOUTME:`) lines; translate Portuguese you touch.
- Never commit game files (`ISO/`, `disc/`, `runtime/`, `recomp/`, `*.elf`, RAM dumps). Commits touching docs run
  `python tools/doclint.py` first; after adding a `WOTM_` variable run `python tools/wotm_env.py` (NATIVO.md table).
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build the runtime from the PowerShell tool:
  `$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"; cmd /c C:\Projects\wotlane-src\build_runtime.bat`
  Native tests: `cmd /c C:\Projects\wotlane-src\tools\test_native.bat` (133 checks green before this plan).
- Live checks need `$env:PS2X_CD_IMAGE = 'C:\Projects\wotlane\ISO\War of the Monsters (USA).iso'` and
  `$env:WOTM_DISC = 'C:\Projects\wotlane-src\disc'`; park windows on the left monitor; never inject real mouse/keyboard input.
- The Bash tool mangles `\\` in heredocs: write Python edit scripts to the scratchpad and run them with `python -I <file>`.
- **Determinism:** everything written to guest RAM is written on the EE thread, at `dbInitDb` entry, from the map files and
  the image only. No clocks, no host addresses, no unordered containers driving write order.

## Review focus

1. Grid relink: after any edit every collision path is listed in exactly the cells its box covers, each once, and no list
   node is shared by two cells (Task 3 tests, Task 8 dump check).
2. The patch runs once per load: a second `dbInitDb` on the same image (menus, REBUILD LEVEL) must not apply twice (Task 6).
3. The reserve area is never used by anything else while a stage is loaded (Task 1 answer, Task 6 guard).
4. `stages/` folders never override disc files and are part of the mod digest (Task 5).
5. A map whose `baseHash` does not match is refused with a log line and the stock stage loads (Task 6).

---

### Task 1: confirm the open points (research, no code)

**Files:** Modify `docs/superpowers/specs/2026-11-09-map-creator-design.md` (new section "PR 1 checks" at the end).

Answer each with file:line evidence; where an answer contradicts the spec, stop and report before Task 2.

- [ ] **1a. Image size and hash point.** In `src/game/Shell.cpp` (`LoadLevelDB`, ~:1388) find where the inflated size of
  the stage `.NGP` is known after the call (e.g. `fileStatus` +0x74 count, `ngpAddr[]`; see `wotm_overrides.cpp:1182-1200`).
  Confirm nothing writes into the image between the end of `LoadLevelDB` and the entry of `dbInitDb` other than
  `dbsRelocateFileZero` (and list what that one writes, if inside the image).
- [ ] **1b. Level or not at `dbInitDb`.** `dbInitDb` is also called for the shell and pre-shell (`Shell.cpp:263-264`,
  `:1986-1987`). Find a value that says the call is for a stage (its `_dbheader` argument equal to `0xA00000`, a Shell
  state, or a flag our `LoadLevelDB` hook sets and the `dbInitDb` hook clears). Pick the flag approach unless something
  simpler is certain.
- [ ] **1c. REBUILD LEVEL and restarts.** Does "REBUILD LEVEL" / a new round re-run `LoadLevelDB` (fresh image) or only
  `dbInitDb` on the old image? Answer decides Task 6's once-per-load guard.
- [ ] **1d. Reserve area.** Proposed: `kReserve = PS2_RAM_SIZE - 0x101000`, size `0x100000` (1 MB right below the top-of-RAM
  slots at `-0x1000..`). grep `native/` for every use of addresses in `[PS2_RAM_SIZE - 0x101000, PS2_RAM_SIZE - 0x1000)` and
  read the high-RAM loader (`wotm_overrides.cpp:1182-1230`, `[wotm] stage load: ... MB to the end of RAM`) for the highest
  address monsters/RTX/TEX reach with 8 monsters. If it can reach the reserve, Task 6 logs and refuses edits that need it.
- [ ] **1e. Native renderer.** In `native/wotm_native.hpp` (`Scene::walk` and any bounds check such as `ok(addr)` against the
  image range, caches keyed by node address): does it draw (i) a `NULL` (op 9) node as nothing, (ii) a `ROTATE` node that
  lives outside `[0xA00000, image end)`? Note what must change for (ii); if it is more than a bounds widening, TURN on
  `TRANSLATE`-only pieces is dropped from PR 1 (spec "Pieces").
- [ ] **1f. Grid cell order and coverage rule.** From `HdItCell::reset` (`src/common/HdIterator.cpp:35-110`) and
  `hdSaveColGrid`: cell index formula (`row * nCols + col`?) and how a box maps to cells (floor of
  `(x - minX) / cellSize`, inclusive max, clamping). Task 2 confirms the rule on all 19 stages.
- [ ] **1g. Stage selection slots 18-23.** In `Shell.cpp` `screenLevelSelect`/`levelUnlocked`: are 18-23 shown and
  selectable when present in the list (lock, picture switch child, name row)? What does the picture switch child draw for
  them today?
- [ ] **1h. Water and kill zones.** `WaterGrid.cpp` and `RST/<STAGE>.MRS`: world-space data tied to pieces? (Only record;
  PR 1 does not fix them.)
- [ ] **Commit:** `docs: map creator PR 1 checks`.

### Task 2: `tools/stage_map.py` (pieces, grid rule, make)

**Files:** Create `tools/stage_map.py`. Uses `tools/ngp.py` (`inflate_ngp`, `Image`, `OPNAMES`).

- [ ] **Step 1: the walker and the grid rule.** Write `stage_map.py` with:

```python
#!/usr/bin/env python3
# ABOUTME: Map creator tool: lists a stage's pieces, makes a map folder (stage.json with baseHash) and checks the collision
# ABOUTME: grid rule (every path listed in exactly the cells its box covers) on a stock .NGP or a RAM dump of an edited stage.
import argparse, json, math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ngp

BASE = 0xA00000
FNV_OFF, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3


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

    def u16(self, a):
        d, o = self._at(a, 2)
        return struct.unpack_from("<H", d, o)[0]

    def u8(self, a):
        d, o = self._at(a, 1)
        return d[o]

    def f32(self, a):
        d, o = self._at(a, 4)
        return struct.unpack_from("<f", d, o)[0]


def children(m, n):
    """Child pointers of node n (same opcodes as the native walk; collision-only groups included)."""
    op = m.u32(n) & 63
    def arr(a, k, stride=4, off=0):
        return [m.u32(a + stride * i + off) for i in range(min(k, 4096))]
    if op == 1:  return arr(n + 0x20, m.u16(n + 8))
    if op == 3:  return arr(n + 0x1C, m.u32(n + 8))
    if op == 4:  return arr(n + 0x50, m.u32(n + 8))
    if op == 30: return arr(n + 0x2C, m.u32(n + 8))
    if op == 6:  return arr(n + 0xC, m.u8(n + 0xB))
    if op == 2:
        k = m.u32(n + 4)
        return arr(n + 0x20, k, 16, 8) if 0 < k < 16 else []
    if op == 8:  return [m.u32(n + 4), m.u32(n + 8)]
    if op == 23: return [m.u32(n + 4)]
    if op == 39: return [m.u32(n + 0xC)]
    if op == 31: return arr(n + 0xC, m.u16(n + 0xA), 12)
    if op == 25: return arr(n + 0x2C, m.u32(n + 0x28))
    return []


def walk_all(m):
    """{node: parent} for every node reachable from the roots (roots map to None)."""
    parent, stack = {}, [(r, None) for r in ngp.Image(m.regions[0][1]).roots()]
    while stack:
        n, p = stack.pop()
        if n in parent or not m.ok(n, 16):
            continue
        parent[n] = p
        stack.extend((c, n) for c in children(m, n) if c)
    return parent


def find_grid(m, parent):
    grids = [n for n in parent if (m.u32(n) & 63) == 15]
    if len(grids) != 1:
        raise ValueError("expected one COLL_GRID, found %d" % len(grids))
    return grids[0]


def grid_cells(m, g):
    """[(cell index, [(listnode, path)])] in cell order."""
    ncol, nrow = m.u16(g + 4), m.u16(g + 6)
    out = []
    for i in range(ncol * nrow):
        lst, ln = [], m.u32(g + 0x20 + 4 * i)
        while ln:
            lst.append((ln, m.u32(ln + 4)))
            ln = m.u32(ln)
        out.append((i, lst))
    return out


def covered(m, g, path):
    """Cells a path's box covers (rule confirmed in Task 1f; adjust here if 1f says otherwise)."""
    ncol, nrow, cs = m.u16(g + 4), m.u16(g + 6), m.f32(g + 8)
    minx, miny = m.f32(g + 0xC), m.f32(g + 0x10)
    x0, y0, x1, y1 = m.f32(path), m.f32(path + 4), m.f32(path + 0xC), m.f32(path + 0x10)
    c0 = max(0, min(ncol - 1, int(math.floor((x0 - minx) / cs))))
    c1 = max(0, min(ncol - 1, int(math.floor((x1 - minx) / cs))))
    r0 = max(0, min(nrow - 1, int(math.floor((y0 - miny) / cs))))
    r1 = max(0, min(nrow - 1, int(math.floor((y1 - miny) / cs))))
    return {r * ncol + c for r in range(r0, r1 + 1) for c in range(c0, c1 + 1)}


def check(m):
    """Returns a list of problems (empty = the grid rule holds)."""
    parent = walk_all(m)
    g = find_grid(m, parent)
    seen_nodes, where = set(), {}
    probs = []
    for i, lst in grid_cells(m, g):
        for ln, p in lst:
            if ln in seen_nodes:
                probs.append("list node %08x in two cells" % ln)
            seen_nodes.add(ln)
            if i in where.setdefault(p, set()):
                probs.append("path %08x twice in cell %d" % (p, i))
            where[p].add(i)
    for p, cells in where.items():
        want = covered(m, g, p)
        if cells != want:
            probs.append("path %08x: cells %s, box covers %s" % (p, sorted(cells), sorted(want)))
    return probs
```

- [ ] **Step 2: `check` command on all stock stages.** Add `main()` with subcommands (`argparse`):
  `check <NGP or dump>` (a `.NGP` is inflated with `ngp.inflate_ngp`; a dump is raw image bytes, and a sibling
  `<dump>.reserve` file, if present, maps at the reserve address given by `--reserve 0x...`, default `0x03EFF000`; confirm the
  default against Task 1d). Prints `<name>: N paths, M list nodes, OK` or the problems and exits 1.
  Run: `python tools/stage_map.py check disc/LVL/*.NGP` (all 19 stages).
  Expected: 19 lines ending `OK`. If any stage fails, fix `covered()` (cell order or rounding) until all 19 pass; the
  rule that passes is the one Task 3 implements in C++. Record it in the Task 1 section of the spec.
- [ ] **Step 3: `pieces` command.** A piece (spec "Pieces"): a TRANSLATE (3) or ROTATE (4) node with a parent, whose subtree
  contains an OBJECT (0) or POLY_COLL (42), which has no TRANSLATE/ROTATE ancestor that is itself a piece, and is not a
  root-level TRANSLATE with `id1 == 3000` (pedestrians; `id1 = word >> 18`). Print one line per piece:
  `0x<offset from BASE>  <TRANSLATE|ROTATE>  pos x y z  paths N  [destructible]`, where `pos` is the world position (sum of
  translations, ROTATE fourth row; use `ngp.mul`/`translate_m`) and `paths` the collision paths whose `path[]` contains it.
  Run: `python tools/stage_map.py pieces disc/LVL/CENTRAL.NGP | find /c "0x"` (PowerShell: `| Measure-Object -Line`).
  Expected: about 366 (the research count); write the exact number in the spec's Task 1 section.
- [ ] **Step 4: `make` command.** `make --base CENTRAL --id <id> --name "<NAME>" --out <folder> [--remove 0x..]...
  [--move 0x..=x,y,z[,turn]]...` writes `<folder>/stage.json` (format in the spec; `baseHash` = `"%016x" % fnv1a(image)`
  over the inflated `disc/LVL/<base>.NGP`; `node` as `"0x%x"` offset) after checking each node is a piece. No photo (the
  selection falls back to the base stage's picture). Run:
  `python tools/stage_map.py make --base CENTRAL --id t1 --name "TEST ONE" --out %TEMP%\sm\Test\stages\t1 --remove <first piece from step 3>`
  Expected: the file exists and parses (`python -c "import json;print(json.load(open(r'...')))"`).
- [ ] **Step 5: commit** `tools: stage_map.py (pieces, make, grid check) for the map creator`.

### Task 3: pure header: map file, hash, walk, edits

**Files:** Create `native/wotm_stagemap.hpp`; Test `native/tests/wotm_native_test.cpp` (add `#include "wotm_stagemap.hpp"`).

Interfaces (namespace `wotm::stagemap`); write the tests first, one group per step, run `tools\test_native.bat`, see them
fail, implement, see them pass.

```cpp
// ABOUTME: Map creator, pure part: stage.json, the image hash, the stage hierarchy walk, pieces and the edits (move, turn,
// ABOUTME: remove) with collision-grid relink and sphere widening, on a Mem view of guest memory (tested without the game).
#pragma once
#include "wotm_json.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace wotm::stagemap
{
constexpr uint32_t kBase = 0xA00000;

struct Edit { uint32_t node = 0; bool remove = false; bool move = false; float pos[3] = {0, 0, 0}; float turn = 0; };
struct Map { std::string id, name, base, dir; uint64_t baseHash = 0; std::vector<Edit> edits; };

uint64_t fnv1a(const uint8_t *p, size_t n);              // 64-bit FNV-1a, same as stage_map.py
bool parse(const std::string &json, Map &out, std::string &why);   // checks fields, node hex, "remove" xor move/turn
std::string write(const Map &m);                          // round-trips through parse

struct Region { uint32_t base; uint8_t *p; uint32_t size; };
struct Mem {                                              // guest view: image + reserve (runtime: rdram slices)
    std::vector<Region> r;
    uint8_t *at(uint32_t a, uint32_t n) const;            // nullptr when unmapped
    bool ok(uint32_t a, uint32_t n = 4) const { return at(a, n) != nullptr; }
    uint32_t u32(uint32_t a) const; uint16_t u16(uint32_t a) const; uint8_t u8(uint32_t a) const; float f32(uint32_t a) const;
    void w32(uint32_t a, uint32_t v) const; void wf32(uint32_t a, float v) const;
};
struct Reserve { uint32_t next, end; uint32_t alloc(uint32_t n); };   // 16-aligned bump allocator; 0 when full

std::vector<uint32_t> children(const Mem &m, uint32_t n);             // same table as stage_map.py children()
std::map<uint32_t, uint32_t> parents(const Mem &m, uint32_t imageSize);  // node -> parent (0 for roots)
bool isPiece(const Mem &m, const std::map<uint32_t, uint32_t> &par, uint32_t node);   // same rule as stage_map.py pieces

struct Result { int applied = 0, refused = 0, relinked = 0, fromReserve = 0; std::vector<std::string> log; };
Result apply(const Mem &m, uint32_t imageSize, const Map &map, Reserve &res);   // all edits, in file order
}
```

- [ ] **Step 1: JSON and hash.** Tests: `stagemap_parse` (a valid file with a move+turn and a remove parses to the right
  `Edit`s; missing `base`, bad hex node, `remove` with `move`, unknown key in an edit, non-number pos each fail with a reason);
  `stagemap_write_roundtrip` (`parse(write(m)) == m` field by field); `stagemap_fnv` (`fnv1a("", 0) == 0xCBF29CE484222325`,
  `fnv1a("a", 1) == 0xAF63DC4C8601EC8C`). Implement with `wotm::json::parse`/`dump`.
- [ ] **Step 2: synthetic image builder (test helper).** In the test file, a `struct Img` that lays out nodes in a
  `std::vector<uint8_t>` at `kBase`: header word (roots count + root pointers), `group(kids, center, radius)`,
  `translate(kids, xyz)`, `rotate(kids, mat)`, `object()` (op 0, sphere center 0, radius 10), `polycoll()` (op 42), a
  `path(box, nodes)` and a `grid(ncol, nrow, cell, minx, miny)` whose cells are filled with the coverage rule from Task 2.
  Test `stagemap_walk`: a root GROUP with two pieces (TRANSLATE -> GROUP -> OBJECT + POLY_COLL) and a ped TRANSLATE
  (`id1 3000`) gives `parents()` with every node, `isPiece` true for the two, false for the ped one, the GROUP and the root.
- [ ] **Step 3: remove.** Test `stagemap_remove`: applying `{node, remove}` sets the node's opcode bits to 9 and keeps the
  other bits of the word; the grid is unchanged; `Result.applied == 1`. An edit on a non-piece is refused
  (`refused == 1`, log names the node) and changes no byte (compare the whole image).
- [ ] **Step 4: move with grid relink.** Test `stagemap_move`: a piece with one path covering cell (0,0) moved by +250 in X
  (cell 200) ends with the TRANSLATE `trans` = new position, the path box shifted by the delta, the path listed only in the
  cells the new box covers (use the same helper as the builder), no list node in two cells, and `relinked` counting the
  path. A move that needs more cells than it had takes nodes from the reserve (`fromReserve > 0`) and every new node lies in
  `[res.next before, res.end)`. A move with an empty reserve that needs more cells is refused and leaves the image unchanged.
  Implement: world matrix of the parent chain (row vectors, `p' = p * M`, like `ngp.walk`); `W_old`, `W_new` of the piece;
  each path box's 8 corners mapped by `inverse(W_old) * W_new`, new AABB; unlink the path's nodes from every cell, relink
  into covered cells reusing freed nodes first, then `Reserve::alloc(8)`. Before writing anything, compute everything and
  check the reserve has room, so a refused edit writes nothing.
- [ ] **Step 5: spheres.** Test `stagemap_spheres`: after a move outside its ancestor GROUP's sphere, every ancestor GROUP
  (op 1) between the piece and the root has center unchanged and `radius >= |piece world center - center| + piece radius`
  and `radiusSqrd == radius * radius`; a move inside the sphere changes nothing. Piece radius = max over its direct
  children with a sphere (GROUP +0x10/+0x1C, OBJECT +0x10/+0x1C, LOD +0x10/+0x1C) of `|child center| + child radius`.
  Ancestor spheres are in the space of their own parent chain: convert the piece's world center with the inverse of the
  ancestor's world matrix.
- [ ] **Step 6: turn.** Test `stagemap_turn_rotate_piece`: a ROTATE piece turned 90 has its 3x3 multiplied by `rotZ(90)`
  and its fourth row set to `pos` when `move` is given. Test `stagemap_turn_translate_piece`: a TRANSLATE piece turned 90
  gets a new ROTATE in the reserve (op 4, `numKids` = the TRANSLATE's, children copied, mat = `rotZ(90)` with fourth row =
  the position), the parent's child slot that held the TRANSLATE now holds the ROTATE, every path entry equal to the
  TRANSLATE is replaced by the ROTATE, and the boxes and cells follow the turned corners. **If Task 1e said TURN on
  TRANSLATE pieces is dropped, replace this test by one that refuses the edit with a log line.**
- [ ] **Step 7: run all tests** (`tools\test_native.bat`): previous 133 + the new ones, 0 failures.
- [ ] **Step 8: commit** `native: wotm_stagemap.hpp (map file, walk, move/turn/remove with grid relink)`.

### Task 4: cross-check C++ against the stock grid

**Files:** Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1:** test `stagemap_stock_grid_rule`: if `WOTM_TEST_STAGE` (a raw inflated image written by
  `python -c "import sys;sys.path.insert(0,'tools');import ngp;open(sys.argv[2],'wb').write(ngp.inflate_ngp(sys.argv[1]))" disc/LVL/CENTRAL.NGP %TEMP%\central.bin`)
  is set, load it, build `parents()`, and check the C++ coverage rule gives exactly the stock cells for every path;
  skipped (prints `skip`) when unset, so CI without a disc passes. Run once with the variable set: pass.
- [ ] **Step 2:** apply a move and a remove from the `pieces` list to that image in the test and run the same rule: pass.
- [ ] **Step 3: commit** `native: stagemap checked against a stock collision grid`.

### Task 5: scan `stages/` folders

**Files:** Modify `native/wotm_mods.hpp` (near `scanSkins`, `:360`); Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1: failing test** `mods_scan_stages`: a temp mods folder with `A/stages/m1/stage.json` (valid),
  `A/stages/bad/stage.json` (invalid), `A/stages/m1/photo.png` (any bytes) gives `Registry::stages()` with one entry
  (`id m1`, `dir` the folder, `photo` set), a log line for `bad` with the reason, and `files()` contains no `STAGES/` path
  (the disc override list must not see them).
- [ ] **Step 2:** implement `struct Stage { stagemap::Map map; std::filesystem::path photo; }`, `scanStages(mods)` called
  from `scan` after `scanSkins`, `stages()` (copy under `m_lock`), skip `STAGES/` in the override walk like `SKINS/`
  (`:176`), and a first-wins rule on duplicate ids across mods (log the loser). Log:
  `[stages] map <id> ("<name>", <mod>): base <BASE>, <n> edits`.
- [ ] **Step 3:** test `mods_digest_includes_stages`: changing a byte in `stage.json` changes `digest()` of the mods folder.
- [ ] **Step 4:** run tests, commit `mods: scan stages/ folders (map creator)`.

### Task 6: runtime hooks: hash at `LoadLevelDB`, apply at `dbInitDb`

**Files:** Create `native/runtime/wotm_stagemap.inc`; Modify `native/runtime/wotm_live.inc` (include + install, next to
`wotm_levelsel.inc`), `native/wotm_syms.h` only if a symbol is missing.

- [ ] **Step 1:** `wotm_stagemap.inc` with `namespace wotm_stagemap { void setActive(const std::string &id);
  void install(PS2Runtime &); }`. State (EE thread only): `g_active` (map id or empty), `g_hash`, `g_size`, `g_pending`
  (set by `LoadLevelDB` exit, cleared at `dbInitDb`).
- [ ] **Step 2: `LoadLevelDB` hook** (wrap like `loadLevelFilesTapped`, `wotm_overrides.cpp:1167`): after the original
  returns, read the image size (Task 1a), `g_hash = fnv1a(rdram + 0xA00000, size)`, `g_pending = true`. Log once per load
  with an active map: `[stages] image <BASE> <size> bytes, hash <16 hex>`.
- [ ] **Step 3: `dbInitDb` hook** (`wotm::fn::dbInitDb__FP9_dbheader10_vramAddrs`): on entry, if `g_pending` (and the
  Task 1b level test), clear `g_pending`; if `g_active` names a scanned map whose base is the loaded stage, compare hashes
  (mismatch: `[stages] <id>: base <BASE> does not match this disc` and nothing applied); build a `Mem` with the image and the
  reserve (Task 1d); the image region must be exactly `size` bytes (Task 1a), since `parents()` has no size argument and
  walks whatever the first region maps; reset `Reserve` to its start, `apply()`, log `[stages] <id>: applied N edits, R grid nodes relinked
  (K from reserve)` plus each refusal. Then call the original. Per Task 1c, the guard means a second `dbInitDb` without a new
  `LoadLevelDB` applies nothing.
- [ ] **Step 4: dump.** `WOTM_STAGE_DUMP=<path>`: right after `apply()`, write the image bytes to `<path>` and the reserve to
  `<path>.reserve`; log `[stages] dump <path>`. Add the variable to NATIVO.md's table, run `python tools/wotm_env.py`.
- [ ] **Step 5: `WOTM_MATCH` map id.** In the `WOTM_MATCH` parser (`wotm_overrides.cpp:2589+`), a non-numeric last field
  that names a scanned map sets the stage to its base index (`GameLevelNames` lookup) and calls `setActive(id)`; an
  unknown id logs `[wotm] WOTM_MATCH: map "<id>" not found in mods/` and is rejected like a bad stage.
- [ ] **Step 6: build the runtime**, then run the scripted check by hand:
  `python tools/stage_map.py make --base CENTRAL --id t1 --name "TEST ONE" --out $env:TEMP\sm\Test\stages\t1 --remove <piece> --move <piece2>=<x+300>,<y>,<z>`
  and `tools\live_check.ps1 -Env "WOTM_MODS=$env:TEMP\sm;WOTM_MATCH=3,1,t1;WOTM_AIVSAI=1;WOTM_STAGE_DUMP=$env:TEMP\sm\dump.bin" -Secs 40 -Expect '\[stages\] t1: applied 2 edits'`
  then `python tools/stage_map.py check $env:TEMP\sm\dump.bin`. Expected: `OK`.
- [ ] **Step 7: commit** `runtime: apply map edits at dbInitDb (map creator)`.

### Task 7: stage selection slots 18-23

**Files:** Modify `native/runtime/wotm_levelsel.inc` (`extend`, `levelSelectHook`, `picName` and the window-side picture
code), `native/runtime/wotm_padsel.inc` (next to `resolveRandom`'s call, `:2121`).

- [ ] **Step 1:** in `extend()`, after `kRandom`, append slots `18 + i` for the first 6 maps of `Registry::stages()` (sorted
  by id, so every PC lists the same order) whose base stage is in the list. In `levelSelectHook`, write each map's name
  (upper case, max 17 chars + NUL) into `kLongNames + 18 * slot`. Log `[stages] slot <n>: <id>`.
- [ ] **Step 2:** `resolveMap(rdram)` next to `resolveRandom`: if `m_levelNum` is a map slot, write the base stage's index,
  call `wotm_stagemap::setActive(id)` and return true (the narrator does not announce, like RANDOM); otherwise
  `setActive("")`. Call it from `wotm_padsel.inc` where `resolveRandom` is called. Log `[stages] map <id>: stage <n>`.
- [ ] **Step 3:** picture: for a map slot, the picture switch borrows the base stage's child (as stages 12/13/16 do today)
  and `photo.png` from the map folder, when present, draws over it through the same `levelpics` path.
- [ ] **Step 4:** live check by hand with the keyboard script used by `run_live.bat -Auto` (or `PS2X_PAD` script): reach the
  stage selection, move to the last entries, screenshot (`WOTM_SHOTS`): the map's name and picture show. Confirm and fight:
  log has `[stages] map t1: stage 1` and `applied 2 edits`.
- [ ] **Step 5: commit** `levelsel: saved maps in stage selection slots 18-23`.

### Task 8: checks

**Files:** Modify `tools/live_checks.ps1`, `tools/net_checks.ps1`; maybe `tools/net_check.ps1` (`-Stage` accepting a map id).

- [ ] **Step 1: `map_load`** live check: a `Clean`ed mods folder is made by `stage_map.py make` (run from the check's setup,
  like `creator_script` uses `WOTM_CREATOR`; if `live_checks.ps1` has no setup hook, add a `Setup` scriptblock key), then
  `WOTM_MATCH=3,1,t1 WOTM_AIVSAI=1 WOTM_STAGE_DUMP=...` for 60 s. Expect `\[stages\] map t1 .*|||\[stages\] t1: applied 2 edits`;
  Reject crash/freeze and `does not match`. After the run, `stage_map.py check` on the dump must print `OK` (fail the check
  otherwise).
- [ ] **Step 2: `map_bad_hash`**: the same map with `baseHash` edited: Expect `does not match this disc`, the fight still
  starts (`\[match\]`).
- [ ] **Step 3: `map_online`** net check: 2 players, `-Stage t1` (both PCs with the same mods folder), 60 s: identical
  state (the existing hash comparison). And `map_online_missing`: the second PC without the folder is refused by the mod
  digest (`mods_diferentes` pattern).
- [ ] **Step 4:** run `tools\live_checks.ps1 -Only map_load`, `-Only map_bad_hash`, `tools\net_checks.ps1 -Only map_online`,
  `-Only map_online_missing`, plus the existing `creator_script`, `creator_reload` and `mods_diferentes`: all pass.
- [ ] **Step 5: commit** `checks: map creator load, bad hash, online`.

### Task 9: docs and PR

**Files:** Modify `docs/passos/MODS.md` (new "Stages (maps)" section: folder, `stage.json` fields, `stage_map.py`, limits),
`docs/passos/NATIVO.md` (stage selection slots, `WOTM_STAGE_DUMP`, `WOTM_MATCH` map id), spec status line.

- [ ] **Step 1:** write the docs, `python tools/doclint.py`, `python tools/wotm_env.py`, commit `docs: map creator PR 1`.
- [ ] **Step 2:** final run: `tools\test_native.bat`, the Task 8 checks, `stage_map.py check disc/LVL/*.NGP`.
- [ ] **Step 3:** push `map-creator` to `origin`, open a PR against `creator-v1` titled "Map creator step 1: load
  rearranged stages (#10)" with: what changes, research summary, how it was checked, known limits (spec), and a hand-test
  list for Jake (a map made with `stage_map.py` shows in selection with its name; removed building gone and walkable;
  moved building solid in its new place, not in the old, still destructible; CPU fight runs; online with Lane's mods
  folder matching). Do not merge. Comment on #10 with the link.
