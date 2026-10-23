# Painel "Recompiled options" no menu de pausa da luta (2026-10-23)

> **Implementado no mesmo dia** (branch `painel-pausa`; resumo em `NATIVO.md`, "Painel na pausa"). Diferenças do plano: a caixa 3D da
> pausa é desligada com `hierSetCsDrawMe(shell->m_cs610, 0)` enquanto o painel está aberto (em vez de mexer no `gDrawOverlay`); o
> banco de sons do Shell **não** está carregado na luta (conferido: `gSoundBanks[14]` é 0), então o som nativo carrega uma cópia
> do `SHELL_S.BNK` só para o painel, no grupo 2 (o jogo silencia os grupos 0 e 1 na pausa); a caixa da pausa ficou 1,4x mais larga para
> o RECOMPILED OPTIONS caber; o painel aceita todos os dispositivos, como no menu.

Pedido do dono: mexer nas opções do port (vídeo, features, câmera, controles) no meio da luta, sem voltar ao menu principal. Só leitura
do código; nada foi implementado nem compilado. Fontes: `native/runtime/wotm_mainmenu.inc` (painel), `wotm_pausemenu.inc` (ganchos da
pausa), `wotm_legend.inc` e `wotm_prompts.inc` (legenda e dica), `wotm_live.inc` (integração na janela, `applyVideoSettings`),
`wotm_photo.inc`, `wotm_pad.inc`, `wotm_padsel.inc` (roteiro, `callGuest`, `sfx`), `wotm_save.inc`, `native/wotm_fx.hpp`
(`gsScreenRect`), `src/game/Sound.cpp`, `src/common/input.cpp` e o asm de `ShellFinished` (`displayDialog`, `pauseLevelOverlay`,
`checkInputForExit`, `getCurrentMenuSelection`) com o rodata dele.

## 0. Resumo

- **Entrada pela própria pausa:** a linha 3 (hoje escondida, o antigo CONTROL SCHEME) volta com o texto **RECOMPILED OPTIONS**. O X nela
  (ou o clique) abre o painel. O texto cabe no buffer que o jogo já reescreve a cada quadro.
- **Com o painel aberto, o `pauseLevelOverlay` não roda**: o gancho dele chama o `panelInput` (thread do EE, uma vez por quadro do laço do
  `displayDialog`) e devolve 0 ("continua na pausa") sem chamar a original. Assim nenhuma leitura de entrada do jogo acontece (cursor,
  vibração, esquema, X, Start), e a lista da pausa não é desenhada.
- **Desenho em 2D** na thread da janela: o mesmo `renderPanel` (textura `g_prt`), colado reto no centro do quadro depois do texto do jogo,
  com um véu escuro; o hover sai do ponteiro em pixels do quadro.
- **Sons pelo banco da fase** (`playPauseMenuSound`, ids 0x50 e 0x4F), não pelo do Shell.
- **Travar na pausa só o KEYBOARD (teclado separado)**; o resto pode mudar ao vivo (detalhes na seção 6).
- Estimativa: ~350 a 450 linhas, quase tudo em `wotm_pausemenu.inc` e `wotm_mainmenu.inc`; uma sessão para escrever e outra para conferir
  com o dono (seção 9).

## Decisões do dono (2026-10-23)

- **Sons:** os mesmos do painel no menu principal (`wotm_padsel::sfx`). A seção 1 ("Sons") diz que o banco do Shell provavelmente não está
  carregado na fase: conferir na etapa 2. Se não tocar, avisar o dono antes de trocar pelo banco da pausa (`playPauseMenuSound`).
- **Tela dividida:** a pausa mostra só a vista de quem pausou, e tudo bem. Quem controla o painel não importa: qualquer dispositivo,
  como no menu.
- **Fundo:** a luta continua desenhada atrás do painel, justamente para comparar as mudanças ao vivo (resolução, AA, filtro). Some só a
  lista da pausa. Véu escuro leve ou nenhum, para não atrapalhar a comparação (conferir com o dono).
