# Arquitetura e plano (decidido em 2026-10-09; estado em 2026-10-16)

Objetivo: um **port nativo de PC fácil de modar** de War of the Monsters (PS2, NTSC-U). Este documento diz o que cada peça faz, o que mora no repo e o que se gera localmente.
Direção confirmada em 2026-10-16: **port primeiro**; a decomp só avança na superfície de mod.

## As camadas
1. **Base: o jogo recompilado.** O [PS2Recomp](https://github.com/ran-j/PS2Recomp) traduz o executável inteiro (5.382 funções, 0 falhas de decodificação, VU0 incluso) para C++ literal
   (`ctx->r4 = ADD32(ctx->r4, 0x20)`), uma função por função MIPS, sobre uma RAM de PS2 emulada. É ilegível, mas fiel: lógica, classes e libs. Ninguém precisa ler isso.
2. **Runtime (hardware falso).** Separado do código recompilado, fornece o que o PS2 tinha em hardware: GS e VU1 em software, IOP/som, IPU/filmes, controle, disco.
   No port, o VU1 e o GS emulados ficam desligados (`PS2X_NO_VU1`, `PS2X_NO_GS`) e o desenho é da camada nativa. O VU1 emulado continua útil como **referência**: a checagem VU1 x nativo
   (`docs/passos/VU1CHECK.md`) compara, por triângulo, o que ele mandaria ao GS com o que o nativo desenha (mais de 99% casado na geometria).
3. **Camada nativa (o que escrevemos).** `native/` neste repo: desenho com GPU (raylib) a partir da hierarquia de cena lida da RAM do EE, partículas, sombras, neblina, texto do menu,
   som (`wotm_snd.hpp`), filmes (`wotm_fmv.hpp`), teclado, VU0 nativo gerado do microcódigo. Os hooks que rodam dentro do runtime moram em `native/runtime/`. Este é o trabalho do port
   (`docs/passos/NATIVO.md`).
4. **Camada legível (decomp).** `src/` + `include/`: funções `matched`/`equivalent` em C++ legível. **Não entra no port base.** Serve para (a) modar: pega-se a versão legível da função, altera-se e
   rebuilda-se no ELF; (b) o oracle; (c) estruturas e formatos que a camada nativa precisa (`include/hieri_types.h`, `docs/FORMATOS.md`).

## O que vai no repo e o que se gera
- **No repo:** a decomp legível, os headers de tipos, `tools/`, a camada nativa (`native/`, inclusive `native/runtime/`), a documentação.
- **Nunca no repo (regra 1 do `CLAUDE.md`):** o executável, a ISO, o assembly, **o C++ gerado pelo PS2Recomp** (é código do jogo) e o `.inc` do VU0 nativo (tradução do microcódigo). Cada pessoa os gera a partir da sua ISO.
- **Layout:** o PS2Recomp é um **submódulo** (`third_party/PS2Recomp`, branch `wotm` do fork `yanm1103/PS2Recomp`, com a correção do `sqrt`). Clone com `git clone --recurse-submodules`.
  O código gerado fica em `recomp/` (no `.gitignore`): `recomp/retail/` (ELF retail) e `recomp/nm/` (ELF com as equivalentes, runtime cobaia). O runtime lê essa pasta com `-DPS2X_GENERATED_DIR=<repo>/recomp/retail`.
  O CMake do fork recebe `-DWOTM_NATIVE_DIR=<repo>/native` e compila `native/runtime/`; no patch `tools/runtime_patch/wotm-runtime.patch` ficam só as costuras (CMake, `main.cpp`, taps no VIF1/VU1/memória, escalonador, pad).
- **Build do runtime:** `build_runtime.bat` na raiz (sem LTCG; `docs/passos/RUNTIME.md`). *Ainda manual:* gerar `recomp/` (analisador + `ps2_recomp` sobre o ELF). *Falta:* um script único que faça isso a partir da ISO.
- O recompilador precisa da correção de `SQRT.S`/`RSQRT.S` (PR https://github.com/ran-j/PS2Recomp/pull/277); sem ela, 108 + 76 funções do jogo calculam errado.

## Direitos autorais e licença
- O C++ que o PS2Recomp gera é uma **tradução mecânica do código do jogo**, portanto obra derivada protegida: não pode ser publicado. É o motivo do modelo acima (cada pessoa gera a partir da própria ISO).
- A decomp legível (`src/`) também deriva do código original. Projetos de decompilação costumam publicá-la sem assets e isso é tolerado na prática, mas é zona cinzenta. Não é aconselhamento jurídico.
- Substituir funções **não** torna publicável o que sobra do output do PS2Recomp: o que ainda não foi reescrito continua sendo o código original traduzido.
- O código **deste repositório** é GPL-3.0 (`LICENSE`), a mesma licença do PS2Recomp. A licença não cobre o jogo. Se o port distribuído usar o runtime do PS2Recomp, o conjunto fica sob GPL;
  escrever a nossa própria camada de plataforma é o que deixaria o projeto livre para mudar de licença no futuro.

## Fluxo de um mod
1. Escolher a função (por exemplo, uma IA ou uma regra de fase) e abrir a versão legível (`matched` ou `equivalent`).
2. Alterar o comportamento (um mod **não** dá byte match, por definição).
3. Gerar o jogo traduzido com as reescritas no lugar das originais: `sh tools/wsl/recomp_nm.sh` (WSL: `build_nm.sh nmall` + PS2Recomp → `recomp/nm/`), depois
   `build_runtime.bat nm` (Windows: `build_dev_nm`, o runtime "cobaia", mesmos hooks) e `run_live.bat -Build build_dev_nm` para jogar.
4. Conferir que **só** o que se queria mudou: `tools\lote.ps1 -Build build_dev_nm` com o mesmo lote (mesma `-Seed`) do runtime normal e comparar os `resumo.csv`;
   para uma função isolada, `tools/recomp_oracle` compara a versão original com a nova. Equivalentes que não linkam no ELF completo ficam listadas em `config/nm_all_exclude.txt`.
   **Estado:** a cadeia existe, mas o cobaia ainda quebra na luta (`docs/passos/NATIVO.md`, "Runtime cobaia"); está parado até o port ter HUD e widescreen.

## Marcos
| # | Marco | Estado (2026-10-16) |
|---|---|---|
| 0 | Jogo inteiro recompilado, compila e roda no runtime até o menu | feito |
| 1 | Filmes (FFmpeg) | feito: `native/wotm_fmv.hpp` toca os `.PSS` de tela cheia e o telão do menu; falta sincronizar o áudio com o relógio do jogo |
| 2 | Travessia nativa da hierarquia de cena (`world`, céu, listas de `Cs`) | feito: rígidos, skinned, LOD, destrutíveis, prédios fantasma, mapa de ambiente, luz paralela, neblina |
| 3 | Chegar a uma fase e desenhar objetos e modelos nativos | feito: `WOTM_MATCH` entra direto na luta; geometria 99%+ casada com o VU1 |
| 4 | Partículas, fonte, `ScreenPolys`, luz, névoa | feito, menos o HUD (PATH2/PATH3: barras de vida, medidores, retratos) e os fades de tela |
| 5 | Controle completo, som, FMVs | teclado e som nativos feitos (`wotm_snd.hpp`); gamepad e remapeamento ficam para depois |
| 6 | Widescreen (primeiro mod), HUD, fades, iluminação e bugs gráficos na ordem do inventário | próximo (`VU1CHECK.md`, "Inventário do port") |
| 7 | Mods de exemplo com verificação pelo oracle e pelo cobaia | depois do 6 |

## Armadilhas já pagas
- `sqrtf` em C++ NM sai com `sqrt.s` na forma errada: use `eeSqrtf` (`include/vecmath.h`).
- `TakeScreenshot` do raylib sai vazio se o lote de desenho não for descarregado antes (`rlDrawRenderBatchActive`).
- O VU1 emulado é lento demais para desenhar (a luta caía a ~15 vsync/s) e, sem VU1, o jogo precisa que alguém zere `objsInPacket`/`objsInAlphaPacket`; por isso cortamos na hierarquia de cena e deixamos o VU1 só como referência da checagem.
- `pktAddVu1ObjAsm`, `hierTraverseAsm` e `hierCsUpdateAsm` são assembly manuscrito com convenção própria: não decompilar, substituir.
- Hook de host sem `hostLeaf` congela a lógica quando a fatia de tempo acaba dentro dele (`NATIVO.md`, "hooks de host").
