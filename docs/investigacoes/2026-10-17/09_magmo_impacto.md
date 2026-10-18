# 09: partículas gigantes no impacto do projétil do Magmo

Item "Projétil do Magmo" de `docs/passos/VU1CHECK.md` ("Em aberto", fotos do dono de 2026-10-17). Só investigação: nada no repo nem no runtime foi alterado.

**2026-10-18: o passo 1 de "Como resolver" foi aplicado** (`Particles::shadowRam`, `Snapshot::fxRam`; `WOTM_FXLIVE=1` volta ao antigo). Pendente de validação visual do dono; validado, este arquivo pode ser apagado.

## Resumo

**Não é posição nem matriz mal lida. É uma corrida entre a thread do EE e a apresentação.** O instantâneo do quadro (`Snapshot`) guarda de cada sistema de partículas só o endereço, o tipo e a **contagem** `numParticles` do fim do quadro N. As posições, tamanhos, idades e cores são lidas **da RAM viva** na hora do desenho, numa outra thread. Se a apresentação redesenha o quadro N depois que o `particleMove` do quadro N+1 já matou partículas (a contagem viva caiu), o nativo lê `pos[n]`, a qword logo depois da última partícula. Ali o `*PacketBuild` gravou o fecho do pacote do VIF.

No `Sparkle` (preset 27 = 0x1B, texSprite), que o impacto do Magmo cria duas vezes, essa qword é `00000000 00000000 00000000 50000002`. Lida como partícula, vira um ponto em **(0, 0, 0)** com `pos.w = 8,59e9`. O desenho do texSprite limita o meio-lado a 0,25 × profundidade, então sai um quadrado do tamanho máximo do microprograma (192 × 90 px do GS, ~40% da altura da tela) **na origem do mapa**. Em mundo, ele cresce com a distância da câmera à origem.

Isso bate com os quatro pontos do relato do dono e com o "pisca" que a foto não pega (detalhes abaixo). O VU1 do runtime não tem o defeito.

Grau de certeza: **alto (~85%)** de que este é o defeito relatado. O mecanismo (contagem do instantâneo e dados vivos) está confirmado no código, e o conteúdo de `pos[n]` foi medido na foto 005. Falta a confirmação ao vivo depois da correção, porque nenhuma foto consegue mostrar o defeito (ver "Por que a foto não pega").

## O que o impacto cria

`Weapons::...` (explosão por tipo, `src/game/Weapon.cpp:1796-1809`), casos 0x15/0x16 (lava do `StateLavaBlast`):

| chamada | preset | tipo (vis.attribs) | caminho nativo |
|---|---|---|---|
| `particleCreateFx(&res.point, 0x6B, 1)` + `ricochetParticlesOffSurface` | 107 `LavaFrags` | 13 frag | `Scene::collectParticles` (copia `Sprite`, seguro) |
| `particleCreateFx(&res.point, 9, 5)` | 9 `WhiteExplosionFx` | 4 texSprite | `Particles` (lê RAM viva) |
| 2 × `particleCreateFx(&res.point, 0x1B, 1, m)` | 27 `Sparkle` (Glitter, vida 30, emissor 120 campos, autoanimado com gravidade) | 4 texSprite | `Particles` (lê RAM viva) |
| 2 × `particleCreateFx(&res.point, 0x6A, 1, m)` | 106 `ExploSmokeTrail` | 5 stripe | `Particles` (lê RAM viva) |

Além disso, o `GreatBallsOfire` (preset 47, sprine) do próprio projétil (`Weapon.cpp:1402`) leva `particleKillFx` (para de emitir e morre quando as partículas morrem). O sprine tem guarda contra a corrida (`sprineQuads` compara `q306` com `(s.n - 1) N`, `native/wotm_fx.hpp:1005`), então não entra.

Posição das partículas: todas em mundo. `particleCreateFx` (`src/common/particle.cpp:1912`) põe `StaticVec`, `ParticleToEmitVec` e `prevParticleToEmitVec` em `res.point`, e `particleEmit` soma a esse ponto. Não existe espaço local nem `pos.w` de posição nesses tipos.

## Evidência (foto `2026-10-17_171318/foto_005`, CANYON2, g_frame 17859)

Lido da RAM do instantâneo (registro 'A') com um script de rascunho (sistemas ativos de `fx[]`, `pos[0..]`, e a qword `pos[n]`):

