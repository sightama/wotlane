# Checagem VU1 x nativo

Estado, pistas e correções da checagem. Índice em `docs/PROXIMOS_PASSOS.md`; o método (coleta por cadeia, faces de costas, formato da saída) está em `NATIVO.md`, "Runtime: checagem VU1 x nativo". Microcódigo desmontado em `build/vu1/` (`tools/vu1dis.py`; código do jogo, fora do repo).

## Como funciona
- `WOTM_VU1CHECK=<s>` (runtime, `wotm_vu1check.inc`): a cada `<s>` s, num quadro da luta, o VU1 executa a cadeia do quadro. Cada triângulo do XGKICK casa (1 px) com o nativo do mesmo objeto (`polyPkt`); cada quadrado de partícula, com o do seu `fx[i]`. Empate de geometria desempata pela cor.
- Compara geometria, textura (TEX0) e forma das partículas. Nos pares casados, também os atributos (linha `atributos:` e até 8 objetos `atributos pkt`):
  - `uv difere` (> 1 texel; `volta inteira` não conta), `cor difere` (RGB > 4, a cor que o nativo desenha), `alfa difere` (com ABE nos dois), `ABE difere`, `ALPHA difere`, `TFX/TCC fora do padrao`;
  - `neblina N (FGE difere, F difere)`: F do XYZF2 contra `Scene::fogF`.
- Conta à parte: `repetidos` (passadas da sombra projetada), `so pela sombra` (tbp 3e60), `em textura (sombra)` (FRAME fora dos buffers de tela 0x46/0x8C), `costas pelo recorte`.
- `tools/lote.ps1` liga com `-Check 1`. `resumo.csv`: `casados_pior`, `textura_difere_pior`, `fx_casados_pior`, `vif1_fora_de_passo` (deve ser 0), `uv/cor/alfa/abe/alpha_difere_pior`.
- `WOTM_VU1CHECK_DUMP=<prefixo>` grava cada checagem em `<prefixo>_<g_frame>.csv`: triângulos (posição, TEX0, FBP, UV e RGBA por vértice, ABE, ALPHA, FGE, TEX0 inteiro), `hdr` (por programa: endereço do MSCAL, `ffffffff` = MSCNT, e qwords 5–8 e 17–20 do VU1), `luzes` (`paraL`, `D_0025B238`, `specularColor`), partículas (`fxcam`, `fxp`, `fxmat`) e origens dos SPRITE fora de `fx[]` (`rectorf`).
- Diagnóstico da neblina: `WOTM_FOGSPLIT=1` (metade esquerda sem), `=2` (máscara; imprime os parâmetros), `=3` (alterna por quadro).

## Estado (2026-10-15)
- Geometria: fase 1 de 99,2% a 99,7% no pior caso; fase 8, 98,7% a 99,8%; METRO, 96,2% a 99,9%. Textura: 0 a 1 triângulo diferente por checagem.
- Atributos (fase 1, 40 s): pior `cor difere` 366 por checagem (era 2.832); monstros com 95% a 98% dos vértices iguais; `uv difere` 184 a 240; neblina com 0 a 4 F diferentes em ~10 mil.

## Em aberto
Não corrigir sem perguntar ao dono qual atacar.
- **Arco elétrico (tipo 7, Robo-47):** especificado em "Microprogramas"; o VU1 desenha 20 quadrados por sistema e o nativo 6 (trechos retos). Falta implementar.
- **Sprine (tipo 8):** especificado; falta implementar e conferir no dump o j duplo do 1º passo, o UV com r_{k+1} e a deriva com N ímpar.
- **Neblina nas partículas:** o sprine e o arco calculam F como os objetos (ver "Microprogramas"). Inclui o descarte com F < 1, que explica os pedestres da METRO (abaixo).
- **Texturas compostas de poeira/fumaça:** os 340–380 SPRITE fora de `fx[]` por quadro vêm do `dustPacket` (`D_0078F6E0`, `texDrawDustPacketBuild`) e compõem na VRAM (0xFD800) as texturas das partículas com texInfo 0xFFFF; o nativo usa um círculo suave. Falta gerá-las (`dustPktParams`, `dustLighting`) e a checagem contá-las como render-to-texture.
- **Lens flare (tipo 11):** fora do `particleDraw` (`particleAddLensFlareToList`, `particleLensFlareTestZbuffer`, `particleLensFlareRender`). Recurso a implementar.
- **13. UV do mapa de ambiente** (`envMapped` 0x2, ex.: tex 86, 38–55 texels): o programa 0x0A88 gera o ST pela normal girada por uma matriz da câmera (qwords 997–999 e 0–2) mais a posição na tela. Pede ler o programa; o CLOAK usa o mesmo caminho (`envMapIsVideo`).
- **16 (resto):** no Togera, ~5% dos vértices com um cinza a mais no nativo (média 8; provável especular). O fator de contexto do q5 (+0x160 do bloco do hier) não é reproduzido.
- **10.** Textura trocada entre tex 156 e 193, um triângulo às vezes (ambiguidade, como a 14).
- **Não coberto:** texels/CLUT e alfa da textura (a escala `4a/255` do alfa de vértice no nativo contra `a/128` do GS não foi conferida), atributos das partículas (a explosão opaca), linhas e leques do VU1, HUD (PATH2/PATH3).

