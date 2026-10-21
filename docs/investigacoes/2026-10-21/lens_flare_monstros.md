# Lens flare dos monstros (prévia sem luta)

2026-10-21. Objetivo: desenhar o lens flare do monstro (olho do Robo-47 etc.) na prévia do menu (`native/wotm_model.hpp`),
com câmera fixa, só com o disco. Legenda: **[C]** confirmado (código + dado), **[H]** hipótese.
Scripts de apoio (rascunho, fora do repo): varredura de nós op 5 nos `.NGP`, pose do idle com o `tools/pose.py`, leitura das
tabelas no ELF e comparação com `build/dumps_live/ram_energy_robot.bin`.

## 1. Quais monstros têm luz com lens flare

Nó `LIGHT` = opcode 5 (`HierLight`, 64 bytes, `include/hieri_types.h`; o `hierTraverseAsm` chama o `hierLightNode` nesse caso).
Varredura de todos os `disc/MON/*.NGP` (travessia por todos os LODs/SWITCH + força bruta por ponteiros): **[C]** só 8 arquivos têm
luz, e todas têm `flags = 0x0F` (`isOn | hasLensFlare | hasScreenGlare | isDirectional`; sem `isBiDirectional`, sem
`lightAtInfinity`), `id2 = 0`, `position = (0, 0, 0, 1)` e `beamNormal = (0, 0, -1, 0)` no espaço local. A posição e a direção
vêm toda da `ROTATE` pai (a luz é folha dela).

| Arquivo | Nó | intensity | range | rgb | beamWidth | pais (de baixo para cima) |
|---|---|---|---|---|---|---|
| ROBOT0 (Robo-47) | 0xA0ABB0 | 40 | 101 | 1, 0.2093, 0.163 | 0.9738 | ROTATE 0xA0ABF0 → ANIM_XFORM 0xA0AC50 (matrixIdx 10, canais rX/rY/rZ 29/30/31) → SKEL_BONE 0xA0AD50 → ANIM_XFORM 0xA14B80 |
| ROBOT1 | 0xA0A2B0 | 40 | 101 | 1, 0.2093, 0.163 | 0.9738 | ROTATE 0xA0A2F0 → ANIM_XFORM 0xA0AC50 (idem) |
| ROBOT2 | 0xA0A2B0 | 40 | 101 | 0.1, 1, 0.1 (verde) | 0.9738 | idem ROBOT1 |
| ROBOT3 | — | | | | | sem luz |
| MONKEY1 (Congar) | 0xA011B0 e 0xA01250 (dois olhos) | 40 | 101 | 0.23, 0.2093, 1 | 0.9913 | ROTATE 0xA011F0 / 0xA01290 → ANIM_XFORM 0xA012F0 (matrixIdx 14, canais 52/53/54) → SKEL_BONE 0xA013E0 (out 13) |
| MONKEY2 | 0xA011B0 (um olho) | 90 | 151.5 | 1.5, 0.1, 0.1 | 0.9564 | ROTATE 0xA011F0 → TRANSLATE 0xA01250 → ANIM_XFORM 0xA01270 (matrixIdx 14, canais 52/53/54) |
| MONKEY3 | 0xA011B0 e 0xA01250 | 90 | 151.5 | 0.1, 0.1, 1.5 | 0.9564 | ROTATE 0xA011F0 / 0xA01290 → TRANSLATE 0xA012F0 → ANIM_XFORM 0xA01320 |
| JELLY0 | 0xA00080 | 250 | 252.5 | 1, 0.2093, 0.163 | 0.9738 | ROTATE 0xA000C0 → ANIM_XFORM 0xA0B0A0 (matrixIdx 1) → SKEL_BONE 0xA0B180 |
| ASSBOSS1 | 0xA2A2D0 | 300 | 276.6 | 1, 0.15, 0.12 | 0.9738 | ROTATE 0xA2A310 → ANIM_XFORM 0xA2A480 (matrixIdx 2) → SKEL_BONE 0xA2A570 |

ROTATE do Robo-47 (ROBOT0), linhas: `(1 0 0 0) (0 0 1 0) (0 -1 0 0) (-0.1021 16.1817 8.6261 1)`: a luz fica a 16,2 unidades à
frente (+Y) do osso da cabeça e o feixe local -Z vira +Y (frente do monstro).

