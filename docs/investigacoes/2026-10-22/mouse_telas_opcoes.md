# Mouse nas telas 3D do menu que continuam as do jogo (7, 8, 11, 12, 13, 14, 15)

Data: 2026-10-22. Pedido: especificação, por tela, para (1) mouse sobre um elemento escolhe ele e (2) clique esquerdo confirma (X),
com o gancho já existente na saída do `screenGetInput(1)` (`wotm_mainmenu.inc` → `wotm_gamescreens::input`) e a projeção de objetos
do Shell na janela (`wotm_padsel::objectPlane`).

Fontes: o C das telas em `src/game/screen.cpp` (todas em `NON_MATCHING`, conferidas por contagem de referências no asm onde importava),
`include/game/shell.h`, `native/wotm_syms.h` e a **árvore real dos switches**, lida da RAM do `menus.vu1snap` do corpus (quadros 682 =
tela 12, 767 = tela 15, 990 = tela 8) com um script de rascunho (`%TEMP%\wotm_mouse\snapdump.py`, fora do repo: reconstrói a RAM
do instantâneo e despeja os nós; malhas pelo `tools/ngp.py`). O banco do Shell é carregado uma vez, então a estrutura vale para todas
as telas, inclusive 7, 11, 13 e 14, que não estão no corpus. Coordenadas abaixo são as locais dos objetos (do `polyPkt`), só para
comparar posições entre si.

## Achado geral: por que a tentativa genérica não acha os itens

1. **`forSwitchObjects` só desce um nível.** Ele aceita filho OBJECT, ou GROUP com OBJECTs diretos. Nessas telas os filhos são
   GROUP de GROUP, SWITCH dentro de SWITCH, TRANSLATE (op 3) e CHAR_INSTANCE (op 25, setas animadas). Na tela 15, o item 1 aceso é
   `GRP{GRP{brilho}, GRP{texto}}` (nenhum OBJECT direto), o item 2 é um SWITCH cujos filhos são outros SWITCHes, e só o item 3
   apagado (`GRP{OBJ}`) aparecia: foi o que se viu com o cursor no item 1.
   **Correção:** um caminhante recursivo que segue só o caminho ativo: SWITCH → filho `which` (+8), GROUP → todos os filhos (u16 +8,
   filhos +0x20), TRANSLATE → filhos (+8 contagem, +0x1C), CHAR_INSTANCE → filhos (+0x28 contagem, +0x2C), ANIM_XFORM → filhos
   (+4, +0xCC), OBJECT → coleta. O `objectPlane` já filtra o que não está em `scene.items`.
2. **`currentSelection[tela]` não é "índice do item"** em nenhuma dessas telas, salvo a 15 (e mesmo lá o item 0 não existe: 1..3).
   Nas 11/12/13/14 ele é um **valor** (mexido por esquerda/direita), na 8 é o índice na lista de fases, na 7 é a coluna do monstro.
3. **Brilho e setas** entram na união se nada for filtrado: o brilho (OBJECT de 24 ou 30 vértices, `texId[0]` 194) é maior que o
   texto e invade a linha vizinha. Use só os objetos de texto/dígito para o retângulo da linha (ou divida as faixas verticais
   pelo meio entre centros vizinhos).
4. **Eixo X local invertido.** Nas telas 11/12/13 o dígito fica em X local positivo e aparece à **esquerda** na tela (foto
   `docs/screenshots/2026-10-17/2026-10-17_ffa_menu_0_cpu_monsters.png`: "◀ 0 cpu monsters ▶"). Decida esquerda/direita das setas pelo
   X **projetado**, nunca pelo índice em `m_menuObj` (a ordem das duas setas muda de tela para tela).

## Tela 15: "first to" (`screenBestOf`)

- **Elementos:** três linhas, de cima para baixo: item 1 "sudden death", item 2 "first to N wins", item 3 "unlimited play".
  Escolhido: `currentSelection[15]` ∈ {1, 2, 3}.
