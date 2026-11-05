# Character creator, step 1 (recolor skin): design

Date: 2026-11-05. Branch: `creator` (off `wotlane/mac`). Status: approved by Jake from a clickable mockup; written for Lane's
review. Issue: #42 (step 1 of 3). Sibling: #10 (map creator) shares the entry and the tint tool.

## Goal

Players make a costume of their own inside the game and fight with it, online too, without the mod kit: a CREATE row in the
native main menu opens a panel where they pick a monster and a costume, tint each of its textures (hue, saturation, brightness)
with a live 3D preview, name it and save. The result is an ordinary mod skin (`mods/Creator/skins/<id>/`: `skin.json` plus one
PNG per tinted texture), so it loads through what exists today (`docs/passos/MODS.md`, "Skins"), shows in the selection as one
more costume of that monster, and friends get it by copying the folder (the mod digest already refuses a network match where it
is missing).

Decisions taken with Jake:
- **Step 1 is recolor only.** No portrait, no custom monster fields, no geometry. Those are steps 2 and 3 of #42.
- **Entry:** a fifth row, CREATE, on screen 2 (the SINGLE PLAYER list) under TRAINING. Screen 1 is already at the 7-row cap
  (`kMaxRows`), and CREATE sits naturally next to TRAINING as the other "not a match" entry.
- **Controls:** one block of three sliders (HUE, SATURATION, BRIGHTNESS) that edits the texture chosen on a TEXTURE row. Every
  texture keeps its own three values. Nine rows, no scrolling (three rows per texture would scroll for most monsters).
- **Preview:** the selection's 3D model (`wotm_model.hpp`), idle animation, redrawn with the tinted textures on every change.
- **Save:** PNGs through raylib's `ExportImage`, `skin.json` through `wotm_json.hpp`, the skin added to the registry at once so
  it is in the costume list without a restart (fallback below).

## Section 1: the row and the panel

**Row.** `kScreens[]` in `native/runtime/wotm_mainmenu.inc` gets `{kCreatorRow, "CREATE"}` as the fifth row of screen 2
(`kCreatorRow = -4`, an item that is not the game's, like `kTrainingRow`). The cursor logic that handles `kTrainingRow`
(`g_extraRow`, mouse hover, up and down) treats `kCreatorRow` the same way: the game's `currentSelection` stays on
FREE-FOR-ALL while our row is lit. X on it opens the creator panel the way X on RECOMPILED OPTIONS opens the options panel
(`g_open`, `g_suppress`, the game sees no input while it is open); triangle on the panel closes it and the cursor stays on
CREATE. The panel plumbing is tied to screen 1 today (`v.panel` is 0 on any other screen, `g_open` drops when `currScreen`
leaves 1, and the input gate reads `cur == 1`): those three places accept screen 2 as well when the open panel is the creator.
Leaving screen 2 (triangle to the main menu) closes nothing: the panel only exists while open.

**Panel.** The same texture (`g_prt`, `kPW x kPH`), box, title row, rule, row highlight, arrows, hint line and legend as the
options panel; `View` gets `bool creator` (like `training` and `moves`), and `renderPanel`, `panelInput`, `panelLayout`,
`panelHit` and the legend take a creator branch. Layout: the VIDEO tab's with the preview box (rows on the left 60%, the box on
the right), title CREATE, no tabs; L1 and R1 badges stay but do nothing. Rows, top to bottom:

| Row | Value | Left and right | X |
|---|---|---|---|
| MONSTER | name from `kMonsters` (`wotm_mods.hpp`), the ten playables | previous and next monster; resets COSTUME to 0, the sliders to 0 and NAME to the default | nothing |
| COSTUME | `1`..`4` (disc costume + 1) | previous and next costume that exists on the disc (`MON/<PREFIX><n>.NGP`); resets the sliders | nothing |
| TEXTURE | `T<id> k/N` (texId of the texture, `k` of `N` textures of the model) | previous and next texture | nothing |
| HUE | `-180`..`+180` in steps of 15, shown with `°` | step | nothing |
| SATURATION | `-100`..`+100` in steps of 10 | step | nothing |
| BRIGHTNESS | `-60`..`+60` in steps of 10 | step | nothing |
| NAME | up to 11 letters (the HUD limit), cursor under one letter while editing | editing: move the cursor; else nothing | enters and leaves editing |
| SAVE | (centered) | nothing | saves (Section 3) |
| BACK | (centered) | nothing | closes the panel |

