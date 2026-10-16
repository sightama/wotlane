# Ferramentas, headers compartilhados e teste diferencial

(Dividido de `docs/PROXIMOS_PASSOS.md`; índice lá.)

## Ferramentas (tools/)
- `wsl/gate.sh` (build + SHA1, sai com erro se falhar), `wsl/scoreall.sh` (pontuação por função com NON_MATCHING), `wsl/convert_tus.sh`, `wsl/check.sh`,
  `ccmatch.py`, `nm_wrap.py`, `new_tu.py`, `place_data.py`, `progress.py` (escreve `config/status.csv`), `callgraph.py`, `m2c.sh`, `wsl/variants.py`, `wsl/permute.py`.
- Verificação por TU num comando (2026-10-15): `~/.venvs/wotm/bin/python tools/verify_tu.py game/<TU> [...] [--runs N] [--no-nm] [-v]` roda a pontuação (`ccmatch`), o `difftest_all`, o `nm_retvals` e, salvo com `--no-nm`, um `build_nm.sh verify` só com essas TUs para o `nm_calls` e o `nm_audit`; imprime uma tabela por função (score, difftest, chamadas, layout, retorno) e sai com erro se alguma equivalente difere no difftest, nas chamadas ou tem retorno `void` consumido. Com o ELF leva ~4,5 min (o `build_nm`); sem, segundos. `sh tools/m2c_batch.sh [csv | tu...]` gera os rascunhos do m2c de toda a fila (`config/boot_coverage_asm.csv` por padrão) em `build/m2c/<tu>/<função>.c`, em paralelo e só o que falta.
- Camada nativa e runtime: `native_syms.py` (`--gen` grava `native/wotm_syms.h` com `wotm::sym::<dado>` e `wotm::fn::<função>` a partir de `symbol_addrs.txt`; `--audit` lista os literais hex de `native/` que batem com um símbolo, caem dentro de um ou não casam; `--apply` troca os exatos por nome, só no código, e inclui o header onde faltar), `vu1cmp.ps1` (checagem VU1 x nativo e imagens a partir de instantâneos), `corpus.ps1` (corpus de regressão: `-Capture` grava menus e lutas em `<runtime>\corpus`, `-Check -Baseline <csv>` diz o que piorou depois de uma mudança em `native/`), `build_runtime.ps1` (ou `build_runtime.bat` na raiz), `lote.ps1`, `wotm_env.py` (confere a tabela de variáveis `WOTM_*` do `NATIVO.md` contra o código), `vu0native.py`, `vu1dis.py`, `fx.py`.
- Scripts `.sh` devem ser criados por heredoc no bash; gravar com Python no Windows deixa CRLF e o `sh` do WSL quebra (`sed -i 's/\r$//'` conserta).
- README automático: `tools/update_readme.py` (só stdlib, lê `config/status.csv`) reescreve o bloco entre `<!-- PROGRESS:START -->` e `<!-- PROGRESS:END -->` do `README.md` e o `docs/progress.svg`. Hook versionado `.githooks/pre-commit` roda isso e dá `git add`; ativar uma vez por clone com `git config core.hooksPath .githooks`. O hook não roda `progress.py` (leva ~12 s via WSL e precisa de `disc/`): rodar `python3 tools/progress.py` antes de commitar, ou `WOTM_REFRESH=1 git commit ...` para o hook atualizar o `status.csv`. Nunca bloqueia o commit.
- Mod `/wotm` (HUD) carrega com `startup_command.bat` (no `.gitignore`), que inicia o Claude com `--plugin-dir ~/.claude/my-plugins/wotm-hud`.

## Headers compartilhados (include/)
Regra: uma classe/API usada por mais de um TU mora num header; não redeclarar parcialmente dentro do `.cpp`.
- `engine.h`: API do motor (matemática, timers, `_animHandle`/animation*, particleKillFx, hier/hd). `game/game.h` inclui.
- `game/game.h`: `Monster` (0x11190 bytes: `m_cs`, `m_playerNum`, `m_id`, `m_health`, `m_stamina`, `m_target`, `m_camUnify`), `TheGame`
  (`m_huds[4]`, `m_slots[16]` = os `Monster` em 0xB80, `m_monsters[]` = ponteiros, `m_gravity`, `m_gameMode`, `m_matchMode`, `m_levelId` (1 central, 2 vegas, 3 canyon2,
  5 airport, 6 threemile, 7 sanfran, 8/15 island, 9 tokyo, 10 ufo, 11 final boss, 26 bigshot, 27 crush), `m_numSlots`, `m_numMonsters`, `m_levelIdx`, `m_playerMask`),
  `Cameras` (`m_cameras`, `m_state`), acessores `gameSlotBase(idx)` (mantém a ordem `idx*0x11190 + 0xB80`), `gameHud(i)`, `gameWeapons()`.
