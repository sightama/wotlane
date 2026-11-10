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

## PR 2 checks

Task 1 of the PR 2 plan (`docs/superpowers/plans/2026-11-10-map-copy.md`). Scripts (scratchpad, not committed) on the 19 stock
images inflated with `tools/stage_map.py inflate`, through `stage_map`'s `children`, `walk_all`, `subtree`, `pieces` and grid
readers. Addresses in the generated recompiled code (`recomp/retail/`, not in git) are retail addresses.

**Where the answers differ from the plan** (to settle before Task 2):

1. **Decision 6, `CHAR_INSTANCE` above the source (2e).** The rule refuses **251 world pieces on 11 stages**, not "VEGAS 4 and
   3-MILE 2": 3-MILE 70, SANFRAN 52, AIRPORT 50, CANYON2 35, MINISF 20, CAPITOL 8, TOKYO 8, VEGAS 4, DBALL 2, UFO 1, VOLCANO 1
   (mostly big id1 0 `CHAR_INSTANCE`s: SANFRAN 0x5f56b0 holds 52, 3-MILE 0x557040 65, AIRPORT 0x25cec0 and 0x26dcf0 25 each).
   168 of them need the context (an `OBJECT` with an animation channel index >= 0 at +0x30..+0x3C, `src/common/dbs.cpp:154-163`,
   read by the native renderer only inside a `CHAR_INSTANCE`, `native/wotm_native.hpp:4319-4330`, `:5141-5152`; or animated
   nodes below). 83 have no channel and a static subtree (3-MILE 58, CANYON2 19, TOKYO 5, VOLCANO 1): the wrapper costs them
   nothing. Either keep the rule (safe; AIRPORT keeps 117 of 175 pieces, SANFRAN 146 of 214, CANYON2 10 of 45) or narrow it to
   "a `CHAR_INSTANCE` or `ANIM_XFORM` above and an `OBJECT` with a channel >= 0 in the subtree". Affects Tasks 5, 11, 13.
2. **Decision 3, pickup roots (2a).** Every root over a pickup piece is a GROUP, so "a wrapper in the root's last slot holding
   its old last child" works, but the roots do not all have one child, and one stage has two: CANYON2 has pickup pieces under
   0xd0 (1 child) and 0x706f0 (4 children), so a stage needs **one pickup wrapper per header root**, not one per apply; UFO
   0x1d060 has 6 children and VOLCANO 0x75f0 41, and on UFO, VOLCANO and CANYON2 0x706f0 the old last child is itself a pickup
   piece (0x1d040 `TRANSLATE`, 0x7590 `ROTATE`, 0x70690 `ROTATE`). As for the world root (decision 1), the pickup wrapper must
   read the root's last slot from the live image (a step 1 turn of that piece puts a reserve `ROTATE` there). Pickup root flags
   are 0x0001 on 7 stages and 0x0003 on 8 (the wrapper copies them, as planned). Affects Tasks 6, 11, 13 (a pickup copy on
   VOLCANO or UFO).
