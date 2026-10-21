*English translation of [docs/investigacoes/2026-10-19/interpolacao.md](../../../investigacoes/2026-10-19/interpolacao.md). The original is authoritative; this copy may lag behind it.*

# Frame interpolation: survey and plan (2026-10-19)

Code-reading-only investigation (nothing compiled or run). Request: the native layer draws, at the monitor rate, the blend of the last two
game snapshots (camera, object matrices, bone palette), with one frame of delay ([NEXT_STEPS.md](../../NEXT_STEPS.md) (`PROXIMOS_PASSOS.md`),
"Interpolação de quadros" (frame interpolation); [steps/NATIVE.md](../../steps/NATIVE.md) (`passos/NATIVO.md`), "Taxa de apresentação" (presentation rate)).

**Important precedent:** there already was an interpolation, removed in commit `2fa669e` (2026-10-16). It had `lerpM4` with
quaternion + uniform scale decomposition and `slerp`, `lerpCamera` with cut detection (400 units / `dot(fwd) < 0.7`) and
`Scene::blend(A, B, alpha)` matching items by `(node, occurrence)`, palettes by pointer pair and debris by `Sprite::key`, with F4 /
`WOTM_NOINTERP`. It was removed for two reasons: (1) with the logic at 60 and the screen at 60 it only added one frame of delay; (2) the HUD, which the game glues to the
camera, was blended separately from it and jittered. Reason (1) stopped applying once presentation moved to the monitor rate (2026-10-18). Reason (2) has a
solution (below). The old code (`git show 2fa669e -- native/wotm_native.hpp`) is a starting point, but the capture has changed a lot since
then (EE thread, two timings, split screen, RAM shadow for the particles).

## 1. The snapshot today

### What it stores (`native/wotm_native.hpp`)
- `struct Snapshot` (wotm_native.hpp:747-764): lists of `DrawItem` (`items`, `sky`, `overlay`, `hud`, `hp`, `hpOverlay`), `sprites`
  (debris, type 13), `fx` (`FxSystem`: base, index, type, count), `fxRam` (32 MB shadow copy of what the particle drawing
  reads), `flares`, `shadows` (`ShadowBlocker` with the `DrawItem`s that cast), `screenRects` (fades, bars, strips),
  `screenPolys` (`ScreenPolys` HUD), `lights`, `fog`, `cam` (`GameCamera`: pos, fwd, up, fovy), `vctx` (view), `g` (`g_frame`),
  `valid` and `view1` (split screen). **It has no timestamp** (the `double t` was removed in `2fa669e`).
- `DrawItem` (wotm_native.hpp:441-470): `M4 m` (row vector: rows 0-2 = axes with scale, row 3 = translation), `node`
  (HierObject), `animPkt`, `lodIdx`, `pal` (shared_ptr to the 32 palette matrices, shared per block), `fade` (LOD
  cross-fade), `uvOff`, `color`, `spec`, lights in object space, `env`, `hudId`, `cs` (the `_cs` that emitted it), `aref`, `zmsk`, `lodX`,
  point lights (`ptRel`/`ptCol`/`ptFall`, already relative to the object). ~280 bytes each.
- `Scene::takeSnapshot` (wotm_native.hpp:1671-1705) copies the collection lists and reads the camera (`readGameCamera`, :488) and, for each item
  with `animPkt`, the 32 matrices of the palette block (`palette`, :3201) into one `shared_ptr` per block. **The block address is not
  stored** (only the pointer to the copy), and it is needed to match palettes between two snapshots.
- `Scene::show` (:1708-1729) copies the snapshot into the presentation `Scene` and returns the camera.

### Where it is captured (EE thread)
- **Scene** at the exit of `hier(v, 0)`: `wotmCaptureScene` (wotm_live.inc:754-776; hook in wotm_overrides.cpp:904). One collection
  `Scene` per view (`captureScene`, :745-749) and `g_sceneSnap[2]` (:750) pending.
