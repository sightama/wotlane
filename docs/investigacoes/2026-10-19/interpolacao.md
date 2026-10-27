# Interpolação de quadros: levantamento e plano (2026-10-19)

Investigação só de leitura de código (nada compilado nem rodado). Pedido: o nativo desenhar, na taxa do monitor, a mistura dos dois
últimos instantâneos do jogo (câmera, matrizes dos objetos, paleta de ossos), com um quadro de atraso (`PROXIMOS_PASSOS.md`,
"Interpolação de quadros"; `passos/NATIVO.md`, "Taxa de apresentação").

**Antecedente importante:** já houve uma interpolação, retirada no commit `2fa669e` (2026-10-16). Ela tinha `lerpM4` com
decomposição em quatérnio + escala uniforme e `slerp`, `lerpCamera` com detecção de corte (400 unidades / `dot(fwd) < 0,7`) e
`Scene::blend(A, B, alpha)` casando itens por `(node, ocorrência)`, paletas por par de ponteiros e destroços por `Sprite::key`, com F4 /
`WOTM_NOINTERP`. Saiu por dois motivos: (1) com a lógica a 60 e a tela a 60 ela só atrasava um quadro; (2) o HUD, que o jogo cola na
câmera, era misturado separado dela e tremia. O motivo (1) deixou de valer com a apresentação na taxa do monitor (2026-10-18). O (2) tem
solução (abaixo). O código velho (`git show 2fa669e -- native/wotm_native.hpp`) é ponto de partida, mas a captura mudou muito desde
então (thread do EE, dois tempos, tela dividida, sombra de RAM das partículas).

## 1. O instantâneo hoje

### O que guarda (`native/wotm_native.hpp`)
- `struct Snapshot` (wotm_native.hpp:747-764): listas de `DrawItem` (`items`, `sky`, `overlay`, `hud`, `hp`, `hpOverlay`), `sprites`
  (destroços, tipo 13), `fx` (`FxSystem`: base, índice, tipo, contagem), `fxRam` (cópia de sombra de 32 MB do que o desenho das
  partículas lê), `flares`, `shadows` (`ShadowBlocker` com os `DrawItem` que projetam), `screenRects` (fades, faixas, barras),
  `screenPolys` (HUD do `ScreenPolys`), `lights`, `fog`, `cam` (`GameCamera`: pos, fwd, up, fovy), `vctx` (vista), `g` (`g_frame`),
  `valid` e `view1` (tela dividida). **Não tem carimbo de tempo** (o `double t` saiu em `2fa669e`).
- `DrawItem` (wotm_native.hpp:441-470): `M4 m` (vetor-linha: linhas 0-2 = eixos com escala, linha 3 = translação), `node`
  (HierObject), `animPkt`, `lodIdx`, `pal` (shared_ptr para as 32 matrizes da paleta, compartilhado por bloco), `fade` (cross-fade de
  LOD), `uvOff`, `color`, `spec`, luzes no espaço do objeto, `env`, `hudId`, `cs` (o `_cs` que o emitiu), `aref`, `zmsk`, `lodX`,
  luzes pontuais (`ptRel`/`ptCol`/`ptFall`, já relativas ao objeto). ~280 bytes cada.
- `Scene::takeSnapshot` (wotm_native.hpp:1671-1705) copia as listas da coleta e lê a câmera (`readGameCamera`, :488) e, para cada item
  com `animPkt`, as 32 matrizes do bloco da paleta (`palette`, :3201) num `shared_ptr` por bloco. **O endereço do bloco não fica
  guardado** (só o ponteiro para a cópia), e ele faz falta para casar paletas entre dois instantâneos.
- `Scene::show` (:1708-1729) copia o instantâneo para a `Scene` da apresentação e devolve a câmera.

### Onde é capturado (thread do EE)
- **Cena** na saída de `hier(v, 0)`: `wotmCaptureScene` (wotm_live.inc:754-776; gancho em wotm_overrides.cpp:904). Uma `Scene` de
  coleta por vista (`captureScene`, :745-749) e `g_sceneSnap[2]` (:750) pendentes.
- **Fim do quadro** na entrada de `timerWaitForMinUpdateRate` (`waitMinRateByVblanks`, wotm_overrides.cpp:680-707, captura em :691) e
  no `sceGsSyncV` do laço do `displayDialog` (:611): `wotmCaptureFrame` (wotm_live.inc:802-880) junta a cena pendente com as partículas
  de agora (`collectFx`), os flares, a cópia de sombra (`Particles::shadowRam`, wotm_fx.hpp:578-640, pool de até 6 buffers de 32 MB),
  os fades e o `ScreenPolys`. Quadro sem `hier(0, 0)` (filme, carga) coleta tudo ali.
- `wotmCaptureFadeStep` (wotm_live.inc:781-800): passo do `Shell::FadeScreen` com redesenho, sem partículas.

### Como a apresentação consome (thread da janela)
- `struct Capture` (wotm_live.inc:382-389): **um buffer só** (`snap` + `fresh`), protegido por mutex. Se a captura publica dois
  quadros antes de a apresentação pegar um, o primeiro se perde.
