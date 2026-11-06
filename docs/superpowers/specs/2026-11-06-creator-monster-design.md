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

A new `Registry::addCustom(folder, why)`, like step 1's `addSkin`, reads the `monster.json`, adds the custom under `m_lock`, and
registers its skin through `addSkin`. The selection screen's CUSTOM list then shows it without a restart. Readers of `customs()`
that run on other threads get the same locking `skins()` got in step 1. If live registration is refused, SAVE shows
`SAVED. RESTART TO USE`.

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