- **Entrada:** 1 (cima) e 2 (baixo) andam com volta (1 ↔ 3); a tela apaga o switch do item antigo (0) e acende o novo (1).
  3/4 só valem com o cursor no item 2: mudam `mSelectWins` (`wotm::sym::mSelectWins`) de 0 a `numWins` (= 8, em
  `numWins_2965`), com volta; o número mostrado é `mSelectWins + 2` (2..10). X: grava `Shell::m_killTarget` (1, `mSelectWins+2`
  ou 0) e segue para a escolha de monstro (6, 18 ou 19 conforme `m_numAIs` e `m_mode`). Triângulo: volta à 12 (modo 2) ou 13.
- **Objetos** (`m_menuItemSw[15][i]` = shell+0x25C+(150+i)*4):
  - item 1 (shell+0x4B8): SWITCH de 2 filhos. [0] apagado = `GRP{OBJ texto}` (tex 162; X local -24..35, Z 39..49);
    [1] aceso = `GRP{GRP{OBJ brilho tex 194}, GRP{OBJ texto}}`.
  - item 3 (shell+0x4C0): igual, Z 14,4..27,3 (X -29,5..41). [1] = `GRP{OBJ texto, GRP{OBJ brilho}}`.
  - item 2 (shell+0x4BC): SWITCH de 2 filhos, que **são os switches dos itens 4 e 5**: [0] = item 4 (shell+0x4C4, linha apagada),
    [1] = item 5 (shell+0x4C8, linha acesa). Cada um tem 9 filhos, um por `mSelectWins`. Filho k do item 4 =
    `GRP{GRP{GRP{OBJ "first to"? tex 195}, GRP{OBJ tex 51}}, GRP{OBJ dígito tex 142}}` (o filho 8, "10", tem dois dígitos).
    Filho k do item 5 = o mesmo com brilho (tex 194), e um CHAR_INSTANCE com as **duas setas** (OBJECTs tex 190, X local -43,5..-35,7
    e 46..53,8, na altura da linha); as setas só existem com a linha acesa. Linha inteira: X -34,5..42,2, Z 26..40.
  - Não use o item 4/5 como linhas próprias: eles são a mesma linha do item 2. Varra só os itens 1..3 (o 2 com o caminhante
    recursivo pega o filho certo do 4 ou do 5).
  - As setas também estão em `m_menuObj[15]` (shell+0x6DC+(1350+i)*4; 90 entradas, entre elas os pares tex 190 de cada filho do
    item 5: índices 35/36, 41/42, ...); só o par do filho em uso está na cena.
- **Retângulos:** os três textos são objetos separados (não dividir um retângulo em faixas). As faixas se encostam/sobrepõem um
  pouco em Z (49..39, 40..26, 27..14): para o "hover", escolha a linha cujo centro projetado está mais perto na vertical, ou corte no
  meio entre centros.
- **Saltar:** com 3 itens e volta, qualquer destino fica a **um** passo (1 ou 2): injete um código e confira no quadro seguinte. Não
  escreva `currentSelection[15]` direto (teria de apagar/acender os switches 1..3 e o item 4/5 como a tela faz).
- **Clique:** em cima da seta esquerda/direita (projetada) com o cursor no item 2 → 3/4 (um passo por clique); no resto da linha
  → passo até ela e depois 6. Clicar no item 2 sem estar nele: primeiro o passo, depois X (não 3/4).
- **Armadilhas:** o quadro de entrada (`screenFirstPass`) chama `screenPrepareDefault(15, sel)`, que acende só o item `sel`; no
  instantâneo do 1º quadro da tela todos os switches estavam em 0 (ainda antes da tela rodar). Só o controle 1 conta.

## Tela 12: free-for-all de 1 jogador, número de CPUs (`screenFreeForAllOptions1P`)

- **Elemento:** uma linha "◀ N cpu monsters ▶". Valor = `currentSelection[12]`, 1..3 por esquerda/direita (3 → `sel>=2 ? sel-1 : 3`;
  4 → `sel<3 ? sel+1 : 1`). Pode valer 0 se nunca foi mexido (aí X manda para a tela 3, escolha de monstro, sem passar pela 15);
  com 1..3, X grava `m_numAIs` e vai à 15. Triângulo: zera `m_numAIs` e volta.
