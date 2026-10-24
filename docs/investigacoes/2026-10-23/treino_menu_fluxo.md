# Modo treino: fluxo do menu (TRAINING -> monstros -> fase -> luta)

Investigação de 2026-10-23 (só leitura). Fonte: `src/game/screen.cpp`, `src/game/ui.cpp` (`userintMain`), `src/game/Shell.cpp` (`main`,
`EvaluateGameStatus`, `EvaluateOnePlayerChallengeStatus`), `include/game/shell.h`, e os ganchos de `native/runtime/wotm_padsel.inc`,
`wotm_mainmenu.inc`, `wotm_overrides.cpp`, `wotm_modes4.inc`. Endereços por nome em `native/wotm_syms.h`.

## Campos usados (shellInfo = `wotm::sym::shellInfo`)

| campo | offset | quem grava | valores |
|---|---|---|---|
| `m_mode` (kMode) | +0x2C10 | `screenGameModes1P` no X | 1 aventura, **2 free-for-all de 1 jogador** ("challenge" no `EvaluateGameStatus`), 4 endurance; o `main` grava 0x40 ao voltar aos menus (0x3F nos filmes) |
| `m_numAIs` (kNumAIs) | +0x2BB8 | 1ª passada da tela 2 (0), tela 12 no X (1..3; 0 com o mod do `ffaOptions1PWithZero`) | CPUs |
| `m_killTarget` | +0x2A44 | tela 15 no X | 1 = "first to 1" (morte súbita), `mSelectWins + 2` = "first to N", **0 = sem alvo (unlimited)** |
| `m_battleMode` (kBattleMode) | +0x2B54 | só a tela 5 (2 jogadores: 3/6) e `screenMGSelect` | o free-for-all de 1 jogador **não** grava (fica o valor velho) |
| `m_monsterSel[0..7]` | +0x2938 | seleção | [0] jogador, [4..] CPUs (`<<5`) |
| `m_monsterSel[4..6]` | +0x2948..+0x2950 | 1ª passada da tela 12 zera | CPUs escolhidas |
| `m_costume[0..]` | +0x2BE0 | seleção | [0] jogador, [2] a 1ª CPU |
| `m_numPlayers` | +0x2BB4 | tela 1 | 1 |
| `m_levelNum` | +0x2BB0 | tela 8 no X | fase |
| `m_inSession` | +0x2BCC | `EvaluateGameStatus` | 0 = a sessão (laço do `rtMain`) acabou |
| `m_inMenus` | +0x2BD0 | idem | apesar do nome: 1 = emendar outra sessão sem menu (demo); 0 = voltar aos menus |

Globais: `currScreen`, `prevScreen`, `lastScreen`, `oldScreens[16]`, `screenFirstPass`, `betweenScreens`, `currAnimationIndex`,
`currentSelection[]` (`wotm::sym::currentSelection`, um int por tela; jogador 2 em [22 + tela]), `g_levelList`, `mSelectWins`,
`initialAnimation`.

## 1. Telas do free-for-all de 1 jogador (despacho: `switch (currScreen)` do `userintMain`)

```
2  screenGameModes1P          X em FREE-FOR-ALL (currentSelection[2] = 2): m_mode = 2, g_levelList = PO_1PFreeForAll,
                              changeScreen(2, 12, dir 1, anim 2, track)
12 screenFreeForAllOptions1P  NÚMERO DE CPUs: esquerda/direita em currentSelection[12] (1..3; 0 com o mod), X: m_numAIs = sel,
                              changeScreen(12, 15, 1, 0); com 0: m_numAIs 0 e changeScreen(12, 3, 1, 0) (pula a 15).
                              1ª passada: zera m_monsterSel[4..6] e os switches de coluna das seleções 2P/3P/4P (+0x1C4, +0x1C8,
                              +0x1D0, +0x1D4, +0x1D8, +0x1E0)
15 screenBestOf               REGRAS ("first to"): item 1 -> m_killTarget 1, item 2 -> mSelectWins + 2, item 3 -> 0 (sem fim);
                              resetSelectables(); próxima tela por m_numAIs (modo 2): 1 CPU -> 6, 2 -> 18, 3 -> 19
6  screenCharSelect2P         modo 2 com 1 CPU: o controle 1 escolhe o dele (m_monsterSel[0], m_costume[0]) e depois o da CPU na
   (18 screenCharSelect3P,    mesma coluna (m_monsterSel[4], m_costume[2]); os dois prontos -> changeScreen(6, 8, 1, 3)
    19 screenCharSelect4P)    (18/19: o jogador e depois cada CPU, uma por vez)
8  screenLevelSelect          X numa fase destravada: m_levelNum, changeScreen(8, 99, 1, 4)
99 (no próprio userintMain)   loga a partida, currScreen = prevScreen = 1, oldScreens = {1, 1, 0}, initialAnimation = 1,
                              userintReturnToShell -> Shell main: LoadLevelFiles ... InitPlayers ... laço do rtMain
```

