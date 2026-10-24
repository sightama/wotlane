# Luz do monstro 3D na seleção e na lista de saves (investigação, 2026-10-24)

Pergunta do dono: como fazer o monstro 3D da seleção nativa (`wotm_padsel.inc`) e da lista de saves (`wotm_savepics.hpp`), os dois
desenhados pelo `wotm::model::Renderer` (`native/wotm_model.hpp`), ficar iluminado como o jogo ilumina. Só investigação: nenhum
código mudou.

Grau de certeza: **[A]** conferido no código ou em dados (disco, RAM de instantâneo); **[M]** deduzido do código sem conferir na tela;
**[B]** palpite.

## Resumo

- O `Renderer` não usa nenhuma luz do jogo. Ele tem uma direcional fixa, presa à câmera, mais um piso de 0,7 aplicado a **todos** os
  vértices com normal, e corta o resultado em 1,0× a textura. **[A]**
- No jogo, a luz de um monstro na luta vem de três luzes paralelas por fase, fixas no mundo, e de um especular por objeto. Não há termo
  ambiente. Só os OBJECTs com o bit `dirLight` recebem luz; os outros saem com a cor de vértice crua. A cor pode chegar a ~2× a
  textura. As luzes saem de um arquivo pequeno do disco, `RST/<FASE>.MRS` (chave 5), e o Shell tem o seu, `RST/SHELL.MDR`. **[A]**
- A seleção do retail **não tinha monstro 3D**: as telas 3, 6, 18 e 19 só mexem em switches de retratos, painéis de traje e no filme
  do telão. Não existe "luz da seleção do jogo" para copiar. O que existe no Shell é a luz do `SHELL.MDR`: uma branca quase a pino
  e duas de preenchimento bem fracas, pensada para os poucos objetos iluminados da cena do menu. **[A]** para as chamadas, **[M]** para
  "nenhum monstro 3D".
- **Recomendação:** levar para o `Renderer` a fórmula do VU1, que o nativo da luta já reproduz em `Scene::vertexRgb`, com as luzes
  passadas por parâmetro (`ParaLights`). Como fonte das luzes, ler a chave 5 de um `.MRS` do disco: na seleção, uma fase fixa
  (sugestão: `CENTRAL.MRS`, ou a fase escolhida/última jogada); na lista de saves, o `.MRS` da fase do save. Ler o `paraL` da RAM viva
  fica como opção só para a lista de saves aberta dentro da luta. Custo estimado: 1 a 1,5 dia. Os detalhes estão na seção 3.

## 1. Como o `Renderer` ilumina hoje

- `native/wotm_model.hpp:915`: o comentário já diz "uma direcional fixa + ambiente, por vértice, sobre a cor de vértice do pacote (o
  jogo usa as luzes da fase)". **[A]**
- `wotm_model.hpp:972-975`: a direção `L` é calculada a partir do `yaw` da câmera (`yaw - 0,7` rad, componente vertical 0,8): uma luz
  de cima, vinda da frente-esquerda **da câmera**. Ela gira junto com a câmera e não depende da fase. **[A]**
- `wotm_model.hpp:1010-1013`: por vértice, `d = max(0, n·L)` e `lit = 0,7 + 0,6·d`. Normal nula dá `lit = 1`. A cor enviada é
  `min(255, cor_do_vértice · 2 · lit)`, ou seja, em unidades de 0..1 do GL, `cor/128 · lit`, **cortada em 1,0**. O alfa sai `a·2`.
  **[A]**
- `wotm_model.hpp:751-792` (`Model::build`): a normal gira pelo osso x (nos skinned) e pela matriz do item e depois é normalizada,
  como em `Scene::vertexRgb`. Já o `PVtx` (`:284-296`) não guarda nada do objeto além do estado do GS. Ficam de fora:
  - o bit `dirLight` (0x100 das flags em +0xC);
  - o multiplicador de cor do OBJECT (`redAnim..alphaAnim` em +0x40, ou os canais de animação, o `vf31`);
  - `specLightMe` / `specIntensity` (+0x6C);
  - o mapa de ambiente.

  Por isso todo vértice com normal é "iluminado", mesmo os que no jogo não recebem luz. **[A]**
