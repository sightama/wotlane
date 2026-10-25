# Seleção de 3 e 4 jogadores: código perdido em merges (2026-10-25)

## Achado
Conferindo se os merges do colaborador do multijogador perderam progresso da master: não perderam. O que se perdeu foi código
dele mesmo. Nos conflitos do `native/runtime/wotm_padsel.inc` ele ficou com o lado que vinha da master e descartou o próprio, e os
PRs seguintes (#58, #60, #61) não tocam nesse arquivo, então nada voltou.

| Merge | Lado descartado | O que saiu |
|---|---|---|
| `64a02e1` (master em `multiplayer-pendentes`) | `7ce571a`, que trazia `6b5cb36` | CPUs na seleção de 3 e 4 |
| `ab8af8e` (`multiplayer-pendentes` em `multiplayer-modos4`) | `7f83bf5` | lista de modos com 3 e 4, eliminação e minigames pelo menu |

Os outros merges conferidos (`7ce571a`, `74edfed`, `cd20fb2` e os merges dos PRs no GitHub) mantiveram os dois lados. Método:
`git show --remerge-diff <merge>` (mostra só como os conflitos foram resolvidos) e `git diff --numstat <merge>^1 <merge>` nos PRs.

## O que falta no código de hoje
Com 3 ou 4 jogadores, o `configStep` põe `g_multi` e o `startMultiFreeForAll` vai direto à tela 15 com `m_numAIs` 0 (log
`[padsel] N jogadores: free for all, sem CPU`). Sumiram:

- **CPUs (`6b5cb36`):** a ida à tela 13 do retail (0, 1 ou 2 CPUs); `g_ais`, `kMaxCards`, `cardQueue`/`activeCard`/`lastReadyCard`
  (os quadros das CPUs são do jogador 1, um de cada vez depois do dele, e o triângulo desfaz o último pronto da fila); `cardRect`
  com duas fileiras acima de 4 quadros; quadro de CPU apagado com "P1 PICKS"; o `selectFinish` gravando `m_monsterSel[4 + j]`,
  `m_costume[2 + j]` e `m_numAIs`; no `changeScreenHook`, a 15 indo às seleções 18/19 do retail redirecionada à 6; a volta de
  5 com destino 13 ou 15 indo ao menu principal.
- **Modos (`7f83bf5`):** com 3 e 4 a lista "free-for-all / elimination / minigames" aparece como com 2 (`g_phase = Reveal`), e as
  telas do retail seguem (13, 15, 11 continues, 14 escolha do minigame); na eliminação (modo 6) traje 0 e monstro exclusivo
  (`SelectSlot` taken 0; escolhas em Shell+0x2B84/0x2B88 e `wotmElimSetStart` do `wotm_modes4.inc` para o "play again");
  minigame vindo da 14 vai à seleção do telão no lugar da tela 7 (`menuAnim` 8 ao contrário), termina na carga
  (`changeScreen(6, 0x63)`) e volta à 14; volta da seleção à tela anterior (11, 14 ou 15).

O `wotm_modes4.inc` (regras de 3 e 4 na eliminação e nos minigames) e a chamada do `wotmElimSetStart` no `wotm_overrides.cpp`
estão na master; hoje só se chega a eles pela `WOTM_MODE`.

## Como refazer
O `wotm_padsel.inc` mudou muito desde então (seleção nativa de 1 e 2, skins de mod, casa CUSTOM, mouse na barra, `selWaiting`,
`selBase`, `selBoxW`), então não dá para copiar de volta: reaplicar a lógica em cima do arquivo atual, lendo os dois commits
(`git show 6b5cb36 -- native/runtime/wotm_padsel.inc`, idem `7f83bf5`). Teste por roteiro: `PS2X_PAD` + `WOTM_PADSEL_SCRIPT`,
`WOTM_SHOTS` (3 jogadores + 1 CPU, 4 + 2; eliminação e um minigame com 3).

## Docs que ficaram errados
- `docs/passos/NATIVO.md`, "Menu multijogador de 2 a 4": descreve a tela 13 com CPUs e a lista de modos com 3 e 4 como existentes.
- `TODO.md`: item 11 ("testes" dos minigames com 3 e 4 só valem pela `WOTM_MODE`) e "Até 7 CPUs pelo menu" (a tela 13 com 3 e 4
  depende desta correção).