- **Teclado separado (KEYBOARD, primeira linha da aba CONTROLS): travado na pausa** (decidido). Ligar essa opção no meio de uma luta
  de 1 jogador manda o teclado para o jogador 2, que não existe; quem joga pelo teclado perde o monstro e não sai mais da pausa pelo
  teclado. Na pausa a linha aparece apagada, sem setas, e não muda; a dica diz "CHANGE IT IN THE MAIN MENU". O resto da aba fica livre.

## 1. O que o jogo faz na pausa (retail)

### `displayDialog__Fi` (`wotm::fn::displayDialog__Fi`)
- Roda o próprio laço de quadros até o overlay mandar sair. Ao entrar na pausa (argumento 0) zera `gHudEnable` (`wotm::sym::gHudEnable`) e
  `gCurrentMenuSelection`, pausa os sons do jogo (`SoundManager::initPause`) e escurece a cena (`Shell::FadeScreen`).
- **Tela dividida:** pela leitura do asm, se a pausa começa com `gUseUnifiedView` (`wotm::sym::gUseUnifiedView`) desligado (fora dos
  modos 8 e 9), o jogo copia a câmera da vista de quem pausou (`gPlayerThatPaused`, `wotm::sym::gPlayerThatPaused`) para a vista unificada
  e liga `gUseUnifiedView`. Ou seja, **a pausa é sempre uma vista só, em tela cheia**, mesmo com 2 a 4 jogadores. Na saída o flag volta a 0.
  Conferir na tela (seção 8): a camada nativa já segue o `gUseUnifiedView` para decidir a divisão (`wotm_live.inc`, na contagem das vistas).
- Por quadro: `inputUpdate`, `inputAnyKey` (música), `Hud::update`, `hier`, `particleDraw`, `Hud::print`; aviso "CONTROLLER UNPLUGGED"
  quando o controle de `gPlayerThatPaused` some; depois despacha por estado para o overlay (`pauseLevelOverlay`, `areYouSure`, etc.).
- A captura do quadro para a janela também acontece nesse laço (o `wotmCaptureFrame` é chamado no `sceGsSyncV` do fim dele,
  comentário da `struct Capture` no `wotm_live.inc`): a janela continua recebendo quadros durante a pausa.

### `pauseLevelOverlay__FRi` (`wotm::fn::pauseLevelOverlay__FRi`), um quadro
1. Copia a tabela de 5 ponteiros de texto (rodata `D_006EFC48`) para a pilha. **A pilha, na fase, está no scratchpad (0x70000000), fora do
   rdram** (ver o comentário do `text` no `wotm_save.inc`): o gancho não deve ler nada pelo `$sp`.
2. Monta os textos variáveis com `sprintf`: o item 2 ("VIBRATION ON/OFF") e o item 3, num buffer de 24 bytes do rodata (`D_006EFC30`, que
   começa como "CONTROL SCHEME"), onde escreve "CONTROL TYPE A/B/C" conforme o `m_ctlrCfg` de quem pausou.
3. Para a vibração dos controles, chama `getCurrentMenuSelection(5, gPlayerThatPaused)` (cima/baixo, som 0x50) e, com `gDrawOverlay`,
   desenha os 5 itens (`setMenuFont(i)` + `fontSpritePrintCenteredXY(0, 320, y, texto)`, y = 0x55, 0x5E, 0x67, 0x70, 0x79).
4. Posiciona um objeto 3D na frente da câmera de quem pausou (o nó do ponto de fim 770, `Shell::m_ep770Word20`, `shell+0x618`;
   provavelmente o fundo da pausa) e o liga ou desliga com `hierSetCsDrawMe(cs, gDrawOverlay)`.
5. Item 2: esquerda/direita (`inputGetInput(0x80 / 0x20, jogador)`) trocam a vibração. Item 3: esquerda/direita trocam o
   `m_ctlrCfg` (`shell+0x2B8C + 4 * jogador`, `include/game/shell.h`) e o campo `Monster+0x672C` do monstro do jogador (tipo A = 1 e 0;
   com o jogador 3 ou 4 a escrita cai fora do array, no `m_vibration`).