- O shader (`:1067-1079`) multiplica textura × cor do vértice × `gain` (`setGain(1)`). O `gain` existe mas fica em 1. **[A]**

Consequências visíveis:
- O monstro nunca passa de 1,0× a textura. No jogo, com as luzes das fases (cores de 1,0 a 2,0 por luz, somadas), as partes viradas
  para a luz chegam perto de 2×. **[A]** para a fórmula, **[M]** para o efeito na tela.
- O piso de 0,7 impede lados escuros: o monstro fica "chapado". **[A]**
- Objetos sem `dirLight` saem iluminados assim mesmo. Exemplo forte: no `ENERGY0.NGP` (Kineticlops) só 120 dos ~4.250 triângulos
  têm `dirLight`. O corpo, no jogo, é só cor de vértice. Contagem feita com `tools/ngp.py`, LOD mais detalhado:

  | Arquivo | Triângulos | Com `dirLight` | Skinned com `dirLight` |
  |---|---|---|---|
  | `MONKEY0` | 4.455 | 3.279 | 3.279 |
  | `ROBOT0` | 5.339 | 3.364 | 1.620 |
  | `ALIEN0` | 5.922 | 4.833 | 2.432 |
  | `ENERGY0` | 4.246 | 120 | 0 |

  Nenhum monstro tem objeto com luz pontual. **[A]**

## 2. Como o jogo ilumina

### 2.1 Na luta (o que o nativo já reproduz)

- **Fórmula do VU1** (programa 0x1010, objetos com `dirLight`), reproduzida em `native/wotm_native.hpp:3737-3750` (comentário e
  `readParaLights`) e `:3957-3990` (`Scene::vertexRgb`). A normal crua (módulo ~127) gira pelo osso x e pelo objeto. Para as três
  luzes paralelas e para o vetor-meio do especular, `d_i = max(0, n·L_i / 4096)` (L com módulo 32). Então:
  `cor = min(255, cor_do_vértice · mul · Σ_{i=0..2} cor_i · d_i + especular · d_3^16)`. **[A]**
  - `cor_i` = RGB da luz / 128 (`vu1LightColor`, `D_0025B238`; `src/common/plight.cpp:71-75`). **[A]**
  - Não há termo ambiente. O `viewSetAmbient` existe e a UFO tem ambiente 15, mas esse valor não chega ao VU1. Está documentado em
    `wotm_native.hpp:3674-3677`: aplicado, a UFO ficava 2,13× mais escura que no PCSX2. Sem `dirLight`, `cor = cor_do_vértice · mul`
    (`:3963-3965`). Com mapa de ambiente, o programa também não aplica a luz paralela (`:3918-3925`). **[A]**
  - Especular: `especular = specIntensity` (+0x6C) do objeto com `specLightMe` (0x2000). Sem o bit, vale o que o objeto anterior
    deixou na memória do VU1 (`:4144-4148`; os pedaços do Togera dependem disso). O vetor-meio é recalculado por vista em
    `pLightCompSpecular` (`plight.cpp:103-126`): `normalize(L0 + e)`, com `e` = a matriz da câmera aplicada a (0, -1, 0). O
    `specularColor` global é 0 no retail (`plight.sdata`) e ninguém o muda: a linha 3 de `vu1LightColor` fica zerada. **[A]**
  - Abaixo de um nó SCALE/ANIM_SCALE não uniforme, o hier usa outra tabela de luz (`Scene::lightScale`, `:3559-3603`; peças do
    Togera). **[A]**
  - O GS modula a textura pela cor do vértice com 0x80 = 1,0. Até 255 (~2×) é permitido. **[A]**