## Microprogramas das partículas
Lidos em `build/vu1/particle.txt`; o sprine foi conferido com o pacote num dump (`build/dumps_live/ram_B1.bin`, fx 82). Contexto comum (qwords 0–7, de 0x256B90): q0–q3 = `worldToScreenMat` transposta; q4 = ambiente×0,3, `fogFA` em w; q5 = metade do foco (192; 90,24), `fogFB` em w; q6 = (−2048, −2048, −2049, 2048); q7 = (4095, 4095, 16777215, 255). CLIP: x e y em [0, 4096], W ≥ 1, e some se F < 1.

Gerador do VU: `RINIT` faz R = 0x3F800000 | (bits(fs) & 0x7FFFFF); `RNEXT` faz R = (((R<<1) ^ bit4(R) ^ bit22(R)) & 0x7FFFFF) | 0x3F800000 (float em [1, 2)).

**Sprine (tipo 8, entrada 0x2770):**
- Memória: `pos[]` no qword 8; 32 cores em 264–295; UV em 296–299; tag em 300 (pa = `preAgeFactor` no x); parâmetros em 301–304; M2 = linha 2 da `ParticleToEmitMat` (fx+0x2CD0) em 305; 306 = ((n−1)·N, age[n−1]·N, n−1).
- N = max(2, int(rate@0x2D8C + 0,5)), h = 1/N. q301 = (h·(numColors−1)/vida, h·expansionRate, —, f6), f6 = (max−min)/min se min > 0, senão 1. q302 = (minSize, N, (N+1)>>1, sprites por lote). q303 = h²·(0,75−0,5N; −1,75; 0,5N+1,25; −0,25); q304 = h²·(1,5; −3,5; 2,5; −0,5).
- Curva de `pos[n−1]` a `pos[0]`, um trecho por par A = pos[i], B = pos[i−1] (Pm antes, Pn depois): duas parábolas com C1 no meio. Fantasmas: Pm = B − M2·((B−A)·M2) no início, Pn = 2·pos[0] − pos[1] no fim.
- Diferenças avançadas (bit a bit): D1 = q303·(Pm,A,B,Pn), D2 = q304·(…), D2′ = q304 invertido (w,z,y,x)·(…). p = A; por passo k: emite em p, p += D1, D1 += D2; quando N−1−k == (N+1)>>1, D1 = D1 − c·D2 + c·D2′ e D2 = D2′ (c = 0,5 com N par, 0,125 ímpar; a deriva com N ímpar é do retail). Cada trecho recomeça em pos[i]; pos[0] não é emitido: (n−1)·N sprites.
- Por passo global s: a = age[n−1] + s/N; colorPos = min(31, (numColors−1)·a/vida·j), j = 1 + pa·(2 − r); cor = lerp da tabela, +0,5, trunc (no 1º passo de trecho que não começa lote, j entra duas vezes). Lado = (minSize + expansionRate·a)·(1 + (r−1)·f6), quadrado alinhado à tela. F = min(fogFA + fogFB/W, 255) no 2º vértice.
- R: RINIT(pos[n−1].w) e um RNEXT no começo; RINIT(pos[i].w) por trecho; um RNEXT por passo. O UV do passo k usa r_{k+1} (sel = bits & 3: 0 espelha U e V, 1 U, 2 V, 3 normal; du′ = du − 8, dv′ = dv − 8); j e lado usam r_k.
- GIF: PRIM 0x176 (SPRITE, TME, FGE, ABE, FST), REGS = RGBAQ, UV, XYZF2, UV, XYZF2; buffers 300 e 662.