- `wotmDrawNative` (:1003): pega `cap.snap` se `fresh` (:1105-1113) e o guarda em `State::snap` (:240, um só). Redesenha só com
  instantâneo novo, fade do `Shell::FadeScreen` mudando, câmera livre pausada, janela mudando, ou a ~60/s parado (`rerender`,
  :1185-1186); nas outras apresentações reapresenta `rts[cur]`.
- Desenho: `show(view1)` e `drawView` (:1237), `show(snap)` (:1238), `S.scene.draw` (:1295) com `Camera3D` montada de `gc`,
  `drawHp` (:1308), texto, fades, popups. `S.cur ^= 1` a cada redesenho: o texId 0 ("quadro anterior") é o redesenho anterior.

### O que muda para guardar dois
1. `Snapshot` ganha o tempo: `fields` (contagem de fields no início do quadro, `0x6F8DC0`, que o `waitMinRateByVblanks` já lê) e
   `hostT` (relógio do host na captura, `steady_clock`). Ganha também o endereço do bloco da paleta em cada item (ou um mapa
   bloco → cópia no instantâneo).
2. `Capture` passa a guardar os **dois últimos publicados** (`prev`, `snap`), para a apresentação não perder o par quando atrasa.
3. `State` guarda `A` e `B` (o anterior e o mais novo) e um relógio de exibição (abaixo). Troca de fase (`levelSig`), `g_loadFreeze`,
   pausa e câmera livre pausada zeram `A`.
4. `show` vira `show(A, B, α)` (ou um `Snapshot blend(A, B, α)` que o `show` atual consome, o que deixa o desenho intocado).
5. `rerender` passa a valer em toda apresentação enquanto `α < 1` com interpolação ligada.

## 2. Identidade entre dois instantâneos

| Coisa | Identidade estável? | Nasce / some / troca | Tratamento |
|---|---|---|---|
| Objeto do mundo (`items`, `cs` = 0) | `(node, ocorrência)`; quase todo estático | Corte por esfera (`outOfView`, :3580) feito na câmera de B | Interpolar; matriz igual nos dois = copia sem conta. Com a câmera atrasada em relação a B, o que B cortou e ainda está na vista interpolada some na borda: coletar com margem no corte (raio × 1,2 ou planos alargados) quando a interpolação estiver ligada |
| Objeto de `_cs` (monstros, carros, destroços, power-ups) | `(cs, node, ocorrência dentro do cs)` (DrawItem::cs; ocorrência conta instâncias do mesmo nó no mesmo `_cs`) | `_cs` reaproveitado por outro objeto, teleporte (renascer, troca de rodada) | Interpolar; salto de translação acima do limiar (o antigo era 150 unidades) ou `node` diferente = B sem mistura |
| LOD (`walk` caso 2, :3640-3696) | O nó muda com o LOD | Troca de LOD = item novo; cross-fade (`fade`) já existe | Itens só em B: desenhar B; só em A: não desenhar. `fade`: interpolar quando o par existe |
| SWITCH (6), INTERACTIVE_STATE (39), DESTRUCTIBLE (31), CONTROL (8, prédio "fantasma"), ANIM_XFORM/ANIM_SCALE invisível | Nó filho muda | Estado novo aparece em B | B (o estado novo entra até 1 quadro antes; imperceptível) |
| Paleta de ossos (`pal`) | Endereço do bloco (`palette`, :3201: `animPkt[+0x68 do OBJECT]`) | Troca de LOD do monstro muda o bloco | Interpolar matriz a matriz quando o bloco é o mesmo nos dois e o item casou; senão B |
| Céu (`sky`) | `(node, ocorrência)` | Fixo no mundo (nota da câmera livre) | Interpolar como mundo |
| HUD (`hud`), ícones de botão (`overlay`), lista de alta prioridade (`hp`, `hpOverlay`: molduras de diálogo, pause) | `hudId`/`cs` | O jogo os põe na frente da câmera a cada quadro (`Hud::update`, `AlignCsWithView`) | **Não interpolar a matriz**: rebasear para a câmera interpolada, `M' = M_B · V_B⁻¹ · V_I` (V = câmera → mundo), ou desenhar com a câmera de B. Era isso que tremia em 2026-10-16 |
| Câmera (`cam`, por vista) | A vista | Corte de cinema, troca de câmera, câmera livre, troca de tela | Interpolar com detecção de corte (seção 3) |
| Vista (`vctx`), tela dividida (`view1`) | Vista 0/1 | `viewToggleSplitScreen`, `gUseUnifiedView` (agarrão), cinema | `vctx` diferente ou `view1` presente só num dos dois = corte (B inteiro) |
| Partículas `fx[]` (sprite, texSprite, linha, strite, dustTrail, wake...) | Sistema: `FxSystem::base`/`index` (pode ser reusado depois de morto). Partícula: **o índice não é estável**: `particleMove` compacta a lista mantendo a ordem (`numParticles--`, `pos[i + dIdx]`, src/common/particle.cpp:1583-1688; o mesmo em `dustMove` :1301 e `wakeAndKiteMove` :1366); novas entram no fim | Nascem e morrem a cada quadro | Não casar por índice. **Recuar B pela velocidade**: `p(α) = pos_B − (1 − α)·(vel_B·fields + accel)`, que é a própria integração do `particleMove` ao contrário (é a conta que a linha já usa para a cauda, `pos − vel`). Tamanho: `pos.w − (1 − α)·vel.w·fields` **preservando os bits baixos** de `pos.w` (máscara 3 ou 0xF: escolhem o UV). Nascidas no último passo (`age ≤ fields`): posição de B. Idade (cor): B (é `int16`; muda por quadro). Sistema só em A: não desenhar |
| Arcos (7/17), corrente, sprine, stripe/sweepTrail | Pontos postos pelo jogo (nós do monstro, rastro) | Rastro desloca índices | B no começo; depois casar arco por índice (contagem fixa) e interpolar |
| Destroços (13, `Scene::sprites`) | `Sprite::key = fx·4096 + índice` (wotm_native.hpp:541) **não sobrevive à compactação** | Idem | Recuar pela velocidade como os outros (guardar `vel` no `Sprite` na coleta, `collectParticles`) |
| Pedestres (14) | Idem | Idem | Recuar pela velocidade ou B |
| Sombras (`ShadowBlocker`) | `index` em `psBlkLst` + casters por `node` | Bloqueador liga/desliga | Casters casados como os itens; `origin` interpolada; `extent`, `shadowCs` de B |
| Luzes paralelas, neblina, luzes pontuais por item | — | Mudam devagar | B |
| Lens flare (`flares`) | Lista do jogo, em pixels do GS da câmera de B | — | B (adianta até um quadro em relação à cena). Etapa posterior: reprojetar (desprojetar com a câmera de B, projetar com a interpolada) |
| Texto, `ScreenPolys`, faixas do menu (`letterbox`), fades (`screenRects`) | Lista 2D | Montados por quadro | B. Opcional depois: alfa do fade interpolado entre A e B quando o retângulo é o mesmo |
| Barras sobre o inimigo (`WOTM_ENEMYBARS`), ícone de vida | Projetados pelo jogo com a câmera de B | — | B (adiantam um pouco); reprojetar numa etapa posterior |
| Números de dano (`WOTM_DAMAGEPOP`) | Relógio próprio da janela, ponto no mundo | — | Já usam a câmera do desenho; nada a fazer |
| Texture 0 (quadro anterior: mapa de ambiente de vídeo, CLOAK) | `rts[cur ^ 1]` | Passa a ser a apresentação anterior (1/3 de quadro do jogo a 180 Hz) | Aceitável para refração; se algum efeito acumular (realimentação), manter uma cópia do último redesenho com α = 1 |
| Filme do telão, escolha de lados (padsel) | Textura própria | — | Nada |
| Alfa por nó no menu (`fadeByNode`, lê `node + 0x4C` da RAM viva no desenho) | — | — | Continua lendo ao vivo (já é assim hoje) |
| Câmera livre (F5) | Pose na janela | — | Pausada: sem interpolação (um instantâneo só). Rodando: o jogo desenha com a pose a 60, a interpolação suaviza |