| sistema | preset | tipo | n | `pos[n]` (hex) | como float |
|---|---|---|---|---|---|
| #40, #47, #83, #88 | 27 Sparkle | texSprite | 84, 74, 74, 66 | `00000000 00000000 00000000 50000002` | (0, 0, 0), w = **8,59e9** |
| #41, #44, #45, #66, #73, #74, #92, #94 | 106 ExploSmokeTrail | stripe | 36 / 52 | `13000000 140002e1 00000000 00000000` | (1,6e-27, 6,5e-27, 0), w = 0 |
| #64 | 44 Flames | sprite | 40 | `13000000 140002e1 00000000 00000000` | idem |
| #48 | 105 FootStomp | sprite | 12 | `00000002 01000106 6202c001 00001f1f` | z = 6e20 (fora da tela) |

- `pos[]`, `vel[]` e `age[]` ficam dentro do próprio `fx[i]` (`pos` em +0x10 no texSprite, +0x180 no stripe, +0xE0 no sprite...). `fragPacketBuild` (e os outros `*PacketBuild`) chamam `setClosingContext(pos + 16 n, ...)`, que grava FLUSHA/MSCAL/FLUSH/DIRECT logo depois da última partícula. Quando `n` cai, o fecho anda para trás e a qword que era uma partícula viva vira fecho.
- Nesta foto, as partículas do impacto estão perto da origem: os rastros 106 começam em ~(80, −315, 115) e os Sparkle em (−80, −79, −40), (22, −60, 82)... com o olho em (−276, −669, 168). Por isso "nasce no centro do mapa" e "no impacto" se confundem no relato. O quadrado espúrio, porém, fica **sempre** em (0, 0, 0), onde quer que o projétil acerte.
- Replay desta foto (`tools/vu1cmp.ps1`, sem corrida): partículas 572 VU1 contra 576 nativo, **100% casados**, os 12 sistemas nos dois lados. No instantâneo, o desenho do nativo está certo; o defeito não aparece no replay.

## O caminho no nativo

1. Captura no fim do quadro, na thread do EE (`wotmCaptureFrame`, `native/runtime/wotm_live.inc:490-528`): `cap.collectFx(ram)` → `Particles::collect` (`native/wotm_fx.hpp:355-377`) guarda `FxSystem{base, index, type, n}` com `n = numParticles` **de agora**. `Snapshot::fx` (`native/wotm_native.hpp:602`, `takeSnapshot` em 1467) só copia essa lista.
2. Apresentação, em outra thread, a cada quadro do host (`wotm_live.inc:690` → `Scene::draw` → `fx.draw(ram, ...)`, `wotm_native.hpp:2167`). `ram` é a RDRAM viva, e o instantâneo se repete até chegar outro ("os desenhos até o próximo quadro do jogo repetem este").
3. `Particles::forEachQuad` (`wotm_fx.hpp:1081` e seguintes) itera `i < s.n` (do instantâneo) e lê `pos + 16 i` da RAM viva (`P(i)`, linha 1087). No ramo de sprites (linhas 1224-1254), `half = 0,5 |w| = 4,3e9` e `r = min(half, 0,25 × depth)` dão `depth / 4`. O quadrado sai no teto `MINI` do microprograma (meio-lado 96 × 45,12 px do GS), centrado na origem. O teste `isfinite` (linha 1227) não barra, porque 8,59e9 é finito.
4. Stripe (106): o ponto falso entra em `pts` com `w = 0`. Como `wHead` vem de `P(s.n - 1)`, que agora é o falso, sai `0 − minSize = −4`. Toda a faixa engorda 4 e ganha um trecho da ponta até a origem: um risco até o centro do mapa.
5. A janela da corrida: entre o `particleMove` do quadro N+1 (dentro do `particlePrepDraw`/`particleDraw`, logo antes da captura N+1) e a captura N+1. Qualquer quadro do host desenhado nessa janela pega `n_vivo < s.n`. No impacto isso é frequente: os Sparkle (vida 30 campos) e o `WhiteExplosionFx` perdem partículas quase todo quadro por ~2 s. Daí o pisca.

Outras leituras vivas do mesmo `draw`, menos visíveis: a chave de ordem lê `emit.ParticleToEmitVec` (`wotm_fx.hpp:699-700`); o `striteTriangle` lê `worldToScreenMat`, `worldCtx.eo` e `prevViewMat` (303-312), que já podem ser da câmera do quadro seguinte; `setFog` lê `vu1FogParams`; o `dustTrailVu` e o `wakeVu` leem a palavra de idade e o cabeçalho do pacote.

## Por que o VU1 do runtime está certo

