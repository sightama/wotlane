# Map creator, step 2 (copy pieces): design

Date: 2026-11-09. Branch: `map-copy` (off `map-creator`, PR #48). Status: approved by Jake; written for Lane's review.
Issue: #10. Builds on `docs/superpowers/specs/2026-11-09-map-creator-design.md` (step 1: move, turn, remove).

## Goal

A map can copy any building, prop or placed pickup of its stage and place the copies anywhere, each with its own turn. A copy
is a real piece: solid, destructible on its own (destroying a copy leaves the original standing), with its own effects, and it
drops its own rooftop pickups. Copies are what makes a rearranged stage feel like a different map (Jake's hand test of step 1:
moving a few buildings did not).

Roadmap chosen by Jake (2026-11-09), one PR each: copy (this), the in-game editor (with copy and group tools: clear a
district, move a block), mixing stages (pieces from other stages with their textures), look and mood (sky, time of day, fog,
music, retint).

Decisions taken with Jake:
- **Copies are built in guest RAM at load**, like step 1's turn: no `.NGP` writer and no `.PTR` rewrite (step 2 of #10 as
  filed is no longer needed for copies). The map file stays a small edit list with no disc data.
- **Only per-instance state is copied; geometry is shared.** A copy costs about 8 to 14 KB for a typical destructible
  building (CENTRAL, METRO), up to 77 KB for the largest (VEGAS).

## What the research found (2026-11-09, all 19 stages by script)

- **Where copies hang.** Only the image header's root list points at the world root (id1 1); it is never another node's child.
  `dbsTraverse` starts from the header (`src/game/db.cpp:146`), `viewSetWorldEpNode` keeps the node in `worldCtx[i].ep`
  (`src/common/view.cpp:351-356`), the native renderer walks `worldCtx.ep` (`native/wotm_native.hpp:2075-2078`), and
  `hdTraversePath` checks only GROUP flags, SWITCH and DESTRUCTIBLE links on a path (`src/common/hdTraverse.cpp:131-176`).
  So copies go under one new reserve GROUP `G` that takes the world root's last child slot: `G = {old last child, copy1..N}`,
  with the parent's flags (0x0003) and a sphere covering all of them; the root's sphere is widened as in step 1. The header is
  not touched (`levelSignature` hashes header pointers, `native/runtime/wotm_live.inc:374-380`). Pickup copies go under a
  second wrapper in the pickup root's last slot (CENTRAL root 0x600, last child 0x5d0850); never under the world root
  (the game draws pickups through their own CS: they would draw twice). METRO's pickups are nested (0x2e0 -> 0xc6d380).
- **Per-instance state (must be copied):** INTERACTIVE_STATE (`id2`, `whichChild`, `Destructible.cpp:160, 389, 398`; disc
  `id2` is 0 everywhere), DESTRUCTIBLE (`hdTraversePath` compares `child[state]` with `path[i+1]`), SWITCH (`ChangeState`
  writes `whichChild = 1`, `Destructible.cpp:367-372`), PART_EMITTER (0xB0 bytes: `isActivated`, `cachedPosition`,
  `cachedOrientation`, `hier.cpp:751-840`; 44 to 84% of a copy's bytes), SCRIPT (matrix written by `dbs.cpp:146`), the pickup
  GROUP and its first SWITCH (`Pickup::init` writes `numKids = 1`, `whichChild`, `id2`, `Pickup.cpp:192-201, 216-219`; copy
  them from their disc state at `dbInitDb` entry), and every ancestor of those inside the piece.
- **Shared:** OBJECT (traversal only rewrites bit 11 from the node's own fields, `dbs.cpp:157-160`; weapons and power-ups
  already draw one node many times per frame through CSs, `PowerUps.cpp:269`, `Weapon.cpp:1188`; the native cache is keyed by
  mesh, and `already()` only cuts below depth 24, the deepest OBJECT is at depth 21), POLY_COLL, COLL_VOL, BSP (local hulls;
  hits take `id2` from the copied INTERACTIVE; 17 paths on AIRPORT, ISLAND, VEGAS, VOLCANO have a nonzero `id2` below an
  INTERACTIVE: checked in PR), debris chunks (drawn at the destructible's matrix).
- **Action trees:** `ACT_DOMINO` targets inside the piece are retargeted to the copy (CAPITOL 28, VEGAS 38); targets outside the
  piece (TOKYO 16, TOKYODM 9, AIRPORT 6, VEGAS 6, 3-MILE 2) are kept and logged (a copy would knock down the original's
  neighbour). `ACT_HATPICKUP` offsets get a private, turned copy (`ActionDispatch.cpp:390-397`).
- **Collision:** a copy's path is the original path with each copied node replaced by its copy (same length, same ancestor
  GROUPs, so flags and hit ids stay right), `0x24 + 4 * len` bytes with its own `traceId`; box = moved box (turned: bounding box
  of the turned corners), linked into the grid with the step 1 rule. Median paths / grid nodes per destructible piece:
  CENTRAL 5/24, METRO 4/13, MINISF 7/64, ISLAND 1/1.
- **Limits:** Interactives has 1024 slots, slot 0 reserved, and **no bounds check** (`Interactives.cpp:40-47`): overflow
  corrupts memory. Stock peak METRO about 415 (206 interactives + 201 pickups + up to 8 monsters). Destructibles 1500 (stops
  silently, `Destructible.cpp:157`); CS pool 1024 shared with monsters, debris and shots (`CsPool.cpp:34`); particle effects 100
  (overflow returns -1, safe); auto emitters 128 (guarded); `DelayAction` pool 64. Pickups come from `MemoryStack::global`
  (0x170 to 0x4C0 bytes each; its free room is measured in PR). Fixed tables keyed by id1: CAPITOL pillars (8, 6 used),
  CANYON2 pillars, SanFran saves.
- **Memory:** 200 copies of a typical CENTRAL or METRO building is 1.6 to 2 MB. The reserve grows from 1 MB to 4 MB:
  base `0x3BFF000`, still about 17 MB above the 8-monster load end (~0x2B10000).
- **Determinism:** Interactives indices follow the preorder walk and pickup ids the `initAfterDbLoad` list order; copies in the
  last slots register after every original, in edit-file order, so originals keep their stock indices.

## Saved map

New edit kind in `stage.json`, one per copy:

```json
{ "copy": "0x2d85a0", "at": [768.1, 499.6, 74.1], "turn": 90 }
```

`at` is the copy's world position (absolute, like `move`); `turn` is degrees about Z added to the source's rotation. The
source must be a piece that step 1 can edit and is not refused below. Copies are applied after removes and moves, in file
order. A copy of a piece that is also moved, turned or removed copies the source's **disc** state (the edit list is applied to
the image after copies are built from the stock nodes).

