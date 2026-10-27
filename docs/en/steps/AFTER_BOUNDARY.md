*English translation of [docs/passos/POS_FRONTEIRA.md](../../passos/POS_FRONTEIRA.md). The original is authoritative; this copy may lag behind it.*

# After the hardware boundary: priorities (owner, 2026-10-19)

Applies once the hardware boundary falls ([`BOUNDARY.md`](BOUNDARY.md), work in another session). Until then the order in [`NEXT_STEPS.md`](../NEXT_STEPS.md)
(port inventory) still holds. The list is the owner's; the order and the notes were agreed with him in the same conversation. Each item says what the repo already
knows and what is still to be found out.

## Order
| # | Item | Depends on | Estimated cost |
|---|---|---|---|
| 1 | Guest RAM above 32 MB (+ measure 4 views): **done on 2026-10-19** | - | low |
| 2 | Enemy HUD: health and energy together, portrait on the left: **done on 2026-10-19** | - | low (base ready) |
| 3 | Save in readable JSON, by names: **done on 2026-10-19** | - | medium |
| 4 | `mods/` folder that overrides disc files | - | low/medium |
| 5 | No "memory card 1 or 2" screen; extra slots in the JSON | 3 | low/medium |
| 6 | Autosave icon | 3 | low |
| 7 | Modern (orbital) camera with camera-relative movement | - | high |
| 8 | Mods that add skins and monsters (without replacing) | 3, 4 | medium/high |
| 9 | Training mode with hitboxes and damage on screen | - | medium |
| 10 | Playable bosses | 8, 9 | high |
| 11 | "2 players" becomes "multiplayer", up to 4 players | 1 | high |
| 12 | Selection with a 3D preview of everyone selected | 1, 11 | medium |
| 13 | Campaign (adventure) with up to 4 players | 11, 7 | the highest |

**In any gap** (cheap, no dependency): remap controls in the launcher, photo mode, PC graphics (section "Gaps").

Why this order: 1 unlocks half the list and is cheap. 2 is a quick win on top of a mod that already exists. 3 and 4 come together because
they define how mods refer to things (names, not indices); 3 → 5 → 6 use the same save hook. 7 comes before multiplayer
because 4 views with the old camera are worse, and because the group campaign needs it. 9 comes before 10 because balancing bosses
and mod monsters calls for seeing hitboxes and damage. 10 is a special case of "add a monster" (8). The 4-player campaign comes last:
cinematics, bosses and rules were made for 1 player.

### 1. RAM above 32 MB
- Measured in [`investigations/2026-10-18/16_monsters.md`](../investigations/2026-10-18/16_monsters.md): with 8 monsters the METRO load goes up to 0x23B4300 (35.7 MB) and freezes
  (`g_frame` 205). The other stages fit; San Francisco and Vegas are left with ~2 MB of headroom.
- It is "step 2" of that plan: the runtime's `PS2_RAM_SIZE` is parametric (`ps2_memory.h`, power of 2). Raise it to 64 MB and move the
  ceiling of the load area (today ~0x1F10000) through a hook in `fileOnlyNgpFile`/`getNextNgpLoadAddr`. 32-bit pointers and a flat bank:
  the `dbs` relocation does not change. The native layer already uses `wotm::Ram{rdram, PS2_RAM_SIZE}`.
- It touches a PS2Recomp seam: regenerate `tools/runtime_patch/wotm-runtime.patch`.
- Check alongside: `fileStatus` with 14 slots (stage + 12 monsters), `texInfo` with 1000, resident VRAM of the `.RTX`.
- **Measure the cost of 4 views here** (scene drawn 4 times; with 8 monsters the CPU is already at ~9 ms per frame) before investing in the
  interface of item 11.

**Done on 2026-10-19.**
- **64 MB of RAM, the game sees 32.** In the runtime patch (`ps2_memory.h`): `PS2_RAM_SIZE` 64 MB and `PS2_GUEST_RAM_SIZE` 32 MB. The stack of the
  main thread (`SetupThread` with stack -1), `GetMemorySize`, the runtime heap and the stack of the asynchronous callbacks follow the
  second one (`System.cpp`, `ps2_runtime.cpp/.h`), so everything the game allocates stays where it was; the upper 32 MB only receive what the
  hooks send there. Addressing is by mask (`PS2_RAM_MASK`) and the translated code has no hardcoded 32 MB. Both executables
  change (the game code is compiled once for both).
