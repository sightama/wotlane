# Mouse nas telas 9 (carregar/salvar), 21 (unlocks) e nos diálogos sim/não (2026-10-22)

Especificação para a etapa 9 da câmera (mouse nos menus): passar o mouse sobre um elemento o escolhe, clique esquerdo confirma (X).
Só leitura do código; nada foi implementado. Fontes: `src/game/screen.cpp` (`screenLoadSave`, `screenUnlocker`,
`screenWriteInstructions`), `src/common/font.cpp`, o asm de `ShellFinished` (`displayDialog`, `areYouSure`, `getCurrentMenuSelection`,
`setMenuFont`, `checkInputForExit`) e de `McPage` (`McPage::update/updateMenuPos/drawMenu`, `McTextBox::update`, `McPageMgr::Input`),
`native/runtime/wotm_save.inc`, `wotm_gamescreens.inc`, `wotm_pausemenu.inc`, `wotm_pad.inc`, `native/wotm_mouse.hpp`.

## 0. Coordenadas do texto (vale para tudo abaixo)

- `fontSpritePrintXY(f, x, y, s)` e `fontSpritePrintCenteredXY(f, x, y, s)`: unidades da tela do PS2 (640 x 224, campo). O
  `fontBuildPrim` soma a origem da vista (centro menos metade da largura/altura), então **(x, y) é o canto de cima à esquerda do primeiro
  glifo**; no centrado, x é o centro da linha. **y é o topo, não o meio**: a linha ocupa `y .. y + altura`, com altura = `Font+0x0A / 16`
  e largura = `nchars * Font+0x0E / 16` (fontes em `D_00735890`, passo 0x38: `+0x08` largura do glifo, `+0x0A` altura, `+0x0C`
  altura da linha, `+0x0E` avanço, tudo em subpixels de 1/16).
- Janela para tela do PS2: o mesmo do `wotm_pausemenu::hoverItem`: `gr = gsScreenRect(rw, rh)`, `gx = (x - gr.x0) * 640 / gr.w`,
  `gy = y * 224 / rh`.
- A conferir: o `hoverItem` da pausa testa `|gy - linha| < 4.5`, ou seja, trata o y como meio da linha. Se o y é o topo (como o
  `fontBuildPrim` indica), a faixa da pausa está meia linha acima do texto. Vale olhar com o `WOTM_MENUMOUSELOG` antes de copiar a regra.
- O `McTextBox::update` passa o x,y guardado no `addTextString` direto para `fontSpritePrintXY`/`...CenteredXY` (fonte = `FontInfo+0x14`,
  tamanhos do `makeCurrentFont`: `+0x00` largura, `+0x04` altura, `+0x08`/`+0x0C` espaçamentos, em subpixels): o modal de saves e as
  páginas do cartão estão no mesmo sistema.

## 1. Tela 9 (`screenLoadSave`): carregar a aventura

### O que é a tela
- `screenLoadSave` não lê `screenGetInput` e não usa `currentSelection[9]`: a cada quadro chama `MonsterMc::updateStoryModeLoad`, que
  monta a sequência de páginas do cartão (`McPrepBoxScreen` → `McLoadGameBoxes` → `McSelectReel`) e roda `McPageMgr::update` até ela
  terminar (aí `changeScreen` para a escolha de fase ou de volta à tela 1).
- No port (com `WOTM_SAVEAUTO` ligado, o padrão), o `update` do `McLoadGameBoxes` é trocado pelo **modal nativo de saves**
  (`wotm_save.inc`, `boxesReplaced` → `modalUpdate(..., saving=false)`); o `McSelectReel` nunca abre. A mesma função serve a gravação na
  fase (`chooseReplaced`, `saving=true`, no lugar do `McChooseSlot`, depois do "salvar?" do `McAskToSave`, ver seção 4).
