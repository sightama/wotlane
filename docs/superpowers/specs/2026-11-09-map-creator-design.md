# Map creator, step 1 (rearrange a stage): design

Date: 2026-11-09. Branch: `map-creator` (off `creator-v1`, PR #46). Status: approved by Jake section by section; written for
Lane's review. Issue: #10. Sibling: #42 (character creator, skins only: its step 2, custom monsters, was dropped as redundant
with skins, PR #47 closed).

## Goal

Players make new maps inside the game by rearranging a stock stage: move, turn and remove its buildings, props and
destructibles with a free camera, name the result and save. The saved map is its own entry in the stage selection, plays
offline and online, and friends get it by copying a small folder that holds no disc data.

Decisions taken with Jake:
- **Rearrange, not recolor and not new geometry.** A recolored stage is the same kind of thing as the dropped custom monster
  (same thing, new paint). New geometry (glTF) stays a later step of #10.
- **Step 1 edits are move, turn and remove.** Duplicate and add need the stage image to grow, which means a writer for the
  image and its `.PTR` (step 2 of #10 as filed); they come later.
- **A map is an edit list applied at load (approach A)**, not a rewritten `.NGP` in the mod folder. The mod is a few KB of
  JSON, holds no disc data (repo rule: never send game files), and two maps on the same base stage do not fight over one file
  name.

## What the research found (2026-11-09, `CENTRAL` checked by script)

- **No fixups for the stage image.** `LoadLevelDB` inflates the `.NGP` to `0xA00000` and it is used as linked:
  `dbsRelocateViaPtrListFile` returns for file 0 (`src/common/dbs.cpp:448`) and `dbsRelocateFileZero` only patches texture
  bases. A node's address is its file offset + `0xA00000` on every PC and every boot.
- **Collision moves with its piece.** `POLY_COLL` hulls are in local space; `hdTraversePath` (`src/common/hdTraverse.cpp:110-193`)
  applies the `TRANSLATE`/`ROTATE` above them at run time, honours `SWITCH` and `DESTRUCTIBLE` state, and stops at a `NULL`
  node. For all 203 translate-only collision paths in `CENTRAL`, local extents plus the summed translates equal the stored box.
- **The collision grid is baked on disc** (`_HierCollisionGrid`, `include/hieri_types.h:721-733`): one per stage, X/Y cells
  (`CENTRAL`: 18 x 16 cells of 200 units), each cell a linked list of 8-byte `{next, path}` nodes pointing at a
  `_HierCollisionPath` (`hieri_types.h:702-718`) with a **world-space box** and the node path from the root to one hull. The
  game never rebuilds it (`hdSaveColGrid`, `hd.cpp:200-206`); only `csListArr` (moving objects) changes at run time. On every
  stock path, the cells listing it are exactly the cells its box covers (0 mismatches in `CENTRAL`).
- **Positions are copied once in `dbInitDb`.** `Destructibles::AddDestruct` stores position and matrix
  (`Destructible.cpp:177-180`); emitters and scripts get world matrices in the same traversal (`dbs.cpp:144-146`, `383-392`).
  So edits must land after `LoadLevelDB` and before `dbInitDb`'s traversal.
- **Removing a piece is one opcode.** The game itself turns mode-disabled groups into `NULL` (op 9) (`dbs.cpp:167-168`);
  traversal, collision paths and destructible registration all stop there.
- **CPU routes are baked separately.** `PTS/<STAGE>.AIP` (`AiPathTool::loadPoints`, `src/game/AiPathTool.cpp:28`) is a
  world-space route net searched by A* (`AiPathFinder.cpp:27`); `AiNavigator::ObstacleSensor` and the climb/avoid code react to
  live collision. Spawn points (`.SPT`), power-ups (`.PUP`), flyers (`.FOP`) and pedestrians (`.PED`) are world-space files in
  `PTS/` too. None follow a moved piece.
- **Stage selection** already has a native extension: `wotm_levelsel.inc` adds entries, renames them and supplies
  `levelpics/<name>.png`; `resolveRandom` rewrites the index at confirm. Indices 18 to 24 are unused developer slots.

## Saved map

`mods/<mod>/stages/<id>/` (the editor writes `mods/Creator/stages/<id>/`):
- `stage.json`:
  ```json
  {
    "name": "DOWNTOWN MESS",
    "base": "CENTRAL",
    "baseHash": "<hash of the inflated base image>",
    "edits": [
      { "node": "0x61e340", "move": [120.0, -40.0, 0.0], "turn": 90 },
      { "node": "0x62a1c0", "remove": true }
    ]
  }
  ```
  `node` is the piece's offset in the inflated image. `move` is the new translation (absolute, world units) and `turn` the
  rotation about Z in degrees, added to the piece's own rotation. A `baseHash` that does not match the loaded image refuses
  the map (`[stages] <id>: base CENTRAL does not match this disc`) instead of patching the wrong bytes.
- `photo.png`: the stage-select picture, taken from the editor camera on SAVE.
- Temp-then-rename save, as in the character creator.

## Pieces

A piece is the nearest `TRANSLATE` (or `ROTATE`) above a building, prop or destructible (typically
`GROUP -> TRANSLATE -> INTERACTIVE -> DESTRUCTIBLE -> ...`). `CENTRAL` has 317 (rule in PR 1 checks, 1i). Ground hulls with no
transform above them (7 in `CENTRAL`) and pedestrian groups (root `TRANSLATE`s with id1 3000) are not pieces. A piece with
a `ROTATE` gets moved through the translation row of its matrix. Most buildings have only a `TRANSLATE`, so a turn on one
allocates a `ROTATE` in the reserve (see "Grid nodes") whose matrix is the turn with the piece's translation in its fourth row
(`hdTraversePath` treats a `ROTATE` as rotation about its fourth row, `src/common/hdTraverse.cpp:141-149`), copies the
`TRANSLATE`'s children into it, points the parent's child slot at it, and replaces the `TRANSLATE` with it in every collision
path through the piece (same length, edited in place). Pointers are guest addresses, so a node outside the image works as long
as the reserve is not reused while the stage is loaded and the native renderer accepts it; PR 1 checks both first and, if
either fails, TURN is limited to pieces that already have a `ROTATE`.

## Loading a map

1. **Selection.** Each saved map takes a slot from 18 to 23 (6 maps listed at once, the picture switch has 24 children; more are a later step) with its name and
   photo, through `wotm_levelsel.inc`. On confirm the index is rewritten to the base stage's index and the map becomes the
   active edit set, so the stock files, music, stage code and `PTS/` files all load as normal.
2. **Hook.** A new hook at the entry of `dbInitDb` (`0x0012E418`) applies the active edit set when the loaded file zero is a
   level (it is also called for menus and the pre-shell). For each edit:
   - **move / turn:** write the translation and rotation; for every collision path through the piece, recompute its
     world-space box and relink its grid nodes to the cells the new box covers; widen the culling sphere (+0x10 center,
     +0x1C radius) of the ancestor groups above the piece. Spheres below the piece are local and unchanged.
   - **remove:** set the piece's opcode bits to `NULL`. Its collision paths leave the grid (traversal would stop at the
     `NULL` anyway) and their list nodes go to a pool that the moves use before the reserve.
3. **Grid nodes.** A moved path that covers more cells than before needs more list nodes than the image holds. Edits are
   checked in file order (the first edit of a node stands), then all removes run in file order, then the moves and turns in
   file order. A move first takes all its paths out of the grid; its list nodes join the pool, which holds the removed
   pieces' nodes and what earlier moves left over; nodes come from the pool first, then from a small reserved area of
   guest RAM above the image (the runtime already uses
   memory above `0x2000000`, `wotm_overrides.cpp:1182-1186`). A piece moved past the grid's extent would lose collision; the
   loader clamps and logs it, and the editor never allows it.
4. **Determinism.** The edits are written from the EE thread at one fixed point and depend only on the map files, which the
   mod digest (`wotm::mods::digest`, `native/wotm_mods.hpp:106`) already requires to match on every PC online.

Log: `[stages] map <id> ("<name>", <mod>): base CENTRAL, 12 edits` at scan; `[stages] <id>: applied 12 edits, 31 grid nodes
relinked (4 from reserve)` at load.

## Editor

- **Entry:** on the stage selection, triangle on a stock stage or a saved map opens EDIT. The stage loads with no monsters and
  the free camera (`WOTM_FREECAM`), with a native panel in the style of the character creator.
- **Picking:** a crosshair at the screen centre; the piece whose bounding sphere the view ray hits first is highlighted (tint)
  and named on the panel (`PIECE 0x61e340`).
- **Controls (keyboard and pad):** fly with the stick or arrows; GRAB (Space / X) holds the piece; while held, the arrows
  slide it on the ground relative to the camera in 10-unit steps (held = faster), two keys raise and lower it, two keys turn it
  by 15 degrees. DELETE, UNDO (several levels) and RESET PIECE (back to its disc place). Rows NAME, SAVE, BACK as in the
  character creator, with its OVERWRITE? and NAME IN USE messages.
- **Live preview:** moving a translation moves the drawing at once (the native renderer reads the image). Collision and
  destructibles only follow when the map is loaded to play; testing is save, then fight.
- **Guard rails:** spawn points and power-up places show as markers, and a piece covering one turns red (BLOCKS SPAWN, saving
  is still allowed). Moves stop at the grid's edge. CPU routes from `.AIP` can be drawn as thin lines.
- **Save** writes the folder above plus the photo, registers the map live (no restart) and lists it in the selection.
- **PR 2 note:** PR 1's `dbInitDb` hook tells the stage's load from the shell's by the NGP count (`wotm_stagemap.inc`,
  `dbInitDbTapped`: 2 or more, the stage plus at least one monster). The editor's "stage with no monsters" loads one NGP and
  must revisit that test before it relies on the load-time apply.