O `*PacketBuild` grava o UNPACK com a contagem viva em `pos − 4` (no `fragPacketBuild`: `0x6C000008 | n << 16`) e o fecho em `pos[n]`, no mesmo instante. O VU1 recebe a cadeia já montada e nunca lê `pos[n]` como partícula. Não há corrida porque ele consome a cópia DMA do quadro. O replay da foto 005 confirma: 100% dos quadrados casados.

## Por que a foto não pega

A foto (`WOTM_FOTOS`) para o EE no ponto da captura (`wotmPauseBlock`, `native/runtime/wotm_vu1check.inc:1812`), e o `vu1cmp` refaz o quadro sobre uma RAM fixa. Nos dois casos a contagem e os dados são do mesmo instante. **Nenhum instantâneo novo vai mostrar o defeito**, então não peça foto ao dono para isto. Se for preciso ver antes da correção, só uma gravação de tela ao vivo (OBS) de uma luta com o Magmo atirando: o quadrado (textura Glitter, amarelo/branco) fica fixo na origem do mapa e pisca nos ~2 s depois de cada acerto.

## Como resolver (passos)

1. **Copiar os dados das partículas no instantâneo** (correção de verdade). Em `wotmCaptureFrame`, depois do `collectFx`, copiar para o `Snapshot`:
   - o bloco `fx[]` inteiro (`wotm::sym::fx`, 100 × 0x2EE0 = 1,2 MB, ~0,1 ms de `memcpy`), que contém `pos[]`, `vel[]`, `age[]`, os cabeçalhos de pacote (sprine q300, wake q264, arco) e os campos `info`/`vis`/`emit`;
   - as tabelas e o estado que o `draw` lê: cores (`fxaddr::colors`, 0x256C10), `TextureInfo`, `texId`, `TEX0`, `worldToScreenMat` (5 × 0x40), `worldCtx[0]` (planos e `eo`), `vu1FogParams`, e o destino de `prevViewMat` de cada strite (`fx + 0x2EC0`, 64 bytes).

   O mais simples sem mexer em todas as leituras: um `Ram` de sombra para as partículas, um buffer de 32 MB por instantâneo (dois, alternados com o `Snapshot`; no Windows as páginas que ninguém toca não ocupam memória), onde só essas faixas são copiadas. O `Particles::draw`/`drawFlares`/`forEachQuad` recebem esse `Ram` em vez do vivo (`wotm_native.hpp:2167`). A checagem VU1 (`wotm_vu1check.inc`) já roda na thread do EE e continua com a RAM do instante.
2. **Paliativo barato** (se quiser um passo antes do 1): em `forEachQuad`/`drawSystem`/`sprineQuads`, usar `n = min(s.n, numParticles vivo)` e pular o sistema se `vis.attribs` ou `emit.fxId` vivos mudaram desde a coleta. Some o quadrado na origem, mas a corrida continua: posições de um quadro com contagem de outro, e partículas recém-compactadas trocam de lugar.
3. **Diagnóstico para conferir:** uma variável nova (por exemplo `WOTM_FXRACE=1`, que entra na tabela do `NATIVO.md`; `python tools/wotm_env.py` confere) que, no `draw`, conta por segundo os sistemas com `numParticles vivo < s.n` e imprime `[fx] corrida: N sistemas (preset, s.n, vivo)`. Antes da correção, numa luta com o Magmo atirando, deve acusar o preset 27 (e o 9 e o 106). Com o passo 1 não há o que comparar; o diagnóstico serve para provar a causa antes de corrigir.
4. Conferência: `tools\corpus.ps1 -Check -Baseline <csv>`, que não deve mudar nada, porque o replay não tem corrida. Depois `run_live.bat` numa fase com o Magmo: o dono acerta o projétil em monstro, prédio e chão, com a câmera longe e perto do centro do mapa. O quadrado na origem e o risco até a origem devem sumir. Depois disso, tirar o item de "Em aberto".

## Observações à parte (não são este defeito)

- No replay da foto 005, cada `Sparkle` tem 1 quadrado a mais no nativo que no VU1 (82/83, 74/75, 71/72, 66/67; total texSprite 293 × 297), todos pequenos (3 a 6 px). O VU1 descarta uma partícula por sistema que o nativo desenha, provavelmente a de idade no limite da vida ou a recém-emitida. Baixa prioridade, mas é o "sobram 4" da linha de partículas.
- `Scene::collectParticles` (frags) varre `fx[1..99]` sem olhar a lista de ativos nem o `0x400` (`wotm_native.hpp:1528-1532`). Hoje não faz mal (o `particleDestroyEffect` zera `vis.attribs`), mas é outra leitura que não segue o `particleDraw`.
