# Checagem VU1 x nativo: estado e próximos passos (2026-10-14)

(Índice em `docs/PROXIMOS_PASSOS.md`. Como a checagem funciona, as armadilhas de método e as medições estão em `NATIVO.md`, seção "Runtime: checagem VU1 x nativo".)

## Onde paramos
- **Implementado:** `WOTM_VU1CHECK=<s>` (runtime, `wotm_vu1check.inc`). A cada `<s>` segundos, num quadro da luta, o VU1 executa a cadeia daquele quadro e cada triângulo do XGKICK é casado com o triângulo nativo do mesmo objeto (`polyPkt`), com tolerância de 1 px.
  - Commit 4066e26 (geometria).
  - **Textura por triângulo** (08a4df2): cada par casado compara o TEX0 em vigor no GS quando o VU1 desenhou com o do slot que o nativo usa. A linha de resumo ganha `textura difere N, sem TEX0 K` e a lista ganha linhas `textura pkt … mais comum: vu (…) nativo slot S (…)`.
  - **Partículas por sistema:** os triângulos e os SPRITE do GS que vêm do `pkt` de um `fx[i]` vão para esse sistema e são comparados, quadrado a quadrado, com os que o nativo monta para ele. Saem as linhas `particulas:` (totais), `particulas por tipo (casados/vu/nativo)` e os sistemas com mais diferença.
  - A linha de resumo mostra `XGKICK N tris, M fora da tela` (todos os triângulos do VU1, antes do filtro de tela) e separa `particulas N tris` de `sem objeto`.
  - No `tools/lote.ps1` fica ligado por padrão com `-Check 10`. O `resumo.csv` traz `checagens`, `casados_pior`, `objetos_divergentes`, `textura_difere_pior`, `objetos_textura`, `fx_casados_pior` (pior % de quadrados do VU1 casados num quadro com partículas) e `fx_tipos_divergentes` (tipos de sistema listados com diferença).
  - `WOTM_VU1CHECK_DUMP=<prefixo>` grava os triângulos de cada checagem num CSV para análise offline. A última coluna dos triângulos é o TEX0 (0 = sem textura). As partículas saem como `fx<i>,vuq|natq,<n>,x0,y0,…` e `fxsys,<i>,<cadeia>,<preset>,<tipo>,<n>,<câmera>`.
- **Medido:**
  - Geometria: em central, 1 contra 1, de 98,2% a 99,9% dos triângulos do VU1 casam por quadro, contando os monstros.
  - Textura (central e fase 2, depois da correção do TME): de 0 a 11 triângulos por quadro com textura diferente, entre 7.500 e 14.000 casados. Nenhum triângulo ficou sem TEX0 conhecido.
  - Partículas (fases 1, 2, 4, 5, 7 e 8, 1 contra 1, 60 a 90 s): o número de quadrados por sistema bate quase sempre entre VU1 e nativo. A forma só bate em sprite e texSprite (abaixo).

## Corrigido com a checagem
- **PRIM.TME da seção** (textura): o decodificador nativo ignorava o bit TME do PRIM na tag GIF de cada seção de tira. Seções que o jogo desenha só com a cor de vértice saíam com a textura do slot 0.
  - Exemplo: o objeto tex 75 em central, com 160 a 322 triângulos por quadro (cerca de 1/3 do objeto).
  - Agora a seção ganha o slot `kSlotNoTex` (31; `objectTex0`/`objectTexId` devolvem 0) e o `drawList` a desenha sem textura.
- **Tamanho de sprite e texSprite** (partículas): o nativo desenhava cada partícula com o dobro do tamanho. O meio-lado na tela é `pos.w / 2 * foco / profundidade`, não `pos.w * …` (a razão VU1/nativo era 0,50 em x e em y, com o centro no lugar, em todos os sistemas desses dois tipos).
  - Depois da correção, os sprites casam inteiros (12/12, 22/22 quadrados). A explosão (texSprite, preset 9) casa pela metade (10 de 19), com o sistema deslocado de 1 a 3 px.
  - A geometria das partículas saiu do `drawSystem` para `Particles::forEachQuad` (e `Scene::spriteCorners` nos destroços), que servem ao desenho e à checagem.

## Pistas encontradas (nenhuma confirmada como bug; nada corrigido)
1. **Objetos que o nativo desenha e o jogo não** (`vu 0` na saída): estão inteiros na tela e de frente.
   - Exemplos em central: tex 276 (até 545 triângulos), 234, 247 e 307.
   - Suspeita: escolhas de visibilidade (CONTROL/prédio fantasma, DESTRUCTIBLE, LOD). É a pista mais ligada aos bugs já reportados.
2. **Monstros com 20% a 28% a mais de triângulos no VU1, só com 3 IAs** (`WOTM_MATCH=1,3,2,2,1`).
   - Os vértices são os mesmos (0,04 px), mas ligados de outro jeito, e a diferença cresce no fim de cada sub-pacote.
   - No 1 contra 1 os monstros casam inteiros.
   - Causa possível: a regra de ADC do nativo ou dado velho na memória do VU1 emulado. Precisa de referência do PCSX2 para decidir.
