# TODO

What is left to do, and only that. Bug reports go first. A finished item leaves this file: the story goes into the commit message and what now exists goes into the reference doc (in the present tense). Doc rules: `CLAUDE.md`, "Documentation".

**Direction: browser first.** The browser version is the release. Mac-native features, Mac friends zips and browser-versus-Mac cross-play runs are not release gates; the native build and its tests stay in CI because the browser game is built from the same code (`docs/passos/WEB.md`).

## Order
| # | Item | Depends on | Cost |
|---|---|---|---|
| 8 | Mods that add monsters (skins already exist) | — | medium/high |
| 10 | Playable bosses | 8 | high |
| 13 | Campaign (adventure) with up to 4 players | — | the highest |

10 is a special case of "add a monster" (8). The 4-player campaign comes last: cinematics, bosses and rules were made for 1 player.

### 8. Mods that add monsters
How it works today: `docs/passos/MODS.md` ("Skins", "Custom monsters").
- **Monsters with their own files:** a `files` field in `monster.json`, with a free type (6, 12 or 16) or remapping a type. Today a custom monster uses the type of one of the game's monsters. A new model depends on the `.NGP` coming back in through the kit (today it only goes out).
- **Skins in elimination (mode 6):** `Shell::LoadMonstersDB` loads costume 0 of the 10 monsters once, and the players switch monsters between rounds without reloading. Choosing a costume there requires changing the load (one file per chosen monster), not just the screen.
- **Mod file larger than the disc's** outside `MON/*`: the game loads into fixed-size regions and can overwrite what comes after (the log warns). Make room per file type.
- **Ceiling warning for a skin with its own `.TEX`/`.RTX`/`.PTR`:** a skin has no original to compare against, so today nothing warns. The load log should compare against the real ceilings: `texInfo[1000]` (final `maxTexId`), the `.RTX`'s resident VRAM and the `.PTR`'s 0x30D40 buffer (`0f091a4:docs/investigacoes/2026-10-23/skins_mod_carga.md`, item 8).


