# LOD fixo nas "Recompiled options": investigação

2026-10-23. Só leitura (código, asm, docs); nada compilado, nada aberto. Pergunta do dono: dá para ter uma opção no painel que
fixe o nível de detalhe (LOD) dos modelos 3D, sempre o baixo ou sempre o alto, além do automático do jogo? Legenda: **[C]**
confirmado (código + dado ou doc já validada), **[H]** hipótese a conferir.

**Resposta curta:** dá, e é barato. O port já escolhe o LOD por conta própria no desenhista nativo (`Scene::walk`, caso 2 do
`native/wotm_native.hpp`), reproduzindo a regra do jogo; o jogo não guarda a escolha em lugar nenhum e nada da lógica depende
dela. A opção vira um modo da `Scene` (automático / alto / baixo) passado só nas coletas ao vivo. O `Scene` até já tem o gancho
(`collect(ram, maxLod)`, usado pelo `viewer --lod n` e pela tecla L), mas com duas falhas que a opção precisa corrigir (abaixo).

## Contexto
- A cena desenhada no PC é coletada da RAM do EE a cada quadro pelo `Scene::collect` (`native/wotm_native.hpp`), na thread do EE
  (`wotmCaptureFrame`, `native/runtime/wotm_live.inc`), e desenhada pela GPU. O jogo recompilado continua percorrendo a hierarquia
  (`hier`) e montando os pacotes do VU1, que no `wotm.exe` (`PS2X_HW_EMU=0`) ninguém desenha; no `ps2EntryRunner.exe` eles são a
  referência da checagem VU1 x nativo (`docs/passos/VU1CHECK.md`).
- O `POS_FRONTEIRA.md` já registra a ideia ("Modo Nintendo DS: LOD sempre no nível mais alto; o LOD é escolhido no nativo").
- Opções do painel: chave em `native/wotm_settings.hpp` (`enum Key`, `kEnv`, `kDefault`, `g_val`, linha no `settings.ini`), linha
  na aba em `kTabDefs` e valores em `choices()` no `native/runtime/wotm_mainmenu.inc`.

## O que se sabe

### Símbolos e endereços (`native/wotm_syms.h`)
| Símbolo | Endereço | Papel |
|---|---|---|
| `hierTraverseAsm` | `0x207198` (0x11E8, asm escrito à mão, `asm/nonmatchings/common/texm/`) | travessia de desenho; **o caso LOD (opcode 2) começa em `0x207820`** e escolhe o nível ali mesmo |
| `hierLod__FP8_hierlodfPP9_hierheadP10_hierstack` | `0x204768` (0x188) | versão em C da mesma regra (`src/common/hier.cpp`, `NON_MATCHING` 42/98). **[C] Ninguém a chama:** não há `jal` nem ponteiro para ela no asm nem nos dados; é código morto |
| `shadowLodRangeSq` | `0x6F87C0` | "distância²" fixa usada no lugar da distância ao olho quando a travessia é a da sombra |
| `psBlockerSetLodRange__FP19_pshadowblockerlistf` | `0x208F18` | grava `lodRangeSq` (`psBlkLst + 0x30*i + 0x28`); `psGetBlockerLodRangeSq__Fi` (`0x2097D8`) lê |
| `hierShadow__Fii` | `0x204430` | para cada bloqueador de sombra copia `psGetBlockerLodRangeSq(i)` para `shadowLodRangeSq` e chama `hierTraverseAsm` |
| `hierClipRange` | `0x6F87AC` | 5,0 no `hierInit`; recorte, não LOD |
| `hierSetSwitch__FP11_hierswitchi` | `0x204E68` | nó SWITCH (`whichChild`): troca de estado, **não é LOD** |

### O nó LOD (`include/hieri_types.h`, `docs/FORMATOS.md`)
`_hierlod` (opcode 2): `numLods` em `+4`, `ctx` (estado do GS) em `+8`, `lodCenter` em `+0x10`, `radius` em `+0x1C` e a lista
`lods[i]` em `+0x20`, 16 bytes cada: `fade` (`+0`), `switchOutDis` (`+4`, **distância ao quadrado**), `child` (`+8`). Índice 0 é o
mais grosseiro (o de maior alcance), o último o mais detalhado.

### Como o jogo escolhe hoje [C]
No caso LOD do `hierTraverseAsm` (`0x207820`):
1. O centro do LOD (`+0x10`) vai ao VU0 (microrrotina em `0x398`), que devolve a distância² ao olho da vista sendo desenhada.
2. Se a travessia é a da sombra (`tracingShadow`), essa distância é trocada pela `shadowLodRangeSq` (cópia no scratchpad feita na
   entrada da função).
