# Where a Firefox frame goes, and how much a redo can leave out (issue #30), 2026-11-18

Question: the synctest (`README.md` here) found a redone frame costs about as much as a played one, so the rollback window that fits in
Firefox is 0. Where does the EE thread's time go in Firefox, and could a redone frame be made cheap enough for rollback to help
Firefox players?

**Answer: no, not by skipping work in the redo.** The drawing-only work a redo can skip is about 10% of a frame (shadows, VU1
packets, object culling), on top of the 2 to 5% already skipped (native capture, sound mixing). The rest is the game's own state:
the hierarchy walk's transforms (`hier`, with the VU0 microprograms it runs, a third of all the time), animation, collision and the
game update. A redone frame at about 0.85 to 0.9 of a played one still leaves Firefox far from fitting even one frame of rollback.
What would change the answer is a general speedup of about 2x (every frame, played or redone), not a rollback trick.

## How it was measured

- `node tools/browser_mp_test/profile_firefox.mjs` runs an offline AI vs AI direct fight in Playwright's Firefox with the Gecko
  profiler started from the environment (`MOZ_PROFILER_STARTUP`, 1 ms interval, JS and native stacks), and writes the profile when the
  browser closes. The page must be built with wasm names: `CMAKE_EXE_LINKER_FLAGS=--profiling-funcs` in `build_web` (the name
  section only; the code is the same).
- `python3 tools/gecko_profile_top.py <profile> --last 60` picks the busiest worker thread (the EE thread) and lists self and total
  time per function over the busy samples (CPU used since the previous sample).
- What a redo can skip: `WOTM_SYNCTEST_REDO_SKIP=<address>,...` (new, `native/runtime/wotm_rollback.inc`) makes those guest functions
  return at once in the redone frames that are never shown (all but the last of each redo). A byte compare of the snapshot always
  differs then (the skipped function writes buffers), so the test is a second one: the run's per-frame state hashes (`WOTM_STATEHASH`)
  against a run without the skip, with `python3 tools/hashspread.py`. A difference that stays in the same few 128 KB blocks is a buffer
  the game rewrites every frame; one that spreads to dozens of blocks changed what the game does (the fight is chaotic: a changed
  decision reaches `shellGame`, `fx` and the sound buffers within seconds). Two runs without a skip give identical hashes.

M1 Max shared with other workers (load 12 to 30 during the runs), so absolute times are high; the shares are what matters.

## Where the EE thread's time goes (Firefox 160, last ~45 s of the fight)

The EE thread was busy 52% of the time in a 1 vs 1 at 60 Hz (about 8.7 ms a frame) and 66% with 8 monsters at 30 Hz (about 22 ms).
Total time (the function and what it calls), share of the busy samples:

| function | 1 vs 1, 60 Hz | 8 monsters, 30 Hz | what it is |
|---|---|---|---|
| `hier` (both passes, all views) | 43.8% | 47.9% | the hierarchy walk: transforms, culling, VU1 packets, shadows |
| of which `hierTraceCsList` | 23.4% | 29.5% | the coordinate systems' world transforms |
| of which `hierTraceWorld` | 10.0% | 10.0% | the stage's objects |
| VU0 microprograms (`executeVU0Microprogram`, called from `hier` and the game) | 30.4% | 31.3% | recompiled VU0 code; `vu0_01a8` alone is 9.4% self |
| `TheGame::Update` + `Update2` | 29.5% | 35.6% | monsters, pick-ups, AI, collisions, cameras |
| `animationRunGlobal` | 4.8% | 5.8% | animation curves |
| `hierShadow` | 5.3% | 7.4% | shadows (drawing) |
| `pktAddVu1ObjAsm` | 3.2% | 1.6% | VU1 packets (drawing) |
| `psObjectCull` | 1.8% | 1.4% | object culling for the drawing |
| native capture (`wotmCaptureScene`) | 2.6% | 1.0% | already skipped in a redo |
| sound mixing (`snd::Engine::render`) | 2.0% | 0.9% | already skipped in a redo |

The profile's stacks are cut where the scheduler resumes guest code, so `rtMain`'s total (64% in the 1 vs 1) is a lower bound and the
shares above are of all busy samples.

## What a redo can leave out

| skipped in hidden redone frames | 1 vs 1 (3 back) | 8 monsters (2 back) |
|---|---|---|
| `hierShadow` (0x204430) | stays in 2 blocks (stack, `psObjDma`) | spreads (21 blocks, `shellGame`, `fx`): the shadow pass writes state the game reads |
| `psObjectCull` (0x208a20) | stays in 3 blocks (`hierAlphaPkt`, `psObjDma`) | stays in 3 blocks (`hierAlphaPkt`, `psObjDma`) |
| `pktAddVu1ObjAsm` (0x208410) | stays in 6 blocks (`hierObjPkt`, `gsPkt`) | PENDING_PKT |
| the three together | stays in 9 blocks; redone frame 5.04 to 4.43 ms (native) | spreads to 26 blocks (`shellGame`, `fx`, `audioBuff`) |
| `particlePrepDraw` (0x21b078) | spreads (24 blocks) | |
| `texmGetTextureAddrs` (0x206f30) | spreads (22 blocks) | |
| `hierTraceWorld` (0x202f40) | spreads (58 blocks) | |
| `hierTraceCsList` (0x202fd8) | spreads (76 blocks) | |
| `hier` (0x2029b0) | spreads (80 blocks) | |

So the transforms the walk computes are read by the game (collision, cameras, the AI), and only the drawing's leaves can go.

## Expected latency with a cheaper redo

With a redone frame at 0.87 of a played one (the best case above) and the synctest's Firefox costs (played 11.05 ms, save 3.11,
restore 3.03 in a 1 vs 1; 25.19, 3.49 and 3.33 with 8 monsters), one frame of rollback needs 11.05 + 6.14 + 9.6 = 26.8 ms of a
16.7 ms frame, and 25.19 + 6.82 + 21.9 = 53.9 ms of 33.3 ms with 8 monsters. The window that fits stays 0 in Firefox.

What it would take for W = 1 in a Firefox 1 vs 1: played + save + restore + redone under ~15 ms (with margin), about half of today's
26.8 ms. Candidates, none rollback-specific: the hot VU0 microprograms as native SIMD code (they are 22% self time in six programs;
this also speeds every played frame and Chrome), and a snapshot that copies only the pages written since the last one (today 40 MB are
copied each save and restore; most of the 32 MB is the stage and models, which a fight never writes). Both would need their own
measurement before anyone counts on them.