Hints (the line under the big screen, `wotm_legend::hint`) per row, in the voice of the other panels: MONSTER "WHICH MONSTER TO
RECOLOR", COSTUME "THE COSTUME WHOSE TEXTURES ARE THE STARTING POINT", TEXTURE "WHICH PART OF THE SKIN THE THREE SLIDERS BELOW
CHANGE", HUE "TURNS THE COLORS OF THIS TEXTURE AROUND THE WHEEL", SATURATION "GRAYER TO THE LEFT, STRONGER TO THE RIGHT",
BRIGHTNESS "DARKER TO THE LEFT, LIGHTER TO THE RIGHT", NAME "THE COSTUME NAME IN THE SELECTION. X TO EDIT" (editing: "LEFT AND
RIGHT MOVE, UP AND DOWN CHANGE THE LETTER, X DONE"), SAVE "WRITES THE SKIN TO MODS/CREATOR/SKINS AND ADDS IT TO THE COSTUME
LIST", BACK "LEAVES WITHOUT SAVING". Legend: d-pad MOVE, left and right CHANGE, X SELECT, triangle BACK (editing the name:
left and right MOVE, up and down LETTER, X DONE).

**Name editing.** While editing, up and down roll the letter under the cursor through space, A to Z, 0 to 9 and `-`; left and
right move the cursor over the 11 slots; X (or triangle) leaves editing. On the keyboard, typing a letter sets the slot and
advances the cursor. The default name is `<MONSTER> 2`; `3`, `4`... when that id already exists in `mods/Creator/skins/`.

**Costumes that exist.** `wotm_monsters::costumes(type)` (`native/net/wotm_monsters.hpp`) says how many disc costumes a
monster has; COSTUME cycles through those.

**Id.** The name in lowercase, with every run of characters outside `a-z0-9` replaced by `_` and edges trimmed (`skin.json`
ids allow only letters, digits and `_`, up to 32). Empty after that: `skin`.

**Mouse.** Hover selects the row and the arrows step the value, as in the options panel (`panelHit` uses the same row
geometry). A click on SAVE, BACK or NAME is X.

**State** lives on the EE thread in a `wotm_creator` namespace (new file `native/runtime/wotm_creator.inc`, included from
`wotm_mainmenu.inc` next to the training rows, since it needs the panel's statics): monster index, costume, texture index, per
texture `{hue, sat, bright}`, name, cursor, editing flag, row, `saved` message. `publish` copies it into `View` (new fields:
`creator`, `creatorRow`, `creatorHint`, and a `CreatorPreview {monsterType, costume, base, gen, tints[]}` struct) under `g_m`,
so the window thread reads a snapshot.

## Section 2: the preview

The window thread keeps one `wotm::model::Model` and one `wotm::model::Renderer` for the creator (`wotm_creator::Preview`,
inside the `.inc` but only touched by the window thread, like `SelModel`). On every drawn frame with `v.creator`:
- if the published `(monsterType, costume)` differs from the loaded model's, load `MON/<PREFIX><costume>` the way `selModel`
  does (same fallbacks, `computeFraming(kIdle)`), and keep a copy of its decoded textures as the untinted originals;
- if the published tint generation differs from the last applied, tint each original with that texture's values
  (`wotm::tint::shift`, Section 4) into the model's texture list through a new `Model::setTextures(std::vector<TexImage>)`,
  which bumps `gen()` so the Renderer reuploads them (`Renderer::upload` already keys on `gen()`);
- pose idle by time since the model was loaded (`pose(kIdle, frame)`), render into the Renderer's target with the disc
  costume's framing (`savepics::Drawer::framingOf`, yaw 0), and draw that target into the panel's preview box with the same
  premultiplied blend the selection uses.

The costume's texture list and texIds come from the model: `Model::textures()` plus a parallel `Model::texInfo()` (the `id`
each `TexImage` was decoded from in `texIndex`, its `rtx` flag and whether its raw alpha goes above 0x80). Right after `load`,
`Model::decodeAllTextures()` decodes every OBJECT slot the file draws with, visible in the idle pose or not (switch children,
other detail levels, hidden parts), so the list covers what a fight can show and has the same order in the preview's Model and
the save's. The same disc texture can appear twice (same texId under two palettes or GS settings), and a fight replaces a
texture by `(rtx, texId)` whatever the palette, so the TEXTURE row, the tints and the PNGs are per group of entries with the
same `(rtx, texId)` (up to 32 groups). The starting pixels are what the game would draw: if a retexture mod already changes
that costume, its PNGs are the starting point. Textures the model does not reference are not offered (they would not show anyway).

