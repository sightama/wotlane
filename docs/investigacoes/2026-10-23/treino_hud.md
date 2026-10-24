# Modo treino: HUD do CPU à direita e números nas barras (investigação, 2026-10-23)

Pedido do dono: no treino, (a) um HUD do CPU no canto direito da faixa 4:3, no estilo do HUD do jogador e espelhado; (b) o número
da vida e da stamina por cima das barras do CPU e do jogador. Só leitura; nada foi editado.

## 1. Como o HUD do jogador sai hoje

- **Não é desenho nativo próprio.** O HUD do P1 é um modelo 3D do `hier` (ids 0x1C20..0x2133). O `Hud::update` copia a matriz da
  câmera para o `_cs` do HUD (`Hud` +0xF4) a cada quadro e anima as barras com `animationSetToPercent`. O nativo coleta esses objetos
  em `Scene::hud` (`native/wotm_native.hpp:843`, marcação em `:4394-4431`, `DrawItem::hudId`) e os desenha por último, sem
  profundidade (`NATIVO.md`, linha "HUD" do item Céu). Não passa pelo VU1/GS emulado: é a geometria do `hier` desenhada pela cena
  nativa.
- **Proporção 4:3:** como o HUD é projetado na frente da câmera e o widescreen é Hor+, ele fica sozinho dentro da faixa 4:3 central.
  A faixa sai de `wotm::gsScreenRect(fbw, fbh)` (`native/wotm_fx.hpp:384`): `w = min(fbw, fbh·aspect)`, `x0 = (fbw - w)/2`. Para
  converter da tela do GS (640 x 224) para o quadro nativo: `X = gr.x0 + gx·gr.w/640`, `Y = gy·rh/224`. É a mesma conta do
  `drawQuads` e do `drawScreenRects` (`wotm_native.hpp:3401`, `:3437`).
- **Posição medida** (print 2560x1440 `docs/screenshots/2026-10-23/2026-10-23_modo_treino_painel.png`, faixa 4:3 = x 320..2240,
  convertida para unidades do GS; erro de ±0,5):

  | peça (P1) | x (de 640) | y (de 224) | px no print |
  |---|---|---|---|
  | moldura preta externa | 13,3 .. 275,3 | 8,4 .. 90,8 | 360..1146, 54..584 |
  | retrato (imagem) | 17,0 .. 61,7 | 10,0 .. 30,6 | 371..505, 64..197 |
  | vida (miolo colorido) | 65,3 .. 272,0 | 15,1 .. 20,5 | 516..1136, 97..132 |
  | quadrado do especial | 69,7 .. 83,0 | 24,7 .. 31,1 | 529..569, 159..200 |
  | stamina vertical (miolo) | 18,0 .. 29,0 | 32,7 .. 88,2 | 374..407, 210..567 |

  Espelhado para o CPU: `x' = 640 - x` (moldura 364,7..626,7; retrato 578,3..623,0; vida 368,0..574,7; especial 557,0..570,3;
  stamina 611,0..622,0; os y não mudam). O próprio jogo desenha o HUD do jogador 2 assim, espelhado (retrato à direita, vida para a
  esquerda, stamina embaixo do retrato): veja `docs/screenshots/2026-10-19/2026-10-19_tela_dividida_2p.png`. Vale refinar com um
  print em `WOTM_ASPECT=4:3`.

## 2. Como o jogo desenha o HUD (`include/game/hud.h`, `src/game/Hud.cpp`, asm `game/Hud`)

- `TheGame::m_huds[4]` (stride 0x2E0, no começo do `game`). `Hud::parseHudElements` reparte os nós do banco: `0x1F47` (modelo do
  HUD, +0xE4) e `0x1F4D` (switch do retrato, +0xEC) vão para `m_huds[id2 != 0]`, ou seja, **existe um HUD 1 (o do P2) no banco**.
  `0x1F55` (+0xF0) é a barra de chefe da aventura, só no HUD 0.
- `Hud::initAfter(i)` (i < 2): `_cs` do modelo em +0xF4 (drawMe 1), barra de chefe em +0xFC (só com `m_gameMode == 1`), handles
  de animação com o índice do HUD: 0x1F4B (+0x130), 0x1F4C (+0x140), 0x1F48 (+0x150), 0x1F49 (+0x160/+0x170), 0x1F4E (+0x180..,
  flash da fúria), 0x1F4F (+0x1B0.., energia baixa), 0x1F56/0x1F57.