6. `checkInputForExit(jogador, &start)`: verdadeiro com X (0x4000); com o segundo argumento, também com Start (0x08), e então grava 1 no
   bool da pilha. Só vale com `gDrawOverlay` e depois dos primeiros 31 quadros (`g_frame`). Start força o item 0. Item 0 (CONTINUE):
   devolve o HUD e sai (som 0x4F). Itens 1 (RESTART) e 4 (QUIT): vão ao "ARE YOU SURE?" (estado 0x10, som 0x4F). **Itens 2 e 3: o X não
   faz nada.**
- `gDrawOverlay` (`wotm::sym::gDrawOverlay`) começa em 1 e ninguém o escreve no retail (resto de depuração); só os overlays o leem.

### Sons
- `ShellSound::playShellSFXSound` toca do banco `BANK_SHELL` (`Shell_s.bnk`), carregado pelo `SoundManager::loadShellSoundBanks` só nos
  menus; o `loadRTSoundBanks` da fase carrega ambiente, monstros e fase. O id 1 ainda para a música do shell.
- `ShellSound::playPauseMenuSound` toca do `BANK_AMBIENT` (`GenAmb_s.bnk` / `GenAm_ls.bnk`), que existe na luta: é o que a pausa usa
  (0x50 mexer, 0x4F confirmar).
- **Conclusão:** dentro da fase, o `wotm_padsel::sfx` (Shell) não é confiável: o banco provavelmente não está carregado (ou tocaria outro
  som). O painel precisa de uma indireção: no menu, `playShellSFXSound` com 2/4/6/8, como hoje; na pausa, `playPauseMenuSound` (via
  `wotm_padsel::callGuest` com `wotm_padsel::kShellSound`, como o `selHook` já faz) com 0x50 para 2/4 (cursor, valor) e 0x4F para 8 (ok,
  abrir). Para o 6 (voltar), 0x50 ou 0x4F: escolha do dono. A conferir: o modal de gravar na fase (`wotm_save.inc::sfx`) chama o
  `playShellSFXSound`; se aquilo soa certo, o banco do shell ainda está na RAM de som depois da vitória, mas não conte com isso na pausa.

## 2. O que o port já faz na pausa (`wotm_pausemenu.inc`)
- `overlayHook` (entrada/saída do `pauseLevelOverlay`, `g_inOverlay`, `wotm::mouse::gameMenu()`), `printHook` (esconde a linha que começa
  com "CONTROL" e sobe o QUIT de 0x79 para 0x70), `selHook` (cursor pula o item 3; mouse por `hoverItem` e `wotm::mouse::gsPointer`;
  clique vira X por `g_crossReq = 2`), `dialogHook` (`g_inDialog`), `menuFontHook`/`titleFontHook`/`defSizeHook` (itens dos diálogos).
- O tipo A fixo do `m_ctlrCfg` é feito no `frame()` do `wotm_mainmenu.inc`, que só roda nas telas do menu (ganchos de saída de
  `screenMain`, `screenOptions`, `screenGameModes1P/2P`, `screenTransition`). Na luta ninguém o força: o que protege é a linha não existir.

## 3. O painel hoje (`wotm_mainmenu.inc`) e o que é preso ao menu
- Estado na thread do EE: `g_open`, `g_tab`, `g_optSel[]`, `g_top[]`, `g_bindMap`, `g_capture`, `g_showPreview`, `g_panelPrev[]`,
  `g_panel` (animação 0..1, +-0,1 por quadro), `g_suppress` (20 quadros sem entrada depois de fechar).
- `openPanel()`, `panelInput()` (lê os dispositivos direto por `readPanel`, inclusive o roteiro `WOTM_PADSEL_SCRIPT`; mouse pelo
  `g_hover`), `captureInput()` e `controlsInput()` (aba CONTROLS). Chamado do `getInputHook` do `screenGetInput` na tela 1.
