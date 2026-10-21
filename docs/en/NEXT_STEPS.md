*English translation of [docs/PROXIMOS_PASSOS.md](../PROXIMOS_PASSOS.md). The original is authoritative; this copy may lag behind it.*

# Next steps (updated on 2026-10-19; details in docs/passos/, English in [steps/](steps/))

## Direction (decided on 2026-10-09, confirmed on 2026-10-16): port first
The goal is still a native PC port that is easy to mod. The base is the **recompiled** game (PS2Recomp, outside the repo) plus this repo's native layer (`native/`), and the decompilation is the
readable layer that replaces, via *hook*, only the functions that matter for modding. Practical consequences:
- **The port comes before the decomp.** Next: graphics bugs **in the inventory order** of the check ([steps/VU1CHECK.md](steps/VU1CHECK.md), "Inventário do port" (port inventory)),
  with `tools\corpus.ps1 -Check` before and after each change in `native/` (workflow in [CLAUDE.md](CLAUDE.md), "Fluxo do port" (port workflow)). The guinea-pig runtime (equivalents inside the runtime) is on hold, incomplete
  ([steps/NATIVE.md](steps/NATIVE.md), "Runtime cobaia" (guinea-pig runtime)).
- **Decomp only on the mod surface:** `Shell::Load*`, `MonsterParse`, camera. **Byte match is not a per-function requirement.** `equivalent` is enough; `matched` when it comes cheap; `gate.sh`
  is still mandatory (the normal build's ROM must match the SHA1). When decompiling outside that surface again, the queue is `config/boot_coverage_asm.csv` (engine/formats/logic first;
  memory card, `input`, `vi`, `ps`, `sce/*`, `lib989snd` last).
- **Verifying the equivalents:** `tools/verify_tu.py game/<TU>` (score, difftest, calls, layout, return values); for functions with VU0 or `min/max/madd` use `tools/recomp_oracle`
  ([tools/recomp_oracle.md](tools/recomp_oracle.md), `tools/recomp_oracle/README.md`; currently leaves only, 24 of 60 candidates ran).
