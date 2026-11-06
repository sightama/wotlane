# Character Creator Step 2 (custom monster) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The CREATE panel gets a TYPE row (SKIN / MONSTER). MONSTER saves a custom monster (`monster.json`) plus a skin the
custom owns (tinted PNGs and a rendered HUD portrait), and registers both so the custom is in the CUSTOM list at once.

**Architecture:** Pure helpers in `native/wotm_creator.hpp` (`monsterJson`, the monster script parser, the portrait texId
table) and registry changes in `native/wotm_mods.hpp` (`"skin"` key, `addCustom`, locked `customs()` reads), all tested without
raylib. `native/runtime/wotm_creator.inc` gains the TYPE row, the MONSTER rows, the monster save (step 1's skin save, then
`monster.json` last) and a window-thread portrait render requested by the EE-thread save.

**Tech Stack:** C++17 (runtime, native tests), raylib (render texture, `LoadImageFromTexture`), PowerShell checks.

**Spec:** `docs/superpowers/specs/2026-11-06-creator-monster-design.md` (read it first). Step 1 code and spec:
`native/runtime/wotm_creator.inc`, `docs/superpowers/specs/2026-11-05-character-creator-design.md`.

**Branch:** `creator-monster`, off `creator-v1` (PR #46, in review). The PR for this work targets `creator-v1` until #46 merges.
**Never merge** any PR: Jake's rule is a formal PR, a human or pipeline review, then an approved merge.

## Global constraints

- English everywhere; every new file starts with two `// ABOUTME:` lines.
- Never commit game files (`ISO/`, `disc/`, `runtime/`, `recomp/`, `*.elf`). Docs commits run `python tools/doclint.py` first.
  A new `WOTM_` variable needs its table row, then `python tools/wotm_env.py` must pass.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build: from PowerShell, `$env:PATH = "C:\Program Files\CMake\bin;$env:PATH"; cmd /c C:\Projects\wotlane-src\build_runtime.bat`
  (about 40 s; must end with `Executable updated.` and no warnings in the creator files). Native tests:
  `cmd /c C:\Projects\wotlane-src\tools\test_native.bat` (133 checks green before this plan).
- Live checks: `$env:PS2X_CD_IMAGE = 'C:\Projects\wotlane\ISO\War of the Monsters (USA).iso'` and
  `$env:WOTM_DISC = 'C:\Projects\wotlane-src\disc'`. Without WOTM_DISC the native layer finds no SHELL/LVL/MON `.RTX` and draws
  no text (log `[native] resources: SHELL (not found)`). Never inject real mouse or keyboard input; scripted pads only
  (`PS2X_PAD`, `WOTM_PADSEL_SCRIPT`).
- Nothing here touches the lockstep state: a custom on a playable base runs that base's code.

---

### Task 1: research (no code): portrait texIds, portrait through a skin, the HUD name

Three facts the later tasks depend on. Write the answers into a new section "## Findings (Task 1)" at the end of the spec and
commit it (`docs: creator step 2 findings`). If finding B or C is "no", stop and report to the controller before Task 2.

- [ ] **A. Portrait texId per playable monster.** For each of the ten playable prefixes (`kMonsters` in `native/wotm_mods.hpp`)
  and each disc costume 0..3, find the texId of the HUD portrait inside `MON/<PREFIX><costume>.RTX`. Start from how Goliath's was
  found (`ASSBOSS1.RTX` texId 6, "the last entry in the .RTX, 128x64", `wotm_mods.hpp` near the `"portrait"` comment) and from
  the native HUD portrait drawing (`native/runtime/wotm_live.inc`, "Rectangle with a resident .RTX texture (enemy portrait)",
  around line 1128). `tools/tex.py` and `tools/modkit.py` read `.RTX` files from `disc/MON/` (the extracted disc; never commit
  what they export). Record the texId, the size, and whether one rule covers all (for example "last entry, 128x64") or a table
  is needed.
- [ ] **B. Does a skin's PNG reach the HUD portrait?** A skin's PNGs are keyed by the skin's own stem (`LIZARD5`) and texId
  (`texmods::Index`, `Scene::modTexture`). Read how the HUD portrait is drawn and confirm it goes through `modTexture` for the
  fighter's costume file. Then prove it: hand-make a skin folder in a temp mods folder (`WOTM_MODS=<temp>`) for Togera with only a
  solid-color PNG named for the portrait texId (name from `cr::pngName`), start a fight with that costume
  (`WOTM_MATCH=togera:<skin index>,...`, parser in `wotm_overrides.cpp` around line 2590) and take a screenshot (`WOTM_SHOTS`,
  see `docs/passos/NATIVO.md`). The HUD portrait must show the color, and Togera's base costume must not.
- [ ] **C. The HUD name for a custom on a playable base.** `setShownName` (`native/runtime/wotm_custom.inc` around line 135)
  writes the type's entry of `MonsterNewNames` and runs only for the boss types today, and `customByType(type)` returns the first
  custom of a type whatever its costume. Find where the HUD and the end-of-round texts read the name, and whether a per-fighter
  name is possible (the name table is per type). Record the rule the plan will use. Proposed: when exactly one fighter of that
  type is in the fight and its costume is a creator custom's skin index, write the custom's name into that type's entry for the
  fight and restore the game's entry at the end. With two fighters of the same type, both keep the game's name. Confirm this is
  safe (the names are drawing only, not lockstep state) or report a better hook. Also confirm `customByType(type)` without a
  costume (`setPov`, `customOfSel`) cannot pick a creator custom up for a plain Togera: creator customs have no `camera`, and
  `customOfSel` only looks at non-playable types, but write down each caller's answer.

### Task 2: pure helpers in `wotm_creator.hpp`

**Files:** Modify `native/wotm_creator.hpp`; Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1: failing tests.** Add three tests in the style of step 1's `creator_skin_json_and_png_names`:
  - `creator_monster_json`: `cr::monsterJson("kaiju_king", "KAIJU KING", "togera", 1, false, "kaiju_king")` parses with
    `wotm::json::parse` into an object with exactly those six keys and values (`"ai"` a bool) and ends with a newline.
  - `creator_monster_script_parse`: `cr::parseMonsterScript("togera:1:kaiju:120", sc, err)` gives Togera's type (3), costume 1,
    id `kaiju`, hue 120. `"togera:1"` (no id), `"togera:7:x"` (costume out of 0..3), `"assboss:1:x"` (not playable: the
    creator only builds on the ten) and `"togera:1:x:nan"` fail with a non-empty `err`.
  - `creator_portrait_tex`: `cr::portraitTex(prefix, costume)` returns Task 1 A's values for at least Togera 0..3 and Ultra-V
    0..3, and -1 for an unknown prefix.
- [ ] **Step 2: run, expect the three to fail to compile.** `cmd /c C:\Projects\wotlane-src\tools\test_native.bat`.
- [ ] **Step 3: implement.** `monsterJson` builds the object the same way `skinJson` (same file, around line 45) does and
  returns `json::dump(v)`, which already ends with a newline. Key order: id, name, base, costume, ai, skin. `parseMonsterScript`
  calls `parseScript` and then rejects a monster that is not one of the ten playable types (the table the panel's BASE row uses).
  `portraitTex` is Task 1 A's rule or table.
- [ ] **Step 4: tests green.** Same command; expect 136 checks, 0 failures.
- [ ] **Step 5: commit** `native: creator helpers for custom monsters (monster.json, script, portrait texId)`.

### Task 3: the `"skin"` key

**Files:** Modify `native/wotm_mods.hpp` (`struct Custom`, `scanCustoms` around line 518, `customCostumes` around line 240),
`native/runtime/wotm_padsel.inc`; Test `native/tests/wotm_native_test.cpp`; Docs `docs/passos/MODS.md` ("Custom monsters").

- [ ] **Step 1: failing test** `mods_custom_skin_first`: a temp mods folder with `Creator/monsters/kaiju/monster.json`
  (`"base": "togera", "costume": 1, "skin": "kaiju"`) and `Creator/skins/kaiju/skin.json` (`"monster": "kaiju"`, one 4x4 PNG,
  built the way `mods_add_skin_after_scan` builds its folder). After the scan, `customCostumes(*custom("kaiju"))` is
  `{<the skin's index>, 1}`. The same folder without `"skin"` gives `{1, <index>}` (today's order). A `"skin"` naming no skin of
  that custom keeps today's order and logs `[mods] monster kaiju: "skin" kaiju is not one of its skins`.
- [ ] **Step 2: run, expect fail.**
- [ ] **Step 3: implement.** Add `std::string skin;` to `Custom` (comment: the id of its own skin, listed first). `scanCustoms`
  reads `"skin"` as a lowercase string. `customCostumes`: when `c.skin` names one of `c`'s skins, push that skin's index first,
  then `c.costume`, then the other skins; otherwise today's order. The unknown-skin log runs once per custom, after the skins are
  placed.
- [ ] **Step 4:** the selection starts a custom on `cu->costume` (`wotm_padsel.inc` around line 788, "the first of
  customCostumes", and the other `customCostumes` users around lines 1150, 1352, 1516). Make each start on `customCostumes(*cu)[0]`
  so "the first" stays true. Build the runtime.
- [ ] **Step 5: tests green.** MODS.md gets a `"skin"` bullet after `"ai"`; doclint; commit
  `native: monster.json "skin" puts the custom's own skin first`.

### Task 4: `Registry::addCustom` and locked reads

**Files:** Modify `native/wotm_mods.hpp`, `native/runtime/wotm_padsel.inc` (every use listed by
`grep -n "customs()" native/runtime/*.inc native/runtime/*.cpp`); Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1: failing test** `mods_add_custom_after_scan`: scan a temp mods folder with no customs; then write
  `Creator/monsters/kaiju/monster.json` and its skin. `addCustom(<monsters/kaiju>, why)` returns non-null, `custom("kaiju")`
  finds it, and `addSkin` of the skin folder then gives a skin with `custom == "kaiju"`. A second `addCustom` of the same folder
  updates in place (same position in `customs()`). An `id` already used by a custom from another mod fails with a `why`.
- [ ] **Step 2: run, expect fail.**
- [ ] **Step 3: implement.** Split the per-folder body of `scanCustoms` into `bool readCustom(const fs::path &folder, const
  std::string &mod, Custom &c, std::string &why) const` and use it in both places. Boot logs must stay byte-identical: diff a
  boot log before and after. `addCustom(folder, why)` works under `m_lock`: it overwrites in place when the same id comes from
  the same folder (`sameDir`), refuses when it comes from another folder, and otherwise appends. Readers on the window thread
  (the selection's custom list, `wotm_padsel.inc` around lines 2566, 2651, 2658) must not walk `m_customs` while the EE thread
  grows it. Add `std::vector<Custom> customsCopy() const` (a locked copy) and use it on the window thread; EE-thread readers
  keep `customs()`. Document the rule next to `customs()` the way step 1 documented `skins()`.
- [ ] **Step 4: green; build the runtime.**
- [ ] **Step 5: commit** `native: Registry::addCustom for customs saved in game`.

### Task 5: the TYPE row and the MONSTER rows

**Files:** Modify `native/runtime/wotm_creator.inc` (`enum Row` line 14, `kHints` around 22, `State` around 30, `adjust`
around 231, `input` around 275), `native/runtime/wotm_mainmenu.inc` (creator rows in `panelLayout`, `renderPanel` and
`panelHit`; search `wotm_creator::`).

- [ ] **Step 1:** Rows become a per-type list instead of one enum order. Keep `enum Row` as the set of row kinds and add
  `RType` and `RCpu`. Add `kSkinRows[] = {RType, RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RSave, RBack}`,
  `kMonsterRows[] = {RType, RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RCpu, RSave, RBack}` and a helper
  `rowsOf(const State &)`. `State::row` becomes the position in that list. `State` gains `bool monsterType = false;` and
  `bool cpu = false;`; the `static_assert` that `State` is trivially copyable must still pass.
- [ ] **Step 2:** Labels: RType shows `SKIN` or `MONSTER`. RMonster's label is `MONSTER` in SKIN mode and `BASE` in MONSTER
  mode. RCpu is `CPU CAN USE` with `YES` or `NO`. Hints use only the game font's letters (no slash, plus, comma or degree sign):
  RType "MAKE A NEW COSTUME OR A NEW MONSTER", RCpu "WHETHER COMPUTER PLAYERS MAY PICK IT".
- [ ] **Step 3:** Left or right on RType flips `monsterType`, keeps the monster, costume and tints, and clamps `row` to the new
  list. `defaultName` runs only when the name is still the old default. Moving off SAVE cancels OVERWRITE? as today.
- [ ] **Step 4:** The panel layout reads the row count from `rowsOf` (11 rows in MONSTER mode). Check that the panel still fits
  at 720 lines on screen 2 without overlapping the preview box or the legend. If it does not, shrink the row step the way the
  VIDEO tab does (`panelLayout`, "VIDEO has a smaller step").
- [ ] **Step 5:** build; `tools\live_checks.ps1 -Only creator_script` still passes (the SKIN path is unchanged); commit
  `native: creator TYPE row (SKIN or MONSTER) and its rows`.

### Task 6: the monster save

**Files:** Modify `native/runtime/wotm_creator.inc` (`SaveCtx` around 364, `checkExisting` around 424, `registerLive` around
588, `saveAs` around 610).

- [ ] **Step 1:** `SaveCtx` gains `bool monster` and `std::filesystem::path monsterDir` (`mods/Creator/monsters/<id>`). In
  MONSTER mode the skin's `skin.json` names the custom (`"monster": "<id>"`), and `monster.json` comes from
  `cr::monsterJson(id, name, kMonsters[s.monster].name, s.costume, s.cpu, id)`. It is written as a temp file and added to
  `files` **last**, so `commitFiles` renames it after every PNG and the `skin.json`.
- [ ] **Step 2:** Name rules in `checkExisting` for MONSTER mode. `NAME IN USE` when the id is a playable monster's name or
  prefix (`wotm::mods::customBase`), when `Registry::custom(id)` comes from another folder than `monsterDir`, or when
  `skins/<id>/skin.json` names a game monster (a step 1 skin). `OVERWRITE?` when `monsterDir` exists. SKIN mode gains one
  rule: `NAME IN USE` when `skins/<id>/skin.json` names a custom (that folder belongs to a monster).
- [ ] **Step 3:** `registerLive` in MONSTER mode calls `addCustom(monsterDir)` first, then `addSkin(dir)` and the texmods
  `replaceSkin` as today. Either refusal gives `SAVED. RESTART TO USE` with the `why` logged.
- [ ] **Step 4:** Log `[creator] saved monster <id> (base <name> costume <n>, <k> png) in <ms> ms`.
- [ ] **Step 5:** build; a scripted save (Task 8 Step 2 can come first if that is easier) writes both folders; commit
  `native: creator saves a custom monster and its own skin`.

### Task 7: the portrait

**Files:** Modify `native/runtime/wotm_creator.inc` (preview block around lines 674 to 800, and the save).

- [ ] **Step 1:** On the window thread, `renderPortrait(const State &, int w, int h, std::vector<uint32_t> &rgba)` draws the
  tinted preview model into a `w x h` render texture (Task 1 A's portrait size) with a transparent background. The camera faces
  the model and frames the top 45% of its bounds (head and shoulders). `LoadImageFromTexture` then gives the RGBA rows, flipped
  vertically because render textures are upside down.
- [ ] **Step 2:** Handshake. The EE-thread save sets an atomic `g_portraitReq` carrying the `tintGen` and the size. The window
  thread sees it in `renderPreview`, renders, stores the pixels in `g_portraitPx` under a mutex and sets `g_portraitDone`. The
  save waits up to 500 ms in 5 ms sleeps. On timeout it skips the portrait, logs `[creator] portrait skipped (no frame in
  500 ms)` and saves the rest. Scripted saves at boot have no window, so they skip it with `[creator] portrait skipped (scripted
  save)`.
- [ ] **Step 3:** The pixels go through `writeBytes` as `cr::pngName(cr::portraitTex(prefix, costume), rtx)` in the skin
  folder, with step 1's temp-then-rename. When `portraitTex` is -1, skip and log.
- [ ] **Step 4:** build; commit `native: creator renders a HUD portrait for a custom monster`.

### Task 8: the HUD name, the scripted save, checks and docs

**Files:** `native/runtime/wotm_custom.inc` (Task 1 C's rule), `native/runtime/wotm_creator.inc` (`scriptInstall` around 651),
`tools/live_checks.ps1`, `docs/passos/NATIVO.md` (creator section and env table), `docs/passos/MODS.md`.

- [ ] **Step 1:** Implement Task 1 C's name rule. Log `[custom] kaiju_king: HUD name "kaiju king" for type 3` once per fight.
- [ ] **Step 2:** `WOTM_CREATOR_MONSTER=<base>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]` in `scriptInstall`, parsed by
  `cr::parseMonsterScript`, saving with `monsterType = true` and `cpu = false`. Add the env table row in NATIVO.md, then
  `python tools/wotm_env.py` must pass.
- [ ] **Step 3:** `tools/live_checks.ps1` gets `creator_monster_script` and, right after it, `creator_monster_reload`, in the
  style of `creator_script` and `creator_reload`. The first uses
  `Env = "WOTM_CREATOR_MONSTER=togera:1:kaiju:120;WOTM_MODS=$env:TEMP\wotm_creator_monster"`, cleans that folder, and expects
  the `[creator] saved monster kaiju` line and the `[mods] skin` line for the custom's skin. The second uses the same `WOTM_MODS`
  without the creator variable and expects `[mods] custom monster kaiju ("KAIJU", Creator)`. Quotes in the regex are written
  `\x22`, because powershell -File drops a bare quote.
- [ ] **Step 4:** Run both, plus `creator_script`, `creator_reload`, `tools\net_checks.ps1 -Only mods_diferentes` and the
  native tests. All pass.
- [ ] **Step 5:** Docs. The NATIVO.md creator section gets "TYPE and custom monsters". MODS.md "Custom monsters" mentions
  creator customs and the `"skin"` key. doclint; commit `native: scripted custom monster save, checks and docs`.

### Task 9: hand-off (no merge)

- [ ] Final whole-branch review (`superpowers:code-reviewer`), fixes applied.
- [ ] Push `creator-monster` and open a PR with base `creator-v1`, so the diff is only step 2. Title
  `Character creator step 2: custom monsters (#42)`. The body follows PR #43's style: what changes, log lines, how it was
  checked, a hand-test list. Labels `enhancement`, `side:both`, `needs-lane`. Comment on #42 with the link.
- [ ] **Do not merge.** Rebuild Jake's game on the branch for his hand test (with `WOTM_DISC` set). Afterwards rebuild the game,
  server and launcher at Lane's game-night commit 9005ccc.
