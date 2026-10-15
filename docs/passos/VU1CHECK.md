# Checagem VU1 x nativo (2026-10-14)

Fonte única do estado, do que foi corrigido e das pistas da checagem. Índice em `docs/PROXIMOS_PASSOS.md`; o método em detalhe (coleta por cadeia, faces de costas, formato da linha de saída) fica em `NATIVO.md`, seção "Runtime: checagem VU1 x nativo".

## Como funciona
- `WOTM_VU1CHECK=<s>` (runtime, `wotm_vu1check.inc`): a cada `<s>` segundos, num quadro da luta, o VU1 executa a cadeia do quadro. Cada triângulo do XGKICK é casado (1 px) com o triângulo nativo do mesmo objeto (`polyPkt`), e cada quadrado de partícula com o do seu sistema `fx[i]`.
- Compara geometria, textura (TEX0 por triângulo; o casamento prefere pares de mesma textura) e a forma das partículas.
- Conta à parte:
  - `repetidos`: passadas da sombra projetada sobre o que já casou;
  - `so pela sombra`: triângulo com a textura da sombra (tbp 3e60) que nenhuma passada base casou (ver pista 9);
  - `em textura (sombra)`: triângulos desenhados num FRAME que o quadro usa como textura (a silhueta no quadro da sombra).
- Objeto que não casa com a coleta nativa da sua cadeia tenta a da 1ª cadeia (a OT de alfa sai no fim do quadro).
- `tools/lote.ps1` liga por padrão com `-Check 1`. O `resumo.csv` traz `casados_pior`, `textura_difere_pior`, `fx_casados_pior`, `vif1_fora_de_passo` (deve ser 0) e outras colunas.
- `WOTM_VU1CHECK_DUMP=<prefixo>` grava os triângulos e quadrados de cada checagem em `<prefixo>_<g_frame>.csv`, para análise offline (o `lote.ps1` herda a variável do ambiente). Por sistema de partículas grava também a câmera da comparação (`fxcam`: W 4x4 por linhas e olho, projeção como o `projectPt`) e as partículas cruas (`fxp`: pos, vel, idade, minSize, maxSize) e as matrizes cruas (`fxmat`).

## Estado
- Central, 1 contra 1, 100 s, uma checagem por segundo: de 97,7% a 100% dos triângulos do VU1 casam; 5 de 91 checagens ficam abaixo de 99%. Textura: de 0 a 1 triângulo diferente por checagem.
- Lote 1 contra 2 IAs, fases 1 e 4: pior caso de 97,4% e 96,0%.
- METRO, 1 contra 1, 60 s, 11 checagens: pior caso de 96,2% a 99,9%; textura: de 0 a 1 triângulo diferente por checagem (era 90, pista 8).

## Em aberto
Não corrigir sem perguntar ao dono qual atacar.