- `Hud::update` (rascunho do m2c lido nesta investigação), com o monstro `game + m_viewSlot[view]·0x11190 + 0xB80`:
  - retrato: `node1F4D +0xC/+0x10 = monstro +0x6860/+0x6864`; `hierSetSwitch(node1F4D, m_boostFull (+0xF4))` troca a cara normal
    pela da fúria;
  - vida: `+0xB0 = smoothEasyIn(vida/vidaMax, 0,2)` (+0x44C/+0x448) → `animationSetToPercent(+0x130)`; mensagem 0xC abaixo de 0,15;
    o trecho do dano recente fica em `+0xB8` (segura `+0xBC` quadros) → +0x140;
  - stamina: `+0xC0 = min(cur, max)/max` suavizado (StaminaMeter +0x460: max +0, maxLevel +8, cur +0x10, esgotada +0x20) → +0x150;
    o monstro +0xE8 diferente de 0 força 1,0; abaixo de 0,3 pisca (+0xD0 e handles 0x1F4F, `playLowStaminaSound` se +0x6CD4 == 1);
  - fúria: `+0xC8 = max(0, cur - max)/(maxLevel - max)` → +0x160 (rosa);
  - especial: `isSpecialAvailable` liga a animação +0x180/+0x190.
  - Câmera do HUD: com `gUseUnifiedView`, a câmera unificada (`Cameras::m_cameras + 0x2A50`); senão, com `inputGetInputMode() != 0`
    (tela dividida), a câmera do próprio HUD; **senão, a câmera 0 (`m_cameras + 0xCF0`) para qualquer índice**.
  - drawMe: fora da tela dividida o `update` do HUD 0 liga o `_cs` do HUD 0 e, com `m_numMonsters` (+0x1203D8) >= 2, também o do
    HUD 1 (`game +0x3D4`).
- `rtMain` (asm `rtMain__Fb`, 0x1886B0..0x1886FC): com `gUseUnifiedView` e `m_numMonsters == 2` chama `Hud::update` e
  `addHealthIndicators` dos HUDs v e v+1 (os dois HUDs na tela cheia, P1 à esquerda e P2 à direita); senão, só o da vista.
- Texturas: o retrato é a última entrada do `.RTX` do monstro (T8H 128x64; cara normal na metade esquerda e da fúria na direita;
  `ASSBOSS1.RTX` texId 6 no Goliath). As barras do modelo são geometria do `hier` animada, não sprites com textura própria.
- `Hud::drawBossHealthBar(f)` só anima a barra de chefe (nó 0x1F55, +0xFC), sem retrato e sem stamina, e só aparece com
  `m_gameMode == 1`. Não serve para isso.

**Pedir ao jogo um segundo HUD (opção B):** é possível em tese. Num gancho na saída do `Hud::update(0)` em 1 jogador (o
`wotm_split4.inc` já envolve `update__3Hud`, `hudUpdate` em `:608`, e na luta normal só passa adiante), chamar o original com
`a0 = game + 0x2E0` e `m_viewSlot[1]` (`game + 0x1203EC`) trocado pelo slot do CPU durante a chamada. Fora da tela dividida o HUD 1
iria para a câmera 0 já espelhado, com retrato, fúria, flash e o retrato de mod. Antes, conferir: (1) se `m_huds[1].m_node1F47`
(`game + 0x2E0 + 0xE4`) e o `_cs` (+0xF4) existem numa luta de 1 jogador (o banco de HUD pode trazer só o HUD 0); (2) o que o
`_cs` do HUD 1 desenha hoje (o drawMe já fica 1 com 2 monstros e a matriz nunca é atualizada); (3) efeitos colaterais do `update(1)`:
mensagens (`addMessage 0xC`, `updateMessage`), texto "Wins" à direita (`fontSpritePrintRightXY` x 0x258) nos modos 2..6, som de
energia baixa e mensagens de fase com `m_gameMode == 1`. Não chamar o `addHealthIndicators` do HUD 1. É um teste de ~15 min com um
log desses ponteiros. O dono pediu desenho nativo, então fica como alternativa.

## 3. O que já existe desenhado pelo nativo "como o mod"