- **Objetos:** `m_menuItemSw[12][0]` (shell+0x43C): SWITCH de 11 filhos, filho = valor, cada um `GRP{OBJ dígito tex 142}` (o 10 tem
  dois dígitos). É o "objeto pequeno" visto: **é só o número**. O resto da linha está em `m_menuObj[12]` (17 entradas):
  [0] fundo da faixa (32 vértices, sem textura), [1] rótulo "cpu monsters" (tex 51), [2] brilho (tex 194), [3..14] os dígitos dos
  filhos, [15] e [16] as setas (tex 190; aqui [15] fica à esquerda na tela e [16] à direita, mas decida pelo X projetado).
  A tela põe o switch em `currentSelection[12]` a cada quadro.
- **Mouse:** não há o que escolher por hover (um elemento só). Clique na seta da esquerda → 3, da direita → 4, no resto da linha
  (rótulo, dígito, faixa) → 6. Roda do mouse → 3/4 é um extra barato.
- **Saltar direto** (se quiser clicar num valor): escrever `currentSelection[12]` basta (a tela reaplica o switch no quadro),
  mas sem o som; injetar 3/4 é o mais seguro.

## Tela 13: free-for-all de 2 jogadores, número de CPUs (`screenFreeForAllOptions2P`)

- Igual à 12, com valor 0..2 (volta 0 ↔ 2), switch em `m_menuItemSw[13][0]` (shell+0x464, 11 filhos de dígito) e `m_menuObj[13]`
  com o mesmo arranjo (aqui [15] está à direita na tela e [16] à esquerda). X grava `m_numAIs` (0, 1 ou 2) e vai à 15; triângulo volta
  à 5. A tela também põe `m_sw5CC` em 0 e o switch do valor a cada quadro.

## Tela 11: eliminação de 2 jogadores (`screenElimOptions2P`)

- **Elemento:** título acima e, embaixo, "◀ N ▶". Valor = `currentSelection[11]`, 0..4 (3: `sel>0 ? sel-1 : 4`; 4: `sel<4 ? sel+1 : 0`).
  X grava `m_elimination = sel + 1` e `m_continues[0..1]`, e vai à 6 (escolha de monstro 2P). Triângulo volta.
- **Objetos:** `m_menuItemSw[11][0]` (shell+0x414): SWITCH de 6 filhos (0..5; o 5 não é usado), cada um
  `GRP{GRP{OBJ dígito tex 142}, GRP{OBJ brilho tex 194}}` (X local 2,5..10,5; brilho -0,2..15,6; Z 25..40).
  `m_menuItemSw[11][1]` (shell+0x418): a linha de título (tex 81, X -37..48, Z 38..50); os dois filhos são o mesmo quadro.
  `m_menuObj[11]` (17): [0] faixa, [1]/[2] título, [3..14] dígito e brilho de cada filho, [15]/[16] setas (X local 13,5..21,4 e
  -8,3..-0,5, ladeando o dígito).
- **Diferença das 12/13:** a tela só reaplica o switch do valor quando o valor muda (e o 1º quadro chama `screenPrepareDefault(11,
  sel)`, que mexe no switch do título com `i == sel`, sem efeito visível). Se for escrever `currentSelection[11]` direto, escreva
  também o byte `which` (+8) do switch, ou injete 3/4 (recomendado).
- **Mouse:** como na 12: setas → 3/4, o resto (dígito, título, faixa) → 6.

## Tela 14: escolha de minigame (`screenMGSelect`)

- **Elemento:** uma figura do minigame no telão. Valor = `currentSelection[14]`, 1..3 (Dodge Ball, Big Shot, Crush), volta com
  `numSelectables_2959` = 3. 3/4 mudam; X: só se o minigame estiver destravado (`monsterSelectMode[44/45/46].state == 1`, os
  `SelectSlot` de 0x2C em `wotm::sym::monsterSelectMode`) grava fase 25/26/27 e modo 7/8/9, põe o switch em 4 e vai à 7; travado só
  toca o som. Triângulo volta.