6. **Resto das partículas:**
   - **Sprine (8):** a contagem casa (`N = max(2, int(+0x2D8C + 0,5))` passos por trecho), mas os pontos ficam a cerca de 2 px. A largura `minSize` não foi conferida. Lido em parte (microprograma em 0x2770 = `D_2770` do `sprinePacketBuild`):
     - A perturbação é confirmada: `RINIT R` com o `pos.w` da partícula e `RNEXT` a cada passo, ou seja, o gerador aleatório do VU semeado por partícula. Para casar, o nativo tem que emular o `R` do VU (LFSR de 23 bits).
     - A curva anda por diferenças avançadas, com a base e os coeficientes do bloco do VU em 300–306 (vf20–vf26; `vi9` = contagem em 306.z).
     - O cabeçalho que o `sprinePacketBuild` grava antes do `pos[]`: uma linha da `ParticleToEmitMat` (`fx + 0x2CD0`), `(n − 1)·N`, `idade[n − 1]·N` e `n − 1`.
   - **Arco elétrico (7, `common/electric`):** subdivisão e zigue-zague feitos no microprograma, com semente `+0x2DC0` e jitter `+0x2E50`. Pede ler o microprograma.
   - **Fase 8, objetos (`casados_pior` de ~92%):** o pior caso é sempre o mesmo, em 1 ou 2 checagens por rodada, só no começo da luta (g de 279 a 486, de 5 a 8 s; nas fases 1 e 4 não aparece).
     - O que se vê: o VU1 desenha os dois monstros no LOD mais baixo, e o nativo não põe nenhum triângulo deles na tela.
       - O adversário aparece como objeto com item nativo e `nativo 0`: o skinned de 309 triângulos (Robo-47, nó 1239ac0, tex 294/295) mais uma parte rígida de 50 (flags 311). Com a Togera: 270 + 58, tex 285.
       - O jogador cai em `sem objeto` (Congar no LOD 0, nó 111a0f0, 214 triângulos).
     - Medido em g=431 (`WOTM_LISTSKIN`): no desenho ao vivo, o nativo tem o 1239ac0 (LOD 0, `animPkt` 1255b30) em (1145, −104, −26), a mesma posição dos outros LODs. Ou seja, o item existe, mas nenhum triângulo dele cai na tela quando projetado com a vista 0.
     - Suspeita: nesse trecho (introdução da luta?) os monstros saem por outra câmera ou vista, e a checagem só projeta com o `worldToScreenMat` da vista 0. Próximo passo: ver qual matriz a cadeia desses objetos usa e, se for outra vista, decidir se o nativo deve desenhá-la. Não depende do Robo-47.
   - **Arco elétrico (7) é do Robo-47:** 146 linhas de tipo 7 numa rodada com ele e 0 numa rodada sem ele (Congar contra Togera, fase 8). São vários sistemas (fx 72 a 75). Não conferido se é a cabeça.
   - **Pedestres na METRO:** são 8 sistemas de 125 (fx 91 a 98). Em cerca de 5% das checagens o VU1 não desenha nenhum deles (`0/0/1000`), e às vezes só parte (`750/750/1000`). O nativo desenha todos. Não é o teste de visão (já aplicado, quase não mudou) nem o `info.attribs` (0x400 nos dois casos; o `emit.info.attribs` dos pedestres é 0). Suspeita: instante (os pedestres saem noutro trecho do quadro?). Em 90 s: VU1 19108, nativo 23858.
   - **Lens flare (tipo 11):** era o "sistema que só o VU1 tem" de 2 a 4 quadrados (fx 80 na fase 8). Não passa pelo `particleDraw`: o `hier` põe as luzes com flare numa lista dentro de `lensFlarePacket[2]` (`particleAddLensFlareToList`), o `particleLensFlareTestZbuffer` testa a visibilidade no z-buffer e o `particleLensFlareRender` desenha (retângulos alinhados à tela). O nativo não desenha flares: é recurso a implementar, não correção.
   - **SPRITE do GS fora de `fx[]` (identificado):** de 340 a 380 por quadro. Não é HUD: vêm do `dustPacket` (`D_0078F6E0`, montado por `texDrawDustPacketBuild` no `particleInitAfter`) e de um trecho do `displayList`. É a composição das texturas de poeira e fumaça iluminadas na VRAM (0xFD800), que as partículas "compostas" (texInfo 0xFFFF) usam; o nativo as troca por um círculo suave. Falta: a checagem contá-los como render-to-texture, e o nativo gerar essas texturas (parâmetros em `dustPktParams`, luz em `dustLighting`). O dump grava as origens (`rectorf`).
7. **Itens de alfa animados** (tex 330/333, e tex 0 com flags `0xa210`/`0xa310`): às vezes saem de 1 a 6 px fora, em direções variadas. Parece questão de instante da coleta.
8. Corrigida (ver "Corrigido"). Falta só identificar o que é o objeto da METRO (vidro, água, distorção?).
10. **Textura trocada entre tex 156 e 193:** 1 triângulo de vez em quando (sobreposição?).

## Explicadas, sem bug no nativo
- **Pista 9, `so pela sombra` (de 0 a 22 triângulos por checagem):** analisado com dump na METRO (22 triângulos em 11 checagens, todos com tbp 3e60). Dois casos:
  - **Faces de costas que só a sombra desenha** (17 de 22 de costas; 15 sem triângulo da passada base no lugar): tudo indica que a passada base descarta as faces de costas e a passada da sombra projetada (`psAttachDmaToAlpha`) as redesenha sem culling (não conferido no microprograma). Em objetos pequenos (de 1 a 8 triângulos) todos os triângulos do VU1 são dessa passada. Isso também engana a regra de faces de costas da checagem: com só triângulos de costas, o objeto parece "sem culling".
  - **Triângulos finos** (≤ 1 px de altura), com a passada base no mesmo lugar: ambiguidade do casamento guloso.
  - **Ajuste possível na checagem:** separar os triângulos com a textura da sombra antes da regra de culling e contá-los como repetidos. Só muda a contagem.

