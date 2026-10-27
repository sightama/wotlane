*English translation of [CLAUDE.md](../../CLAUDE.md). The original is authoritative; this copy may lag behind it.*

# WotM: decompilation of War of the Monsters (PS2, NTSC-U, SCUS-97197)

Decompilation project for the `SCUS_971.97` executable. The final goal is a native PC port that is easy to mod. The project owner writes in Portuguese; reply in Portuguese.

**Direction: port first.** The base is the game recompiled with PS2Recomp (outside the repo; the generated C++ is game code) plus this repo's native layer
(`native/`: GPU drawing, sound, movies, keyboard), which already runs the fight at 60 fps. The decompilation (`src/`) is the readable layer that replaces, via hooks, only what matters for modding (`Shell::Load*`, `MonsterParse`,
camera); the test-subject runtime with the equivalents is on hold. **Byte match is not a per-function requirement** (`equivalent` is enough; `matched` when it comes cheap; `gate.sh` is still mandatory). The emulated VU1/GS of the
runtime is only the reference for the check ([steps/VU1CHECK.md](steps/VU1CHECK.md), `docs/passos/VU1CHECK.md`). The rules below still apply; where they say "matching", read "matching when that is the function's goal".

**What to do:** **[TODO.md](TODO.md)** (`TODO.md` at the root; the list and the order are the owner's); the owner's bug reports take priority over it.

Read before starting: [README.md](../../README.md) (setup), [ANALYSIS.md](ANALYSIS.md) (`docs/ANALYSIS.md`: what is known about the ELF and the status conventions), [PLAN.md](PLAN.md) (`docs/PLANO.md`: architecture) and [TODO.md](TODO.md). Index of `docs/passos/` (English in [steps/](steps/)):
- [NATIVE.md](steps/NATIVE.md) (`NATIVO.md`): native layer (drawing, textures, monsters, particles, shadows, menus, the "Recompiled options" panel, icons), host hooks, watchdog (vigia), batch (lote), table of `WOTM_*` variables.
- [VU1CHECK.md](steps/VU1CHECK.md): VU1 vs native check, corpus, the owner's photos, open bugs.
- [RUNTIME.md](steps/RUNTIME.md): recompiled runtime on Windows (build, `PS2X_*`, keyboard, useful addresses). [BOUNDARY.md](steps/BOUNDARY.md) (`FRONTEIRA.md`): what of the PS2 the port no longer emulates.
- [TOOLS.md](steps/TOOLS.md) (`FERRAMENTAS.md`: `tools/`, headers, difftest), [PITFALLS.md](steps/PITFALLS.md) (`ARMADILHAS.md`: gcc 2.95 and NM build gotchas), [BACKBONE.md](steps/BACKBONE.md) (`ESPINHA.md`: game flow), [PCSX2_TESTING.md](steps/PCSX2_TESTING.md) (`TESTE_PCSX2.md`: halfcpp in PCSX2).
- [MODS.md](steps/MODS.md) (`mods/` folder, textures as PNG, `tools/modkit.py`), [MODELOS_3D.md](../passos/MODELOS_3D.md) (animated monster straight from the disc; no English copy), [WEB_VIEWER.md](steps/WEB_VIEWER.md) (`VIEWER_WEB.md`: evaluation of the in-browser viewer; the viewer is ready at https://yanm1103.github.io/wotm-model-museum/).
- Dated investigations in `docs/investigacoes/<data>/` (`<data>` = date; English in [investigations/](investigations/)). A closed one (what it asked for is already in the code) is deleted; whatever stays pending goes to `TODO.md`, and references become `<commit>:docs/investigacoes/...` (read with `git show`).
- [FORMATS.md](FORMATS.md) (`docs/FORMATOS.md`): the game's file formats (level, monster, sound, movie, memory card save).

## Documentation: three kinds of text, each in its place
- **Reference** (`README.md`, this file, `docs/*.md`, `docs/passos/*.md`): how things are today, in the present tense. No dates, no "it used to be", no "done/fixed on", no account of how it was tested. A measured number only goes in if it is used (e.g. "9619 ticks per field").
- **To do** (`TODO.md`): only what is open, each item short (what it is, where to start, where the context is). Finished: the line goes away, with no "done" and no strikethrough; what came to exist goes into the reference. The owner's decisions not to do something go under "Descartado" (ruled out).
- **Diary** (what happened, why, how it was measured and tested): the commit message and `docs/investigacoes/<data>/`.
- `python tools/doclint.py` checks the lines the commit adds and the whole `TODO.md`; git's pre-commit runs it on its own (turn it on once per clone: `git config core.hooksPath tools/githooks`). An old line that is only edited is not charged for what it already had; `--all` lists what still needs migrating.

## Port workflow (change in `native/`)
1. Before touching anything: `tools\corpus.ps1 -Check` saves the baseline (`<runtime>\corpus\resumo_<data>.csv`; capture the corpus once with `-Capture`). What is wrong or missing comes from the inventory ([VU1CHECK.md](steps/VU1CHECK.md), "Inventário do port" (port inventory)), not from memory.
2. Edit `native/*.hpp` or `native/runtime/*.inc|.cpp` (hooks and game code that run inside the runtime). Addresses by name (`wotm::sym::`/`wotm::fn::` from `native/wotm_syms.h`), never raw numbers. A new diagnostic variable goes into the table in [NATIVE.md](steps/NATIVE.md) (`NATIVO.md`; `python tools/wotm_env.py` checks it).
3. `build_runtime.bat` at the root (configures and builds `build_dev`, without LTCG; ninja does not track `native/`, the script takes care of that). Never the `build.bat` from the old folder with LTCG.
4. Check: `tools\corpus.ps1 -Check -Baseline <baseline csv>` says what got worse; `run_live.bat` to see it on screen; `tools\lote.ps1` for batch fights with the watchdog (vigia).
5. Commit without the gate (`native/`, `docs/` and `tools/` outside the build do not change the ROM). A change to a PS2Recomp seam (CMake, `main.cpp`, taps) requires regenerating `tools/runtime_patch/wotm-runtime.patch`.

## Rules that cannot be broken
1. **No game file goes into the repo.** `ISO/`, `disc/`, `asm/`, `assets/`, `build/`, `*.rom`, `*.elf` and `tools/cc/` are in `.gitignore`. `asm/` contains game code: never `git add -f`, never paste game assembly into issues, PRs or documentation.
2. **The ROM must keep matching the retail SHA1.** Every commit that changes decompiled code (`src/`, `include/`, `asm/`, config read by the build) goes through `sh tools/wsl/gate.sh && git commit ...` (build + SHA1, exits with an error if it fails); a commit that only touches `native/`, `docs/` or `tools/` outside the build cannot change the ROM and does not need the gate. Never `gate.sh | tail && git commit` nor `check.sh; git commit`: the pipe hides the failure and the commit goes out with a broken build.
3. **No direct push to `master` if you are not the owner.** Collaborators work in a fork/branch and open a PR (see [CONTRIBUTING.md](CONTRIBUTING.md)).
4. **Mark clearly what is identical and what is only equivalent.** A function that matches byte for byte gets no marking; a function that does not match goes in `#ifdef NON_MATCHING` (with `INCLUDE_ASM` in the `#else`) and the comment states the score and the cause. Use `tools/nm_wrap.py`. A behavior change disguised as decomp is not accepted; if one is necessary, flag it explicitly.
5. **Do not spend effort on hardware code** (GS/VIF/DMA, timers, SDK): it is in `config/hw_funcs.txt` and the port replaces it. `game/Sound` and `game/StreamingSoundManager` are not hardware, but they are low priority.

## Environment
- Windows, with the build running in **WSL Ubuntu**: use `wsl -d Ubuntu` (the default distro is `docker-desktop`, which does not work). Venv: `~/.venvs/wotm/bin/python`. Repo root in WSL: `/mnt/c/Users/<você>/WoTM` (`<você>` = you).
- Compiler: ee-gcc 2.95.2 (SN ProDG), downloaded by `tools/setup_toolchain.sh` into `tools/cc/`. Requires your own ISO in `ISO/`, extracted to `disc/`.
- `.sh` scripts need LF. Writing with Python on Windows leaves CRLF and WSL's `sh` breaks (`sed -i 's/\r$//'` fixes it).
- `config/status.csv` (the state of each function) is generated by `python3 tools/progress.py`; only the owner commits it. **Collaborators do not commit this file**, to avoid conflicts in every PR. The README has no progress block.
- When writing files with regex or `\n`, use the Edit/Write tools. A Python heredoc turns `\n` into a real line break.
- **Never run inline Python** (`python3 - <<EOF`, `python -c "...\..."`): Windows paths (`C:\Users\...`) and `\n` break. A hook (`.claude/hooks/block_python_heredoc.py`, enabled in `.claude/settings.json`) rejects these commands. Save the script with Write (scratch folder) and run `python arquivo.py` (`arquivo` = file); use Edit/Write to change repo files.

## Per-function workflow
1. Read `asm/nonmatchings/<tu>/<função>.s` (`<função>` = function). For more than ~30 instructions, `sh tools/m2c.sh <tu> <função>` gives a draft. Never trust it without reading the asm (the arguments it shows are register noise). Replace `unkNNN` with named fields from the headers.
2. Convert the TU to C++ if it is not already: `sh tools/wsl/convert_tus.sh <Tu>` (one at a time).
3. Write natural C++, in the natural order of the source (it often already matches). Score with `sh tools/wsl/scoreall.sh <TU>`; if it does not match, `tools/nm_wrap.py <arquivo> <símbolo> "<nota>" [Classe::método]` (file, symbol, note, Class::method). Only tune when it is cheap (`tools/wsl/variants.py`, `tools/wsl/permute.py`).
4. Verify the whole TU in one command: `~/.venvs/wotm/bin/python tools/verify_tu.py game/<TU>` (score, difftest, call sequence, layout and consumed return values, in one table per function; `--no-nm` skips the ELF for the equivalents and takes seconds instead of minutes). m2c drafts for the whole queue: `sh tools/m2c_batch.sh` (in `build/m2c/`).
5. Run `gate.sh`, then `python3 tools/progress.py`, then commit.

## Known pitfalls (bugs that have already happened)
An "untuned" score can hide a semantic error. After writing an equivalent function, check:
- **Return value used by the caller.** A `void` where retail returns `$v0` (e.g. `addInteractive`: the slot index is the hat's id). `tools/nm_retvals.py`.
- **Call sequence.** `tools/nm_calls.py`.
- **Struct size and stride.** Add a `static_assert` with the retail stride (e.g. `PathNode` is 0x40, not 0x30). An empty base class takes one byte (`AiBrain`).
- **Delay slots.** An inverted compare hidden in the delay slot (e.g. `Monster::isIdle`) and a wrong branch target (e.g. `AiGrappleAttack`, heavyPunch vs toss). Read the asm to the end.
- **Order and helpers.** When replacing `INCLUDE_ASM` with C++, keep the functions in original address order (each definition where its `INCLUDE_ASM` was, never grouped). `static`/`static inline` helpers go inside `#ifdef NON_MATCHING`: ee-gcc 2.95 emits unused helpers and shifts the ROM.
- **Strings and rodata.** Moving strings from `INCLUDE_ASM` to C changes the rodata padding (add `.word 0` in asm in `.rodata`). `switch` jump tables in C need the same padding (that is why a function with a `switch` stays in `NON_MATCHING` even when it matches: the table comes from the retail rodata). Virtual classes: keep the vtable, ctor and `__tf` as `INCLUDE_ASM`; `tools/place_data.py` places data by address.
- **Shared headers.** Moving a class to a header can change `sizeof` and shift fields of partial structs. Before touching: `sh tools/wsl/scoreall.sh <todos os TUs de src/game> > ~/base.txt` (all the TUs in src/game); then compare with `diff`.
- **Compiler tricks.** `__asm__("#SNFIX_SMALL sym")` marks a global as gp-relative; `register float x __asm__("$f2")` pins an FPR register; an `int` vs `void` return in the callee changes allocation; arguments are evaluated left to right (hoisting a call into a local moves an `addiu`); `sqrt.s` needs `.word 0x46040104` (in NM C++ use `eeSqrtf` from `include/vecmath.h`: gas encodes `sqrtf` in the MIPS32 form and the EE reads `$f0`).

## Order of attack
Decompile top-down along the real execution path, not by a "small functions" filter: `main` → `Shell::LoadLevelFiles/LoadLevelDB/LoadMonstersDB` → `dbInitDb`/`dbsRelocate*`/`file*` → `InitPlayers`/`AddMonster`/`MonsterParse` → `rtMain` loop (hier/animation/drawing) → `TheGame::Update` logic. These loaders also document the file formats. For scope by reachability: `python3 tools/callgraph.py <raízes> --no-libs` (`<raízes>` = roots; writes `config/callgraph.csv`; copy and restore it if you do not want to overwrite it; it only follows `jal`, so it is a lower bound).

## Behavior verification
No unit tests, by project decision. Verification is through: `tools/difftest.py` / `difftest_all.py` (retail vs our function in a MIPS emulator, same random state), and, when something is running, comparison with PCSX2. Debugging a symptom: pause PCSX2, extract the EE RAM and recompute the retail logic from the dump (`tools/ramdiff.py`). If an ELF variant hangs with "executing data", compare the EE RAM with the ELF before suspecting the decompiled function (it has happened: it was relocated code overwritten by DMA).

**Dumps and snapshots are disposable.** `.vu1snap` (~40 MB each), `ram_*.bin`/`vram_*.bin` (36 MB), watchdog (vigia) dumps and `lote/` (batch) folders stay out of the repo and fill the disk. Once a fix is validated, delete the snapshots and dumps that motivated it (`snaps/`, `discroot/ram_*`, `lote/<data>/`); keep only the ones that still show an open problem or that you deliberately named as a regression corpus (`tools/vu1cmp.ps1 <pasta>` (folder)). Never leave `WOTM_VU1CHECK_SNAP` on in a batch.