**Arco elétrico (tipos 7 e 17, entrada 0x19F8; `electricArcPacketInit/Build/Update` em `asm/common/partdraw.s`):**
- fx+0x2DC0 = semente, fx+0x2D9C = `normalScale` (amplitude), fx+0x2E50 = `segmentInitU` (fase da textura em U).
- Memória: `pos[]` em cadeias no qword 8 (cabeça: w = 1 + nº de elos; elo: w = 5<<(nível−1) ou 0; 0 na cabeça encerra); 16 cores (float 0..255) em 138–153; tag em 154 (PRIM 0x7C: TRISTRIP, IIP, TME, FGE, ABE, STQ); 155 = (minSize, seed, segInitU, 138 + CLUToffset); 156 = (expansionRate, jag, segDeltaU, numColors−1). Exige n ≥ 3.
- RINIT(seed) por pacote. Elo de a a b com nível L > 0 vira 2^L sub-trechos: no nível l, amp = jag·L/2^(l−1) e cada vão ganha o ponto médio (0,5a + r·amp − 1,5·amp) + 0,5b (cubo de lado amp em xyz do mundo, r de uma janela deslizante de RNEXT).
- Fita: fantasmas P₋₁ = 2P₀ − P₁, Pₙ = 2Pₙ₋₁ − Pₙ₋₂; normais na tela de t_in e t_out somadas (mitra aproximada, usa ERSQRT); vértices S ± v·(fxh·s_k/W, fyh·s_k/W), s_k = minSize + exp·k/S; ST = (U_k/W, 1/W ou 0, 1/W), U_k = segInitU + segDeltaU·(k − S/2); cor por lerp; neblina como no sprine, exceto nos pontos 0 e 1. ADC no triângulo fora da banda, com W < 1 ou F < 1. O recorte no plano próximo é inferido: o nativo pode descartar W < 1.

