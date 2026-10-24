# Monstros 3D direto do disco (pose, esqueleto, texturas)

(Índice dos docs no `CLAUDE.md`.) Como montar um monstro animado só com os arquivos de `MON/`, sem o jogo nem dump de RAM. Foi
feito em 2026-10-20/21 para a lista de saves (monstro em idle sobre a foto da fase, `NATIVO.md`, "Save em JSON") e serve à prévia 3D da
seleção (item 12) e ao visualizador web (`VIEWER_WEB.md`). Formatos de base (malha, curvas) em `docs/FORMATOS.md`.

## Código
| Onde | O quê |
|---|---|
| `tools/animcurve.py` | avaliador de curvas (porte de `src/common/AnimCurve.cpp`) |
| `tools/pose.py` | pose de um monstro (`Model(ngp).pose(anim, quadro)`), `--list` mostra as animações; grava `.glb` posado |
| `tools/gltf.py` | exporta `.glb` (texturas do `tex.py`); com `model=` as malhas skinned saem na pose |
| `tools/monglb.py` | `.glb` animado para o viewer web: joints = ossos com saída (pai = quem gravou a matriz de partida na pilha), inversa de ligação = `restPoseInv`, partes rígidas como nós; curvas amostradas a 60/s; misturas do GS, textura rolando, fogo e lens flare nos extras. `--boss plant|root|mecha` monta os chefes das peças das fases (3-MILE, CAPITOL). Bate com o `pose.py` vértice a vértice |
| `tools/shellpics.py` | fotos largas das fases e logos dos monstros do `SHELL.TEX/.RTX` em PNG (as mesmas do `wotm_savepics.hpp`) |
| `native/wotm_model.hpp` | o mesmo em C++: `wotm::model::Model` (carga, pose, triângulos posados, texturas) e `Renderer` (raylib, RenderTexture) |
| `native/monview.cpp` | `build\monview.exe disc MONKEY0 [--anim 90] [--yaw 35] [--shot x.png]`: janela animada (setas giram, PgUp/PgDn trocam a animação) ou PNG |

Na lista de saves: `wotm_save.inc` posta um `savepics::kMonster` (monstro | traje << 8); o `Drawer::prepare` do `wotm_savepics.hpp`
desenha o monstro no RenderTexture antes do quadro (o `wotm_live.inc` chama), e o `draw` cola a textura entre a moldura e o texto.

## Arquivos
- `MON/<PREFIXO><traje>.NGP` (cru, sem zip), `.TEX`, `.RTX` (crus nos monstros) e `.PTR`. Prefixos: MONKEY Congar, ROBOT Robo-47,
  LIZARD Togera, SHOGUN Ultra-V, MANTIS Preytor, ENERGY Kineticlops, LAVA Magmo, DRAGON Raptros, ROCK Agamo, ALIEN Zorgulon, ASSBOSS
  Goliath Prime. Traje 0 a 3 = `Shell::m_costume` (`Shell::formatFilename1`).
- **O `.NGP` vem pré-ligado para 0xA00000.** Lido nessa base, os ponteiros valem sem o `.PTR` (o `.PTR` só desloca para onde o jogo
  carregou). O nativo põe o arquivo num buffer em 0xA00000 e usa o `Ram` de sempre (`decodeObject` serve sem mudança).
- Raiz 0 = o monstro (GROUP com o id do monstro); raiz 1 = GROUP id 0 (retratos do HUD e extras); depois COLL_GRID e ANIM_MGR.

## Animação
- Instância principal: o `CHAR_INSTANCE` (op 25) alcançável da raiz 0 com mais animações. `character` em +0x20 (`numAnims` u16 @0,
  `numChannels` u16 @2, `animations[]` @0xC); valores iniciais dos canais em `ci+4`.
- Animação `id` = a de `animations[k]` com `head >> 18 == id` (o Monster guarda a k-ésima como `m_anims[id]`, ids 0..299,
  `Monster::initAfterDbLoad`). `startTime` @4, `endTime` @8, `numChannels` u16 @0xC, curvas @0x18. **Idle = 90 (0x5A)**
  (`StateIdle::chooseBaseIdle`; 0x5F cansado, 0x60 com especial). Tempo em quadros de 60/s; o idle do Congar tem 160.