- **End of frame** at the entry of `timerWaitForMinUpdateRate` (`waitMinRateByVblanks`, wotm_overrides.cpp:680-707, capture at :691) and
  in the `sceGsSyncV` of the `displayDialog` loop (:611): `wotmCaptureFrame` (wotm_live.inc:802-880) joins the pending scene with the current
  particles (`collectFx`), the flares, the shadow copy (`Particles::shadowRam`, wotm_fx.hpp:578-640, pool of up to 6 buffers of 32 MB),
  the fades and `ScreenPolys`. A frame without `hier(0, 0)` (movie, loading) collects everything there.
- `wotmCaptureFadeStep` (wotm_live.inc:781-800): a `Shell::FadeScreen` step with redraw, without particles.

### How the presentation consumes it (window thread)
- `struct Capture` (wotm_live.inc:382-389): **a single buffer** (`snap` + `fresh`), protected by a mutex. If the capture publishes two
  frames before the presentation picks one up, the first is lost.
- `wotmDrawNative` (:1003): takes `cap.snap` if `fresh` (:1105-1113) and keeps it in `State::snap` (:240, just one). It only redraws with a
  new snapshot, a changing `Shell::FadeScreen` fade, a paused free camera, a changing window, or at ~60/s when idle (`rerender`,
  :1185-1186); on the other presentations it re-presents `rts[cur]`.
- Drawing: `show(view1)` and `drawView` (:1237), `show(snap)` (:1238), `S.scene.draw` (:1295) with a `Camera3D` built from `gc`,
  `drawHp` (:1308), text, fades, popups. `S.cur ^= 1` on every redraw: texId 0 ("previous frame") is the previous redraw.

### What changes to keep two
1. `Snapshot` gets the time: `fields` (field count at the start of the frame, `0x6F8DC0`, which `waitMinRateByVblanks` already reads) and
   `hostT` (host clock at capture, `steady_clock`). It also gets the palette block address in each item (or a map
   block → copy in the snapshot).
2. `Capture` starts keeping the **last two published** (`prev`, `snap`), so the presentation does not lose the pair when it lags.
3. `State` keeps `A` and `B` (the previous and the newest) and a display clock (below). A level change (`levelSig`), `g_loadFreeze`,
   pause and a paused free camera reset `A`.
4. `show` becomes `show(A, B, α)` (or a `Snapshot blend(A, B, α)` that the current `show` consumes, which leaves the drawing untouched).
5. `rerender` applies on every presentation while `α < 1` with interpolation on.

## 2. Identity between two snapshots

