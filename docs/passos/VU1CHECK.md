# Checagem VU1 x nativo (2026-10-14)

(Índice em `docs/PROXIMOS_PASSOS.md`. Método, armadilhas e medições antigas: `NATIVO.md`, seção "Runtime: checagem VU1 x nativo".)

## Como funciona
- `WOTM_VU1CHECK=<s>` (runtime, `wotm_vu1check.inc`): a cada `<s>` segundos, num quadro da luta, o VU1 executa a cadeia do quadro. Cada triângulo do XGKICK é casado (1 px) com o triângulo nativo do mesmo objeto (`polyPkt`), e cada quadrado de partícula com o do seu sistema `fx[i]`.
- Compara geometria, textura (TEX0 por triângulo; o casamento prefere pares de mesma textura) e a forma das partículas.
- Separa três casos:
  - `repetidos`: passadas da sombra projetada sobre o que já casou;
  - `so pela sombra`: triângulo com a textura da sombra que nenhuma passada base casou;
  - `em textura (sombra)`: triângulos desenhados num FRAME que o quadro usa como textura (a silhueta no quadro da sombra).
- Objeto que não casa com a coleta nativa da sua cadeia tenta a da 1ª cadeia (a OT de alfa sai no fim do quadro).
- `tools/lote.ps1` liga por padrão com `-Check 1`. O `resumo.csv` traz `casados_pior`, `textura_difere_pior`, `fx_casados_pior`, `vif1_fora_de_passo` (deve ser 0) e outras colunas.
- `WOTM_VU1CHECK_DUMP=<prefixo>` grava os triângulos e quadrados de cada checagem em CSV, para análise offline.

## Estado
Central, 1 contra 1, 100 s, uma checagem por segundo: de 97,7% a 100% dos triângulos do VU1 casam; 5 de 91 checagens ficam abaixo de 99%.

Lote 1 contra 1, fases 1 e 4: pior caso de 97,4% e 96,0%. Textura: de 0 a 1 triângulo diferente por checagem em central.

## Corrigido
Bugs do nativo:
- **PRIM.TME da seção** (08a4df2): as seções só com cor de vértice saíam com a textura do slot 0. Agora usam o slot `kSlotNoTex`.
- **Sprite e texSprite com o dobro do tamanho:** o meio-lado é `pos.w/2·foco/profundidade`.
- **Pista 3, `_cs` com matriz não ortonormal** (8020844): o VU1 desenha em `olho + (trans − olho)·RᵀR`. Afetava os destroços que encolhem. Corrigido no `Scene::drawCs`.
- **Pista 6, partículas:** o `pos.w` não é tamanho em nenhum dos tipos abaixo.
  - **Pedestres (14)** (631a053): o `pos.w` é `float(variante)`, com o quadro da animação na meia-palavra baixa. Agora são carimbos `minSize × maxSize` em pé no Z, apoiados em `pos`. Casam 100%.
  - **Destroços (13)** (2f5f471): SPRITE alinhado à tela de lado `max(minS, 2·minS·|w|)`, com os bits baixos do `w` escolhendo a célula e o espelhamento. Casam de 96% a 100%.
  - **Stripe (5)** (070c3f9): perpendicular à faixa **na tela**, com meia-largura `|pos.w| − |pos.w[n−1]| + minSize` (a faixa afina com o emissor parado). Casam de 94% a 100%.
  - **dustTrail (15)** (91d1e7b): o `pos.w` é a idade normalizada. Meio-lado `S/2 + exp·vida·w/2`, limitado a 0,375·profundidade. O termo `S ≈ 1,12·max·a3/255 − 0,3·min` é aproximado (até 4% de erro). Razão de tamanho de 0,98 a 1,06.
- **Pista 7** (4f249df):
  - ANIM_SCALE leva o filho por `scale·(p + preScale) + translate + postScale` (os itens presos aos monstros saíam até 42 px fora).
  - LOD faz cross-fade como o `hierLod`: na faixa de `fade`, desenha o LOD `i` e o `i−1` com alfa (`DrawItem::fade`).

