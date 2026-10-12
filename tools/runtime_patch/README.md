# Patch do runtime (PS2Recomp)

`wotm-runtime.patch`: nossas mudanças sobre `ran-j/PS2Recomp` (commit base `2c5fbb9`): desenho nativo (`wotm_scene.inc`/`wotm_live.inc`), teclado, perfil do host (`PS2X_HOSTPROF`), lote do IOP (`PS2X_IOP_BATCH`, 60 vsync/s na fase) e medições.

Fica de fora `ps2xRuntime/src/runner/register_functions.cpp` (registro de funções gerado a partir do jogo: código do jogo, não entra no repo). Gere-o com o PS2Recomp a partir do seu ELF.

Aplicar: `git apply --whitespace=nowarn tools/runtime_patch/wotm-runtime.patch` dentro do clone do PS2Recomp.

`wotm-fmv.patch`: filmes nativos (`PS2X_FMV=1`, `native/wotm_fmv.hpp`). Base: branch `wotm` do fork (`yanm1103/PS2Recomp`, 79a7c7e, o submódulo). Toca em `wotm_live.inc` (hooks e `wotmFmvInstall`/`wotmFmvPresent`), `ps2_runtime.cpp` (chamada na apresentação) e `main.cpp` (instalação). Precisa de `-DWOTM_NATIVE_DIR=<WoTM>/native` e do FFmpeg (`PS2X_ENABLE_FFMPEG`, padrão no Windows); sem eles, o `PS2X_FMV` só avisa. Aplicar: `git apply --whitespace=nowarn tools/runtime_patch/wotm-fmv.patch` dentro de `third_party/PS2Recomp`.
