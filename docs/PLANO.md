# Arquitetura e plano (2026-10-09)

Objetivo: um **port nativo de PC fácil de modar** de War of the Monsters (PS2, NTSC-U). Este documento diz o que cada peça faz, o que mora no repo e o que se gera localmente.
Estado verificado em 2026-10-09; o que ainda não foi feito está marcado.

## As camadas
1. **Base: o jogo recompilado.** O [PS2Recomp](https://github.com/ran-j/PS2Recomp) traduz o executável inteiro (5.382 funções, 0 falhas de decodificação, VU0 incluso) para C++ literal
   (`ctx->r4 = ADD32(ctx->r4, 0x20)`), uma função por função MIPS, sobre uma RAM de PS2 emulada. É ilegível, mas fiel: lógica, classes e libs. Ninguém precisa ler isso.
2. **Runtime (hardware falso).** Separado do código recompilado, fornece o que o PS2 tinha em hardware: GS e VU1 em software, IOP/som, IPU/filmes, controle, disco.
   O do PS2Recomp serve como **ponte** para testar: hoje o jogo chega ao menu, mas o VU1 interpretado executa microcódigo inválido e o filme do menu não aparece.
3. **Camada nativa (o que escrevemos).** Substitui o hardware falso por código de PC: desenho com GPU a partir da hierarquia de cena (`hierTraverseAsm` e seus 7 chamadores em `common/hier`),
   controle, depois som e FMVs. Este é o trabalho do port.
4. **Camada legível (decomp).** `src/` + `include/`: funções `matched`/`equivalent` em C++ legível. **Não entra no port base.** Serve para (a) modar: pega-se a versão legível da função, altera-se e
   rebuilda-se no ELF; (b) o oracle; (c) estruturas e formatos que a camada nativa precisa (`include/hieri_types.h`, `docs/FORMATOS.md`).

## O que vai no repo e o que se gera
- **No repo:** a decomp legível, os headers de tipos, `tools/`, a camada nativa, os hooks, a documentação.
- **Nunca no repo (regra 1 do `CLAUDE.md`):** o executável, a ISO, o assembly e **o C++ gerado pelo PS2Recomp** (é código do jogo). Cada pessoa o gera a partir da sua ISO.
- **Layout:** o PS2Recomp é um **submódulo** (`third_party/PS2Recomp`, branch `wotm` do fork `yanm1103/PS2Recomp`, com os nossos patches e a correção do `sqrt`). Clone com `git clone --recurse-submodules`.
  O código gerado fica em `recomp/` (no `.gitignore`): `recomp/retail/` (ELF retail) e `recomp/nm/` (ELF com as equivalentes). O runtime lê essa pasta com `-DPS2X_GENERATED_DIR=<repo>/recomp/retail`,
  sem copiar nada para dentro do submódulo.
- **Ainda manual:** gerar `recomp/` (analisador + `ps2_recomp` sobre o ELF) e compilar o runtime (MSVC no Windows). *Falta:* um script único que faça isso a partir da ISO.
  Os hooks específicos do jogo e a travessia de cena moram em `native/runtime/` neste repo desde 2026-10-15 (`wotm_scene.inc`, `wotm_live.inc`, `wotm_overrides.cpp`...); o fork só tem as costuras (`tools/runtime_patch/README.md`).
- O recompilador precisa da correção de `SQRT.S`/`RSQRT.S` (PR https://github.com/ran-j/PS2Recomp/pull/277); sem ela, 108 + 76 funções do jogo calculam errado.

## Direitos autorais e licença
- O C++ que o PS2Recomp gera é uma **tradução mecânica do código do jogo**, portanto obra derivada protegida: não pode ser publicado. É o motivo do modelo acima (cada pessoa gera a partir da própria ISO).
- A decomp legível (`src/`) também deriva do código original. Projetos de decompilação costumam publicá-la sem assets e isso é tolerado na prática, mas é zona cinzenta. Não é aconselhamento jurídico.
- Substituir funções **não** torna publicável o que sobra do output do PS2Recomp: o que ainda não foi reescrito continua sendo o código original traduzido.
- O código **deste repositório** é GPL-3.0 (`LICENSE`), a mesma licença do PS2Recomp. A licença não cobre o jogo. Se o port distribuído usar o runtime do PS2Recomp, o conjunto fica sob GPL;
  escrever a nossa própria camada de plataforma (marcos 3 a 5) é o que deixaria o projeto livre para mudar de licença no futuro.

## Fluxo de um mod
1. Escolher a função (por exemplo, uma IA ou uma regra de fase) e abrir a versão legível (`matched` ou `equivalent`).
2. Alterar o comportamento (um mod **não** dá byte match, por definição).
3. Gerar o jogo traduzido com as reescritas no lugar das originais: `sh tools/wsl/recomp_nm.sh` (WSL: `build_nm.sh nmall` + PS2Recomp → `recomp/nm/`), depois
   `build_runtime.bat nm` (Windows: `build_dev_nm`, o runtime "cobaia", mesmos hooks) e `run_live.bat -Build build_dev_nm` para jogar.
4. Conferir que **só** o que se queria mudou: `tools\lote.ps1 -Build build_dev_nm` com o mesmo lote (mesma `-Seed`) do runtime normal e comparar os `resumo.csv`;
   para uma função isolada, `tools/recomp_oracle` compara a versão original com a nova. Equivalentes que não linkam no ELF completo ficam listadas em `config/nm_all_exclude.txt`.

## Marcos
| # | Marco | Estado |
|---|---|---|
| 0 | Jogo inteiro recompilado, compila e roda no runtime até o menu | feito |
| 1 | Filme de fundo do menu (FFmpeg) | nativo: `native/wotm_fmv.hpp` troca `CMovie`/`playMpegMovieWithDB` e toca o `.PSS` (`PS2X_FMV=1`); testado fora do jogo, falta conferir no runtime |
| 2 | Travessia nativa da hierarquia de cena (`world`, céu, listas de `Cs`) | leitura funciona; o menu não usa a hierarquia 3D; precisa de uma fase para ter conteúdo |
| 3 | Chegar a uma fase (entrada por roteiro) e desenhar objetos e modelos nativos | em andamento |
| 4 | Partículas, fonte, `ScreenPolys`, luz, névoa | |
| 5 | Controle completo, som, FMVs | FMVs: ver o marco 1 |
| 6 | Mods de exemplo com verificação pelo oracle | |

## Armadilhas já pagas
- `sqrtf` em C++ NM sai com `sqrt.s` na forma errada: use `eeSqrtf` (`include/vecmath.h`).
- `TakeScreenshot` do raylib sai vazio se o lote de desenho não for descarregado antes (`rlDrawRenderBatchActive`).
- O VU1 do runtime do PS2Recomp roda microcódigo inválido neste jogo; por isso cortamos na hierarquia de cena, não no VU1.
- `pktAddVu1ObjAsm`, `hierTraverseAsm` e `hierCsUpdateAsm` são assembly manuscrito com convenção própria: não decompilar, substituir.