3. **Objetos com tex 66 e várias instâncias** às vezes não casam.
4. **Resto da textura (1 a 11 triângulos por quadro):** o VU1 usa a textura de outro objeto ou o slot vizinho do mesmo objeto.
   - Exemplo: `tbp 27a5 128x128 cbp 1bd2` aparece no VU1 em objetos sem relação entre si (tex 110, 155, 225, 236). Também aparecem pares trocados de slot (tbp 23a9 e 23b1 nos tex 231 e 233).
   - Hipótese: seção com seletor 0 herda o TEX0 que o GS tinha, que pode ser do objeto desenhado antes. O nativo herda só dentro do próprio objeto e começa no slot 0.
   - Não conferido. Seria um detalhe do jogo, não necessariamente algo a imitar.
5. **Checagens com o VU1 sem nenhum objeto:** de um certo quadro em diante, todas as checagens dão `objetos vu 0` e `XGKICK 0 tris` (ou só partículas), enquanto o nativo desenha normalmente.
   - Visto uma vez na fase 2 (do quadro 2529 ao fim) e depois em 2 de 6 execuções de 60 s (central desde o quadro 757; fase 4 desde o 143). Outras execuções na mesma fase não tiveram.
   - O lado do VU1 não depende da coleta nativa, então não deve vir das mudanças da checagem, mas isso não foi provado. `casados_pior` 0 no `resumo.csv` marca a execução.
6. **Formas de partícula que o nativo não imita** (por tipo; a contagem de quadrados por sistema bate):
   - **Pedestres (14):** o VU1 desenha cada um como um quadradinho inclinado (85 ou 100 por sistema, de 2 a 3 px). O nativo desenha sprites voltados para a câmera com o tamanho de `pos.w`: cerca de 12% saem com tamanho zero, e os outros têm em mediana metade do tamanho do VU1. O tamanho e a orientação devem vir de outro campo.
   - **Tipo 7 (arcos?):** o VU1 faz de 2 a 3 quadrados por partícula; o nativo, um de tamanho zero.
   - **Destroços (13):** o VU1 manda retângulos alinhados à tela (SPRITE do GS, sem giro), com cerca de 0,6 do tamanho que o nativo sorteia. O nativo gira e sorteia o tamanho por partícula.
   - **dustTrail (15):** o VU1 desenha retângulos enormes (meio-lado de 120 a 145 px); o nativo, pontos (razão de cerca de 200 a 300).
   - **sprine (8):** o VU1 subdivide a faixa (cerca de 8 quadrados por trecho entre partículas); o nativo liga as partículas com um quadrado por trecho.
   - **stripe (5):** mesma contagem; largura de 0,8 a 1,2 vez a do nativo e deslocamento de até 2 px.
   - **Sistemas que só o VU1 tem** (tipo `0 -` na linha por tipo): de 2 a 19 quadrados por checagem, de sistemas que o `Particles::collect` não aceita.
   - **SPRITE do GS fora de `fx[]`:** de 320 a 350 retângulos por quadro, quase constantes, de origens fora dos sistemas de partícula. Não identificados (HUD ou ícones?).

## O que a checagem não cobre hoje
Ela compara a geometria dos objetos (triângulos na tela) e das partículas (quadrados na tela) e a textura dos objetos (TEX0). Cor de vértice, mistura e alfa ficam de fora.
- **Textura certa decodificada errada:** não pega. A checagem compara qual textura (endereço, formato e paleta), não os texels que o nativo gerou a partir dela.
- **Textura e cor das partículas:** não compara (só a forma).
- **Linhas e leques do VU1:** só conta os vértices (`line` das partículas sai assim).
- **HUD:** muito provavelmente não passa pelo VU1. O jogo manda o texto direto ao GS, como as fontes (`fontPacket`); o resto do HUD não foi conferido. A checagem não vê nada disso.
- **Mistura e alfa** (por exemplo, a explosão opaca): fora do alcance.

Resumo: pega objeto ou partícula que falta, sobra, está fora do lugar ou com o tamanho errado, e objeto com a textura trocada.

## Próximos passos (do mais barato ao mais caro)
1. **HUD pelo caminho direto ao GS:** pôr um tap também nos pacotes GIF que vão direto ao GS (PATH3/PATH2, fora do VU1) e comparar com o que o nativo desenha por cima da cena (texto, ícones). Antes, confirmar por onde o HUD sai e de onde vêm os ~340 SPRITE do GS fora de `fx[]`.
2. **Cor de vértice e ALPHA por triângulo:** o XGKICK traz o RGBAQ de cada vértice e o registrador ALPHA; daria para estender a comparação de textura para cor e mistura (alcança a explosão opaca) e para as partículas.
3. **Investigar as pistas 1 a 6 acima** (pedir ao usuário qual atacar e não corrigir sem pedir). Na pista 6, os pedestres são os mais visíveis: são centenas por quadro.

## Problema antigo encontrado no caminho
O caminho emulado inteiro (sem `PS2X_NO_VU1`) está quebrado. A luta trava no primeiro quadro (laço do `particleDraw`) e, contornado isso, cai em `GS::consumeLocalToHostBytes` (leitura de VRAM do `sceGsExecStoreImage`, chamado a cada quadro de 0x2153FC). A checagem não depende desse caminho. O método para localizar um `[CRASH] exe+0x…` está em `NATIVO.md`.

## Log do runtime
O `run_live.ps1` (na pasta do runtime, fora do repo) passou a usar `Start-Process -RedirectStandardError`. O `2>` do PowerShell 5 gravava o `live.err.log` em UTF-16 e quebrava as linhas do stderr em 120 colunas, e o lote perdia o fim das linhas `[vu1check]`. Quem tiver uma cópia antiga do `run_live.ps1` deve fazer a mesma troca.