3. **Decisions 2 and 12, context lost under the wrapper (2c, 2e).** Besides `CONTROL`, `LOD` and `SWITCH`, copyable pieces sit
   under three more kinds of ancestor whose effect the copy loses:
   - a `DESTRUCTIBLE` state (the piece is part of a building's state): AIRPORT 8, METRO 59, SANFRAN 34. The original goes when
     that building changes state; a copy stays.
   - a mode-masked GROUP (flags & 0xB0): VEGAS 51 (flags 0x0021), CAPITOL 12 (0x0081 9, 0x0023 2, 0x0083 1), 3-MILE 1 (0x0033).
     `dbsTraverse` turns such a GROUP into `NULL` in the masked game modes (`src/common/dbs.cpp:167-168`; masks
     `src/game/db.cpp:117-134`: 0x80 story, 0x10 modes 2/4/5, 0x20 modes 3/6-9), so there the original is not drawn, not solid
     (`hdTraversePath` stops at `NULL`, `src/common/hdTraverse.cpp:179`) and not registered. The copy hangs under the wrapper
     and shows in every mode (VEGAS's 51 appear in free for all).
   - a GROUP with collision-option bits (flags 0x0102: 3-MILE 10, ISLAND 1; `hdTraverse.cpp:135`): a query with that option
     goes through the original and hits the copy.

   Decision 12's line (`CONTROL|LOD|SWITCH 0x... above it does not hang over its copies`) should also name `DESTRUCTIBLE` and
   flagged GROUP, or mode-masked sources should be refused. Known limits get a line either way. Affects Tasks 5, 8 (logs), 11,
   18.

- **2a. Roots.** World root (id1 1) on all 19 stages: a GROUP, flags 0x0003, `ctx` 0, held only by its header slot (no child
  slot of any reachable node points at it). Children 1 (BIGSHOT, CANYON1, TOKYODM) to 10 (VEGAS); the last child is an id1 0
  GROUP on 17 stages (1 child, except TOKYODM 5 and DBALL 8), an `OBJECT` on CRUSH (0x1fc0) and a `ROTATE` with a power-up id
  (0x13F6) on TRAINING (no copyable piece there); `G = {old last child, copies}` works for all. Roots over level pickup pieces:
  13 GROUPs with one child (3-MILE 0x7f00, AIRPORT 0x3d0, CANYON1 0x130, CANYON2 0xd0, CAPITOL 0x510, CENTRAL 0x600 -> 0x5d0850,
  ISLAND 0x690, METRO 0x2e0 -> 0xc6d380, MINISF 0x510, SANFRAN 0x49a0, TOKYO 0x4a0, TOKYODM 0x1920, VEGAS 0x4e0) and the three
  above (CANYON2 0x706f0, UFO 0x1d060, VOLCANO 0x75f0); none is held by a node. No level pickup is under a world root. Roots
  that hold pickups that are not pieces need no wrapper: CANYON2 0x6d0f0 (8 pickups under transforms), METRO 0x6f60 (6 subway
  `CHAR_INSTANCE`s, id1 0x13A3), DBALL 0x254f0 (a `TRANSLATE` root).
- **2b. One holder.** 0 on all 19 stages: no `INTERACTIVE_STATE`, `DESTRUCTIBLE`, `SWITCH`, `PARTICLE_EMITTER`,
  `SCRIPT_ACTION` or level pickup GROUP is held by two child slots, and a walk with `dbsTraverse`'s rules (no visited set, the
  children it pushes, `src/common/dbs.cpp:141-420`; path counts over the DAG) reaches every `INTERACTIVE_STATE`,
  `DESTRUCTIBLE` and pickup GROUP exactly once (no cycles). Task 11's `check` needs no stock exception.
- **2c. Ancestor ids.** Paths through a piece with a nonzero id1 (other than 1) or id2 on a node strictly between the root and
  the piece root: 3-MILE 287 (the stage-wide `SWITCH`, id1 0x2714, a 3-MILE end point) and SANFRAN 87 (`INTERACTIVE_STATE` and
  `DESTRUCTIBLE` of the saves 0x275F and 0x2760 above the piece); 0 elsewhere. With the copy path `[root, wrapper, tail]` the
  hit id1 changes on 238 of the 3-MILE paths and 84 of the SANFRAN ones (`hdTraverse.cpp:127-131`, `:190-191`: a copy's hits
  report id1 1). At run time an `INTERACTIVE_STATE` above the piece also gives the original's hits its Interactives index
  (`Destructible.cpp:160`); 5 paths (3 pieces) on 3-MILE and 2 (1 piece) on SANFRAN have no id2 of their own below, so hits on
  their copies report no interactive (id2 0, as any plain building). Recorded only (decision 2). Also checked for decision 2:
  every `DESTRUCTIBLE` inside a piece is the child of an `INTERACTIVE_STATE` inside the same piece (all 19 stages), so no copy
  path reaches a `DESTRUCTIBLE` before its `INTERACTIVE_STATE` (`hdTraverse.cpp:176` reads `interState->whichChild`).
- **2d. The 17 paths.** All 17 have the nonzero id2 on an id1 0 GROUP between the `INTERACTIVE_STATE` (disc id2 0) and a
  `POLY_COLL` hull with both ids 0: AIRPORT 2 (GROUPs 0x88370, 0x724a0, id2 7), ISLAND 6 (0x672f0, 0x601f0, id2 6), VEGAS 3
  (0x4b8bc0, id2 5), VOLCANO 6 (0x20a7f0, 0x203870, id2 6). Nothing writes them at run time: the id2 writers in `src/` are
  `Destructible.cpp:160` (the `INTERACTIVE_STATE`), `Pickup.cpp:219` (the pickup GROUP), `HeliPickup.cpp:162`,
  `TheGame.cpp:290`, `HomingBug.cpp:240`, `FinalBoss.cpp:217`, `PlantBoss.cpp:519-1205` (CS nodes), `animation.cpp:1333`
  (`CHAR_INSTANCE`) and `particle.cpp:294-296` (`LIGHT` lens flares). The GROUP's id2 overrides the `INTERACTIVE_STATE`'s on
  the path (`hdTraverse.cpp:130-131`), so the original and its copies report the same fixed Interactives index (5 to 7) for
  those hulls, as on the stock stage (`Debris.cpp:471-472` damages it when it is a monster). Known limits line. One `LIGHT`
  sits in a copyable piece (UFO): shared, its lens-flare id2 is shared by the copies.
- **2e. Refusals per stage** (decisions 4-6; a piece can have several reasons; "above" = a `CHAR_INSTANCE` or `ANIM_XFORM` on
  its first-parent chain):

  | Stage | Pieces | Copyable | Special id | Fixed table | Animated (subtree) | Animated (above) | `ACT_ANIM` | Scripted action | Other kind |
  |---|---|---|---|---|---|---|---|---|---|
  | 3-MILE | 298 | 216 | 7 | 5 | 22 | 70 | 0 | 0 | 1 |
  | AIRPORT | 175 | 117 | 0 | 0 | 58 | 50 | 0 | 0 | 0 |
  | BIGSHOT | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
  | CANYON1 | 92 | 92 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
  | CANYON2 | 45 | 10 | 0 | 3 | 4 | 35 | 0 | 3 | 0 |
  | CAPITOL | 145 | 121 | 0 | 6 | 11 | 8 | 0 | 0 | 0 |
  | CENTRAL | 317 | 290 | 0 | 0 | 26 | 0 | 5 | 3 | 2 |
  | CRUSH | 21 | 21 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
  | DBALL | 19 | 8 | 0 | 0 | 9 | 2 | 0 | 0 | 0 |
  | ISLAND | 433 | 417 | 2 | 0 | 14 | 0 | 5 | 1 | 1 |
  | METRO | 703 | 686 | 0 | 0 | 17 | 0 | 4 | 0 | 0 |
  | MINISF | 91 | 68 | 0 | 0 | 3 | 20 | 0 | 0 | 0 |
  | SANFRAN | 214 | 146 | 2 | 0 | 19 | 52 | 0 | 0 | 1 |
  | TOKYO | 218 | 188 | 0 | 0 | 25 | 8 | 7 | 1 | 2 |
  | TOKYODM | 249 | 246 | 0 | 0 | 3 | 0 | 0 | 0 | 0 |
  | TRAINING | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
  | UFO | 91 | 89 | 0 | 0 | 2 | 1 | 0 | 0 | 2 |
  | VEGAS | 250 | 236 | 0 | 0 | 13 | 4 | 0 | 2 | 1 |
  | VOLCANO | 113 | 110 | 0 | 0 | 2 | 1 | 0 | 0 | 0 |

  3476 pieces, 3061 copyable, 415 refused. Locator emitters (id1 0x1D4C..0x1D6B): 0. Special ids: 3-MILE GROUP 0x2711 and
  `SWITCH` 0x2713 (end points) and 5 GROUPs with id1 0xC (`dbProcInteractive` acts only on 1, 2 and 18 below 0x20,
  `src/game/db.cpp:182-196`, so "below 0x20" refuses these 5 needlessly; harmless), ISLAND `SCRIPT_ACTION` 0x2262 and 0x2263
  (user scripts, `db.cpp:239`), SANFRAN GROUP 0x275B (`canyon2AddEpNode` range, `db.cpp:249`), TRAINING `ROTATE` 0x13F6
  (power-up). Fixed tables: 3-MILE 0x2712, 0x2715..0x2718; CANYON2 0x2775..0x2777; CAPITOL 0x27AE..0x27B3. The spec's "29 pieces
  on 8 stages" counted every node of any kind against a narrower list and skipped BIGSHOT, TRAINING and DBALL; recounted that way
  it is 31 on 10 stages (29 without BIGSHOT and TRAINING). Under the plan's split (dispatched kinds; ops 31 and 39 only for the
  fixed tables) it is 12 pieces (special id) and 14 (fixed table), most also animated. "Other kind" is only ever an animation
  node (`ANIM_XFORM`, `CHAR_INSTANCE`, `ANIM_SCALE`) and always comes with "animated (subtree)": no piece is refused for it
  alone, and no `ACTION_DATA` is in any copy set. Dominoes of copyable pieces: retargeted (target in the copy set) CAPITOL 28,
  VEGAS 36; kept (target outside the piece) 3-MILE 2, AIRPORT 6, TOKYO 2, TOKYODM 7, VEGAS 6; none inside the piece but not
  copied.

  **After amendments A1 and A3** (plan, measured with `copySet()` on the 19 images, 2026-11-10): **2969 of 3476 copyable**.
  3-MILE 267, AIRPORT 109, BIGSHOT 0, CANYON1 92, CANYON2 25, CAPITOL 109, CENTRAL 290, CRUSH 21, DBALL 8, ISLAND 417, METRO 627,
  MINISF 68, SANFRAN 112, TOKYO 193, TOKYODM 246, TRAINING 0, UFO 89, VEGAS 185, VOLCANO 111. The table above plus A1's 83 minus
  A3's counts gives the same on 17 stages; 3-MILE (267, not 273) and CANYON2 (25, not 29) differ because A3 was measured before
  A1, on the pieces the old rule left copyable: of the pieces A1 frees, 3-MILE has 6 in a destructible's state (all under a
  `CHAR_INSTANCE`) and CANYON2 has 1 mode-masked and 3 with a script action. Copyable pieces under a GROUP with collision-option
  flags: 3-MILE 10, ISLAND 1. The later rules (a shared node holding a copied one, child counts past 4096, action trees past 20
  levels or 256 actions in a group) refuse no stock piece, and the stage counts per traversal path equal the counts per node
  (2b).