## 3. Como interpolar

### Matrizes (`DrawItem::m`, casters de sombra, paleta)
- Lerp componente a componente encolhe e entorta a malha em giros rápidos; não serve sozinho.
- Proposta (generaliza o `decomposeRot` de `2fa669e`, que só aceitava escala uniforme): na convenção do código, a escala entra como
  `mul(scaleM(sx, sy, sz), m)`, ou seja, **escala por linha**. Decompor cada matriz em `s_i = |linha i|`, `R` = linhas normalizadas
  (ortonormalizar por Gram-Schmidt ou polar se o erro for pequeno), `t` = linha 3. Interpolar `s` linear, `R` por slerp do quatérnio
  (com troca de sinal para o caminho curto), `t` linear. Determinante negativo (espelho): trocar o sinal de uma linha nos dois e
  devolver depois. Se a ortogonalidade das linhas normalizadas falhar (cisalhamento de escalas não uniformes encadeadas, destroços que
  encolhem: `drawCs` lembra que as linhas podem ter norma < 1), cair no lerp componente a componente.
- Paleta: as 32 matrizes do bloco são relativas ao objeto (o desenho faz `skinVertexPM(pm, skinM, v)`, :2755-2785), então interpolar
  cada osso com a mesma função e o `it.m` à parte. Custo: 32 slerps por bloco; meia dúzia de blocos por quadro.
- Atalho: matriz igual em A e B (`memcmp`) = copiar; cobre quase todo o cenário.

### Câmera
- Posição e FOV lineares; orientação por slerp do quatérnio da base (fwd, up, direita) ou nlerp de `fwd` e `up` seguido de
  reortonormalização (o antigo; basta para o giro de um quadro).
- **Corte** (sem mistura, mostra B): salto de posição acima de N unidades (antigo: 400), `dot(fwd) < 0,7`, FOV mudando mais de ~5°,
  `vctx` diferente, `view1` aparecendo ou sumindo, `fields_B − fields_A` > 4 ou relógio do host com buraco > 100 ms (pausa F7, carga,
  diálogo longo), troca de fase. Um corte de verdade no meio de uma interpolação (cinema começando) cai no limiar de posição/direção.
  Melhor ainda, sinalizar no gancho: o `doCinema`/`endCinema` e o `gameReestablishViews` já são pontos conhecidos (tela dividida).