- **Frame interpolation (pending, chosen by the owner on 2026-10-18):** presentation already runs at the monitor rate ([steps/NATIVE.md](steps/NATIVE.md), "Taxa de apresentação" (presentation rate)), but it repeats the game frame. For real intermediate frames, the native layer would draw the blend of the last two snapshots (camera, object matrices, bone palette), with one frame of delay; particles and objects that are born or disappear between the two need separate handling. Running the logic above 60 does not work: `updateRate` only comes from 60/fields and many things count in frames.
- **After the hardware boundary (owner's list, 2026-10-19):** RAM above 32 MB, enemy HUD, save in JSON (by names), `mods/` folder, no memory card screen, autosave icon, orbit camera, mods that add skins and monsters, training mode, playable bosses, multiplayer up to 4, 3D preview in select, campaign with up to 4; in the gaps, control remapping, photo mode and PC graphics; order, dependencies and notes in [steps/AFTER_BOUNDARY.md](steps/AFTER_BOUNDARY.md) (`passos/POS_FRONTEIRA.md`).
- **Open items off the critical path:** PR #277 in PS2Recomp (`SQRT.S`/`RSQRT.S`; 108 + 76 game functions affected); oracle for functions with calls; coverage with gameplay to feed back into `boot_coverage_asm.csv`.

## Where we are
- **Port:** the game runs on Windows at 60 fps with native drawing (levels, monsters, particles, shadows, fog, menu text), fighting by keyboard, sound and movies. Geometry matched with the VU1 on more than 99%
  of triangles; texture, UV, color and fog checked; `hier` HUD (portrait, health and energy bars, *letterbox*) and Hor+ widescreen (16:9 window, `WOTM_ASPECT`) and fades since 2026-10-16; parallel light, point lights (fire) and light under scale like the VU1 since 2026-10-17. Game and test executable: [steps/BOUNDARY.md](steps/BOUNDARY.md) (`passos/FRONTEIRA.md`), steps 5 to 10 (hardware boundary closed on 2026-10-19). The rest of the inventory is missing (open residues in [steps/VU1CHECK.md](steps/VU1CHECK.md)). Code in `native/` and `native/runtime/`; status per area in [steps/NATIVE.md](steps/NATIVE.md).
- **Decomp:** `sh tools/wsl/gate.sh` says `ROM OK` (build + SHA1). Progress (`python3 tools/progress.py`, on 2026-10-16): `game` 1723 of 3181 functions decompiled (identical + equivalent),
  417 KB of 909 KB; `common` 836 of 1144. Conventions in [ANALYSIS.md](ANALYSIS.md) ("Convenções de status das funções" (function status conventions)).
- **Day-to-day tools:** `tools/corpus.ps1` (regression corpus: 5 fixed game snapshots that the check redoes before and after each change), `tools/vu1cmp.ps1` (VU1 vs native check and images from snapshots, with inventory by owner, trigger
  `mem:currScreen` and F8 key), `tools/lote.ps1` (batch fights with watchdog), `tools/verify_tu.py`, `tools/m2c_batch.sh`, `tools/wotm_env.py` (table of `WOTM_*` variables).

## How to resume
1. `wsl -d Ubuntu` (the default distro is `docker-desktop`, which does not work). Venv: `~/.venvs/wotm/bin/python`. Root: `/mnt/c/Users/TwistZero/WoTM`.
2. **Port:** `build_runtime.bat` (`build_dev` build, without LTCG), `run_live.bat` to play (it opens `ps2EntryRunner.exe` because it enables the screenshots; the clone's `run_live.ps1` opens `wotm.exe`; keys in [steps/RUNTIME.md](steps/RUNTIME.md)), `tools\corpus.ps1 -Check` before and after. Commit without the gate.
3. **Decomp, commit gate:** `sh tools/wsl/gate.sh && git commit ...` (preferably with `git commit` running on Windows; WSL's git has no identity).
   `check.sh; git commit` or `check.sh | tail && git commit` commit broken builds.
4. Converting new TUs: `sh tools/wsl/convert_tus.sh TuA TuB ...` (one at a time, only keeps the ones that do not break the ROM, places vtables with `place_data.py`).
   After a TU is removed/reverted, regenerate with `~/.venvs/wotm/bin/python configure.py --split` (otherwise `build.ninja` points at a nonexistent file).
5. Per function: read `asm/nonmatchings/<tu>/<func>.s` (or run m2c), write the C++ in place of the `INCLUDE_ASM` line,
   `sh tools/wsl/scoreall.sh <TU...>` (scores with `-DNON_MATCHING`), `nm_wrap.py <arquivo> <simbolo> "<nota>" [Classe::metodo]` (file, symbol, note, Class::method) if it does not match.
   Drafts for the whole queue at once: `sh tools/m2c_batch.sh` (`build/m2c/`). At the end of the TU: `~/.venvs/wotm/bin/python tools/verify_tu.py game/<TU>`. Details in [steps/TOOLS.md](steps/TOOLS.md).
6. Before touching a shared header: `sh tools/wsl/scoreall.sh <todos os TUs de src/game> > ~/base.txt` (all the TUs in src/game); then compare with `diff`. Run `gate.sh`.

## Index of the details (`docs/passos/`, English in `docs/en/steps/`)
- [NATIVE.md](steps/NATIVE.md): `native/` (raylib), textures/palettes, live drawing, skinned monsters, LOD, particles/effects, shadows, physics with `dt` in the runtime, host hooks, watchdog (vigia), direct match (`WOTM_MATCH`), batch (lote), guinea-pig runtime, table of `WOTM_*` variables.
- [VU1CHECK.md](steps/VU1CHECK.md): VU1 vs native check: how it works, **port inventory** (what is missing/extra, by owner), status, open residues, particle microprograms, explanations (the history of fixes up to 2026-10-17 is in git, `git show 5eca44b:docs/passos/VU1CHECK.md`).
- [RUNTIME.md](steps/RUNTIME.md): recompiled runtime on Windows: build without LTCG, `PS2X_*` variables, keyboard, boot trace, useful addresses.
- [TOOLS.md](steps/TOOLS.md): `tools/`, shared headers (`include/`), `TheGame` state, difftest.
- [PITFALLS.md](steps/PITFALLS.md): bugs/gotchas already seen (gcc 2.95, gp, rodata, `sqrtf`, `__asm__("sym")`, NM build).
- [PCSX2_TESTING.md](steps/PCSX2_TESTING.md): halfcpp ELF in PCSX2, `gen_nm_ld.py` layout, heap, behavior bugs found while playing.
- [BACKBONE.md](steps/BACKBONE.md): the `main` -> monster on the map chain, menu, `rtMain`, monster states: what is known about the game flow.
- [BOUNDARY.md](steps/BOUNDARY.md): hardware boundary: what of the PS2 the port still uses per frame (drawing, VU0, IOP, clock, SDK), how the runtime serves each part and the order for removing the emulator code from the executable (`tools/hw_frame.py` measures it).
- [AFTER_BOUNDARY.md](steps/AFTER_BOUNDARY.md): priorities after the hardware boundary (RAM, HUD, save in JSON, camera, mods, bosses, multiplayer up to 4), with what is already known about each item.
- [WEB_VIEWER.md](steps/WEB_VIEWER.md): idea for a 3D model viewer in the browser (2026-10-14).
