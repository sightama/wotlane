*English translation of [docs/passos/RUNTIME.md](../../passos/RUNTIME.md). The original is authoritative; this copy may lag behind it.*

# Recompiled runtime (PS2Recomp): how to run it and what to know

(Split from [NEXT_STEPS.md](../NEXT_STEPS.md) (`docs/PROXIMOS_PASSOS.md`); index there.) This file keeps what is used today. The diary of the performance investigations (batched IOP, deferred timers, virtual clock, native VU0) is in [NATIVE.md](NATIVE.md) (`NATIVO.md`, "Física com `dt`" (physics with `dt`)) and in the git history (`git log -- docs/passos/RUNTIME.md`).

## Where the code is
- Runtime clone (outside the repo): `C:\Users\TwistZero\wotm-recomp-win` (submodule `third_party/PS2Recomp`, branch `wotm` of the fork `yanm1103/PS2Recomp`). The generated code (`register_functions.cpp` etc.) never goes to the repo.
- Our code that runs inside the runtime is in **`native/runtime/`** in this repo ([NATIVE.md](NATIVE.md), "Onde mora o código do runtime" (where the runtime code lives)); the fork only has the seams ([tools/runtime_patch.md](../tools/runtime_patch.md), `tools/runtime_patch/README.md`).

## Fast build (without LTCG)
The PS2Recomp release enables `/GL` + `/LTCG` and the link takes ~28 min on every change. With `-DPS2X_FAST_LINK=ON` (branch `wotm`) the game still runs at 60 fps; incremental rebuild in ~13 s, from scratch in ~4 min.
- **Always use `build_runtime.bat`** at the repo root (or `tools/build_runtime.ps1`): it configures `build_dev` with `-DPS2X_FAST_LINK=ON -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF -DPS2X_ENABLE_IOP_RPC_TRACE=OFF`, touches `ps2_runtime.cpp` (ninja does not track `native/`), generates the native VU0 `.inc` when it is missing and compiles. Since 2026-10-19 **two executables** come out in `build_dev\ps2xRuntime\`, from the same objects of the translated game: `wotm.exe` (the game, without emulated GS/VU1/VIF1 and without the check) and `ps2EntryRunner.exe` (test: emulated parts, `WOTM_VU1CHECK`, `WOTM_FOTOS`, `--vu1cmp`; what `corpus.ps1` and `vu1cmp.ps1` use). The clone's `build_dev.bat` compiles both (`--target ps2EntryRunner wotm`); an old clone needs that line. Details in [BOUNDARY.md](BOUNDARY.md) (`FRONTEIRA.md`), step 5. `build_runtime.bat nm` does the same for the guinea-pig runtime (`build_dev_nm`).
- **Aggressive logs must stay OFF.** `PS2X_ENABLE_AGRESSIVE_LOGS` comes `ON` upstream: every generated function writes entry/exit with a flush (295 MB in 45 s) and the boot does not get past Press Start.
- The build links with `/MAP` and, since 2026-10-19, `/DEBUG` (PDB with public symbols only, without `/Zi`; `/OPT:REF /OPT:ICF` keep the binary; `cmake/ReleaseMode.cmake`, `PS2X_FAST_LINK`). `PS2X_HOSTPROF` resolves names through the PDB; to locate a `[CRASH] exe+0x…` without it, look for the largest symbol ≤ the address in `ps2EntryRunner.map`.

## Running
- `run_live.bat` (or `run_live.ps1` in the clone: `-Auto 0` to play, `-Stats 1` logs `[native]`, `-Build build_dev_nm` for the guinea pig, `-Exe test` forces the test one). It runs `wotm.exe`, except with `WOTM_VU1CHECK` or `WOTM_FOTOS` on (`run_live.bat` turns screenshots on by default; the launcher does not), with `-Exe test` or when `wotm.exe` is missing or older: then `ps2EntryRunner.exe`. It sets `PS2X_NO_VU1=1 PS2X_NO_VIF1=1 PS2X_NO_GS=1 PS2X_SCENE=1 PS2X_FMV=1` (emulated VU1 and GS off in the test one, native drawing and movies; `wotm.exe` turns all three on by itself). It uses `Start-Process -RedirectStandardError` on purpose (PowerShell 5's `2>` writes UTF-16). Logs in `live.err.log`.
- Direct match without the menu: `WOTM_MATCH=<jogador>[,<ia>...],<fase>` (player, AI, level; 0 to 7 AIs; no AI = alone in the level); AI vs AI: `WOTM_AIVSAI=1`; batch: `tools\lote.ps1` (all in [NATIVE.md](NATIVE.md)).
- Other useful `PS2X_*`: `PS2X_PAD` (button script with timings), `PS2X_DUMP_RAM="3300,3900"` (writes `ram_<q>.bin`/`vram_<q>.bin` for the viewer), `PS2X_CD_IMAGE` (ISO), `PS2X_PEEK="hex,hex"` (RDRAM words in the log), `PS2X_HOSTPROF=1` (host RIP sampler, all threads every ~1 ms, `hostprof.txt`; `=2` only the EE thread; `=3` only the EE thread and only when its frame goes over 100 ms, waking every 50 ms; with `WOTM_FIELDLOG`, a frame over 100 ms is reported as `[hostprof]` with the approximate call chain), `PS2X_COVERAGE` (function coverage), `PS2X_IOP_BATCH` (default 256).
- **Keyboard** (backend in `ps2_pad.cpp`, adds to the gamepad): **arrows = left analog stick**, **I/J/K/L = D-pad**, **Space = X**, **Z = square**, **X = triangle**, **S = circle**, **Enter = Start**, **Backspace = Select**, **Q = L1**, **W = R1**, **Shift = L1+R1**, **A = L2**, **C = R2**. Native layer: **F3** FPS, **F7** pause and resume, **F6** (paused) advances one game frame, **F8** takes a screenshot with a note (with `WOTM_FOTOS`; only with `WOTM_VU1CHECK`, it records and compares the next frame). Workflow in [VU1CHECK.md](VU1CHECK.md), "Fotos" (screenshots).

## Boot trace (black screen before the movie)
From process start to the first movie, the runtime writes each stage to stderr with the time since start and shows the stage on the black screen ("Loading... 3.2 s"). Always on; it stops when the boot finishes. A stage stuck for more than 2 s repeats as `... ainda em "<etapa>" ha N s (PC do EE 0x..., vsync N, R/s)` (still in "<stage>" for N s (EE PC 0x..., vsync N, R/s)): map the PC with `config/status.csv`. Measured: window 0.6 s; IOP modules 0.8 s; then the game sits ~3.5 s idle counting vsync at 60/s (the game's own wait, grows if vsync drops); movie at 4.4 s. Code: `wotmBoot*` in `wotm_live.inc`.

## Useful addresses (menu)
`currScreen` 0x6F8464 (0x14 = Press Start, 1 = menu and also inside the level, 0x63 = loading), `nextScreen` 0x6F846C, `betweenScreens` 0x6F7E8C, `screenFirstPass` 0x6F7E80, `targetAlpha` 0x6F7E74, `fadingIn/Out` 0x6F807C/0x6F8080, `world` 0x6F87C4, `g_frame` 0x6F7E38. In new code use `wotm::sym::<nome>` (name) (`native/wotm_syms.h`).

## Open items
- Gamepad and key remapping; `PS2X_FAST_BOOT` (skip the opening waits without skipping the initialization).
- The movies' audio follows the wall clock, not the game's.
- Host stalls of 100 to 330 ms outside the fight (`_NLG_Return2`, MSVC stack unwinding) still without a cause.