## Corrigido
Bugs do nativo:
- **Pista 6, `fx[100]`** (d43c9c3): o laço dos destroços (e o do `viewer`) parava em fx 97; o VU1 desenha destroços em fx 98 e 99 (os "sistemas que só o VU1 tem" de até 110 quadrados). Agora vai até `fxaddr::count`.
- **Pista 6, teste de visão das partículas:** o nativo pulava o `viewInFOV(0, &FOVsphere, &FOVextent)` do `particleDraw` (caixa contra os 6 planos de `worldCtx[0]`). Agora `Particles::inViewFov` o aplica (sistemas e destroços); nenhum sistema que o VU1 desenha ficou de fora.
- **Pista 8, texId 0 = quadro em exibição:** o nativo desenhava sem textura um objeto da METRO (nó `a1c560`, 92 triângulos, flags `0x8316`) que o VU1 texturiza com o framebuffer. A cada quadro o `rtMain` chama `texmPrepTexInfoZero` com o `DISPFB2` de `viewGetDb(0)` (+0x10 ou +0x50, pela paridade de `g_frame`), que grava `displayBuffer` (0x6F87F8) = `FBP << 5`; o `texmGetTextureAddrs` monta para texId 0 o TEX0 `displayBuffer | 0x28128000` com TH 8 (PSMCT24, TBW 10, janela 1024x256 sobre o buffer em exibição, isto é, o quadro anterior). Os dois buffers de 640x224 ficam em FBP 0x46 e 0x8C (tbp `8c0`/`1180`). Agora o `texKey` dá `kScreenKey` para texId 0, o runtime alterna duas render textures e passa a do quadro anterior (`Scene::screenTexture`), e o UV vira texel do GS na tela 640x224 (`u·1024/640`, `1 − v·256/224`). A checagem compara com o TEX0 montado a partir do `displayBuffer`: textura difere na METRO caiu de 90 para de 0 a 1.
- **PRIM.TME da seção** (08a4df2): as seções só com cor de vértice saíam com a textura do slot 0. Agora usam o slot `kSlotNoTex`.
- **Sprite e texSprite com o dobro do tamanho:** o meio-lado é `pos.w/2·foco/profundidade`.
- **Pista 3, `_cs` com matriz não ortonormal** (8020844): o VU1 desenha em `olho + (trans − olho)·RᵀR`. Afetava os destroços que encolhem. Corrigido no `Scene::drawCs`.
- **Pista 6, partículas:** o `pos.w` não é tamanho em nenhum dos tipos abaixo.
  - **Pedestres (14)** (631a053): o `pos.w` é `float(variante)`, com o quadro da animação na meia-palavra baixa. Agora são carimbos `minSize × maxSize` em pé no Z, apoiados em `pos`. Casam 100%.
  - **Destroços (13)** (2f5f471): SPRITE alinhado à tela de lado `max(minS, 2·minS·|w|)`, com os bits baixos do `w` escolhendo a célula e o espelhamento. Casam de 96% a 100%.
  - **Stripe (5)** (070c3f9): perpendicular à faixa **na tela**, com meia-largura `|pos.w| − |pos.w[n−1]| + minSize` (a faixa afina com o emissor parado). Casam de 94% a 100%.
  - **Strite (6):** lido no microprograma (entrada 0xA88 = `D_A88` do `stritePacketBuild`) e conferido com dump, erro < 0,1 px. `H = M·pos` com a matriz atual e `T = M′·(pos − vel)` com a **matriz do quadro anterior**, a `prevViewMat` (`fx + 0x2EC0`, 4 qwords no pacote; colunas x/y/z/translação em mundo, zerada nos outros tipos). Ponta `A = 3T − 2H`, base `B = 3H − 2T`, cada um dividido pelo seu w. A direção `m` é `B − A` normalizada no espaço dividido pelos focos (fx, fy: 384 e 180,48 na luta). Em pixels, a ponta recua `0,5·pos.w·(m.x·fx, m.y·fy)/w`, a base avança `0,25·…` e a meia-largura é `0,433·pos.w·(−m.y·fx, m.x·fy)/w` (0,433 = 0,866/2, o `I` do microprograma). O nativo (`Particles::striteTriangle`) faz a conta em pixels do GS e volta ao mundo pela matriz atual. Casam 100% (118 de 118 na fase 8).
  - **Wake (16):** um triângulo por partícula, equilátero no plano XY do mundo, centrado em `pos`, com um vértice em +X. Raio `R = minSize/2 + 120·w + 7,5·s·(2 − s)`, com `s = min(w/0,235, 1)`, medido só no preset 146 (minSize = maxSize = 90, w de 0 a 0,94; erro < 0,4 unidade; o termo que satura é ajuste). Casam 100% (372 de 372 na fase 8). A checagem compara os triângulos soltos de strite e wake com o quad degenerado do nativo.
  - **dustTrail (15)** (91d1e7b): o `pos.w` é a idade normalizada. Meio-lado `S/2 + exp·vida·w/2`, limitado a 0,375·profundidade. O termo `S ≈ 1,12·max·a3/255 − 0,3·min` é aproximado (até 4% de erro). Razão de tamanho de 0,98 a 1,06.
- **Pista 7, itens** (4f249df):
  - ANIM_SCALE leva o filho por `scale·(p + preScale) + translate + postScale` (os itens presos aos monstros saíam até 42 px fora).
  - LOD faz cross-fade como o `hierLod`: na faixa de `fade`, desenha o LOD `i` e o `i−1` com alfa (`DrawItem::fade`).