`tools/stage_map.py make --copy 0x..=x,y,z[,turn]` (repeatable) writes copy edits; riders of the source (step 1's rule) are
copied too, each to `at + turn(R - P)` with the same turn, unless `--no-riders`.

Power-up places riding the source (step 1's power-up rider rule, `PTS/<STAGE>.PUP`) are copied as new points:

```json
{ "addpowerup": 4, "from": [777.0, 600.7, 630.2], "at": [-88.1, 634.1, 630.2] }
```

`addpowerup` names the disc point whose type and arg are copied; `from` guards it as in step 1. The `exportPoints` hook
appends the new points after the stock ones (indices stay stable, in file order) while `numPoints` stays at or under the
tool's 256 slots; past that the rest are refused and logged.

## Refused sources (logged, the rest of the map applies)

- Pieces holding special ids: monster, pedestrian, weapon, power-up, script or level end-point ranges (29 pieces on 8 stages),
  and the fixed id1 tables (CAPITOL and CANYON2 pillars, SanFran saves).
- Animated pieces: ANIM_XFORM, CHAR_INSTANCE, or an `ACT_ANIM` action (2 to 58 per stage).
- Copies past the cap: the loader counts the stage's Interactives (interactives + pickups + 8 monsters) plus each copy's
  INTERACTIVE_STATE and pickup nodes, and refuses copies that would go past 1000 (headroom under 1024), and past 1400
  destructibles.

## Loading

`apply()` gains a copy step after moves and turns:
1. Pick the copy set: the piece root plus every node in its subtree that holds per-instance state (list above) and every
   ancestor of such a node inside the piece. Everything else is shared.
2. Allocate the copies in the reserve (16-byte aligned), copy their bytes from the stock image, point copied parents at copied
   children, set the copy's transform (a ROTATE when turned, as in step 1).
3. Copy each DESTRUCTIBLE's action tree down to its HAT and DOMINO actions; turn HAT offsets; retarget in-piece dominoes.
4. Build the copy's collision paths (copied nodes swapped in), box them and link them into the grid.
5. Add the copy to `G` (world pieces) or to the pickup wrapper (pickups), widening spheres.
Atomic per copy, like step 1's edits. Log: `[stages] <id>: copied 0x2d85a0 x12 (96 KB, 60 grid nodes)` and refusals.

Context a copy loses by hanging under `G` (logged, Known limits): a CONTROL above the source (the copy is always solid, no
see-through), a LOD above it (always at that detail), a SWITCH above it (stage-wide switching skips it; 3-MILE 263 pieces).

## Testing

- **Native tests:** copy set selection, pointer rewrite inside the copy (no copied node points into the source's per-instance
  nodes), shared leaves really shared, paths and grid rule after copies, action retarget and HAT turn, cap refusal, atomicity,
  golden digest for a fixed copy set.
- **Disc-backed test:** copies on CENTRAL, METRO, VEGAS (dominoes), ISLAND (LODs) pass the grid rule in C++ and
  `stage_map.py check`.
- **Live checks:** a copy is solid (player 1 lands on a copy placed under the start point); destroying a copy leaves the original
  intact (state of the original's INTERACTIVE in RAM); 200 copies load within the reserve and the cap is enforced (a map asking
  for 700 copies applies up to the cap and logs the rest); the Interactives count after load is read from RAM.
- **Online:** `map_copy_online`, 2 players on a copy-heavy map, identical state.
- **Demo map for Jake:** CENTRAL with the park filled with towers and a wall of copies.

## Known limits (step 2)

- Copies only within the same stage (mixing stages is the next-but-one PR).
- No copies of animated or special-id pieces; dominoes that point outside the piece knock down the original's neighbour.
- See-through, LOD and stage-wide switching context is lost on copies (see above).
- About 300 to 500 copies per map depending on the stage (Interactives cap).
- CPU routes, spawn and power-up places unchanged (as in step 1).
