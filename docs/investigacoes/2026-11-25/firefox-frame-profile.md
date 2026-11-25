# Firefox frame profile on today's build (2026-11-25)

The coordinator asked for a fresh Firefox profile of today's build (wip/browser e245a069, with #193's native VU0), for a 4-monster and
an 8-monster fight, to find the hotspots by self time and whether any is cheaply fixable. That is the remaining lever for waits once a
device can't hold the match rate. Short answer: **no single cheap fix.**
- On this Mac (M1 Max), Firefox's game thread works 28% of the time with 4 monsters and 43% with 8, at the 30 Hz of a 3-or-more-player
  match. The draw on the page's thread works 36% and 44%.
- The cost is spread over the game's own code: hierarchy traversal, collision and VU0. The two pieces of our own code that
  stand out are both already cached or locked by design: the CPU-rasterized shadow and dust textures on the draw, and the sound mix on
  the game thread.

## Method

- Page built with function names (`--profiling-funcs`, `tools/build_web.sh`, copied to `<runtime>/web_dist_prof`), build id
  wl-3d5083e4aa6d45a3.
- `node tools/browser_mp_test/profile_firefox.mjs --match <spec> --step 2 --seconds 60 --env WOTM_MUTE=1`, headless Firefox 157, with
  `--match 3,1,2,4,1` (4 monsters) and `--match 3,1,2,4,5,7,8,9,1` (8 monsters). `--step 2` is the 30 Hz fixed step that online matches
  of 3 or more players use.
- `python3 tools/gecko_profile_top.py <profile> --thread <n> --skip 25`. The profiler's buffer kept the last 9.0 s (4 monsters) and 5.6
  s (8 monsters) of the game thread.
- The top "busy" entry on every thread is the futex wait (`Atomics.wait` under `_do_futex_wait`), which is the thread waiting for the
  next frame, not working. It is left out of the work below. "Work" is busy samples minus those, over all samples of the thread.

## Game thread (the worker that runs the EE)

| | 4 monsters | 8 monsters |
|---|---|---|
| work, share of the time | 28% (about 9.5 ms of a 33.3 ms frame) | 43% (about 14 ms) |
| `hierTraverseAsm` (hierarchy traversal, self) | 7.5% of the work | 6.6% |
| VU0 microprograms (`vu0_*` and `executeVU0Microprogram`, self) | 15.8% | 15.9% |
| collision (`hdSphereVsPoly*`, `hdTraversePath`, `hdTestNode`, `collisTestEnvironmentCollisions`, self) | 6.4% | 6.8% |
| sound mix (`snd::Engine::render`, `Voice::refill`, self) | 4.1% | 7.2% |
| guest dispatch (`dispatchGuestBranch`, `lookupFunction`, `pushDispatchPc`, self) | 2.9% | 3.6% |
| `EeScheduler::checkpointDue` | 1.8% | 1.1% |

- The VU0 programs are already native (#193). `__restrict__` on their generated code measured no gain (`tools/vu0bench.sh --ref`, the
  same day: 189 to 191 ns per call against 185 to 189 natively, wasm within its noise; not merged).
- The guest dispatch is already a flat table lookup (`PS2Runtime::lookupFunction`), plus a diagnostic history push (`pushDispatchPc`)
  on every call; together under 4% of the work, so little is left to gain there.
- The sound engine is mixed on the game thread, one frame at a time, by design in online matches (`setFrameLocked` under
  `WOTM_FIXEDSTEP`, `native/runtime/wotm_snd.inc`). The game reads sound state (`snd_SoundIsStillPlaying_CB`), so moving the mix to
  another thread would have to keep that answer identical on every player's machine. That is not a cheap change.

## Draw (the page's thread)

| | 4 monsters | 8 monsters |
|---|---|---|
| work, share of the time | 36% | 44% |
| `Scene::drawList` (self) | 19.1% of the work | 21.0% |
| `Scene::shadowTexture` (CPU silhouette per shadow caster) | 13.0% | 14.8% |
| `Scene::dustTexture` (CPU dust and smoke compositing) | 12.8% | 11.2% |
| `Scene::vertexRgb` (CPU vertex lighting) | 8.4% | 9.9% |
| `Scene::skinVertexPM` | 3.4% | 2.6% |

- Shadows and dust are already reused when nothing changed (`shadowKey`, the dust hash). With monsters and particles moving they are
  likely redone on most game frames (not counted: the draw profile's `shadowHit`/`shadowMiss` counters would say).
- Making them cheaper means drawing them on the GPU (render-to-texture, as the PS2 did) or at a lower resolution. A lower resolution
  changes the picture.

## What this says

- On this Mac at 30 Hz the average load leaves more than half the frame to spare on both threads; the worst frames were not examined, and
  the profile is headless Firefox with the profiler on.
- On a device two to three times slower (an estimate), the 8-monster game thread (about 14 ms here) would take 28 to 42 ms of the 33 ms
  frame. The levers there are the game code above, none of them cheap.
- The candidates, from the biggest share to the smallest. The percentages are shares of the thread's work, not savings: moving work to
  the GPU removes only part of it.
  1. Shadow and dust silhouettes drawn on the GPU (render-to-texture): about a quarter of the draw's work, a sizeable change in
     `native/wotm_native.hpp`.
  2. Vertex lighting and skinning in a shader instead of on the CPU: about an eighth of the draw's work, also sizeable.
  3. The sound mix's per-voice cost (`Voice::refill`, the resampling in `Engine::render`) on the game thread: about 7% of its work with
     8 monsters, without moving it off the thread.