- **2f. Copy sizes** of the copyable destructible pieces. Node bytes = the copy set (decision 4), 16-aligned; total = nodes +
  the private actions of a turned copy (decision 8) + its paths (`0x24 + 4 * len`, 16-aligned) + one 16-byte list node per
  stock grid cell.

  | Stage | Destructible copyable | Nodes median / max (bytes) | Total median / max (bytes) | Largest |
  |---|---|---|---|---|
  | 3-MILE | 138 | 400 / 5984 | 528 / 7584 | 0x20e290 |
  | AIRPORT | 20 | 5040 / 10192 | 5696 / 12496 | 0xad6d0 |
  | CANYON1 | 79 | 816 / 2496 | 944 / 4608 | 0xebc00 |
  | CAPITOL | 14 | 968 / 42880 | 1192 / 61264 | 0x5c4e10 |
  | CENTRAL | 50 | 6224 / 11904 | 7136 / 13328 | 0x37d6e0 |
  | CRUSH | 20 | 5208 / 11824 | 5720 / 12752 | 0x92700 |
  | ISLAND | 115 | 320 / 12208 | 416 / 13808 | 0x346790 |
  | METRO | 137 | 6864 / 18976 | 7552 / 20224 | 0x979760 |
  | MINISF | 30 | 7960 / 10624 | 9864 / 12640 | 0x1ba2f0 |
  | SANFRAN | 86 | 2448 / 7008 | 2856 / 7840 | 0x2a8340 |
  | TOKYO | 43 | 96 / 12336 | 192 / 14816 | 0x255390 |
  | TOKYODM | 108 | 592 / 15152 | 712 / 17280 | 0x1fcb40 |
  | UFO | 78 | 1184 / 16784 | 1352 / 19136 | 0x37d7c0 |
  | VEGAS | 56 | 736 / 60480 | 832 / 71952 | 0x3e7890 |
  | VOLCANO | 69 | 384 / 4256 | 512 / 5024 | 0x143350 |

  (CANYON2's 10 copyable pieces are not destructible; BIGSHOT, DBALL and TRAINING have none.) In line with the spec: 7 to 8 KB
  median on CENTRAL and METRO (200 copies about 1.4 to 1.5 MB), 72 KB for the largest VEGAS piece, and CAPITOL's capitol
  0x5c4e10 at 61 KB. The 4 MB reserve holds them.
- **2g. `MemoryStack::global`.** Cleared by `TheGame::InitBeforeDbLoad` (retail 0x1365A0, `jal clear__11MemoryStack` with
  `$a0` = 0x54D950) right before `LoadLevelFiles` in the session loop (`src/game/Shell.cpp:283-284`), and by
  `Shell::InitBeforeUserintDbLoad` for the menus (`Shell.cpp:568`); `animationInitGlobal` clears another stack (0x7329F0).
  REBUILD LEVEL goes through the session loop again (fresh clear); PLAY AGAIN and RESTART do not reload. After `dbInitDb`'s
  entry the stack serves, in order: the level's traversal (pickups `alloc16` 0x170..0x4C0 plus a 0xC list node,
  `src/game/Pickup.cpp:1101-1175`; `PedGroup` 0x190 per id1 0xBB8 node, `db.cpp:201-203`); then, per monster image,
  `dbsRelocateViaPtrListFile` pushes a mark and reads the whole `mon/<name>.ptr` at `low` (`src/common/dbs.cpp:450-456`, popped
  at :492-493), up to **164376 bytes** (ALIEN0-2, `disc/MON/`), so pickup copies must leave at least that much (plus 16) free
  or the read runs past the pool; then `InitAfterDbLoad`: `PointToolKit::loadPoints` takes 0x4000 + 4 from the high end
  (`src/common/PointToolKit.cpp:35-48`, released only when the file loads), the Central and Vegas military `BoxFormation`s
  (`Pickup.cpp:982`, `:1008-1009`) and `TheGame`'s `pushMark` (`src/game/TheGame.cpp:93`). Task 14 measures, on CENTRAL and
  METRO with 8 monsters: `hi1 - low` at `dbInitDb` entry and exit, the largest `.ptr` read, and `hi1 - low` after
  `InitAfterDbLoad`; `kGlobalKeep` = what is used after `dbInitDb` entry besides the pickups, plus 64 KB (the plan's start,
  0x40000, already covers the 164376-byte read).

  **Measured (Task 14, 2026-11-10).** Map `c0` = one copy of a 32-byte prop (no pickup), `WOTM_MATCH=3,1,2,5,9,8,4,7,c0`
  (8 monsters), `WOTM_AIVSAI=1`; room = `hi1 - low` at `dbInitDb` entry (the runtime's log line) and in `ram_2400.bin`
  (0x54D954, 0x54D960); stock pickups from `stage_counts()`; used = entry - frame 2400 - pickups x 0x170 (the smallest
  pickup: an upper bound of what is not a pickup):

  | Stage | entry (low, hi1) | frame 2400 | pickups | used besides pickups | PedGroups |
  |---|---|---|---|---|---|
  | CENTRAL | 501648 (0x8066FC, 0x880E8C) | 326028 | 115 | 133300 | 16 (6400 B) |
  | METRO | 485252 (0x8066FC, 0x87CE80) | 309156 | 201 | 102128 | 8 (3200 B) |
  | VEGAS | 501648 (0x8066FC, 0x880E8C) | 360864 | 117 | 97728 | 7 (2800 B) |
  | ISLAND | 501648 (0x8066FC, 0x880E8C) | 384368 | 95 | 82320 | 6 (2400 B) |

  `kGlobalKeep` = 133300 rounded up to 16 KB (147456) + 64 KB = **0x34000** (212992 bytes). The transient `.ptr` read needs
  164376 + 16 bytes on top of the pickups and the PedGroups allocated before it (at most 6400 here): 170792 bytes, under
  0x34000. With it the pickup room is 281 KB on CENTRAL, VEGAS and ISLAND and 265 KB on METRO. **Open:** the estimate charges
  every stock pickup `kPickupBytes` (0x500) plus the stock debris pickups, which already passes that room on METRO (338 KB)
  and leaves VEGAS 18 KB and CENTRAL 87 KB: a copy with debris pickups (most destructibles: 0x2d85a0 takes 1536 B) is
  refused after a few dozen on CENTRAL and at once on METRO.
- **2h. `exportPoints` callers.** Besides `PointToolKit::loadPoints` (through vtable slot 3, `PointToolKit.cpp:51-58`), two
  game functions call `PowerUpTool::exportPoints` directly with `$a0` = `PowerUpTool::instance` (0x2AF0C0) and no
  `loadPoints` before: `sanFranUpdate` (retail 0x18D874 and 0x18D8FC, each after `sanFranQuake`, `AiPathTool::exportPoints`
  and `PathTool::exportPoints`) and `threeMileUpdateCountDown` (0x1D24B0, the meltdown, after `turnOffAllVehicles`). The
  four `register_functions.cpp` entries for 0x1881D0 are resume points inside the function. So a second `exportPoints` in the
  same load happens mid-fight on SANFRAN and 3-MILE with `numPoints` already at the appended total: decision 13's guard
  (append nothing then) is needed and right, and the recreated power-ups include the added ones.
- **2i. Native renderer depth.** `Scene::walk` starts at the world root with depth 0 (`native/wotm_native.hpp:2075-2078`) and
  cuts repeated nodes only past depth 24 (`:5013`). Deepest `OBJECT` under the world root: SANFRAN 21; 3-MILE, CENTRAL,
  ISLAND, METRO 20; the others 8 to 19. The wrapper is at depth 1 and a copy's root at 2, so a copy's objects are at most one
  level deeper than its source's: 22 at most, under 24.
