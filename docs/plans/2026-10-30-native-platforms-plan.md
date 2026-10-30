# Native builds on every platform: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `wotm` builds and runs natively on macOS arm64, then Linux, while the Windows build stays exactly as it is.

**Architecture:** One POSIX path (clang/gcc + CMake + Ninja) next to the untouched MSVC path. Splits go behind `#ifdef _WIN32` /
`__APPLE__` / x86-vs-`USE_SSE2NEON` and CMake `else()` branches. `sh` scripts mirror the `.ps1` ones. PS2Recomp seam changes are folded
back into `tools/runtime_patch/wotm-runtime.patch`.

**Tech Stack:** Apple clang 17, CMake 4.4, Ninja, PS2Recomp `2c5fbb9` + patches, sse2neon v1.9.1, raylib 5.5 (FetchContent), FFmpeg
via Homebrew + pkg-config.

**Spec:** `docs/plans/2026-10-30-native-platforms-design.md`

## Global Constraints

- Windows unchanged: never edit an `if(MSVC)` block, a `.bat`/`.ps1` or a `_WIN32` branch to satisfy clang. Add, don't alter.
- No game code in git: `recomp/`, `runtime/`, `disc/`, `ISO/`, `<build>/wotm_gen/` stay ignored. Check `git status` before every `git add`.
- No `src/`, `include/` or `asm/` changes (no `gate.sh` on this Mac). If one becomes necessary, stop and ask batman.
- A PS2Recomp seam change means regenerating `tools/runtime_patch/wotm-runtime.patch` in the same commit.
- English everywhere. Translation-only edits go in their own commits, never mixed with logic.
- New tools come from Homebrew (`brew install`). Scripts locate `/opt/homebrew/bin` themselves.
- Commits are small and upstreamable, and say what they're for. No pushes to yanm1103's repos and no PRs there without asking batman.
- Expected ELF SHA1: `1ed7f8457436a9170d8f85386e267369c7fcdd4f`. PS2Recomp base: `2c5fbb9389e11dd95693385969490c9e8e6f57b4`.

## Review Focus

- A Windows branch changed by accident: review every diff hunk for edits inside `_WIN32`/`MSVC` code (expected: none).
- arm64 float behavior drifting from x86 in VU0 math: `PS2X_VU0_VERIFY=1` must report 0 differences (Task 5 and milestone 3).
- Scripts run twice, or after a failed half-run: every step must skip completed work or redo it cleanly (Tasks 2, 3).
- Paths with spaces (the ISO is `War of the Monsters (USA).iso`): quote everything in `sh` (Task 2 uses that ISO).
- A fresh shell without Homebrew on `PATH`: scripts must still find `cmake`, `ninja`, `7zz` and `pkg-config` (Tasks 2, 3).

---

## Milestone 1: generate and compile on macOS arm64

### Task 1: Prerequisites and the patch-regeneration script

**Files:**
- Create: `tools/regen_runtime_patch.sh`

**Interfaces:**
- Produces: `sh tools/regen_runtime_patch.sh [runtime dir] [--add <path in PS2Recomp>...]`. It rewrites `tools/runtime_patch/wotm-runtime.patch`
  from `git diff HEAD` in `<runtime>/PS2Recomp`, limited to the file list already in the patch plus any `--add` files (new
  files get `git add -N`). It never includes `ps2xRecomp/src/lib/fpu_translator.cpp`.

- [ ] `brew install ffmpeg pkgconf`. Check: `pkg-config --modversion libavcodec` prints a version.
- [ ] Write the script with `-h` help, `set -eu`, and an error when the runtime dir or its PS2Recomp clone is missing.
- [ ] Verify on a clean, freshly patched clone (from Task 2): running it leaves `git diff tools/runtime_patch/` empty.
- [ ] Commit.

### Task 2: `tools/setup_runtime.sh` and `tools/sync_mods.sh`

**Files:**
- Create: `tools/setup_runtime.sh`, `tools/sync_mods.sh`

**Interfaces:**
- Produces:
  - `sh tools/setup_runtime.sh [--iso <path>] [--skip-build]`. It ends with `runtime/PS2Recomp` patched, `runtime/discroot/SCUS_971.97`,
    `recomp/retail/register_functions.cpp`, `runtime/build_dev/build.ninja`, and `.wotm_recomp_dir` containing the runtime path.
  - `sh tools/sync_mods.sh [runtime] [repo]`, the same behavior as `sync_mods.ps1` (`rsync -a --delete` per mod, skipping `_*` and `.*`).