A tela 3 (`screenCharSelect1P`) e a 17 (`screenSelect1AI`) são da aventura/endurance (e do free-for-all de 0 CPU). No free-for-all
de 1 jogador com CPUs **a escolha do monstro do inimigo já acontece na seleção** (tela 6 com 1 CPU; 18/19 com 2/3). O número de CPUs
decide a tela (1 -> 6, 2 -> 18, 3 -> 19) e o `screenCharSelect2P` copia `m_numAIs` para `AiLeft.2904` na 1ª passada; o modo 2 da
tela 6 supõe 1 CPU. A seleção nativa de `wotm_padsel.inc` já cobre esse caso (`soloPanels`, `g_solo == 6`, `mode == 2`: quadro do
jogador e quadro da CPU, "waiting" até o jogador confirmar); ela começa sozinha no gancho do `screenTransition` (1ª passada com
`cur == 6 && !g_multi`).

Voltas (triângulo): 12 -> prevScreen (2) com anim 2; 15 -> 12 (modo 2); 6 -> prevScreen com anim 0; 8 -> prevScreen (restaura
`currentSelection[6]` quando prevScreen 6 e modo 2).

## 2. Como pular as telas 12 e 15 (1 CPU, sem fim)

O que as duas telas fazem e precisa ser feito à mão: `m_numAIs = 1` (+0x2BB8), `m_killTarget = 0` (+0x2A44), zerar `m_monsterSel[4..6]`
(+0x2948, +0x294C, +0x2950) e o switch da coluna do jogador 2 da tela 6 (`hierSetSwitch(*(shell + 0x1E0), 0)`; a 1ª passada da tela 6 no
modo 2 não mexe nele). `resetSelectables` e `initSelectSwitches` a própria tela 6 chama na 1ª passada; `currentSelection[6]` já foi
posto em `m_selFirst + 1` pela 1ª passada da tela 2. `g_levelList` e `m_mode` a tela 2 já gravou.

A câmera: 2 -> 12 usa a animação 2 e 12 -> 15 -> 6 não têm animação (as três telas ficam na mesma posição), então 2 -> 6 com a
animação 2 chega ao lugar certo. A transição (`screenTransition`) é genérica (fade de `lastScreen`, `screenPrepareDefault(currScreen)`),
nada nela depende de passar pela 12.

Duas formas equivalentes, ambas na thread do EE:

- **(recomendada) saída do `screenGameModes1P`** — já existe o `modes1Hook` (`WOTM_MM_EXIT_HOOK` em `wotm_mainmenu.inc`). Depois da
  original, se `g_menuTraining` e `currScreen == 12` (o `changeScreen(2, 12, 1, 2)` acabou de rodar: prevScreen = 2, anim 2,
  `screenFirstPass = 1`, `betweenScreens = 1`): gravar `currScreen = 6` e os campos acima. Igual a ter chamado `changeScreen(2, 6, 1, 2)`;
  a pilha `oldScreens` fica com um empurrão só. Tudo dentro do `wotm_mainmenu.inc`, onde mora o `g_menuTraining`.
  Não chamar um segundo `changeScreen` (empilharia 12 como prevScreen). Nem chamar antes da original (ver o comentário do `g_after` em
  `wotm_padsel.inc`: a 1ª passada da tela nova rodaria no lugar dela).
- alternativa: no `changeScreenHook` de `wotm_padsel.inc`, com `from == 2 && to == 12` e o treino armado, trocar `ctx->r[5]` por 6 (e
  gravar os campos). Exige expor o `g_menuTraining` (por exemplo um `inline std::atomic<bool>` no `wotm_training.h`), porque o padsel
  vem antes do mainmenu.