- **Retrato de monstro custom/mod:** não é desenhado à parte. O `"portrait"` do `monster.json` (`native/wotm_mods.hpp:74-75,
  438-450`) vira troca de textura do `.RTX` (`native/wotm_texmods.hpp:107-115`, chave `<PREFIXO><traje>` + texId). Tanto o HUD do
  jogo quanto as texturas residentes do nativo (`Scene::residentTexture` → `modTexture`, `wotm_native.hpp:3484-3501`) a recebem.
- **Bloco de barras dos inimigos (`WOTM_ENEMYBARS`, "QoL features"): é o código a reaproveitar.** Em
  `native/runtime/wotm_overrides.cpp`, namespace anônimo, `:1756-1979`:
  - `barFrac` (`:1777`), perfis medidos do HUD do jogador `kHealthProf`, `kLagProf`, `kEnergyProf`, `kOverProf`, `kFuryDimProf`,
    `kExhaustedProf`, `kSpecialProf` e o piscar da fúria `kFuryBlinkPeriod/On` (`:1788-1802`, `:1917`);
  - `gradBar` (`:1805`, faixas no sentido do y), `barRing` (`:1816`, moldura), `barLag` (`:1832`, dano recente por monstro);
  - `monsterPortrait(rdram, m)` (`:1869`): acha o TEX0 do retrato no NGP do monstro, com cache por arquivo carregado;
  - `emitBars(rdram, m, box)` (`:1923`): retrato (cara da fúria com `m_boostFull`), vida com dano recente, especial, energia
    com fúria e esgotada, em unidades da tela do GS.
  - Saída: `wotmBarRect` e `wotmBarTex` (`native/runtime/wotm_live.inc:907`, `:933`) empilham `ScreenRect` com `barView` em
    `g_pendingBars` (só na thread do EE). O `wotmCaptureFrame` passa a fila ao instantâneo (`wotm_live.inc:1085-1086`) e o
    `Scene::drawScreenRects` a desenha na faixa 4:3 (`wotm_native.hpp:3444-3469`). O modo foto com "sem HUD" tira os retângulos de
    barra (`wotm_photo.inc:603-611`); com `anchor = -1` eles ficam parados na tela, como um HUD.
- **Números com a fonte do jogo:** `drawDamagePopups` (`wotm_live.inc:1222-1287`) monta `TextQuad` com os glifos da fonte do
  "WINS" (`DamageFont`, capturada na thread do EE de `FontText::kFonts + 0x38·g_lastFontIdx` e `g_font.tex0`, `:1200-1221`) e chama
  `scene.drawQuads` em unidades 640 x 224. Dá para copiar para os números das barras.

## 4. Barras do P1 e escala dos valores

- Barras na tela: tabela da seção 1. Centro do miolo da vida do P1 = (168,7; 17,8); da stamina = (23,5; 60,5); no CPU = (471,3;
  17,8) e (616,5; 60,5).
- A vida é float cru (`+0x44C` atual, `+0x448` máximo; no print, 175 no Togera e 180 no Congar). A barra do jogo mostra a fração
  suavizada, então o número deve ler o valor cru (o painel já o lê em `wotm_trainview.inc:176-179`).
- Stamina: `cur` +0x470 (StaminaMeter +0x10), `max` +0x460 e `maxLevel` +0x468 (o teto da fúria). A barra azul é `min(cur, max)/max`
  e o rosa por cima é `(cur - max)/(maxLevel - max)`. Esgotada: +0x480. O `capture` do treino ainda não guarda `maxLevel` nem o
  estado de esgotada (`wotm_trainview.inc:170-179`).
- No print com "ALWAYS RAGE" o rosa ocupa a parte de baixo da barra vertical. Pelo jeito a barra enche de baixo para cima;
  conferir com a stamina abaixo do máximo.

## 5. Recomendação (opção A, nativa)

1. **Layout num lugar só:** constantes da seção 1 (moldura, retrato, vida, especial e stamina do P1, em unidades 640 x 224, com
   `mirror(x) = 640 - x`) num `namespace` de `native/wotm_training.h` (já incluído pelo `wotm_training.inc` e pelo
   `wotm_trainview.inc`).