Steps match `setup_runtime.ps1` one to one:
1. Tools.
2. Disc: use `disc/` if present, otherwise `7zz x -odisc`. Check the SHA1.
3. Clone and check out PS2Recomp, then apply both patches idempotently (`git apply --reverse --check` first).
4. Build tools: `cmake -S PS2Recomp -G Ninja -DCMAKE_BUILD_TYPE=Release -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_TEST=OFF
   -DPS2X_BUILD_STUDIO=OFF -B build_tools`.
5. Translate: run the analyzer, edit `output = "../retail/"` in `config.toml`, then run recomp.
6. Configure `build_dev` with exactly the `-D` flags in `configure_dev.bat`, plus `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` only if
   configure fails without it.

Each step prints `== step`. Command output goes to `runtime/logs/setup_<step>.log`; on failure the script prints the last 30 lines
and the log path.

- [ ] Write both scripts, with `-h` help.
- [ ] Run `sh tools/setup_runtime.sh --skip-build`. Expected: the final line prints the number of translated `.cpp` files (> 0). Failures in
  the analyzer/recompiler build are seam bugs: fix them in `runtime/PS2Recomp`, then run `regen_runtime_patch.sh`.
- [ ] Run it again. Expected: every step says it's skipping.
- [ ] Commit the scripts (plus the regenerated patch, if anything changed).

### Task 3: `tools/build_runtime.sh`

**Files:**
- Create: `tools/build_runtime.sh`

**Interfaces:**
- Consumes: `.wotm_recomp_dir` (Task 2).
- Produces: `sh tools/build_runtime.sh [build dir name, default build_dev]`, which builds the `wotm` and `ps2EntryRunner` targets. It also writes, into
  `<build>/wotm_gen/`:
  - `wotm_version.gen.h` (`#define WOTM_VERSION "<git describe --tags --always --dirty>"`, rewritten only when it changes)
  - `wotm_vu0_native.gen.inc` (`~/.venvs/wotm/bin/python tools/vu0native.py disc/SCUS_971.97 <out>`, when missing or older than the generator)
  - `wotm_resources.gen.h` (Task 6)

  It touches `PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp` first, because ninja doesn't track `native/`. The log goes to
  `runtime/logs/<build>.log`. On failure it prints the first 40 `error:` lines and the log path; on success it prints the binary paths.

- [ ] Write the script, with `-h` help.
- [ ] Run it. Expected: it fails with compiler errors (Tasks 4 to 6 fix them). Save the first error list to scratch, as the work list.
- [ ] Commit.

### Task 4: PS2Recomp seam fixes for non-MSVC/ARM

**Files:**
- Modify (inside `runtime/PS2Recomp`, then regenerate): `ps2xRuntime/CMakeLists.txt`, plus whatever else the build exposes.
- Modify: `tools/runtime_patch/wotm-runtime.patch`, `tools/runtime_patch/README.md` (only if the file list changes).

- [ ] Add `-mavx2 -mbmi2 -mlzcnt -mpopcnt` only for non-MSVC builds when `NOT PS2X_IS_ARM_TARGET` (Linux report, item 1). Leave the MSVC
  `/arch:AVX2` line untouched.
- [ ] Fix any other seam compile error from the Task 3 list the same way, with additions only.
- [ ] `sh tools/regen_runtime_patch.sh`, then review `git diff tools/runtime_patch/` hunk by hunk: no change inside `if(MSVC)`.
- [ ] Commit ("runtime patch: build flags for non-MSVC x86 and ARM").

### Task 5: arm64 SIMD in `native/`

**Files:**
- Modify: `native/runtime/wotm_vu0_native.cpp`, `native/wotm_native.hpp:27`

**Interfaces:**
- Produces: in `wotm_vu0_native.cpp`, a 4×double type `D4` used everywhere `__m256d` is today. On x86, `D4` is `__m256d` and the current
  intrinsics are used. Under `USE_SSE2NEON`, `D4` is `struct { float64x2_t lo, hi; }`, with helpers for cvtps_pd, cvtpd_ps,
  add/sub/mul, andnot (abs), cmp EQ/GT/LT with ordered-quiet semantics, and movemask. `classify4`, `fin4`, `ADD4`/`SUB4`/`MUL4` and
  madd/msub get the same bodies on both, written against `D4`.