- O `eye_` da `Scene` (faces de trás, mapa de ambiente, neblina, LOD do desenho) passa a ser o olho interpolado, o que o `show` já faz.
- As partículas leem a `worldToScreenMat` e o `worldCtx.eo` da **cópia de sombra** (foco, neblina, cauda de linha e strite,
  wotm_fx.hpp:293-470, 899-925): na mistura, gravar ali a matriz da câmera interpolada (refeita a partir de `GameCamera` com a mesma
  regra de `readGameCamera`: linha 0 = 384·direita + 2048·frente, linha 1 = −180,48·cima + 2048·frente, linha 3 = frente). A
  `prevViewMat` (cauda) fica a de B.

### Tempo e α
- Carimbo: `fields` (o contador do jogo no início do quadro; um quadro de 2 fields dá vão de 2) + `hostT` na captura.
- Relógio de exibição na janela, em fields: `D(agora) = agora_em_fields − O`. A cada instantâneo novo, `O_alvo = hostT_B(em fields) −
  fields_B + atraso`; `O` segue o alvo com filtro (ex.: 5% por instantâneo) e pula se o erro passar de 2 fields. `α = (D − fields_A) /
  (fields_B − fields_A)`, preso em [0, 1]. Sem instantâneo novo, α chega a 1 e fica (segura B; **sem extrapolar**).
- Por que não a conta antiga `α = (agora − t_B) / (t_B − t_A)`: o `hostT` da captura varia com o trabalho do EE no quadro (o
  `wotmCaptureFrame` roda no fim da lógica, ~6 ms com variação), e essa variação vira tremida. O filtro a absorve; o `fields` dá o vão
  certo nos quadros de 2 fields.
- `atraso` = 1 quadro (16,7 ms) + margem (~2 ms) para a captura atrasada ainda chegar antes de α bater em 1.

## 4. Custo e latência

- **Memória:** cada instantâneo tem ~1 MB de itens (alguns milhares × ~280 B) + paletas (2 KB por bloco). A cópia de sombra das
  partículas é um buffer de 32 MB por instantâneo (só ~1,3 MB tocados: `fx[]` 1,2 MB + tabelas); manter A e B segura 2 do pool de 6, e
  a mistura precisa de mais um (a área de `fx[]` copiada de B e reescrita com as posições recuadas e a matriz da câmera). Total a mais:
  ~35 MB de reserva, ~3 MB tocados.
- **CPU da mistura:** casar milhares de itens por hash e decompor só os que mudaram (monstros, carros, destroços: centenas) + ~200
  slerps de osso: bem abaixo de 0,5 ms por apresentação. Recuo das partículas: algumas centenas por quadro, desprezível; o `memcpy` de
  1,2 MB por apresentação a 180 Hz é ~220 MB/s, aceitável (ou reescrever só as faixas `pos[]` dos sistemas).
- **O custo grande é o redesenho:** hoje a cena é redesenhada a 60/s e reapresentada nas outras. O desenho é em modo imediato do raylib
  com a transformação, o skinning e a luz por vértice na CPU (`Scene::draw`, :2364; laço por triângulo em :2779+). Com interpolação,
  redesenho em toda apresentação: 3× a 180 Hz, 2,4× a 144 Hz. **Não há medida do tempo do desenho hoje** (o `PS2X_STATS` conta
  redesenhos, não mede). Se o desenho passar do período do monitor (5,5 ms a 180 Hz), o vsync cai para a metade e a tela fica pior que
  hoje. Por isso: medir primeiro (etapa 0) e ter um teto (`WOTM_INTERP_HZ`, redesenhar no máximo N/s, reapresentando nos outros).
  A saída de fundo é a malha em buffer da GPU com skinning no shader, que já é a direção do port (renderização na fronteira da engine).
- **Latência:** hoje B aparece assim que é capturado e fica ~1 quadro na tela. Com interpolação, B só aparece inteiro quando o próximo
  chega: **+1 quadro do jogo (16,7 ms) + a margem**, do comando à tela. Entra no `atraso` do relógio de exibição; é o preço fixo do
  método. Extrapolar (sem atraso) foi descartado: monstros e câmera mudam de direção a cada quadro (golpes, agarrões).
- **vsync e relógio do host (`FRONTEIRA.md`, passo 4):** são dois relógios: o jogo anda no vsync virtual do `EeScheduler` (ancorado no
  relógio de parede, com os remendos `checkpointDue`/`waitForEvent`) e a janela no vsync do monitor. A interpolação é justamente o que
  absorve a batida entre os dois; o filtro do relógio de exibição segue a cadência do jogo, qualquer que seja. Quando o vblank passar a
  vencer só pelo relógio do host, os carimbos ficam mais limpos, mas nada no plano depende disso. Monitor a 60 Hz com o jogo a 60:
  α fica perto de 1 ou 0 e só soma o atraso; o padrão deve ser **desligar sozinho** quando a taxa de apresentação for ≤ 60 (a não ser
  que o jogo esteja em 30, quadros de 2 fields).
- Replay (`--vu1cmp`, `g_localCollect`), checagem VU1 e `tools\corpus.ps1` não podem mudar: interpolação sempre desligada ali.

## 5. Plano em etapas