- **De onde vêm as luzes.** `paraL` (0x6DFC00) guarda as direções em colunas, ×32, e `vu1LightColor` guarda as cores/128. Os dois são
  montados por `plightUpdateParaLights` (`plight.cpp:52-95`) a partir de `paraLightR/G/B/H/P` (heading e pitch em graus). Quem grava
  esses valores é o leitor de "reset files", `resetcom::readKey`, chave 5 (`src/common/resetcom.cpp:76-79` e `:200-206`). **[A]**
  - Formato do arquivo: cabeçalho `{float 'KIRR', versão 2,0, criador}` e chaves `{tipo, tamanho, dados}` até a chave 1. Os dados da
    chave 5 são cinco blocos de três floats: R[3], G[3], B[3], H[3] e P[3]. Outras chaves: 2 = fundo, 3 = ambiente, 4 = neblina,
    7 = hora do dia; na fase, `0xBC2` = `dustLighting` (`src/game/reset.cpp`). **[A]**
  - Caminho (`resetcom::getFileAndPathname`, `:276-311`): fases em `\RST\<fase>.MRS` (criador 4) e o Shell em `\RST\shell.MDR`
    (criador 2). O Shell lê o seu em `Shell::InitialMemCardScreen` (`src/game/Shell.cpp:1989-1993`) e de novo em `userintMain`
    (`src/game/ui.cpp:290-292`), a cada volta ao menu. A fase lê o dela na carga (`Shell.cpp:277`, `setFileName(whichLevel)`). **[A]**
  - A hora do dia (`todActive`) trocaria as luzes, mas a chave 7 de todos os arquivos olhados vem com `tod 0`. **[M]**
- **Direção a partir de h e p.** Conferido em RAM de instantâneos do corpus (`<runtime>\corpus\*.vu1snap`, RAM lida pelo padrão do
  ELF): a coluna `i` de `paraL` / 32 é exatamente `mathfHPtoVector(h, p) = (cos p · sin h, cos p · cos h, sin p)`, e a cor é RGB/128.
  Bateu em METRO (`agamo_fogo`), AIRPORT (`airport_ceu`), CENTRAL (`luta_centro`) e SHELL (`menus`, `shell_ceu`), em todas as luzes.
  O vetor aponta **para** a luz: `d = n·L` positivo nas faces viradas para ela. **[A]**
- **Luzes fixas no mundo.** `hierTraceWorld` copia `paraL` para a tabela `lightDir` e `hierCsUpdate` a leva ao espaço de cada `_cs`
  (`src/common/hier.cpp:174` e `:250`). Assim, a luz que um monstro recebe depende para onde ele está virado na fase. Não existe
  "a" luz do monstro: existe a da fase, girando com o heading dele. **[A]**

Luzes de algumas fases (chave 5; h e p em graus; RGB com 128 = 1,0):

| Arquivo | Luz 0 | Luz 1 | Luz 2 |
|---|---|---|---|
| `CENTRAL.MRS` (= `TRAINING`, `BIGSHOT`, `CRUSH`) | (190,140,110) h 120 p 45 | (170,170,225) h −120 p 45 | (225,170,160) h 0 p 25 |
| `METRO.MRS` | (250,230,160) h 120 p 45 | (170,130,110) h −120 p 45 | (200,190,130) h 0 p −50 |
| `TOKYO.MRS` | (180,180,255) h 120 p 45 | (200,130,210) h −120 p 45 | (150,100,150) h 0 p −50 |
| `UFO.MRS` | (150,150,200) h 120 p 45 | (250,140,155) h −120 p 45 | (45,40,60) h 0 p −55 |
| `SHELL.MDR` | (128,128,128) h 49 p 92 | (23,32,48) h 0 p 45 | (22,22,36) h 101 p −38 |

Nas fases, o padrão é sempre o mesmo: duas luzes fortes no alto, de trás-esquerda e trás-direita de quem olha para +Y (h ±120, p 45),
e uma terceira de +Y, de baixo ou rasante. **[A]**

### 2.2 No Shell e na seleção original

- `screenCharSelect1P`, `2P` e `4P` só chamam `hierSetSwitch`, `hierSetCsDrawMe`, `isSlotOpen`, `findAvailableMonster`,
  `modelsLeft`, `updateSelectSwitches*`, os sons e o filme (`moviesReadyScreenObject`, `CMovie::Play`, `moviesReleaseScreenObject`).
  Nenhuma carrega ou desenha um `.NGP` de monstro. O `SHELL.NGP` tem 18 triângulos skinned ao todo. Conclusão: a seleção do retail
  mostrava retratos, painéis de traje e o filme do telão, **sem monstro 3D**. Não há luz de seleção para imitar. **[A]** para as
  chamadas, **[M]** para a conclusão.
