# Mouse nas telas de escolha de monstro (3, 17 e 6)

Investigação de 2026-10-22 para a etapa 9 (mouse nos menus que continuam os do jogo). Fontes: `src/game/screen.cpp` (C das telas em
`NON_MATCHING`), `src/game/Shell.cpp` (`Shell::AddEpNode`), `include/game/shell.h`, `include/game/monster_select.h` e a hierarquia do
`SHELL/SHELL.NGP` do disco lida offline (rascunhos em `build/scratch_mouse/`, fora do repo: `shell_eps.py`, `parents.py`, `group.py`;
leem o disco do usuário, não gravam nada do jogo). Endereços por nome de `native/wotm_syms.h` (`wotm::sym::`).

## Resumo para implementar

1. **Retratos da fileira:** não estão nos switches. `m_selectSw[i]` (Shell+0x168, i = coluna-1) só tem as marcas "1"/"2"/cadeado por
   cima do retrato, e o quadro 0 (sem marca, o normal) é um nó NULL: por isso o filho em uso não dá retângulo. O retrato da coluna c é o
   filho c do GROUP irmão dos switches (receita abaixo, com validação). Projetar esse OBJECT com o `objectPlane`.
2. **Caixinhas de traje:** `m_costumeSw[k]` (Shell+0x90, k = traje 0..3, de cima para baixo) e `m_costumeSw2[k]` (Shell+0xA0, a caixa
   da direita: jogador 2 ou a IA no modo 2). Os três filhos (0 fechado, 1 aberto, 2 cursor) são objetos no mesmo lugar: o filho em uso
   (byte +8) sempre dá o retângulo. Só valem com o cs da caixa desenhado (`m_costumeBoxCs` +0x88 / `m_costumeBoxCs2` +0x8C: `drawMe`
   +0xC = 1 e alfa float +0x7C > 0). Quadro 0 = traje fechado: ignorar o hover.
3. **Variáveis da escolha:** coluna = `currentSelection[tela]` (jogador 2 do 2P: `currentSelection[22 + 6]` = índice 28), 1-based
   (1..10); traje = estáticos locais `curCostume1_2833` (tela 3), `curCostume1_2847` (17), `curCostume1_2894` / `curCostume2_2895` (6).
4. **Entrada:** esquerda (3)/direita (4) andam coluna; cima (1)/baixo (2) andam traje entre os abertos; X (6) confirma; triângulo (5)
   volta. Na tela 3, X sai da tela na hora (história: carrega a luta; resistência ou sem IA: escolha de fase). Na 6, X marca "pronto".
5. **Saltar:** dá para escrever a coluna/traje direto (receitas por tela abaixo), mas o jeito mais seguro e com o som do jogo é injetar
   um passo de esquerda/direita (ou cima/baixo) por quadro até chegar, só para alvos válidos. Clique = mover até o alvo e então X.
6. **Tela 17 (`screenSelect1AI`) é praticamente inalcançável:** nenhum código chama `changeScreen(..., 17)` a não ser ela mesma. O 1P
   contra IA passa pela tela 6 (modo 2, 1 IA), 18 (modo 2, 2 IAs) ou 19 (3 IAs) via "first to" (`BO_NEXT_SCREEN`). Tratar a 17 como a 3
   (abaixo), sem prioridade.

## Hierarquia (SHELL.NGP)

Ids de ep = id1 - 0x2328 (`Shell::AddEpNode`); id2 separa as cópias (0 = select normal, 1 = select de minigame).