- **Objetos:** `m_menuItemSw[14][0]` (shell+0x48C), SWITCH de 5 filhos: [0] quadro vazio (tex 31), [1..3] = `TRN{OBJ figura, TRN{CHAR_INSTANCE}}`
  (tex 188, 186, 187), [4] = **a tela 7 inteira** (abaixo). Tudo num lugar à parte do mundo (X≈-235, Y≈1755, quadros de ~1,5
  unidades): projete com a câmera do quadro, como as outras. O aviso de travado é `Shell+0x600` (SWITCH: [1] = `GRP{OBJ tex 157}`
  no mesmo lugar), que a tela acende quando o minigame em foco está travado (`levelUnlocked(25..27)`).
  `m_menuObj[14]` está vazio e não há setas: o texto vem do `charSelectStringOne` (fonte 2D).
- **Mouse:** a figura é o único alvo. Proposta: clique no terço esquerdo/direito da figura → 3/4, no meio → 6 (sem efeito se travado,
  como no jogo); roda → 3/4. O `forSwitchObjects` atual não acha nada aqui (filho é TRANSLATE).

## Tela 7: escolha de monstro do minigame, 2 jogadores (`screenMinigames2P`)

- **Elementos:** colunas de monstro (retratos) e, por jogador, 4 trajes. Jogador 1 = `currentSelection[7]` (coluna 1..10;
  `MG_SEL(p) = currentSelection[p*22 + 7]`, o do jogador 2 é `currentSelection[29]`) e `curCostume1_2912` (traje 0..3; jogador 2:
  `curCostume2_2913`). Feito: `playerOneIsDone_2914` / `playerTwoIsDone_2915`. Colunas válidas: `beginning_2908`..`ending_2909` (2..9,
  1 e/ou 10 quando `monsterSelectMode[0]`/`[36]` destravados).
- **Entrada (só o controle 1 é nosso; o 2 lê `screenGetInput(2)`):** 3/4 trocam de coluna (com volta; a esquerda pula uma coluna sem
  modelo livre), zeram o traje; 1/2 andam pelos trajes abertos (nada se só um aberto). X: se o traje em foco está aberto e livre
  (`state == 1 && taken == 1`), grava `m_monsterSel[0]`/`m_costume[0]`, esconde o painel de trajes (`m_costumeBoxCsMG[0]`, shell+0xB0) e
  marca feito; feito e o outro também → carregamento (99). Triângulo: desfaz a escolha, ou volta à 14 se não havia. Com os dois feitos
  a tela vai sozinha ao carregamento. No começo de cada quadro a tela pode avançar `currentSelection[7]` sozinha se a coluna não tem traje.
- **Objetos** (dentro do filho [4] de `m_menuItemSw[14][0]`, que a 14 deixa aceso ao entrar na 7): `GRP{ GRP(4)[big P1 = SWITCH
  shell+0x1EC, big P2 = SWITCH shell+0x1F0, painel de trajes P1, painel P2], GRP(11)[ GRP(12)[OBJ placa tex 54, **10 OBJ retrato**
  (coluna 1..10 nesta ordem), GRP com 10 brilhos], m_selectSwMG[0..9] (shell+0x194+4*i: marcas 1/2/ambos/travado, filho 0 vazio) ] }`.
  O retrato da coluna c é o filho c (1..10) do grupo de 12; confere com a posição do `m_selectSwMG[c-1]`. Trajes do jogador p:
  `m_costumeSwMG[p][k]` (shell+0xB8+0x10*p+4*k), SWITCH de 3 filhos `GRP{OBJ}` (0 indisponível, 1 disponível, 2 cursor; tex 170..173),
  empilhados na vertical ao lado do retrato grande do jogador.
- **Mouse (jogador 1):** retrato da coluna c (c em `beginning_2908..ending_2909`) → passos 3/4 até `currentSelection[7] == c`
  (desista se não convergir; a coluna pode ser pulada); ícone de traje k → passos 1/2 até `curCostume1_2912 == k` (só se o traje
  estiver aberto: switch dele em 1 ou 2); clique → passo e depois 6. Ignore o mouse com `playerOneIsDone_2914 != 0` (só X/triângulo
  fazem algo) e quando `m_costumeBoxCsMG[0]` tem `drawMe` (+0xC) 0. `m_menuItemSw[7][*]` e `m_menuObj[7]` estão vazios.

## Tela 8: escolha de fase (`screenLevelSelect`)

