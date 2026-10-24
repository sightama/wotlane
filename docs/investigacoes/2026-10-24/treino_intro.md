# Modo treino: pular a cena de apresentação (intro) da luta

Data: 2026-10-24. Pedido: no modo treino, pular a cena de cinema que mostra os monstros e a fase antes da luta. Só leitura (src/, asm/,
native/); nada foi mudado no código.

## 1. Quem dispara e como termina

**Disparo: script da fase, carregado com o banco de dados.** `dbProcInteractive` (0x12E5xx, `src/game/db.cpp`, faixa de ids
0x2134..0x2327), com `g_noScripts == 0`:
- ep node **0x2134** com `m_gameMode` (TheGame +0x1203C8) **2**, **0x2135** com modo **3**, **0x2136** com modo **1** →
  `ActionDispatch::addAction(raiz do script, matriz, posição, 0)`. É a intro: uma árvore de `ActHead` (grupo com animações dos
  monstros `ACTANIM_PLAYER`, `ACT_PLAYERPOS`, som, e um `ACT_CINEMA` com a duração em fields).
- ids 0x2260..0x2327 só são registrados (`addUserDefinedScipt`), para `callScriptFromId` depois (chefes).

O treino é o free-for-all de 1 jogador (modo 2, `treino_menu_fluxo.md`), então a intro dele é sempre o nó 0x2134 da fase.

`addAction` põe um `DelayAction` (pool de 64) no `TaskManager` da TU (`s_actionTasks`, 0x6F9080, que é ele mesmo uma tarefa do
`TaskManager::global`, `initBefore`) com `delay * 60` fields. Quando chega a vez, `DelayAction::doIt` (0x100FA8) despacha pelo tipo; o
`ACT_CINEMA` (tipo 9) vai para **`doCinema(DelayAction*, ActCinema*)`** (0x100940):
- `moviePlaying = 1` (0x6F7900), `movieAborted = 0` (0x6F7904);
- letterbox: `Hud::m_letterbox` (0x2A3000) +0x28 = `flags & 1`, `Letterboxing::setEnabled(true)`;
- agenda `DelayAction::endIt` em `s_actionTasks` após `length` fields; se `timerGetFieldCount() < 60` ou `s_fadeOut`, agenda também
  `DelayAction::fade` (`fadeOutAndIn(4)`) em `length - 30`;
- `startCinema` de cada `m_slots[i]` (Monster: blends a 0,5, estado +0x7984, e fora do modo 1 a animação `m_anims[0x5A]`);
- `game->m_playerMask ^= 7` (os monstros passam a rodar `updateCinema` em vez de `update` no `TheGame::Update`);
- `PowerUps::startCinema`, `PedGroup::startCinema`, `LevelPickups::startCinema`, `hierSetLosPointsPerView(0, 0)`,
  `Cameras::m_cameras[0]` +0xD90 = 0;
- com 2+ monstros `viewToggleSplitScreen(true, false)` (cena em tela cheia);
- `shell->m_inCinema` (+0x2BA4) = 1; `StreamingSoundManager::startCinemaMusic`.

**Fim:** o `endIt` agendado (`DelayAction::endIt`, 0x101150) chama **`endCinema`** (0x100B20). Dois caminhos:
- **normal** (`s_endingActions == 0`): `Monster::endCinema` de cada slot (`enterNewState(+0x7984)`), `moviePlaying = 0`,
  `movieAborted = 0`, letterbox +0x28 = `(flags >> 1) & 1` e `setEnabled(false)`, `m_playerMask |= 7`, `PowerUps::endCinema`,
  `PedGroup::endCinema`, `LevelPickups::endCinema`; com 2+ monstros `viewToggleSplitScreen` de novo e câmera 1 +0xD98/+0xD90 = 1;
  câmera 0 +0xD98/+0xD90 = 1 (câmera recomeça atrás do monstro); `m_inCinema = 0`; `stopCinemaMusic` (volta a música da luta).
- **durante um `stopAllActiveActions`** (`s_endingActions == 1`): só `moviePlaying = 0`, `m_inCinema = 0`, e reagenda o próprio
  `endIt` no `TaskManager::global` com 4 fields. Esse segundo `endIt` roda fora do flush e faz o caminho normal inteiro.