| Shell | ep | Nó | Filhos |
|---|---|---|---|
| `m_selectSw[0..9]` (+0x168 + 4i) | 700..709, id2 0 | SWITCH, 9 filhos | 0 NULL (sem marca); 1 marca do jogador 1; 2 jogador 2; 3 os dois; 4 cadeado (OBJECT/TRANSLATE só nas colunas 1 e 10; NULL nas outras); 5 IA 1 / jogador 3; 6 IA 2 / jogador 4; 7 jogadores 3 e 4; 8 IA 3. `m_selectSw[10]` (ep 710) não existe no arquivo: fica nulo |
| `m_costumeSw[0..3]` (+0x90) | 242..245 | SWITCH, 3 filhos (GROUP com 1 OBJECT cada) | 0 fechado, 1 aberto, 2 cursor; mesmo lugar |
| `m_costumeSw2[0..3]` (+0xA0) | 246..249 | idem | caixa da direita |
| `m_costumeBoxCs` (+0x88) / `m_costumeBoxCs2` (+0x8C) | 240 / 241 | GROUP (o cs) | as 4 caixinhas, 12 objetos |
| `m_sw5CC` (+0x5CC) | 720 | SWITCH, 11 filhos | 0 NULL; 1..10 a imagem grande do monstro da coluna (painel do 1P) |
| Shell+0x1DC / +0x1E0 (`m_ep1DC[0][0..1]`) | 571 / 572, id2 0 | SWITCH, 11 filhos | painel grande do jogador 1 / do jogador 2 (ou da IA) no 2P; quadro = coluna |
| `m_monsterCursorSw[0..2]` (+0x1C0) | 740..742 | SWITCH | painéis do 3P (fora deste escopo) |

Posições no mundo (Z para cima), só para orientar: retratos em z ≈ 42, coluna 1 em x ≈ -402 até a coluna 10 em x ≈ -577 (passo ~19,5
em x local); caixa do jogador 1 em x ≈ -403, caixa da direita em x ≈ -576, trajes 0..3 em z ≈ 94, 85, 75, 66 (traje 0 em cima). As
caixas ficam acima da fileira, perto das colunas 1 e 10. Painel do jogador 1 (ep 571) perto da coluna 2, o do jogador 2 perto da 8:
provavelmente a coluna 1 aparece à esquerda; a projeção confirma.

### Achar o retrato da coluna c na RAM

No arquivo, os 10 switches e o grupo dos retratos são filhos do mesmo GROUP `P` (11 filhos: `[0]` = grupo dos retratos, `[1..10]` =
`m_selectSw[0..9]`), e `P` vem logo depois do nó do `m_selectSw[9]`. A imagem é carregada inteira e só relocada, então o leiaute vale
na RAM:

```
sw9  = rd32(shell + 0x168 + 9*4) & 0x1FFFFFFF
P    = sw9 + 0xC + 4 * rdram[sw9 + 0xB]          // SWITCH: filhos a partir de +0xC, contagem em +0xB
validar: (rdram[P] & 0x3F) == 1 (GROUP), u16(P + 8) == 11, rd32(P + 0x20 + 4*k) == m_selectSw[k-1] para k = 1..10
G    = rd32(P + 0x20) & 0x1FFFFFFF               // GROUP de 12 filhos: [0] faixa de fundo (grupo), [1..10] retratos, [11] molduras
retrato(c) = rd32(G + 0x20 + 4*c)                // OBJECT, c = 1..10
```

Se a validação falhar, alternativa sem leiaute: o objeto do filho 1 de `m_selectSw[c-1]` (OBJECT, ou o único objeto do GROUP) tem o
centro da esfera (+0x10..+0x18, local) igual ao do retrato da coluna; procurar em `scene.items` um OBJECT desenhado com o mesmo centro
(±0,5) e raio ~13 (+0x1C). Os dois estão sob o mesmo pai sem transformação no meio, então qualquer um dá o retângulo.

O retrato é desenhado sempre (mesmo nas colunas travadas, que ganham o cadeado por cima no quadro 4). Ignorar colunas fora da faixa
destravada e, onde vale, colunas sem traje livre (abaixo).

## Tela 3: `screenCharSelect1P` (1 jogador)

Entra pelos modos de 1 jogador: história (`m_mode` 1), free-for-all sem IA (modo 2, via tela 12) e resistência (modo 4, 5 IAs sorteadas).

- **Coluna:** `currentSelection[3]` (`wotm::sym::currentSelection + 4*3`), 1..10. Faixa: `first_2831`..`last_2832` (inclusiva,
  recalculada no começo de cada quadro pelos destraves `monsterSelectMode[0].state` e `[36].state`: 2..9, 1..9, 2..10 ou 1..10).