- O Shell só usa luz paralela, a do `SHELL.MDR` (conferida na RAM dos instantâneos `menus` e `shell_ceu`), e ela vale para 16
  objetos do `SHELL.NGP` (~2.900 de ~90 mil triângulos). Essa luz não serve para o monstro: a principal aponta quase a pino (p 92),
  as outras duas valem 0,2 a 0,37. Um monstro de pé ficaria com o topo claro e a frente e os lados quase pretos. **[M]**
  (fórmula aplicada de cabeça, não desenhado).

## 3. O que falta e os caminhos

### Parte comum (vale para qualquer fonte de luz)

1. Em `Model::build`, guardar por vértice, ou por item, o que o VU1 usa: `dirLight` (0x100), o multiplicador de cor do OBJECT (como
   `Scene::objectColor`, sem o `colorQuad` do `_cs`), o especular herdado (`specLightMe`/`specIntensity`, mesma regra de herança) e o
   "tem mapa de ambiente" (sem luz).
2. No `Renderer::render`, trocar `lit` pela fórmula de `vertexRgb` com uma `ParaLights` (direções e cores) recebida por parâmetro:
   `Σ cor_i · max(0, n·L_i)` para o RGB, mais `spec · max(0, n·H)^16`, com `H = normalize(L0 + direção da câmera)` como em
   `pLightCompSpecular`. Sem piso ambiente.
3. Tirar o corte em 1,0: mandar a cor do vértice na escala do GS (0..255, com 128 = 1,0). Por exemplo, mandar `cor/2` e usar
   `gain = 2` no shader, que já tem o uniforme.
4. Decidir o heading do monstro dentro da luz (as luzes são fixas no mundo; ver os riscos).

Custo da parte comum: ~0,5 a 1 dia, mais a conferência com o dono. A matemática já existe e foi conferida contra o VU1 em
`wotm_native.hpp`. O ideal é extrair `vertexRgb`/`ParaLights` para um header comum e não duplicar. Risco baixo para a luta: o
`Renderer` não é usado lá. **[M]**

### Caminho A: ler a chave 5 de um `.MRS`/`.MDR` do disco (recomendado)

- Parser de ~30 linhas: o formato está acima e em `resetcom.cpp`. O arquivo tem 32 a 48 KB e está no disco que o nativo já lê
  (`discRoot`/ISO, como o `.NGP`, o `.TEX` e o `LVL/CENTRAL`). Direções por `mathfHPtoVector`, cores /128. **[A]**
- **Seleção:** usar uma fase fixa (sugestão: `CENTRAL.MRS`, a fase "padrão", a mesma do treino e dos minigames). Outra opção é a
  fase escolhida da última vez (`Shell::m_levelNum`), porque a seleção de fase vem depois da de monstro.
- **Lista de saves:** o `.MRS` da fase do save (a mesma de `kLevels` em `wotm_save.inc:53`, que já escolhe a foto do fundo). O
  monstro fica com a luz da foto atrás dele.
- **Custo:** ~0,25 dia além da parte comum.
- **Riscos:** baixos. Arquivo ausente ou de mod quebrado → cair na luz de hoje. Fases que um mod trocar o `.MRS` passam a valer
  sozinhas (bom para mods).

### Caminho B: ler `paraL`/`vu1LightColor` da RAM viva

- Na seleção, a RAM tem a luz do `SHELL.MDR`, que é a errada para o monstro (seção 2.2). **Não serve para a seleção.** **[A]**
- Na lista de saves **dentro da luta**, a RAM tem a luz da fase em andamento: o monstro do save fica com a luz da luta pausada, não a
  da fase do save. Do menu, volta a ser a do Shell.
- O `Renderer` roda na thread da janela; ler a RAM pede o mesmo cuidado com hora de leitura dos outros desenhos (cópia no
  `Snapshot`, ver a memória "captura na thread do EE").