## Explicadas
- **Pedestres da METRO** (fx 91–99; o nativo desenha os que o VU1 descarta): é a neblina, não o instante. O VU1 descarta a partícula com F < 1. Na METRO, `vu1FogParams` = (−107,9; 255; 1; 326.600) (`build/dumps_live/ram_energy_robot.bin`), então F = −107,9 + 326.600/W < 1 com W > ~3.000. Nos dumps `f4_477/519/560` (2+2) os sistemas perdidos têm W ≥ 3.138 e os desenhados W ≤ 2.953 (a lista do `particleDraw` vai do mais longe ao mais perto, por isso faltam sempre os primeiros, fx 91 e 92). Na fase 1 o corte fica em ~16 mil. Bug do nativo: falta a neblina nas partículas (em aberto). As perdas totais (0/1000) só aparecem nos lotes anteriores à correção do `sceGsSetDefLoadImage` (#311), com o "em textura (sombra)" saltando de ~570 para ~5.900: o FRAME vinha errado, e a checagem descartava as partículas como render-to-texture.

## Explicadas, sem bug no nativo
- **7 (padrão B):** os itens de alfa presos aos monstros às vezes saem 1–2,5 px fora: o pacote leva as matrizes do instante da travessia, e nenhuma coleta cai ali. A checagem tenta também a coleta no `hier(0, 1)`.
- **9, `so pela sombra`:** a passada da sombra projetada redesenha faces de costas que a base descarta, e triângulos finos geram ambiguidade.
- **14 (tex 242/243):** prédio distante de triângulos minúsculos; com 1 px, quase todo triângulo do VU1 tem 4+ nativos no mesmo lugar. 94% a 99,6% têm um nativo igual no lugar; o resto é ambiguidade.

## Corrigido
Nativo (`native/wotm_native.hpp`, `wotm_fx.hpp`). Flags do OBJECT em +0xC (`_hierobject`, `include/hieri_types.h`); o `hierTraverseAsm` (asm do `texm`) monta o cabeçalho de cada OBJECT para o VU1.
- **12, luz paralela** (`dabeefd`): OBJECT com `dirLight` (0x100). Normal V3-8 @0x175 (índice da posição; nos skinned, girada só pelo osso x). `d = max(0, [q6 q7 q8]·n)` (direções de `paraL` 0x6DFC00, ×32, no espaço do objeto; a lane w é o vetor-meio do especular, elevada à 16ª); `cor = min(255, vcol × q5 × Σ q17..19·d + q20·d.w¹⁶)`, q17–19 = `vu1LightColor` (`D_0025B238`, cor/128), q20 = `specIntensity` (+0x6C) com `specLightMe` (0x2000). `Scene::objectLight`/`vertexRgb`. Capturas: `docs/screenshots/2026-10-15_monstros_sem_luz.png` e `_iluminados.png`.
- **16, multiplicador de cor q5/`vf31`** (`e4aef4e`): `redAnim`…`alphaAnim` (+0x40) ou canais `rAnimIdx`… (+0x34) com instância de animação, vezes o `colorQuad` do CS (+0x70); o alfa também vezes o fade do nó. `DrawItem::color`.
- **13, UV animado** (`5c883d7`): q4/`vf19` = (`uOffset`, `vOffset`, `zBufferFudge`, `fogBurnThru`) (+0x20); com `uvScroll` (0x800) e instância de animação, u e v vêm de `val[uOffIdx/vOffIdx]` (+0x30). Somado ao UV normalizado (exceto no programa 0x0040). `DrawItem::uvOff`.
- **15, neblina** (`8ba5185`): nas seções com `PRIM.FGE`, F = clamp(`fogBurnThru` + `fogFA` + `fogFB`/W, 0, `fogMaxVal`), com `vu1FogParams` (`D_0025B280`, de `fogUpdate`) e W = linha 3 do `worldToScreenMat`·(p − olho); GS: (F·cor + (255−F)·FOGCOL)/255, FOGCOL de `fogColorCtx[4]` (0x4455A0). Vertex shader no `Scene::draw` (burn e FGE na normal do vértice). Fase 1: começa a ~1.200 unidades, total a ~17 mil, cor (37, 20, 13). Capturas: `docs/screenshots/2026-10-15_neblina_*.png`.
- **11, culling por seção:** o bit 15 do seletor da seção liga o culling (ADC nas faces de área positiva na tela); o caminho de recorte (0x1b38) não descarta. `Vtx::cull`, `Scene::backFacing`.
- **8, texId 0 = quadro anterior:** TEX0 `displayBuffer` (0x6F87F8) `| 0x28128000`, TH 8; o objeto é o power-up CLOAK. `kScreenKey` e duas render textures.
- **3, `_cs` não ortonormal** (`8020844`): o VU1 desenha em `olho + (trans − olho)·RᵀR`.
- **7, itens** (`4f249df`): ANIM_SCALE com `scale·(p + preScale) + translate + postScale`; cross-fade de LOD do `hierLod` (`DrawItem::fade`).
- **6, partículas:** `fx[100]` inteiro; `viewInFOV` aplicado; o `pos.w` não é tamanho: pedestres (14) carimbos `minSize × maxSize` em pé; destroços (13) lado `max(minS, 2·minS·|w|)`; stripe (5) meia-largura `|pos.w| − |pos.w[n−1]| + minSize`, perpendicular na tela; strite (6) com a matriz do quadro anterior (`prevViewMat`, fx+0x2EC0), ponta `3T − 2H`, base `3H − 2T`, meia-largura 0,433·pos.w; wake (16) triângulo equilátero no XY, raio `minSize/2 + 120w + 7,5s(2−s)`; dustTrail (15) meio-lado `S/2 + exp·vida·w/2`. Sprite/texSprite: meio-lado `pos.w/2·foco/profundidade`.
- `PRIM.TME` 0 usa `kSlotNoTex`; o cache de malhas dos skinned revalida com a mesma assinatura do `decodePacket`.

Runtime (clone; os genéricos estão nos PRs do PS2Recomp #310, #311 e #312):
- **17, opcodes EFU** (#312): ERSQRT = 0x79, ESIN = 0x7C, EATAN = 0x7D, EEXP = 0x7E (0x77 inválido), como as tabelas `LowerOP_T3_xx` do PCSX2. Corrigido também no `tools/vu1dis.py`.
- **`VSQI`/`VSQD`… do tradutor VU0** (#310): campos trocados e acesso à RDRAM em vez da memória do VU0; o FOV da sombra passava tudo e o `psObjDma` estourava (VIF1 fora de passo na fase 8).
- **`sceGsSetDefLoadImage`/`StoreImage`** (#311): gravavam struct própria em vez do pacote do SDK; o GIF engolia os DIRECT seguintes (inclusive o FRAME da sombra) e o BP era ×8.
- **2/5, VIF1 fora de passo** (`dfd525a`): DIRECT com imagem GIF maior que ele lia códigos VIF como pixels.
- **1 e 7 (padrão A), OT de alfa:** o `hook_particleDraw` e a apresentação (`wotmDrawScene`) zeravam `objsInPacket`/`objsInAlphaPacket` antes do `hierFlushObjQ`; no quadro amostrado, não zeram mais.
- Checagem: textura por rodadas (4), sombra à parte (9), render-to-texture e instante da coleta (7).

## Notas
- O `ps2EntryRunner` é unity build: mudar só `src/main.cpp` não recompila; toque `build_dev/ps2xRuntime/CMakeFiles/ps2EntryRunner.dir/Unity/unity_168_cxx.cxx`.
- O caminho emulado inteiro (sem `PS2X_NO_VU1`) trava no `particleDraw`; a checagem não depende dele.
- O `run_live.ps1` usa `Start-Process -RedirectStandardError` (o `2>` do PowerShell 5 grava UTF-16 e quebra linhas).
