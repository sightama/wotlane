# Opções do modo treino no menu de pausa (2026-10-23)

Pedido do dono: tirar as opções do modo treino do menu F2 (`wotm_trainview::draw2d`, texto solto e teclas 1..0) e pô-las no menu de
pausa (Start) da luta, com a mesma cara dos menus do jogo. Só leitura; nada foi editado. Números de linha da branch `modo-treino`
(fd0a6f2 + o `wotm_mainmenu.inc` modificado na árvore).

## 1. Como a pausa nativa funciona hoje

### A lista é a do jogo
- `pauseLevelOverlay__FRi` (retail, `asm/game/ShellFinished.s`) desenha os 5 itens: chama `getCurrentMenuSelection(5, gPlayerThatPaused)`
  e, com `gDrawOverlay`, faz `setMenuFont(i)` + `fontSpritePrintCenteredXY(0, 0x140, y, texto)` para i = 0..4, y = 0x55, 0x5E, 0x67,
  0x70, 0x79 (passo 9, unidades da tela do PS2 640 x 224). `setMenuFont(i)` lê `gCurrentMenuSelection` e acende o item igual a ele
  (tamanho/cor via `fontSetSize`/`fontSetColor`).
- Depois do `checkInputForExit`, o overlay decide pelo item (`s2`): 0 CONTINUE, 1 RESTART, 2 e 3 nada (a vibração e o esquema mudam
  com esquerda/direita antes), 4 QUIT (diálogo ARE YOU SURE?). **Qualquer outro índice (5) cai no "não faz nada" e devolve 0**: um
  sexto item não quebra a lógica do jogo. Com o Start (o bool na pilha) o `movn` zera o item: Start sempre sai da pausa.
- O texto é capturado pela camada nativa como o resto do texto do jogo (fonte do jogo, mesmo desenho do HUD).

### Ganchos (`native/runtime/wotm_pausemenu.inc`, namespace `wotm_pausemenu`)
| o quê | linha |
|---|---|
| `kItems = 5`, `kOptions = 3`, `kLineY[]`, máscaras `kCross 0x4000`, `kStart 0x08`, `kLeft 0x80`, `kRight 0x20` | 16-19 |
| `widenBox` (alarga a caixa 3D `shell->m_cs610`, linha 0 da matriz x1,4) | 40-54 |
| `overlayHook` (`pauseLevelOverlay`): marca `g_inOverlay`, `wotm::mouse::gameMenu()`, chama `wotm_mainmenu::pauseFrame`; se o painel está aberto devolve 0 sem rodar a original | 56-78 |
| `printHook` (`fontSpritePrintCenteredXY`): troca "CONTROL TYPE x" por "RECOMPILED OPTIONS" escrevendo no próprio buffer do jogo | 91-109 |
| `hoverItem` (mouse: item sob o cursor, usa `kItems`/`kLineY`, meia largura 130) | 199-207 |
| `selHook` (`getCurrentMenuSelection`): bloqueia com `pauseBlocked()`, mouse escolhe/clica (`requestCross(2)`), som `playPauseMenuSound(0x50)` | 210-266 |
| `inputHook` (`inputGetInput`): esquerda/direita no item 3 dão 0; tudo 0 com `pauseBlocked()`; anota `g_sawCross`/`g_sawStart` dentro do `checkInputForExit` | 273-292 |
| `exitHook` (`checkInputForExit`): X (sem Start) com `gCurrentMenuSelection == kOptions` chama `wotm_mainmenu::openPausePanel` e devolve 0 | 296-320 |
| `install` | 322-340 |

Os códigos de botão que chegam ao jogo são as máscaras do `inputGetInput` (acima). O painel **não** usa esses códigos: lê os
dispositivos direto (abaixo).