- Presos ao menu:
  1. `panelInput` fecha escrevendo `g_open = false; g_suppress = 20` (globais do menu).
  2. Todos os sons são `wotm_padsel::sfx` (Shell).
  3. A janela só desenha o painel dentro do `renderUi`, que sai cedo sem `g_view.on`/`rows`; o `g_view` só é publicado pelo `frame()` (menu).
  4. O hover só existe no `drawPanelOnTelao` (plano 3D do telão, `hitOnPlane`); o `drawOnTelao` grava `g_hover` **a cada quadro sem tela
     dividida, também na luta** (vazio).
  5. `previewParams` supõe o painel em ~55% da altura da janela (`boxScreenRows`).
  6. A dica e a legenda (`wotm_legend::hint/post`) são postadas dentro do `renderPanel`; a legenda tem faixa preta opaca no rodapé
     (`kBarTop` 0,893), feita para o menu; o `post` já aceita `barAlpha` (o modo foto usa translúcida).
  7. `renderUi` usa o atlas da fonte do texto do quadro anterior (`S.scene.text.front().tex0`); sem texto no quadro, cai na Bahnschrift.

## 4. Plano de implementação por etapas

### Etapa 1: a linha volta como RECOMPILED OPTIONS (só `wotm_pausemenu.inc`)
- `printHook`: no texto que começa com "CONTROL", em vez de pular, escrever "RECOMPILED OPTIONS" (18 letras + NUL = 19 bytes) **no próprio
  endereço que veio em `$a3`** (é o buffer de 24 bytes do rodata; o jogo o reescreve a cada quadro antes de imprimir) e deixar a original
  desenhar. Assim não é preciso endereço cru do `D_006EFC30` (que não está no `wotm_syms.h`). Tirar a troca do y do QUIT.
- `selHook`: tirar o pulo do item 3. `hoverItem`: as 5 linhas nas alturas originais e a meia largura de 100 para ~130 (a linha nova tem 18
  letras; "CONTROL TYPE A" tinha 14).
- **Bloquear esquerda/direita no item 3**: novo gancho em `inputGetInput__Fii` (`wotm::fn::inputGetInput__Fii`) que, com `g_inOverlay`,
  `gCurrentMenuSelection == 3` e máscara 0x80 ou 0x20, devolve 0 sem chamar a original (`ctx->r[2] = 0; ctx->pc = GPR_U32(ctx, 31)`).
  Filtrar pela máscara é obrigatório: o `checkInputForExit` chama o mesmo `inputGetInput` com 0x4000 e 0x08. Sem isso o jogo troca o
  `m_ctlrCfg`/`Monster+0x672C` e toca 0x50.
- Atualizar o comentário do topo do arquivo e a linha do log do `install`.

### Etapa 2: abrir o painel pelo X no item 3
- Gancho em `checkInputForExit__FiPb` (`wotm::fn::checkInputForExit__FiPb`), só com `g_inOverlay`. O Start também faz ele devolver 1, e
  o bool que diria "foi Start" está na pilha (scratchpad). Em vez de ler a pilha: o gancho do `inputGetInput` da etapa 1 anota, enquanto
  estamos dentro do `checkInputForExit` (flag de entrada/saída como o `g_inOverlay`), se a leitura 0x4000 deu verdade (`g_sawCross`) e se
  a 0x08 deu (`g_sawStart`).
- Na saída do `checkInputForExit`: se `gCurrentMenuSelection == 3`, `$v0 != 0`, `g_sawCross` e não `g_sawStart`, abrir o painel da pausa
  (`openPausePanel`, seção 4.3) e devolver 0. Com Start, deixar como está (o jogo continua a luta, como no retail).
- O clique do mouse no item 3 já vira X (`g_crossReq = 2`): passa pelo mesmo caminho.

### Etapa 3: estado próprio da pausa no `wotm_mainmenu.inc`
- Separar o "onde" do painel: `enum Where { InMenu, InPause }` e `g_where`. Novos `g_pauseOpen`, `g_pausePanel` (animação),
  `g_pauseSuppress`. `g_tab`, `g_optSel`, `g_top`, `g_bindMap`, `g_capture`, `g_showPreview` continuam compartilhados (o painel lembra a
  última aba; menu e pausa nunca estão abertos juntos).
