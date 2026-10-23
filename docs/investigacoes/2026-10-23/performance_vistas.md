# Custo da tela dividida de 3 e 4 jogadores: investigação

2026-10-23. Só leitura (código, docs e medidas já registradas); nada foi compilado, aberto nem medido de novo. Pergunta: como baratear
a tela dividida de 3 e 4 jogadores no port, onde cada vista a mais custa caro nas duas threads (`POS_FRONTEIRA.md`, item 1, "Custo
por vista", e item 11, relato de 2026-10-22). O documento é para quem for pegar o trabalho: onde está cada coisa, o que medir antes e
depois, e uma ordem de ataque com ganho e risco. Legenda: **[C]** confirmado (código, ou medida já registrada nos docs), **[H]**
hipótese a conferir com a medida indicada.

**Resposta curta:** são dois gargalos que se somam, e o mais lento manda.
- **Thread da janela (desenho nativo):** quase todo o custo por vista é CPU gerando vértice por vértice no modo imediato do raylib,
  com um descarregamento do lote (`rlDrawRenderBatchActive`) a cada troca de textura ou de estado. Nada do que é igual entre as
  vistas é aproveitado: a fase é transformada e iluminada de novo em cada vista e em cada quadro, os monstros são deformados pelos
  ossos de novo em cada vista, e **a textura de cada sombra é rasterizada na CPU de novo em cada vista**. O corte por vista (frustum)
  já existe. O maior ganho está em trocar o modo imediato por um buffer próprio com lista de comandos (sem mudar a ordem) e, em cima
  dele, guardar a geometria estática pronta entre quadros (depois, na GPU). O ganho mais barato e mais seguro está nas sombras.
- **Thread do EE (jogo recompilado):** o `hier` de cada vista é código do jogo; a parte que só serve ao VU1 (montar os pacotes) é
  pequena (~20% do `hier` pela medida do `FRONTEIRA.md`) e cara de cortar. Antes de mexer, vale medir o build com LTCG e perfilar o
  `hier` por vista. Na CPU do colaborador (Ryzen 3 3200G) só o lado nativo não basta para a lógica voltar a 60 com 3 e 4 jogadores.

## Contexto

- **Duas threads em paralelo [C].** A do EE roda o jogo recompilado; na saída de cada `hier(v, 0)` o gancho `hierTapped`
  (`native/runtime/wotm_overrides.cpp`) chama `wotmCaptureScene` (`native/runtime/wotm_live.inc`), que faz a coleta nativa
  (`Scene::collect`, com a travessia `Scene::walk`) e o instantâneo (`Scene::takeSnapshot`) da vista. No fim do quadro, o
  `wotmCaptureFrame` junta as vistas 1 a 3 em `Snapshot::views` e publica. A thread da janela (`wotmDrawNative`, mesmo arquivo)
  desenha cada vista numa textura (`drawView`: `Scene::show` + `Scene::draw` + `Scene::drawHp`), depois os flares adiados
  (`drawFlaresFor`, com `wotm::AsyncDepth`) e compõe os quadrantes. O teto é a thread mais lenta.
- **Laço do jogo por vista [C]** (`docs/passos/ESPINHA.md`; `rtMain__Fb`, `0x188318`): `CullView` (`CullView__6Debrisi`,
  `0x12F810`), troca de buffers, `viewUpdate`, HUD, `hier(v, 0)` (`hier__Fii`, `0x2029B0`), `TheGame::Update` (`0x1372B8`, só na
  vista 0), `animationRunGlobal` (`0x1F1820`), `Update2` (`0x1376A8`), partículas (`particleDraw__Fi`, `0x21B378`), `hier(v, 1)`.
  Consequência que pesa no reaproveitamento entre vistas: **a vista 0 é capturada antes do `TheGame::Update` e as vistas 1 a 3
  depois** (o comentário do `hierTapped` diz o mesmo para as vistas 2 e 3). Monstros e objetos que se movem têm matrizes diferentes
  entre a vista 0 e as outras; a fase estática é igual em todas.
- **Dois executáveis [C]** (`docs/passos/FRONTEIRA.md`, passo 5; `RUNTIME.md`): `wotm.exe` (o jogo, `PS2X_HW_EMU=0`, sem VU1/VIF1/GS)
  e `ps2EntryRunner.exe` (teste: checagem VU1 x nativo `WOTM_VU1CHECK`, fotos, `--vu1cmp`, corpus). O desenho nativo é o mesmo
  código nos dois; o replay do corpus usa o `Scene` do `ps2EntryRunner.exe`, então **toda mudança no `Scene::draw` aparece no
  corpus** (bom para pegar regressão).

## O que se sabe (medidas já registradas)

### Thread da janela [C]
`POS_FRONTEIRA.md`, item 1 (máquina do dono, monitor de 180 Hz, luta IA x IA no Centro, `wotm.exe`, `PS2X_STATS`):

| Monstros | Vistas | Redesenho (parede / CPU) |
|---|---|---|
| 2 | 1 | 8,9 / 7,2 ms |
| 2 | 2 | 15,1 / 14,0 ms |
| 8 | 1 | 11,7 / 10,5 ms |
| 8 | 2 | 18,6 / 17,9 ms |

Um modelo simples que fecha com a tabela: **CPU ≈ V × (B + M × n)**, com V vistas, n monstros, **B ≈ 6 ms** (fase, céu, HUD,
partículas e o fixo por vista) e **M ≈ 0,55 ms por monstro por vista** (deformação, desenho, sombra do monstro e os receptores dela).
4 vistas e 4 monstros: ~33 ms; com 2 monstros, ~29 ms (o `POS_FRONTEIRA.md` estimou ~27). A meta de 16,7 ms (com folga, ~14 ms)
pede **B perto de 2,5 ms e o custo dos monstros dividido entre as vistas**.

Item 11 (colaborador, Ryzen 3 3200G + RX 570): 3 vistas ~36 ms depois da correção do lens flare, "~12 ms de CPU por quadrante contra
~7 ms da tela cheia". **[H, forte]** os dois números são de máquinas diferentes: 36 / 3 = 12 ms por vista no 3200G, contra ~7 ms por
vista no PC do dono (a mesma razão de ~1,7 aparece no EE). O quadrante não é mais caro que a tela cheia: os `ViewDef` dos quadrantes
mantêm o FOV da tela cheia (`wotm_split4.inc`, cabeçalho), e o custo não depende da área. Conferir com a medida do passo 0 na mesma
máquina. Na prática, no 3200G a meta é ~3× mais barato por vista; no PC do dono, ~2×.

### Thread do EE [C]
- PC do dono: cada vista a mais custa ~2,5 ms (2 monstros) e ~3,8 ms (8 monstros); 4 vistas ~13 ms (cabe).
- 3200G: `hier(v, 0)` ~4 ms por vista, 2P ~17 ms, 3P ~21 ms, lógica a 30 (`updateRate`). A captura nativa é ~0,3 ms por vista.
- `FRONTEIRA.md`, passo 3 (PC do dono, 1 vista, 2026-10-18): a montagem dos pacotes custa ~0,2 ms dos ~6 ms do EE
  (`pktAddVu1ObjAsm` 130 a 165 µs em ~200 chamadas, `pktLinkAlphaObjAsm` 6 µs, sombras ~10 µs, sprites ~5 µs), contra ~1 ms do
  `hier` inteiro. Ou seja, **o pacote é ~20% do `hier`, não "boa parte"**; o resto é a travessia em si (VU0 traduzido para o
  corte, LOD, animação dos `ANIM_XFORM` em `hierAnimTransNode` `0x203640`, ossos em `hierRotateSkel`/`hierTranslateSkel`
  `0x203580`/`0x2034A0`, que escrevem a paleta que o nativo lê, nós de controle `hierCtrlNode` `0x2048F0`, luzes, emissores,
  localizadores, sombras `psObjectCull` `0x208A20`). O DMA dos pacotes já é descartado no `wotm.exe`; só a montagem sobra.

## Onde está cada coisa

| O quê | Onde | Observação |
|---|---|---|
| Laço do redesenho e das vistas | `wotm_live.inc`, `wotmDrawNative`, perto da linha 1500 (`drawView`, flares adiados, composição) | vistas 1 a 3 desenhadas antes da 0 |
| Medida do redesenho | `wotm_live.inc`, `s_drawMs`/`s_drawCpu`/`s_drawN` (~1486 e ~1759), linha `[native] redesenho:` (~1911) | **só dois baldes: 1 vista e "dividida"** (2, 3 e 4 caem juntos) |
| Captura por vista (EE) | `wotm_live.inc`, `wotmCaptureScene` (~1025) e `wotmCaptureFrame` (~1078); `captureScene(v)`: uma `Scene` de coleta por vista | coleta + instantâneo; partículas no fim do quadro |
| Medida do `hier` e da captura | `wotm_overrides.cpp`, `hierTapped` (~1016) e `fieldLogFrame` (~676): `vistas=`, `hier=`, `cap=`, `hier23=`, `cap23=` | `hier=` soma os dois passos da vista |
| Travessia nativa e corte por vista | `native/wotm_native.hpp`, `Scene::walk` (~3880), `outOfView` (~3856), `Scene::collect` (~1729) | já corta OBJECT e GROUP pelos planos da vista (como o `hierTraverseAsm`) |
| Instantâneo / troca de vista | `Scene::takeSnapshot` (~1813, copia as paletas em `DrawItem::pal`) e `Scene::show` (~1852, copia os vetores para a `Scene` da apresentação) | cópia por vista, barata |
| Desenho de uma vista | `Scene::draw` (~2530): céu, `drawList` passe 2 (travessia), passe 1 (OT de alfa), `drawShadows`, `drawSprites`, `fx.draw`, overlay, HUD | |
| Envio dos objetos | `Scene::drawList` (~2708): monta `order`, abre um lote por textura/estado (`rlBegin`/`rlSetTexture`, `rlEnd` + `rlDrawRenderBatchActive` a cada troca) e emite cada vértice | o laço por vértice está em ~3032-3084 |
| Por vértice | `skinVertexPM` (~3744), `xform`, `backFacing` (~1525), `envUv`, `vertexRgb` (~3705), `shaderNormal` (`wotm_fx.hpp`, ~155), `rlColor4ub`/`rlTexCoord2f`/`rlNormal3f`/`rlVertex3f` | |
| Malhas | `Scene::mesh` (~2395): cache por endereço do pacote, revalidado por `packetSig` a cada chamada | chamado várias vezes por item e por vista |
| Sombras | `collectShadows` (EE), `drawShadows` (~1417), `shadowTexture` (~1561), `forEachTri` (~1535) | silhueta 128x128 rasterizada na CPU |
| Partículas | `native/wotm_fx.hpp`, `Particles::draw` (~1003), destroços em `Scene::drawSprites` (~2047) | também modo imediato (`RL_QUADS`) |
| Vistas 2 e 3 | `native/runtime/wotm_split4.inc` | quadrantes, câmeras, HUDs |

## O que custa na thread da janela [C, pelo código]

Por vista, para cada item visível, em cada passe em que ele entra:
- `drawList` percorre a lista inteira três vezes (céu à parte; passe 2 e passe 1 da lista principal), e em cada uma chama
  `mesh()` (busca no `unordered_map` mais `packetSig` com `skipZeroQwords`, leitura da RAM), `inAlphaOT`, `texKey` por slot
  e `textureId` (outra busca) duas vezes por entrada de `order`.
- Abre um lote novo a cada troca de textura, de mistura, de teste de alfa, de ZMSK, de passe do GS ou de CLAMP; cada abertura fecha o
  anterior com `rlEnd` + `rlDrawRenderBatchActive` (upload do lote inteiro e um draw). O `setAlphaRef` também descarrega. No passe 2
  a ordem é a da travessia, então as trocas são muitas.
- Para cada vértice: transformação (três matrizes de osso mais a do objeto nos deformados), `backFacing` nas seções com culling,
  `vertexRgb` (nos iluminados, **três raízes quadradas por vértice** para normalizar as linhas da matriz do objeto, que são
  constantes por item), UV do mapa de ambiente quando há, e quatro chamadas do rlgl (`rlColor4ub`, `rlTexCoord2f`, `rlNormal3f` via
  `shaderNormal`, `rlVertex3f`).
- **Sombras (`drawShadows`):** para **cada bloqueador** (cada monstro), `shadowTexture` passa por todos os triângulos dos que
  projetam (`forEachTri`: deformação lendo as matrizes da RAM a cada vértice, mais `vertexRgb` que ninguém usa), rasteriza na CPU num
  quadro 128x128, borra duas vezes (3x3, com alocação de vetores) e manda com `UpdateTexture`. Depois percorre todos os itens da
  vista testando a esfera contra o cilindro e reemite os receptores (de novo com `vertexRgb` descartado). Tudo isso **por vista**,
  embora a silhueta seja do espaço da luz e não dependa da câmera. Há ainda `std::getenv("WOTM_SHADOWLOG")` sem `static` por
  bloqueador e por quadro (o próprio código lembra que o `getenv` é lento no Windows).
- Achado à parte **[H]**: o `forEachTri` lê a paleta e as matrizes da RAM viva (`palette(ram, it)`, `skinVertex(ram, ...)`), não a
  cópia do instantâneo (`DrawItem::pal`) que o `drawList` usa. Na thread da janela, com o EE escrevendo o quadro seguinte, a silhueta
  da sombra pode sair com a pose de outro quadro (a memória do projeto já registrou flicker de sombra por leitura fora de hora).
  Usar o `it.pal` resolve isso e barateia ao mesmo tempo (item A).

O que **não** pesa: a GPU (a meia largura e o quadrante não barateiam, e a RX 570 sobra), a coleta (fica na thread do EE, ~0,3 ms
por vista), o `Scene::show` (cópia de vetores) e os flares (já adiados, com profundidade por PBO).

**[H] Por objeto ou por vértice?** O `POS_FRONTEIRA.md` diz "envio de cada objeto"; o código mostra as duas coisas: um custo por
lote (descarregamento a cada troca de estado) e um custo por vértice (matemática + quatro chamadas do rlgl). Nenhuma medida separou
os dois. É o primeiro número a tirar (passo 0), porque decide entre o item C (os dois) e o E (só o por vértice).

## Candidatos

Cada item: onde mexe, ganho esperado (no PC do dono, 4 vistas e 4 monstros, base ~30 ms de CPU da janela; no 3200G multiplique por
~1,7), risco, como medir, dependência.

### 0. Medir direito antes (pré-requisito, meio dia)
- **Onde:** `wotm_live.inc` (baldes do redesenho por número de vistas: 1, 2, 3, 4, em vez de 1 e "dividida"); `Scene::draw` e
  `drawList` (contadores por vista: itens, entradas de `order`, vértices emitidos, lotes abertos, chamadas a `setAlphaRef`, e tempo
  de cada fase: céu, passe 2, passe 1, sombras com `shadowTexture` à parte, sprites, `fx.draw`, HUD). Sair na linha
  `[native] redesenho:` do `PS2X_STATS` (ou numa variável nova, `WOTM_DRAWSTATS=1`, que entra na tabela do `NATIVO.md` e passa no
  `python tools/wotm_env.py`). Tempo com `QueryPerformanceCounter`; o `wotmThreadCpuMs` anda em passos de 15,6 ms do Windows e só
  serve em média de muitas amostras (já é assim no balde de 300).
- **Também medir:** o mesmo cenário no build com LTCG (`build_runtime.bat build`, item G1) e, no EE, o `hier` por vista com
  `PS2X_HOSTPROF=3` ou o `top` da linha `[field]` (os três pcs que mais gastaram).
- **Corpus de tela dividida:** o `corpus.ps1` não tem sessão de tela dividida e a checagem VU1 x nativo só compara a vista 0. Antes
  de qualquer mudança no desenho, guardar umas fotos de referência da luta de 4 (F8/fotos no `ps2EntryRunner.exe`, ou ao menos
  capturas de tela do `wotm.exe` com o mesmo `WOTM_MATCH`) para comparar a olho as vistas 1 a 3.

### A. Sombras uma vez por redesenho (o mais barato; meio dia a um dia)
- **Onde:** `Scene::drawShadows`/`shadowTexture`/`forEachTri` (`wotm_native.hpp`, ~1417-1615).
- **O quê:** (1) cache das texturas de sombra por redesenho, com chave pelos dados de entrada (índice do bloqueador, origem,
  extensão, direção da luz e, por quem projeta, nó + matriz + bloco da paleta): as vistas 1 a 3 (mesmo estado do jogo) passam a
  dividir a mesma textura; a vista 0 (antes do `Update`) cai numa chave própria quando o monstro se move. Com 4 vistas: 2
  rasterizações por bloqueador em vez de 4, sem mudar nenhum pixel. (2) `forEachTri` com um parâmetro para não calcular cor nem
  normal quando o chamador não usa (as duas chamadas das sombras), e deformando pela `DrawItem::pal` em vez de ler a RAM.
  (3) `getenv` do `WOTM_SHADOWLOG` em `static`. (4) Os dois vetores do borrão alocados uma vez (membro), não a cada sombra.
- **Ganho [H]:** a parte "sombra" do M; estimativa 2 a 4 ms em 4 vistas e 4 monstros no PC do dono (a medida do passo 0 diz quanto
  do 0,55 ms por monstro é sombra). Em 1 vista também ganha um pouco (o `vertexRgb` e as leituras da RAM somem).
- **Risco:** baixo. A silhueta das vistas 1 a 3 fica idêntica à de hoje (mesma entrada); o desvio de LOD da sombra (pelo olho da
  vista, não pela `shadowLodRangeSq` do jogo; `lod_fixo.md`) continua igual. Trocar a RAM viva pela cópia muda a sombra só quando
  hoje ela sai errada (pose de outro quadro).
- **Como medir:** tempo de `drawShadows` por vista (passo 0); corpus `-Check -Baseline` sem piora; olho nas sombras dos 4 monstros
  em movimento.
- **Independente** dos outros.

### B. Constantes por item fora do laço por vértice (meio dia)
- **Onde:** `drawList` e `vertexRgb`.
- **O quê:** normalizar as três linhas da matriz do objeto uma vez por item (hoje três `sqrt` por vértice iluminado); resolver
  `mesh()`, `texKey` e `textureId` uma vez por item por vista (um vetor paralelo à lista, preenchido no primeiro passe e reusado no
  segundo, na sombra e no `translucent`); tirar do laço por vértice os blocos de diagnóstico que fazem trabalho mesmo desligados
  (todos já são `static`, conferir caso a caso).
- **Ganho [H]:** 5 a 15% do B, ~1,5 a 4 ms em 4 vistas. Resultado numérico idêntico, se a normalização for feita com a mesma
  expressão (a divisão por `len` continua por vértice; só o `sqrt` sai).
- **Risco:** baixo; corpus tem de sair idêntico.
- **Independente**; ajuda qualquer caminho que vier depois.

### C. Buffer próprio e lista de comandos no lugar do modo imediato (1 a 2 dias)
- **Onde:** `drawList` (e, num segundo momento, `drawShadows`, `drawSprites` e `Particles::draw`).
- **O quê:** em vez de `rlBegin`/`rlVertex*`/`rlEnd`/`rlDrawRenderBatchActive`, o `drawList` escreve os vértices num vetor próprio
  (posição, UV, cor, a "normal" com os flags do shader) e grava uma lista de comandos `{primeiro vértice, contagem, textura, mistura,
  limiar de alfa, máscara de Z, CLAMP, modo}`; no fim, um upload só (`glBufferData`/`glBufferSubData` num VBO com orphaning) e um
  `glDrawArrays` por comando, mudando só o estado que mudou. **Mesma ordem, mesmos estados, o mesmo shader (`alphaShader_`) com o
  layout de atributos do rlgl**: a imagem tem de sair igual. O MVP vem de `rlGetMatrixModelview`/`rlGetMatrixProjection` (o que o
  rlgl já manda); o uniform do limiar de alfa vira estado do comando. Antes de cada bloco GL próprio, `rlDrawRenderBatchActive` para
  não misturar com o lote do raylib.
- **Ganho [H]:** tira o custo por lote (cada descarregamento do rlgl sobe o lote inteiro e refaz estado) e troca quatro chamadas por
  vértice por escritas num vetor. Se o passo 0 mostrar centenas de lotes por vista, é o maior ganho isolado: estimativa 30 a 50% do
  B, 7 a 12 ms em 4 vistas.
- **Risco:** médio-baixo. Pontos de atenção: os estados que o `drawList` muda no meio (`rlEnableDepthMask`/`rlDisableDepthMask`, wrap
  da textura no CLAMP, `gsBlend*` com os passes extras do `doubled` e do As > 1, `setAlphaRef`) precisam virar campos do comando;
  MSAA (`wotm::msaa::Target`) e o render scale não mudam; a textura do quadro anterior (`screenId_`) e a do filme continuam por id.
- **Como medir:** lotes e tempo do passe 2 e do passe 1 por vista (passo 0); corpus idêntico; `run_live` com 1, 2 e 4 jogadores.
- **Base de D, E e H** (é o que permite gerar vértices fora do rlgl, guardá-los e gerá-los em outra thread).

### D. Reaproveitar vértices prontos entre vistas e entre quadros (1 a 2 dias, depois de C)
- **O quê:** um cache de "fluxo de vértices pronto" (posição no mundo, UV, cor iluminada, flags) por item, com chave exata:
  nó + endereço do pacote + `packetSig` + matriz + bloco da paleta (e o conteúdo dela, nos deformados) + estado da luz (`lights_`,
  luzes pontuais do item, `color`, `spec`, `uvOff`). Dentro de um redesenho, o mesmo monstro visto em três vistas é deformado uma vez
  (as vistas 1 a 3 têm o mesmo estado; a 0 tem o seu). Entre quadros, a fase estática (matriz e luz iguais) não é recalculada.
  Fica por vista o que depende do olho: `zBufferFudge` (desloca no raio do olho), `backFacing`, UV do mapa de ambiente (`env`),
  fade de LOD (`it.fade`) e os passes extras; os itens com `env` podem simplesmente ficar fora do cache.
- **Ganho [H]:** a matemática por vértice da fase some na maioria dos quadros (câmera andando não muda a matriz dos objetos) e a dos
  monstros cai para ~2 vezes por quadro em vez de V. Somado a C, o B poderia chegar perto do custo de copiar vértices.
- **Risco:** médio: chave incompleta = objeto com cor ou pose velhas. A luz pode mudar no meio da luta (explosões, luzes pontuais),
  os destrutíveis trocam de nó (chave nova, ok), o `uvOff` anima texturas rolando. Começar só pelos itens sem `animPkt`, sem `env`,
  sem luz pontual, e só depois os deformados.
- **Como medir:** taxa de acerto do cache e tempo do passe 2 (passo 0); corpus idêntico.
- **Depende de C.** É o que a ideia "skinning uma vez por quadro" (candidato 5 do pedido) vira na prática.

### E. Malhas estáticas na GPU (2 a 4 dias, depois de C e D)
- **O quê:** o fluxo pronto dos itens estáticos (de D) vai para VBOs residentes, um por item ou por grupo de itens com o mesmo estado,
  subidos uma vez e reusados por todas as vistas e quadros; por quadro só sobram os draws (e, com `glMultiDrawArrays` ou
  agrupamento por estado, poucos). O que depende do olho vai para o shader: `zBufferFudge` (uniform por draw ou atributo), culling das
  seções com o bit 15 pelo `GL_CULL_FACE` (a regra do `backFacing` em sentido de giro do GL; seções com e sem culling em faixas
  separadas). Itens com `env`, deformados, com `uvOff` animado ou com fade continuam no caminho de C/D.
- **Ganho [H]:** o B das vistas vira custo de draw; com a fase quase toda estática, 1 a 2 ms por vista no PC do dono. É o que leva
  4 vistas para baixo de 14 ms no 3200G.
- **Risco:** médio-alto. Mais código GL fora do raylib; a regra de giro tem de bater com o `backFacing` (o corpus pega); a luz
  "estática" tem de ser invalidada quando muda (chave de D); memória da GPU (pouca: a fase é pequena para um PC).
- **Depende de C e D.**

### F. Corte por vista e distância de desenho
- **Já existe [C]:** `Scene::walk` corta OBJECT e GROUP pela esfera contra os planos da vista (`outOfView`, `Particles::inViewVolume`,
  o mesmo teste do `hierTraverseAsm`, com o `g_doFOVGroups`), e as partículas por `inViewFov`. Cada vista coleta só o que ela vê; o
  LOD (`switchOutDis`) já some com objetos longe. Não há ganho grande sobrando aqui.
- **O que sobra:** (1) os receptores de sombra já vêm da lista cortada, mas o teste esfera x cilindro roda para todos os itens da vista
  por bloqueador: um corte prévio pela caixa de cada bloqueador é barato (entra junto com A). (2) Uma **distância máxima de desenho
  por vista** ou um corte de objetos pequenos nos quadrantes muda o que aparece: só como opção do painel (aba VIDEO cheia, ver
  `lod_fixo.md`), nunca como padrão. Ganho proporcional ao que some; baixa prioridade.

### G. Lado do EE
- **G1. Build com LTCG (só medir, 1 h + ~30 min de link):** `build_runtime.bat build` liga `/GL` + `/LTCG` (`RUNTIME.md`); o
  `build_dev` não. O código traduzido é enorme e cheio de chamadas pequenas; inlining entre unidades pode baratear o `hier` e o resto
  **[H]**, sem risco de comportamento. Se o ganho for real, o `wotm.exe` distribuído sai do build com LTCG e o desenvolvimento segue
  no `build_dev`. Vale também para a thread da janela (o `native/` compila no mesmo executável).
- **G2. Perfilar o `hier` por vista (meio dia):** com `PS2X_HOSTPROF=3` e o `top` do `[field]`, separar quanto de cada vista é
  travessia, VU0 traduzido, animação/ossos, sombras do jogo (`psObjectCull`, `hierShadow__Fii` `0x204430`), partículas
  (`particleDraw`) e pacotes. Sem isso, qualquer corte é chute.
- **G3. Cortar a montagem dos pacotes do VU1 só no `wotm.exe` (1 a 3 dias, risco alto):** `pktAddVu1ObjAsm` (`0x208410`) e
  `pktLinkAlphaObjAsm` (`0x20895C`), chamados de dentro do `hierTraverseAsm` (`0x207198`), e os montadores de sombra e partícula.
  - **Ganho [H]:** ~20% do `hier` pela medida do `FRONTEIRA.md`; no 3200G, ~0,8 ms por vista (3P: ~21 → ~18,5 ms). Sozinho não
    devolve os 60.
  - **Risco:** o `FRONTEIRA.md` (passo 3) já decidiu não fazer: o `pktAddVu1ObjAsm` é assembly à mão com convenção própria (lê
    registradores e estado do VU0 deixados pelo `hierTraverseAsm` e devolve estado no contexto), então devolver cedo exige refazer o
    que ele deixa. Os contadores de objetos no pacote são lidos pelo laço de espera do `particleDraw` e pelo `hierFlushObjQ`
    (`0x209D88`), hoje ganchos. A paleta dos ossos **não** vem do pacote (vem do `hierAnimTransNode`/`hierRotateSkel`, que
    escrevem `animMatrixPtr`), mas isso tem de ser conferido com `WOTM_LISTSKIN=1` depois do corte. O `hierTraverseAsm` inteiro não
    pode sair (anima, controle, câmera, luz, partículas, ossos, localizadores, LOD).
  - **Onde:** um gancho por nome (`wotm::fn::pktAddVu1ObjAsm`) nos `wotm_overrides.cpp`, compilado só com `PS2X_HW_EMU=0`, para
    a checagem, as fotos e o replay do `ps2EntryRunner.exe` continuarem com o caminho original. Se exigir mexer no C++ gerado, é
    costura do PS2Recomp: regenerar `tools/runtime_patch/wotm-runtime.patch`.
  - **Medir:** `hier=`/`hier23=` do `[field]` antes e depois; `WOTM_LISTSKIN=1`; menu pelo roteiro de pad até a luta; lote
    (`tools\lote.ps1 -Check 0`) sem vigia acusando nada.
- **G4. Reaproveitar trabalho do jogo entre vistas (só depois de G2):** a animação dos `ANIM_XFORM` e as multiplicações dos ossos
  rodam no `hier` de cada vista com a mesma entrada (vistas 1 a 3). Pular isso é mudar o código do jogo: só com prova de que o
  resultado é idêntico (difftest/`ramdiff` antes e depois) e, de novo, só no `wotm.exe`. Fica registrado como pista, sem estimativa.
- **G5. Captura nativa fora da thread do EE (~0,3 ms por vista, baixo ganho):** a coleta (`Scene::collect`) lê a RAM no instante
  da saída do `hier`; tirá-la da thread exigiria copiar a RAM relevante antes. Não compensa agora.

### H. Paralelizar a geração por vista (1 a 2 dias, depois de C)
- **O quê:** com C, cada vista gera o seu vetor de vértices e a sua lista de comandos sem tocar no GL; as vistas vão para threads
  de trabalho e o thread da janela só sobe e desenha. Pré-condições: `mesh()` e `textureId()` mudam caches (`cache_`, `tex_`,
  `partial_`, `texAmax_`) e o `textureId` cria textura GL, que só pode nascer no thread do contexto. Solução: um pré-passe no thread
  da janela que resolve malhas e texturas de todos os itens das vistas, e a geração em paralelo só lê. O cache de D precisa de
  trava ou de ser preenchido no pré-passe.
- **Ganho [H]:** a geração dividida pelos núcleos livres. No 3200G (4 núcleos, 4 threads; EE e janela já ocupam dois) sobram 1 a 2:
  até ~2× na geração. No PC do dono, mais.
- **Risco:** médio (corridas de dados; o `ViewScope`/`vctx` e outros estados da `Scene` são por instância e hoje compartilhados entre
  as vistas no `S.scene`; cada thread precisa da sua cópia do estado de vista).
- **Depende de C**; combina com D (cache compartilhado) e E (menos para gerar).

## Ordem recomendada

| # | Item | Esforço | Ganho esperado (PC do dono, 4 vistas, 4 monstros; ~30 ms hoje) | Risco | Depende |
|---|---|---|---|---|---|
| 1 | 0. Medidas (baldes 1-4, contadores por passe, lotes, vértices) + G1 (LTCG) | meio dia + 1 h | decide o resto | nenhum | - |
| 2 | A. Sombras uma vez por redesenho (+ paleta do instantâneo) | meio dia a 1 dia | 2 a 4 ms | baixo | - |
| 3 | B. Constantes por item | meio dia | 1,5 a 4 ms | baixo | - |
| 4 | C. Buffer próprio + lista de comandos | 1 a 2 dias | 7 a 12 ms (se os lotes forem muitos) | médio-baixo | - |
| 5 | D. Cache de vértices entre vistas e quadros | 1 a 2 dias | o grosso do que sobra do por vértice | médio | C |
| 6 | E. Malhas estáticas na GPU | 2 a 4 dias | B → 1 a 2 ms por vista | médio-alto | C, D |
| 7 | H. Geração em threads | 1 a 2 dias | até ~2× no que sobrar | médio | C |
| - | G2 (perfil do `hier`) em paralelo a 2-4; G3 só se G2 e G1 não resolverem o EE no 3200G | meio dia / 1 a 3 dias | ~0,8 ms por vista no 3200G | alto | G2 |
| - | F (distância de desenho) só como opção, se ainda faltar | - | proporcional | visual | - |

Com 0 + A + B + C (3 a 4 dias), a estimativa é a janela de 4 vistas sair de ~30 para ~15 ms no PC do dono **[H]**, o suficiente
para 60 lá (o EE já cabe com ~13 ms). No 3200G a janela ainda ficaria em ~25 ms e o EE em ~21 a 25 ms: para ele, D + E (+ H) na
janela e G1/G2/G3 no EE, mais 1 a 2 semanas, e mesmo assim o EE pode continuar acima de 16,7 ms com 4 vistas. Se ficar, o caminho é
aceitar a lógica a 30 com 3 e 4 jogadores nessa classe de CPU, ou a apresentação interpolada que o `POS_FRONTEIRA.md` já lista como
pendente.

## Como medir (protocolo)

- **Cenário fixo:** `wotm.exe` (`run_live` com `-Exe` do jogo, ou o launcher), `WOTM_MATCH=3,1,2,5,1` (4 monstros no Centro, o
  mesmo das medidas do `FRONTEIRA.md`), `WOTM_PLAYERS=4` e `WOTM_AIVSAI=1` (os quatro jogadores na IA), 70 s, `PS2X_STATS=1` e
  `WOTM_FIELDLOG=1`. Repetir com `WOTM_PLAYERS=3`, `=2` e sem ele (1 vista), e com 2 monstros (`WOTM_MATCH=3,1,1`, Togera contra Congar
  no Centro; a fase é o último número, `NATIVO.md`). `WOTM_SPLITLOG=1` uma vez para confirmar que as vistas vieram em quadrantes.
- **Números a guardar:** da linha `[native] redesenho:` (depois do passo 0, um por número de vistas) a parede e a CPU médias; do
  `[field]`, mediana e p99 do host menos ócio, `hier=`/`hier23=`, `cap=`/`cap23=` e a fração de quadros em 1 field. Três corridas de
  cada, alternadas, como no `FRONTEIRA.md`; numa tabela no fim deste arquivo ou no `POS_FRONTEIRA.md`.
- **Mesma máquina** para o antes e o depois; anotar CPU e monitor. Não comparar o PC do dono com o 3200G.
- **Regressão visual:** `tools\corpus.ps1 -Check` antes (base) e `-Check -Baseline <csv>` depois de cada item; A, B e C têm de sair
  sem diferença. Para as vistas 1 a 3, que o corpus não cobre, comparar as fotos do passo 0.
- **Estabilidade:** `tools\lote.ps1 -Check 0` (o `lote.ps1` não tem parâmetro de jogadores: conferir se ele herda `WOTM_PLAYERS` do
  ambiente antes de contar com isso) e uma luta de 4 pelo menu (`wotm_padsel.inc`), com o dono conferindo na tela.
- Instantâneos e dumps gerados nas medidas: apagar depois (regra do `CLAUDE.md`).

## Armadilhas

- **A vista 0 é de antes do `TheGame::Update`, as outras de depois.** Qualquer reaproveitamento entre vistas tem de usar chave
  exata (matriz, paleta), não "a mesma coisa da vista 0"; senão as vistas 1 a 3 mostrariam monstro e sombra um quadro atrasados.
- **A ordem do passe 2 é a do GS.** Reordenar por textura (a forma ingênua do "lote por textura") muda o resultado de mistura e de
  empate de profundidade (camadas coplanares com `zBufferFudge`, o prédio em cross-fade do `luta_hud`). Por isso o item C mantém a
  ordem e tira só o custo de cada troca. Reordenar só os itens opacos sem ABE e sem ZMSK é possível depois, com o corpus vigiando.
- **Estado da `Scene` compartilhado entre as vistas:** `S.scene` é uma só e o `show` troca `vctx`, olho, luz e neblina a cada vista;
  caches por vista (A, D) e threads (H) não podem depender desse estado implícito.
- **`textureId` cria textura GL:** nunca fora do thread da janela.
- **Checagem e replay:** cortes no lado do EE (G3, G4) só no `wotm.exe` (`PS2X_HW_EMU=0`); o `ps2EntryRunner.exe` mantém o caminho
  original, que é a referência do `VU1CHECK.md`. Mudanças no `Scene` valem nos dois e aparecem no corpus.
- **`wotmThreadCpuMs` tem passo de 15,6 ms:** use média de muitas amostras ou `QueryPerformanceCounter`.
- **Variáveis novas** (`WOTM_DRAWSTATS` ou outra) entram na tabela do `NATIVO.md` e passam no `python tools/wotm_env.py`.
- **Build:** `build_runtime.bat` (build_dev) para o dia a dia, pelo PATH mínimo (memória do projeto); o LTCG só para medir G1 e para
  o executável distribuído.