3. Percorre os níveis do último para o primeiro e pega o **maior índice com `distância² < switchOutDis`**. Nenhum: o nó não
   desenha nada (é também o recorte por distância do objeto).
4. Cross-fade: se o `fade` do nível escolhido é > 0 e a distância passou dele, `t = 1 - (d - fade) / (switchOutDis - fade)`; com
   `t > 0,5` desenha o nível i inteiro e empilha o i - 1 com peso `(1 - t) * 2`, senão o i - 1 inteiro e o i com peso `2t`; sem
   i - 1, o i com alfa `t` (some aos poucos no limite). A regra é a mesma do `hierLod` em C.

Portanto **o critério é só a distância ao olho** (por vista), não o tamanho na tela, o FOV, a resolução, o número de jogadores
nem a carga. A escolha **não fica guardada**: vive em registradores e na pilha do scratchpad durante a descida, e some no fim do
quadro. Os `switchOutDis` vêm do arquivo do modelo (`dbsRelocate*`); **[H]** nada no código decompilado os altera em tempo de jogo
(não achei escrita; o nativo os lê da RAM a cada quadro, então respeitaria se houvesse).

### O que tem LOD de verdade
- **Monstros [C]:** `CS` → GROUP → `CHAR_INSTANCE` → `SKEL_BONE` + `LOD` (`NATIVO.md`, "Hierarquia, LOD e personagens"). Fotos de
  2026-10-11 mostram jogador no LOD 3 e inimigo nos LODs 0 a 2 (`docs/screenshots/2026-10-11/`), então há pelo menos 4 níveis em
  alguns. O esqueleto (`SKEL_BONE`) é irmão do LOD: o osso é o mesmo para todos os níveis. Casos especiais conhecidos: o traje 3
  do Togera tem um **último nível com `switchOutDis` 0**, nunca desenhado, que é uma malha grosseira em A-pose (`MODELOS_3D.md`);
  o Dragon tem dois objetos skinned no mesmo nível (corpo e asas); o chefe final (CAPITOL) usa paleta de 32 matrizes.
- **Prédios e cenário [C]:** sim, com cross-fade (o caso `luta_hud` do commit `5aa37ec`: prédio `d51400` sob o `d51c40` com fade
  0,21; `VU1CHECK.md`, pista 7). `tools/gltf.py --lod 0` exporta o cenário no nível grosseiro, o que confirma LODs nas fases.
- **Destrutíveis [C]:** `DESTRUCTIBLE`/`INTERACTIVE_STATE` escolhem o **estado** (intacto, ruína), não o detalhe. Os LODs que
  houver abaixo de um estado seguem a regra comum.
- **Partículas: sem LOD [C para nós da hierarquia, H para o resto].** Partículas são sprites do sistema de efeitos
  (`wotm_fx.hpp`; pedestres são fx 91–99); nenhum símbolo com lod/detail no sistema de partículas. Há recorte por vista
  (`Debris::CullView`, `0x12F810`), não troca de detalhe. Emissores (op 22) e luzes com lens flare (op 5) que estejam dentro de
  um nível de LOD só aparecem quando aquele nível é percorrido.
- **Colisão: não depende do LOD [C].** `src/common/hdTraverse.cpp` desce sempre por `lods[0].child` (o grosseiro).
- **Sombra:** o jogo usa um LOD **fixo por bloqueador** (o que o objeto teria a 1200 unidades: `psBlockerAdd` grava 1200²;
  o chefe final usa 20000², `src/game/FinalBoss.cpp`), não o da câmera.

### O que o desenho nativo faz [C]
`Scene::walk`, caso 2 (`native/wotm_native.hpp`, perto da linha 3917) **refaz a regra do jogo**: transforma o centro, mede a
distância² ao olho da câmera do jogo daquela vista (`readGameCamera`), pega o maior índice com `d² < switchOutDis`, reproduz o
cross-fade na ordem do `hierTraverseAsm` (i - 1 antes de i; `DrawItem::fade` e `lodX` para a OT de alfa). Não lê nenhuma escolha
do jogo. Já existe o modo forçado: `collect(ram, maxLod)` com `maxLod >= 0` grava `lod_`, e o caso 2 então usa
`min(lod_, numLods - 1)` sem cross-fade. Só o `viewer.cpp` o usa (`--lod n`, tecla L); o runtime chama `collect(ram)` (automático)
em `wotm_live.inc` (captura por vista, quadro sem `hier`, coleta do quadro e coleta da câmera livre) e em `wotm_vu1check.inc`
(`collectNative`).

Outros consumidores do nível no nativo:
- `Scene::palette`: a paleta de ossos é `animPktPtr[OBJECT + 0x68]`, índice do próprio objeto, não do LOD; só cai no `lodIdx`
  quando `+0x68` é negativo.