- **Custo:** baixo (~0,25 dia). **Risco:** médio, porque a fonte da luz muda de sentido conforme onde a lista é aberta.

### Caminho C: luz fixa copiada do jogo, no código

- É o caminho A com os números de uma fase escritos no fonte. Custo mínimo, mas perde as luzes por fase e o que os mods mudarem. Só
  vale se o disco não estiver à mão, o que não acontece: o modelo já vem do disco.

### Riscos e decisões em aberto

- **Heading.** Com a luz da fase e o monstro de frente para a câmera (o `Renderer` põe a câmera em +Y e o monstro olha para +Y), as
  duas luzes fortes (h ±120) ficam atrás e no alto do monstro. A frente fica só com a luz 2 (de +Y, de baixo em METRO e TOKYO,
  rasante em CENTRAL) e com o especular. Na luta isso acontece quando o monstro anda para a câmera. Para o "retrato", parece melhor
  girar as luzes 180° em heading (as duas fortes ficam do lado da câmera, à esquerda e à direita). Ou expor um ângulo e o dono escolher
  vendo. **[B]** (não desenhado).
- **Brilho.** Com as cores das fases somadas, partes do monstro passam de 1,5× a textura e chegam ao corte de 255. É o visual da
  luta, mas na seleção, sobre o telão escuro, pode parecer estourado. Conferir com o dono. **[M]**
- **Kineticlops e objetos sem `dirLight`** vão ficar visivelmente diferentes de hoje (mais "chapados", como no jogo). É o certo, mas
  é mudança que o dono vai notar.
- **O que continua de fora:** o `colorQuad` do `_cs` (flash de dano, irrelevante aqui), luzes pontuais (nenhum monstro tem), mapa de
  ambiente (o `Renderer` não o desenha; só não deve iluminar esses objetos) e a tabela de luz abaixo de SCALE não uniforme (Togera:
  duas peças 15 a 20 níveis mais claras, como era no nativo antes do `lightScale`). Esta última dá para portar depois, se aparecer.
- **Conferência:** não há referência do retail para a seleção. Para validar a fórmula no `Renderer`, compare com um monstro parado
  na luta no mesmo heading, com as luzes da mesma fase. Uma foto ao lado da outra basta.

## 4. A lista de saves

Sim, a mesma solução: o `wotm_savepics.hpp` usa o mesmo `Model` + `Renderer` (`:215`, `kYaw` 35, `kPitch` 8). A diferença é a fonte
da luz:
- **Recomendado:** o `.MRS` da fase do save (caminho A). Assim é igual do menu e de dentro da luta, e combina com a foto da fase no
  fundo.
- Opção: dentro da luta, a RAM viva (caminho B), que dá a luz da luta pausada. Não recomendo, porque o mesmo save mudaria de luz
  conforme onde a lista é aberta.

## Arquivos

- `native/wotm_model.hpp`: `PVtx` (`:284`), `Model::build` (`:751`), `Renderer::render` (`:945`, luz em `:972-975` e `:1010-1013`),
  shader (`:1067`).
- `native/wotm_native.hpp`: `paraL`/`vu1LightColor` (`:57-60`), `lightScale` (`:3559-3603`), `objectColor` (`:3678`),
  `readParaLights` (`:3743`), `objectLight` (`:3920`), `vertexRgb` (`:3957`), especular herdado (`:4144`).
- `src/common/plight.cpp`, `src/common/resetcom.cpp`, `src/game/reset.cpp`, `src/game/Shell.cpp:1989`, `src/game/ui.cpp:290`,
  `src/common/hier.cpp:174/250`, `src/common/mathf.cpp:549` (`mathfHPtoVector`).
- Disco: `RST/*.MRS`, `RST/SHELL.MDR` (igual a `SHELL/SHELL.MDR`).
- Scripts usados (rascunho, fora do repo): dump das chaves de reset, contagem de `dirLight` por `.NGP` (sobre `tools/ngp.py`) e
  leitura de `paraL` nos `.vu1snap`.