- O shell não tem objetos 3D para a tela 9: varrendo a hierarquia do `SHELL.NGP` a partir das raízes, nenhum nó tem os ids de ponto de
  fim 90..99, ou seja, `m_menuItemSw[9][*]` fica nulo (o `wotm_gamescreens::frame` não acha retângulos e não reivindica o clique).

### Fases
1. `McPrepBoxScreen`: a câmera do menu vai até as caixas (sem entrada; o modal ainda não existe).
2. Modal aberto (`g_modal.open`, `g_modal.page` = a página). Entrada lida do `McPageMgr::Input` (`kInputPressed = McPageMgr+0x1158`,
   `{máscara, apertado}` x 10; índices 0 cima, 1 baixo, 2 X, 3 triângulo, 4 bola, 5 quadrado, 6 start, 7 select, 8 esquerda, 9 direita).
   Os primeiros 6 vsyncs (`g_vsyncs - openedAt < 6`) não leem nada.
3. Gravar, linha ocupada: `m.confirm` (segundo X sobrescreve; triângulo ou mexer cancela a confirmação).

### Elementos escolhíveis e variável
- `g_modal.rows` (até 99; `Row::used`), o escolhido é **`g_modal.cur`**, a primeira visível é `g_modal.top`, 10 visíveis
  (`kVisibleRows`).
- Carregar pula as linhas vazias (`modalMove` só para em `used`); gravar aceita todas (a última é sempre o vazio do fim).
- Cima/baixo: 1 linha, com volta nas pontas; esquerda/direita: 10, para na ponta. X: carregar = `loadRow` + `finish(true)`;
  gravar = confirma ou grava (`saveRow`). Triângulo/bola: sai (`McPage::onBack`) ou desfaz a confirmação. Sons 2/4 ao mexer, 8 no X,
  6 ao sair.

### Onde aparece (desenho do port com o `McTextBox` do jogo, `modalDraw`)
- `Y(y)` = `y` no menu; na fase (gravar) `Y(y) = round(58 + (y - 25) * 0.7)`.
- Título "Save files": `(318, Y(40))`, alinhado à esquerda.
- **Linha k da lista** (k = i - top, 0..9): texto alinhado à esquerda em **x = 318** (`kRightX`), **y = Y(60 + 14k)**. Passo 14 no menu
  (9,8 na fase). Fontes: `s_selectedFont` x0,52 no escolhido, `s_unselectedFont` x0,7 nas outras (alturas lidas em tempo de execução
  das cópias em `kFontsBuf`: `+0x04` de `kFontsBuf + 2*0x28` e `+ 3*0x28`; para o teste basta a faixa do passo).
- Faixa sugerida para a linha k: `gx` em `[310, 630]`, `gy` em `[Y(60 + 14k) - 2, Y(60 + 14(k+1)) - 2)`. A metade esquerda
  (x < 300: logo, foto e monstro 3D do `wotm_savepics.hpp`, desenhados pela janela; texto em x 30 e centro 164) não é escolhível.
- Com `m.confirm` a esquerda mostra "Overwrite save file N?" / "Press X again to overwrite" (`(30, Y(40))`, `(30, Y(70))`), sem botão.

### Como implementar
- Tudo dentro do `modalUpdate` (thread do EE), depois da guarda dos 6 vsyncs, antes do teste dos botões:
  1. `wotm::mouse::gameMenu()` e `wotm::mouse::claim()` a cada quadro com o modal aberto (ver armadilhas: sem isso o mouse está no mapa
     da luta).
  2. Ponteiro (`wotm::mouse::pointer`) → `gx, gy`; linha `k` sob ele → `i = top + k`, válida se `i < rows.size()` e (`saving` ou
     `rows[i].used`).
  3. Hover: só quando `g_moveSeq` mudou (como nas outras telas) e `i != cur`: `cur = i`, `confirm = false`, som 2 ou 4. `top` não muda
     (a linha já está visível).
  4. Clique (`takeClick(g_clickL)`) numa linha válida: `cur = i` e seguir o mesmo caminho do X (carregar, confirmar ou gravar). Clique
     fora da lista: ignorar (não vira X, porque o modal reivindicou).