### O painel "Recompiled options" na pausa (`native/runtime/wotm_mainmenu.inc`, namespace `wotm_mainmenu`)
Thread do EE (dentro do gancho do overlay):
| o quê | linha |
|---|---|
| `struct View` (estado publicado para a janela: `on`, `screen` (-1 = pausa), `panel`, `tab`, `opt`, `top`, `preview`, `bindMap`, `capture`...) | 69-86 |
| `g_m` (mutex), `g_view`, `enum HoverPart`, `struct Hover`, `g_hover`, `g_pauseView`, `g_pauseHover` | 87-100 |
| `enum Where {InMenu, InPause}`, `g_where`, `g_pauseOpen`, `g_pausePanel`, `g_pauseSuppress` | 112-116 |
| `enum Tab`, `kTabNames`, `struct Opt {st::Key key; label; hint}`, `TabDef`, `kTabDefs`, `g_tab`, `g_optSel[]`, `kVisRows = 7`, `visRows` | 122-158 |
| `tabRows`, `choices`, `stepValue`, `valueText` (valor em texto por `st::Key`) | 188-293 |
| `enum PanelAct {PUp, PDown, PLeft, PRight, POk, PBack, PPrev, PNext, PClear, PPreview}`, `g_panelPrev`, `panelKey`, `panelPad`, `readPanel` (teclado pelo mapa + Enter/Esc/Backspace/Q/E/Tab/Delete/P; gamepads pelo mapa e analógico; roteiro `WOTM_PADSEL_SCRIPT`) | 299-362 |
| `panelSfx` (sons do Shell 2/4 cursor, 6 voltar, 8 ok; na luta, cópia do banco pelo som nativo) | 371-382 |
| `openPausePanel` (abre: `g_where = InPause`, `g_pauseOpen`, `readPanel(g_panelPrev)` para o X que abriu não contar, consome o clique) | 384-396 |
| `lockedOpt` (KEYBOARD travado na pausa) | 486 |
| `panelInput` (um quadro: bordas, mouse pelo `g_pauseHover`, voltar, abas, cima/baixo com rolagem, esquerda/direita/X por `stepValue` + `st::set`); devolve true quando fecha | 490-587 |
| `publishPause` (copia o estado para `g_pauseView` sob `g_m`) | 591-606 |
| `pauseFrame` (chamado pelo `overlayHook`: com o painel aberto desliga a caixa 3D por `hierSetCsDrawMe(cs, 0)`, roda `panelInput`, ao fechar `g_pauseSuppress = 20` até soltarem tudo; anima `g_pausePanel` 0,1/quadro; publica) | 610-638 |
| `pauseBlocked`, `pauseDialogEnd` | 639-650 |

Thread da janela (raylib):
| o quê | linha |
|---|---|
| `label`, `labelLeft`, `textWidth`, `glyphAdvance`, `gsToRgb` (fonte do jogo pelo atlas `tex0`; sem atlas, Bahnschrift) | 1020-1070 |
| `kPW 1800`, `kPH 1100`, `g_prt` (textura do painel), `struct PanelLayout`, `panelLayout(tab, preview)` | 1073-1098 |
| `kLit`, `kDim` (cores, escala do GS) | 1134 |
| `renderPanel(scene, ram, tex0, View)`: fundo arredondado, abas + L1/R1, fio; monta `PRow {label, value, hint, arrows, kind, bind, locked}` e desenha nome à esquerda (`labelLeft` em `L.labX`), fio, valor centrado em `L.valX` com setas em `valX ± dx` na linha escolhida, faixa da linha escolhida, setinhas de rolagem; dica (`wotm_legend::hint`) e legenda de botões (`wotm_legend::post`, faixa 0,55 na pausa) | 1136-1335 |
| `drawPausePanel2d(rw, rh)` (põe `g_prt` no centro do quadro, véu, `wotm::mouse::claim()`, `panelHit` → `g_pauseHover`) | 1473-1504 |
| `renderPauseUi` (chama `renderPanel` com `g_pauseView`) | 1506-1515 |
| `panelHit(u, w, View, Hover&)` (mesmas medidas do `renderPanel`: faixa das abas, linhas a partir de `lineH*2.2`, passo `L.step`, setas `HRowLeft`/`HRowRight`) | 1519-1543 |

Chamadas no quadro (`native/runtime/wotm_live.inc`): `renderPauseUi` na 1647 (fora do BeginTextureMode do quadro), `drawPausePanel2d`
na 1732 (depois do texto do jogo, antes da legenda 1759), painel de dano do treino 1764, `wotm_trainview::draw2d(open)` (F2) na 1869.
`wotm_legend::post`/`hint` estão em `native/runtime/wotm_prompts.inc:279/287`.