Nothing touches the game's RAM: the preview is native drawing only, like the selection's model.

## Section 3: save

X on SAVE, on the EE thread:
1. Load the same model (`Model::load`, the EE thread's own instance, read from the disc or ISO through `wotm::disc`), decode
   its textures, tint each one with its values.
2. Write `mods/Creator/skins/<id>/` under the mods folder the registry uses (`Registry::dir()`; `mods/` next to the ELF or the
   repo's, `wotm_mods.inc`), creating `Creator/` if needed:
   - `skin.json`: `{"monster": "<name>", "id": "<id>", "name": "<NAME>", "base": <costume>, "ai": false}` through
     `json::dump`;
   - one PNG per texture whose values are not all zero, named `t<id padded to 4>.png` (`rtx_t<id>.png` for an `.RTX` texture),
     the bare form `texmods::Index::parseName` accepts for skins, at the texture's own size, through `ExportImage` (the
     `TexImage`'s RGBA, alpha as decoded, which `texmods::loadPng` reads back on the same scale; for a texture whose raw alpha
     goes above 0x80, `loadPng` reads the PNG raw and the decoded pixels saturate everything from 0x80 up, so the save writes the
     texture's own raw alpha bytes, `Model::rawAlpha` through `creator::applyRawAlpha`: the fight draws the disc's alpha and the
     selection what the preview showed). A texture with all-zero
     values is skipped: the base costume's own texture serves (the registry aliases whatever the folder lacks).
3. Register it without a restart: `Registry::addSkin(dir)` scans that one folder with the same rules as `scanSkins` (next
   costume index for the monster, aliases for the files it lacks, its PNGs into `skinPngs()`), and
   `texmods::Index::get().replaceSkin(stem, pngs)` puts the new PNGs into the map (retiring the costume's previous entries, so a
   texture an overwrite dropped falls back to the base costume's). No other skin moves: their costume indices and the
   sector ranges of existing mod files stay as they are (the new skin brings no `.NGP`/`.TEX`, so no new sectors either); only
   this costume's own PNG entries are replaced (retired, not freed, so a reader holding one stays valid).
   `Index` gets a mutex around `build`, `find` and `add`, because the window thread decodes models while the EE thread adds.
   `Registry::skins()` is read by the selection's EE-thread hooks and by the window only on the selection screen; the creator
   is open on the main menu, so the add has no concurrent reader. A new costume index means a new `g_selCache` key, so the
   selection loads it fresh.
4. Log `[creator] saved mods/Creator/skins/<id> (N png)` and show `SAVED` as the SAVE row's value for 3 seconds; the panel
   stays open. The registry's own line `[mods] skin togera/<id> ("NAME", Creator) = costume 4 (LIZARD4); base 1, N png`
   follows from the add.

**Name in use.** A skin folder holds one monster's skin: if `mods/Creator/skins/<id>/skin.json` names another monster, SAVE
shows `NAME IN USE` and writes nothing; the player changes the name. Saving deletes the folder's PNGs it did not write, so an
overwrite from another costume leaves no stale textures.

**Overwrite.** If `mods/Creator/skins/<id>/` exists, SAVE first shows `OVERWRITE?` as its value; the next X overwrites (the
registry entry is replaced: same index if the skin was registered this run, else added), triangle or moving off the row
cancels. A skin of the same id in another mod is not touched (the registry's "beats" rule decides, as today).

**Fallback.** If `addSkin` reports it could not register (over 99 costumes, stem over 8 letters, or the folder failed the
`scanSkins` checks), the files stay on disk, the log says why, and the SAVE row shows `SAVED. RESTART TO USE` (a period: no comma could be confirmed in the game font's atlas). This is also the
path if the live add turns out to fight something unforeseen in the selection: the files are right either way.

**Errors.** A folder or PNG that cannot be written: `[creator] cannot write <path>: <reason>` and the SAVE row shows
`CANNOT WRITE`; nothing is registered. No disc model (a costume missing on the disc): COSTUME skips it, so this does not reach
SAVE (if the model still fails to load, `[creator] MON/<file>.NGP not found` and `NOT FOUND`, nothing written). Mods off
(`WOTM_MODS=0`): `MODS ARE OFF`, nothing written.

## Section 4: the tint and the scripted run

**Tint** (`native/wotm_tint.hpp`, pure, no raylib): `void shift(std::vector<uint32_t> &rgba, float hueDeg, float sat,
float bright)` converts each pixel to HSV, adds `hueDeg` to the hue (wrapping), scales saturation by `1 + sat/100`, scales
value by `1 + bright/100`, clamps and converts back; alpha untouched. All three zero is the identity, bit for bit. It is the
same function the map creator (#10, step 1) will use on stage textures.

**Scripted run** (`WOTM_CREATOR=<monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]`, for example
`WOTM_CREATOR=togera:1:green:120`): at install (`wotmCreatorInstall`, after `wotmModsInstall`), the EE thread runs Section 3
with those values applied to every texture, the id as given and the name = the id in uppercase, then the game continues to
boot. A bad value logs `[creator] WOTM_CREATOR ignored: <reason>` and nothing is written. The variable goes in the NATIVO.md
table, and it is what `live_check` drives.

## Section 5: what is out of scope in step 1

- The HUD portrait stays stock colored (it lives in the shell's `.RTX`, not the costume's).
- No sharing button: friends copy `mods/Creator/skins/<id>/`.
- No custom-monster fields (name on the HUD, narrator, camera): step 2 of #42, same panel over a base type.
- No marking of in-game skins in the costume list beyond the mod name `Creator` the registry already logs.
- Online: a skin is drawing only; nothing in the lockstep state changes. Picking the skin itself in a lobby needs the lobby
  and server to carry a mod costume (today `net_lobby.py`, `LobbyClient.cs` and `wotm_net_server.cpp` hand out disc costumes
  0 to 3 only): a protocol change, later, with #14.

## Section 6: testing

- **Native tests** (`native/tests/wotm_native_test.cpp`, `tools\test_native.bat`): `tint_identity_and_hue` (zero shift is bit
  identical; +120° turns pure red into pure green and keeps alpha; saturation −100 grays; brightness clamps),
  `creator_id_from_name` (`"TOGERA 2"` -> `togera_2`, `"  Green!! Togera  "` -> `green_togera`, empty -> `skin`),
  `creator_skin_json` (the dumped text parses back to the five fields), `creator_env_parse` (`togera:1:green:120` and the
  short forms; `bogus:1:x` and `togera:9:x` rejected).
- **Live check** (`tools\live_check.ps1`): `-Env "WOTM_CREATOR=togera:1:green:120;WOTM_MODS=<temp mods folder>" -Expect
  "\[creator\] saved .*skins[\\/]green \(\d+ png\)|||\[mods\] skin togera/green"`, then a second run with the same
  `WOTM_MODS` and no `WOTM_CREATOR` expecting the `[mods] skin togera/green` line (the folder loads at boot like any mod).
- **Lockstep** (`tools\net_check.ps1 -Players 2 -ExtraEnv "WOTM_MODS=<that folder>"`): both PCs with the created skin in
  their mods folder form the match and keep identical state (the digest agrees); the existing `mods_diferentes` entry already
  covers one PC without it (refused, "game configuration differs"). A fight with the skin itself online waits for the lobby
  change in Section 5.
- **By hand:** open CREATE, tint, save, pick the costume in the selection and in a fight; screenshot of the panel with the
  preview for the issue (`docs/screenshots/<date>/`).

## Files

| File | Change |
|---|---|
| `native/wotm_tint.hpp` | new: HSV shift |
| `native/wotm_creator.hpp` | new, pure (testable without raylib): id from name, `WOTM_CREATOR` parsing, `skin.json` text, PNG names |
| `native/runtime/wotm_creator.inc` | new: state, rows, input, save, preview, `WOTM_CREATOR` |
| `native/runtime/wotm_mainmenu.inc` | CREATE row, `View::creator`, branches in `renderPanel`, `panelInput`, `panelLayout`, `panelHit`, legend |
| `native/wotm_model.hpp` | `texIds()`, `setTextures()` |
| `native/wotm_mods.hpp` | `addSkin(dir)` (the per-folder part of `scanSkins` factored out) |
| `native/wotm_texmods.hpp` | mutex, `add(SkinPng)` |
| `native/runtime/wotm_overrides.cpp` | `wotmCreatorInstall` after the mods |
| `native/tests/wotm_native_test.cpp` | the four tests |
| `docs/passos/NATIVO.md` | section "Runtime: character creator (CREATE in the menu, `WOTM_CREATOR`)" and the env table row |
| `docs/passos/MODS.md` | "Skins": a paragraph on skins made in game |
