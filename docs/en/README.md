# English documentation index

English translations of the project's Brazilian Portuguese docs. In every case the Portuguese original is authoritative; a translation may lag behind it. The main [README.md](../../README.md) is already in English and is not duplicated here.

This repository ([sightama/wotlane](https://github.com/sightama/wotlane)) is a fork of
[yanm1103/War_of_the_Monsters_recomp](https://github.com/yanm1103/War_of_the_Monsters_recomp) and the primary home of our work. We merge upstream changes wherever they make
sense; our changes may be offered to upstream later as pull requests, as a courtesy, if the upstream owner wants them.

## Project
| English | Original | Description |
|---|---|---|
| [CLAUDE.md](CLAUDE.md) | [CLAUDE.md](../../CLAUDE.md) | Rules, workflows and known pitfalls for contributors and Claude Code |
| [CONTRIBUTING.md](CONTRIBUTING.md) | [CONTRIBUTING.md](../../CONTRIBUTING.md) | How to contribute: priorities, setup checks, PR workflow |
| [pull_request_template.md](pull_request_template.md) | [.github/pull_request_template.md](../../.github/pull_request_template.md) | Pull request checklist template |
| [GLOSSARY.md](GLOSSARY.md) | [GLOSSARIO.md](../../GLOSSARIO.md) | Monster and level names (in-game vs internal), cut monsters and bosses |
| [ONLINE.md](ONLINE.md) | (wotlane only, no original) | Playing online with friends: build, host the server, join, what changes online, troubleshooting |

## Plans and status
| English | Original | Description |
|---|---|---|
| [PLAN.md](PLAN.md) | [docs/PLANO.md](../PLANO.md) | Architecture (recompiled base, runtime, native layer, decomp), licensing and milestones |
| [TODO.md](TODO.md) | [TODO.md](../../TODO.md) | What is left to do, in the owner's order, and what the owner ruled out |
| [ANALYSIS.md](ANALYSIS.md) | [docs/ANALYSIS.md](../ANALYSIS.md) | What is known about the disc and ELF, toolchain, TU/data split, function status conventions |
| [FORMATS.md](FORMATS.md) | [docs/FORMATOS.md](../FORMATOS.md) | Game file formats as revealed by the decompiled loaders |
| [screenshots.md](screenshots.md) | [docs/screenshots/README.md](../screenshots/README.md) | Conventions for the native-drawing screenshot history |

## Steps (detail docs)
| English | Original | Description |
|---|---|---|
| [steps/NATIVE.md](steps/NATIVE.md) | [docs/passos/NATIVO.md](../passos/NATIVO.md) | Native layer: drawing, textures, characters, hooks, watchdog, batch runs, `WOTM_*` variables |
| [steps/VU1CHECK.md](steps/VU1CHECK.md) | [docs/passos/VU1CHECK.md](../passos/VU1CHECK.md) | VU1 vs native check: method, port inventory, open residues |
| [steps/RUNTIME.md](steps/RUNTIME.md) | [docs/passos/RUNTIME.md](../passos/RUNTIME.md) | Recompiled runtime on Windows: fast build, running, keyboard, boot trace, useful addresses |
| [steps/BOUNDARY.md](steps/BOUNDARY.md) | [docs/passos/FRONTEIRA.md](../passos/FRONTEIRA.md) | Hardware boundary: what of the PS2 the port still uses per frame and the plan to remove it |
| [steps/AFTER_BOUNDARY.md](steps/AFTER_BOUNDARY.md) | `92af448:docs/passos/POS_FRONTEIRA.md` (deleted upstream) | Old priority list after the hardware boundary; open items now in [TODO.md](TODO.md), finished ones in the step docs |
| [steps/MODS.md](steps/MODS.md) | [docs/passos/MODS.md](../passos/MODS.md) | `mods/` folder, textures as PNG, skins, custom monsters, `tools/modkit.py` |
| [steps/TOOLS.md](steps/TOOLS.md) | [docs/passos/FERRAMENTAS.md](../passos/FERRAMENTAS.md) | Tools, shared headers, `TheGame` decomp status, differential testing |
| [steps/PITFALLS.md](steps/PITFALLS.md) | [docs/passos/ARMADILHAS.md](../passos/ARMADILHAS.md) | Pitfalls already seen in the NM decomp, build and game |
| [steps/PCSX2_TESTING.md](steps/PCSX2_TESTING.md) | [docs/passos/TESTE_PCSX2.md](../passos/TESTE_PCSX2.md) | Running the halfcpp ELF in PCSX2 and behavior bugs found while playing it |
| [steps/BACKBONE.md](steps/BACKBONE.md) | [docs/passos/ESPINHA.md](../passos/ESPINHA.md) | Game flow: load chain, menu, game loop, monster states |
| [steps/WEB_VIEWER.md](steps/WEB_VIEWER.md) | [docs/passos/VIEWER_WEB.md](../passos/VIEWER_WEB.md) | Feasibility of a browser-based 3D model viewer |

## Investigations
| English | Original | Description |
|---|---|---|
| [investigations/2026-10-18/16_monsters.md](investigations/2026-10-18/16_monsters.md) | [docs/investigacoes/2026-10-18/16_monstros.md](../investigacoes/2026-10-18/16_monstros.md) | Feasibility of 16 monsters in one fight, and results with 8 |

## Tools
| English | Original | Description |
|---|---|---|
| [tools/runtime_patch.md](tools/runtime_patch.md) | [tools/runtime_patch/README.md](../../tools/runtime_patch/README.md) | The PS2Recomp runtime patches: what they contain, applying and regenerating |
| [tools/recomp_oracle.md](tools/recomp_oracle.md) | [tools/recomp_oracle/README.md](../../tools/recomp_oracle/README.md) | Recompilation oracle comparing retail functions with our equivalents |
| [tools/pcsx2_auto.md](tools/pcsx2_auto.md) | [tools/pcsx2_auto/README.md](../../tools/pcsx2_auto/README.md) | Automated PCSX2 testing of the half-C++ ELF |