- `Scene::collectShadows` usa o mesmo `walk` (`drawCs`), logo **usa a distância ao olho**, não a `shadowLodRangeSq` do jogo
  (pequeno desvio já existente, sem bug relatado).
- `wotm_model.hpp` (monstro 3D da lista de saves, direto do disco) já usa sempre o mais detalhado que o jogo desenharia de perto:
  o maior índice com `switchOutDis > 0`.
- Correção de doc: o `NATIVO.md` ainda diz "o `fade` entre LODs não é reproduzido"; é reproduzido desde o `5aa37ec`.

## Onde intervir
| Ponto | Avaliação |
|---|---|
| Gancho no `hierLod` | **Não serve:** é código morto; a escolha real está no meio do `hierTraverseAsm`. |
| Gancho/patch no caso LOD do `hierTraverseAsm` | Possível no C++ gerado, mas é costura no PS2Recomp (regenerar `tools/runtime_patch/wotm-runtime.patch`), mexe nos pacotes do VU1 (a referência da checagem) e não muda nada na tela do `wotm.exe`, que desenha pelo nativo. Descartado. |
| Escrever nos `switchOutDis` da RAM | Muda dado do jogo, contamina instantâneos e o replay, e o cenário da fase inteira teria de ser varrido. Descartado. |
| **Desenhista nativo (`Scene::walk`, caso 2)** | **O ponto limpo.** A escolha já é nossa; a opção só muda a regra. Nada do jogo lê o resultado. |

## Desenho proposto
- Chave nova `Lod` em `wotm_settings.hpp`: `WOTM_LOD`, valores `0 = auto` (padrão, como o jogo), `1 = high`, `2 = low`; linha
  `LOD=` no `settings.ini`.
- Na `Scene`, trocar o `lod_` (índice cru) por um **modo** (`LodAuto`, `LodHigh`, `LodLow`, e o índice forçado do viewer, que
  continua existindo como diagnóstico). Regra dos modos forçados:
  - **Manter o recorte do jogo:** só desenha se o automático desenharia algum nível (`d² < switchOutDis[0]`, o maior alcance).
    Sem isso, objetos que o jogo some por distância (detalhe miúdo, nós de LOD de nível único usados como recorte) apareceriam
    no horizonte. O `maxLod` de hoje não faz isso.
  - **Alto** = o maior índice com `switchOutDis > 0`, não `numLods - 1` (o nível 0 do traje 3 do Togera; a mesma regra do
    `wotm_model.hpp`). O `min(lod_, n - 1)` de hoje pegaria a A-pose.
  - **Baixo** = índice 0.
  - Sem cross-fade entre níveis (o nível é constante); mas no limite do recorte, quando o automático estaria no nível 0
    sumindo (fade com alfa `t`), aplicar o mesmo `t` ao nível forçado, para o objeto não estalar ao sumir.
- A opção entra **só nas coletas ao vivo** (`wotm_live.inc`), por um setter da `Scene` lido de `settings::g_val` a cada quadro. O
  `collectNative` da checagem e o replay `--vu1cmp`/corpus continuam no automático (não passam o modo), como as opções gráficas.
- Sombras: com modo forçado, a silhueta usa o mesmo nível (sai de graça do `walk`). À parte, e opcional: no automático, usar a
  `lodRangeSq` do bloqueador (`psBlkLst + 0x28`) em vez do olho, como o jogo.
- Painel: a aba VIDEO está cheia (`kMaxOpts = 8`, e o layout põe as 8 ao lado da prévia). Duas saídas: subir `kMaxOpts` para 9 e
  ajustar o `panelLayout` e a prévia (`wotm_preview.hpp`, imagem nova "LOD"), ou pôr em FEATURES (3 linhas). Rótulo sugerido:
  `MODEL DETAIL` com `AUTO / HIGH / LOW` e dica "HIGH KEEPS FULL DETAIL AT ANY DISTANCE. AUTO SWITCHES LIKE THE PS2". Decisão do dono.

## Plano por etapas
0. **Inventário dos níveis (só leitura do disco):** com `tools/ngp.py`, listar por `.NGP` (MON e LVL) quantos nós LOD, quantos
   níveis, os `switchOutDis` e se algum nível tem `child` nulo, `switchOutDis` 0 ou fora da ordem decrescente. Responde "os
   níveis baixos existem em todos os modelos?" (hoje só sei dos casos acima) e acha outros Togera-traje-3.
