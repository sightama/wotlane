# Second opinion on browser performance from gemini-4-neon, 2026-11-23

Lane asked for an outside read of the browser performance work: send the measurements, ask what we missed and what to try, then send
the code it asks for and judge every suggestion against the code and `../2026-11-19/performance-audit.md`. This note records what was
sent, what came back, a verdict on each suggestion and the new ideas worth measuring. Nothing here was built or measured on the
game; the checks below are reads of the code and of the saved profiles.

**In short:** most of Gemini's list is untested hypotheses about code generation (aliasing, per-instruction work, label-per-instruction
switches) that cost little to A/B. Do first: (1) Firefox tiering warm-up, (2) `__restrict__` and macro A/Bs, (3) the big-switch functions.
Its biggest numbers are optimistic. Counts below come from scripts and generated code that are not in the repo, so they cannot be rerun
from a clone.

Verdict words: **valid** (holds up against the code), **invalid** (contradicted by the code or the audit), **already tried** (measured and
dropped, or already on the audit's list), **needs measurement** (plausible, untested; the first cheap test is named).

## What was sent

Model `gemini-4-neon` through the Gemini API, two rounds in one conversation. Lane approved both rounds.

- **Round 1, documents only, no source code (58 KB):** `performance-audit.md`, the rollback synctest `README.md` and `firefox-cost.md`, and
  the online-seamless plan with its measured latency stages and handoff table. Asked for: missed bottlenecks and blind spots, ranked
  speedup ideas for a recompiled PS2 game in WebAssembly, and the code it would want to see.
- **Round 2, the files it asked for (372 KB), with round 1 as history:**
  - Profile summaries: the self-time top rows of the Chrome 1 vs 1 trace, the Firefox 1 vs 1 profile (`tier_default`) and the Firefox
    8-monster profile (`30hz_8m`); a section and function-size table of the built `wotm.wasm` (a script written for this; the binary has
    no name section, so functions appear as indexes).
  - Runtime: `build_web.sh`, the Emscripten block of the runtime's CMake, `ee_vblank_clock.h`, `ps2_runtime_macros.h` (whole),
    `R5900Context`, `lookupFunction`, `dispatchGuestBranch`, `executeVU0Microprogram`, `EeScheduler::checkpointDue`, the top of
    `writeIORegister`, all taken from the runtime with `wotm-runtime.patch` of wip/browser applied.
  - VU0: `native/wotm_rtz_lanes.hpp`, `wotm_vu0_native.cpp/.h`, `tools/vu0native.py`, and the generated `vu0_0000` and `vu0_01a8`.
  - Recompiled game code (generated, never in the repo; it left the machine only for this review): `hierTraceCsList`, `hierCsUpdateAsm`,
    the first 1100 lines of `hierTraverseAsm`, the first 500 lines of `hdSphereVsPoly`, the first 330 of
    `collisTestEnvironmentCollisions`, `animationRunGlobal` and one `evaluateBetweenKeyframes`; plus the repo's `src/common/hier.cpp`
    and `native/wotm_rollback.hpp` and `.inc`.

The key was read from the environment and never printed or written anywhere. The scripts and prompts live in the session scratch
folder, not in the repo.

## Cost

| round | input tokens | output tokens | thinking tokens | total |
|---|---|---|---|---|
| 1 | 17 991 | 8 115 | 19 015 | 45 121 |
| 2 | 193 968 | 14 574 | 22 791 | 231 333 |
| both | 211 959 | 22 689 | 41 806 | 276 454 |

The API returned token counts only, and the price of this model was not looked up. At a deliberately pessimistic assumption of
$15 per million input tokens and $75 per million output tokens with thinking billed as output, the two rounds come to about $8,
under the $20 limit. Treat that figure as an upper-bound guess, not a bill.

## Where the reply was wrong or overreached

- Round 1 assumed `lookupFunction` was a hash map, as the audit's row J says. The code (`ps2_runtime.cpp`) shows a flat table lookup
  with a range check, so row J's wording was wrong. Gemini corrected itself in round 2 after reading the code.
- It reads `kMaxSpinLead` (period minus 1 ms) as the vblank thread spinning 15.6 ms of every frame. That is only the clamp; the lead
  is the worst recent oversleep plus 250 us, decaying (`ee_vblank_clock.h`), and after #155 Windows showed `vbl_late_ms` of 0.1 to 0.6.
  The real spin share on a coarse-timer machine is unmeasured, which is why the idea below is a measurement.
- Its totals are optimistic. Waves 1 and 2 are said to take Windows Firefox from 19 to 25 ms down to 7 to 10 ms and Chrome from
  15 to 17 down to 6 to 8. The gains it lists overlap (the same VU0 and call costs counted in several ranks), and none was measured. For
  VU0 it quotes 15 to 25% of a 1 vs 1 frame where the audit's measured per-call change (-37% Chrome, -41% Firefox) gives 8% and 10%.
- It guesses at things it could not see (the cause of the 8-monster shadow spread, what the 10% Firefox row is) and says so in places.
- Its rollback design in round 2 contradicts itself halfway through ("Wait:") and leaves out the host's own writes to guest RAM (DMA,
  the native layer, stage loads), which a macro-only dirty map would miss.