- **Elemento:** a foto da fase (o telão inteiro) e o nome em fonte 2D (`charSelectStringOne`). Escolhido: `currentSelection[8]`, índice
  na lista `*g_levelList` (`LevelList {first, count, int *levels}`): 1P/MP free-for-all = 13 fases (1..11, 14, 15), endurance 9,
  eliminação 6. **O 1º quadro da tela zera o cursor** (`currentSelection[8] = first`, 0).
- **Entrada:** 3/4 com volta (`beginning_2925`/`ending_2926`); com `m_mode == 2 && m_numAIs >= 2` ou `m_mode == 3 && m_numAIs > 0` o
  índice 3 é pulado. Cada passo troca o switch da foto, o nome, solta o filme e toca o som. X só numa fase destravada
  (`levelUnlocked`): grava `m_levelNum` e vai ao carregamento. Triângulo: devolve os cursores da escolha de monstro e volta.
- **Objetos:** `m_sw5D0` (shell+0x5D0), SWITCH de 24 filhos indexado pelo **número da fase** (`levels[cur]`), cada um `GRP{OBJ foto
  (88 vértices, X -101..101, Z -36..86), CHAR_INSTANCE}`; `m_lockedSw` (shell+0x614) [1] = aviso de travado no mesmo lugar.
  Sem itens nem setas no Shell (`m_menuObj[8]` vazio, painel da tela 8 nulo).
- **Mouse:** como na 14: faixas esquerda/direita da foto → 3/4, centro → 6; roda → 3/4. Uma alternativa melhor fica para depois:
  desenhar por cima uma fileira de miniaturas (as do `wotm_savepics.hpp`) e saltar por passos até a escolhida.
- **Armadilha forte: a tela 8 chama `screenGetInput(1)` duas vezes no mesmo quadro** quando a fase em foco está destravada e
  `showTexCounter_2928 >= 0x2E` (a primeira chamada só confere "nada apertado" para soltar o filme). O `wotm_gamescreens::input` atual
  é de estado (conta tentativas, consome o clique) e devolveria o código na primeira chamada, que a tela descarta. Calcule o código uma
  vez por quadro do jogo e devolva o mesmo nas duas chamadas (ou só aja na chamada cujo RA é o do `switch`). As telas 7, 11..15 chamam
  `screenGetInput(1)` uma vez.

## Como saltar com segurança (todas)

Injetar um código por quadro e conferir o efeito no quadro seguinte (como o `input` genérico já faz) é seguro em todas: o jogo faz as
trocas de switch, sons e efeitos colaterais (filme, nome da fase, marcas). Direção pelo caminho mais curto com volta; teto de passos.
Escrever a variável direto só vale nas 12/13 (switch reaplicado todo quadro) e, com o `which` do switch junto, na 11. Na 8, 7, 14 e 15
não.

## Resumo do que implementar

| Tela | Alvos de hover | Variável | Hover → | Clique |
|---|---|---|---|---|
| 15 | linhas 1..3 (`m_menuItemSw[15][1..3]`, caminho ativo recursivo, sem brilho) | `currentSelection[15]` | 1 passo 1/2 | 6; nas setas (tex 190 do item 5, só com item 2) 3/4 |
| 12 | linha única (+ setas `m_menuObj[12]`) | `currentSelection[12]` 1..3 | nada | setas 3/4, resto 6 |
| 13 | idem (`m_menuObj[13]`) | `currentSelection[13]` 0..2 | nada | setas 3/4, resto 6 |
| 11 | dígito + título (+ setas `m_menuObj[11]`) | `currentSelection[11]` 0..4 | nada | setas 3/4, resto 6 |
| 14 | figura (`m_menuItemSw[14][0]`, filho TRN) | `currentSelection[14]` 1..3 | nada | terços 3/6/4 |
| 8 | foto (`m_sw5D0` filho `levels[cur]`) | `currentSelection[8]` (índice da lista) | nada | faixas 3/6/4; **mesmo código nas 2 chamadas do quadro** |
| 7 | 10 retratos + 4 trajes do jogador 1 | `currentSelection[7]`, `curCostume1_2912` | passos 3/4 ou 1/2 | passo e 6; nada com P1 feito |