Pose (convenção do `pose.py`: vetor-linha, `p_mundo = (p, 1) @ M`, Z para cima, monstro olhando para +Y) **[C]**:

| Arquivo | pose de base | idle (anim 90) quadro 0 | feixe no idle q0 |
|---|---|---|---|
| ROBOT0/1/2 | (-0.10, 16.18, 99.06) | (-0.69, 19.51, 93.53) | (0.014, 0.993, -0.119) |
| MONKEY1 | (±3.82, 14.74, 90.81) | (3.55 / -4.09, 29.09, 78.99) | (±0.097, 0.994, 0.046) |
| MONKEY3 | (±3.97, 15.60, 90.56) | (3.71 / -4.24, 29.96, 78.78) | (±0.122, 0.992, 0.040) |
| JELLY0 | (0.01, 58.20, 248.36) | (-11.07, 65.51, 234.90) | (0.054, 0.993, -0.102) |
| ASSBOSS1 | (-0.04, 51.84, 112.03) | (11.63, 60.42, 159.71) | (0.183, 0.962, -0.201) |

No `wotm_model.hpp` basta um `case 5` no `visit`: `pos = (position, 1) * m`, `feixe = (beamNormal, 0) * m` com a matriz corrente
(a mesma que o OBJECT receberia ali).

## 2. hierLightNode e o que entra na lista

C++ equivalente em `src/common/hier.cpp` (`hierLightNode`, NON_MATCHING só por registradores) **[C]**:

- Entra só com `isOn && hasLensFlare`, `lensFlareListIndex < 74`.
- `out = (Eo + position) * matStack[idx]` (3x3) = luz − olho no mundo; `pos = out + viewGetTrans(view)` = posição da luz no mundo.
- `nrm = out / |out|` (olho → luz). Sem `lightAtInfinity`: com `isDirectional`, `dotProd = −nrm · (beamNormal * matStack)` =
  cos entre o feixe e a direção luz → olho (1 = feixe apontado para a câmera); `beamWidthFactor = beamWidth`. Sem direcional:
  `dotProd = 1`, `beamWidth = 0`.
- `word +0x10 = 0` e depois o byte `flags` (as da luz, 0x0F); `rgb = lightRed/Green/Blue`; `ID = head.id2` (0 nos monstros: sem
  debounce); `intensity = intensity`. `range` e `hasScreenGlare` não são lidos por nenhum código do flare **[C para o
  hierLightNode/Render; H para o resto do jogo]**.

`particleLensFlareTestZbuffer`: `mathfTransform3dTo2d` dá `(X, Y, W, Z)`; descarta fora da tela ou `W < 1`; grava `pos = (X, Y, Z)` e
`distance = W` (100 só com `lightAtInfinity`). **[C]** `W` é a profundidade ao longo da frente da câmera em unidades do mundo: a linha
3 da `worldToScreenMat` tem norma 1,00000 no dump (`(-0.0656, -0.9593, -0.2746, 0)`), e `viewScreenMats` tem focos 384 (X) e 180,48
(Y, em linhas de campo de 224), centro 2048.

`particleLensFlareRender` (constantes no asm: 1/256, 1/16, 1/4, 0,5, 1, 1,03125, 1,5, 2): para luz local (o caso dos monstros), as 4
amostras centrais do bloco 4x4 de Z em torno de (X, Y); visível se `Z ≤ fptoui(a + b / (W − 1,5))` (o `Zfudge` lido como float é o
byte 0x0F, denormal, então vale o mínimo 1,5) **[C, resumo do wotm_fx.hpp conferido pela checagem VU1]**. `intensity *= visíveis/4`;
`pos +=` centroide; abaixo de 1/256 a entrada some. Para a prévia: ou testar a profundidade da própria prévia (oclusão a 1,5 unidade
da lente, igual ao `drawFlares`), ou supor visível (fração 1).

Microprograma 0x2B20 por entrada (já no `flareSprites` do `wotm_fx.hpp`) **[C]**:
`Q = 1/W; brilho = I Q²; tamanho = √I Q`; com flags & 8: `f = max(dot, 0) + bw − 1`, some se `f ≤ 0`, `brilho *= f/bw`,
`tamanho *= f/bw`. Com bw = 0,9738 o flare acende com o feixe a menos de ~88,5° da câmera (dot > 0,026) e cai linear com o cos.