- **Traje:** `curCostume1_2833` (0..3). Abertos: `isSlotOpen(col-1, c)` = `monsterSelectMode[(col-1)*4 + c]` com `state == 1 &&
  taken == 1` (slot de 0x2C bytes: `taken` +0, `state` +4). `numAvail1_2830` = quantos abertos.
- **Esquerda/direita:** coluna -1/+1 com volta (abaixo de `first` vai a `last` e vice-versa); não pula coluna nenhuma além das
  travadas da ponta. Se o traje 0 da nova coluna tiver `taken == 1`, o traje volta a 0. Som 5 (esq.) / 3 (dir.).
- **Cima/baixo:** traje -1/+1 com volta, pulando os fechados; nada se só um estiver aberto. Som 2 / 4.
- **A cada quadro, antes da entrada:** `updateSelectSwitches(col, 0, 0, 1)` (marcas), switches de traje, avanço do traje até um aberto
  (se a coluna não tiver nenhum, anda a coluna; na coluna 10 vai a 0, ver armadilhas) e `hierSetSwitch(m_sw5CC, col)` (imagem grande).
- **X:** `m_costume[0] = traje`; se o slot não for `state 1`, nada. Senão `m_monsterSel[0] = monsterNametomonsterId[col-1]`, o slot
  fica `taken 0`, `monsterIsSelected_2829[0] = 1`, e: modo 1 → `changeScreen(3, 0x63)` (carrega a luta); modo 4 (sorteia as IAs) ou
  sem IA → `changeScreen(3, 8)` (escolha de fase). Não há estado "pronto": X sai da tela.
- **Triângulo:** volta (`changeScreen(3, 0xC)` no modo 2, senão `prevScreen`), devolve o slot (`taken 1`).
- **Painel grande** (`m_sw5CC`): opcional mapear clique nele como X.

**Saltar direto (alternativa aos passos):** com `col` em `[first_2831, last_2832]` e `state == 1` em algum traje dela: escrever
`currentSelection[3] = col`; se `monsterSelectMode[(col-1)*4].taken == 1`, `curCostume1_2833 = 0`. O quadro seguinte acerta marcas,
trajes e imagem grande (escrever no gancho de saída do `screenGetInput(1)` deixa um quadro de atraso; não chamar nada). Traje: escrever
`curCostume1_2833 = c` só se `isSlotOpen(col-1, c)`. Sem som (o `sfx` do `wotm_padsel.inc` toca 5/3 ou 2/4 se quiser).

## Tela 17: `screenSelect1AI` (IA contra 1 jogador; quase morta)

Mesma tela da 3, uma vez por IA (`AINum` conta as escolhidas, `AINumLeft` as que faltam; o prompt é "PICK OPPONENT n").

- **Coluna:** `currentSelection[17]`; faixa `first_2848`..`last_2849`. **Traje:** `curCostume1_2847`.
- **Esquerda/direita:** só colunas com `modelsLeft(col) > 0` (algum traje com `taken && state`); traje volta sempre a 0. Os casos de
  ponta do retail são escritos à mão (tabela de `switch`), mas o efeito é "próxima coluna com traje livre".
- **Cima/baixo, triângulo:** como na 3 (triângulo volta à tela 3).
- **X:** `m_costume[2 + AINum]`, `m_monsterSel[4 + AINum]`, `AINum++`, `AINumLeft--`, slot `taken 0`; reabre a 17 (`changeScreen(17,
  17)`, com transição: `betweenScreens` liga e o `screenGetInput` devolve 0) ou, na última, vai à fase (8).
- **Saltar direto:** `currentSelection[17] = col` com `modelsLeft(col) > 0` e na faixa; `curCostume1_2847 = 0`.

## Tela 6: `screenCharSelect2P` (2 jogadores, ou 1 contra 1 IA)

Com 3 ou 4 jogadores a tela 6 é escondida pelo `wotm_padsel.inc` (`g_selOn`): o mouse daqui não deve agir nesse caso.