- `panelInput` deixa de escrever `g_open`/`g_suppress`: devolve "fechou" (ou recebe ponteiros), e cada chamador fecha o seu. No menu
  continua igual; na pausa: `g_pauseOpen = false`, `g_pauseSuppress = 20`.
- `panelSfx(rdram, ctx, rt, id)` no lugar dos ~15 `wotm_padsel::sfx` do painel (`panelInput`, `captureInput`, `controlsInput`): com
  `g_where == InPause`, `playPauseMenuSound` com o mapa da seção 1; senão o de hoje.
- `openPausePanel()`: `g_where = InPause`, `g_pauseOpen = true`, `g_showPreview = true`, `readPanel(g_panelPrev)` (o X que abriu não
  conta), zerar o clique pendente (`wotm::mouse::takeClick(g_clickL)`, `g_crossReq = 0`), log `[pausa] painel aberto (jogador N)`.
  Sugestão: com `g_bindMap` no mapa do gamepad de quem pausou (`gPlayerThatPaused`), se ele estiver em gamepad.

### Etapa 4: o painel no lugar do overlay (thread do EE)
No `overlayHook`, na entrada do `pauseLevelOverlay`:
- **Painel aberto:** `wotm::mouse::gameMenu()`, `wotm_pad::markMenuFrame()` (ver armadilhas), `panelInput(...)`, publicar a View da pausa
  (abaixo) e **pular a original**: `ctx->r[2] = 0` (o `displayDialog` continua na pausa), `ctx->pc = GPR_U32(ctx, 31)`. Um quadro do laço
  do `displayDialog` = uma chamada do `panelInput`, exatamente como o `screenGetInput` no menu.
  - Por que pular a original em vez de bloquear função por função: não há leitura nenhuma do pad pelo jogo (cursor, vibração, esquema,
    X, Start), o cursor fica no item 3 e a lista some, num gancho só. O que se perde por quadro é o `sprintf` dos textos, o parar da
    vibração (já parada nos quadros anteriores) e o reposicionamento do objeto 3D do fundo, que fica onde estava (a câmera não anda
    na pausa) e continua desenhado (o `DrawMe` do último quadro). A conferir na tela se esse fundo atrás do painel fica bom; se não, ligar
    `gDrawOverlay = 0` enquanto aberto e voltar a 1 ao fechar (e na saída do `displayDialog`).
  - O `gExitDialog` que o overlay grava na entrada e zera no fim fica no valor do último quadro (zero): sem efeito.
- **Fechado há pouco (`g_pauseSuppress > 0`):** rodar a original, mas com a entrada do jogo bloqueada: no `selHook`, devolver o
  `gCurrentMenuSelection` atual sem chamar a original; no `checkInputForExit`, devolver 0 sem chamar; no `inputGetInput` (dentro do
  overlay), devolver 0. Decrementar por quadro **e** só zerar quando nenhuma ação do painel estiver apertada (`readPanel`): o Esc e o
  Enter são Start no mapa padrão do teclado (`kDefaultKb`), e o X/triângulo do gamepad também confirmam na pausa.
- Publicar a View da pausa (`g_pauseView`, sob `g_m`): `on = g_pausePanel > 0`, `alpha = 1`, `panel = g_pausePanel`, `tab`, `opt`, `top`,
  `bindMap`, `capture`, `preview`, `screen = -1`. Animar `g_pausePanel` +-0,1 por quadro como o `g_panel`. Ao fechar, publicar até chegar a 0.
- `dialogHook`, na saída do `displayDialog`: se o painel ainda estiver aberto (não deve acontecer, ver seção 7), fechar, zerar a View, voltar
  `gDrawOverlay` a 1 se a variante da etapa 4 o mexeu, log de aviso.

### Etapa 5: desenho em 2D na janela
- `renderPauseUi(scene, ram, tex0)`: lê `g_pauseView`; com `panel > 0`, chama o `renderPanel` (mesma textura `g_prt`). Chamar no
  `wotm_live.inc` junto do `wotm_mainmenu::renderUi` (antes do quadro, linha ~1639), para a dica e a legenda serem postadas antes do
  `wotm_legend::draw`.
