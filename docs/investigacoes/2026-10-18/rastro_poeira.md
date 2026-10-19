# Rastro de poeira mais longo e largo no replay (bug 4, VEGAS, `g_frame` 2706)

Foto `C:\Users\TwistZero\wotm-recomp-win\fotos\2026-10-18_025733\foto_001.vu1snap`. Rodei `tools\vu1cmp.ps1 -Full -NoPng -Dump`
(as PNGs da foto não foram regravadas) e `WOTM_VU1CMP_RAM` (RAM do quadro, já apagada). Nada do repo foi alterado além deste arquivo.

## Conclusão

1. **A faixa clara no céu é o fx 16, não o fx 52.** Coordenadas do GS → tela (x − 1728, (y − 1936)·2): o fx 16 (preset 106, 8
   quadrados) ocupa x 225–350, y 18–234, exatamente a diagonal do diff. O fx 52 fica no canto inferior esquerdo (x 2–134, y 236–368,
   em cima da cowgirl), e o fx 13 é o feixe tênue da esquerda. **A geometria do fx 16 casa: 8 de 8 quadrados a até 1 px.** Comprimento e
   largura são os mesmos nos dois lados.
2. **"Mais longo e largo" é efeito da cor.** O stripe tem `info.attribs` (fx+0x2E30) = 0x600, com o bit **0x200 = `useThisColor`**:
   o `stripePacketInit` (asm/common/partdraw.s:2185, laço em 0x2127E0–0x212844) não pega o RGB da tabela. Ele lê o vetor apontado por
   `info.useThisColor` (fx+0x2E58, `ParticleInfoType` +0x28 em include/hieri_types.h:1112), que é `trailColoring` (0x5B4F60), e grava
   RGB = vetor.xyz e A = tabela.a × vetor.w. Na RAM do quadro: fx 13/16/33/52/68 → 0x5B4F60 = (60, 50, 50, 1,0) = `3c3232`, e a
   tabela (ci 736) = `70 67 57` com alfa 80→01. Isso bate com a checagem: VU `3c3232`, nativo `706757`, alfa igual. O `trailColoring`
   é por fase: chave 0xBC3 do reset (src/game/reset.cpp:43-45), e o `fragColoring` (0xBC4) faz o mesmo papel nos destroços. O nativo
   já respeita o 0x200 no frag (native/wotm_native.hpp:1558), mas não nas partículas: `Particles::colorAt` (native/wotm_fx.hpp:781) e
   o uso em native/wotm_fx.hpp:1444/1461/1522 leem só a tabela.
   - A mistura é a comum (ALPHA 0x44: (Cs − Cd)·As + Cd). Com (60, 50, 50) o rastro fica perto do céu escuro e as bordas suaves da
     textura somem; com (112, 103, 87) elas aparecem, e a faixa parece mais larga e mais comprida. Conferi nas PNGs (script `blend.py`): ajustei
     As e o texel pelo pixel nativo no meio do feixe e só troquei a cor pela do VU. O ref previsto sai (57, 29, 39) contra (52–55,
     22–26, 33–37) medidos, em 11 linhas de y 55 a 205. Resíduo de ~5 níveis (ver o item 4).
3. **O fx 52 com 0 de 40 casados é artefato da checagem, não diferença de forma.** Os vértices do VU e do nativo são os mesmos (a
   ~0,1 px), mas o VU tem 81 triângulos (um a mais, a meia ponte na ponta esquerda, junto da borda da tela: vértice extra
   (1743,7; 2120,1), com U de uma partícula além da última que o nativo desenha). O `vuQuads` (native/runtime/wotm_vu1check.inc:868)
   pareia triângulos consecutivos de forma gulosa. Com o triângulo a mais no começo, todos os pares saem defasados de um. Cada
   "quadrado" do VU junta a borda de uma partícula com a da vizinha, e nenhum casa (fica um solto na outra ponta, "soltos 1"). O fx 68
   (1 x 1, soltos 1) é o mesmo caso. A única diferença real é esse triângulo de ~14×7 px na borda: o VU recorta por vértice (ADC) e o
   nativo descarta a ponte inteira.
4. **Diferença secundária, o U da textura.** No VU, o U do stripe é por segmento e rola com o tempo: o init usa `segmentInitU`/
   `segmentDeltaU` (fx+0x2E50/0x2E54), o contador de quadros `& 0xF` / 16 e `newParticles` (fx+0x2EDE); o build desconta
   `segmentDeltaU/rate` por partícula nova e dá a volta em +1. Fica U ≈ −0,455…−0,58 no fx 16, com passo 1/64 por partícula. O nativo usa
   U = i/(n − 1) (native/wotm_fx.hpp:1194, 1262-1263). O V (0/1 atravessando a faixa) é igual. Isso muda que texels aparecem ao longo
   do rastro (o resíduo de ~5 níveis), não a forma.