### Modo treino (estado)
`native/wotm_training.h`: `g_on` (25), `g_opts` com os bits `kShowAttacks`, `kShowHulls`, `kShowCapsule`, `kRefill`, `kPanel`,
`kShowInputs` (13-19), `enum Dummy {DStand, DCpu, DBlock, DBlockAfter}` + `g_dummy` (23, 27), `g_dummyHealth` (28), `enum Meters {MOff,
MFull, MRage}` + `g_meters` (30-31), `g_resetStats` (32). Todos `std::atomic`. Lidos na thread do EE em `wotm_training.inc` (275, 472,
483, 493, 518) e pela janela no desenho. O menu F2 é `wotm_trainview::draw2d` (`wotm_trainview.inc:371-444`), com a lógica de cada
opção (a recarga cicla OFF → 100 → 75 → 50 → 25 → 10 → OFF mexendo em `kRefill` e `g_dummyHealth`, linhas 399-410) e os textos
(`kDummy`, `kMeterNames`, 418/431).

## 2. Recomendação

### 2a. Item "TRAINING OPTIONS" na lista da pausa (sexto item, só com `wotm_training::g_on`)
O mais barato e com a cara do jogo é um **sexto item desenhado pelas próprias funções de fonte do jogo**, depois do QUIT:
1. `selHook`: dentro do overlay, com `a0 == kItems` e `g_on`, trocar `$a0` para 6 antes de chamar a original (o cursor passa a ir de
   0 a 5 e dá a volta). O retail não faz nada com o item 5 (seção 1).
2. `overlayHook`: depois da original voltar (`ctx->pc == g_overlayRa`), ainda com `g_inOverlay` verdadeiro, se `g_on` e
   `rd32(gDrawOverlay)`: `callGuest(setMenuFont, 5)` e `callGuest(fontSpritePrintCenteredXY, 0, 0x140, 0x82, <texto>)`. O texto tem de
   estar na RAM do PS2: um buffer próprio no estilo do `wotm_modes4::scratchText` (`wotm_modes4.inc:58, 105`; `kScratch = PS2_RAM_SIZE -
   0x800`, 8 x 0x80). Esse fica em outra TU (`wotm_overrides.cpp`) e é usado nos minigames: usar uma vaga própria (por ex. uma constante
   em `wotm_pausemenu` uns 0x40 bytes fora das 8 vagas e dos ViewDef do split4; conferir o que há abaixo de `kScratch`) e nunca o
   scratchpad. Fazer as duas chamadas com `g_inOverlay` ainda ligado: senão o `menuFontHook`/`printHook` gravam o item como item de
   diálogo (`g_dlg[5]`). Depois, `fontSetDefaultSize(0)`/`fontSetDefaultColor(0)` se o texto seguinte do quadro sair com a fonte do menu.
3. `kLineY`/`hoverItem`/`kItems`: virar contagem dinâmica (6 com o treino, y 0x82) para o mouse achar o item.
4. `exitHook`: X (sem Start) com `gCurrentMenuSelection == 5` abre o painel do treino (`openPausePanel` com o modo treino, ver 2b) e
   devolve 0, igual ao RECOMPILED OPTIONS. `inputHook`: esquerda/direita no item 5 não fazem nada no retail, nada a bloquear (conferir
   no asm do overlay, linhas do `beq $v1, ...` depois do `inputGetInput` de 0x80/0x20).
5. **Caixa:** a caixa 3D (`m_cs610`) foi feita para 5 linhas; a sexta (0x82) pode passar do pé dela. `widenBox` só escala a linha 0
   (largura); talvez precise escalar a linha 1 (altura) e/ou descer o centro. Conferir numa foto antes de mexer.
6. Alternativa se a caixa der trabalho ou o dono quiser o item antes do QUIT: trocar índices é caro (o jogo testa `s2 == 4` para o
   QUIT); a opção sem sexto item seria o `printHook` renomear outra linha, o que tira uma função do jogo. Sexto item no fim é o melhor.

### 2b. O painel: reaproveitar o painel existente num "modo treino", não abrir um painel novo
Usar a máquina inteira da pausa (`g_pauseOpen`, `g_pausePanel`, `g_pauseSuppress`, `pauseFrame`, `pauseBlocked`, `publishPause`,
`drawPausePanel2d`, `renderPauseUi`, `panelSfx`, `readPanel`, legenda/dica) e só trocar **as linhas** e **o cabeçalho**. As opções do
treino não são `st::Key` (não vão para o `settings.ini`, são atômicos do `wotm_training.h`), então não cabem no `kTabDefs`/`Opt`; uma
aba nova no `kTabDefs` obrigaria a criar chaves de settings e mudaria `kTabs`/`tabX`/`panelHit` das abas. Por isso:

- **Estado (EE):** `static bool g_pauseTraining` (o painel aberto pelo TRAINING OPTIONS) e `static int g_trainSel` (linha). Um
  `openPausePanel(rdram, ctx, runtime, bool training)` (ou um `openTrainingPanel` que chama o mesmo corpo) liga `g_pauseTraining`.
  `pauseFrame` fica igual; ao fechar, desligar `g_pauseTraining`. `pauseDialogEnd` também o zera.
- **View:** dois campos novos, `bool training` e o `opt` com `g_trainSel` (e `top` próprio se rolar); `publishPause` os copia.
- **Entrada (EE):** no `panelInput`, logo depois do bloco do mouse e do `PBack` (que já fecha e toca o som 6), se `g_where == InPause
  && g_pauseTraining`: chamar um `trainingInput(rdram, ctx, runtime, e)` e retornar. Nele: `PUp`/`PDown` andam `g_trainSel` (com volta e
  som 2/4), `PLeft`/`PRight`/`POk` trocam o valor (som 2/4/8), `PPrev`/`PNext` ignorados (sem abas), `PClear` nada. O bloco do mouse do
  `panelInput` usa `tabRows(g_tab)` e `g_optSel[g_tab]` e trata `HTab`: com o treino, usar a contagem/linha do treino e ignorar
  `HTab`/`HPrevTab`/`HNextTab` (ou fazer um `rows()`/`sel()` que já escolhe).
- **Linhas do treino** (uma tabela `constexpr` com rótulo e dica, perto do `kTabDefs`), cada uma com `get`/`step` sobre os atômicos,
  aproveitando a lógica do `draw2d`:
  | linha | valores | estado |
  |---|---|---|
  | ATTACK SPHERES | ON/OFF | bit `kShowAttacks` |
  | BODY BOXES | ON/OFF | `kShowHulls` |
  | PUSH CAPSULES | ON/OFF | `kShowCapsule` |
  | DUMMY | STAND STILL / CPU FIGHTS / ALWAYS BLOCK / BLOCK AFTER 1ST HIT | `g_dummy` (0..3) |
  | HEALTH REFILL | OFF / 100 / 75 / 50 / 25 / 10 (a fonte do jogo não tem `%`: "100 PERCENT" ou só o número; ver `valueText` do RENDER SCALE) | `kRefill` + `g_dummyHealth` |
  | STAMINA AND SPECIAL | OFF / ALWAYS FULL / ALWAYS RAGE | `g_meters` |
  | INPUT DISPLAY | ON/OFF | `kShowInputs` |
  | DAMAGE PANEL | ON/OFF | `kPanel` |
  | CLEAR DAMAGE LOG | ação (só X, sem valor nem setas, como o RESTORE DEFAULTS) | `g_resetStats = true` |

  Escrever os bits com `g_opts.fetch_xor(bit)` (ou `fetch_or`/`fetch_and`), não `load` + `store`: o F2 (se ficar) escreve da janela.
  Listas param nas pontas e X dá a volta, como o `stepValue` (`wrap = e[POk]`).
