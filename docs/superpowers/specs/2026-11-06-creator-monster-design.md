# Character creator, step 2 (custom monster): design

Date: 2026-11-06. Branch: `creator-monster` (off `creator-v1`, PR #46, which is in review). Status: approved by Jake in chat;
written for Lane's review. Issue: #42 (step 2 of 3). Builds on step 1 (`2026-11-05-character-creator-design.md`).

## Goal

From the same CREATE panel, players make a custom monster: a new entry in the CUSTOM cell of the selection screen with its own
name, its own colors, its own HUD portrait and a choice of whether CPUs may use it. It plays exactly like the game monster it is
built on (its base). The result is an ordinary custom monster folder plus a skin that the custom owns
(`docs/passos/MODS.md`, "Custom monsters" and "Skins"), so it loads through what exists today and friends get it by copying the
folders.

Out of scope (Jake chose "identity only"): stats, moves, mixing two monsters, its own narrator line and a borrowed camera. The
narrator and camera stay the base's.

## Section 1: the panel

The CREATE panel gains a first row, **TYPE**, with the values SKIN and MONSTER (left/right). The rows below follow it:

| TYPE = SKIN (step 1, unchanged) | TYPE = MONSTER |
|---|---|
| MONSTER | BASE (the ten playable monsters) |
| COSTUME | COSTUME (the base's disc costume the colors start from) |
| TEXTURE, HUE, SATURATION, BRIGHTNESS | the same four |
| NAME | NAME (up to 11 letters: the HUD's limit) |
| | CPU CAN USE (YES/NO, default NO) |
| SAVE, BACK | SAVE, BACK |

The preview, the texture groups and the tints work as in step 1. Changing TYPE keeps the monster, costume and tints, so a skin
can be turned into a monster. The default name in MONSTER mode is the base's name plus a number, as in step 1. Jake chose one
list with a TYPE row over tabs, so Q/E stay hidden.

## Section 2: what SAVE writes

Approach (chosen over a plain custom whose PNGs would also change the base costume): **a custom plus a skin that the custom
owns.** For a monster named "KAIJU KING" (id `kaiju_king`) on Togera costume 1:

```
mods/Creator/monsters/kaiju_king/monster.json
mods/Creator/skins/kaiju_king/skin.json      "monster": "kaiju_king" (the custom's id)
mods/Creator/skins/kaiju_king/t####.png      the tinted texture groups, as in step 1
mods/Creator/skins/kaiju_king/t####.png      the portrait (Section 3)
```

`monster.json`:

```json
{ "id": "kaiju_king", "name": "KAIJU KING", "base": "togera", "costume": 1, "ai": false, "skin": "kaiju_king" }
```

- **New key `"skin"`** (format change, for Lane's review): the id of one of the custom's own skins. `customCostumes` lists it
  first, so picking the custom gives the tinted look. The disc costume follows it and any other skins come after. Without the
  key the order is today's.
- A skin's PNGs only apply to its own costume slot (its stem, e.g. `LIZARD5`), never to the base costume, so the base monster's
  look and portrait never change.
- The skin is written first with step 1's temp-then-rename save (`writeGroupPngs`, `commitFiles`, `removeStale`). The
  `monster.json` is written last through its own temp file, so a failed save leaves no custom pointing at a missing skin.
- **Names.** The id comes from the name (`idFromName`). SAVE shows `NAME IN USE` when the id belongs to a game monster, to
  another mod's custom, or to a step 1 skin folder of the same name. An existing creator monster of that id asks `OVERWRITE?`.
  Other messages are step 1's: CANNOT WRITE, MODS ARE OFF, NO MODS FOLDER, `SAVED. RESTART TO USE`.

## Section 3: the portrait

At SAVE the window thread renders the tinted preview model's head and shoulders, with no background and a fixed camera framed
from the model's bounds. The render is written as the PNG of the base costume's HUD portrait texture, inside the custom's skin
folder. That texture's id (texId) per playable monster still has to be found. Today only Goliath's is known (`ASSBOSS1`, texId
6). The plan starts with that research: find the texId in each of the ten `<PREFIX><costume>.RTX` files and keep a table in
`native/wotm_creator.hpp`. If a monster has no portrait texture, the save skips the portrait and logs it.

The render happens on the window thread, which owns the GPU. The EE thread's save waits for it through a request flag plus the
finished pixels, with a timeout that skips the portrait instead of hanging.

## Section 4: live registration

A new `Registry::addCustom(folder, why)`, like step 1's `addSkin`, reads the `monster.json` and adds the custom under `m_lock`; the
creator's save then registers the custom's skin with `addSkin` (the skin's `"monster"` has to name a registered custom). The
selection screen's CUSTOM list then shows it without a restart. Readers of `customs()` that run on other threads get the same
locking `skins()` got in step 1. If live registration is refused, SAVE shows `SAVED. RESTART TO USE`. A re-save that changes the
base is one such refusal: the custom's skins keep costume indices chosen for the old monster type.

## Section 5: scripted run

A new variable `WOTM_CREATOR_MONSTER=<base>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]` saves and registers a monster at boot,
like `WOTM_CREATOR`, with CPU CAN USE off. It is added to the variable table (`tools/wotm_env.py`).

## Section 6: testing

- Native tests: the `monster.json` writer and its round trip through the registry's reader; `"skin"` ordering in
  `customCostumes`; `addCustom` after the scan; the id clash rules; the portrait texId table.
- `tools\live_checks.ps1`: `creator_monster_script` (a scripted save logs the custom and its skin) and
  `creator_monster_reload` (the next boot lists both).
- `tools\net_checks.ps1 -Only mods_diferentes` still refuses a PC whose mods differ.
- By hand: TYPE switching, the custom in the CUSTOM list without a restart with the tinted look first, the portrait in the
  HUD, CPU CAN USE, and the base monster's own costume and portrait unchanged.

## Files

- `native/wotm_creator.hpp`: `monsterJson`, the portrait texId table, the id clash rules.
- `native/wotm_mods.hpp`: the `"skin"` key, `addCustom`, locking for `customs()`.
- `native/runtime/wotm_creator.inc`: the TYPE row, MONSTER rows, the monster save, the portrait request.
- `native/runtime/wotm_mainmenu.inc`: the extra rows in the panel layout.
- `native/tests/wotm_native_test.cpp`, `tools/live_checks.ps1`, `docs/passos/MODS.md`, `docs/passos/NATIVO.md`.

## Findings (Task 1)

### A. Portrait texId per playable monster

Every `MON/<PREFIX><costume>.RTX` of the ten playable monsters exists on the disc for costumes 0 to 3 (40 files). In each one the
portrait is the only entry that is not a palette, and it is the last entry of the file (the size-0 packet that `Scene::loadRtx`
already calls "the HUD portrait"). It is always T8H, 128x64. The texId is not the same across monsters or even across costumes of
one monster, so the creator needs the table (or the rule "last entry of the .RTX", read from the file):

| Monster (prefix) | costume 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Congar (MONKEY) | 2 | 2 | 2 | 3 |
| Robo-47 (ROBOT) | 12 | 12 | 12 | 12 |
| Togera (LIZARD) | 3 | 3 | 3 | 4 |
| Ultra-V (SHOGUN) | 11 | 10 | 11 | 11 |
| Preytor (MANTIS) | 9 | 8 | 8 | 8 |
| Kineticlops (ENERGY) | 4 | 4 | 4 | 3 |
| Magmo (LAVA) | 3 | 3 | 3 | 3 |
| Raptros (DRAGON) | 4 | 4 | 5 | 3 |
| Agamo (ROCK) | 3 | 3 | 3 | 2 |
| Zorgulon (ALIEN) | 6 | 6 | 6 | 6 |

For reference: ASSBOSS1 is texId 6 (128x64), ASSBOSS0 is texId 5 (64x32), JELLY0 and FINAL0 have no portrait entry.

The 128x64 image holds two 64x64 faces side by side: the left half is the normal face and the right half is the fury face (the
same face, reddened). The game swaps them when the monster's fury is full (`m_boostFull`, Monster+0xF4; `emitBars` in
`wotm_overrides.cpp` picks u 0..64 or 64..128 the same way). The creator's portrait has to fill both halves, for example the same
render twice with the right half tinted red, or the fury face goes back to the disc's. `tools/modkit.py portrait_png` already
exports this image the way the HUD expects it (the "last 128x64 entry" rule).

### B. A skin's PNG reaches the HUD portrait: yes

The HUD portrait (the game's own HUD in the corner and the `WOTM_ENEMYBARS` block over an enemy) is drawn from the resident T8H
image through `Scene::residentTexture`, which asks `Scene::modTexture` first. The PNG lookup uses the file the fighter's costume
loaded (`LIZARD4` for the skin), so a skin's `rtx_t<texId>.png` applies only to the skin's costume.

Live run: a temporary mods folder with one skin, `T1/skins/pink/skin.json` = Togera, base 0, and a single solid magenta 128x64
`rtx_t0003.png`. Boot log: `[mods] skin togera/pink ("PINK", T1) = costume 4 (LIZARD4); base 0, 1 png`. In the fight:
`[mods] LIZARD4.RTX t3 <- T1/togera/pink/rtx_t0003.png (128x64; game 128x64)`.

- `WOTM_MATCH=3:pink,3:0,1` with `WOTM_ENEMYBARS=1`: the player's HUD portrait is solid magenta (the HUD draws it at half
  brightness, (126, 0, 126) on screen, as it does the disc's art), and the enemy bar of the Togera in costume 0 shows the normal
  Togera face.
- `WOTM_MATCH=3:0,3:pink,1`: the reverse. The player's HUD (costume 0) shows the normal face and the enemy bar of the skin is
  magenta.

So the portrait PNG belongs in the custom's skin folder, named `cr::pngName(texId, true)` with the texId of the skin's base
costume from the table above. Nothing else is needed for the HUD to show it. (Note added later: that took commit 5d11479, in which
`Scene::loadRtx` looks a costume's resident images up by the name the game loaded, `LIZARD4`, not by the file it read, the base
costume's.)

### C. The HUD name of a custom on a playable base

Where the name is read: the game prints names from `MonsterNewNames[17][12]`, one static copy per translation unit, five copies
in RAM (`MonsterNewNames_006EAA78` in Hud: "%s CRUSHED" and "%s WINS" in fights; `_006E8D90` in CrushLevel; `_006E8958` next to
BigShotLevel; `_006ED318` next to the memory card screens; `_006EFEC8` next to the Shell). The port's own readers are
`wotm_modes4.inc` (`winsText` reads `_006E8D90`, `bsSetSlot` copies `_006E8958`). The table is per type, so a name per fighter is
not possible without hooking each print; the native select screen draws its own names and does not read these tables.

The rule for later tasks (as proposed, with one change on where it runs):

- At the entry of `Shell::LoadLevelFiles` (the hook `wotm_sync::levelLoad` and `wotm_split4` already use), first put the game's
  names back in all five copies (kept from boot), then, for each type, count the fighters of that type in this fight (the Shell's
  `m_monsterSel`/`m_costume` for players and CPUs, including players 3 and 4 that take the CPU path in `wotm_split4.inc`). When
  exactly one fighter has that type and its costume is the skin index of a creator custom's `"skin"`, write that custom's name
  (the 11-letter cut and lowercase of `setShownName`) into the type's entry of the five copies. Two or more fighters of the same
  type keep the game's name.
- Put the game's names back at the entry of `userintMain` (back to the menus), like `wotm_modifiers.inc` clears its fight latch,
  so the memory card and Shell screens never show a custom's name for a plain monster.

Online: the names are only used for drawing, but they live in the game's RAM and the network match hashes all 32 MB of it
(`wotm_sync::netHash`). Writing them is safe only because every PC writes the same bytes at the same frame: the mods digest makes
the customs and their skin indexes the same on every PC, `WOTM_MATCH` is the same, and the entry of `Shell::LoadLevelFiles` runs on the EE
thread, where the frame index restarts, before the first frame of the fight. The write must never happen on the window thread or at
a host-clock moment.

Callers of `customByType(type)` without a costume, for a creator custom (playable base, no `"camera"`) in a plain Togera fight:

- `setShownName` (`wotm_custom.inc`, `loadBankCommon`): runs only for types 13, 14 and 15. A creator custom is never picked up.
- `setPov` (`wotm_custom.inc`): it does find the creator custom for type 3, but its `cameraFrom` is empty and the type is not 13,
  so no table is written: a plain Togera keeps its camera. Side effect to fix in the creator task: the first custom of a type
  hides the others. A creator custom saved before an older Togera-based custom with `"camera"` makes that custom lose its
  borrowed camera, and the reverse makes a plain Togera borrow it. `setPov` should look up by the followed monster's costume
  (`customByType(type, costume)`, or the custom whose `"skin"` index matches).
- `customOfSel` (`wotm_padsel.inc`): returns nullptr for every playable type, so a creator custom is never picked up. The
  consequence is the other way round: places that use it (`monsterName` in the `[padsel]` log lines, the CUSTOM cell highlight
  after confirming when `g_soloCustomPick` is not set) show the base monster for a creator custom. The CUSTOM list itself reads
  `customs()` and is not affected.