## Delivery

Two PRs, stacked on `creator-v1` (the editor reuses the creator's panel, name entry, OVERWRITE flow and live registration).
Nothing is merged by us; Lane reviews.

1. **Loading maps, no editor.** `stage.json` parse and checks, the `dbInitDb` hook (move, turn, remove, grid relink, spheres),
   the selection slot with name and photo, and `tools/stage_map.py` to make a map without the editor (`pieces` lists a
   stage's pieces, `make` writes a map folder with the right `baseHash`, `check` runs the grid rule on an image or a RAM dump).
2. **The editor.**

## Testing

- **Grid rule:** every collision path is listed in exactly the cells its box covers. A native test and
  `tools/stage_map.py check` (on the stock images and on RAM dumps of edited maps) run it on all 19 stock stages, unedited
  and edited.
- **Native tests:** `stage.json` parse, `baseHash` refusal (the hash is taken right after `LoadLevelDB` inflates the image, before anything patches it), piece rules (what is pickable, TURN availability), remove as `NULL`,
  grid relink including reserve nodes.
- **Live checks** (scripted, with `WOTM_DISC` set so text and screenshots work): a removed building is gone and can be walked
  through; a moved building is solid in the new place and not in the old, and still destructible; a CPU fight on an edited map
  runs.
- **Online:** a 2-player `tools\net_check.ps1` on an edited map keeps identical state; a player without the map is refused.
- **Hand tests** for Jake listed in each PR.

## Known limits (step 1)

- No duplicate or add, no ground or terrain edits.
- CPU routes (`.AIP`) are not updated: CPUs that hit a moved building climb or steer around it.
- Spawn and power-up places stay where the disc puts them.
- Trigger zones stay where the disc puts them (except 3-MILE's 13 under pieces, which follow).
- Pickups on a rooftop float when their building moves (a pickup is its own piece).
- Level pickups are placed by `dbsTraverse`'s callback with the traversal transform, and its case 3 in `src/common/dbs.cpp`
  is an untuned draft (the game runs the original code). Confirmed on the running game (live check `map_load`): a moved
  `CENTRAL` pickup is placed at its new spot (`createPickup`'s position, read from the fight's RAM) and none at the old one.
- Water and kill zones (`WaterGrid.cpp`) and `RST/<STAGE>.MRS` are not mapped yet; PR 1 checks them.
- A `CONTROL` above a moved piece (the see-through "ghost" building switch) keeps its sphere and BSP volumes where the disc
  puts them, so the switch to the ghost version follows the old place; the loader logs it (1j).
- Six maps listed at once in the selection.
- Online in step 1 goes through `WOTM_MATCH` (a map id in the stage field); picking a map in the launcher's lobby needs a
  string stage in the lobby protocol and is a follow-up.

## PR 1 checks

Task 1 of the PR 1 plan. Addresses in the generated recompiled code (`recomp/retail/`, not in git) are retail addresses.

- **1a. Image size and hash point.** `LoadLevelDB` (`src/game/Shell.cpp:1373-1390`) is its own function
  (`LoadLevelDB__5Shell` 0x001A9AB0, `native/wotm_syms.h:4212`), called only from `LoadLevelFiles` through the runtime's
  dispatch (hookable like `LoadMonstersDB`). It calls `zipInflateAll(path, 0xA00000)` then `fileOnlyNgpFile`, which keeps
  only the end rounded up to 0x80 plus 0x80 in `fileStatus.ngpAddr[1]` (`src/common/file.cpp:394-405`); the bytes between
  the real end and that are stale RAM, and `loadMonstersDBHigh` overwrites `ngpAddr[1]` with 0x2000000 right after
  (`native/runtime/wotm_overrides.cpp:1190-1200`). **Decision:** the exact size is the return of `zipInflateAll`
  (0x0022B620, `wotm_syms.h:6495`), taken by a tap on `zipInflateAll` while the `LoadLevelDB` hook is inside the original
  (dest 0xA00000); the hash covers `[0xA00000, 0xA00000 + size)` and is taken on the hook's exit (`ngpAddr[1]` there is
  only a sanity bound). Between `LoadLevelDB` and `dbInitDb`, `LoadLevelFiles` calls only `LoadMonstersDB`,
  `LoadResTexture`, `LoadTexture`, `getVramAddr`, `dbsRelocateFileZero` and progress/print helpers (`Shell.cpp:1850-1871`,
  same list in the recompiled function). The first three write past the image end (next load address, or 0x2000000 with
  high RAM: `Shell.cpp:1239-1240`, `:1315-1316`, `:1417-1418`), and `texmResInit`/`texmInit` touch only the RTX/TEX files
  (`src/common/texm.cpp:156-193`). `dbsRelocateFileZero` (`src/common/dbs.cpp:504-537`) reads `lvl/<name>.PTR` into the
  next texture load address and patches, inside the image, TEX0.TBP0 (bits 5-18 of the high word) at each offset of the
  PTR's third list; edits never touch TEX0 words, so the two commute. A mod that replaces `LVL/<STAGE>.NGP` changes the
  hash and the map is refused, as designed.
- **1b. Level or not at `dbInitDb`.** Every caller passes 0xA00000 (`Shell.cpp:264`, `:1871`, `:1987`, and
  `DisplayLoadBackground`, which loads `shell/load.ngp` at 0xA00000 and calls `dbInitDb`), so the argument says nothing.
  **Decision: flag.** The `LoadLevelDB` hook sets `g_pending`; the `dbInitDb` hook consumes and clears it. In
  `LoadLevelFiles` no other `dbInitDb` runs between the two (callees above).
- **1c. Restarts.** PLAY AGAIN keeps the session (`m_inSession` 1, monsters re-initialised) and the pause RESTART (RESET
  POSITIONS) runs `TheGame::ResetLevel` and `rtMain(0)` (`Shell.cpp:316-327`, `src/game/TheGame.cpp:798-827`): no load and
  no `dbInitDb`, so the edited image stays as it is, which is what we want. REBUILD LEVEL (`finishedMultPlayBattleLevel`,
  retail 0x1AD0D0-0x1AD0D8) sets `m_restart` 1, `m_inMenus` 1 and `m_inSession` 0: `EvaluateGameStatus` calls
  `DisplayLoadBackground` (`load.ngp` over 0xA00000, `Shell.cpp:662-665`) and the session loop runs `LoadLevelFiles` again
  (`Shell.cpp:274-331`), so it is a **fresh image** and the edits must be applied again. **Decision:** the flag of 1b is the
  once-per-load guard (one apply per `LoadLevelDB`); the active map stays set for the whole session (until the next stage
  confirm) instead of being consumed by the first load. The `WOTM_KILLTARGET` row of `docs/passos/NATIVO.md` says REBUILD
  LEVEL rebuilds "without reloading"; the retail code above reloads, and the Task 6 live check should log two loads.
- **1d. Reserve area.** `PS2_RAM_SIZE` is 64 MB (0x4000000) and the game sees 32 MB (`PS2_GUEST_RAM_SIZE`,
  `runtime/PS2Recomp/ps2xRuntime/include/runtime/ps2_memory.h:47-49`). Top-of-RAM slots in `native/`: -0x1000
  (`wotm_split4.inc:59`), -0x800 (`wotm_modes4.inc:58`), -0x400, -0x3E0 (`wotm_modes4.inc:771`), -0x3A0, -0x380, -0x360
  (`wotm_pausemenu.inc:30-32`), -0x300 to -0x290 (`wotm_levelsel.inc:31`). Nothing in `native/` or the runtime uses
  [0x3EFF000, 0x3FFF000). Loads above 32 MB end at about 0x2B10000 with 8 monsters on Metro (`NATIVO.md`, "Up to 8
  monsters"), 20 MB below. **Decision:** `kReserve = PS2_RAM_SIZE - 0x101000`, 1 MB, with 16-byte aligned allocations
  (`_hierrotate` is `aligned(16)`, `include/hieri_types.h:517-525`); Task 6 makes `wotmCheckLoadEnd`
  (`wotm_overrides.cpp:1205-1234`) warn against `kReserve` instead of the end of RAM. Game code reaches it: `READ32` and the
  fast paths mask with `PS2_RAM_MASK` (64 MB), and only BIOS, scratchpad, I/O, GS and VU ranges are special
  (`ps2_runtime_macros.h:223-230`, `:313-319`; `runtime/ps2_address.h:50-66`). Monster NGPs already live above 32 MB and are
  walked by `dbsTraverse` (`src/game/db.cpp:140-152`), `hdTraversePath` and the hier traversal through the same loads. No
  game code tests a node pointer against the image range: 0xA00000 appears in `src/` only in the loaders and
  `dbsRelocateViaPtrListFile` (`dbs.cpp:454`).
- **1e. Native renderer.** `Ram::ok` accepts any address in [0x1000, 64 MB) (`native/wotm_native.hpp:45`; the live `Ram`
  is the whole RAM, `native/runtime/wotm_live.inc:819`). `Scene::walk` draws (i) nothing for a `NULL` node (op 9 falls to
  `default: break`, `wotm_native.hpp:5250`) and (ii) a `ROTATE` anywhere in RAM (`:5112-5118`: matrix at +0x10, count at
  +8, children at +0x50). Caches are keyed by mesh and texture, not by transform node (`cache_`, `tex_`, `:1112-1121`), and
  `levelSignature` reads only the image header (`wotm_live.inc:374-380`). **Nothing to change; TURN on TRANSLATE-only
  pieces stays in PR 1.** No `TRANSLATE` on a collision path carries action data (+0xC) on any of the 19 stages, so the
  `ROTATE` (pad at +0xC) loses nothing.
- **1f. Grid rule.** Cell index `row * nCols + col` (`src/common/HdIterator.cpp:35-48`, `hd.cpp:233-236`), col from X and
  row from Y. Rule: cols `floor((boxMin.x - minX) / cellSize)` to `floor((boxMax.x - minX) / cellSize)` inclusive, rows the
  same with Y, clamped to [0, nCols - 1] and [0, nRows - 1] (as `hdGetSphereGridBox`, `hd.cpp:210-226`). Script on all 19
  stages: 4348 paths, 0 mismatches (truncate-inclusive and floor/ceil-exclusive also give 0, since no stock box ends exactly
  on a cell line; floor-inclusive is the safe one, a superset of what `HdItGridBox::reset`, `HdIterator.cpp:228-251`,
  visits). No list node is shared by two cells and no path is listed twice in a cell. Every stage has one grid with
  200-unit cells and no stock box leaves [minX, maxX] x [minY, maxY]; the stored `maxX`/`maxY` can be less than
  `minX + nCols * cellSize`, so the move clamp uses the stored max.
- **1g. Stage selection slots 18-23.** `screenLevelSelect` (`src/game/screen.cpp:3128-3264`) takes any level number from
  the list: name from `GameLevelLongNames[28][18]` (`:3106`, rows 18-23 are developer names), lock from `levelUnlocked`
  (`:4889-4917`: 18-23 hit the default, open), picture through `hierSetSwitch(m_sw5D0, level)`. The `kList` copy holds 24
  entries (`wotm_levelsel.inc:31`); it has 17 today (retail, the missing 1-16, RANDOM), so 6 maps fit. In `SHELL.NGP` the
  picture switch has 24 children and 17-23 are the same kind of object as 16 (one texture, half of Tsunopolis); `kOwn`,
  `g_obj` and `g_pics` (`wotm_levelsel.inc:159`, `:300-301`) already have room for indices up to 23. Slots 18-23 are below
  0x1D, so `LoadLevelDB` would use `GameLevelNames[slot]` (no file), never `host0:` (`Shell.cpp:1376-1383`). Rewriting
  `m_levelNum` in the `changeScreen(8, 99)` hook where `resolveRandom` runs (`wotm_padsel.inc:2119-2124`) is enough: the
  narrator and `extraCpus` run after it in the same hook, `InitRTState` copies `m_levelNum` at session start
  (`Shell.cpp:578-586`), and the selection itself keeps only the list cursor.
- **1h. Water and kill zones (record only).** Water is a global height per stage (`WaterLevel`, set through
  `setWaterLevel` by `TheGame::InitBeforeDbLoad` and Three Mile's countdown; `src/game/Monster.cpp:101-112`) plus the
  surface type of collision polygons, which move with their hull. The `WaterGrid` constructor has no direct caller in the
  recompiled code. `RST/<STAGE>.MRS` is the stage's reset (tweak) file (`src/common/resetcom.cpp:293`): background color,
  ambient, fog, parallel lights, time of day (`resetcom.cpp:60-86`), dust and particle colors and the default ground height
  (`src/game/reset.cpp:28-55`); nothing tied to pieces. Trigger zones are `COLL_TRIG` collision paths in the grid. Most hang from their
  own root (id1 5, e.g. `CENTRAL`'s 18 under 0x2510) under a `TRANSLATE` or `ROTATE` with no geometry, which is not a
  piece: they stay where the disc puts them. On all 19 stages only 13 trigger paths go through a piece, all on 3-MILE
  (under the world root); those follow their piece through the same relink, so the relink includes trigger paths.
- **1i. Piece rule** (`tools/stage_map.py`, mirrored by the loader). Walk: depth-first preorder from the header's roots in
  order, children in slot order, a node reached twice keeps its first parent. The children table needs `ANIM_XFORM` (17,
  count +4, children +0xCC), `SKEL_BONE` (35, count +4, children +0xC) and `ANIM_SCALE` (38, count +4, children +0x60) as
  well as the plan's list: without them 527 node entries of stock collision paths are unreachable. With them every node of every
  stock path is reached and each step of a path is a parent-child link, except 39 steps (AIRPORT, CAPITOL, ISLAND, VEGAS,
  VOLCANO) that leave a `NULL` on disc, where `hdTraversePath` stops anyway. **Rule:** a piece is a `TRANSLATE`
  or `ROTATE` with a parent, whose subtree holds an `OBJECT` or `POLY_COLL`, with no transforming node (`TRANSLATE`,
  `ROTATE`, `SCALE`, `ANIM_XFORM`, `ANIM_SCALE`) on its first-parent chain, and either (a) whose chain ends at the root
  with id1 1 (the world root, the only one drawn: `viewSetWorldEpNode`, `src/game/db.cpp:185`, `view.cpp:351`) or (b) whose
  first node with a nonzero id1, in preorder of its subtree (itself first), has a level pickup id (0x12C0-0x13EB; placed by
  `LevelPickups::createPickup` with the traversal transform, `db.cpp:222-223`). Roots are told apart by id1, never by
  offset. The plan's "no piece above" let in 48 nodes of `CENTRAL` under an `ANIM_XFORM` (animated vehicles and parts,
  whose place comes from the animation) and, on METRO and VEGAS, nodes under a `SCALE`; with no transform above, a piece's
  own translation (a `ROTATE`'s fourth row) is its world position, which is what `move` writes. `CHAR_INSTANCE` and
  `SKEL_BONE` do not transform and do not block. Root-level transforms (pedestrians, id1 3000) and what is under them are
  excluded by the same rule, and so are the id1 0 template roots (HUD elements, weapon and power-up nodes, particle
  textures, shell end points, the font; 3-MILE's power-up template root id1 5107) and `CENTRAL`'s 39 unreferenced
  `TRANSLATE -> OBJECT` pairs that are never drawn. No piece has two parents on any stage. `CENTRAL`: **317 pieces**
  (94 `ROTATE`): 202 under the world root, 55 of them on collision paths (all destructible), and 115 level pickups. Per
  stage (world + pickups): 3-MILE 298 (264 + 34), AIRPORT 175, BIGSHOT 1, CANYON1 92, CANYON2 45, CAPITOL 145, CENTRAL
  317, CRUSH 21, DBALL 19, ISLAND 433, METRO 703, MINISF 91, SANFRAN 214, TOKYO 218, TOKYODM 249, TRAINING 1, UFO 91,
  VEGAS 250, VOLCANO 113.
- **1j. `CONTROL` ancestors** (found in the Task 3 review). Pieces with a `CONTROL` (op 8) above them, per stage: UFO 78,
  SANFRAN 72, ISLAND 48, VOLCANO 28, MINISF 20, CAPITOL 15, AIRPORT 12, CANYON2 12, CENTRAL 0. A `CONTROL` holds a sphere
  (+0x10 center, +0x1C radius) and BSP volumes (+0xC count, +0xE type, +0x20 pointers) in its own space, which is world
  space here; it picks child1 (solid) or child2 (ghost, or nothing) from where the eye is (`NATIVO.md`, "CONTROL"). The
  native `Scene::ctrlChild` (`native/wotm_native.hpp:4948`) reads only the volumes, not the sphere; the retail
  `hierCtrlNode` (0x2048F0) is asm and its use of the sphere was not checked. PR 1 does not change `CONTROL` data: a moved
  piece under one logs `CONTROL <node> above it keeps its see-through volumes and sphere where the disc puts them`.
  Task 8's live checks include a moved piece under a `CONTROL` (UFO or SANFRAN).