Variáveis (todas `wotm::sym::`): coluna do jogador 1 `currentSelection[6]`, do jogador 2 `currentSelection[28]`; trajes
`curCostume1_2894` (caixa esquerda, `m_costumeSw`) e `curCostume2_2895` (caixa direita, `m_costumeSw2`: jogador 2, ou a IA no modo 2);
prontos `playerOneIsDone_2896`, `playerTwoIsDone_2897`, `AIplayerIsDone_2898`; faixa `beginning_2888`..`ending_2889`; contagem de
abertos `numAvail1_2900`/`numAvail2_2901`; espera final `watchForASecond_2899` (10 quadros); coluna confirmada do jogador 1 em
`D_00731458` + (6*2)*4 (restaurada pelo triângulo; esse símbolo não está no `wotm_syms.h`, mas o mouse não precisa dele). Painéis grandes: Shell+0x1DC (jogador 1) e +0x1E0 (jogador 2/IA), quadro = coluna.

O jogador 1 é lido por `screenGetInput(1)` e o 2 por `screenGetInput(2)`, no mesmo quadro, nessa ordem (o 2 só fora do modo 2). O gancho
atual só mexe na chamada com `$a0 == 1`: o mouse dirige o jogador 1.

### Modos normais (`m_mode` 3 = free-for-all, outros fora de 2 e 6)

- **Esquerda/direita:** `findAvailableMonster(±1, jogador, 6)`: anda com volta na faixa até uma coluna com `modelsLeft > 0` (pula
  colunas sem traje livre, por exemplo a do outro jogador se ele levou o último). Jogador 1: traje volta a 0 sempre; jogador 2: só se o
  traje 0 da nova coluna tiver `taken 1`. Pronto: o jogador 1 já pronto não anda (o 2 também não).
- **Cima/baixo:** traje entre os abertos; nada se pronto ou se só um aberto.
- **X:** se não pronto e o slot (coluna, traje) está aberto: `m_monsterSel[p]`, `m_costume[p]`, slot `taken 0`, caixa escondida
  (`drawMe 0`), pronto = 1. Se os dois já estão prontos, X vai à fase na hora. Pronto sozinho + X: nada.
- **Triângulo:** pronto → desfaz (pronto 0, coluna do jogador 1 volta à confirmada, slot `taken 1`, caixa reaparece); não pronto → sai
  da tela (`prevScreen`) mesmo que o outro esteja pronto.
- **Os dois prontos:** 10 quadros e `changeScreen(6, 8)`.

### Modo 6 (eliminação de 2 jogadores)

Traje sempre 0, cima/baixo ignorados, caixas de traje não aparecem (sem `drawMe`; checar `drawMe` e alfa antes de projetar). Esquerda/
direita: `findAvailableMonster` e depois anda até a coluna com o traje 0 aberto. Os dois não podem ficar no mesmo monstro: com um pronto,
se o outro estiver na mesma coluna o jogo o empurra (`findAvailableMonster(1, ...)`) no começo do quadro. X copia a escolha também para
Shell+0x2B84 / +0x2B88.

### Modo 2 (1 jogador contra 1 IA, vindo do free-for-all de 1 jogador)

Só o controle 1. Duas fases na **mesma** coluna `currentSelection[6]`:
- **Fase A** (`playerOneIsDone_2896 == 0`): o jogador escolhe o seu monstro; traje em `curCostume1_2894`, caixa esquerda. X → pronto.
- **Fase B** (`playerOneIsDone_2896 == 1`, `AIplayerIsDone_2898 == 0`): esquerda/direita mexem a mesma coluna (agora a da IA; painel
  Shell+0x1E0), traje em `curCostume2_2895` na caixa direita (`m_costumeSw2`, mostrando os trajes da coluna); X → `m_monsterSel[4]`,
  `m_costume[2]`, IA pronta. Triângulo → volta à fase A (desfaz os dois).
- Com os dois prontos: X vai à fase na hora, ou após 10 quadros.

### Saltar direto na tela 6