## 3. Os 18 sprites: constantes do ELF

**[C]** O `lensFlarePacketInit` copia seis vetores inicializados do `.data` (dados do ELF, não montados de arquivo) para a pilha:
`D_006F44E0` (R[18], G[18], B[18], bytes), `D_006F4518` (índice no `TextureInfo`, byte), `D_006F4530` (t, float),
`D_006F4578` (meio-lado, float), `D_006F45C0` (A, float), `D_006F4608` (seletor de UV, byte; vai nos 2 bits baixos do float do
meio-lado). Lidos do `disc/SCUS_971.97` e iguais, sprite a sprite, às duas cópias do pacote no dump `ram_energy_robot.bin`
(0 diferenças). Seletor → uvMax: 0 = 31,5 (0x1F8/16), 1 = 63,5 (0x3F8), 2 = 127,5 (0x7F8, não usado).

| k | texInfo | t | meio-lado | sel (uvMax) | R G B | A |
|---|---|---|---|---|---|---|
| 0 | 17 | 1.0000 | 100 | 1 (63,5) | 255 255 255 | 8.0 |
| 1 | 15 | 0.1500 | 6 | 0 (31,5) | 192 192 255 | 1.4 |
| 2 | 19 | 0.1250 | 6 | 0 | 255 192 224 | 1.0 |
| 3 | 17 | -0.7750 | 30 | 1 | 255 224 192 | 0.9 |
| 4 | 17 | -0.1250 | 20 | 1 | 192 255 224 | 0.8 |
| 5 | 17 | 0.1750 | 20 | 1 | 255 192 224 | 0.75 |
| 6 | 19 | -0.1000 | 8 | 0 | 224 192 255 | 0.7 |
| 7 | 17 | -0.6250 | 40 | 1 | 224 192 255 | 0.7 |
| 8 | 18 | 1.1250 | 150 | 1 | 255 255 255 | 0.7 |
| 9 | 15 | 0.2500 | 10 | 0 | 192 255 192 | 0.7 |
| 10 | 19 | -0.1500 | 12 | 0 | 192 224 255 | 0.6 |
| 11 | 19 | -0.7000 | 32 | 0 | 192 255 224 | 0.5 |
| 12 | 19 | 0.3500 | 14 | 0 | 255 224 192 | 0.5 |
| 13 | 15 | -0.3000 | 20 | 0 | 255 255 192 | 0.5 |
| 14 | 15 | 0.4000 | 16 | 0 | 255 192 192 | 0.4 |
| 15 | 18 | -0.5750 | 30 | 1 | 128 255 192 | 0.2 |
| 16 | 15 | -0.0750 | 30 | 0 | 255 192 255 | 0.125 |
| 17 | 18 | -1.1250 | 120 | 1 | 64 255 128 | 0.125 |

Uso (microprograma): sprite k no ponto `C + t (S − C)` (C = centro da vista, S = a luz na tela). k = 0: meio-lado
`100 · tamanho · 8` px do GS, cor `min(255, RGB · rgb)` (não depende da distância). k > 0: meio-lado fixo (px do GS), cor
`min(32, RGB · rgb · A · brilho)`. Cantos `±(h, h/2)` em px do GS (Y em linhas de campo); UV de (0,5; 0,5) no canto + até uvMax no
canto −; cor de vértice com alfa 0x80, `ALPHA = 0x48` (`Cs·As + Cd`, aditivo), `TEX0` com TCC = 1, TFX = 0 (MODULATE). Depois de um
sprite com as três cores < 1 a entrada sai dos seguintes.

Consequência para os monstros: com I = 40 o brilho `I/W²` só passa de 1/(255·1,4) com W < ~120 unidades; mais longe só o sprite 0
aparece (é o que se vê na luta). Na prévia, os fantasmas k > 0 aparecem só se a câmera estiver perto.

## 4. Texturas dos sprites