- Cada curva escreve `val[dataIdx]` (FORMATOS.md, "Animação"). Conferido contra `animOutput.val` de uma luta: 128 de 130 canais
  iguais (os 2 restantes, 0,007, vêm de uma camada de mistura).

## Travessia (matrizes das partes)
Vetores-linha (`p' = p·M`), filho = `local · pai`. Por nó:
- **ANIM_XFORM (17):** visível = `+0xC`, ou `val[visidx]` (`+0x10`) se o canal foi escrito; 0 = corta. Rotação = `-val` dos canais
  rX/rY/rZ (`+0x12..0x16`; sem canal, o vetor em `+0x20`), matriz `mathfRotMatrixPRH` (= microprograma 0x4D0 do VU0). Bits em
  `+0xC2`: bit 0 `L = pre·L` (`+0x40`), bit 1 `L = L·post` (`+0x80`). Depois `m[3] += translação` (canais `+0x18..0x1C` ou `+0x30`).
  Filhos em `+0xCC`, quantidade em `+4`.
  **Só remonta se algum canal estiver sujo:** `firstIdx` (s16 `+0x1E`) e máscara `animating` (u8 `+0xC1`); remonta se algum bit
  `b` da máscara estiver ligado e o canal `firstIdx+b` foi escrito. Senão a matriz é a do arquivo, `atMat[matrixIdx]` (`ci+0xC`,
  índice em `+0xC4`). Conferido no Robo-47 (`ram_1100`): 35 remontadas, 5 do arquivo, 0 diferenças; sem a regra as garras
  (índices 27 e 39) saem viradas.
- **ANIM_SCALE (38):** lê `val` direto (sem olhar se foi escrito); `pre(+0x40) · escala · post(t + 0x50)`; escala zero corta.
- GROUP (bit 0 de `+0xA`), LOD (o mais detalhado que o `hierLod` desenharia de perto: o maior índice com `switchOutDis` (`+0x24 + 16i`) > 0; o traje 3 do Togera tem um último nível com 0, nunca desenhado, e pegar o último dava a malha grosseira em A-pose), TRANSLATE, ROTATE, SCALE, SWITCH, CONTROL/ACTION_DATA,
  INTERACTIVE/DESTRUCTIBLE como no `Scene::walk` do nativo.

## Esqueleto e paleta
- **SKEL_BONE (35):** `numKids` +4, `skelOutputMatIdx` s16 +8, `isRootNode` +0xA, filhos +0xC, `restPoseInv` +0x10. Empilha o osso
  (`si+1`); se for raiz, empilha também uma identidade (`mi+1`).
- O **próximo nó de transformação** (ANIM_XFORM, TRANSLATE, ROTATE) com `si >= 0` desempilha: `smat[mi+1] = local · smat[mi]`,
  `paleta[osso.out] = restPoseInv · smat[mi+1]`, `si-1`, `mi+1`. O estado passa por valor a cada subárvore.
- Endereço de cada saída: `animMatrixPtr[out]` (`ci+0x14`). O OBJECT skinned acha o seu bloco por `animPktPtr` (`ci+0x18`)
  `[OBJECT+0x68]` (tag DMA ref, +4 = bloco), matriz do slot `s` em `bloco + 0x10 + 0x40·s`.
- **Skinning:** 3 ossos por vértice, `w1 = w`, `w2 = frac(w·2048)`, `w3 = 1 - w1 - w2`, slot = `(código & 0x7F) / 4`; depois a matriz
  do objeto. A normal gira só pelo osso x.
- Conferido contra a paleta de um dump de luta (`ram_1500`): igual. Em outros quadros a paleta da RAM está um quadro atrás do
  `atMat` (não é erro do modelo).

## Texturas
- Slot `s` do OBJECT: texId em `+0x50 + 2s`, TEX0 em `+0x70 + 0x30s` (quantos: `+0x5A`, até 5); o slot da seção vem da tag GIF
  (`Vtx::slot`).
- **Nos monstros os números do `.NGP` já são os do próprio arquivo:** texId = o do pacote do `.TEX` (ou do `.RTX`, ex.: texId 2 T8H
  do Congar), CBP = `tbp` da paleta CT32 do `.RTX`. Deslocamento zero nos 10 monstros e nos trajes (o `tex.Pairing` acha 0/0).