Bugs do runtime:
- **Pistas 2 e 5, VIF1 fora de passo** (dfd525a): o DIRECT com imagem GIF maior que ele consumia códigos VIF como pixels, gravando ROW/MODE falsos e ligando o ADC em todo vértice. A METRO ia de 0% a 94%–99,8%; os monstros com 3 IAs ficavam com 20% a mais de triângulos. Guarda: `VIF1 fora de passo N`.
- **Pista 1, OT de alfa fora da checagem** (f8a1d84): o `hook_particleDraw` zerava os contadores antes do `hierFlushObjQ`. No quadro amostrado, agora dispara o flush antes.

Defeitos da própria checagem:
- **Pista 4, textura** (4769b05): o casamento guloso trocava camadas de slots diferentes. A hipótese do seletor 0 herdando o TEX0 foi descartada com teste.
- **Pista 9, sombra:** a passada da sombra acusava "textura difere". Agora conta à parte (`so pela sombra`).
- **Pista 7, render-to-texture e instante da coleta:** ver "Como funciona".

## Em aberto
6. **Resto das partículas:**
   - **Sprine (8):** a contagem casa (`N = max(2, int(+0x2D8C + 0,5))` passos por trecho), mas os pontos ficam a cerca de 2 px (perturbação aleatória pelos bits do `pos.w`?). A largura `minSize` não foi conferida. Pede ler o microprograma.
   - **Arco elétrico (7, `common/electric`):** subdivisão e zigue-zague feitos no microprograma, com semente `+0x2DC0` e jitter `+0x2E50`. Pede ler o microprograma.
   - **Strite (6) e wake (16):** só o nativo desenha quadrados para eles.
   - **Pedestres na METRO:** numa checagem, VU1 750 e nativo 1000 (um sistema descartado pelo `pedsUpdateFOVbox`?).
   - **Sistemas que só o VU1 tem:** de 2 a 19 quadrados por checagem.
   - **SPRITE do GS fora de `fx[]`:** cerca de 340 por quadro, de origem não identificada (HUD?).
7. **Itens de alfa animados** (tex 330/333, e tex 0 com flags `0xa210`/`0xa310`): às vezes saem de 1 a 6 px fora, em direções variadas. Parece questão de instante da coleta.
8. **METRO, objeto com textura do tamanho da tela:** tex 0, nó `a1c560`, 92 triângulos. O nativo o desenha sem textura; o VU1, com `tbp 1180 psm 1 1024x256` (o framebuffer? reflexo ou vidro).
9. **Sombra sem passada base:** de 0 a 10 triângulos por checagem. Não conferido por quê.
- **Textura trocada entre tex 156 e 193:** 1 triângulo de vez em quando (sobreposição?).

## O que a checagem não cobre
- Texels decodificados: ela compara qual textura, não os pixels.
- UV, cor de vértice, ALPHA/mistura: por isso não pega as texturas animadas nem a explosão opaca.
- Textura e cor das partículas.
- Linhas e leques do VU1 (só contados).
- HUD: vai direto ao GS, fora do VU1.

## Próximos passos
1. Terminar as pistas em aberto (perguntar qual antes de corrigir).
2. Estender a comparação para UV, RGBA de vértice e ALPHA/ABE por triângulo (alcança as texturas animadas e a explosão), e talvez um hash da CLUT por quadro.
3. HUD: tap nos pacotes que vão direto ao GS (PATH2/PATH3) e identificação dos ~340 SPRITE fora de `fx[]`.

## Notas
- O caminho emulado inteiro (sem `PS2X_NO_VU1`) está quebrado: trava no laço do `particleDraw` e depois cai em `GS::consumeLocalToHostBytes`. A checagem não depende dele.
- O `run_live.ps1` (fora do repo) usa `Start-Process -RedirectStandardError`. O `2>` do PowerShell 5 gravava em UTF-16 e quebrava as linhas em 120 colunas.
