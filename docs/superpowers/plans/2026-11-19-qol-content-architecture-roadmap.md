# QoL, content creation and architecture roadmap

> **For agentic workers:** this is a roadmap, the plan of plans. No item here is ready to code. Each workstream becomes its own
> spec and implementation plan (`superpowers:brainstorming`, then `superpowers:writing-plans`) when someone claims it, and then runs
> with `superpowers:subagent-driven-development` or `superpowers:executing-plans`. Claim the item first (see "Claims and
> collaboration").

**Goal:** make the browser game better to play, let players make and share their own maps and characters, and change the game's
architecture where that makes it faster, safer to evolve and easier to mod.

**Approach:** classify every idea by what it may touch (the determinism classes below), reuse what exists (Jake's in-game creators,
upstream's editor and monster pipeline, the lockstep input log, the state snapshot from the synctest) before writing anything new,
and order the work so each step ships alone.

**Tech stack:** C++ native layer compiled to WebAssembly (`native/`), page JavaScript (`web/`), `wotm_server`, Python tools
(`tools/`), Blender and the Craft Apps for assets.

**Status:** proposal for Lane, written 2026-11-19 against `wip/browser` at `fb1f703c`. Every decision marked **my call** was made by
the author without asking Lane, as asked; the ones that need Lane's yes are collected in "Decisions for Lane".

**Inputs read:** `docs/ONLINE.md`, `docs/passos/WEB.md`, `docs/passos/MODS.md`, `docs/passos/NATIVO.md`, `TODO.md`,
`docs/superpowers/plans/` and `specs/`, the performance audit (`docs/investigacoes/2026-11-19/performance-audit.md` on branch
`sightama/rollback-cost`), the rollback synctest report (`docs/investigacoes/2026-11-18/rollback-synctest/README.md`), the open
issues (#10, #30, #31, #42, #44, #77 and the rest of the open list), PRs #46, #48, #49, #73, #68, #69, and upstream
`yanm1103/War_of_the_Monsters_recomp` at `upstream/master` (377 commits ahead of `wip/browser`).

## Global constraints

- Browser first: Chrome and Firefox on a computer are the release; Mac and Windows native builds stay in CI because the browser
  game is built from the same code (`TODO.md`, "Direction").
- Nothing from the game goes into the repo or into anything a player shares (`CLAUDE.md`, rule 1). A creation holds recipes and
  original art, never disc bytes.
- Every online change keeps the desync matrix at 0 and the build id rule: all PCs run the same build.
- Real browsers prove performance claims (Chrome, Firefox, Brave), and every test run is muted (`WOTM_MUTE=1`, `--mute-audio`).
- English only; no em or en dashes anywhere; effort is counted in lines of code and PRs, never in time.
- The owner's "Discarded" list in `TODO.md` stays discarded: our own rollback netcode, 5 to 8 players with a shared camera, an
  unlock screen for mod skins, a panel option to pin the LOD, and the others there. This roadmap proposes none of them.
- Rollback itself stays discarded. The synctest in flight (`WOTM_SYNCTEST`, `native/wotm_rollback.hpp`) is used here only as a
  tool: its whole-state snapshot is what replay seeking needs.

## Determinism classes

Every item below carries one class. It decides how risky the item is and how it must be tested.

| class | meaning | rule |
|---|---|---|
| **P** presentation | changes only what one player sees or hears (drawing, page, HUD, audio output) | the game never reads it; no desync possible; test in real browsers |
| **I** input side | changes how one player's input is produced before it is sampled into the lockstep | only inputs travel, so each player may differ; the transform must run before the input is sampled |
| **S** simulation | changes what the game computes (stage data, monsters, rules, the update path) | every PC must compute the same thing; needs the same build, the same content digest, a desync-matrix run and a snapshot compare |

Settings that are P or I can differ between players in one match. S content must be identical on every PC, which is what the
mods digest enforces today.

## Ground truth

What exists today, from the docs and code listed above. Items marked (done) are not proposed again.

**QoL already built (done):** widescreen Hor+ with `WIDE HUD` and aspect ratio options; frame interpolation; key and button
rebinding inside the game's panel (`KB_<BUTTON>` lines in `settings.ini`); vibration; touch controls with a layout, size and
opacity (`web/touch.js`); a pad hub with four slots (`web/pads.js`); saves backup and restore as zips (`web/saves.js`); photo mode
and free camera offline; startup movies; invite links, lobby chat, pings, device health chips, owner controls, kick and ban;
a PLAY AGAIN screen when a wins target is set; send report with diagnostics (`web/diag.js`, `web/report_ui.js`).

**Content already built (done):** recolor skins made in game (PR #46, in `native/wotm_creator.hpp` and `native/runtime/wotm_creator.inc`), travelling
online by id (protocol 5, #68, #69); maps made by rearranging a stage, as an edit list applied at load (PRs #48, #49: move, turn,
remove, copy, power-up edits, `native/wotm_stagemap.cpp` about 2480 lines); an in-game map editor with a free camera (PR #73, built
on Lane's side and treated as Jake's area by Lane's comment on #77; in the browser build it is the Map editor row of the Settings
card's Mods tab, `native/runtime/wotm_mapedit.inc`); export and import of creations as zips (`web/creations.js`); a mod kit for PNG
textures, sounds and skins (`tools/modkit.py`, `tools/modkit_gui.py`); custom monsters as `monster.json` over a base type
(Goliath Prime, Cerebulon; experimental).

**What is missing for players:** no way to get a friend's creation except copying files by hand; every folder in `mods/` changes the
digest, so a library of ten creations makes nobody able to join anybody; no new geometry, new skeletons or new animations
(`TODO.md` item 8 waits on a `.NGP` writer); the game has no replays, spectators, stats, page-level remapping or accessibility
options; a lobby ends when the match ends in the browser.

**What upstream has that we do not** (`upstream/master`):
- A level editor, `tools/level_editor/`: a three.js page served locally by `editor.py` (about 120 KB), `web/app.js` (about 334 KB),
  menus, undo and redo, multi-select, collision view, texture and light panels, sky, terrain from a heightmap, props from Blender
  `.glb`, prefabs, markers, pedestrian and AI nets, HUD panel, pack-as-mod, a "Test in game" button, and about 20 test files. It
  needs the player's own disc and a local Python; it is not hostable as a public page because the stage decode (seven
  `tools/lvl*.py` files, among them `lvl.py`, 3181 lines; `lvlprop.py`, 1695; `lvlglb.py`, 277) runs server side. It also imports
  upstream's `tools/modkit.py` (1729 lines against our 912: it adds the disc as a folder or an `.iso`, `disc_cache`,
  `disc_listdir`). The diff against our `master` for the folder alone is 138 files and 25,831 inserted lines, most of it vendored
  three.js and `app.js`; with the `lvl*.py` files, `ngprig.py` and `retarget.py` it is about 34,500 lines. Upstream keeps its docs in
  `docs/guides/` where ours are in `docs/passos/`.
- A level patch loader in the native layer: `wotm_levelpatch.inc` (2478 lines), `wotm_levelprops.inc` (759), `wotm_levelsel.inc`
  (483), `wotm_levelreload.inc` (275), `wotm_levelpeds.inc` (244). Its patch format (`mods/<mod>/levels/<STAGE>.json`) names objects
  by handle and adds `hp`, `scale`, `traffic`, `peds`, props, terrain and path cuts that ours lacks (ours has riders, power-up edits
  and a catalogue of loader limits that theirs lacks).
- A monster pipeline for **new models**: `files` in `monster.json`, `Import mesh...`, `Import animations...`, `Import rig...`
  (a Blender armature of its own, `tools/ngprig.py`), Blender export, and the mods that use it (Godzilla, CJ, Mecha). This is the
  `.NGP` coming back in that our `TODO.md` item 8 is waiting for.

**Collision to resolve before importing anything:** both repos use `mods/<mod>/stages/<id>/stage.json` for a map, with different
schemas (ours: `name`, `base`, `baseHash`, `edits`; theirs: `id`, `name`, `base`, `narrator`, `random`, plus a separate
`levels/<ID>.json` patch). Same path, different meaning.

**Performance facts that bound the work** (the audit): the game thread is the limit; 35 to 40% of it is the hierarchy walk and 23
to 41% the VU0 programs; a redone frame costs as much as a played one (about 5.6 ms in Chrome on a fast Mac, so about 3 per 16.7 ms
there and about 1 in Firefox, where a played frame is already near 19 ms in live matches), so replays fast forward only in Chrome
and cannot be scrubbed cheaply; the main thread renderer is about 25 to 30% CPU skinning, vertex lighting and the dust texture. Snapshots hold 39.7 MiB (1 vs 1) to 46.9 MiB (8 monsters).

## Claims and collaboration

Checked on 2026-11-19 with the rules in `CLAUDE.md` ("Before taking a task").

| area | who has it | evidence | what this roadmap does |
|---|---|---|---|
| Map creator (#10), PRs #48, #49 | Jake (`jrkubic`) | filed and driven by Jake; #48 (rearrange) and #49 (copy) merged 2026-11-15; the issue's own steps after that (stage variants, the `.NGP` writer, object editor, glTF, lobby sharing) are his plan, no assignee | **does not take it.** Proposes shared ground rules and offers the pieces he does not want (below) |
| Character creator (#42), PR #46 | Jake | merged 2026-11-15; steps 2 and 3 parked by Lane's review on 2026-11-16 | does not take it; reuses its folder format |
| In-game map editor (PR #73) and its free camera (#77) | Jake's area by Lane's decision | #73 and #77 were authored on Lane's side; Lane took #77 off the factory queue on 2026-11-16 as Jake's call | untouched; the split below asks Jake whether he wants it |
| Upstream merge (#31) | unclaimed | no assignee, no comments, no branch, no open PR (Jake's `docs/upstream-claim-check` branch, PR #151, only changes the claim rules in `CLAUDE.md`) | proposed as slices (Part B, B0); whoever starts claims the issue first |
| Port of the online stack to `master` (#160) | Jake (filed 2026-11-19) | one batch of `wip/browser` #133 to #158 onto `master`, which lacks 139 commits of `wip/browser` and has 44 of its own | B0 targets `wip/browser`, the release branch; the slices reach `master` through that port |
| Rollback synctest and cost (#30, PR #169) | Lane's workers | branch `sightama/rollback-cost`, PR #169 open | read only; replays reuse its snapshot code after it merges |
| Online seamless plan, 30 Hz switch (#124), desync in 3 players (#35) | Lane's workers | `docs/superpowers/plans/2026-11-17-online-seamless.md` | not touched; items here that edit `native/net/` are marked as conflict zones |
| Performance audit items | Lane's workers | PR #169; the audit's "Order of work": A, then F, D and B, then E, H and I; C waits for Lane | Part C only sequences them |
| TODO.md order | Lane | `CLAUDE.md`: "the list and its order belong to the owner" | this roadmap does not edit `TODO.md`; Lane moves items |

**Proposed split with Jake** (his call on every line, to be posted on #10 and #42 with a link to this document):
1. Jake keeps both in-game editors and decides their next steps. They are the casual, in-browser surfaces and they already ship in the
   browser build.
2. The sharing layer (lobby transfer, links, the digest) is network and server code that Lane's workers own (protocol 5 was theirs),
   so Lane's side builds it, and Jake's step 5 in #10 ("sharing in the lobby") is handed over only if he agrees.
3. Jake is asked to adopt one small thing in his writers: a `creation.json` manifest (Part C, C1). If he prefers, Lane's side sends
   that PR to him for review instead of merging it.
4. Jake is told what upstream has (#10 already says so) and is asked for his preference between the options in the #10 comment
   of 2026-11-18; this roadmap recommends option 2 below.

**Proposed approach to upstream** (Lane's call; draft in "Draft note to Yan"): stop treating the 377-commit gap as one merge. Take it
by slices, tools first, with a determinism check on every native slice, and open one upstream issue about the `stages/<id>/stage.json`
name collision before either side ships more.

## Part A: quality of life

Ranked by value over effort. Value is how much a player notices; effort is code written, with PR counts; risk is the determinism
class plus anything else that can go wrong. Items A1 to A5 are the top five.

| # | feature | value | effort | class | depends on |
|---|---|---|---|---|---|
| A1 | Replays from the input log, with director mode | high | about 1,000 lines, 3 PRs | P (recording), S (playback must match the build) | C3 versioned builds |
| A2 | Quick rematch: the lobby survives the match | high | about 450 lines, 2 PRs | P (lobby layer) | none; conflict zone `native/net/` |
| A3 | Spectate | medium-high | about 650 lines, 3 PRs | P | A1 |
| A4 | Remappable keys and touch layouts on the page | medium-high | about 500 lines, 2 PRs | I | none |
| A5 | Accessibility pack | medium-high | about 900 lines, 3 PRs | P and I | none |
| A6 | Match stats and a post-match card | medium | about 600 lines, 2 PRs | P (reads state) | none |
| A7 | Real-hardware pad and phone pass, installable page | medium | about 300 lines, 2 PRs | P | C3 for the service worker |

**A1. Replays from the deterministic input log** (value high). The game is deterministic and only inputs travel, so a replay is the
match header plus the inputs per frame. The header is the `MATCH` environment (stage or map id, monsters and skins, modifiers, wins,
`hz`), the build id, the wasm hash and the digest of the creations used. The body is per slot, per frame pad state (buttons, sticks,
the camera input the lockstep already carries), delta coded and deflated: a few KB per minute. Playback runs the same fixed step
offline with the file as the input source, so it reuses the lockstep path instead of adding a second input path.
- Why it is worth more than it looks: the same file reproduces a desync or a lag report. "Send report" can attach the replay, so a
  bug report carries its own reproduction, which is what the diagnostics pipeline lacks.
- Director mode: playback is offline, so the free camera and photo mode (F5, F6, F7, F8, off online) are legal in it. Add speed
  control (pause and step exist; fast forward is bounded by the EE's real speed: about 3 game frames per presented frame in Chrome on a fast Mac and about real time in Firefox, so fast forward is a Chrome feature) and a
  save-as-video later through WebCodecs, with the Craft Apps (`filmcraft-cli`) for trims and titles.
- Hard limit: a replay is valid for one build. A newer build changes the computation. So replays need versioned builds (C3), or
  they die at the next deploy. The page refuses a replay for an unknown build with a plain message and, with C3, loads the right
  build instead.
- Seeking backwards needs snapshots of 40 MiB each; store one every 10 s in memory only for a session, never in the file.
- PRs: (1) file format, recorder and player in `native/wotm_replay.hpp` plus `native/runtime/wotm_replay.inc`, and a check
  `tools/replay_check.py` that records a fight, plays it back, and compares the state hash every 60 frames (about 450 lines);
  (2) page: list, download, upload, play, build guard, attach to Send report (about 350 lines of `web/replays*.js` with tests);
  (3) director mode: speed, free camera, keyframes in memory (about 200 lines).
- Risk to determinism: none from recording (it reads inputs). Playback risk is the same as the synctest's, which found two leaks
  that the lockstep could not see and fixed them; PR 1 repeats that proof on the browser build before PR 2 ships.

**A2. Quick rematch** (value high for game nights). In the browser a finished match reloads the page and the server closes the lobby
(`docs/passos/WEB.md`, "Online"), so the friends re-enter a new code every round. Change: the lobby survives `match_over` in a
`lingering` state for 120 s; each member gets a token at join (kept in `sessionStorage`); after the reload the page sends
`REJOIN <code> <token>` and lands in the same lobby with the same members, stage and settings; the owner presses Start again.
- Protocol: one additive command, so an older server answers with an unknown command error, which the page already handles for
  `DIAG`; the protocol number stays 5 unless the lobby message format changes.
- PRs: (1) server: linger state, tokens, expiry, tests in `native/net/` (about 250 lines); (2) page: store the token, auto rejoin,
  "Rematch" button, a `browser_mp_test` session (about 200 lines).
- Risk: lobby layer only. Conflict zone: `native/net/wotm_net_server.cpp`, where the online seamless work lives; send PR 1 after
  that plan's net tasks merge or coordinate by comment on #30.

**A3. Spectate.** A spectator is a connection that receives the relayed inputs and never sends. It joins with a replay catch-up
(fast forward through the frames so far, shown with a progress bar) and then follows live, so it needs A1's player. The server must
leave spectators out of the lockstep barrier and out of the holdback logic (`native/net/wotm_net_holdback.hpp`), otherwise a slow
viewer slows the players, which is the failure the 2026-11-17 slow-motion night showed. Cap at 8 spectators per lobby.
- PRs: (1) server role, relay, barrier exclusion, a synthetic test where a stalled spectator never changes the players' pace (about
  300 lines); (2) game: spectator input source and catch-up (about 200 lines); (3) page: "Watch" in the lobby list for lobbies in a
  match (about 150 lines).
- Risk: none to the players if the barrier test passes; every spectator frame is computed locally.

**A4. Remappable controls on the page.** `TODO.md` lists it under out of order: the page's Controls tab only shows defaults while the
game's own panel rebinds. Add a capture UI that writes the `KB_<BUTTON>` lines through `updateSetting`, reads the real key labels with
the Keyboard Map API (`navigator.keyboard.getLayoutMap()`, so AZERTY and QWERTZ players see their own letters), warns on conflicts,
and adds presets (WASD, arrows, left handed). Touch gets drag-to-place buttons and a left-handed preset in the gear panel.
- Class I: the mapping happens before the input is sampled, so players may differ.
- PRs: (1) keyboard capture, labels, conflicts, tests with `controls.test.mjs` style (about 350 lines); (2) touch layout editor
  (about 150 lines). Page only; no change to the wasm and the build id.

**A5. Accessibility pack.** Three independent PRs.
1. HUD scale and high contrast outlines for the player markers, with colorblind-safe palettes, in the native drawing near
   `Scene::drawHudAtEdges` (about 350 lines, class P).
2. The page honors `prefers-reduced-motion`, `prefers-contrast` and `prefers-color-scheme`; the game gets a reduce-flashing and
   reduce-shake switch applied to the native fade and camera drawing (about 300 lines, class P). The shake switch applies only to the
   modern camera and the native fades, never to a value the simulation reads.
3. Input assists: hold-to-toggle for block, mash-to-hold for repeated presses, and a sticky special chord, implemented where the pad
   is sampled so only the result travels (about 250 lines, class I, with a test that two players with different assists stay in sync
   in the desync matrix).
- Captions for narrator lines need a transcript per monster and stage; that is a content task (data entry), listed as later.

**A6. Match stats and post-match card.** Read-only counters from game state (damage dealt, buildings destroyed, KOs, time alive,
accuracy), shown on the result screen and kept in the browser's file system. Because the game is deterministic, every client
computes the same numbers; the server can accept a result only when the clients agree, so stats cannot be forged without every
player cooperating. The card is an image the page draws, shareable like a screenshot. `WOTM_MODSTATS` already reads some of this.
- PRs: (1) counters in `native/` and the agreement check (about 350 lines); (2) page card, history list (about 250 lines).

**A7. Real hardware and installable page.** The Gaps list says touch and pads were checked only with emulation. One checklist-driven
pass on a real Android phone and real non-standard pads (`fallbackLayout` is a guess), with fixes found. Then a web app manifest
and a service worker for the page shell. The service worker must key on `version.json`, otherwise it serves a stale wasm to a server
that expects a new build id; it depends on C3 to be safe.

**Not proposed.** Cloud saves stay parked (`TODO.md`, out of order, the owner has not decided). The creations library needs a
storage story anyway, and Part B's link sharing gives players a way to move a creation between devices without an account.
Own rollback netcode, an unlock screen for mod skins and 5 to 8 player shared camera are discarded by the owner.

## Part B: content creation

### B-Decisions (my calls)

1. **One creation format, several authoring surfaces, one sharing path.** Players author in whatever fits them (Jake's in-game
   editors, upstream's desktop editor, Blender and the Craft Apps), and everything lands as the same small folder, validated by one
   checker and shared by one mechanism. This avoids building a second editor.
2. **Reuse upstream's editor and monster pipeline; do not rebuild them.** `app.js` plus the Python tools are tens of thousands of
   working lines with tests, GPL-3.0 like the fork, and Yan is still pushing (2026-11-18). Rebuilding costs more than adopting and
   gives players nothing extra. This is option 2 of the 2026-11-18 comment on #10, with an addition: our in-game editors and the
   upstream editor write the same format.
3. **Disc-free recipes by default.** A map is an edit list. A recolor skin should be a tint recipe (hue, saturation, brightness per
   texture region, plus the texture region ids) that the player's own disc turns into PNGs at load, because that is tiny, travels
   in a link, can be digest checked, and ships no game art. PNG skins and anything with models stay as files and are shared by file or
   lobby transfer only.
4. **Digest by use, not by folder.** A match's digest covers the content that match uses (the stage or map, the custom monsters and
   the S-class files of the chosen monsters), so owning twenty creations never blocks a join. Skins that are PNG only are expected
   to be class P (the creator's own done-when says "the skin is drawing only"): a player who lacks one sees the base costume, and
   the lobby offers the transfer. A skin with its own `.NGP` or sound is class S until proven otherwise and goes in the digest.
   **This rests on an assumption that does not hold today.** Skin costume numbers are "assigned on each boot, in mod/id order"
   (`MODS.md`, Skins) and the first six maps across all enabled mods take stage slots 18 to 23 (`MODS.md`, "How it loads"), so two
   PCs with different libraries get different costume indexes and stage slots, which are game state. The digest forces identical
   libraries today and that is what keeps it safe. The assumption becomes true only when costume index and stage slot depend on the
   match alone (the ids in `MATCH`), never on the local library; C1 PR 1 makes that change and proves it with a two-PC match whose
   libraries differ, through the desync matrix. Until then the digest stays by folder. The six-map cap is a stated limit of the
   stage select, so a "library of twenty" means twenty installed and six pickable per session unless the select grows.
5. **No hosted gallery in the first phases.** Hosting means moderation, takedowns and storage of derived game art. Links with the
   recipe inside the fragment, and transfer between lobby members, need no server storage. A content-addressed store is an optional
   last phase and Lane's decision.
6. **The in-browser version of upstream's editor waits for evidence.** It needs the stage decode in JavaScript or in the wasm (3,000
   to 5,000 lines of port) and the local tool covers power users. Build it only if creators ask.

### B0. Import upstream by slices (needs the claim on #31)

| slice | what | effort | class | notes |
|---|---|---|---|---|
| 1 | `tools/level_editor/`, the seven `tools/lvl*.py` files, their tests, and upstream's `tools/modkit.py` (the editor imports its disc reading; ours is 912 lines against 1729, so this is a merge of the kit, which B4 uses too) | 2 PRs: the editor with `lvl*.py` (a copy of about 34,500 lines; the Portuguese there is nearly none: `editor.py`, `lvl.py`, `lvlprop.py` and `app.js` are English), then the kit merge (+1,674 and -262 lines to reconcile, with its 81 Portuguese lines translated) | none (tools only) | gives creators the editor now; "Test in game" is retargeted to open the browser build with `?WOTM_MATCH=...` and the creation loaded by import; docs go to `docs/passos/` not `docs/guides/`; the imported tree is checked for game bytes (rule 1) before commit |
| 2 | **spike first:** a converter between our `stage.json` edits and upstream's `levels/<STAGE>.json` for the shared vocabulary (move, turn, remove, copy), and a comparison of the two loaders on `CENTRAL` by state hash | 1 PR, about 500 lines of Python and a report | none | answers "which loader is the base" with evidence, and checks that upstream handles and our `node` offsets name the same objects |
| 3 | the winning loader's native files, plus the features the other loader has that it lacks | 3 to 5 PRs, about 4,200 lines landed (the five native files total 4,239) plus a translation commit: `wotm_levelpatch.inc` alone has 153 Portuguese lines and a Portuguese header | **S** | needs the desync matrix on a custom stage with props, scale and traffic; rename the schema collision first |
| 4 | the monster pipeline: `files` in `monster.json`, `ngprig.py`, `retarget.py`, `modkit` monster commands, `wotm_mods.hpp` and `wotm_custom.inc` changes (+2,095 and -510 lines against ours) | 3 to 4 PRs, about 3,500 lines landed, each followed by its translation commit | **S** | settles `TODO.md` item 8 without writing the `.NGP` writer ourselves. No `mods/` content comes with it: upstream's Godzilla and CJ mods carry `.NGP`, `.TEX`, `.RTX` and `.BNK` files that look disc derived plus third party characters, so rule 1 and the characters' rights both rule them out; only the tooling is imported |

Slice 3 touches Jake's area: `native/wotm_stagemap.*`, `wotm_mapedit*`, `wotm_creator*`. Nothing in it starts without his reply on
#10. If he prefers to keep his loader as the base, slice 3 becomes "port upstream's extra features into `wotm_stagemap`" and the
spike still tells which features carry over. Backward compatibility for existing `stage.json` creations needs Lane's explicit yes
(see "Decisions for Lane").

### B1. The creation pack (C1 is its engine)

Every creation is a folder with a `creation.json` manifest at its root: `format`, `kind` (`skin`, `stage`, `monster`, later
`rules`), `id`, `name`, `author`, `requires` (build id range, base stage hash as `baseHash` does today), a file list with SHA-256,
and `class` (P or S). The page, the lobby, the validator, the kit and the in-game editors all read this one file. Jake's writers add
it on save. Detail in C1.

### B2. Sharing, in three tiers

1. **File** (done): zip export and import in the Mods tab (`web/creations.js`, strict path checks, 256 entries and 32 MB caps). Add a
   preview thumbnail and the manifest to the list.
2. **Link** (new): `https://playwotm.com/#c=<base64url of the deflated recipe>`, up to about 2 KB, for recipe-only creations (a
   map edit list, a skin tint recipe). Opening the link shows "Install <name> by <author>?" with what it changes, then writes it to
   `mods/`. Nothing is stored on the server. Larger creations use tier 3.
3. **Lobby transfer** (new): when a joiner lacks a creation the match uses, the owner's page offers the files over the lobby
   connection through the server (size cap 4 MB per match, 64 files, SHA-256 checked against the manifest, same strict path
   validation as import, only data kinds: JSON, PNG, WAV, `.NGP`/`.PTR`, never scripts). The joiner sees a consent prompt, the
   files are written before the game boots (an online match already boots the wasm fresh into the fight, so the registry that scans
   at boot sees them with no restart). This is the "sharing in the lobby" step of #10.
   - PRs: (1) digest by use and per-creation digests in the `MATCH` line (about 300 lines, class S, conflict zone `native/net/`);
     (2) transfer: server relay of blobs with caps and tests, page prompt and writer (about 700 lines); (3) link install (about 250
     lines of `web/links.js`).
   - Risk: a malicious creation. Mitigations: data kinds only, caps, hash checks, consent, the loader's existing limits (4096 edits,
     reserve checks) and the invariant that a refused creation changes nothing. A fuzz test over `stage.json` and the recipe
     decoder belongs to PR 2.

### B3. Authoring surfaces and who owns them

| surface | for whom | status | owner |
|---|---|---|---|
| Skin creator in game (recolor) | anyone, in the browser | shipped | Jake |
| Map editor in game (move, turn, remove, copy) | anyone, in the browser | shipped | Jake |
| Stage variants (recolor by region, sky, music), the cheapest custom map | anyone | Jake's step 1 in #10, not built | Jake |
| "Your creations" hub: thumbnails, publish as link, install, delete | anyone | list exists in the Mods tab | Lane's side builds the page parts (B2), Jake's side the in-game entry |
| Upstream level editor (local) | power users with Python and their own disc | slice 1 of B0 | Lane's side |
| Monster pipeline with Blender | power users | slice 4 of B0 | Lane's side, after Jake's reply |
| Rule packs (custom game modes) | anyone | later, below | open |

**Rule packs** (low priority, class S): today's modifiers (Super Jump, Fast Monsters, Turbo) are code with a lobby string. A
`rules` creation kind holds a small parameter table (gravity, walk speed, damage, health, power-up rates) that the game reads at the
stage load, in the digest. It turns the modifiers into data and gives creators a way to make a mode with no code. About 450 lines,
2 PRs, after C1.

### B4. Asset pipeline with the Craft Apps

The Craft Apps are editors with no model inside and no 3D, rigs or maps (the skill's own limit); AI generation is not available.
MCP file access is confined to `~/craft-work`, so every recipe copies inputs in and results out. Models and rigs are Blender's
(the upstream pipeline already imports its `.glb`).

| asset | tool | recipe |
|---|---|---|
| costume textures, portraits (`rtx_t0011.png`), HUD art | `photocraft` | `modkit.py export <mod> MON/<NAME>` to `~/craft-work`, edit with masks per texture id (the creator's region masks), keep the original aspect ratio and the alpha rule (255 is the game's opaque), save PNG, `modkit.py pack`, `modkit.py check` |
| stage select photos (`levelpics/<id>.png`, 2:1), logos, banners | `vectorcraft` | draw at the exact aspect, export PNG at the size `MODS.md` asks |
| particle sheets (explosion 32 frames of 64 by 64), fire colors | `photocraft` and `effectcraft` | compose frames, export a sheet PNG, name it by the stage and texture id |
| narrator lines (`narrator/<id>.mp3`), monster sounds, music | `soundcraft` | record or edit, normalize loudness, bounce WAV or MP3, `modkit.py pack` rebuilds `.BNK`/`.VPK` at the original rate; sound length is simulation-relevant in banks, so a changed length makes the creation class S |
| replay clips, trailers | `filmcraft-cli`, `effectcraft` | from A1's video export; titles and trims |
| balance tables for rule packs | `gridcraft-cli` | a sheet exported to the `rules` parameter table |
| creator guide, press kit | `wordcraft`, `pdfcraft`, `deckcraft` | optional |

Deliverables: one script `tools/creation_new.py <kind> <id>` that scaffolds a folder and its work folder; one script
`tools/creation_check.py <folder>` that validates the manifest, sizes, aspect ratios, caps and class, and returns a short report
with a way to see the full one (the validator CI runs on every shipped creation); and one agent-facing guide,
`docs/passos/CREATOR_GUIDE.md`, with the table above as copy-paste recipes. About 500 lines of Python, 2 PRs.

### Draft note to Yan (for Lane to send or discard)

> Hi Yan, we run a fork of War_of_the_Monsters_recomp that plays in the browser (WebAssembly, online lobbies, playwotm.com) and is
> English only. We would like to take your level editor and monster pipeline as the base for our players' creations. Two things to
> agree on first: we both use `mods/<mod>/stages/<id>/stage.json` for a map with different schemas; could yours carry a `format` key
> (or move to `custom_stages`) so both tools can tell them apart? And we would like to send the pieces that help you back: a patch
> format check that fails on a changed base stage, and the determinism checks we use for online play. Would you rather we open
> issues or pull requests, and which slices do you want to keep stable?

## Part C: architecture

Each item has value, effort, risk and what it depends on. Classes as above.

| # | change | value | effort | class | depends on |
|---|---|---|---|---|---|
| C1 | Creation pack: manifest, registry, digest by use | very high (enables all of Part B) | about 1,100 lines, 3 PRs | S (digest) | Jake's review of writer changes |
| C2 | State hash over everything that can diverge | high | about 200 lines, 1 PR | none (read only) | none |
| C3 | Build-versioned deploys | high | about 350 lines, 2 PRs (one in the private coord repo) | none | none |
| C4 | Native renderer on the GPU | medium-high | about 1,800 lines, 3 PRs | P | none |
| C5 | Game-thread cost, the audit's list in order | high | audit's own sizes | mostly S | the audit's owner |
| C6 | Asset streaming and compressed textures | medium | about 600 lines, 2 PRs | P | C3, C1 |
| C7 | Rules as data (modifiers table) | medium | see B3, rule packs | S | C1 |

**C1. Creation pack and registry** (value very high). Today `native/wotm_mods.hpp` (730 lines) scans `mods/` at boot and hashes the
whole folder; skins register again after a save but stages and monsters do not. Replace the scattered scans with one registry that
loads `creation.json`, validates, assigns ids, exposes the lists the selection screens read, rescans on demand, and computes a
digest per creation and per match (B-decision 4). The kit and the page read the same manifest. Hashing is over raw file bytes, never
over parsed JSON, so every PC computes the same digest.
- C1 PR 1 stays stage-agnostic: it freezes the manifest only for `skin` and leaves `kind: stage` open until the loader spike
  (B0 slice 2) and the answer on the `stages/<id>/stage.json` collision are in, because upstream's `wotm_mods.hpp` (982 lines
  against our 730) already parses that file with another schema.
- PRs: (1) manifest reader, registry, per-creation digest, costume index and stage slot derived from the match alone (not from the
  local library, see B-decision 4), and the digest by use in the `MATCH` line, with a native test and a two-PC match whose
  libraries differ that passes the desync matrix (about 600 lines); (2) the page and kit read the manifest, the list shows
  thumbnails and classes (about 350 lines); (3) the in-game writers add the manifest on save, reviewed by Jake (about 250 lines).
- Risk: the digest decides who may play together, so a bug that makes two honest PCs disagree blocks every match. The first PR runs
  the desync matrix and the existing `net_checks` mods test before anything else ships.

**C2. A state hash that covers the content.** The per-frame check covers the game's 32 MB. The map reserve at the top of RAM (4 MB
at `0x3BFF000`, where map copies live) and the monsters' files above 32 MB are outside it, and the docs already say a difference
there "would not be caught as it happens". As creations grow, that is where divergence will hide. Extend the hashed range list to
the map reserve and the upper RAM in use (7.7 MiB in a 1 vs 1, 14.9 MiB with 8 monsters per the synctest snapshot), measure the
cost per frame in Chrome and Firefox before turning it on by default, and hash upper RAM less often if it is slow. One PR, about
200 lines, with a test that a one byte difference in the reserve is reported.

**C3. Build-versioned deploys.** Serve each build under an immutable path (`/b/<build id>/`: `wotm.js`, `wotm.wasm`, `version.json`,
the page scripts that must ship with the wasm), keep the last 10, and let the page pick the build a lobby or a replay names.
Gains: a deploy during a game night no longer breaks a rejoin ("different build"); replays outlive deploys (A1); the browser's wasm
code cache works on an immutable URL (the audit measured the wasm at 41 MB, 17 s at 20 Mbps before brotli); a service worker
(A7) has a safe key. Storage is small (about 4 MB brotli per build). One PR in the coord repo (deploy script, retention) and one in
this repo (`web/boot.js` choosing a build, about 200 lines, tests with a fake server).

**C4. The native renderer on the GPU.** Audit item 4: per-vertex lighting (`Scene::vertexRgb`), skinning (`skinVertexPM`) and the
dust texture (`dustTexture`) run on the CPU and are about 30% of the page's main thread. Move them to vertex and fragment shaders
in the WebGL2 path. Class P, so no desync risk; the check is pixel comparison on the corpus plus the real-browser frame profile.
It frees the main thread on low core devices that today compete with the game thread, and opens the door to HD texture packs,
better shadows and optional post effects, all of which creators will want. Medium-large; split into lighting, skinning and dust.

**C5. Game-thread cost.** The audit's ranked list is the plan (VU0 rounding and flags, Firefox lazy tiering, disc chunk hashing,
runtime seams). Items that change what the game computes (transforms recomputed for static objects, shadows, off-screen
civilians) need Lane's go and the desync matrix at 0; this roadmap changes none of that, it only asks that C1 and C2 land before the
S-class performance items so a divergence has somewhere to be caught.

**C6. Asset streaming.** Fight data is about 22 MiB per match and is prefetched while the lobby waits; music streams are read whole
on the game thread; mod PNGs decode on load. For creator content: decode PNG textures off the main thread, and accept compressed
textures (KTX2 with Basis) for HD packs so a 4x texture does not cost 16x memory; make the match prefetch (`web/match_prefetch.js`)
fetch the creations a match uses by digest, in the same pass as the disc chunks. Two PRs, after C1 and C3.

**Top three, in order:** C1 (everything in Part B stands on it), C2 with C3 (online and replays stay trustworthy as content grows
and builds change), then C4 (the one large presentation win with no determinism risk). C5 continues in its owner's hands.

## Sequencing

Nothing starts without its claim (issue assignment or a comment "Working on this: <who>, branch <name>").

| step | content | why here |
|---|---|---|
| 0 | Post the Jake split on #10 and #42; claim the #31 slice; Lane decides the items in "Decisions for Lane" | unblocks every owner question |
| 1 | A4 remap (page only), A5 PR 1 (HUD scale), C3 versioned builds, C2 state hash, B0 slice 1 (upstream tools), B0 slice 2 (loader spike) | small, independent, no S-class change; gives creators the editor and the evidence for step 3 |
| 2 | C1 creation pack, A2 quick rematch, A1 replays (PR 1, then 2), B4 scripts and guide | foundations for sharing and the first big player-visible wins |
| 3 | B2 digest by use, lobby transfer and links, A1 PR 3, A3 spectate, A6 stats, A5 PRs 2 and 3 | sharing and watching |
| 4 | B0 slices 3 and 4 (upstream loader, monsters), C4 GPU renderer, C6 streaming, rule packs | the long, deep items; each gated by the spike or by Jake's answer |
| later | in-browser editor, hosted gallery, captions | only on demand |

PR count if everything ships: about 45 PRs. Total new code is about 12,000 lines of our own plus about 45,000 lines landed from upstream (34,500 of them the editor, the `lvl*.py` tools and their vendored libraries; 4,200 the native level loader; 3,500 the monster pipeline; the rest the kit merge).

## Decisions for Lane

Only these need a yes; everything else above is the author's call and can be overridden in review.
1. **Backward compatibility of existing `stage.json` and skin folders.** `CLAUDE.md` requires explicit approval for it. Default
   recommendation: yes for one release, as a one way converter at load, because Jake's and Lane's test maps would otherwise
   break; remove it after the next deploy.
2. **Contacting Yan** with the draft above, or leaving upstream alone and importing quietly. Recommendation: contact, because the
   schema collision exists either way.
3. **A hosted gallery** stays out of phases 1 to 3 (B-decision 5); and whether anything derived from disc art may be shared at all
   by lobby transfer: PNG skins, but also `.NGP`, `.PTR`, `.TEX`, `.RTX`, `.BNK` and WAV files made from disc data, which tier 3
   would relay. Rule 1 forbids game bytes in the repo; the site's own position on derived art is Lane's call. Recommendation:
   recipes and original art travel freely; derived files travel only between lobby members, only with consent, and are never
   published or stored by us. The tooling imported from upstream (B0) brings no `mods/` content.
4. **`TODO.md` item 8** (mods that add monsters) rewritten from "write the `.NGP` writer" to "adopt upstream's pipeline". Lane edits
   the list; this document only recommends it.
5. **Versioned builds on the VM and 10 retained builds** (about 40 MB brotli on disk, more raw). Recommendation: yes.

## Review focus

The failure modes this roadmap implies and no task's tests cover yet, most likely first. Each gets its test in the item's own plan.
1. Two honest PCs computing different digests (line endings, path order, JSON key order) so nobody can join: C1 PR 1 hashes bytes
   in a fixed path order and tests the same folder from Windows and Mac line endings.
2. Two PCs with different libraries assigning different costume indexes or stage slots, a desync that the digest by use would
   stop hiding: C1 PR 1's two-PC match with different libraries, run through the desync matrix, before the digest narrows.
2. A replay recorded on one build played on another, silently wrong: A1 PR 2 refuses on a build or wasm hash mismatch and a test
   plays a mutated header.
3. A stalled spectator slowing a match: A3 PR 1's synthetic stall test.
4. A hostile creation file (huge, deep paths, bad JSON, over the caps, an edit list that passes the reserve): B2 PR 2's fuzz test and
   the loader's "refused creation changes nothing" invariant.
5. A new S-class loader on a custom stage diverging from the retail path only under copies, scale or traffic: B0 slice 3 runs the
   desync matrix on a stage that uses each feature, and C2 covers the map reserve first.
6. The service worker or a cached page serving an old wasm to a newer server: A7 waits for C3 and keys on `version.json`.

## Self-review against the brief

- (A) QoL: A1 to A7 with value, effort in lines and PRs, class and dependencies; top five named.
- (B) Content: a recommended path (reuse upstream's pipeline, one format, recipes by default, link and lobby sharing, digest by
  use), the Jake split, the upstream slices, the mod format, and the Craft Apps pipeline with their real limits.
- (C) Architecture: C1 to C7 with value, effort, determinism class and dependencies; top three named.
- Claims: Jake's areas are not taken; the unclaimed #31 is proposed as slices and needs a claim; Lane's workers' areas are only
  referenced.
- Open risks: the numbers are estimates by comparison with existing files (for example `wotm_stagemap.cpp`, 2480 lines; the
  upstream loader files, about 4,200 lines) and are refined in each item's own plan.