- `point_tool_kit.h` (base de PathTool/PowerUpTool/StartPointTool/AiPathTool; derivados chamam `PointToolKit::init/loadPoints/getPoint`), `task_manager.h`, `bidir_link.h`,
  `cs_pool.h` (tudo estático), `game/{shell,hit_history,pickup,military_pickup,pickup_sound,vehicle_navigator,hud,weapons,power_ups,start_points,stamina_meter,crush_level,levels,streaming_sound}.h`.
- Mover uma classe para header pode mudar `sizeof` e deslocar campos de structs parciais que a embutem (aconteceu com `StaminaMeter`): sempre comparar antes/depois.
- Funções que o retail chama com `this` mesmo sem usá-lo (ex.: `StartPoints::getNumPoints`) só batem se declaradas não-estáticas; `isThisTypeFull` é estática.
- Ainda não unificados: `GamePad` (`GamePad.cpp` vê 6 ints, `GamePadClipPlayer.cpp` vê bytes), `GamePadClipPlayer`/`AiPadClips` (tipo do clipe diverge entre `AiGrapple` e os outros),
  `Ai`, `Destructibles`.

## Estado da decomp em `TheGame` (fonte: `config/status.csv`)
- `src/game/TheGame.cpp` convertido. Equivalentes: `Update`, `InitAfterDbLoad`, `ResetLevel`, `UpdatePadTweaks`, `gameResolveLifeAndDeath`; matched: `Update2`, `SetGravity`, `SetOkToUnify`, `GetNumAIsAlive`. Ainda em asm: `InitBeforeDbLoad` e `gameCheckForCloseCombat`.
- A decomp só avança na superfície de mod (direção de 2026-10-16): `Shell::Load*`, `MonsterParse`, câmera. Para o que falta em cada TU, `grep -c INCLUDE_ASM` ou o `status.csv`.

## Teste diferencial (tools/difftest.py, tools/difftest_all.py)
Roda a função retail e a nossa (compilada com `-DNON_MATCHING`) num emulador MIPS (unicorn), com o mesmo estado aleatório, e compara retorno, chamadas a outras funções
(os callees viram `jr ra`; nome + argumentos relevantes, com strings comparadas pelo texto e ponteiros de função pelo nome), bytes alterados na arena e nos dados do retail e as variáveis
que o próprio TU define (alias para a cópia do retail). Serve para validar as funções "equivalentes (untuned)" sem PCSX2.
- Uma função: `~/.venvs/wotm/bin/python tools/difftest.py game/Monster isFullHealth__7Monster --args this --ret int --runs 60 [--objsize 0x122000]`
  (`--args` aceita `this,p,i,b,f`; `--alt OUTRAFUNC` é o controle negativo, deve dar MISMATCH; `--objsize` mantém ponteiros aleatórios fora do objeto, use 0x122000 para `TheGame`).
- Todas as equivalentes de TUs: `python3 tools/difftest_all.py game/Monster game/TheGame ... --runs 30` (adivinha a assinatura pelo nome mangled e o retorno pela definição).
- Resultado de 2026-10-07: ~95 funções equivalentes concordam; o teste achou e corrigimos 7 erros reais (Vehicle::setCs/setPos, StaminaMeter::credit, UpdatePadTweaks, TheGame::Update (2º TaskManager),
  MonkeyChains::init (FxTextureId é enum), AnimContact).
- Limites: não executa VU0/COP2 nem `min.s`/`madd.s`; funções com métodos virtuais ou estruturas encadeadas válidas são puladas ("skipped"); mais de 4 args inteiros ou 2 floats não são suportados;
  é evidência (só os caminhos que o estado aleatório alcança), não prova. Rodar em toda função equivalente nova antes de commitar.
- Armadilha do emulador: hook de escrita de memória quebra saltos com store no delay slot; por isso comparamos imagens de memória no fim, não escritas.