- **Loading above 32 MB** (`wotm_overrides.cpp`, `loadMonstersDBHigh`): on entry to `Shell::LoadMonstersDB` the next load
  address (`fileStatus.ngpAddr[ngpLoaded]`, right after the stage NGP) becomes 0x2000000. The monster NGPs and, behind them, the RTX and TEX
  of the stage and of the monsters go up; only the stage NGP stays below. The monster relocation (`dbsRelocateViaPtrListFile`) adds
  "address - 0xA00000" in 32 bits and works at any address. `WOTM_HIGHRAM=0` goes back to the PS2's contiguous loading. The line
  `[wotm] carga da fase` (stage load) says how much is left up to the end of RAM.
- **Checked:** Metro with 8 monsters (before, it froze while loading) loads up to ~0x2B10000 (21 MB of headroom) and fights for 60 s in both
  executables, without the watchdog; menu through the pad script up to the fight; corpus with no regression. The 32 MB snapshots of the corpus still
  open (the replay accepts RAM smaller than the build's and zeroes the rest); the new ones have 64 MB in the whole frame. The same 8-monster fight in Centro
  with and without `WOTM_HIGHRAM`: the same divergent objects in the VU1 x native check, no different texture.
- **Cost per view** (AI x AI fight in Centro, 70 s, `wotm.exe`, 180 Hz monitor; `WOTM_FIELDLOG` with `vistas=`/`hier=`/`cap=` and
  `PS2X_STATS` with `[native] redesenho:` (redraw); the 2nd view comes from `WOTM_PLAYERS=2`, which alternates split and unified screen):

  | Monsters | Views | EE per frame (median / p99) | Native redraw per frame (wall / CPU) |
  |---|---|---|---|
  | 2 | 1 | 5.8 / 8.0 ms | 8.9 / 7.2 ms |
  | 2 | 2 | 8.2 / 12.0 ms | 15.1 / 14.0 ms |
  | 8 | 1 | 10.0 / 14.5 ms | 11.7 / 10.5 ms |
  | 8 | 2 | 13.8 / 18.3 ms | 18.6 / 17.9 ms (96.5% of frames in 1 field) |

  Each extra view costs ~2.5 ms of EE with 2 monsters and ~3.8 ms with 8 (the `hier` and what it pulls in), and ~6.5 to 7 ms of CPU on the
  window thread: native drawing is CPU-bound (submission through raylib), not GPU-bound, and the half width of the view barely makes it cheaper. The
  two threads run in parallel, so the ceiling is the slower one. Extrapolating to 4 views: EE ~13 ms with 2 to 4 monsters (fits), but the
  redraw ~27 ms (does not fit in 16.7). **Before item 11, native drawing needs to get cheaper per view** (what repeats per
  view: collecting and submitting each object; candidates: static stage meshes in GPU buffers, batching by texture, per-view culling before
  submission). With 8 monsters and 2 views the window already exceeds 16.7 ms today.

### 2. Enemy HUD
- Base ready: `WOTM_ENEMYBARS=1` ([`NATIVE.md`](NATIVE.md), variable table) already replaces the health icon with health bars (`m_health`/
  `m_maxHealth`) and energy bars (`StaminaMeter`, +0x460), drawn natively through the hook on `Hud::drawHealthIcon`/`ScreenPolys::addPoly`.
- **Done on 2026-10-19.** A single block above each enemy's head: portrait at full height on the left; on the right, health on top and,
  below, the special button and the energy (thinner and shorter). Each piece has its own black outline, the gray band between them is
  shared and there is a black outline around the outside.
- Portrait: last entry of the monster's `.RTX` (T8H 128x64; left half normal, right half rage). The hook finds the monster's file
  through the `epNode` of `m_cs` (+0xC) and the ranges of `fileStatus.ngpAddr`, and searches the NGP for that portrait's TEX0
  (`tbp0 == maxResAddr[i-1]`); cached per stage load. Rage = `m_boostFull` (+0xF4), the same key as the `hierSetSwitch` of
  `Hud::update`. The texture is upside down in VRAM (the game draws with V = 0x80 - v): the rectangle goes from V 64 to 0.
  Special lit = `m_specialWeapon` (+0xF3) outside mode 9, like `isSpecialAvailable`, in the red of the player's HUD icon.
- The native side gained a textured screen rectangle (`ScreenRect::tex0`, `wotmBarTex`), respecting the split screen clipping.
- Checked: fights with 3 enemies (`WOTM_MATCH=3,1,2,5,1` and `1,1,2,3,1`), the 3 portraits found and upright like the player's; rage
  forced in a test build shows the other half; special forced lit in red, the same as the player's with `WOTM_CHEATS=esp`.
  Photos in `docs/screenshots/2026-10-19/2026-10-19_hud_inimigos_bloco*`.
- Open: a block near the top of the screen can end up over the player's HUD (the position comes from the head, clamped by the game).

### 3. Save in JSON
- Today the runtime's `sceMc*` stub (`ps2xRuntime/src/lib/Kernel/Stubs/MemoryCard.cpp` in the fork) writes and reads files in `mc0/` on the host
  (port 1 in `mc1/`, next to it). The one that assembles the blocks is `game/MonsterMc` (the pages and what goes in each file); `game/McFile`
  (tasks `McLoadFile`/`McSaveFile`/`McSetupGameDir`/`McScrambleIcon` over `sceMc*`) and `game/McPage` (menus) are platform.
- **Layout mapped on 2026-10-19 from the asm** (without needing a diff between saves) and checked on the save that exists in
  `discroot/mc0/BASCUS-97197` (decodes with nothing left over). `python tools/mcsave.py <pasta>` shows both files as JSON by name.

**Files** (folder `BASCUS-97197`, title "War of the Monsters"):

| File | Bytes | Content | Built by / applied by |
|---|---|---|---|
| `BASCUS-97197` | 0x380 | options, battle tokens and unlocks (`_SettingsData`) | `MonsterMc::getSettingsData` / `setSettingsData` |
| `savegame.dat` | 0x280 | 10 adventure slots (`_SaveGameData`) | `MonsterMc::getSaveData(slot)` / `setSaveData(slot)` |
| `icon.sys`, `ICON.ICO` | 964, 61120 | PS2 icon (item 6) | `McSetupGameDir`; `McScrambleIcon` rewrites `icon.sys` on every options save |

**No checksum.** Both reads (`McLoadSettings::update`, `McSelectReel::refreshSaveData`) only check the `0x12345678` marker at the end
of the block; a wrong marker becomes a card error. No pointers: everything is a 32-bit `int`, little-endian.

**`BASCUS-97197` (options):**

| Off | Source | In the example save |
|---|---|---|
| 0x00 | `TheGame+0x1203CC` (`m_matchMode`; the `Shell` lights `m_difficultySw[valor]`: it is the difficulty) | 0 |
| 0x04, 0x08 | `Shell::m_sfxVolume`, `m_musicVolume` (0..0x400) | 1024, 830 |
| 0x0C | `Shell+0x2A40`: the constructor sets 50 and nobody else reads it (dead option?) | 50 |
| 0x10 | `Shell::m_mono` (becomes 0/1 on load) | 0 |
| 0x14, 0x18 | `Shell::m_ctlrCfg[2]` | 1, 1 |
| 0x1C, 0x20 | `Shell::m_vibration[2]` | 1, 1 |
| 0x24 | battle tokens (`Shell+0x2B24`) | 55975 |
| 0x28..0xE7 | `monsterSelectMode[i].state` of the 48 `SelectSlot` (only `state`; `taken`, price and names stay in the ELF) | see below |
| 0xE8..0xF3 | `TheGame+0x1204B0..B8`, 3 ints: not identified (`game.h` puts the end of `m_actuator` there, inferred; `Weapon` reads neighbors as tweaks) | 0, 0, 0 |
| 0xF4..0x34B | **not written**: the buffer is on the stack and this stretch writes garbage (32 non-zero words in the example) | - |
| 0x34C | marker `0x12345678` | |

The 48 flags are 12 groups of 4. Groups 0 to 9 are the monsters in the order of the `aiMonsters` of `Shell::RandomlySelectAI` (Raptros, Togera,
Preytor, Congar, Robo-47, Agamo, Ultra V, Magmo, Kineticlops, Zorgulon), and entry `j` of the group is costume `j`. Groups 10 and 11 are
the stages of `levelUnlocked`: entry 40 Volcano, 41 Mini Baytown, 42 Capitol, 43 UFO, 44 Dodge Ball, 45 Big Shot, 46 Crush; 47 is left over.
The boot's `McCheckForSweetTooth` looks for `BASCUS-97101/globalFile` (the Twisted Metal: Black save) on the card and, if found, turns on
entry 23 ("Agamo Costume 4", the Mecha Sweet Tooth; `ROCK3`) and plays `ShellSound::playSweetUnlockedSound`. On PC this unlock is the
key `unlocks.mechaSweetTooth` in the JSON, which takes precedence over `agamo.skins[3]` (created with the card's state when missing).
The names and prices of each entry are in the table itself (`SelectSlot::name`/`cost` in the ELF) and confirm the order of the groups; entry
47 is a "Level 4" costing 5000 tokens that no code reads.

**`savegame.dat` (adventure):** +0 number of slots used (the highest index saved + 1); slots at `0x10 + i*0x3C`; marker at 0x268; the rest
is zero. Each slot uses only 0x14 of the 0x3C bytes:

| Off in slot | Source | On load (`setSaveData`) |
|---|---|---|
| +0x00 | `Shell::m_levelNum` (stage where the player stopped) | same; the AIs come from `m_levelNumAIs[fase]` and `levelMonsters[fase]` |
| +0x04 | `Shell::m_monsterSel[0]` (`monstro << 5 \| variante`) | same |
| +0x08 | `Shell::m_lives[0]` | same |
| +0x0C | `Shell+0x2B28 + Shell+0x2B2C` (stage points + accumulated) | all into `+0x2B2C` |
| +0x10 | `Shell::m_costume[0]` | same; and `m_numPlayers = 1`, `m_mode = 1` |

Saving a slot (`McSaveAdventure::onEnter`) reads the whole `savegame.dat`, replaces only that slot (`copySaveData`) and writes the 0x280 back
again; with the file missing (`McSaveAdventureBlank`) it starts from a zeroed block with the marker. The 10 slots are the "reels" of the load
screen (`McSelectReel`, `McLoadGameBoxes`); card 2 gives another 10.

**When the game saves** (page sequences built in `MonsterMc::update*`): `updateBootLoad` (boot: `McBootPage` →
`McLoadSettings` → `McCheckForSweetTooth`), `updateSettingsLoad`/`updateSettingsSave` (options menu), `updateUnlockSave` (after
buying on the unlock screen: `McAskToSave` and saves the options, where the unlocks live), `updateEndOfStorySave`, `updateStoryModeSave`/
`updateStoryModeLoad` (adventure) and `updateEndureSave`. Every save goes through `McChooseSlot` (card 1 or 2), `McCheckSpace`,
`McCheckFormat` and `McCheckOverwrite` and saves the options (`McSaveSettings`); the options, unlock, endurance and end-of-story ones also
create a blank `savegame.dat` if missing (`McSaveAdventureBlank`), and the adventure one picks the slot (`McSelectSaveSlot`) and saves the
slot (`McSaveAdventure`). In other words: tokens and unlocks go to the card on any save.

**Done on 2026-10-19** (`native/runtime/wotm_save.inc`, JSON in `native/wotm_json.hpp`; `WOTM_SAVEJSON=0` turns it off).
- Without touching the fork or decompiling: hooks on entry to `MonsterMc::stdInit` and `MonsterMc::stdExit`, which open and close every
  sequence of card pages (boot, options, unlock, endurance, adventure). In `stdInit` the binaries of both cards (`mc0/`,
  `mc1/`) are rebuilt from `save.json`; in `stdExit`, if the game wrote any binary (date or size changed), the JSON is
  updated from it. The game still sees the same 0x380/0x280 bytes; the stack garbage (0xF4 onward, except the marker) becomes zero.
- **A single JSON**, next to the card folder (`discroot/save.json`): `options`, `battleTokens`, `unlocks` (monsters and costumes, stages) and
  `unknown` (the unnamed fields) once, the same for both cards; `saves` with 20 slots, 0 to 9 on card 1 and 10 to 19 on card 2 (`null` =
  empty slot; the array ends at the last occupied one). Keys by name (`"congar": {"skins": [...]}`, `"levels": {"ufo": true}`, stage and
  monster of the saves by name), never by index: a mod that adds a monster or skin (item 8) only adds keys. Keys the code
  does not know stay in the file when it is rewritten (checked with a `modData` key). Skins are still by position within the monster;
  giving each costume a name is left for item 8.
- Migration: without a JSON, the first `stdInit` creates it from the cards; with a JSON lacking the slots of a card that has adventures, they are
  imported instead of deleted. A JSON that does not open (syntax error) is reported in the log, the cards stay as they are, and before the game
  rewrites it the user's file goes to `save.json.bad`. Writing is through a temporary file and a swap.
- The `[wotm] save: no jogo, N tokens, dificuldade D` (save: in the game, N tokens, difficulty D) at the end of each sequence says what the game has in RAM.
- **Checked:** JSON generated from the old save equal to the one from `tools/mcsave.py`; JSON edited (tokens, difficulty, costume) and binaries
  deleted → the game starts with the JSON's values and the rebuilt `savegame.dat` comes out byte for byte equal to the original; through the menu (the owner):
  "accept and save settings" and buying on the unlock screen update the JSON immediately, keeping the mod key; on reopening, the options
  come back; "load game" on card 2 opens the reel that came from it; saving the options on card 2 also updates the JSON.
- Caution: editing `save.json` with the game open can clobber a save (an editor with the file open since before rewrites the
  old version; it happened in the test). The right way is to edit with the game closed.
- **Step c, done on 2026-10-20** (`WOTM_SAVEAUTO=0` brings the screens back): options, unlocks and tokens save themselves, without a card
  screen. The callers (`screen init`, `saveSettings`, `saveUnlocks`, `loadSettings`, `wonStoryOverlay`, `lostEnduranceOverlay`) only
  repeat `MonsterMc::update*` until `MonsterMc+4`; the six `update*` that are not the adventure's (`BootLoad`, `SettingsLoad`,
  `SettingsSave`, `UnlockSave`, `EndOfStorySave`, `EndureSave`) were replaced by a direct copy (the same as
  `getSettingsData`/`setSettingsData`) between RAM and `save.json`, which sets `+0`, `+4` and `McPageMgr+0x130` (sequence finished: the
  options screen only returns with it) in the same call. Two retail details the replacement keeps: the purchase on the unlock screen is only
  carried out inside `McSaveSettings::update` (`g_monsterToUnlock`/`g_skinToUnlock`: sets the `state` and deducts the price; the screen puts -1
  in both right afterwards), and the boot normalizes mono and applies volume and vibration after the sequence ends (that stays in the
  caller). The tokens the game adds outside these sequences (`Shell::EvaluateOnePlayerStoryStatus`, end of a story stage: the only two
  writes to `Shell+0x2B24` outside the card, endurance and the constructor) are caught by a watch in `sceGsSyncV`: tokens or
  unlocks different from the last save go to the JSON (only those: the file's options stay, the options menu may have values
  not yet accepted). The adventure (`updateStoryModeSave`/`Load`) keeps the screens and the binaries, as before (item 5).
- Next: the campaign as the only slot choice (20 reels), which is item 5, together with reviewing the memory card texts that
  remain in it.
- Open: what `Shell+0x2A40` and `TheGame+0x1204B0..B8` are; what exactly `McScrambleIcon` changes in `icon.sys`.

### 4. `mods/` folder
- The loader looks for each file first in `mods/` (same relative path as on the disc) and, if not found, reads the one from the ISO. Replacing a skin,
  texture, sound or stage becomes a mod without code. Hook on the runtime's file reads (CD/`file*`), in the host layer.
- It is the first step of item 8: here it only replaces; adding without replacing comes there.

### 5. No memory card choice
- **Save list (in testing, 2026-10-20, branch `save-auto`; `wotm_save.inc`):** the boxes and the reels are gone (switching boxes
  did not turn out well; the owner's decision). Loading still goes as far as the boxes on the menu map (`McPrepBoxScreen`), and the update of
  `McLoadGameBoxes` becomes a modal with the frame of `McBackground` and the text of `McTextBox` (`McPage` fonts reduced, without
  crossfade when changing lines): on the right the list from `save.json` (up to 99 slots, 10 visible, up/down and left/right by
  10), on the left the chosen save ("<monstro> at <fase>" (<monster> at <stage>), lives, progress = stages completed / 11, no decimal; the save's stage
  is the next one to play; "N of 11 levels" on the bottom line). X loads through `MonsterMc::setSaveData`; triangle/circle is the page's "back". Saving: the update
  of `McChooseSlot` (after the "save?") becomes the same modal; an empty slot saves straight to the JSON, an occupied one asks for a second X. Photo:
  `docs/screenshots/2026-10-20/2026-10-20_lista_de_saves_carregar.png`.
- On a stage (saving after the fight) the frame comes out lower than in the menu (the size comes from the `McBackground` animation; the
  `setDestDims` does not go past 1): the text heights are compressed to fit. The frame goes straight to its final size, because there
  `timerGetFieldsLastFrame` does not advance and it would not grow (`McPageMgr::update` only runs the page once it is ready); the `+0x274` of
  `McChooseSlot` (the `stateIsOk`) is set as in the original. The first 100 ms do not read the pad (the X from the "save?" arrived here and
  saved). Up/down wrap around at the ends. Photo: `docs/screenshots/2026-10-20/2026-10-20_lista_de_saves_gravar_na_fase.png`.
- Black screen at the "save?" after the final Tsunopolis scene (the flying saucer takes the monster and 18 items are left in the stage): the native side
  rejected snapshots with fewer than 20 items. Now it also accepts them when there is a dialog in the high-priority list (`wotm_live.inc`).
- The menu font has no `%` or `:` (the progress comes out "4 of 11 levels"; no `/` either).
- **Logo and stage photo (2026-10-20, `native/wotm_savepics.hpp`):** on the left, from top to bottom and centered: the monster's logo,
  "on <fase>" (on <stage>), the wide stage photo, "Score N" and the rest. The images come from the disc's `SHELL/SHELL.TEX` + `SHELL.RTX` (folder or
  ISO; after the fight the menu is not in memory): 256x128 logos at texIds 121..129 and 52 (Raptros); 512x256 photos in two
  T8 pieces of 256x256 (the square photo in the previous piece is not used). The palettes are 16x16 entries of the `.RTX` allocated in the order
  of the textures (4 per T8, 1 per T4; the `.NGP` does not link these, the menu swaps them by switch). The rows are bottom to top. The
  modal posts the list every frame and the window draws between the frame (high-priority list) and the text, with a 0.2 s entry.
  Without a logo (Spider, Ogre...) the name comes out as text. Photo: `docs/screenshots/2026-10-20/2026-10-20_lista_de_saves_logo_e_fase.png`.
- Missing: the 3D monster over the photo. Review the memory card texts that remain in the adventure.
- The cards' `savegame.dat` only holds the first 20 slots; the others exist only in the JSON.

### 6. Autosave icon
- **Done on 2026-10-20** (`native/wotm_saveicon.hpp`; `WOTM_SAVEICON_TEST=1` shows it every 3 s): the icon is the disc's `MEMCRD/ICON.ICO`
  (a film reel; 1158 vertices, one shape, compressed 128x128 texture; the format is in the file's header). On every
  save of `save.json`, `wotm_save.inc` increments `g_wotmSaveCount`, and `wotmDrawScene` draws the spinning reel in the bottom right corner
  for 1 s (with a 0.15 s fade). It reads from the disc folder, from the ISO (`native/wotm_disc.hpp`, the reader that belonged to the FMV) or, without
  either, from the copy the game wrote to the card. The lights of `icon.sys` are not used (only the vertex color and the texture).

### 7. Modern (orbital) camera
- Today, with the default camera, left/right rotate the camera. When the monsters are close (unified view, `gUseUnifiedView`,
  `LeaveUnifiedView`, camera POV at `+0xCF8` of each 0xEB0 block in `Cameras::m_cameras`, `game.h`) movement is already relative to the
  camera: the starting point is to bring that movement to the default camera.
- It is not just that: orbiting with the right analog stick/mouse, what happens with the locked target, camera collision with buildings (the game already has
  "ghost" buildings by line of sight), transitions with cinematics and grabs, and each view's camera in split screen.
- `Cameras::Update` (table `_6Camera$UPDATE_FUNK`) is still only in asm; the camera is already a decomp surface of the port. The debug free
  camera (F5, `wotm_freecam.h`/`.inc`) has the axis convention ready.

### 8. Mods that add skins and monsters
- Costumes in `m_costume` (the `[2 + j]` overruns `m_mode` from AI 10 on), selection table in `monster_select.h` (groups of 4 per
  monster), `texInfo` with 1000 entries, internal names in `MonsterNames.h` (17 entries). A new monster goes through `MonsterParse` and
  `Shell::Load*`, which are already the port's decomp surface.

### 9. Training mode
- Hitboxes and collision volumes drawn natively, damage of each hit on screen. Helps balance the playable bosses (10) and the
  mod monsters (8): whoever changes `MonsterParse` sees the effect right away. The debug modes that already exist ([`NATIVE.md`](NATIVE.md): god mode,
  win the fight, `WOTM_AIVSAI`) are the base for an opponent that stands still or is controlled.

### 10. Playable bosses
- The bosses are already in the type table (`assboss`, `jelly`, `final` in `MonsterNames.h`, indices 13 to 15); the selection today goes from 1 to 9
  (up to 10 with the bonus, `findAvailableMonster`).
- Some bosses are not a regular `Monster` ([`NATIVE.md`](NATIVE.md), "Vencer a luta" (win the fight); their own camera in `InitPlantBossCamera`): they need control
  states, moves and scale that fit in a normal fight. It uses the path of item 8 to get into the selection.

### 11. Multiplayer up to 4
- Already exists: split screen for 2 (views `viewDef[1]`/`[2]`, `ViewCtx`/`ViewScope`), controls per port (`wotm_pad.inc`,
  `g_ps2PadDeviceMap`) and side selection (`wotm_padsel.inc`); all in [`NATIVE.md`](NATIVE.md). The engine loads up to 16 monsters.
- Missing: ports 3 and 4 in the controls map; 4 views (quadrants) and the HUD for 4; side selection with 4 columns; the menu text
  ("2 players" → "multiplayer").
- Caution: in `LoadMonstersDB` players 3/4 build the path with player 2's monster (a retail quirk,
  [`16_monsters.md`](../investigations/2026-10-18/16_monsters.md)); the new hook must not inherit that.

**3 and 4 player split screen done on 2026-10-20** (`native/runtime/wotm_split4.inc`; enabled by `WOTM_PLAYERS=3..8` with
`WOTM_MATCH`, [`NATIVE.md`](NATIVE.md)). Checked in `ps2EntryRunner` with the AI on everyone (`WOTM_AIVSAI=1`): 4 players in Centro
(`WOTM_MATCH=3,1,2,5,1`) for 145 s, with 3 rounds, deaths and respawns, the 4 cameras on their monsters (no frame at the origin out of
3634) and the 4 HUDs with portrait, health, "WINS" and messages in the right quadrant; 3 players and 1 AI (`3,1,2,7,1`) for 95 s; 2
players unchanged. The test found and fixed, besides what follows: `Hud::initAfter`/`update` returning with index ≥ 2 (no HUD in the
bottom views), the mode 3 round end looping (with 3 or 4 alive nobody respawned) and cameras 2 and 3 going to the origin in POV 3. In the
4-player fight the watchdog reports stretches of ~24 game frames/s (the cost of 4 views, above). What the engine already had: `viewInfo`,
`worldCtx`, `viewDb`, the aim points and the particles' `viewPrevMat` with 5 views, 4 cameras, 4 HUDs, `m_monsters[0..3]` for players,
and `inputInit` opens 2 ports × 4 slots (controller = port + 2 × slot). The cinemas' `viewToggleSplitScreen` does nothing with 3 or more
views. The hooks:
- **Views:** at the exit of `gameReestablishViews` (modes 3, 6, 7) views 0 to 3 become 320 × 112 quadrants (column `v & 1`, row
  `v >> 1`; with 3 the bottom right one stays black). The 4 `ViewDef`s live at the end of the upper RAM (the retail has only 8, none in a
  quadrant), with half the full screen's screen distance (same FOV as 640 × 224); `viewSetFov` rebuilds planes and matrices.
  `viewInfo[v].viewport` stays 1/2 (the cameras use the split screen POV table). `m_numCameras` = players. `viewGrappleConfig` sent
  view 0 back to the half's `viewDef`: the hook reapplies the quadrant.
- **No unified view:** camera 2 (the retail's unified one) is player 3's. `SetOkToUnify` clears `m_camUnify` every frame (`InitCCCamera*`
  returns without unifying). The victory pose still unifies (full screen on camera 2); when the screen splits again, cameras 2 and 3 go
  back to players 3 and 4. `ResetLevel` no longer changes camera 2's POV.
- **Camera 3 with 4 players:** `SetAIMonster` put every AI on camera 3 (`gameInitCamera(3, ai)`); here the AI only gets `m_cameraView` 3.
  Left over: states that call `SetCameraPOV(m_cameraView)` without checking for a human (end of a throw, `StateThrowBack`) touch the POV
  of player 4's camera when it is an AI.
- **Loading:** from `LoadLevelFiles` to the entry of `InitPlayers`, players 3 and 4 go through the AI path (`m_monsterSel[4..]`,
  `m_costume[2..]`, same disc file) and then become players again; `SetPlayerMonster` gets their costume instead of `m_costume[1]`.
  Players 3 and 4 take their controller configuration from `m_vibration[0..1]` (`Monster::playerInit` reads `m_ctlrCfg[m_index]`, which
  only has 2).
- **HUD:** `Hud::initAfter` and `Hud::update` return right away with index ≥ 2. HUD 2/3 becomes a copy of the HUD in the same column
  (`i & 1`) and runs the update like it, with `m_viewSlot`, the camera's `_cs` and the bar swapped for its own view's during the call; the
  two share the `_cs` and the bar animations, which each view redoes right before its `hier`. With `m_numMonsters` ≠ 2 the bar stays at
  the full screen position, which is the quadrant's shape. The HUD text is laid out as in a 224-line view from the top of the quadrant:
  natively, what falls in the bottom half (the "WINS") moves up 112 (only when more than one view sends text; the pause dialog stays as
  it is).
- **Round end (mode 3):** `EvaluateMultiPlayerBattleStatusNoAI` only handles 1 or 0 alive, and `...AI` respawns slots 0/1 as players
  and the rest as AIs. With the quadrants both become: win target as in `...AI`, otherwise each dead one in `m_monsters` respawns through
  `playerInit` (players) or `aiInit` (AIs).
- **POV 3 of cameras 2 and 3:** POV 3 is the camera parked at `+0xD20` (`DetachCamera`, `TogglePOV`); cameras 2/3 never had that
  position and went to the origin. When entering POV 3 it parks where it is; camera 2 leaves its initial POV 3 (the unified view's rest)
  for 0.
- **Cinema (monster introduction before the fight):** with 2 players `doCinema` joins the screen (`viewToggleSplitScreen`); with 3 or more
  views the toggle does nothing and only view 0 followed the cinema, in its quadrant. At the start of each frame, cinema on puts the
  screen in a single view (`viewNumViews` 1, view 0 with `viewDef[0]`, 640 × 224) and cinema off goes back to the quadrants, like the
  retail 2-player mode (checked: introduction in full screen, then the 4 quadrants). On the way back every view is rebuilt: `endCinema`
  calls the toggle, which with 1 view recreates views 0 and 1 as halves (view 1 stayed 320 × 224, with HUD and text outside the quadrant).
- **Spawning:** in `gameGetStartPoint` (modes 2 to 6) a human's first spawn uses list 1 of `StartPoints` and an AI's list 2, in turn;
  respawns use list 3. List 1 only has the retail's 2 player points (Centro: 2; lists 2 and 3 have 4, one in each corner), and players
  2, 3 and 4 spawned on the same point. A player beyond list 1's points spawns as an AI (`m_playerNum` 2 during the call), on list 2
  (checked in Centro: 3 and 4 in two corners of the map). Live check `largada_4`.
- **Motion blur:** no hook, `MotionBlurAA::init` gets the 3 or 4 views (check `borrao_4`). They fit: `drawOverlay(field, odd, view)` addresses `0x7319F0 + field * 0x400 + odd * 0x800 + view * 0x100` and
  the TU's bss is 0x1000 bytes (the next one, `common/animation`, starts at 0x7329F0), that is `[2][2][4 views][0x100]`; with 4 views
  init writes up to base + 0xBD4 (checked on 2026-10-21).
- **Controls:** the runtime's pad stub has the multitap slots (`tools/runtime_patch/wotm-runtime.patch`: controller = port + 2 × slot,
  8 controllers); slot 1's `scePadRead` reads the devices of player port + 3 (`wotm_pad.inc`): the third and fourth raylib controller.
  No hook on `scePad*`.
- **Native:** `Snapshot::views[3]` instead of `view1`; views 2 and 3 captured at the exit of `hier(v, 0)` (fallback: entry of
  `hier(v, 1)`); quadrant textures composited with the black cross bands; vertical FOV from the view's height (only for the `ViewDef`s
  above 32 MB); text, bars over enemies, flares (`lensFlarePacket[v & 1]`, one copy per view) and the particles' previous matrix for 4
  views. `WOTM_LOCALPLAYER` (online) draws only the local player's view, enlarged from its quadrant, with its text, icons, bars and fades
  moved to the whole screen.
- **To check:** real controllers on players 3 and 4, cost (4 views: the redraw estimated above goes past 16.7 ms; the watchdog saw ~24 game
  frames/s in stretches), per-player sounds (`m_index` up to 3), the `TokenManager m_tokens[2]` (only 2) and the victory pose with a win
  target (not tested: `WOTM_MATCH` has no target).
- Missing: side selection with 4 columns and the menu ("2 players" → "multiplayer").

### 12. 3D preview in the selection
- Today the selection shows each player's monster; with 4 that is 4 models loaded in the Shell at the same time.

### 13. Campaign with up to 4
- The campaign is for 1 player: the game mode (`m_gameMode`), the cinematics and the stages with bosses (extra slots 0x1C0/0x1E0 in the
  Capitol, 0x1A0) need to be revisited.

## Gaps (cheap, at any time)
- **Remap controls in the launcher:** pending item from [`RUNTIME.md`](RUNTIME.md) ("Pendências" (pending items): gamepad and remapping). The controls already go through the
  per-port map of `wotm_pad.inc`.
- **Photo mode:** the F5 free camera (`wotm_freecam.h`/`.inc`) already exists; what is missing is pausing the game and hiding the HUD.
- **PC graphics:** resolution scale, MSAA and anisotropic filtering in native drawing. Frame interpolation is already in the queue
  ([`NEXT_STEPS.md`](../NEXT_STEPS.md)).

## Out of the order
- **Online:** not a priority. With 4-player local multiplayer (item 11), Steam Remote Play Together and Parsec already allow playing
  online without netcode: just test and document. Our own netcode (rollback) is ruled out for now: it would require proving that the logic always gives
  the same result with the same inputs.
- **Web monster viewer with several skins:** idea in [`WEB_VIEWER.md`](WEB_VIEWER.md). Requires extracting assets: extraction runs on the user's machine
  from their ISO; nothing extracted goes into the repo (rule 1 of [`CLAUDE.md`](../CLAUDE.md)).

## Owner's ideas ("trips")
- **Nintendo DS mode:** LOD always at the highest level (the LOD is chosen natively, [`NATIVE.md`](NATIVE.md)); real-time audio filter in the native sound
  engine (`wotm_snd.hpp`); maybe two screens, game on top and HUD below with the portrait (the `hier` HUD is already drawn natively,
  so it can be sent to another area of the window).
- **GTA SA pedestrians and CJ playable at full size:** today's pedestrians are particles (fx 91-99, [`VU1CHECK.md`](VU1CHECK.md)), so replacing them
  with 3D NPCs means new drawing and animation. CJ depends on item 8. Assets from another game do not go into the repo: they would come from the
  user's own files, loaded as a mod.