## 2. O jogo já tem "pular": START durante o cinema

`Monster::updateCinema` (0x15BA10; chamado pelo `TheGame::Update` para cada monstro enquanto o `m_playerMask` está invertido), depois
de animar: se `moviePlaying != 0` e (`inputGetInput(8, 0)` ou `inputGetInput(8, 1)`: máscara 8 = START, portas 0 e 1) e
`movieAborted == 0` e `game +0x120454 == 0` e `game +0x120458 == 0` (nenhum fade do `TheGame` em curso):
1. `movieAborted = 1`;
2. `TheGame::fadeOutAndIn(game, 4)` (0x139750);
3. `TaskManager::global.add(ActionDispatch::stopAllActiveActions, 30)` (`add__11TaskManagerPFv_Uii`, 0x2220B0; global em 0x6E0718).

30 fields depois (tela já escura), `stopAllActiveActions` (0x100100) liga `s_endingActions`, executa e apaga **todas** as tarefas de
`s_actionTasks` (o `doIt` no modo "fim": `ACT_PLAYERPOS` ainda põe os monstros na posição, animações pulam para o último quadro
menos 3, sons e cinemas novos não tocam), desliga a marca e zera `moviePlaying`/`movieAborted`. O `endIt` do cinema cai no caminho
"durante o flush" e 4 fields depois o `endCinema` normal fecha tudo; o fade volta a clarear com a luta pronta. O `rtPauseRT` ignora o
START enquanto `moviePlaying` (a pausa não abre junto). O `FinalBoss`/`PlantBoss` leem `movieAborted` para cortar as cenas deles.

Ou seja: hoje, no treino, apertar START (ou a tecla do START) já pula a intro. Nada do treino bloqueia isso: o `inputHook` do
`wotm_pausemenu.inc` só age dentro do overlay da pausa.

## 3. Como pular sozinho a partir do runtime

**Recomendado: refazer o "pular" do próprio jogo, no mesmo lugar em que ele roda.** Gancho em `wotm::fn::updateCinema__7Monster`
(0x15BA10; nenhum gancho do `native/` mexe nele hoje), instalado na lista do `wotmTrainingInstall`:
1. chamar o original (`g_origUpdateCinema`);
2. se `g_on` e a intro está armada (ver item 4) e `moviePlaying != 0` e `movieAborted == 0` e `game +0x120454 == 0` e
   `game +0x120458 == 0`:
   - `FAST_WRITE32(wotm::sym::movieAborted, 1)`;
   - `callGuest(rdram, ctx, runtime, wotm::fn::fadeOutAndIn__7TheGamei, rd32(game), 4)`;
   - `callGuest(rdram, ctx, runtime, wotm::fn::add__11TaskManagerPFv_Uii, wotm::sym::x_11TaskManager_global,
     wotm::fn::stopAllActiveActions__14ActionDispatchv, 30)`;
   - desarmar e logar `[treino] intro pulada`.

Por que assim: é exatamente o caminho que o START já percorre, na mesma thread e no mesmo ponto do quadro (dentro do
`TheGame::Update`, fora da iteração de `s_actionTasks`). O jogo cuida de tudo que o `endCinema` faz (letterbox, `m_playerMask`,
estados dos monstros, PowerUps/PedGroup/pickups, câmeras +0xD90/+0xD98, tela dividida, `m_inCinema`, música do cinema → música da
luta) e o flush aplica as ações da intro que ainda faltavam (posição inicial dos monstros, animações no fim), então os monstros
ficam onde a intro os deixaria. Custa ~0,5 s de fade escuro em vez da cena inteira. Pode baixar o 30 (ex. 8-10) para ficar mais
rápido; com valores muito baixos o corte aparece antes de a tela escurecer (só estético).

Variante mínima equivalente: no mesmo gancho, fingir o START (gancho no `inputGetInput` com `ra` dentro do `updateCinema`) — funciona,
mas é mais frágil que escrever os três passos.

