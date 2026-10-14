# Checagem VU1 x nativo: estado e próximos passos (2026-10-14)

(Índice em `docs/PROXIMOS_PASSOS.md`. Como a checagem funciona, as armadilhas de método e as medições estão em `NATIVO.md`, seção "Runtime: checagem VU1 x nativo".)

## Onde paramos
- **Implementado:** `WOTM_VU1CHECK=<s>` (runtime, `wotm_vu1check.inc`). A cada `<s>` segundos, num quadro da luta, o VU1 executa a cadeia daquele quadro e cada triângulo do XGKICK é casado com o triângulo nativo do mesmo objeto (`polyPkt`), com tolerância de 1 px.
  - Commit 4066e26 (geometria).
  - **Textura por triângulo:** cada par casado compara o TEX0 em vigor no GS quando o VU1 desenhou com o do slot que o nativo usa. A linha de resumo ganha `textura difere N, sem TEX0 K` e a lista ganha linhas `textura pkt … mais comum: vu (…) nativo slot S (…)`.
  - A linha de resumo também mostra `XGKICK N tris, M fora da tela`: todos os triângulos que o VU1 mandou, antes do filtro de tela.
  - No `tools/lote.ps1` fica ligado por padrão com `-Check 10`. O `resumo.csv` traz `checagens`, `casados_pior`, `objetos_divergentes`, `textura_difere_pior` e `objetos_textura`.
  - `WOTM_VU1CHECK_DUMP=<prefixo>` grava os triângulos de cada checagem num CSV para análise offline. A última coluna é o TEX0 (campos que escolhem os texels; 0 = sem textura).
- **Medido:**
  - Geometria: em central, 1 contra 1, de 98,2% a 99,9% dos triângulos do VU1 casam por quadro, contando os monstros.
  - Textura (central e fase 2, depois da correção do TME): de 0 a 11 triângulos por quadro com textura diferente, entre 7.500 e 14.000 casados. Nenhum triângulo ficou sem TEX0 conhecido.

## Corrigido com a checagem de textura
- **PRIM.TME da seção:** o decodificador nativo ignorava o bit TME do PRIM na tag GIF de cada seção de tira. Seções que o jogo desenha só com a cor de vértice saíam com a textura do slot 0.
  - Exemplo: o objeto tex 75 em central, com 160 a 322 triângulos por quadro (cerca de 1/3 do objeto).
  - Agora a seção ganha o slot `kSlotNoTex` (31; `objectTex0`/`objectTexId` devolvem 0) e o `drawList` a desenha sem textura.
  - Depois disso, a família tex 75/73 sumiu da lista.

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
5. **Checagens com o VU1 sem nenhum objeto na tela:** numa execução na fase 2, do quadro 2529 ao fim, o VU1 mandou programas e partículas, mas nenhum triângulo de objeto inteiro na tela (o nativo desenhava normalmente).
   - Não se repetiu em outra execução de 150 s.
   - O contador `XGKICK … fora da tela` foi adicionado para separar "fora da tela" de "não desenhou" se acontecer de novo.

## O que a checagem não cobre hoje
Ela compara geometria (posição dos triângulos na tela) e textura (TEX0) por objeto. Cor de vértice, mistura e alfa ficam de fora.
- **Textura certa decodificada errada:** não pega. A checagem compara qual textura (endereço, formato e paleta), não os texels que o nativo gerou a partir dela.
- **Partículas e pedestres:** o VU1 desenha, mas esses triângulos não vêm da hierarquia de objetos. Caem no contador "sem objeto", que só dá um total por quadro (programas alimentados pela tabela em 0x5B8Axx). Se o nativo não desenhar pedestres, isso não aparece como diferença.
- **HUD:** muito provavelmente não passa pelo VU1. O jogo manda o texto direto ao GS, como as fontes (`fontPacket`); o resto do HUD não foi conferido. A checagem não vê nada disso.
- **Mistura e alfa** (por exemplo, a explosão opaca): fora do alcance.

Resumo: pega objeto que falta, sobra, está fora do lugar ou está com a textura trocada. Não pega o que o nativo nem tenta desenhar quando o jogo desenha fora da hierarquia de objetos.

## Próximos passos (do mais barato ao mais caro)
1. **Partículas por sistema:** atribuir os triângulos "sem objeto" ao sistema de `fx[]` que os gerou, pela origem dos trechos da cadeia, e comparar contagem e posição com o que `Scene::fx` e os sprites nativos desenham de cada sistema. Pedestres são o tipo visual 14.
2. **HUD pelo caminho direto ao GS:** pôr um tap também nos pacotes GIF que vão direto ao GS (PATH3/PATH2, fora do VU1) e comparar com o que o nativo desenha por cima da cena (texto, ícones). Antes, confirmar por onde o HUD sai.
3. **Cor de vértice e ALPHA por triângulo:** o XGKICK traz o RGBAQ de cada vértice e o registrador ALPHA; daria para estender a comparação de textura para cor e mistura (alcança a explosão opaca).
4. **Investigar as pistas 1 a 5 acima** (pedir ao usuário qual atacar e não corrigir sem pedir).

## Problema antigo encontrado no caminho
O caminho emulado inteiro (sem `PS2X_NO_VU1`) está quebrado. A luta trava no primeiro quadro (laço do `particleDraw`) e, contornado isso, cai em `GS::consumeLocalToHostBytes` (leitura de VRAM do `sceGsExecStoreImage`, chamado a cada quadro de 0x2153FC). A checagem não depende desse caminho. O método para localizar um `[CRASH] exe+0x…` está em `NATIVO.md`.

## Log do runtime
O `run_live.ps1` (na pasta do runtime, fora do repo) passou a usar `Start-Process -RedirectStandardError`. O `2>` do PowerShell 5 gravava o `live.err.log` em UTF-16 e quebrava as linhas do stderr em 120 colunas, e o lote perdia o fim das linhas `[vu1check]`. Quem tiver uma cópia antiga do `run_live.ps1` deve fazer a mesma troca.