- Rodinha: com `gameMenu()` ligado, o `wotm_pad.inc` já a transforma em cima/baixo do pad, que o `McPageMgr::Input` lê: rola a lista
  sem mais nada. Botão direito vira triângulo (sair), como nas outras telas.
- Saltar direto é seguro: `cur` é só do port, o jogo não o lê; o `top` só precisa conter `cur` (`clamp` do `modalMove`).

### Armadilhas
- **Na tela 9 o jogo nunca chama `screenGetInput`**, então o `markMenuFrame` do `wotm_mainmenu.inc` não roda e `inMenus()` fica falso
  durante o modal: hoje o mouse está no mapa do teclado da luta (botão esquerdo = quadrado, direito = triângulo, rodinha = troca de alvo,
  sem bit de pad) e o `MenuConfirm` (troca X/bola dos gamepads) não vale ali. O `gameMenu()` resolve o mouse; o `MenuConfirm` é outra
  conversa (talvez chamar `markMenuFrame` também no modal; decidir com o dono).
- Na fase (gravar) é a mesma coisa: estamos dentro do `displayDialog` (`wonLevelOverlay`), sem `screenGetInput`.
- O clique que fecha o "salvar?" (X por pad do `g_crossReq` ou clique) pode chegar no modal: a guarda dos 6 vsyncs cobre o pad; o
  `takeClick` tem janela de 300 ms, então zere o clique pendente no `modalOpen` (`takeClick(g_clickL)` descartando) para um clique no
  "YES" não gravar no slot sob o cursor.
- Gravar sobre linha ocupada pede o segundo X: com o mouse, o primeiro clique liga `confirm`, o segundo grava. Hover numa outra linha
  desliga `confirm` (como o direcional).
- Não usar `currentSelection[9]` nem o `wotm_gamescreens::input` (não roda aqui). Sugiro pôr 9 no `wotm_gamescreens::handled`, para o
  `frame` nem procurar switches.

## 2. Tela 21 (`screenUnlocker`): destraves

### Estado (estáticos da função, nomes de `wotm_syms.h`)
| variável | símbolo | papel |
|---|---|---|
| grupo | `wotm::sym::m_2989` | 0..9 monstros (ordem `kGroupMonsters` do `wotm_save.inc`), 10 fases, 11 minigames |
| cursor | `wotm::sym::s_2990` | linha escolhida (0..3; 0..2 no grupo 11) |
| escolhido desenhado | `wotm::sym::currentMenuSelection_2985` | cópia do cursor feita **antes** do `screenGetInput` |
| itens | `wotm::sym::items_2992` | 4, ou 3 no grupo 11 |
| fade | `wotm::sym::fade_2991` | 0 → 0x80, +4 por quadro ao entrar |
| título | `wotm::sym::UnlockTitle` | = grupo (o `screenWriteInstructions` escolhe "MONSTER/LEVEL/MINIGAME UNLOCKS") |

`currentSelection[21]` não é usado. Entradas: `monsterSelectMode[grupo * 4 + linha]` (`SelectSlot`, passo 0x2C, `+4` state, `+8` custo).

### Códigos
- 1/2 (cima/baixo): cursor -1/+1 com volta (0..3, ou 0..2 no grupo 11); sons 2/4.
- 3/4 (esquerda/direita): grupo -1/+1 com volta (0..11), cursor = 0, `UnlockTitle` = grupo; sons 5/3.
- 6 (X): compra a linha `currentMenuSelection_2985` se travada e com tokens (`Shell+0x2B24`): `g_monsterToUnlock/g_skinToUnlock` →
  `saveUnlocks` (no port grava o `save.json` na hora) → -1 nos dois. Senão mensagem: "CANNOT UNLOCK THIS ITEM HERE!" (grupo 5, linha 3
  travada), "MORE TOKENS NEEDED!", "UNLOCK <monstro> FIRST!" (traje com o monstro travado). Som 8 sempre.
