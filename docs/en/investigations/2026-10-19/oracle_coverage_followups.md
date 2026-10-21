*English translation of [docs/investigacoes/2026-10-19/pendencias_oracle_cobertura.md](../../../investigacoes/2026-10-19/pendencias_oracle_cobertura.md). The original is authoritative; this copy may lag behind it.*

# Open items off the critical path: PR #277, oracle with calls, coverage with gameplay (2026-10-19)

Read-only investigation (nothing compiled, nothing published on GitHub). Open items from [NEXT_STEPS.md](../../NEXT_STEPS.md) (`docs/PROXIMOS_PASSOS.md`), "Pendências fora do caminho crítico" (open items off the critical path).
Drafts (scripts and lists) in the session's scratch folder, outside the repo.

## 1. PR #277 in PS2Recomp (`SQRT.S`/`RSQRT.S`)

### Status today
- `ran-j/PS2Recomp#277`, author `yanm1103`, opened on 2026-10-09, one commit (`9a7ca59`), no update since.
- **No review, no comment, no CI** (the repository does not run checks on PRs: empty `statusCheckRollup`).
- **`mergeable: CONFLICTING` (`DIRTY`)**: `main` has moved on. The conflict comes from **#272** ("Emulate PS2 float semantics for EE FPU results and DIV.S/RSQRT.S",
  by `drakolordx7`, merged on 2026-10-14 13:47), which touched the same lines of `fpu_translator.cpp`.
- What #272 already solved: `RSQRT.S` now comes out as `FPU_RSQRT_S(fs, ft)` = `ps2_frsqrt`, which computes `fs / sqrt(|ft|)` with PS2 semantics
  (no Inf/NaN, zero `ft` gives ±max with the sign of `fs`). **Half of #277 became redundant.**
- What is still wrong on `main` (`eb8688b`): `COP1_S_SQRT` still emits `FPU_SQRT_S(ctx->f[fs])`. On the R5900 the `fs` field of `sqrt.s` is 0, so
  the root comes from `$f0`. #272's `ps2_fsqrt` already takes `|x|` on the slow path, but the operand is still the wrong one. **The 108 functions with `sqrt.s` are still wrong upstream.**
- Useful history for the reviewer: **#218** ("Fix R5900 SQRT.S source register", `GTTeancum`, X-Men Legends) proposed the same `fs`→`ft` swap
  and was closed without merging on 2026-09-27, without comment.
- Related open ones that touch the same subject, VU0 only: #198 (VDIV/VSQRT/VRSQRT), #317 (FDIV rules in VU0), #312 (VU1 EFU).

### Does the project depend on it?
**No.** The recompiled code the runtime uses already comes out right:
- The translator that generates the game's C++ is the WSL clone `~/wotm-recomp/PS2Recomp` (base `2c5fbb9`, branch `main`) with `fpu_translator.cpp`
  **modified and uncommitted**: `SQRT.S` reads `ft` with `fabsf` and `RSQRT.S` emits `FPU_RSQRT_S(fs, ft)`. The C++ generated in
  `wotm-recomp-win\PS2Recomp\ps2xRuntime\src\runner` confirms it: 147 `FPU_SQRT_S(fabsf(ctx->f[ft]))` and 134 `FPU_RSQRT_S(fs, ft)`, none in the old form.
- The two-argument `FPU_RSQRT_S(a, b)` and the PS2 float semantics (`ps2_fpu_fix`, `ps2_fpu_div`, `ps2_fpu_rsqrt`, truncating `cvt.w.s`)
  come from `tools/runtime_patch/wotm-runtime.patch` (the `ps2_runtime_macros.h` hunk). The patch's `FPU_SQRT_S` is still `sqrtf(a)`; the `fabsf` comes from the translator.
- The Windows clone (`wotm-recomp-win\PS2Recomp`) has the translator **without** the fix, but the recompiler is not built there (`PS2X_BUILD_RECOMP=OFF`), so it does not matter.
- The submodule `third_party/PS2Recomp` (fork `yanm1103`, branch `wotm`, `31bdfe1`, base of `wotm-fmv.patch`) has the PR's branch, with `RSQRT.S` inline (`fs / sqrtf(fabsf(ft))`).
- [tools/recomp_oracle.md](../../tools/recomp_oracle.md) (`tools/recomp_oracle/README.md`) describes the fix in prose (step 2) and does not depend on the PR.