Cada etapa liga por variável e fica desligada por padrão até o dono aprovar na tela. Variáveis novas entram na tabela do `NATIVO.md`
(`python tools/wotm_env.py`). Proposta de nomes: `WOTM_INTERP=<lista>` (`0` desliga, `1` = tudo, ou partes: `cam,obj,pal,fx`),
`WOTM_INTERP_ALPHA=<f>` (α fixo, teste), `WOTM_INTERP_DELAY=<ms>` (margem do atraso), `WOTM_INTERP_HZ=<n>` (teto de redesenhos),
`WOTM_INTERPLOG=1` (uma linha `[interp]` por apresentação), F4 alterna ao vivo (livre hoje; era a tecla da interpolação antiga).

0. **Medir e carimbar (nada muda na tela).** Carimbo `fields`/`hostT` no `Snapshot`; `Capture` com os dois últimos; `State` com A e
   B; relógio de exibição e α calculados e só logados. Medir o tempo do redesenho (`[native] desenho=x ms` no `PS2X_STATS`) e
   forçar redesenho em toda apresentação com `WOTM_INTERP=0,redraw` para ver se 180 Hz cabe. **Conferir:** imagem igual à de hoje
   (corpus sem mudança); `[interp]` mostra α em 0, 1/3, 2/3 num monitor de 180 Hz e vão de 2 nos quadros de 2 fields; tempo do
   desenho por apresentação abaixo do período.
1. **Câmera + objetos rígidos + paleta + céu + sombras (`cam,obj,pal`).** `blend(A, B, α)` com a decomposição por linha, casamento por
   `(cs, node, ocorrência)`, paleta por endereço do bloco (guardar no item), HUD/overlay/hp rebaseados para a câmera interpolada,
   detecção de corte, margem no corte por esfera. Partículas ainda em B. (Câmera sozinha não serve nem para olhar: o monstro que a
   câmera segue tremeria contra ela; `cam` sozinho fica como diagnóstico do relógio.) **Conferir:** `WOTM_INTERP_ALPHA=1` dá a imagem
   de hoje bit a bit; `WOTM_INTERP_ALPHA=0` dá a imagem de A. Teste próprio `WOTM_INTERP_SELFTEST=1`: num quadro da luta, no mesmo
   processo e com o mesmo par, desenhar α = 0, α = 1, `show(A)` e `show(B)` em texturas fora da tela e logar quantos pixels diferem
   (esperado 0, fora o texId 0 e o filme do telão, que leem textura viva). Suavidade: `WOTM_CAMLOG` por apresentação e um script
   (`tools/`) que mede a segunda diferença da posição da câmera e de um monstro em tela (hoje: degrau a cada 3 apresentações; com a
   interpolação: constante). Corpus: `-Check` sem mudança (replay desligado). Dono: luta IA x IA num monitor > 60 Hz, giro rápido de
   câmera, agarrão (vista unificada), intro da luta, HUD parado.
2. **Partículas (`fx`).** Cópia de sombra misturada: `pos[]` recuadas pela velocidade, `pos.w` com os bits baixos de B, nascidas no
   último passo em B, `worldToScreenMat`/`eo` da câmera interpolada; destroços (`Sprite`) com `vel` guardada e o mesmo recuo.
   **Conferir:** `WOTM_INTERP_ALPHA=1` igual a hoje; `=0` com as partículas perto da posição de A (não é exato: as mortas no passo
   somem, o arrasto muda a velocidade); tiro do Robo-47, sopro do Magmo, poeira de passos, destroços de prédio caindo.
3. **2D presos ao 3D.** Lens flare, barras do inimigo e ícone de vida reprojetados da câmera de B para a interpolada; alfa dos fades
   interpolado quando o retângulo é o mesmo. **Conferir:** flare dos olhos e das luzes acompanhando a fonte num giro; barra sobre o
   inimigo sem descolar.
4. **Tela dividida e casos de borda.** `view1` com o próprio par e corte quando a divisão muda; texId 0 (decidir: apresentação anterior
   ou último quadro com α = 1); menu (transições do telão, padsel); pausa (F7), câmera livre, diálogos, `Shell::FadeScreen`, carga.
   **Conferir:** `WOTM_MATCH=3,1,1 WOTM_PLAYERS=2` com `WOTM_SHOTS`; menu pelo roteiro de pad até a luta.
5. **Padrão.** Ligar por padrão quando a apresentação passar de 60 Hz (ou o jogo cair para 30) e o desenho couber; caixa no launcher;
   `NATIVO.md` e `PROXIMOS_PASSOS.md` atualizados.

## 6. Riscos

- **Tempo do desenho** (o maior): o redesenho por apresentação multiplica o custo da CPU do desenho imediato. Medir antes; teto por
  `WOTM_INTERP_HZ`; a saída é a malha na GPU.
- **Latência +1 quadro**: inerente. Quem joga com monitor de 60 Hz não ganha nada; desligar sozinho nesse caso.
- **HUD e objetos colados à câmera fora das listas `hud`/`overlay`/`hp`** (itens do menu montados na frente da câmera, objetos
  `AlignCsWithView` com outro id) tremeriam: listar com `WOTM_INTERPLOG` os `_cs` cuja posição relativa à câmera fica constante entre A
  e B e tratar como HUD.
- **Casamento errado** (`_cs` reaproveitado no mesmo quadro, ocorrência deslocada por corte de instância irmã): o limiar de salto
  segura, ao custo de um quadro sem mistura.
