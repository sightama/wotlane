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
`GROUP -> TRANSLATE -> INTERACTIVE -> DESTRUCTIBLE -> ...`). `CENTRAL` has about 366 with geometry. Ground hulls with no
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
   - **remove:** set the piece's opcode bits to `NULL`. Its grid nodes are left in place (traversal stops at the `NULL`).
3. **Grid nodes.** A moved path that covers more cells than before needs more list nodes than the image holds. They come
   first from removed pieces' paths, then from a small reserved area of guest RAM above the image (the runtime already uses
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
- Water and kill zones (`WaterGrid.cpp`) and `RST/<STAGE>.MRS` are not mapped yet; PR 1 checks them.
- Six maps listed at once in the selection.
- Online in step 1 goes through `WOTM_MATCH` (a map id in the stage field); picking a map in the launcher's lobby needs a
  string stage in the lobby protocol and is a follow-up.