Risks (no urgency):
1. The translator fix only exists as an uncommitted modification in the WSL clone. The `~/wotm-recomp/ps2recomp_fpu_sqrt.patch` kept next to it
   **is out of date**: it still has the inline `RSQRT.S`, not `FPU_RSQRT_S`. Whoever recreates the clone from it generates code that does not use the runtime patch's `ps2_fpu_rsqrt`.
2. Moving the runtime base up to the current `main` (≥ `eb8688b`) **will conflict** in the `ps2_runtime_macros.h` hunk of `wotm-runtime.patch`: #272 defines
   `FPU_ADD_S`…`FPU_RSQRT_S` with PS2 semantics. In that rebase the right thing is to drop our hunk (except the `cvt.w.s` truncation/saturation, which is #215, still open)
   and keep only the `SQRT.S` `fs`→`ft` swap in the translator until #277 goes in.

### What is missing to close it
1. Rebase `fix/fpu-sqrt-rsqrt-operands` onto `main` (`eb8688b`), keeping **only `SQRT.S`**: `FPU_SQRT_S(ctx->f[ft])`. The `fabsf` becomes optional because `ps2_fsqrt` already handles negatives.
   For the `RSQRT.S` part, keep #272's version.
2. Test: keep the `sqrt.s $f2,$f2` case (`0x46020084`: do not read `f[0]`) and remove the expectation of the inline `RSQRT.S`, which #272 already covers in `ps2_float_semantics_tests.cpp`.
3. New title and description ("Fix SQRT.S source operand: R5900 reads ft"), citing #272 (which fixed `RSQRT.S`) and #218 (same fix, closed without merging).
   Run `ps2x_tests` first (#272 reports 444/444).
4. Wait for the maintainer. They have merged little recently (#274 on 10-05, #272 on 10-14) and there are 168 open issues/PRs. On the project's side nothing is blocked.

## 2. Oracle for functions with calls

### Which are the 35
I redid `oracle_all.py`'s selection (equivalent, asm with VU0 or special FPU, a signature that `parse_sig` understands, `DirectCall`/`IndirectCall` in the generated C++)
with the `config/status.csv` from 2026-10-09 (`97fcda8`, 329 equivalents). It gave **60 candidates (2 with an unsupported signature), 35 non-leaf**, the README's numbers.
**Today** (1011 equivalents) there are **233 candidates (44 with an unsupported signature) and 173 non-leaf**: AiSeek 14, MovementStates 13, Monster 13, AiReflex 13, particle 11, Cameras 10, Weapon 9, HomingBug 9...

List of the 35, with the callees of each. The state comes from `status.csv`, and `+` marks a callee that also calls other functions. `jalr` is the number of indirect
calls (virtual or through a pointer). Spec: `p` pointer/this, `i` integer, `f` float.

| TU | function | spec | jalr | callees |
|---|---|---|---|---|
| AiAction | `deactivate__13AiActionTupleR2Ai` | pp | 1 | `timerGetFieldCount__Fv` hw |
| AiReflex | `getEntryRelevance__16AiFireProjectileR2Ai` | pp | 0 | `getBestTarget__16AiFireProjectileR2AiRf` matched+; `getMaxLevel__12StaminaMeter` matched |
| AiReflex | `getExitRelevance__16AiFireProjectileR2Ai` | pp | 0 | `overPit__2AiR7Monster` matched; `getTargetRelevance__16AiFireProjectileR2AiP7Monster` equivalent+ |
| AiReflex | `getExitRelevance__15AiGrappleReflexR2Ai` | pp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm; `pinningAi__2Ai` matched |
| AiReflex | `getEntryRelevance__13AiBlockReflexR2Ai` | pp | 2 | `getInfo__9EnemyInfoR7MonsterT1` asm; `getReflexDelay__2Ai` matched+; `getClosestStillApproach__2AiR13DbInteractivef` asm+; `getFovRelevance__2Aiff` equivalent; `getClosestThrownPickup__12LevelPickupsR8_fvectorf` equivalent |
| AiReflex | `getExitRelevance__13AiBlockReflexR2Ai` | pp | 1 | `getReflexDelay__2Ai` matched+ |
| AiReflex | `getExitRelevance__15AiDisposeReflexR2Ai` | pp | 0 | `isHolding__7Monster` matched |
| AiReflex | `enterAction__15AiDisposeReflexR2Ai` | pp | 0 | `target__11AiNavigatorR8_fvectorf` matched; `getThrow__10AiPadClipsv` matched; `rewind__17GamePadClipPlayer` equivalent; `getReflexDelay__2Ai` matched+; `getFieldsSinceEval__13AiActionTuple` matched+ |
| AiReflex | `getEntryRelevance__13AiSmashReflexR2Ai` | pp | 0 | `isHolding__7Monster` matched; `hasEnough__12StaminaMeterf` matched+ |
| AiReflex | `getExitRelevance__13AiThrowPickupR2Ai` | pp | 0 | `pinningAi__2Ai` matched |
| AiReflex | `getBestTarget__11AiRamAttackR2AiRf` | ppp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm; `inFov__9EnemyInfof` asm; `inLos__9EnemyInfo` matched; `getMaxLevel__11HealthMeter` asm; `getCommonGroundProbability__2AiR7Monster` asm+ |
| AiReflex | `getExitRelevance__11AiRamAttackR2Ai` | pp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm; `inFov__9EnemyInfof` asm; `inLos__9EnemyInfo` matched; `pinningAi__2Ai` matched |
| AiReflex | `getExitRelevance__15AiSpecialAttackR2Ai` | pp | 1 | `getInfo__9EnemyInfoR7MonsterT1` asm; `block__2Ai` matched; `isSpecialAvailable__C7Monster` equivalent; `timerGetFieldCount__Fv` hw; `pinningAi__2Ai` matched |
| AiReflex | `getBestTarget__14AiDetonateHeadR2AiRf` | ppp | 10 | `mathfClosestApproach__FR8_fvectorN30` equivalent |
| AiSeek | `getExitRelevance__12AiSeekHealthR2Ai` | pp | 2 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__13AiSeekStaminaR2Ai` | pp | 0 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__13AiSeekSpecialR2Ai` | pp | 0 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__11AiSeekCloakR2Ai` | pp | 0 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__12AiSeekPickupR2Ai` | pp | 0 | `getClosestMonsterWithLos__7Monsterf` equivalent |
| AiSeek | `enterAction__12AiSeekPickupR2Ai` | pp | 1 | `setFocus__2AiP13DbInteractive` matched; `arrive__11AiNavigatorR8_fvectorfff` matched+; `getReflexDelay__2Ai` matched+ |
| AiSeek | `updateAction__12AiSeekPickupR2Ai` | pp | 1 | `__vc__8PadFlagsi` matched; `grab__2Ai` matched |
| AiSeek | `getExitRelevance__13AiSeekMonsterR2Ai` | pp | 0 | `overPit__2AiR7Monster` matched |
| AiSeek | `getExitRelevance__12AiDodgeThrowR2Ai` | pp | 3 | `isHolding__7Monster` matched; `inFlight__2AiR13DbInteractive` asm+ |
| AiSeek | `updateAction__12AiDodgeThrowR2Ai` | pp | 1 | `getInfo__9EnemyInfoR7MonsterT1` asm; `targetPin__2Ai` matched; `getClosestStillApproach__2AiR13DbInteractivef` asm+; `mathfVectorCrossUp__FP8_fvectorT0` equivalent; `tag__11AiNavigatorR8_fvectorf` equivalent+ |
| AiSeek | `getEntryRelevance__10AiDodgeRamR2Ai` | pp | 1 | `getInfo__9EnemyInfoR7MonsterT1` asm; `mathfClosestApproach__FR8_fvectorN30fRf` equivalent; `getFovRelevance__2Aiff` equivalent |
| AiSeek | `getExitRelevance__10AiDodgeRamR2Ai` | pp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm |
| AiSeek | `enterAction__10AiDodgeRamR2Ai` | pp | 0 | `setFocus__2AiP13DbInteractive` matched; `getReflexDelay__2Ai` matched+; `mathfVectorCrossUp__FP8_fvectorT0` equivalent; `tag__11AiNavigatorR8_fvectorf` equivalent+ |
| AiSeek | `enterAction__12AiDodgeStompR2Ai` | pp | 0 | `setFocus__2AiP13DbInteractive` matched; `getReflexDelay__2Ai` matched+; `getInfo__9EnemyInfoR7MonsterT1` asm; `tag__11AiNavigatorR8_fvectorf` equivalent+ |
| AiNavigator | `targetPin__11AiNavigatorR8_fvector` | pp | 0 | `orientTo__11AiNavigatorR8_fvectorf` equivalent |
| HeliVehicle | `updatePosition__11HeliVehicle` | p | 0 | `timerGetUpdateRate__Fv` hw; `resolveForcesEuler__9PointMassf` equivalent |
| Monster | `update__7Monster` | p | 1 | `timerGetFieldsLastFrame__Fv` hw; `clearInputs__7GamePad` matched; `updateDeathSequence__7Monster` matched+; `creditFull__11HealthMeter` matched+; `update__12StaminaMeteri` matched+; `creditFull__12StaminaMeter` matched+; `playerUpdateInputs__7Monster` matched+; `computeHighlight__12LevelPickupsR7Monster` equivalent+; `updateInputs__2Ai` asm+; `interpretInputs__8PadFlagsR7GamePad` asm+; `__vc__8PadFlagsi` matched; `updateOnFire__7Monster` matched+; `updateBeingShocked__7Monster` matched+; `updateAirLegOverride__7Monster` equivalent+; `smoothEasyIn__Fffff` equivalent; `setCloakOff__7Monster` matched+; `TogglePOV__7Camerasi` matched+; `updateReticle__7Monster` asm+; `updateLookAt__7Monster` asm+; `updateBoostAndRage__7Monster` equivalent+; `updateBoundingSphere__7Monster` asm+; `updateAnimContacts__7Monsterb` asm+; `updatePowerUpGlow__7Monster` asm+; `timerGetFieldCount__Fv` hw; `updateMonsterSound__12MonsterSound` matched+ |
| Monster | `takeHit__7MonsterP8_fvectorfi` | ppfi | 3 | `getInteractive__12Interactivesi` matched+; `takeDamage__7MonsterfbP7Monster` equivalent+; `isBlocking__7Monster` equivalent; `isBlockable__10StateBlockR8HitEvent` equivalent; `killPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori` equivalent+; `particleCreateFx__FP8_fvectorifPA3_ff` equivalent+; `playDestructibleSound__23LevelObjectSoundManagerUiP8_fvector` equivalent+; `transitionOK__11StateRecoil` asm; `enterNewState__7MonsterP12MonsterState` equivalent+; `knockBack__7MonsterR8_fvectorff` equivalent+; `transitionOK__12StateShocked` matched; `isHoldingLarge__7Monster` equivalent; `enterSubState__13StateGrappledQ213StateGrappled8SubState` matched+; `takeAdditiveRecoil__7MonsterR8_fvectorf` equivalent+; `inputSetActuator__FiP12ActuatorData` matched+; `transitionOK__12StateStunned` asm; `getStaminaGain__7Monster` matched; `credit__12StaminaMeterf` equivalent |
| MonsterInit | `init__7Monster` | p | 0 | `memset` asm; `dropPickup__7Monster` equivalent+; `dropPickupImpaler__7Monster` matched+; `particleKillFx__FRi` matched; `hierSetCsDrawMe__FP3_csUc` matched; `mathfUnitMatrix__FPA3_f` equivalent; `gameGetStartPoint__7TheGameP7Monster` equivalent+; `getClosestPath__9AiPathNetR8_fvectorUc` asm+; `init__8PadFlagsP7Monster` asm+; `init__15MonsterDynamics` equivalent+; `init__11AnimContactR11_animHandle` asm+; `Init__10FireBreath` matched+; `reset__11HealthMeter` matched; `reset__12StaminaMeter` matched; `initMonsterSound__12MonsterSoundP7Monster` equivalent+; `animationResetCharacter__FR11_animHandle` matched+; `rampIn__9AnimBlendf` matched+; `setSpeed__9AnimBlendf` matched; `setPercent__9AnimBlendf` matched+; `enterNewState__7MonsterP12MonsterState` equivalent+; `setCloakOff__7Monster` matched+ |
| StickShaker | `update__11StickShaker` | p | 0 | `timerGetFieldsLastFrame__Fv` hw; `newHit__10HitHistoryib` equivalent+; `inputSetActuator__FiP12ActuatorData` matched+ |
| hier | `hierSetCameraLosPoint__FP8_fvectorii` | pii | 0 | `viewGetTrans__Fi` matched |

Summary: 99 distinct callees (44 matched, 33 equivalent, 19 asm, 3 hw: the `timerGet*` ones, which only read globals), 45 of them leaves. 21 of the 35 have 1 or 2 callees;
3 are big (`Monster::update` 25, `Monster::init` 21, `Monster::takeHit` 18). 13 have `jalr`.

### How the generated code makes calls
Every `jal` comes out as `runtime->dispatchGuestBranch(rdram, ctx, alvo, origem, volta, GuestBranchKind::DirectCall, "JAL")` (`alvo` = target, `origem` = origin, `volta` = return); `jalr` comes out as `IndirectCall`.
The dispatch looks up the target in `g_ps2RecompiledFunctionTable` (indexed by address; the generated `register_functions.cpp` also fills in the resume points in the middle of functions),
runs the function and checks whether `ctx->pc` came back to `volta`. The current harness defines its own empty table from 0 to 16 MB, which is why it rejects the non-leaf ones.
A false `hasFunction` falls into `reportMissingFunction` (the `SkipCallDebug` policy skips the call).

### Proposed design
The model already exists in `tools/difftest.py` (unicorn): every callee outside the function is **intercepted and recorded** (name and arguments, in order), returns a
deterministic value (`RETS`, `NARGS`, `C_ARGS` for libm, check of the callee-saved registers). The oracle brings that model to the recompiled C++, in three stages:

**Stage 1: intercept and record (covers the 35).**
- The generator reads the `DirectCall` targets from both `.cpp`s (retail from `output_fix`, NM from `output_nm_all`) and translates address → name through each ELF's symbol table
  (`game.elf` and `nm_all.elf`/`nm_all.syms`). The NM addresses differ from the retail ones; matching by name solves it.
- The harness fills **every** empty slot of the table (the 4 M) with an "unknown target" recorder. At the addresses of known callees it puts a named recorder.
  There are two tables, one with the retail addresses and another with the NM ones, swapped between the two runs (a `memcpy` of the table or of a vector of changed slots).
- The recorder logs `(name, a0..a3/t0..t3, f12..f19)`, limited to the number of arguments the mangled signature gives (reuse the difftest's `spec_from_mangled`/`C_ARGS`),
  puts `v0`/`f0` from a pseudo-random stream seeded by (state, call index), the same on both sides, and does `ctx->pc = ra`.
  A `jalr` with a garbage vtable pointer goes to the "unknown" recorder with the target address in the log. Both sides read the same word, so the target is compared too.
- Stubs with real semantics, because the caller depends on the effect: `memset`/`memcpy`/`memmove` (`Monster::init` zeroes the object), placement new `__nw__FUiPv`
  (returns `a1`; used with `AnimBlend` in `takeAdditiveRecoil`/`launchDefaultProjectile`) and a deterministic `bump` allocator for `__builtin_new`/`malloc`, reset on every state.
  libm (`cosf`, `sinf`, `atanf`, `acosf`, `tanf`, `sqrtf`, `atan2f`): run the recompiled retail one, which is deterministic, or record like the difftest. The `timerGet*` only read globals and can run for real.
  `printf`/`inputSetActuator`/sound: only record.
- Comparison: today it is return value + arena + image. It also becomes the **call sequence with arguments**, and the callee-saved registers (`s0..s7`, `fp`, `sp`, `gp`) on exit.
  New class `calls` (memory equal, calls different).
- Random state: the current one (16 KB arena, float/pointer/small/garbage words) is enough at this stage, because no callee executes. Add: (a) the test's `$ra` outside the table,
  so the final return is not mistaken for a call; (b) a cap on calls per state (e.g. 10,000) to cut loops over a random list
  (`getBestTarget__14AiDetonateHead` with 10 `jalr`, like `getClosestMonster`, which already times out).
- Effort: **~1 day** (generator in `oracle_all.py` + template). Covers **the 35**, and the other 138 non-leaf ones of today in the same pass.
  Quality: good on the 32 small and medium ones; on the 3 big ones it compares their own logic and the call sequence, which is where NM tends to go wrong (`nm_calls.py` has already shown that statically).

**Stage 2: real retail callees (allowlist).**
- Compile `output_fix/*.cpp` + `register_functions.cpp` once into a `libretail.a` (5.4 thousand files, `-O1`, a few minutes with `-j`) and use the generated table instead of the harness's.
- In the NM version, rewrite in the `.cpp` text the `DirectCall`/`DirectJump` targets to the retail address of the same name. The `ra` stays the NM one, and
  `dispatchGuestBranch` compares it with the NM `volta`, which matches. That way both sides call **the same retail callee** on the same retail image and only the tested function differs.
- Actually run only the "pure" and leaf callees: `mathf*`, `smoothEasy*`, `PointMass::resolveForcesEuler`, `StaminaMeter::hasEnough`, `getReflexDelay`, `getFovRelevance`, `AnimBlend` getters.
  The others stay recorded. This removes the false positives of "the random return value led to an impossible path".
- Effort: **+1 to 1.5 days**.

**Stage 3: state from a real dump (for AI and Monster).**
- Load a fight `ram_<q>.bin` (`PS2X_DUMP_RAM`) instead of the random arena. `this` comes from the real lists (`TheGame` players, each monster's `Ai`, `Cameras`),
  with coherent vptr, `EnemyInfo`, `Interactives` and `LevelPickups`, and the randomness only perturbs N float/flag fields of the object. The comparison becomes the whole RAM (32 MB, `memcmp`).
  The prerequisite is the map class → where to find instances in the dump (part of it already exists in `native/wotm_syms.h`).
- With that, callees with side effects (`enterNewState`, `takeDamage`, `knockBack`) can run for real, covering `takeHit`/`update`/`init__7Monster` end to end.
- Effort: **+2 days**.

Common prerequisite: **regenerate `output_nm_all`**. Today's is from 2026-10-16 (`nm_all.elf` 13:40). An equivalent made after that still shows up there with the retail asm and
would give a false `exact`. `output_fix` (2026-10-09) is fine, because retail does not change and the translator already had the `sqrt.s` fix.

## 3. Coverage with gameplay to feed back into `config/boot_coverage_asm.csv`

### How the csv was generated (2026-10-09)
- Tap in `PS2Runtime::dispatchGuestBranch` of the WSL clone (`~/wotm-recomp/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp`, uncommitted modification):
  `g_wotmCov[alvo>>2] = 1` for every call target, jump between functions or return. Every 300 frames, with `PS2X_COVERAGE=<arquivo>` (file), the runtime **overwrites** the file with
  the marked addresses, without counts.
- Run: boot to the menus, ~3 min, movies skipped (`~/wotm-recomp/work/cov.log`) → `cov.txt` (786 targets) → `~/wotm-recomp/work/covmap.py`
  (target → function from `status.csv` by address+size range; count by state; writes the `asm` ones) → `cov_asm_pendentes.csv` (601 functions) → copied to `config/boot_coverage_asm.csv`
  ([ANALYSIS.md](../../ANALYSIS.md) (`docs/ANALYSIS.md`): 786 functions = 97 matched, 31 equivalent, 601 asm, 57 hw). `covmap.py` **is not in the repo** and has the Windows path hardcoded.
- Meaning: the decompilation queue "what the game really calls and is still in asm", and it is the default for `tools/m2c_batch.sh`. Today 315 of the csv's 558 distinct functions
  are still `asm` (there are repeated names, such as each TU's `__static_initialization...`). The rest has been done.

### What the Windows runtime already has
`wotm-runtime.patch` evolved the tap: `++g_wotmCov[alvo>>2]` (`uint32` count) and, with `PS2X_COVERAGE=<prefixo>` (prefix), writes `<prefixo>.<apresentação>` (prefix.presentation) every 300 presentations
(`0xENDEREÇO contagem` (0xADDRESS count), cumulative). It is documented in [RUNTIME.md](../../steps/RUNTIME.md) and [BOUNDARY.md](../../steps/BOUNDARY.md) (`FRONTEIRA.md`), and `tools/hw_frame.py` reads that format (calls per frame, modes `hw|sce|tudo` (`tudo` = all)).
**Instrumentation is not missing: only the procedure and the mapping are.**

Hint of the gain, with old files that are still in `wotm-recomp-win\discroot\cov.*` (from 10-09/10-10, from mixed runs, only for an order of magnitude):
`cov.4200` (a run that reached the level) has 1380 functions (498 that are asm today). The union with `cov.2700`/`cov.6000` gives 1441 functions, 514 asm today,
and **199 asm functions (~122 KB) that are not in `boot_coverage_asm.csv`**: AiReflex 19, VehicleNavigator 14, AiSeek 13, HeliVehicleNav 13, MonsterStates 13, Hud 10, Monster 9, Vehicle 7...

### Limits of the tap (for reading the result)
- Coverage by **function entry** (dispatch target), not by block. Returns land inside the caller, which is already marked, and `covmap` maps by range.
- A function entirely replaced by a native hook (`snd_*`, `timerGetCurTics`, `uiMain` under `WOTM_MATCH`, movies, `hierFlushObjQ`...) is still counted, but its callees disappear.
  The direct C++ calls that do not go through the dispatch are only SDK/syscall (`EIntr`, `kprintf`, `Exit`...).
- The file suffix counts **presentations**: on a 180 Hz monitor there are 3 per game frame. For `hw_frame.py` to divide correctly, run with `WOTM_HZ=60`.
- `WOTM_MATCH` skips the menu and `AIVSAI` does not exercise human input (`playerUpdateInputs`, the player's `PadFlags::interpretInputs`). That is why a pass with `run_live -Auto` (pad script) and a played fight are also included.

### Proposed procedure
1. **Tool in the repo**: `tools/cov_merge.py` (`covmap.py` with a relative path, accepting the format with counts and the old one, merging N files). Outputs:
   `config/coverage_asm.csv` with `tu,function,address,size,calls,origem` (origin: `boot|menu|luta`, `luta` = fight), sorted by TU/address, and a summary per TU.
   `m2c_batch.sh` starts reading that csv, or `boot_coverage_asm.csv` is regenerated with the same 4 columns, so as not to break whoever reads it.
2. **Capture** (each run in its own folder, `WOTM_HZ=60`):
   - boot + menu: `run_live.bat -Auto` with `PS2X_COVERAGE=<pasta>\boot\cov` (`<pasta>` = folder);
   - fights: `tools\lote.ps1` with a new `-Coverage` option (a few lines: before each fight, `PS2X_COVERAGE="$disc\cov"` and delete `cov.*`; afterwards, move the last `cov.*`
     to `$out\$tag.cov`). A short batch that covers levels and monsters: `-Monsters 1,2,3,4,5,7,8,9,10,11 -Levels 1,2,4,5,7,8,9,10,13,14,15,16 -AIs 1 -Secs 120`, plus a few
     with `-AIs 3` (pickups, vehicles, helicopters) and with `WOTM_CHEATS=vencer=25` (`vencer` = win; end of round, victory, `updateDeathSequence`);
   - one fight played on the keyboard (human input, grapple, throw, special). It needs the owner at the machine: give notice first, without injecting keys.
3. `python tools/cov_merge.py <pasta>\**\*.cov <pasta>\boot\cov.*` → new csv. Check the difference against `boot_coverage_asm.csv` (expected: +200 asm or more) and commit (only `config/` and `tools/`, no gate).
4. Delete the `cov.*` files and the batch folder after the csv is generated (rule of disposable dumps).

Effort: script ~2 h; option in `lote.ps1` ~1 h; capture ~1 h of machine time (batch without a human watching) + ~10 min of played fight.
I did not open the runtime in this investigation.