- 5 (triângulo): volta para `prevScreen`.

### Onde aparece (só texto 2D; não há objeto 3D)
- `m_menuItemSw[21][*]` **nunca existe**: no `Shell::AddEpNode` os ids 200..220 caem antes no `m_menuObj[id - 200]`, então 210..219
  são `m_menuObj[10..19]`, e não há `m_menuObj[21]`. O `wotm_gamescreens::frame` não acha nada nesta tela.
- Linha i (0..itens-1): **y = 0x52 + 11i** (82, 93, 104, 115), nome centrado em **x = 0xE6 (230)**, estado/custo centrado em
  **x = 0x1C2 (450)** ("UNLOCKED", "FREEBIE", "SPECIAL" ou o texto do custo).
  - Escolhida: fonte 0, `fontSetCharSizesInSubPixels(0xFF, 0x96, -0x13, -0x15)`: altura 9,4 px, avanço ~14,8 px. Outras: fonte 1,
    `(0xBE, 0x78, -0x13, -0x15)`: altura 7,5 px, avanço ~10,7 px.
  - Faixa sugerida: `gy` em `[0x52 + 11i - 1, 0x52 + 11i + 10)`, `gx` em `[90, 560]` (cobre nome e estado; o nome mais longo cabe).
- Outros textos (não escolhíveis): título (320, 10); mensagem (320, 60); " BATTLE TOKENS!" (320, 140) e o número (320, 147);
  instruções (320, 205).
- Não há setas para trocar de grupo. Para o mouse: rodinha já dá cima/baixo; troca de grupo pelo teclado/pad ou, se o dono quiser,
  setas nativas ao lado do título (por exemplo zonas clicáveis em `gx < 200` e `gx > 440` na faixa `gy` 6..24, que devolvem 3/4).

### Como implementar (saída do `screenGetInput(1)` da tela 21, onde o `wotm_gamescreens::input` é chamado hoje)
- Tirar a 21 do caminho genérico (pôr no `handled`) e tratar à parte, com `code` = o que o jogo leu:
  - `code != 0`: devolver como está (teclado/pad mandam).
  - `wotm::mouse::claim()` a cada quadro da tela com o fade completo (`fade_2991 >= 0x80`), para o clique esquerdo solto não virar X.
  - Linha `h` sob o ponteiro (só `h < items_2992`).
  - Hover com `g_moveSeq` mudado e `h != s_2990`: escrever **`s_2990 = h` e `currentMenuSelection_2985 = h`** e tocar o som 2/4
    (`playShellSFXSound` no `Shell+0x2918`, como o `wotm_save.inc::sfx`); devolver 0. O desenho do quadro seguinte usa o escolhido.
  - Clique numa linha: escrever as duas variáveis = `h` e **devolver 6**: a cópia para `currentMenuSelection_2985` já aconteceu antes do
    `screenGetInput`, mas como escrevemos as duas, o `case 6` usa `h` no mesmo quadro (e o teste do grupo 5 linha 3, que compara cursor
    e escolhido, continua coerente). Um passo só, sem andar com cima/baixo.
- Saltar direto é seguro: as duas variáveis são só índice; a validade vem de `h < items_2992` (no grupo 11 não existe linha 3).

### Armadilhas
- **Hoje, na tela 21, um clique esquerdo em qualquer lugar compra a linha escolhida**: nada reivindica o clique, `inMenus()` é verdade
  (a tela chama `screenGetInput`), e o `wotm_pad.inc` transforma o botão esquerdo em X. Compra gasta tokens e grava o JSON na hora.
- O `currentMenuSelection_2985` é copiado do cursor antes da leitura: escrever só o `s_2990` faz o X do mesmo quadro comprar a linha
  antiga.
- Durante o fade de entrada (32 quadros) o texto ainda não aparece: ignorar hover e clique até `fade_2991 >= 0x80`.
- Cliques rápidos: cada clique devolve um 6 (uma compra); não repetir o 6 enquanto o botão está apertado (usar só o `takeClick`).