| Thing | Stable identity? | Born / disappears / changes | Handling |
|---|---|---|---|
| World object (`items`, `cs` = 0) | `(node, occurrence)`; almost all static | Sphere culling (`outOfView`, :3580) done with B's camera | Interpolate; same matrix in both = copy without math. With the camera lagging behind B, what B culled and is still in the interpolated view disappears at the edge: collect with a margin in the culling (radius × 1.2 or widened planes) when interpolation is on |
| `_cs` object (monsters, cars, debris, power-ups) | `(cs, node, occurrence within the cs)` (DrawItem::cs; occurrence counts instances of the same node in the same `_cs`) | `_cs` reused by another object, teleport (respawn, round change) | Interpolate; translation jump above the threshold (the old one was 150 units) or different `node` = B without blending |
| LOD (`walk` case 2, :3640-3696) | The node changes with the LOD | LOD change = new item; cross-fade (`fade`) already exists | Items only in B: draw B; only in A: do not draw. `fade`: interpolate when the pair exists |
| SWITCH (6), INTERACTIVE_STATE (39), DESTRUCTIBLE (31), CONTROL (8, "ghost" building), invisible ANIM_XFORM/ANIM_SCALE | Child node changes | New state appears in B | B (the new state comes in up to 1 frame early; imperceptible) |
| Bone palette (`pal`) | Block address (`palette`, :3201: `animPkt[+0x68 of the OBJECT]`) | The monster's LOD change changes the block | Interpolate matrix by matrix when the block is the same in both and the item matched; otherwise B |
| Sky (`sky`) | `(node, occurrence)` | Fixed in the world (free camera note) | Interpolate like the world |
| HUD (`hud`), button icons (`overlay`), high-priority list (`hp`, `hpOverlay`: dialog frames, pause) | `hudId`/`cs` | The game places them in front of the camera every frame (`Hud::update`, `AlignCsWithView`) | **Do not interpolate the matrix**: rebase to the interpolated camera, `M' = M_B · V_B⁻¹ · V_I` (V = camera → world), or draw with B's camera. That is what jittered on 2026-10-16 |
| Camera (`cam`, per view) | The view | Cinematic cut, camera change, free camera, screen change | Interpolate with cut detection (section 3) |
| View (`vctx`), split screen (`view1`) | View 0/1 | `viewToggleSplitScreen`, `gUseUnifiedView` (grapple), cinematic | Different `vctx` or `view1` present in only one of the two = cut (all of B) |
| Particles `fx[]` (sprite, texSprite, line, strite, dustTrail, wake...) | System: `FxSystem::base`/`index` (may be reused after it dies). Particle: **the index is not stable**: `particleMove` compacts the list keeping the order (`numParticles--`, `pos[i + dIdx]`, src/common/particle.cpp:1583-1688; the same in `dustMove` :1301 and `wakeAndKiteMove` :1366); new ones go at the end | Born and die every frame | Do not match by index. **Step B back by the velocity**: `p(α) = pos_B − (1 − α)·(vel_B·fields + accel)`, which is `particleMove`'s own integration in reverse (it is the computation the line already uses for the tail, `pos − vel`). Size: `pos.w − (1 − α)·vel.w·fields` **preserving the low bits** of `pos.w` (mask 3 or 0xF: they select the UV). Born in the last step (`age ≤ fields`): B's position. Age (color): B (it is `int16`; changes per frame). System only in A: do not draw |
| Arcs (7/17), chain, sprine, stripe/sweepTrail | Points set by the game (monster nodes, trail) | Trail shifts indices | B at first; later match arcs by index (fixed count) and interpolate |
| Debris (13, `Scene::sprites`) | `Sprite::key = fx·4096 + index` (wotm_native.hpp:541) **does not survive compaction** | Same | Step back by the velocity like the others (store `vel` in the `Sprite` during collection, `collectParticles`) |
| Pedestrians (14) | Same | Same | Step back by the velocity, or B |
| Shadows (`ShadowBlocker`) | `index` in `psBlkLst` + casters by `node` | Blocker turns on/off | Casters matched like the items; `origin` interpolated; `extent`, `shadowCs` from B |
| Parallel lights, fog, per-item point lights | - | Change slowly | B |
| Lens flare (`flares`) | Game list, in GS pixels of B's camera | - | B (runs up to one frame ahead of the scene). Later stage: reproject (unproject with B's camera, project with the interpolated one) |
| Text, `ScreenPolys`, menu strips (`letterbox`), fades (`screenRects`) | 2D list | Built per frame | B. Optional later: fade alpha interpolated between A and B when the rectangle is the same |
| Bars over the enemy (`WOTM_ENEMYBARS`), health icon | Projected by the game with B's camera | - | B (they run slightly ahead); reproject in a later stage |
| Damage numbers (`WOTM_DAMAGEPOP`) | The window's own clock, point in the world | - | They already use the drawing camera; nothing to do |
| Texture 0 (previous frame: video environment map, CLOAK) | `rts[cur ^ 1]` | Becomes the previous presentation (1/3 of a game frame at 180 Hz) | Acceptable for refraction; if some effect accumulates (feedback), keep a copy of the last redraw with α = 1 |
| Big-screen movie, side selection (padsel) | Own texture | - | Nothing |
| Per-node alpha in the menu (`fadeByNode`, reads `node + 0x4C` from live RAM while drawing) | - | - | Keeps reading live (it already does today) |
| Free camera (F5) | Pose in the window | - | Paused: no interpolation (a single snapshot). Running: the game draws with the pose at 60, interpolation smooths it |

## 3. How to interpolate