## Verdicts

### Methodology and blind spots (round 1)

| suggestion | verdict | notes |
|---|---|---|
| No profile from the platform that fails (everything on an M1 Max, the problem is Windows x86-64: register count, 32 KB L1i, memory bandwidth) | valid | the audit says the shares carry over and takes Windows numbers only from `ee_busy_ms`; the hardware argument is generic and unmeasured. Needs a Gecko or Chrome profile from a Windows machine with the same tools |
| The 41 MB wasm was never broken down | valid | measured below ("Facts found while checking"): all code, 957 functions over 10 KB, largest 533 KB |
| Firefox is 2.3x slower than Chrome in `vu0bench` even on plain IEEE arithmetic and nothing explains it | valid, needs measurement | the audit's numbers (Firefox 912 vs Chrome 366 ns RTZ; 520 vs 228 ns timing variant) do show it. First test: run the page bench in Firefox with `javascript.options.wasm_lazy_tiering=false` to see whether lazy tiering still matters in a 3 s bench. Gemini's code-based explanation (per-operation widening to doubles, classification and commit even in the timing variant) is the second thing to check |
| Inclusive-only shares hide self time and emulation cost | partly invalid | the tools print both; the self-time rows were sent in round 2. What is missing is a per-function split of emulation cost (register file, `ctx->pc`, address checks) from game logic, which no profile gives |
| Per-frame counts of calls (VU0, indirect calls, checkpoints) are not known | partly invalid | the runtime comments give 27 to 47 thousand `checkpointDue` calls and about 12 thousand VU0 calls per frame in split screen; 1 vs 1 counts are not recorded |
| Firefox Baseline vs Ion split of the hot functions was not recorded | valid | the eager-tiering run (19% less game thread) is the aggregate; a per-function view is not in the profiles |
| Snapshot write set (how many 16 KB pages change per frame) was never measured | valid | `firefox-cost.md` lists the page-diff snapshot as a candidate without measuring it; the synctest already compares slots, so the count is cheap |
| Inline COP1, COP2 and MMI work is hidden inside the subsystem shares | valid, low value | true of any recompiled code; it is part of the emulation cost row above |

### Ranked ideas, round 1 and round 2 merged