- **Atlas da fonte:** passar o último atlas visto (o `wotm_photo::fontTex0` já guarda o do último quadro com texto); com o overlay
  pulado o quadro da pausa pode não ter texto nenhum e o painel trocaria de fonte.
- `drawPausePanel2d(rw, rh)`: dentro do `BeginTextureMode` do quadro, **depois** do `S.scene.drawQuads(S.scene.text, ...)` (linha ~1722)
  e **antes** do `wotm_legend::draw` (linha ~1748): véu preto (~35%, o jogo já escureceu pela `FadeScreen`) e `DrawTexturePro` do `g_prt`
  (lembrar do y invertido da RenderTexture) com alfa `panel`.
- Hover: separar do `drawPanelOnTelao` a conta "coordenadas u, w no painel → parte" (`panelHit(u, w, v, hv)`) e usá-la nos dois; no 2D,
  `u = (p.x - left) / largura`, `w = (p.y - top) / altura`, com o ponteiro `wotm::mouse::g_ptr` (pixels do quadro nativo, do
  `pointerFrame`) e `wotm::mouse::claim()` a cada quadro com o painel na tela. **Gravar num `g_pauseHover` à parte**, lido pelo
  `panelInput` quando `g_where == InPause`: o `drawOnTelao` grava `g_hover` vazio a cada quadro da luta, e a thread do EE poderia ler
  entre as duas escritas (hover piscando, clique perdido).
- `previewParams`: trocar o 0,55 fixo por um parâmetro (a fração da altura da janela que o painel ocupa: 0,55 no telão, a da pausa no 2D).
- Legenda: na pausa, `post(..., la, barAlpha)` com a faixa translúcida (0,5 a 0,6, como o modo foto) para não cortar a luta com uma faixa
  opaca; o `renderPanel` ganha o parâmetro. A dica (`hint`) pode ficar como está (caixa própria em 0,815 da altura).

### Etapa 6: travas e documentação
- KEYBOARD travado na pausa (seção 6): mostrar o valor com as setas apagadas e uma dica "CHANGE IT FROM THE MAIN MENU"; ignorar
  esquerda/direita/X nessa linha quando `g_where == InPause`.
- `NATIVO.md` (seção do painel e do menu de pausa) e o comentário do topo dos dois `.inc`. Nenhuma variável de ambiente nova é necessária;
  se entrar uma de log (`WOTM_PAUSELOG`), vai na tabela do `NATIVO.md` (`python tools/wotm_env.py`).

## 5. Posição e tamanho em 2D
- A pausa é sempre tela cheia (vista unificada, seção 1), então a conta é sobre o quadro inteiro (`RW x RH`).
- Textura 1800 x 1100 (~1,64:1). Proposta: **altura = 0,60 x RH**, largura = altura x 1800/1100 ≈ 0,98 x RH, centrado em x = RW/2,
  topo em ~0,09 x RH (base em ~0,69). Limitar a largura a 0,96 x RW por segurança.
  - 16:9: o painel ocupa ~55% da largura, dentro da faixa 4:3 central (`gsScreenRect`), onde fica o texto do jogo.
  - 4:3: ~74% da largura. 21:9 e 32:9: igual ao 16:9 em altura, centrado.
  - Fica livre o espaço da dica (caixa centrada em 0,815, topo ~0,74 com duas linhas) e o da legenda (0,893 para baixo).
- Tamanho das letras parecido com o do telão (que dá ~55% da altura): 54 px de linha na textura viram ~22 px numa janela de 720.
- Tela dividida: não muda nada se a vista unificada da pausa se confirmar. Se não se confirmar em algum modo (8, 9), centrar no quadro
  inteiro mesmo assim: o painel é de quem pausou.
- Quem pausou controla: o retail só lê o `gPlayerThatPaused`. O `readPanel` hoje lê todos os dispositivos. Sugestão: na pausa, só os
  dispositivos do jogador que pausou (a mesma regra do `wotm_pad::deviceMap(porta)`, mas **sem** o `g_portBase` do 3 e 4 jogadores,
  que é um valor transitório da leitura do slot 1; e com o `g_manualDevs` quando `g_manual`) mais o mouse. Com um jogador, dá no mesmo. É
  uma decisão do dono; a primeira versão pode aceitar todos, como no menu.