### 10. Playable bosses
- Where they stand: `MODS.md`, "Custom monsters" (Goliath, Jelly and Cerebulon; mod `mods/Cerebulon/`). The Plantboss is not viable.
- **Mecha** (Cerebulon's first phase): it is a piece of the final stage (`FinalBoss::updateMecha`, hier 0x27A6..0x27A9), not a `Monster`, like the Plantboss. Playable only as a new monster assembled from the pieces (`tools/monglb.py --boss mecha` already puts them together for the viewer). The owner's decision.
- Native selection: check the framing of the Jelly and the Cerebulon in the CUSTOM slot (`wotm_savepics.hpp`, `kFraming`).

### 13. Campaign with up to 4 players
- The campaign is for 1 player: the game mode (`m_gameMode`), the cinematics and the stages with bosses (extra slots 0x1C0/0x1E0 in the Capitol, 0x1A0) need to be revisited.

## Gaps (cheap, at any time)
- **Browser build without the VU0 microcode:** `tools/build_web.sh` with no `disc/SCUS_971.97` builds without `wotm_vu0_native.gen.inc`, and the browser has no VU0 interpreter, so the game runs but draws black (only a `[vu0]` log line says why). The build should fail instead; adding the `.inc` later also needs `wotm_vu0_native.cpp` rebuilt (ninja does not track it).
- **Map editor:** the session's lock is held during a save and during the per-frame `toMap` (`wotm_mapedit.inc`, `tick`), so the window can stall for a moment; the "SAVED" message stays after later unsaved edits; map ids such as `con` or `nul` pass the id check but are reserved device names on Windows; F5 does nothing while the editor is open (it keeps the free camera on); `parseScript`, `reset` on a copy and the id check have no tests.
- **Touch controls and pads on hardware:** only emulated (`tools/browser_touch_test`). Try a real Android phone (frame rate, vibration, fullscreen and the landscape lock, whether the shoulder buttons cover the top-left HUD) and real non-standard USB pads (`fallbackLayout` in `web/pads.js` is a guess). A tap on the right half of the screen is also a click in a fight (a light attack): check it does not fire by accident.
- **Character creator (#42):** the next steps are parked, not dropped. Step 2 (custom monster: HUD name, announcer line, camera, `ai`) adds little over created skins, which already travel online (lobby protocol 5); step 3 (parts and shapes) needs the `.NGP`/`.PTR` writer that map creator step 3 (#10) would build first.
- **Online holdback, short freezes that keep coming:** a member whose game freezes about 2 s every 6 s (a tab throttled or collecting garbage) slows a match to about 20 of 30 frames a second without being alone at the back in 60% of the samples, so `wotm_net_holdback.hpp` never names it. Judge by the share of the window lost to that member's freezes, with a test next to `holdback_names_freezes_that_keep_coming_and_long_ones`.
- **Rollback synctest, sound engine at frame 7181:** `sh tools/synctest.sh` (AI vs AI, `WOTM_MATCH=3,1,1`, 60 Hz, 2 frames back) reports one mismatch at frame 7181, the same on the native Mac and in Firefox: 2 bytes of the sound engine's snapshot (offsets 0x1b1 and 0x35d natively) differ after the redo. Some engine state set in that frame is missing from `saveState`/`loadState` (`native/wotm_snd.hpp`); start from the voice or sound at those offsets. Context: `docs/investigacoes/2026-11-18/rollback-synctest/firefox-cost.md`.

## Ongoing
- **Off the critical path:** an oracle for functions with calls; coverage with gameplay to feed back into `boot_coverage_asm.csv` (`57d4afd:docs/investigacoes/2026-10-19/pendencias_oracle_cobertura.md`, sections 2 and 3).

## Out of order (until the owner chooses)
- **120 Hz interpolation in Chrome:** on an M1 Max at load ~20, Chrome cannot redraw the scene on every 120 Hz frame: frames stall 100 to 300 ms although a redraw is 1.1 ms of GPU time, 3 ms of main-thread work and 380 draws, and it stalls the same without blending (`WOTM_INTERP=redraw`) and at 448 lines. `interp::PaceGuard` falls back to one redraw per game frame. Find what the browser waits on (a Chrome trace of the GPU process) and measure on a quiet machine (`node tools/web_fps.mjs`); Firefox holds ~100 fps with fewer late frames than before.
- **Display rate read once:** the page measures it at load; a covered window (Chrome gives it 30) or one moved to another screen keeps that rate until the next Play. Re-measure on `visibilitychange` or from the presents themselves.
- **Firefox online on two computers:** on one Mac Firefox x Firefox and Firefox x Chrome stay in sync but run at 50 to 56 fps against 59.7 for Chrome x Chrome (machine load and a covered window confound it; issue #91); check pace with each browser on its own computer (`docs/passos/WEB.md`).
- **Browser audio on an AudioWorklet:** raylib's web audio uses the deprecated ScriptProcessorNode; miniaudio supports AudioWorklets (`-sAUDIO_WORKLET`, `-sWASM_WORKERS`). Measure glitches before and after.
- **More movies in the browser:** only the two boot movies play (`tools/web_movies.py`); the endings, act intros, credits and the menu's big-screen loop are still skipped.
- **Cloud saves:** an encrypted passphrase vault for carrying saves between browsers, after the owner decides it is wanted (`docs/investigacoes/2026-11-16/cloud_saves.md`).
- **Saves tab, one adventure slot:** deleting or exporting a single slot needs the card binaries and `save.json` kept consistent (`wotm_save.inc` imports a card's slots when the JSON lacks them); the page only replaces or deletes all saves.
- **Settings page, key rebinding:** keys and gamepad buttons are changed in the game's panel only; the page's Controls tab could rebind the keyboard (`native/wotm_settings.hpp` `kKeyNames` to the DOM's `event.code`, the `KB_<BUTTON>` lines of `settings.ini`).
- **4 players always at 60:** with 4 views, ~80% of frames come out at 60 and the rest at 30; what holds it back is the EE (median ~14 ms, p90 ~17.5 ms). Candidates, from the profile of the EE samples (`PS2X_HOSTPROF=2` + `_RAW=1`, resolved with the `wotm.map` of the same build): the translated VU0 wrapper (`executeVU0Microprogram` copies ~1 KB of state each way, ~3%) and the out-of-line `checkpointDue` (~3%), both PS2Recomp seams; the VU1 packet assembly that `wotm.exe` discards (`pktAddVu1ObjAsm`, ~4%, risky: item G3 of `723b110:docs/investigacoes/2026-10-23/performance_vistas.md`). In the window, the particles still in rlgl's immediate mode (~0.6 ms per view). Compare by alternating within the same run (an AI vs AI fight does not repeat).



## Discarded by the owner (do not propose again)
- An unlock screen for mod skins (a mod skin is always unlocked) and a sound of its own per skin.
- 5 to 8 players (shared camera).
- A panel option to pin the LOD.
- Player frames of the native selection in 2D outside the big screen (they look better on the big screen).
- Entering training mode from the launcher (the menu's TRAINING line and `WOTM_TRAINING=1` are enough).
- Our own netcode (rollback): it would require proving that the logic always gives the same result from the same inputs.
- Making Goliath Prime or other bosses work in full (CPU, collision, animations that ASSBOSS does not have): custom monsters are mods/experimental.