- **Corte por esfera na câmera de B**: objeto some na borda durante giros sem a margem.
- **Partículas presas a emissor que se move** (`AttachedFlames`, `parentVec`): o recuo pela velocidade não inclui o movimento do
  emissor; conferir o fogo no monstro andando e, se precisar, casar o sistema e somar o deslocamento do emissor (`emitVec`, +0x2CA0).
- **Leituras vivas no desenho** (`node + 0x4C` no menu, `+0x28`, `+0x2C`, texturas): não mudam com a interpolação, mas fazem o teste
  α = 0/1 não ser bit a bit em algumas telas.
- **texId 0**: efeitos que realimentam o quadro anterior mudariam de intensidade com mais redesenhos por segundo.

## 7. Etapa 0 medida (2026-10-26)

`WOTM_INTERP=redraw` (wotm_live.inc) redesenha a cena em toda apresentação, sem mistura. Luta IA x IA (`WOTM_MATCH=3,1,1
WOTM_AIVSAI=1`, Congar x Togera na fase 1), 50 s por rodada, `PS2X_STATS` + `WOTM_DRAWSTATS`, janela no monitor de 75 Hz; 180 e 240 por
`WOTM_HZ` com `WOTM_VSYNC=0`. "2 vistas" = começo da luta com `WOTM_PLAYERS=2` (depois os monstros ficam perto e a vista unifica).

| Rodada | Apresentações/s | Redesenhos/s | Redesenho 1 vista (ms) | 2 vistas (ms) | GPU por redesenho (ms) | CPU do processo em 50 s |
|---|---|---|---|---|---|---|
| 75 Hz, hoje | 75 | 60 | 3,6 | 6,0 | 4,3 | 29 s |
| 75 Hz, redraw | 75 | 75 | 3,1 | 5,3 | 3,7 | 31 s |
| 180, hoje | 172 | 60 | 3,6 | 6,1 | 4,2 | 33 s |
| 180, redraw | 173 | 173 | 2,4 | 4,3 | 2,2 | 47 s |
| 240, hoje | 232 | 60 | 3,2 | 4,8 | 2,2 | 35 s |
| 240, redraw | 223 | 223 | 2,1 | 3,7 | 2,6 | 54 s |

- O redesenho cabe: a 180 o período é 5,5 ms e o redesenho de 1 vista fica em 2–3,5 ms (pior média de 300: 5,9 ms, num trecho de 60/s);
  a taxa de apresentação não caiu. A 240 (4,2 ms) uma vista cabe e duas ficam no limite (perde ~4% das apresentações).
- O custo é CPU: +45% no processo a 180 (~0,3 núcleo a mais), +55% a 240.
- O tempo por redesenho cai com mais redesenhos por segundo (relógio da CPU/GPU mais alto); não é ganho do método.
- Não medido: a mistura em si (estimada abaixo de 0,5 ms), 3 e 4 jogadores (4 vistas).

Monitor principal de 180 Hz com vsync (mesmas lutas, `WOTM_MONITOR=0`):

| Rodada | Apresentações/s | Redesenhos/s | Redesenho 1 vista (ms) | 2 vistas (ms) | GPU por redesenho (ms) | CPU do processo em 50 s |
|---|---|---|---|---|---|---|
| 1 jogador, hoje | 179,6 | 60 | 3,0 | — | 2,7 | 30 s |
| 1 jogador, redraw | 177,1 | 177 | 2,7 | — | 2,4 | 52 s |
| 2 jogadores, hoje | 180,2 | 60 | 2,8 | 4,7 | 3,3 | 35 s |
| 2 jogadores, redraw | 175,9 | 176 | 2,3 | 4,6 | 3,2 | 68 s |

- Com vsync o redesenho também cabe: perde 1,5% das apresentações com 1 vista e 2,5% com a tela dividida (4,6 ms de 5,5).
- A CPU sobe mais que na simulação: +75% com 1 jogador e +90% com 2 (~0,45 e 0,65 núcleo a mais).

## 8. Etapa 1 implementada (2026-10-26)

Opção INTERPOLATE FRAMES (`WOTM_INTERP=1`): `Scene::showBlend` (wotm_native.hpp), relógio e par em `wotmDrawNative` (wotm_live.inc).
Diferenças em relação ao plano: o par passa a ter três instantâneos (P, A, B; P sem a cópia das partículas): com a margem de 3 ms, o
quadro novo chegava antes do instante exibido em ~22% das apresentações e o alfa ficava preso em 0 (o quadro anterior parado por uma
apresentação, passo do tempo exibido variando 0,7 a 0,9 field a 75 Hz); agora esse instante mistura P e A. A margem no corte por esfera
é o raio × 1,2. O teste de imagem bit a bit (`WOTM_INTERP_SELFTEST`) não foi feito.

Luta IA x IA (`WOTM_MATCH=3,1,1 WOTM_AIVSAI=1`), 35 s, `WOTM_INTERPLOG=1` (análise: passo do instante exibido entre apresentações,
comparado com o relógio):