A volta: da tela 6 o jogo faz `changeScreen(6, prevScreen = 2, -1, 0)`, sem animação de câmera (a de 12 -> 2 é a 2 invertida). Para a
câmera voltar, no `changeScreenHook` (ou num gancho de `changeScreen` no mainmenu): treino armado, `from == 6 && to == 2 && dir < 0` ->
`ctx->r[7] = 2`. Ao chegar à tela 2 a 1ª passada dela zera `m_numAIs`. Da fase (8) a volta vai à 6 e funciona sem mudança.

Com `m_killTarget = 0` o `EvaluateOnePlayerChallengeStatus` só reergue quem morreu (`reinitSlot`, `m_inSession = 1`): a luta só termina
por "sair" na pausa (r == 3). O nível pode ser qualquer um de `PO_1PFreeForAll` (o pulo do índice 3 da tela 8 só vale com 2+ CPUs).

## 3. Início e fim da luta na thread do EE

Fluxo do `Shell main`: `userintMain` (menus) -> `LoadLevelFiles` -> `InitAfterDbLoad` -> `InitPlayers` -> `do { r = rtMain(first);
EvaluateGameStatus(r); ResetLevel/UnpauseLevel } while (m_inSession)` -> volta ao topo do `for(;;)` (menus) quando `m_inMenus == 0`.

- **Começou:** entrada de `rtMain` (`wotm::fn::rtMain__Fb`) com `$a0` (first) != 0 — uma vez por sessão, com os monstros já criados.
  Já há o gancho `rtMainTapped` em `wotm_overrides.cpp` (sempre registrado; desliga o congelamento da carga), lugar natural para
  `if (armado && GPR_U32(ctx, 4)) g_on = true`. Alternativas: entrada de `Shell::LoadLevelFiles` (`loadLevelFilesTapped`, mesmo
  arquivo; cedo demais para os monstros) ou `Shell::InitPlayers` (`wotm_split4` e `wotm_modes4` já o embrulham; um terceiro gancho
  encadeia por `lookupFunction`, como o modes4 faz com o split4). `rtMain` roda de novo com first = 0 depois de cada morte/diálogo.
- **Terminou:** saída de `Shell::EvaluateGameStatus` (`wotm::fn::EvaluateGameStatus__5Shelli`) com `m_inSession` (+0x2BCC) == 0.
  No modo 2 isso acontece no r == 3 (sair pela pausa: `m_inMenus = m_inSession = 0`) e no r == 5 (sair da sessão). Hoje o
  `EvaluateGameStatus` só tem gancho com `WOTM_MATCH` (`evaluateGameStatusLogged`); um gancho novo deve encadear pelo
  `lookupFunction` e ser instalado depois desse. Alternativa mais grossa: `TheGame +0x1203C8` (o `m_mode` copiado para o jogo) volta a
  0x40 quando o `main` reentra nos menus.
- **Rede de segurança que já existe:** o `getInputHook` de `wotm_mainmenu.inc` desliga `g_on`/`g_menuTraining` quando `currScreen == 1`
  — e a volta da luta cai exatamente na tela 1 (item 4).

Sugestão de estado: `g_menuTraining` = "armado pelo menu" (como hoje) e `g_on` só entre o `rtMain(first)` e o fim da sessão;
desarmar ao chegar à tela 1. O F2/`WOTM_TRAINING` continuam ligando `g_on` direto.

## 4. Para onde a volta da luta vai

Tela 1 (menu principal). A tela 99 grava `currScreen = 1`, `prevScreen = 1`, `oldScreens = {1, 1, 0}` e `initialAnimation = 1` antes de
sair para a luta; nada no caminho de volta muda o `currScreen` (o `main` só grava `currScreen = exitFromAdvStory` quando ele vale 1,
depois da aventura; `uiInitScreens` não mexe nele). No 1º quadro do `userintMain` o `initialAnimation` faz `changeScreen(1, 1, 1, 1)`.
O `m_mode` chega aos menus valendo 0x40 (o `main` grava). Se o dono quiser voltar à seleção do treino em vez do menu principal, o lugar
é o mesmo da tela 99 (gancho no `changeScreen(8, 99)`, que já é observado no `changeScreenHook` para o narrador) ou a entrada do
`userintMain`: trocar o `currScreen`/`prevScreen`/`oldScreens` que a 99 deixou (não testado).