- [ ] Includes: `#if defined(USE_SSE2NEON)` → `"sse2neon.h"` + `<arm_neon.h>`, otherwise the current `<immintrin.h>` / `<smmintrin.h>`.
- [ ] Implement `D4` and port the helpers. The x86 build must compile to the same instructions (only the type alias changes there).
- [ ] Verify after Task 7: `PS2X_VU0_VERIFY=1` on a fight (milestone 3) reports no differences. Until then, read through the comparisons
  (`_CMP_EQ_OQ` → `vceqq_f64`, `_CMP_GT_OQ` → `vcgtq_f64`, `_CMP_LT_OQ` → `vcltq_f64`: all false on NaN, matching OQ) and movemask (bit
  i = sign bit of lane i).
- [ ] Commit. Translate this file's Portuguese comments in a separate commit right after (265 lines).

### Task 6: Linux-report fixes in `native/` (items 2 to 7, plus the UB)

**Files (upstream line numbers):**
- Modify:
  - `native/runtime/wotm_snd.inc:14`
  - `native/runtime/wotm_standalone.cpp`
  - `native/runtime/wotm_live.inc:300,471,553`
  - `native/runtime/wotm_prompts.inc:15`
  - `native/runtime/wotm_padsel.inc:2632`
  - `native/runtime/wotm_optscreen.inc:219`
  - `native/wotm_savepics.hpp:356`
  - `native/wotm_preview.hpp:259`
- Modify: `tools/build_runtime.sh` (resources header)

Each bullet is a separate commit:

- [ ] Item 2: `#include "Stubs/CD.h"`. `src/lib/Kernel` is already a public include dir of `ps2_runtime`, so no seam change is needed.
- [ ] Items 3, 4: in `wotm_standalone.cpp`, use `strcasecmp`/`fseeko` and `box()`'s own constants on non-Windows. The kernel32/user32
  declarations go inside `#ifdef _WIN32`. On non-Windows, the crash filter becomes a no-op until milestone 4.
- [ ] Item 5: `build_runtime.sh` reads the file paths in `native/wotm.rc` and writes `wotm_resources.gen.h` (one `static const unsigned char`
  array plus size per resource, named after the resource ID). The three loaders use the embedded array under `#ifndef _WIN32` and
  `__has_include`. If the header is missing, they fall back as today.
- [ ] Items 6, 7, UB: move the declaration to global scope, drop the redundant `= 0.f`, and use `uint32_t(x) * 73856093u`.
- [ ] Translation: `wotm_standalone.cpp`, `wotm_prompts.inc`, `wotm_optscreen.inc`, `wotm_savepics.hpp`, `wotm_preview.hpp`, `wotm_snd.inc` (each
  under 500 lines) get their own translation commits. **Ask batman** before translating `wotm_live.inc`, `wotm_padsel.inc` or
  `wotm_native.hpp` whole: 2.4k to 5.4k lines each, and it guarantees conflicts on every upstream pull.

### Task 7: Milestone 1 check

- [ ] `sh tools/build_runtime.sh`. Expected: `wotm` and `ps2EntryRunner` link.
- [ ] `file runtime/build_dev/ps2xRuntime/wotm` → `Mach-O 64-bit executable arm64`. `otool -L` lists only system frameworks and Homebrew FFmpeg.
- [ ] `git status`: nothing generated is tracked. Write the outcome (build time, warnings worth knowing) in the commit message.

## Milestones 2 to 7 (one detailed plan each, written when the previous one passes)

| # | Done when | Main pieces |
|---|---|---|
| 2 | Intro FMVs, menu, the `sheldemo` video in the menu and sound play on this Mac | `tools/runtime_template/run_live.sh` (same env vars as `run_live.ps1`, `PS2X_CD_IMAGE` = the ISO, logs to `live.err.log`), installed by `setup_runtime.sh` |
| 3 | A 1v1 fight and an 8-monster fight hold 60 fps on the F3 counter. `WOTM_MATCH=3,1,1 WOTM_AIVSAI=1` runs 3 minutes with no `[vigia]` freeze or slow lines. `PS2X_VU0_VERIFY=1` shows 0 differences | Fix what the logs show; measure CPU per frame against the Windows numbers in `NATIVE.md` |
| 4 | `wotm` with no arguments asks for the ISO, keeps settings and saves in the data dir, and a forced crash prints a symbolized stack | `wotm_standalone.cpp` / `wotm_live.inc` POSIX branches (design, "Architecture") |
| 5 | The same `sh` scripts produce and run a Linux build (needs a Linux machine or VM: ask batman which) | Linux branches only |
| 6 | `WotM.app` launches from Finder on a Mac without Homebrew FFmpeg; Linux tarball | `tools/release.sh` |
| 7 | batman builds on Windows (`build_runtime.bat`) and plays a fight: identical | Review the full branch diff for `_WIN32`/`MSVC` edits first |