Bugs do runtime:
- **Pistas 2 e 5, VIF1 fora de passo** (dfd525a): o DIRECT com imagem GIF maior que ele consumia códigos VIF como pixels, gravando ROW/MODE falsos e ligando o ADC em todo vértice. A METRO ia de 0% a 94%–99,8%; os monstros com 3 IAs ficavam com 20% a mais de triângulos. Guarda: `VIF1 fora de passo N`.
- **Pista 1, OT de alfa fora da checagem** (f8a1d84): o `hook_particleDraw` zerava os contadores antes do `hierFlushObjQ`. No quadro amostrado, agora dispara o flush antes.
- **Fase 8, VIF1 fora de passo na sombra projetada (bug do recompilador, `VSQI`):**
  - Sintoma: esporádico (de 38 a 2544 em 5 de 6 rodadas). O CALL do `psEndBlkObjs(1)` para o slot 1 do `psObjDma` (`0x520800`) achava dados (`3f80…`), não a tag CNT + FLUSH + DIRECT que o `psFindActiveBlockers` grava.
  - Causa imediata: estouro do slot 0. Cada blocker tem 0x4000 bytes (1024 qwords) no `psObjDma`, e o `psObjectCull` acrescenta objetos sem checar o limite. Na falha, `psObjIdx` = 1052 com 62 objetos nos dois blockers, e a cauda do slot 0 apaga o começo do slot 1.
  - Causa real: o teste de FOV da luz (`psFovTestAsm`, `vcallms 0xC8` no VU0) deixava passar 100% dos objetos. Os planos do frustum da sombra (qwords 22–27 da memória do VU0) estavam zerados. Quem os grava é o `viewStoreNorms1/2InVu0` (chamados pelo `viewShadowUpdate`), com `VSQI` em modo macro.
  - O tradutor do PS2Recomp (`translateVU_VSQI`/`VSQD` em `ps2xRecomp/src/lib/vu_translation_helpers.cpp`) trocava os campos (`fs` = rd, `it` = rt) e gravava na RDRAM em vez da memória de dados do VU0. O código gerado escrevia em `ctx->vi[17..19]`, fora do array de 16 (em cima de `vu0_q`/`vu0_p`).
  - Correção (no clone):
    - tradutor corrigido para `VLQI`, `VSQI`, `VLQD`, `VSQD`, `VILWR` e `VISWR`, que agora acessam a memória de dados do VU0 (`ILWR`/`ISWR` endereçam por qword mais o campo);
    - as duas funções geradas foram reescritas em `runner/` e em `gen/output_fix` (são as únicas que usam essas instruções).
  - Resultado: o FOV corta objetos e `vif1_fora_de_passo` = 0 em 3 de 3 rodadas da fase 8. As fases 1 e 4 continuam iguais.
  - Falta: levar a correção do tradutor ao fork/upstream do PS2Recomp. O diagnóstico `[dma]` (CALL para o `psObjDma` sem tag, com `psObjIdx`/`psNumObjects` por slot) ficou no `ps2_memory.cpp` do clone.

Defeitos da própria checagem:
- **Pista 4, textura** (4769b05): o casamento guloso trocava camadas de slots diferentes. A hipótese do seletor 0 herdando o TEX0 foi descartada com teste.
- **Pista 9, sombra:** a passada da sombra acusava "textura difere". Agora conta à parte (`so pela sombra`).
- **Pista 7, render-to-texture e instante da coleta:** ver "Como funciona".

## O que a checagem não cobre
- Texels decodificados: ela compara qual textura, não os pixels.
- UV, cor de vértice, ALPHA/mistura: por isso não pega as texturas animadas nem a explosão opaca.
- Textura e cor das partículas.
- Linhas e leques do VU1 (só contados).
- HUD: vai direto ao GS, fora do VU1.

## Próximos passos
1. Pistas em aberto (perguntar qual antes de corrigir).
2. Estender a comparação para UV, RGBA de vértice e ALPHA/ABE por triângulo (alcança as texturas animadas e a explosão), e talvez um hash da CLUT por quadro.
3. HUD: tap nos pacotes que vão direto ao GS (PATH2/PATH3). Os SPRITE fora de `fx[]` já foram identificados (textura composta da poeira, pista 6).

## Notas
- O caminho emulado inteiro (sem `PS2X_NO_VU1`) está quebrado: trava no laço do `particleDraw` e depois cai em `GS::consumeLocalToHostBytes`. A checagem não depende dele (detalhes em `NATIVO.md`).
- O `run_live.ps1` (fora do repo) usa `Start-Process -RedirectStandardError`. O `2>` do PowerShell 5 gravava em UTF-16 e quebrava as linhas em 120 colunas.