## Confiança

- Cor (0x200/`trailColoring`): **alta**. Os valores da RAM batem exatamente com a cor do VU, e o código do `stripePacketInit` é direto.
- "Mais longo/largo" = cor, não geometria: **alta**. O fx 16 casa 8/8 e a previsão por pixel trocando só a cor fica a ~5 níveis da
  referência.
- fx 52 = artefato de pareamento: **alta** (mesmos vértices; paridade conferida no dump `fx52,vut`).

## Próximo passo concreto

1. No nativo, ao montar a cor das partículas (o `colorAt` ou quem o chama, nas três chamadas citadas): com `info.attribs & 0x200`
   e `useThisColor` válido, usar RGB = clamp(vetor.xyz) e A = tabela.a × vetor.w. É o mesmo que o frag já faz em
   wotm_native.hpp:1558, mas lá o alfa vem fixo de vetor.w·128; no stripe ele vem da tabela pela idade, vezes w. Antes, conferir que os
   outros `*PacketInit` (sprite, strite, sweepTrail…) tratam o 0x200 do mesmo jeito; o arco, que sai em ~0,8× no `luta_centro`, pode
   ser outro caso de cor vinda de fora da tabela. Depois, `corpus.ps1 -Check` (`congar_olhos`: `3c32327d` contra `70675780` deve
   zerar) e esta foto, onde a linha `particulas cor` do stripe deve ir a 0.
2. Na checagem: parear os triângulos do VU por vértices compartilhados com a vizinhança (ou tentar as duas paridades e ficar com a
   melhor) para o stripe/sweepTrail parar de dar 0% quando a tira começa com um triângulo solto.
3. Opcional: U do stripe como o microprograma (segmentInitU/DeltaU, rolagem por quadro). Antes, ler a parte de U do 0x1708 e da
   rotina 0x1F70/0x2118 em `build/vu1/particle.txt`.

Scripts e dump (fora do repo): `C:\Users\TwistZero\.claude\jobs\ab2fc446\tmp\ag4\` (`d_2706.csv`, `bbox.py`, `cmp52.py`,
`fxram.py`, `blend.py`).

## Reaberto: "o rastro não aparece mais" (2026-10-18 noite)

Fotos `2026-10-18_225102`/001–004 (VEGAS, `g_frame` 471, 504, 704, 1179) e o PCSX2 pausado com rastros na tela (outra fase).

- Replay das fotos: referência e nativo iguais (stripe 62/62 e 70/70 casados, 0 de cor e alfa). O rastro é desenhado, cinza
  avermelhado e curto (foto 002: fx 51, 54, 74 e 77 com 39–40 pontos, ~60 px numa tela de 640). Os outros 8 stripes da RAM não
  entram na lista do `particleDraw` naquele quadro (estão fora da vista); a referência faz o mesmo.
- RAM do PCSX2 (`tools/pcsx2_auto/ramdump.py`) contra a RAM do instantâneo (achada pela âncora do ELF em 0x100100): os mesmos
  campos (attribs 0x600, `useThisColor` = (60, 50, 50, 1), tabela 736 com `706757`, vida 180, máx. 256, `alphaContext` 0x44,
  mesma TEX0). Os pontos crescem um a cada 2 quadros nos dois; no PCSX2 havia 67–68 porque a pausa veio mais tarde (idade 134
  contra 75). A simulação bate.
- Pixels do rastro no print do PCSX2: ~(50, 42, 40) sobre fundo ~(10–25), o tom do 3c3232 modulado e misturado a ~0,5. No
  PCSX2 o rastro aparece porque o fundo é mais escuro; contra o céu da VEGAS (~(60, 40, 45)) ele quase some.
- Conclusão provável: igual ao retail. Antes do 308f2b7 o nativo usava a cor clara da tabela (`706757`), e o rastro chamava
  atenção. Falta conferir no PCSX2 na VEGAS. Se o dono quiser o visual antigo, o caminho é um mod (ignorar o bit 0x200), não
  uma correção.
- Imagens: `docs/screenshots/2026-10-18/2026-10-18_rastro_poeira_pcsx2.png` (recorte do PCSX2) e
  `2026-10-18_rastro_poeira_ref_x_nativo.png` (foto 002, referência à esquerda, nativo à direita, 3x).