- **Desenho (janela):** no `renderPanel`, com `v.training`: no lugar das abas e dos badges L1/R1, o título "TRAINING OPTIONS" centrado
  (`label` em `W*0.5, tabY`, `kLit`) com o fio embaixo; montar `rows[]` como `PRow{rótulo, valor, dica, true, KOpt}` (a "CLEAR DAMAGE
  LOG" como `KReset`: sem valor, sem setas) lendo os atômicos direto (como o `optRow` lê `st::get`). O laço de desenho das linhas, a
  rolagem, a dica e a legenda ficam como estão; a legenda sem "TABS": `{left, right, "CHANGE"}, {back, {}, "BACK"}` (e `{ok, {},
  "CLEAR"}` na última). Sem prévia (`L.preview` falso: usar o layout das abas comuns).
- **Rolagem / caber:** são 9 linhas e `kVisRows = 7`; o mecanismo de rolagem (`v.top`, setinhas) já funciona se o treino guardar o
  seu `top`. Para caber tudo sem rolar: um layout próprio no `panelLayout` (passo ~0,8 `lineH`, `vis = 9`) e começar as linhas mais
  acima, já que não há abas; o início `lineH * 2.2f` está fixo no `renderPanel` (1230, 1286) e no `panelHit` (1537): virar um campo
  `y0` do `PanelLayout` para os dois baterem.
- **Mouse:** `panelHit` com `v.training`: pular o teste da faixa das abas (1524-1533) e usar a contagem das linhas do treino em vez
  de `tabRows(tab)` (1536). O resto (linha, setas) serve igual.
- **F2:** o `draw2d` pode perder o menu de texto (fica só o painel de dano/entradas); se o F2 continuar como atalho para ligar o modo
  treino, decidir com o dono. A linha "Training mode ON/OFF" do F2 não entra na pausa (o item só aparece com o treino ligado).

### Cuidados
- **Threads:** `panelInput`/`trainingInput`/`pauseFrame` rodam na thread do EE (dentro do gancho do `pauseLevelOverlay`), e é lá que
  se escrevem os atômicos do treino e a `View`; `renderPanel`/`drawPausePanel2d`/`panelHit` rodam na janela e só leem (`g_pauseView`
  copiado sob `g_m`; atômicos direto). `g_pauseHover` é escrito pela janela e lido pelo EE, sempre sob `g_m`. Não chamar raylib de
  desenho na thread do EE (o `readPanel` já usa `IsKeyDown`/`IsGamepadButtonDown` lá; é o padrão do projeto).
- **`callGuest` só na thread do EE** e com o `ctx` do gancho (as chamadas do sexto item no `overlayHook`).
- **Mouse:** o clique que abre não pode contar dentro do painel (o `openPausePanel` já faz `takeClick(g_clickL)` e zera
  `g_crossReq`); o `hoverItem` precisa do sexto y; o `drawPausePanel2d` já faz `claim()`.
- **Fechar:** o triângulo/Esc que fecha ainda está apertado: o `g_pauseSuppress = 20` existente cobre (não inventar outro).
- **Estado do menu principal:** `g_tab`/`g_optSel` são compartilhados com o menu; o treino usa os seus (`g_trainSel`), para o painel
  normal continuar abrindo na última aba.
- **Lista de dano (`g_resetStats`):** é só pedir; quem limpa é a janela/EE que já trata a flag (comportamento do F2).
- Variável de diagnóstico nova (se houver) entra na tabela do `NATIVO.md`.

## 3. Resumo dos nomes
- `wotm_pausemenu` (`native/runtime/wotm_pausemenu.inc`): `kItems` 16, `kOptions` 17, `kLineY` 18, `overlayHook` 56, `printHook` 91,
  `hoverItem` 199, `selHook` 210, `inputHook` 273, `exitHook` 296, `install` 322.
- `wotm_mainmenu` (`native/runtime/wotm_mainmenu.inc`): `View` 69, `g_m` 87, `Hover`/`HoverPart` 90-95, `g_pauseView`/`g_pauseHover`
  99-100, `Where`/`g_where`/`g_pauseOpen`/`g_pausePanel`/`g_pauseSuppress` 112-116, `kTabDefs` 127, `g_tab`/`g_optSel` 154-155,
  `kVisRows` 156, `tabRows` 188, `stepValue` 237, `valueText` 254, `PanelAct` 299, `readPanel` 317, `panelSfx` 372, `openPausePanel`
  384, `panelInput` 490, `publishPause` 591, `pauseFrame` 610, `pauseBlocked` 639, `pauseDialogEnd` 641, `label`/`labelLeft` 1042/1066,
  `PanelLayout`/`panelLayout` 1078/1085, `kLit`/`kDim` 1134, `renderPanel` 1136, `drawPausePanel2d` 1473, `renderPauseUi` 1506,
  `panelHit` 1519.
- `wotm_padsel::callGuest` (`wotm_padsel.inc:246`), `wotm_padsel::kShellSound` (46); `wotm_modes4::scratchText` (`wotm_modes4.inc:105`).
- `wotm_legend::post`/`hint` (`wotm_prompts.inc:279/287`).
- `wotm_training` (`native/wotm_training.h`): bits 13-19, `Dummy` 23, `g_on` 25, `g_opts` 26, `g_dummy` 27, `g_dummyHealth` 28,
  `Meters`/`g_meters` 30-31, `g_resetStats` 32. Menu F2: `wotm_trainview::draw2d` (`wotm_trainview.inc:371`), chamado em
  `wotm_live.inc:1869`.