1. **Paleta dos níveis não desenhados pelo jogo [H, principal risco]:** o jogo só monta o pacote do VU1 do nível que escolheu.
   Se o bloco da paleta apontado por `animPktPtr[+0x68]` de um nível não escolhido não for atualizado no quadro, o skinning sai
   com matrizes velhas (malha "explodida" ou congelada). Conferir antes de tudo: `WOTM_LOD=1` / `2` com inimigo longe e perto,
   `WOTM_LISTSKIN=1` (nó, LOD, `animPkt`) e comparar as matrizes do bloco entre dois quadros. Se falhar, o nativo calcula a
   paleta a partir do esqueleto (o caminho do `wotm_model.hpp`, que anima direto do disco) ou copia a do nível desenhado quando
   os objetos têm o mesmo mapa de ossos.
2. Modo na `Scene` (regras acima) + chave nas settings + `WOTM_LOD` na tabela do `NATIVO.md` (`python tools/wotm_env.py`).
3. Painel (aba e prévia, conforme a decisão).
4. Docs: `NATIVO.md` (LOD, corrigir a frase do fade), `POS_FRONTEIRA.md`.

## Armadilhas
- `numLods - 1` não é "o mais detalhado" (Togera traje 3). Usar `switchOutDis > 0`.
- Sem o recorte, "alto" desenharia objetos que o jogo tira por distância; "baixo" também (o nível 0 aparece até o infinito).
- Paleta de ossos dos níveis que o jogo não escolheu (etapa 1). O Dragon (dois objetos no mesmo nível) e o CAPITOL (32 matrizes)
  são os testes de regressão da paleta.
- Emissores e luzes de lens flare dentro de um nível: o nativo os coleta no `walk`; forçando outro nível, uma luz que só exista
  no nível alto passaria a aparecer de longe (ou sumir no baixo). As partículas em si são do jogo e não mudam.
- O cross-fade do automático depende do `eye_` da vista; com tela dividida cada vista tem o seu (já é assim). A opção vale igual
  para todas as vistas.
- Checagem VU1: se o modo vazar para o `collectNative` (por exemplo, lendo `settings::g_val` dentro do `collect`), a checagem ao
  vivo e o corpus passam a acusar diferença em todo objeto longe. O replay fica no ambiente (`wotm_settings.hpp`, inicialização
  estática), então um `WOTM_LOD` esquecido no ambiente também vazaria: o `collectNative` deve forçar o automático.
- Modo foto e câmera livre (`collect (camera livre)`) usam o mesmo `walk`: decidir se seguem a opção (sugestão: seguem; o modo
  foto talvez até force "alto").
- Custo: "alto" decodifica malhas a mais uma vez (cache por nó em `Scene::mesh`) e desenha mais triângulos na cidade inteira;
  irrelevante para GPU de PC, mas vale olhar o tempo da coleta (`g_walkMs`) num lote.

## "Sempre alto" como melhoria de PC
Faz sentido: o PS2 trocava por distância só para caber no VU1/GS. Em resolução alta e com render scale, os níveis grosseiros e o
cross-fade (que deixa prédios meio translúcidos na transição) ficam mais visíveis do que na TV. Com o recorte mantido, "alto" não
muda o que aparece, só a forma. Sugestão de padrão: continuar `auto` (o jogo como era; e os testes automáticos ficam comparáveis),
com "high" recomendado na dica. "Low" serve mais como curiosidade/estilo (o "modo DS" do dono, ao contrário) e para PCs fracos.

## Como testar
- Antes: `tools\corpus.ps1 -Check` para a base; depois, `-Check -Baseline <csv>` tem de sair **idêntico** (o replay ignora a opção).
- `run_live.bat` com `WOTM_LOD=1` e `=2`: monstros perto e longe (Togera traje 3, Dragon, Robo-47 com o lens flare, Congar),
  prédios no ponto do cross-fade (o caso `luta_hud`), limite do recorte no horizonte; tela dividida com 2 e 4 jogadores; sombras.
- Trocar no painel durante a luta e conferir que vale no quadro seguinte e que o `settings.ini` grava.
- `viewer ram.bin --lod n` continua como diagnóstico de índice cru.
- Lote curto (`tools\lote.ps1`) com `WOTM_LOD=1` para ver o tempo de coleta e nenhum erro do vigia.

## Estimativa
- Etapa 0 (inventário): 1 a 2 h.
- Etapa 1 (paleta): 1 h se os blocos forem atualizados para todos os níveis; meio dia a um dia se o nativo tiver de montar a paleta.
- Etapas 2 a 4: 2 a 4 h (o código de escolha é ~30 linhas no caso 2; o resto é settings, painel e prévia; mais se a aba VIDEO
  ganhar a 9ª linha).
- Total provável: **meio dia a um dia e meio**, sem tocar no jogo recompilado nem na ROM (sem gate).