## 3. Diálogos do `displayDialog`: "ARE YOU SURE?" e os sim/não do fim de luta

### Estrutura comum (todos em `ShellFinished`, só asm)
- `displayDialog(which)` roda o próprio laço de quadros e despacha por estado para `pauseLevelOverlay`, `areYouSure`,
  `lostLevelOverlay`, `wonLevelOverlay`, `finishedMultPlay*`, `selectNext*`, `wonStoryOverlay`, `lostEnduranceOverlay`, `finished*`
  (minigames). Nenhum chama `screenGetInput`.
- Escolha: `getCurrentMenuSelection(n, jogador)` lê baixo (0x40) e cima (0x10) do pad (jogador -1 = todos), anda com volta em `0..n-1`,
  toca `playPauseMenuSound(0x50)` e devolve **`gCurrentMenuSelection`** (`wotm::sym::gCurrentMenuSelection`).
- Desenho: para cada item i, `setMenuFont(i)` (i == `gCurrentMenuSelection` → tamanho 20, vermelho; senão tamanho 15, cinza) seguido de
  `fontSpritePrintCenteredXY(0, 320, y_i, texto)`. **O `setMenuFont(i)` marca o item**: o índice dele é o valor da escolha.
- Confirmação: `checkInputForExit(jogador, ...)` = X (0x4000) num pad (o segundo argumento, quando não nulo, aceita também o 0x08).
- Desenho só com `gDrawOverlay != 0` (o cálculo da escolha roda sempre).

### "ARE YOU SURE?" (`areYouSure`, depois do QUIT da pausa)
- `getCurrentMenuSelection(2, gPlayerThatPaused)` (ou outro jogador conforme `calledFrom`).
- Itens: **0 = "YES" em (320, 99)**, **1 = "NO" em (320, 108)**; título "ARE YOU SURE?" em (320, 66), tamanho 30. Alturas: 10 px no
  escolhido, 7,5 px no outro; faixas sugeridas YES `gy` 98..107, NO 107..118, `|gx - 320| < 60`.
- X em YES: sai da luta (zera estados do Shell, som 0x4F, devolve 1). X em NO: volta ao chamador (`*estado = calledFrom`),
  `gCurrentMenuSelection = gLastMenuSelection` (o cursor da pausa volta ao QUIT). Não há "voltar" por triângulo.
- `lostLevelOverlay` tem o mesmo desenho (2 itens em y 99 e 108, título em 66).

### Como implementar (genérico, cobre todos os overlays)
- Ganchos: `setMenuFont__Fi` (guarda "o próximo `fontSpritePrintCenteredXY` é o item i") e o `fontSpritePrintCenteredXY` que o
  `wotm_pausemenu` já intercepta (grava `{i, x, y, largura, altura}` a partir do `Font` da fonte 0 no momento: `nchars * avanço/16`,
  `altura/16`; zera a marca). Limpar a tabela a cada entrada do `getCurrentMenuSelection`.
- No retorno do `getCurrentMenuSelection(n, p)` fora da pausa (`!g_inOverlay`, `a0 == n`), dentro do `displayDialog` e com
  `gDrawOverlay`: `wotm::mouse::gameMenu()` + `claim()`; item i < n sob o ponteiro (retângulos do quadro anterior: as posições não mudam
  entre quadros); hover com `g_moveSeq` mudado → `gCurrentMenuSelection = i` (e `$v0 = i`), som `playPauseMenuSound(0x50)`; clique →
  `gCurrentMenuSelection = i` e `g_crossReq = 2` (o X por duas leituras do pad, como na pausa).
- A pausa (`n == 5`) já tem o seu: manter o `selHook` da pausa para ela e deixar o genérico para o resto (ou trocar a pausa pelo
  genérico, que já sabe do QUIT movido, porque grava o y depois da troca feita pelo `printHook`... cuidado com a ordem dos dois ganchos
  no mesmo endereço).