- Pixels lineares: `.TEX` em `+0x80`; `.RTX` em `+0x80` nos formatos altos (T8H/T4HL/T4HH) e `+0x10` no resto. Paleta 256 com troca
  CSM1 (bits 3 e 4) só nas T8; T4 usa `csa·16 + k` numa paleta de 256. Alfa do GS: `>= 0x80` = 255, senão `·2`; TCC 0 = opaco.

## Estado do GS por nó (mistura, teste de alfa, Z)
Como `Scene::groupState` do nativo: o contexto está em `+4` (GROUP, TRANSLATE, ROTATE, SCALE) ou `+8` (ANIM_XFORM, ANIM_SCALE); vale
para a subárvore. Pacote A+D com `+0x14 == 0x10000000`, `+0x18 == 0xE`, NLOOP em `+0x10`, pares a partir de `+0x20`: ALPHA_1 (0x42,
byte baixo), TEST_1 (0x47: ATE bit 0, ATST bits 1-3, AREF bits 4-11), ZBUF_1 (0x4E, ZMSK). Padrão: `0x44`, sem teste. A mistura só
vale com PRIM.ABE da malha. Modos vistos: `0x44` comum, `0x48` aditiva (Kineticlops), `0x42` `Cd - Cs·As`, `0x46` `Cd·(1-As)`,
`0x81` `(Cd-Cs)·As`. O `Renderer` reproduz com cor pré-multiplicada (opacos primeiro, depois os com ABE na ordem do modelo) e
desenha o fundo dentro do RenderTexture para os modos que dependem do destino. Deslocamento de UV: `+0x20/+0x24` do nó, ou os
canais `+0x30/+0x32` com o bit 0x800 em `+0xC`. No Kineticlops (trajes 0-2) esses canais (142/143) vêm da animação 0x91, que
só tem essas duas curvas e roda em laço por cima da principal (a idle grava 0 neles): U dá uma volta a cada 80 quadros, V duas.
Conferido no dump de luta `build/dumps_live/ram_energy_robot.bin` (0,637 e 1,275, proporção 1:2).

## Enquadramento
`Model::computeFraming(id)` amostra a animação inteira e fixa a caixa; sem isso a câmera acompanha o idle (zoom "dançando").

## Fogo e lens flare
Texturas dos efeitos vêm de `LVL/CENTRAL.TEX/.RTX` (`FxTextures`): Fireball = texId 5 (paleta tbp 13); flare = texIds 54/4/55/56
(paletas 203/9/207/211, texInfo 15/17/18/19). Detalhes da engenharia reversa em `75f5be1:docs/investigacoes/2026-10-21/fogo_agamo.md` e
`75f5be1:docs/investigacoes/2026-10-21/lens_flare_monstros.md`.
- **Fogo (nó op 22, HierParticleEmitter, preset +4 = 141 IdolHeadFlame).** Emissor E = posição(+0x10)·m. Partícula sprite aditiva,
  vida 20 fields, tamanho 7 + 8U, crescimento 1 − 1,5·min(idade/10, 1), vento (0,0,3,2), arrasto 0,0390625, 2 por field, cor por
  tabela de 16 (idade·3072 em 4.12), UV espelhado por 2 bits. Sistema novo começa com 30 passos de aquecimento. O enquadramento
  estica a caixa 55 unidades acima do emissor. Só o 141 é simulado (AttachedFlames e a cabeça voadora não).
- **Lens flare (nó op 5, HierLight com isOn e hasLensFlare).** P = pos·m, D = beam·m; brilho I/W², tamanho √I/W; direcional:
  k = max(dot,0) + bw − 1, escala k/bw. 18 sprites fixos ao longo de C + t(S − C), aditivos, depois do Mode3D, sem teste de Z
  (oclusão não reproduzida: sempre visível). Monstros com luz: ROBOT0–2, MONKEY1–3, JELLY0, ASSBOSS1.

## Não reproduzido
Molas do pelo e da cauda (`BoneSpring`: ficam na pose da animação), olhar/cabeça procedural, camadas de mistura de animação e a luz
do jogo (o `Renderer` usa uma direcional fixa + ambiente sobre a cor de vértice).

## Para a prévia da seleção e o viewer web
- Seleção: o mesmo `Model` + `Renderer` (um por jogador); trocar de traje = recarregar o `.NGP` (~1 MB, mais 10 MB de buffer).
- Web: portar `tools/pose.py` + `animcurve.py` (pequenos) para JS; as texturas e a malha seguem o `tools/gltf.py`.