**[C]** `TextureInfo[i]` é preenchido pelo `particleParseTexture(i, nó)`, chamado pelo `dbProcInteractive` para os nós com
`id1` em 0x1B58..0x1C1F (i = id1 − 0x1B58). Esses nós estão em **todos os `disc/LVL/*.NGP`** (61 a 64 texturas por fase) e em
nenhum `.NGP` do shell ou de monstro: o OBJECT (o filho 0, quando o nó é GROUP) dá `texId[0]` (+0x50) e `gsTexCtx[0].tex0` (+0x70).
Os pixels estão no `.TEX` da fase e a paleta no `.RTX` (CBP do TEX0 = `tbp` da entrada CT32 16x16). Na luta o
`texmParticleTexture` sobe a textura para a VRAM em tempo de execução (por isso o TBP do pacote muda; irrelevante fora do jogo).

Na `CENTRAL` (e com os mesmos números em 3-MILE, AIRPORT, BIGSHOT, CANYON2, CAPITOL, CRUSH, DBALL, METRO, MINISF, SANFRAN, TOKYO,
TRAINING, UFO, VEGAS, VOLCANO):

| texInfo | sprites | texId | formato | tamanho | paleta (tbp no .RTX) | aparência |
|---|---|---|---|---|---|---|
| 17 | 0, 3, 4, 5, 7 | 4 | T8, CSM1 | 64x64 | 9 | branco, alfa radial (o brilho principal) |
| 15 | 1, 9, 13, 14, 16 | 54 | T8 | 32x32 | 203 | hexágono (íris) |
| 18 | 8, 15, 17 | 55 | T8 | 64x64 | 207 | anel colorido (arco-íris) |
| 19 | 2, 6, 10, 11, 12 | 56 | T8 | 32x32 | 211 | disco claro com alfa 0,5 |

`TEX0` no `.NGP` da CENTRAL: 17 = `0x2000012599304000`, 15 = `0x2000196555304000`, 18 = `0x200019E599304000`,
19 = `0x20001A6555304000` (TBW 1, PSM 0x13, CPSM 0, CSM 0, TCC 1). Pixels idênticos (md5 do RGBA decodificado) em todas as fases nas
texturas 15, 18 e 19; a 17 difere só em CANYON1 e TOKYODM. ISLAND, CANYON1 e TOKYODM usam outros texIds (5/55/56/57, 3/52/53/54,
4/53/54/55), por isso a busca deve ser pelo `id1` do nó (0x1B58 + 15/17/18/19), não pelo texId. Decodificação: `tools/tex.py`
(`Library(tex, rtx, ngp_path=...)`, `image(texId, tex0)`), alfa do GS 0x80 = 1,0 (o PNG exportado guarda o dobro).

## Receita para a prévia (câmera fixa)

1. Na travessia do modelo, `case 5`: se `flags & 3 == 3`, guardar `P = (position,1)·M`, `D = normalize((beamNormal,0)·M)`, I, rgb,
   bw, flags.
2. Por quadro: `W` = profundidade de P na câmera da prévia (distância ao longo da frente), `S` = P projetado; `dot = D · normalize(olho − P)`.
3. `Q = 1/W; brilho = I Q²; tam = √I Q`; se flags & 8: `f = max(dot,0) + bw − 1`; `f ≤ 0` → nada; `brilho *= f/bw; tam *= f/bw`.
4. Oclusão opcional: 2x2 pixels da profundidade da prévia em S, visível se a profundidade da cena ≥ W − 1,5; `I *= visíveis/4`
   (recalcular brilho/tam com o I novo).
5. Para os 18 sprites da tabela: centro `C + t (S − C)` (C = centro da prévia); k = 0: `h = 100·tam·8`, cor `min(255, RGB·rgb)`;
   k > 0: `h = meio-lado`, cor `min(32, RGB·rgb·A·brilho)`; parar a entrada quando as três cores < 1. `h` está em px do GS: na prévia,
   `meia-largura = h·f/384` e `meia-altura = h·f/360,96` (f = foco da prévia em px, `(altura/2)/tan(fovy/2)`), o que mantém a
   proporção flare/monstro do jogo (os dois escalam com 1/W).
6. Desenhar sem teste de profundidade, aditivo (`Cs·As + Cd`), textura MODULATE com cor de vértice/128 e alfa 0x80, UV (0,5..uvMax)/lado.