| Rodada | Passo esperado (fields) | Passo medido | Erro p90 / p99 (fields) | Alfa preso em 1 | Cortes de câmera |
|---|---|---|---|---|---|
| 75 Hz, vsync | 0,8 | 0,8 em 95% | 0,03 / 0,38 | 4 de 2332 | 3 (intro) |
| 180 Hz, sem vsync | 0,34 | 0,3–0,4 em 99% | 0,05 / 0,10 | 10 de 5439 | — |
| 180 Hz, 2 jogadores | 0,34 | 0,3–0,4 em 93% | 0,09 / 0,25 | 11 de 4363 | — |

Redesenho de 1 vista a 180 Hz com a mistura: 2,8–3,4 ms (sem ela, etapa 0: 2,3–2,7). `tools\corpus.ps1 -Check` sem mudança (o replay
não interpola). Tela dividida com a mistura: `docs/screenshots/2026-10-26/2026-10-26_interpolacao_tela_dividida.png`.

## 9. Etapa 2 implementada (2026-10-26)

`Particles::stepBack`/`blendRam` (wotm_fx.hpp) e o bloco de partículas do `Scene::showBlend`. Diferenças em relação ao plano da seção 5:
- **Tamanho:** o plano recuava `pos.w` por `vel.w`, mas no `particleMove` o `vel.w` é o passo da cor (vis 1 e 6), e o tamanho cresce
  pelas taxas de expansão. `pos.w` fica o de B.
- **Gravidade:** entra na velocidade depois do movimento, então sai do recuo (`f·(vel − (0, 0, g·f))`). O vento (`accel`, com o `Gusts`)
  entra.
- **Nascidas no passo:** o critério é o `newParticles` (+0x2EDE), não a idade. As do fim da lista ficam em B.
- **Destroços:** guardam o passo no `Sprite` na coleta, porque a chave `fx·4096 + índice` não casa entre quadros.
- **Matriz:** a `worldToScreenMat` da cópia vira `M_B·C_Bᵀ·C_I`, e não uma matriz refeita do zero com 384 e 180,48: assim vale para
  qualquer foco e vista.

Checagem embutida no `WOTM_INTERPLOG` (`[interp] recuo`). Cada partícula que andou é recuada um passo inteiro (α = 0) e comparada com a
mais próxima do mesmo sistema em A; a matriz é refeita com a câmera de A e comparada com a de A. Lutas IA x IA de 45 s
(`WOTM_MATCH=3,1,1`, 180 Hz):

| Rodada | Partículas (erro médio / sem recuo) | Destroços (idem) | Matriz (dif. relativa máx.) |
|---|---|---|---|
| 1 jogador | 430 mil: 0,001 / 1,05 | 616 mil: 0,002 / 2,68 | 0,00000 |
| 2 jogadores | 523 mil: 0,001 / 1,42 | 594 mil: 0,000 / 3,15 | 0,00000 |

Os poucos pares com erro de 0,4 a 4,7 têm atração, órbita ou caixa (forças somadas depois do movimento), ou são o vizinho errado da
busca. Custo: o `show` sobe ~0,1 ms por redesenho (a cópia de ~1,3 MB de `fx[]` e das tabelas), de 0,05–0,08 para 0,13–0,18 ms. O
redesenho todo fica nos mesmos 2–4,5 ms da etapa 1. A checagem do log é quadrática e custa até ~0,7 ms, então tempo se mede sem ela.
`tools\corpus.ps1 -Check` sem mudança. Foto: `docs/screenshots/2026-10-26/2026-10-26_interpolacao_particulas.png` (sopro de fogo
com a mistura). Ficam para conferir na tela: o fogo preso ao monstro andando (o emissor não recua) e o tiro do Robo-47.

## 10. Etapa 3 implementada (2026-10-26)

`Scene::blendFlares`, `anchorShift`/`blendAnchors` e o fim do `Scene::showBlend` (wotm_native.hpp); na tela dividida, as cópias
`barsMix`/`flaresMix` do laço da apresentação (wotm_live.inc). Diferenças em relação ao plano da seção 5:
- **Foco:** vem da `worldToScreenMat` da vista na cópia de sombra de B (parte das linhas 0 e 1 perpendicular à frente), escalado pela
  razão das tangentes do FOV para a câmera misturada. Sem a cópia (`WOTM_FXLIVE`), pelo FOV.
- **Ícone de vida:** reprojetar só a câmera não bastou. Ele segue a cabeça do monstro, que anda entre A e B, e a câmera o persegue: o
  erro ficou em 7,0 px, igual a não fazer nada. O ponto do mundo passou a ser misturado com o do ícone de A mais próximo. Sobrou o
  ícone que o jogo prende na borda da tela (inimigo fora da vista), que não é projeção: o critério é a diferença entre o ícone e a
  projeção do seu ponto ser a mesma em A e em B (até 3 px); se não for, mistura a posição na tela.
- **Fades:** o k-ésimo retângulo que não é barra, com o mesmo lugar, cor e textura em A e B, mistura o alfa. O `Shell::FadeScreen` é
  um laço sem captura e fica de fora.

Checagem embutida no `WOTM_INTERPLOG` (`[interp] hud`): os flares e ícones de B levados para a câmera de A (α = 0) contra os que o
jogo pôs em A. Lutas IA x IA de 45 s (`WOTM_MATCH=3,1,1`, 180 Hz):