### Armadilhas
- Hoje o `gameMenu()` só é chamado dentro do `pauseLevelOverlay`: no "ARE YOU SURE?" e nos fins de luta o mouse está no mapa da luta
  (esquerdo = quadrado, nada acontece; direito = triângulo).
- O `gCurrentMenuSelection` é um só para todos os overlays; o NO do `areYouSure` o restaura a partir do `gLastMenuSelection`. Escrever
  só índices `< n`.
- O clique do QUIT na pausa (X por 2 leituras via `g_crossReq`) pode cair já no "ARE YOU SURE?": no primeiro quadro do diálogo não
  aceitar clique nem `g_crossReq` pendente (zerar ao entrar), senão o X confirma o item escolhido de cara.
- Nem todo overlay tem só itens após `setMenuFont`: imprimir o título usa `setTitleFont`; zere a marca nele e no `fontSetDefaultSize`
  para não tomar o título por item.

## 4. Páginas de cartão com menu (`McPage`): o "salvar?" (`McAskToSave`)

Na fase, depois de vencer na aventura, `updateStoryModeSave` mostra o `McAskToSave` (pergunta e sim/não) antes do modal de gravar. Com
`WOTM_SAVEAUTO=0` voltam todas as outras páginas do cartão, que usam o mesmo menu.
- `McPage::update` = `updateMenuPos` → `drawMenu` → `checkForSelection` (o `McAskToSave::update` chama o `McPage::update`).
- Página `pg`: itens `pg+0x21C` (contagem), **escolhido `pg+0x224`**, item i selecionável se a palavra em `pg + 0x14 + 0x24*i` não é 0
  (o `updateMenuPos` pula os outros). Posição: centrado em **x = 320**, **y = `pg+0x24C` + i * `pg+0x250`** mais o `+0x10` da fonte
  (`s_selectedFont` no escolhido, `s_unselectedFont` nos outros); altura = `FontInfo+0x04 / 16`. Rodapé em (320, 205).
- X: o `checkForSelection` lê `McPageMgr+0x1168` (o "apertado" do X, índice 2 do `McPageMgr::Input`).
- Implementação: gancho na saída do `updateMenuPos__6McPageR7McState` (`a0` = página): `gameMenu()` + `claim()`; item i selecionável
  sob o ponteiro → `pg+0x224 = i` (hover com `g_moveSeq`, som `playPauseMenuSound(0x50)` como o jogo); clique → `pg+0x224 = i` e
  `McPageMgr+0x1168 = 1`, que o `checkForSelection` do mesmo quadro consome (sem passar pelo pad).
- Armadilha: o `McLoadGameBoxes` e o `McChooseSlot` (o modal) não passam por `McPage::update`; o gancho não os afeta. O mesmo clique do
  "YES" não pode chegar ao modal (seção 1).

## Resumo do que implementar
1. Tela 9 / modal de saves (`wotm_save.inc::modalUpdate`): `gameMenu()` + `claim()` com o modal aberto; linha k em x ≥ 310,
   y = Y(60 + 14k) (faixa do passo); hover → `g_modal.cur`, clique → mesmo caminho do X; descartar clique pendente no `modalOpen`.
2. Tela 21: tirar do genérico; na saída do `screenGetInput(1)`, linha i em y = 82 + 11i, x 90..560; hover escreve `s_2990` e
   `currentMenuSelection_2985`; clique escreve os dois e devolve 6; `claim()` sempre (hoje o clique solto compra).
3. Diálogos do `displayDialog`: ganchos `setMenuFont` + `fontSpritePrintCenteredXY` montam os retângulos; no retorno do
   `getCurrentMenuSelection(n)` (fora da pausa) hover → `gCurrentMenuSelection`, clique → `g_crossReq = 2`; `gameMenu()` + `claim()`.
4. Opcional: páginas `McPage` (o "salvar?" na fase) pelo `updateMenuPos`: `pg+0x224` e `McPageMgr+0x1168`.