| idea | verdict | notes |
|---|---|---|
| VU0 in 4-lane IEEE `f32x4` (the top 6 programs, or all) | already measured, partly | the audit measured a timing variant of "rounding to nearest" at -37% Chrome and -41% Firefox per call and left it waiting for Lane. Gemini says that variant still widened to doubles and classified every operation, so a real `f32x4` version could go further; the docs do not say what the variant removed, so that is unchecked. Its frame-share estimate (15 to 25%) is about double what the audit derives (8 to 10%) from the measured per-call change. First test: hand-write `vu0_01a8` in `f32x4` in the page bench |
| Drop VU0 flag work for programs that never read flags, no matter what the interpreter would have left in the registers | needs measurement | the audit's item B measured the upper bound (-22% Chrome, -15% Firefox per call, results kept) and then shipped only the MAC drop, which was noise, because the rest "can't go without changing the VU state". Round 2 adds a fact that changes that: see "Facts", the EE never reads VU0 status, MAC or clip, so the exit flag state is dead for programs with no in-program flag read |
| `__restrict__` on the VU0 functions (`VU1State &s, uint8_t *d`: a `uint8_t` store through `d` may alias `s.vf`, so the compiler reloads registers after each `sq`), 16-byte loads and stores instead of four 4-byte copies | needs measurement | zero determinism risk, a few lines in `tools/vu0native.py`, testable in the existing page bench in minutes. Not in the audit |
| `__restrict__` on the recompiled functions' `rdram` and `ctx`, and the same aliasing argument for every guest store (a `uint8_t` store may alias `ctx->r[]`, so GPRs are spilled around it) | needs measurement | plausible by the C++ rules; bit-identical output. A/B on two or three generated files (a sed on the signature) before touching the recompiler. Not in the audit |
| `Ps2SetGprLow64` does a 128-bit read-modify-write on `ctx->r[reg]` for every integer result (`_mm_move_sd` on a loaded `__m128i`) | needs measurement | confirmed in `ps2_runtime_macros.h`. LLVM may already narrow it to a 64-bit store; look at one function's wasm before changing the header. Bit-identical |
| `ctx->pc = ...` stored before every instruction (1444 times in `hierTraverseAsm`) | needs measurement | confirmed in the generated code. Needed at calls, checkpoints and the out-of-line load and store paths; whether the rest costs anything depends on what LLVM already removes. Keep at every point the runtime reads it |
| `READ*` and `WRITE*` test scratchpad, then special address, then RAM range, with `ctx` passed to out-of-line calls on the cold arms | needs measurement | confirmed in the header. Putting the RAM check first and moving the cold arms into a noinline helper is bit-identical and header-only. Gain unknown |
| Remove `drainCompletedDmacHandlersIfPending` from the scratchpad `WRITE32` path | needs measurement, not zero risk | it is one atomic load per 32-bit scratchpad store plus a never-taken call. It was added on purpose (the native 3 and 4 player work, 2026-10-26), most likely for handler timing after a game store, so removing it can change when DMAC handlers run. Measure the cost first; do not remove it on the argument that a scratchpad store cannot complete a DMA |
| Single-branch `ps2_fpu_fix`; fix outputs only, not inputs | partly valid | the single-branch form is bit-identical and safe to test. Dropping the input fix changes results when a denormal or NaN enters from memory, so it is not "zero risk" and needs the determinism checks |
| Constant scratchpad addresses are emitted as out-of-line `runtime->Load32` and `Store32` calls ("MMIO") | valid, small | 63 such calls in 10 recompiled functions (28 in `hierTraverseAsm`, 4 in `hierCsUpdateAsm`; only 30 of them carry the "MMIO" comment). Direct scratchpad access is bit-identical. Gain probably under 1%; the fix is in the recompiler's constant-address path |
| All-PC `switch (ctx->pc)` makes huge, unoptimizable functions | needs measurement | confirmed for 46 functions with 200 or more case labels (one per instruction in `collisTestEnvironmentCollisions`: 1382 labels for about 1394 instructions). Why the recompiler emits them is not known; resume points are needed after calls and at checkpoints, not at every instruction. See the top ideas |
| Direct `JAL` fast path instead of `dispatchGuestBranch` + `hasFunction` + `lookupFunction` + `pushDispatchPc`; fuse the two range checks | valid | the code does two range checks and two table loads per call plus the ring-buffer write. The seam rows add up to 3.7% (Chrome) and 3.8% (Firefox) self time, so the ceiling is a couple of percent. Hookability (`replaceFunction`, the call trace, the coverage counter, the unwind sequence) must stay |
| Inline fast path for `checkpointDue` | valid, small | the function is already free of clocks and queues; what remains is the cycle adds, the free-spin check, the profile check and the atomic load, 1.3% self in Chrome. A poll stride is not safe to assume: it can move when interrupts reach the guest |
| Re-open J (hash map, out-of-line checkpoint, vector under a lock in `writeIORegister`, register copies) | partly invalid, partly valid | there is no hash map (see above), `writeIORegister` is 0.3 to 0.5% self, and the register-copy part was measured (wrapper 2.6% to 2.5%, no gain). The call and checkpoint part was never tried, which is the valid row above |
| Zero-copy VU0 state, direct call per `vcallms`, drop the per-call static guards and timer | needs measurement | the copy trim (a register mask) was measured and dropped; `R5900Context` already holds the VU0 registers, so skipping the copy entirely is untested. The wrapper is still 2.1% self in Chrome, so something else in it costs; the static guards, `Vu0Timer` and the lookup are candidates, nothing is measured |
| Skip `pktAddVu1ObjAsm` and `psObjectCull` on every build, not only in hidden redos | already on the audit list | audit item G, unscheduled ("small to skip, medium to prove"); `firefox-cost.md` measured that the state spread stays in a few buffer blocks. Valid, worth about 3.5% |
| Find why skipping `hierShadow` spreads to 21 blocks with 8 monsters (trace the first divergence) | needs measurement | audit item H stops at "the pass writes state the game reads". `WOTM_SYNCTEST_TRACE` on that run names the first write; Gemini's guess about the cause is unsupported. Up to 10% with 8 monsters if the side effect is small |
| In-app Firefox tiering warm-up: call the hot functions many times at boot with an early return so SpiderMonkey tiers them up before frame 0 | needs measurement | not in the audit (row C ends at "a tip or a Mozilla bug"). Zero determinism risk. See the top ideas |
| Shrink the wasm: `-Os` or `-O2` for cold code, ThinLTO, `-mnontrapping-fptoint`, `wasm-opt` | needs measurement | the build is `-O3` with no LTO and the link line carries `-O3` too (see "Facts"), so wasm-opt does run. Whether non-trapping float conversions are on was not checked. Cold-code `-Os` only pays if it shrinks the 957 big functions |
| Static `rdram` base so loads fold the base into the offset | needs measurement | how `rdram` is allocated was not in what was sent. Plausible, medium effort, bit-identical |
| Stay on wasm32, do not use Memory64 | valid | nothing here uses it |
| Native `f32x4` rewrites of `hierCsUpdateAsm`, `hierTraverseAsm`, `evaluateBetweenKeyframes` / `animCurveEval`, `hdSphereVsPoly` | needs measurement, large | the hook layer exists for this, but each is a game rewrite whose bits must match what runs today or be re-verified. `hierTraverseAsm` is 7.2% self, the animation pair about 1.5%, the others under 1% each. Start with the smallest (`evaluateBetweenKeyframes`) only to learn the workflow cost |
| Move the disc chunk SHA-256 off the game thread (SubtleCrypto or a wasm SHA-256) | valid, already listed | audit item F. New evidence: the 8-monster Firefox profile (`30hz_8m`, taken after the fight's first 10 s) has `block` 12.6% and `rotr` 6.2% self (18.8% of the busy samples) in JS SHA-256. Whether that time falls after the first 10 s was not verified from the profile file, and the sample interval is not 1 ms, so the seconds are unknown; the audit's 2.8 s in the first 6 s may be only part of it. Online it is small because the lobby prefetch fills the cache first |
| Move the sound mix to the audio thread | already tried | audit item K, unscheduled |
| Off-screen civilians, world transform cache, GPU renderer | already tried | Gemini agrees with the audit's rulings on E, I and D (on E it disagrees with the reasoning but agrees to leave it) |
| Page-diff or dirty-page rollback snapshot | needs measurement | `firefox-cost.md` already lists it. First count how many 16 KB pages change per frame and per fight from the synctest's slots. Rollback is paused, so this waits |
| The unnamed 10% Firefox self row (`0x1967f350c` in `tier_default`) | needs measurement, probably known | its callers are unsymbolicated native frames, and its share matches the "10 to 12%" of Firefox wake-ups from the vsync wait that `firefox-cost.md` counts as busy and the audit leaves out. Gemini noted the address sits in the macOS shared library range, which fits a native wait, but that is inference. Symbolicating it would settle it |
| `_emscripten_get_now` 0.5% self in Chrome: find the caller | needs measurement | trivial to look up in the trace; the host profiler and the runtime's timers are the first suspects |
| `EeVBlankClock::spinLead` may burn a core on a small Windows machine | needs measurement | the spin exists (`WOTM_VBLANK_SPIN=0` turns it off, confirmed in the header); the claim that it is 15.6 ms of every frame is an overreading. A/B `ee_busy_ms` and frame rate with the spin off on a 2 to 4 core Windows machine |
| Check whether `PS2X_FAST_LINK` weakens the link | probably not | `CMakeLists.txt` has no match; the flag is read in `cmake/ReleaseMode.cmake`, where it skips the extra release options on Emscripten, and the Release link line gets `-O3` from the default Release flags (below). Gemini only asked to verify this, so "not a problem" is the finding |

## Facts found while checking

- **The wasm is code.** `wotm.wasm` is 40 874 494 bytes: 40 076 007 in the code section (10 342 functions), 744 778 in the data section.
  957 functions are over 10 KB (26.9 MB together), 97 over 50 KB (10.0 MB), 4109 over 1 KB. The largest bodies are 533 279, 533 244,
  411 501, 358 797 and 300 688 bytes. No name section, so which functions these are is not mapped (a `--profiling-funcs` build has it).
- **Recompiled functions with a case label per instruction.** Counting `case 0x` labels in the translated functions (5382 files): 46 have
  200 or more, 169 have 50 or more, 644 have 10 or more. The largest by labels: `collisTestEnvironmentCollisions` (1382),
  `initDynamics__7Monster` (1111), `update__13PlantBossHead` (1079), `takeHit__7Monster` (894), `update__13StateBatSwipe` (875). The big
  wasm functions are probably these (the biggest sources are about 400 KB), but that mapping is a guess until a names build is sized.
- **The EE never reads the VU0 status, MAC or clip registers.** Every `cfc2` in the recompiled game reads `$vi0`, `$vi2`, `$vi5` or
  `$vi14`; none reads `$vi16` to `$vi18`, and no recompiled file touches `vu0_status`, `vu0_mac_flags` or `vu0_clip_flags`. A microprogram
  call copies the flag registers in from the context and back out at exit (`copyVu0ContextToState`), so a program that read a flag before writing it would see the previous call's value; that is not an issue for the EE, which never reads them. Only `fmand`, `fsand` and similar inside a microprogram
  see them. In the generated code `vu0_01a8`, `0a00`, `0a80`, `0b00` and `0b48` have no such read (`0000`, `00c8`, `0430`, `04d0` and `0748` have a few),
  while `vu0_01a8` still carries flag bookkeeping on 139 lines. The `native/` files and `tools/vu0native.py` of wip/browser have no reader of the context's flag fields either (the runtime patch itself was not searched).
- **`lookupFunction` is a flat table.** `dispatchGuestBranch` checks `hasFunction`, then `lookupFunction` repeats the range check and
  the load and calls `pushDispatchPc` (a ring-buffer write), after storing `ctx->pc`, a live-context pointer and a coverage check.
- **Build flags.** Compile: `-pthread -fwasm-exceptions -msimd128 -msse4.1 -O3 -DNDEBUG`, game code in about 168 unity translation units. Link: the
  same compile flags (so `-O3`) plus `-sJSPI -sWASMFS -sINITIAL_MEMORY=512MB -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2GB`. No LTO.
- **Scratchpad stores through the macros call `drainCompletedDmacHandlersIfPending`**, an inline atomic load that calls out when a DMAC
  handler is pending. It arrived with the 3 and 4 player native work on 2026-10-26.

## Top new ideas worth measuring

Each is new relative to the audit and cheap to test first. None changes what the game computes unless noted.

1. **Firefox tiering warm-up in the page or runtime (target: the 19% of Firefox game thread that eager tiering gave, without a setting).**
   Early-return guard at the top of the 30 to 50 hottest recompiled functions, a flag set during the lobby or load, and a loop that
   calls each of them enough times to exhaust SpiderMonkey's per-function budget so Ion compiles them before frame 0. Zero determinism
   risk (the flag is off before any game frame). First test: guard the ten hottest functions by self time, warm them in
   `profile_firefox.mjs` with default prefs and compare against `wasm_lazy_tiering=false`. Open questions to settle in that test: that the
   budget is charged before the early return, that Ion finishes for 400 KB functions in acceptable time, and that the guard must sit before
   the generated `switch (ctx->pc)`, which means a generator or build-step edit.
2. **Aliasing and codegen hygiene, tested as A/Bs before any recompiler change.** (a) `__restrict__` on `s` and `d` in the generated VU0
   functions, then 16-byte moves in `vu0native.py`, measured in the existing page bench in Chrome and Firefox; (b) `__restrict__` on
   `rdram` and `ctx` in two or three hot generated files by editing the signature, plus the header-only macro changes (RAM check first, cold
   arms out of line, 64-bit GPR writes, single-branch `ps2_fpu_fix`), measured by wasm size and Chrome and Firefox profiles. All of it
   is bit-identical by construction, and the desync matrix and `determinism_check.py` confirm it. Whether LLVM already does any of this is
   the open question; one function's wasm text answers it.
3. **Size and shape of the 40 MB: trim the all-instruction `switch (ctx->pc)` in the 46 largest-label functions.** Find out why the
   recompiler gives those functions a label per instruction, restrict case labels to real resume points (after calls, checkpoint
   targets, jump table targets), and compare wasm size and speed on `collisTestEnvironmentCollisions` and `hierTraverseAsm` alone first
   (hand-edit the two generated files). It targets Firefox's lazy tiering and the Windows instruction cache together. Risk: a missed resume
   point breaks the scheduler's resume by pc, so it needs the synctest, the desync matrix and a long fight.

Expected gains are guesses until measured: idea 1 up to the 19% eager tiering gave on Firefox only; idea 2 unknown (zero if LLVM already
does it); idea 3 unknown, mostly Firefox and Windows.

Also worth a quick look, smaller or less certain:

- Hand-written `f32x4` `vu0_01a8` in the page bench (Gemini's 2 hour test), which also checks the Firefox gap.
- The EE never reading VU0 flags: reopens item B's upper bound (about 5% of a Chrome frame, 4% of a Firefox one) for programs with no
  in-program flag read, once `vu0bench` stops comparing flag registers for them. Search `native/` for readers first.
- The vblank spin on small Windows machines (`WOTM_VBLANK_SPIN=0` A/B on `ee_busy_ms`).
- Call-path seams: one table lookup in `dispatchGuestBranch`, no ring-buffer write, an inlined `checkpointDue`; a couple of percent.
- The 8-monster `hierShadow` first-divergence trace.
- A Windows profile with the same tools, to see how much of the Mac's shares carry over.

Gemini suggestions not given their own row, all small or conditional on the ideas above: padding `rdram` to drop the contiguous-range
helpers, a release no-op for `ps2TraceGuestWrite`, dropping the `in_delay_slot` and `branch_pc` stores and the `add` overflow traps, caching
GPRs in locals, the per-call `g_wotmLiveCtx` store, a direct flag classifier for the few flag-live VU0 operations, a prebuilt VU0 table in place
of `wotmVu0NativeLookup`, `Writer::bytes` as a plain copy, why Firefox's snapshot copy is 2.2x slower than Chrome's, per-monster skeleton
workers, slimmer `wotmCaptureScene` and particle copies, whether the second `hier` pass is needed in one view, and the 4 player online workload
shape. None was checked.

## Corrections this suggests for the audit

- Row J says `lookupFunction` is a hash map per indirect call; it is a flat table with a redundant second check and a history write.
- Row F says first-read hashing happens in the first seconds; the 8-monster profile has 18.8% of its busy samples in JS SHA-256 well into the fight (timing not verified).
- Row B's argument that the flags "can't go without changing the VU state" holds only if something reads that state; nothing in the
  recompiled game does (see "Facts").