**Não recomendado:**
- **Não chamar o `doCinema`** e fazer à mão o `endCinema`: o resto da árvore da intro (animações dos monstros `ACTANIM_PLAYER`,
  `ACT_PLAYERPOS`, sons, outros grupos com atraso) continua agendado e roda com o jogador já no controle (monstro preso em
  animação de apresentação, teleporte no meio da luta). Além disso o `DelayAction` ficaria com `m_active` = 0 e a música/letterbox
  dependeriam de nada ter sido ligado.
- **Chamar `stopAllActiveActions` direto no gancho do `doCinema`**: o `doCinema` roda dentro do `s_actionTasks.update()`; o flush
  (`executeAndDeleteAll`) dessa mesma lista reentrante é arriscado. O jogo sempre adia pelo `TaskManager::global`.
- **`g_noScripts = 1`** (0x6F8180, o "noscripts" da linha de comando): tem de estar ligado antes da carga da fase, mata todos os
  scripts (também os registrados para chefes e o `hierSetCamera` do `rtMain`) e as posições iniciais vindas do script.
- Acelerar o `DelayAction`: o `TaskManager` não tem escala de tempo; mexer nos contadores das tarefas é mais invasivo que o caminho
  do START.

**Riscos que sobram (conferir ao vivo):** a condição dos fades (+0x120454/+0x120458) pode estar ligada nos primeiros quadros; o
gancho só tenta de novo no quadro seguinte (por isso a checagem fica no gancho de cada quadro, não num disparo único). O
`DelayAction::fade` agendado pelo `doCinema` para `length - 30` é apagado pelo flush (no modo "fim" ele não faz nada). O HUD do treino
(`wotmTrainHud`, painel) já some com letterbox e volta com ele. Com `WOTM_PLAYERS` 3/4 o `wotm_split4.inc` segue o `moviePlaying`
e volta aos quadrantes sozinho. A primeira chamada do `rtFadeScreen` (g_frame < 0x41) e o fade do pular se sobrepõem sem problema.

## 4. Cinemas no meio da luta e como reconhecer a intro

- Cinemas de chefe e de fase (`FinalBoss` 0x226A..0x226D, `PlantBoss` 0x2264, `vegasUpdate`, `tokyoUpdate`, `ufoUpdate`,
  `canyon2Update`, `threemile`) só rodam com `shell->m_mode == 1` / `m_gameMode == 1` (aventura). O treino é modo 2: não acontecem.
- Ainda podem existir `ACT_CINEMA` em ações de destrutíveis (`Destructible::DispatchAction`) ou gatilhos (`checkTriggerTree`), que
  são dados da fase (não inventariados aqui).
- Para distinguir: **armar** o pular na entrada do `rtMain(first)` da luta (o mesmo ponto em que o `wotm_overrides.cpp` liga o
  `g_on`), **desarmar** no primeiro uso ou depois de alguns segundos de luta sem cinema (ex. ~5 s de `moviePlaying == 0` depois de
  armar). A intro é agendada pela carga da fase e começa nos primeiros quadros. O RESET POSITIONS (`ResetLevel` + `rtMain(0)`)
  não reagenda scripts, então não há intro de novo (e `rtMain(0)` não arma). Opcional: opção no painel TRAINING OPTIONS
  ("pular apresentação", padrão ligado), já que o START continua pulando quando ela estiver desligada.

## Endereços (native/wotm_syms.h)
`doCinema__FP11DelayActionP9ActCinema` 0x100940, `endCinema__FP11DelayActionP9ActCinema` 0x100B20, `doIt__11DelayAction` 0x100FA8,
`endIt__11DelayAction` 0x101150, `stopAllActiveActions__14ActionDispatchv` 0x100100, `updateCinema__7Monster` 0x15BA10,
`endCinema__7Monster` 0x15BB48, `fadeOutAndIn__7TheGamei` 0x139750, `add__11TaskManagerPFv_Uii` 0x2220B0,
`_11TaskManager$global` 0x6E0718, `moviePlaying` 0x6F7900, `movieAborted` 0x6F7904, `g_noScripts` 0x6F8180, `_3Hud$m_letterbox`
0x2A3000, `setEnabled__12Letterboxingb` 0x146EA0, `startCinemaMusic`/`stopCinemaMusic__21StreamingSoundManager` 0x1C2000/0x1C2088.