Coluna: alvo na faixa e com `modelsLeft(col) > 0` (no modo 6 também `isSlotOpen(col-1, 0)` e diferente da coluna do outro, se ele
estiver pronto); só se o jogador (ou, na fase B, a IA) não estiver pronto. Jogador 1: `currentSelection[6] = col` e traje
(`curCostume1_2894`, ou `curCostume2_2895` na fase B) = 0. O jogo reacende marcas e painel grande no quadro seguinte. Traje: escrever o
cursor da caixa certa só se `isSlotOpen(col-1, c)`. Na dúvida, passos (um `findAvailableMonster` por quadro já faz o pulo certo).

## Como converter o mouse em entrada (as três telas)

Na janela (dentro do `BeginMode3D`, como o `wotm_gamescreens::frame`), com `betweenScreens == 0`:
- retângulo de cada retrato c = 1..10 (receita acima), válido se `c` na faixa da tela e, nas telas 17 e 6, `modelsLeft(c) > 0`;
- retângulo de cada traje k = 0..3 da caixa ativa (caixa esquerda na tela 3/17 e para o jogador 1 da 6; a direita na fase B do modo 2),
  válido se o cs da caixa está desenhado e o switch não está no quadro 0.

Na thread do EE (saída do `screenGetInput(1)` da tela, só com o código lido = 0):
- hover num retrato ≠ coluna atual → um passo por quadro: 3 se o alvo está "antes", 4 se "depois" (a volta nas pontas só existe entre
  `first` e `last`, então a direção pelo sinal sempre chega; nas telas 17 e 6 o passo pula colunas sem traje, o que só encurta). Parar ao
  chegar, se a coluna não mudar num passo, ou depois de 10 passos.
- hover num traje aberto ≠ traje atual → passos de 1/2 (o jogo pula os fechados; com um só aberto ele ignora, mas aí já se está nele).
- clique esquerdo: alvo = o elemento sob o mouse; ao chegar, devolver 6 (X). Clique num retrato confirma com o traje atual.

O `wotm_gamescreens::input` genérico não serve como está: ele compara o item com `currentSelection[tela]`, que aqui é só a coluna; o
traje é outro cursor e o item dos switches `m_menuItemSw[3][i]` não é o retrato.

## Armadilhas

- **Quadro 0 do `m_selectSw` é NULL:** o filho em uso não dá retângulo da coluna sem marca (a causa do fracasso da primeira tentativa).
  Usar o retrato do grupo irmão.
- **Coluna fora da faixa ou sem traje:** o laço de "acha traje aberto" do começo do quadro (telas 3 e 6) anda a coluna quando não há
  nenhum aberto e, na coluna 10, põe a coluna em 0: o quadro seguinte lê `monsterSelectMode[-4..-1]` (fora da tabela). Nunca escrever
  uma coluna travada ou sem traje livre; só mirar alvos válidos.
- **Passar por cima de outros retratos** a caminho das caixas de traje troca o monstro (e, no 1P, o traje volta a 0). As caixas ficam
  acima das pontas da fileira. Se incomodar: só aceitar hover de retrato depois de 2 ou 3 quadros parado nele.
- **X na tela 3 é definitivo** (sai da tela); clique num retrato do 1P confirma logo o monstro.
- **Tela 6:** o segundo controle é `screenGetInput(2)`; não mexer nele. No modo 2 a coluna da IA é a mesma variável da do jogador.
  Jogador pronto ignora esquerda/direita/cima/baixo: o mouse não deve tentar andar (desistir), só aceitar triângulo pelo teclado.
- **3 ou 4 jogadores:** a tela 6 está escondida pelo `wotm_padsel.inc` (`g_selOn`); retratos e caixas ali não são do retail.
- **Transições:** X na 17 reabre a própria 17 com transição; `betweenScreens` liga e o `screenGetInput` devolve 0. Recalcular os
  retângulos só com `betweenScreens == 0` e zerar o alvo ao trocar de tela.
- **Segunda leitura do controle 1:** as telas 3 e 17 têm uma chamada extra a `screenGetInput(1)` atrás de `showTexCounter >= 0x2E`,
  mas esse contador nunca sobe (só é zerado): na prática há uma chamada por quadro.
- **Destraves:** a coluna 1 e a 10 dependem de `monsterSelectMode[0].state` e `[36].state`; as do meio estão sempre destravadas
  (`columnOpen` do `wotm_padsel.inc` faz o mesmo).