### Matrices (`DrawItem::m`, shadow casters, palette)
- Component-wise lerp shrinks and skews the mesh during fast turns; it does not work on its own.
- Proposal (generalizes `2fa669e`'s `decomposeRot`, which only accepted uniform scale): in the code's convention, scale enters as
  `mul(scaleM(sx, sy, sz), m)`, that is, **scale per row**. Decompose each matrix into `s_i = |row i|`, `R` = normalized rows
  (orthonormalize with Gram-Schmidt or polar if the error is small), `t` = row 3. Interpolate `s` linearly, `R` by quaternion slerp
  (with sign flip for the short path), `t` linearly. Negative determinant (mirror): flip the sign of one row in both and
  restore it afterwards. If the orthogonality of the normalized rows fails (shear from chained non-uniform scales, debris that
  shrinks: `drawCs` notes that rows can have norm < 1), fall back to component-wise lerp.
- Palette: the 32 matrices of the block are relative to the object (the drawing does `skinVertexPM(pm, skinM, v)`, :2755-2785), so interpolate
  each bone with the same function and `it.m` separately. Cost: 32 slerps per block; half a dozen blocks per frame.
- Shortcut: same matrix in A and B (`memcmp`) = copy; covers almost all of the scenery.

### Camera
- Position and FOV linear; orientation by slerp of the basis quaternion (fwd, up, right) or nlerp of `fwd` and `up` followed by
  re-orthonormalization (the old approach; enough for one frame's turn).
- **Cut** (no blending, show B): position jump above N units (old: 400), `dot(fwd) < 0.7`, FOV changing by more than ~5°,
  different `vctx`, `view1` appearing or disappearing, `fields_B − fields_A` > 4 or a host clock gap > 100 ms (F7 pause, loading,
  long dialog), level change. A real cut in the middle of an interpolation (cinematic starting) falls under the position/direction threshold.
  Even better, flag it in the hook: `doCinema`/`endCinema` and `gameReestablishViews` are already known points (split screen).
- The `Scene`'s `eye_` (back faces, environment map, fog, drawing LOD) becomes the interpolated eye, which `show` already does.
- The particles read `worldToScreenMat` and `worldCtx.eo` from the **shadow copy** (focus, fog, line and strite tail,
  wotm_fx.hpp:293-470, 899-925): when blending, write the interpolated camera matrix there (rebuilt from `GameCamera` with the same
  rule as `readGameCamera`: row 0 = 384·right + 2048·forward, row 1 = −180.48·up + 2048·forward, row 3 = forward). The
  `prevViewMat` (tail) stays B's.

### Time and α
- Timestamp: `fields` (the game's counter at the start of the frame; a 2-field frame gives a gap of 2) + `hostT` at capture.
- Display clock in the window, in fields: `D(now) = now_in_fields − O`. On each new snapshot, `O_target = hostT_B(in fields) −
  fields_B + delay`; `O` follows the target with a filter (e.g. 5% per snapshot) and jumps if the error exceeds 2 fields. `α = (D − fields_A) /
  (fields_B − fields_A)`, clamped to [0, 1]. Without a new snapshot, α reaches 1 and stays there (holds B; **no extrapolation**).
- Why not the old computation `α = (now − t_B) / (t_B − t_A)`: the capture's `hostT` varies with the EE's work in the frame
  (`wotmCaptureFrame` runs at the end of the logic, ~6 ms with variation), and that variation turns into jitter. The filter absorbs it; `fields` gives the right
  gap on 2-field frames.
- `delay` = 1 frame (16.7 ms) + margin (~2 ms) so that a late capture still arrives before α hits 1.

## 4. Cost and latency

- **Memory:** each snapshot has ~1 MB of items (a few thousand × ~280 B) + palettes (2 KB per block). The particles' shadow
  copy is a 32 MB buffer per snapshot (only ~1.3 MB touched: `fx[]` 1.2 MB + tables); keeping A and B holds 2 of the pool of 6, and
  the blend needs one more (the `fx[]` area copied from B and rewritten with the stepped-back positions and the camera matrix). Extra total:
  ~35 MB reserved, ~3 MB touched.
- **Blend CPU:** matching thousands of items by hash and decomposing only those that changed (monsters, cars, debris: hundreds) + ~200
  bone slerps: well under 0.5 ms per presentation. Stepping particles back: a few hundred per frame, negligible; the 1.2 MB `memcpy`
  per presentation at 180 Hz is ~220 MB/s, acceptable (or rewrite only the systems' `pos[]` ranges).
- **The big cost is the redraw:** today the scene is redrawn at 60/s and re-presented on the other presentations. The drawing is raylib immediate mode
  with the transform, skinning and per-vertex lighting on the CPU (`Scene::draw`, :2364; per-triangle loop at :2779+). With interpolation,
  a redraw on every presentation: 3× at 180 Hz, 2.4× at 144 Hz. **There is no measurement of drawing time today** (`PS2X_STATS` counts
  redraws, it does not time them). If drawing exceeds the monitor period (5.5 ms at 180 Hz), vsync drops to half and the screen looks worse than
  today. Therefore: measure first (stage 0) and have a cap (`WOTM_INTERP_HZ`, redraw at most N/s, re-presenting on the others).
  The fundamental way out is the mesh in a GPU buffer with skinning in the shader, which is already the port's direction (rendering at the engine boundary).
- **Latency:** today B shows up as soon as it is captured and stays ~1 frame on screen. With interpolation, B only shows up fully when the next one
  arrives: **+1 game frame (16.7 ms) + the margin**, from input to screen. It goes into the display clock's `delay`; it is the fixed price of the
  method. Extrapolating (no delay) was ruled out: monsters and the camera change direction every frame (hits, grapples).
- **vsync and the host clock ([BOUNDARY.md](../../steps/BOUNDARY.md) (`FRONTEIRA.md`), step 4):** there are two clocks: the game runs on the `EeScheduler`'s virtual vsync (anchored to the
  wall clock, with the `checkpointDue`/`waitForEvent` patches) and the window on the monitor's vsync. Interpolation is precisely what
  absorbs the beat between the two; the display clock's filter follows the game's cadence, whatever it is. When the vblank starts
  firing only from the host clock, the timestamps get cleaner, but nothing in the plan depends on it. Monitor at 60 Hz with the game at 60:
  α stays close to 1 or 0 and only adds the delay; the default should be to **turn itself off** when the presentation rate is ≤ 60 (unless
  the game is at 30, 2-field frames).
- Replay (`--vu1cmp`, `g_localCollect`), the VU1 check and `tools\corpus.ps1` must not change: interpolation always off there.

## 5. Plan in stages

Each stage is enabled by a variable and stays off by default until the owner approves it on screen. New variables go into the table in [NATIVE.md](../../steps/NATIVE.md) (`NATIVO.md`)
(`python tools/wotm_env.py`). Proposed names: `WOTM_INTERP=<lista>` (list; `0` turns it off, `1` = everything, or parts: `cam,obj,pal,fx`),
`WOTM_INTERP_ALPHA=<f>` (fixed α, testing), `WOTM_INTERP_DELAY=<ms>` (delay margin), `WOTM_INTERP_HZ=<n>` (redraw cap),
`WOTM_INTERPLOG=1` (one `[interp]` line per presentation), F4 toggles it live (free today; it was the old interpolation's key).

0. **Measure and timestamp (nothing changes on screen).** `fields`/`hostT` timestamp in `Snapshot`; `Capture` with the last two; `State` with A and
   B; display clock and α computed and only logged. Measure the redraw time (`[native] desenho=x ms` (drawing=x ms) in `PS2X_STATS`) and
   force a redraw on every presentation with `WOTM_INTERP=0,redraw` to see whether 180 Hz fits. **Check:** image identical to today's
   (corpus unchanged); `[interp]` shows α at 0, 1/3, 2/3 on a 180 Hz monitor and a gap of 2 on 2-field frames; drawing time
   per presentation below the period.
1. **Camera + rigid objects + palette + sky + shadows (`cam,obj,pal`).** `blend(A, B, α)` with the per-row decomposition, matching by
   `(cs, node, occurrence)`, palette by block address (store it in the item), HUD/overlay/hp rebased to the interpolated camera,
   cut detection, margin in the sphere culling. Particles still in B. (Camera alone is not good even for looking: the monster the
   camera follows would jitter against it; `cam` alone stays as a diagnostic of the clock.) **Check:** `WOTM_INTERP_ALPHA=1` gives today's image
   bit for bit; `WOTM_INTERP_ALPHA=0` gives A's image. Dedicated test `WOTM_INTERP_SELFTEST=1`: on a fight frame, in the same
   process and with the same pair, draw α = 0, α = 1, `show(A)` and `show(B)` into off-screen textures and log how many pixels differ
   (expected 0, except texId 0 and the big-screen movie, which read live textures). Smoothness: `WOTM_CAMLOG` per presentation and a script
   (`tools/`) that measures the second difference of the position of the camera and of a monster on screen (today: a step every 3 presentations; with
   interpolation: constant). Corpus: `-Check` unchanged (replay off). Owner: AI vs AI fight on a > 60 Hz monitor, fast camera
   turn, grapple (unified view), fight intro, idle HUD.
2. **Particles (`fx`).** Blended shadow copy: `pos[]` stepped back by the velocity, `pos.w` with B's low bits, born in the
   last step in B, `worldToScreenMat`/`eo` from the interpolated camera; debris (`Sprite`) with `vel` stored and the same step-back.
   **Check:** `WOTM_INTERP_ALPHA=1` identical to today; `=0` with the particles near A's position (not exact: those that died in the step
   disappear, drag changes the velocity); Robo-47's shot, Magmo's breath, footstep dust, building debris falling.
3. **2D tied to 3D.** Lens flare, enemy bars and health icon reprojected from B's camera to the interpolated one; fade alpha
   interpolated when the rectangle is the same. **Check:** eye and light flares following the source during a turn; bar over the
   enemy not drifting off.
4. **Split screen and edge cases.** `view1` with its own pair and a cut when the split changes; texId 0 (decide: previous presentation
   or last frame with α = 1); menu (big-screen transitions, padsel); pause (F7), free camera, dialogs, `Shell::FadeScreen`, loading.
   **Check:** `WOTM_MATCH=3,1,1 WOTM_PLAYERS=2` with `WOTM_SHOTS`; menu via the pad script up to the fight.
5. **Default.** Turn it on by default when presentation goes above 60 Hz (or the game drops to 30) and drawing fits; checkbox in the launcher;
   [NATIVE.md](../../steps/NATIVE.md) (`NATIVO.md`) and [NEXT_STEPS.md](../../NEXT_STEPS.md) (`PROXIMOS_PASSOS.md`) updated.

## 6. Risks

- **Drawing time** (the biggest): redrawing per presentation multiplies the CPU cost of immediate-mode drawing. Measure first; cap with
  `WOTM_INTERP_HZ`; the way out is the mesh on the GPU.
- **+1 frame latency**: inherent. Someone playing on a 60 Hz monitor gains nothing; turn itself off in that case.
- **HUD and objects glued to the camera outside the `hud`/`overlay`/`hp` lists** (menu items built in front of the camera, objects
  `AlignCsWithView` with another id) would jitter: list with `WOTM_INTERPLOG` the `_cs`s whose position relative to the camera stays constant between A
  and B and treat them as HUD.
- **Wrong matching** (`_cs` reused in the same frame, occurrence shifted by the culling of a sibling instance): the jump threshold
  holds it, at the cost of one frame without blending.
- **Sphere culling with B's camera**: an object disappears at the edge during turns without the margin.
- **Particles attached to a moving emitter** (`AttachedFlames`, `parentVec`): stepping back by the velocity does not include the
  emitter's movement; check the fire on a walking monster and, if needed, match the system and add the emitter's displacement (`emitVec`, +0x2CA0).
- **Live reads while drawing** (`node + 0x4C` in the menu, `+0x28`, `+0x2C`, textures): they do not change with interpolation, but they make the
  α = 0/1 test not bit-for-bit on some screens.
- **texId 0**: effects that feed back the previous frame would change intensity with more redraws per second.
