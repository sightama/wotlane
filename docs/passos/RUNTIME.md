# Runtime recompilado (PS2Recomp): como rodar e o que saber

(Índice dos docs no `CLAUDE.md`.) Este arquivo guarda o que se usa hoje. O diário das investigações de desempenho (IOP em lotes, timers adiados, relógio virtual, VU0 nativo) está em `NATIVO.md` ("Física com `dt`") e no histórico do git (`git log -- docs/passos/RUNTIME.md`).

## Onde está o código
- Clone do runtime (fora do repo): `C:\Users\TwistZero\wotm-recomp-win` (submódulo `third_party/PS2Recomp`, branch `wotm` do fork `yanm1103/PS2Recomp`). O código gerado (`register_functions.cpp` etc.) nunca vai ao repo.
- O código nosso que roda dentro do runtime está em **`native/runtime/`** neste repo (`NATIVO.md`, "Onde mora o código do runtime"); o fork só tem as costuras (`tools/runtime_patch/README.md`).

## Build rápido (sem LTCG)
O release do PS2Recomp liga `/GL` + `/LTCG` e o link leva ~28 min a cada mudança. Com `-DPS2X_FAST_LINK=ON` (branch `wotm`) o jogo continua a 60 fps; reconstrução incremental em ~13 s, do zero em ~4 min.
- **Use sempre `build_runtime.bat`** na raiz do repo (ou `tools/build_runtime.ps1`): configura `build_dev` com `-DPS2X_FAST_LINK=ON -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF -DPS2X_ENABLE_IOP_RPC_TRACE=OFF`, toca o `ps2_runtime.cpp` (o ninja não rastreia `native/`), gera o `.inc` do VU0 nativo quando falta e compila. Saem **dois executáveis** em `build_dev\ps2xRuntime\`, dos mesmos objetos do jogo traduzido: `wotm.exe` (o jogo, sem GS/VU1/VIF1 emulados nem checagem) e `ps2EntryRunner.exe` (teste: emulados, `WOTM_VU1CHECK`, `WOTM_FOTOS`, `--vu1cmp`; o que `corpus.ps1` e `vu1cmp.ps1` usam). O `build_dev.bat` do clone compila os dois (`--target ps2EntryRunner wotm`). Detalhes em `FRONTEIRA.md`, passo 5. `build_runtime.bat nm` faz o mesmo para o runtime cobaia (`build_dev_nm`).
- **Logs agressivos têm que ficar OFF.** `PS2X_ENABLE_AGRESSIVE_LOGS` vem `ON` no upstream: toda função gerada grava entrada/saída com flush (295 MB em 45 s) e o boot não passa do Press Start.
- Build linka com `/MAP` e `/DEBUG` (PDB só com os símbolos públicos, sem `/Zi`; `/OPT:REF /OPT:ICF` mantêm o binário; `cmake/ReleaseMode.cmake`, `PS2X_FAST_LINK`). O `PS2X_HOSTPROF` resolve os nomes pelo PDB; para localizar um `[CRASH] exe+0x…` sem ele, procure o maior símbolo ≤ o endereço no `ps2EntryRunner.map`.

## Rodar
- `run_live.bat` (ou `run_live.ps1` no clone: `-Auto 0` para jogar, `-Stats 1` loga `[native]`, `-Build build_dev_nm` para o cobaia, `-Exe test` força o de teste). Roda o `wotm.exe`, salvo com `WOTM_VU1CHECK` ou `WOTM_FOTOS` ligados (o `run_live.bat` liga as fotos por padrão; o launcher não), com `-Exe test` ou quando o `wotm.exe` falta ou é mais velho: aí o `ps2EntryRunner.exe`. Põe `PS2X_NO_VU1=1 PS2X_NO_VIF1=1 PS2X_NO_GS=1 PS2X_SCENE=1 PS2X_FMV=1` (VU1 e GS emulados desligados no de teste, desenho e filmes nativos; o `wotm.exe` liga os três sozinho). Usa `Start-Process -RedirectStandardError` de propósito (o `2>` do PowerShell 5 grava UTF-16). Logs em `<runtime>\logs\` (`live.err.log`/`live.out.log`; o build grava `build_dev.log` ali e o `live_attack.ps1`, `atk.*.log`); a raiz do runtime fica só com scripts e pastas.
- Partida direta sem menu: `WOTM_MATCH=<jogador>[,<ia>...],<fase>` (0 a 7 IAs; sem IA = sozinho na fase); IA contra IA: `WOTM_AIVSAI=1`; lote: `tools\lote.ps1` (tudo em `NATIVO.md`).
- Outras `PS2X_*` úteis: `PS2X_PAD` (roteiro de botões com tempos), `PS2X_DUMP_RAM="3300,3900"` (grava `ram_<q>.bin`/`vram_<q>.bin` para o viewer), `PS2X_CD_IMAGE` (ISO), `PS2X_PEEK="hex,hex"` (palavras da RDRAM no log), `PS2X_HOSTPROF=1` (amostrador de RIP do host, todas as threads a cada ~1 ms, `hostprof.txt`; `=2` só a thread do EE; `=3` só a thread do EE e só quando o quadro dela passa de 100 ms, acordando a cada 50 ms; com `WOTM_FIELDLOG`, quadro de mais de 100 ms sai como `[hostprof]` com a cadeia de chamadas aproximada), `PS2X_COVERAGE` (cobertura de funções), `PS2X_IOP_BATCH` (padrão 256).
- **Teclado** (backend em `ps2_pad.cpp`, soma ao gamepad; mapa padrão, trocável na aba CONTROLS do "Recompiled options", `NATIVO.md`, "Rebind"): **setas = analógico esquerdo**, **I/J/K/L = D-pad**, **Espaço = X**, **Z = quadrado**, **X = triângulo**, **S = bola**, **Enter = Start**, **Backspace = Select**, **Q = L1**, **W = R1**, **Shift = L1+R1**, **A = L2**, **C = R2**. Camada nativa: **F3** FPS, **F7** pausa e continua, **F6** (pausado) avança um quadro do jogo, **F8** tira uma foto com nota (com `WOTM_FOTOS`; só com `WOTM_VU1CHECK`, grava e compara o próximo quadro). Fluxo em `VU1CHECK.md`, "Fotos".

## Rastro do boot (tela preta antes do filme)
Da abertura do processo ao primeiro filme, o runtime escreve cada etapa no stderr com o tempo desde o início e mostra a etapa na tela preta ("Loading... 3.2 s"). Sempre ligado; para quando o boot termina. Etapa parada há mais de 2 s repete como `... ainda em "<etapa>" ha N s (PC do EE 0x..., vsync N, R/s)`: mapeie o PC com `config/status.csv`. Medido: janela 0,6 s; módulos do IOP 0,8 s; depois o jogo fica ~3,5 s ocioso contando vsync a 60/s (espera do próprio jogo, cresce se o vsync cair); filme em 4,4 s. Código: `wotmBoot*` em `wotm_live.inc`.

## Endereços úteis (menu)
`currScreen` 0x6F8464 (0x14 = Press Start, 1 = menu e também dentro da fase, 0x63 = carregamento), `nextScreen` 0x6F846C, `betweenScreens` 0x6F7E8C, `screenFirstPass` 0x6F7E80, `targetAlpha` 0x6F7E74, `fadingIn/Out` 0x6F807C/0x6F8080, `world` 0x6F87C4, `g_frame` 0x6F7E38. Em código novo use `wotm::sym::<nome>` (`native/wotm_syms.h`).

## Pendências
- `PS2X_FAST_BOOT` (pular as esperas de abertura sem pular a inicialização).
- O áudio dos filmes segue o relógio de parede, não o do jogo.
- Travadas de host de 100 a 330 ms fora da luta (`_NLG_Return2`, desenrolar de pilha do MSVC) ainda sem causa.