2. **HUD do CPU, na thread do EE**, em `wotm_overrides.cpp` logo depois do `emitBars` (`:1979`), para usar os auxiliares do
   namespace anônimo: `emitTrainHud(rdram, m)`, com o layout espelhado:
   - moldura preta/cinza/preta com `barRing` em volta de cada célula e do bloco (como o `emitBars`);
   - retrato com `monsterPortrait` + `wotmBarTex(..., fury ? 64 : 0, 64, fury ? 128 : 64, 0)` (o de mod vem sozinho);
   - vida horizontal, enchendo **da direita para a esquerda** (espelhada): `gradBar` com `kHealthProf`/`kLagProf` e `barLag`;
   - stamina **vertical**: falta um `gradBarV` (as mesmas faixas no sentido do x), enchendo de baixo para cima, com
     `kEnergyProf`, `kOverProf`, o piscar da fúria e `kExhaustedProf`;
   - especial: `kSpecialProf` quando `isSpecialAvailable` (a mesma conta do `emitBars:1931`);
   - opcional, para parecer o do jogo: suavizar a fração como o `Hud::update` (`smoothEasyIn` 0,2 por quadro), guardando a
     fração por monstro como o `BarLag`.
   - Entrada pública `void wotmTrainHud(uint8_t *rdram)` (declarada no `wotm_training.h`), que sai sem fazer nada sem o treino
     ligado (`wotm_training::g_on`), fora da luta de 1 jogador (`wotm_split4::splitPlayers`, `m_numPlayers`), com `gHudEnable`
     (`wotm::sym::gHudEnable`) zerado ou com letterbox ativo (`_3Hud$m_letterbox +0x2C`, a mesma condição do `Hud::update`). O CPU
     é o primeiro slot >= jogadores com vida máxima > 0 (o laço de `wotm_trainview.inc:171-179`, `kSlots` 0x120380).
   - Chamar no `wotmCaptureFrame` **antes** do `rects.swap(wotm_live::g_pendingBars)` (`wotm_live.inc:1085`).
   - **Atenção:** `g_barView` (`wotm_live.inc:903`) só é posto pelo `wotmHealthIconShift`. Antes de emitir, ponha 0 (um setter ou
     um parâmetro de vista no `wotmBarRect/Tex`) e chame `wotmHudAnchorEnd()` para `anchor = -1`.
3. **Números (P1 e CPU), na thread da janela**, em `wotm_trainview::drawPanel` (`wotm_trainview.inc:513`, chamado em
   `wotm_live.inc:1764` depois do `drawScreenRects`, então sai por cima das barras):
   - `X = gr.x0 + gx·gr.w/640`, `Y = gy·rh/224` com `gsScreenRect(rw, rh)`;
   - vida "175 / 175" (ou só o atual) centrada no miolo, altura ~80% de 5,4 linhas do GS, com contorno preto (4 desvios) para
     ler sobre o verde;
   - stamina: a barra tem só 11 de 640 de largura (~33 px a 1440p). Duas saídas: texto girado -90° (`DrawTextPro`) ao longo da
     barra, centrado em (23,5; 60,5) e no espelho, ou um número curto no topo dela. Na fúria, mostrar `cur/max` (passa de 100%);
   - o `capture` (`:163`) passa a guardar `maxLevel` e o slot do CPU; as linhas "P1 health ... / CPU1 health ..." do painel
     (`:605-613`) podem sair;
   - para usar a fonte do jogo, copiar o `drawDamagePopups`: capturar a `DamageFont` no `wotm_trainview::capture` (mesmo TU do
     `wotm_live.inc`) e montar `TextQuad` para `scene.drawQuads`. O `drawPanel` hoje não recebe a `Scene`; seria preciso passá-la.
4. **Colisões de tela:** o painel de dano fica no topo à direita da faixa 4:3, em `top = rh/6` (`wotm_trainview.inc:617-619`),
   bem em cima do HUD do CPU (que vai até y 90,8 de 224, ~40% da altura). Desça o painel para `top ≈ 0,42·rh` ou ponha-o à
   esquerda. O histórico de comandos (`drawInputs`, `:478-479`, `top = rh/6`, x na borda esquerda da faixa) cobre a stamina do P1;
   desça-o também.
5. **Conferir:** `run_live` com `WOTM_TRAINING=1` em 16:9 e em `WOTM_ASPECT=4:3`; comparar o bloco do CPU com o HUD do P2 da vista
   unificada de 2 jogadores (`WOTM_MATCH=... WOTM_PLAYERS=2`, monstros perto), que é o espelho real. Corpus sem mudança (o treino
   fica desligado no replay).