| Rodada | Flares (erro médio / sem reprojetar) | Ícones de vida reprojetados (idem) | Ícones pela tela |
|---|---|---|---|
| 1 jogador | 61: 0,005 / 2,65 px | 1284: 0,58 / 4,39 px | 434 |
| 2 jogadores | 57: 0,003 / 1,98 px | 2252: 0,52 / 2,80 px | 0 |

O meio pixel dos ícones é o arredondamento do jogo (o centro do ícone sai em px inteiros). No ícone, a checagem é em parte
construída: o critério dos 3 px já escolhe os que batem; ela mostra que o foco e as câmeras estão certos. Custo: o `show` fica em
0,14–0,19 ms, o mesmo da etapa 2; redesenho todo de 3,2–3,6 ms na tela dividida. `tools\corpus.ps1 -Check` sem mudança. Foto:
`docs/screenshots/2026-10-26/2026-10-26_interpolacao_barras.png` (barras com `WOTM_ENEMYBARS=1` e a mistura). Fica para a etapa 4 o
ícone do jogo (sem barras) na tela dividida, que sai na lista de polígonos da janela inteira.

## 11. Etapa 4 implementada (2026-10-26)

Boa parte da lista da etapa 4 já tinha caído na etapa 1: cada vista da tela dividida mistura com o próprio par (`S.prev.views`); a
vista sem par (divisão ligando, vista unificada do agarrão: `vctx` diferente) sai como B; pausa (F7), câmera livre, foto, diálogos
(buraco de fields), carga (`g_loadFreeze`, sem captura) e `Shell::FadeScreen` (laço sem captura: alfa para em 1) desligam a mistura
ou a deixam em B. O que entrou agora:
- **Ícone de vida do jogo na tela dividida:** sai nos polígonos da janela inteira, montados do instantâneo da vista 0; o laço desloca
  uma cópia (`polysMix`) vista a vista, como as barras, e a devolve à `Scene` depois da vista 0.
- **Presos à câmera fora das listas do HUD** (risco da seção 6): em vez de listar os `_cs` no log e tratá-los à mão, o `mix` do
  `showBlend` testa a pose relativa à câmera (`M·C⁻¹`) em A e em B; igual (1e-3 nos eixos, 0,02 + 1e-4·|t| na posição), vai com a
  câmera misturada. É exato para o que está preso e não muda o resto (a pose relativa só bate se o objeto anda junto com a câmera).
  Contagem no `[interp] hud` ("objetos presos à câmera"): na luta, 15–20% dos pares têm alguns (até 27 em tela cheia, 49 com dois
  jogadores); na tela de título, 2 por quadro.
- **texId 0:** fica a apresentação anterior. O CLOAK e o mapa de ambiente de vídeo não acumulam além do que o PS2 já acumula, e a
  apresentação anterior está mais perto no tempo que o quadro anterior do jogo.

Conferido: lutas IA x IA de 45 s a 180 Hz com 1 e 2 jogadores (flares 0,005 / 0,003 px, ícones 0,54 / 0,51 px, como na etapa 3);
menu com `WOTM_SKIPINTRO=1` e a mistura (9089 apresentações, sem corte, fotos normais). O roteiro `PS2X_PAD` não passa do menu
principal nativo, então as transições de câmera entre as telas do telão e a escolha de lados ficam para o dono conferir.
`tools\corpus.ps1 -Check` sem mudança.

## 12. Etapa 5 implementada (2026-10-26)

A opção INTERPOLATE FRAMES vem ligada (`wotm_settings.hpp`, `kDefault`). Diferenças em relação ao plano da seção 5:
- **Caixa no launcher:** não entrou. As opções de vídeo moram no painel (aba VIDEO); o launcher só tira do ambiente as variáveis
  delas, e o `WOTM_INTERP` passou a estar nessa lista. No painel a linha deixou de ficar apagada a 60 Hz, porque agora vale também
  com o jogo a 30.
- **Jogo a 30:** com a apresentação até 60 Hz, liga quando metade ou mais dos últimos 64 quadros novos tiveram vão de 2 fields e
  desliga com até 8. Nas lutas e no menu medidos aqui o jogo não cai para 30 (99,6–99,8% dos quadros com vão de 1; nenhuma
  sequência de vãos de 2), então a regra não foi vista agindo.
- **Desenho que não cabe:** média móvel (5%) do tempo do bloco de redesenho, que termina antes do `EndDrawing` (sem a espera do
  vsync), com cada amostra limitada a dois períodos; acima de 85% do período suspende, abaixo de 60% volta. A primeira versão
  partia do primeiro redesenho (~100 ms, as texturas) e suspendia no começo de toda luta; a média agora parte de zero.

Conferido sem `WOTM_INTERP` no ambiente, `WOTM_SETTINGS=0` (lutas IA x IA de 45 s):

| Rodada | Redesenhos misturados | Suspensões |
|---|---|---|
| 180 Hz, 1 jogador | 6895 de 6897 | nenhuma |
| 180 Hz, 2 jogadores | 2512 de 4041 | uma, de ~8 s: a tela dividida desenhou em 4–5,7 ms nesse trecho (período de 5,56) |
| 60 Hz, 1 jogador | 0 (o jogo a 60) | — |
| 1000 Hz, 1 jogador | 0 | suspensa logo no começo (período de 1 ms) |

`tools\corpus.ps1 -Check` sem mudança (o replay não interpola).