## 6. Quais opções podem mudar no meio da luta
| aba / opção | onde vale | na pausa |
|---|---|---|
| VIDEO: RESOLUTION, RENDER SCALE, ASPECT RATIO | thread da janela (`applyVideoSettings`, `renderHeight`, `viewAspect`): o desenho refaz as RenderTextures do quadro e das vistas quando o tamanho muda, como no redimensionar da janela, que já funciona na luta | liberar |
| DISPLAY MODE | `setFullscreen`; a exclusiva troca o modo do monitor (um soluço de ~1 s, com o jogo parado: aceitável). O F11 já faz isso na luta | liberar |
| REFRESH RATE, VSYNC | `initPresentRate` | liberar |
| ANTI-ALIASING, TEXTURE FILTER | lidos a cada desenho (`msaa::Target::ensure`, `setTexFilter`) | liberar |
| FEATURES: LOCK-ON TOGGLE | `wotm_overrides.cpp` já trata a opção desligada no meio da luta (solta e volta aos botões do jogo) | liberar |
| ENEMY BARS, DAMAGE NUMBERS | lidos a cada quadro | liberar |
| CAMERA: CAMERA STYLE | `wotm_orbit.inc::enabled()` a cada quadro; a órbita guarda estado (yaw, encolhimento). Ao voltar da pausa a câmera pode pular de uma vez para a do jogo (ou para trás do monstro) | liberar, conferir o salto (seção 8) |
| SENSITIVITY, INVERT Y, AUTO CENTER | lidos a cada uso | liberar |
| CLOSE-UP CAMERA | lido no gancho do `LeaveUnifiedTask`. A pausa liga a vista unificada na tela dividida: conferir que desligar a opção com a pausa aberta não deixa a vista presa ao sair | liberar, conferir |
| CONTROLS: KEYBOARD (teclado separado) | `deviceMap` lê a opção a cada leitura do pad. Numa luta de 1 jogador com gamepad, OWN PLAYER manda o teclado para o jogador 2, que não existe: quem joga no teclado perde o monstro e não consegue nem sair da pausa (o retail lê só o `gPlayerThatPaused`). No multijogador com escolha de lados (`g_manual`) a opção nem vale | **travar** |
| BUTTON ICONS, MENU CONFIRM | ícones; a troca de confirmar só vale com `inMenus()` | liberar |
| mapa de botões (rebind), RESTORE DEFAULTS | `wotm_pad` lê o mapa a cada leitura: vale na hora. O painel sempre tem saída (botão sem tecla vale pelo padrão no `readPanel`); o jogo não: limpar o Start ou o X tira o "sair da pausa" pelo teclado/pad (o mouse ainda confirma CONTINUE) | liberar, igual ao menu |

## 7. Casos de borda
- **O jogo sair da pausa com o painel aberto:** com a original pulada, as únicas saídas do overlay (CONTINUE, RESTART, QUIT, Start) não
  acontecem. O `displayDialog` não sai por conta própria (o "CONTROLLER UNPLUGGED" só escreve o aviso). Fechar a janela
  (`requestStop`) não importa. Mesmo assim, a guarda da etapa 4 na saída do `displayDialog` fecha o painel e limpa a View.
- **F7 (pausa do port) e modo foto (F9, L3+R3):** param a thread do EE, logo o `panelInput`. O modo foto não liga com `inMenus()`
  (`wotm_photo.inc`): chamar `wotm_pad::markMenuFrame()` a cada quadro do painel aberto resolve o F9 de graça. O F7 congelaria o painel na
  tela (a View fica publicada); aceitável, ou ignorar o F7 com o painel aberto.
- **Mouse:** o `overlayHook` já chama `gameMenu()` na pausa; com o painel, `claim()` a cada quadro desenhado para o clique esquerdo não
  virar quadrado. A rodinha chega como cima/baixo pelo `readPanel` (`g_wheelHeld`).
- **`markMenuFrame` na pausa:** também liga a troca X/bola do MENU CONFIRM para o `readPanel` (`sourceBind`) e para a legenda (`bind`), o
  que deixa o painel igual ao do menu. Como a entrada do jogo está bloqueada, não muda nada na luta.
- **Captura de tecla (aba CONTROLS):** o Esc cancela a captura e também é Start: com o painel aberto o jogo não lê nada; ao fechar, a
  supressão da etapa 4 espera soltar.
- **`m_ctlrCfg` com `WOTM_MATCH`:** a luta abre sem passar pelo menu; com 0 ("não escolhido") o jogo põe 1. O bloqueio do item 3 garante
  que fica no tipo A.
- **Entrada do jogo durante a animação de fechar:** a View pode estar em `panel < 1` com o painel já fechado; o hover só conta com
  `panel == 1` (como no telão).

## 8. Como testar
Fluxo do port (`CLAUDE.md`): `tools\corpus.ps1 -Check` antes; `build_runtime.bat` (com o PATH mínimo); `tools\corpus.ps1 -Check -Baseline`
depois; commit sem gate (só `native/` e `docs/`).

Roteiro sem mãos (o dono confere as fotos depois), a partir da luta direta:
- `WOTM_MATCH=3,1,1` (Togera contra Congar em central). `PS2X_PAD` com tempos (formato `<início>:<duração>:<botão>[@jogador]`, como nos
  roteiros já usados): Start para pausar depois da contagem da luta, três vezes baixo até RECOMPILED OPTIONS, X.
- `WOTM_PADSEL_SCRIPT` (relógio da janela, `<s>:<dur>:<dispositivo>:<ação>`, ações `left right ok back up down l1 r1 square`) para o painel:
  `r1` (aba), `down`, `right` (muda um valor; log `[settings] ...`), `back` (fecha). Depois `PS2X_PAD` com X no CONTINUE.
- `WOTM_SHOTS=<s>,...` nos momentos: lista da pausa com a linha nova, painel aberto em cada aba, prévia da VIDEO, painel fechando, a luta
  de volta. Guardar em `docs/screenshots/2026-10-23/` com o nome da convenção.
- Conferir no log: nenhuma linha de troca de esquema, `[pausa] painel aberto/fechado`, os `[settings]`, e que a luta volta sem o
  monstro dar o golpe do botão que fechou (Esc/triângulo/bola).
- Tela dividida: o mesmo com `WOTM_PLAYERS=2` e `WOTM_SPLITLOG=1` (confirma a vista unificada na pausa), pausando pelo jogador 2
  (`start@2`) e mexendo no painel pelo controle 1 (deve ou não responder, conforme a decisão da seção 5).
- Ao vivo com o dono (avisar antes por causa do foco, `feedback-teclas-foco`): RESOLUTION, ANTI-ALIASING e DISPLAY MODE na pausa;
  CAMERA STYLE trocado e a luta retomada (salto da câmera); mouse sobre o painel e sobre a lista da pausa; F9 com o painel aberto.

## 9. Estimativa
- `wotm_pausemenu.inc`: linha de volta, ganchos novos (`inputGetInput`, `checkInputForExit`), overlay pulado, supressão, guarda do
  `displayDialog`: ~120 a 150 linhas.
- `wotm_mainmenu.inc`: `g_where`, fechar sem globais do menu, `panelSfx`, View e hover da pausa, `panelHit` separado, `renderPauseUi` e
  `drawPausePanel2d`, parâmetro do `previewParams`, trava do KEYBOARD: ~180 a 250 linhas (boa parte é mexer em código existente).
- `wotm_live.inc` (duas chamadas), `wotm_legend`/`wotm_prompts` (nada, ou o `barAlpha` no `renderPanel`), `NATIVO.md`: ~20 linhas.
- Total ~350 a 450 linhas: uma sessão para escrever e passar pelo roteiro, e uma rodada com o dono para as decisões em aberto (sons de
  voltar, quem controla na tela dividida, fundo 3D da pausa atrás do painel, trava do KEYBOARD) e para o teste ao vivo.
