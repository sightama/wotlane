# Depois da fronteira de hardware: prioridades (dono, 2026-10-19)

Vale quando a fronteira de hardware cair (`FRONTEIRA.md`, trabalho em outra sessão). Até lá continua a ordem do `PROXIMOS_PASSOS.md`
(inventário do port). A lista é do dono; a ordem e as notas foram combinadas com ele na mesma conversa. Cada item diz o que o repo já
sabe e o que falta descobrir.

## Ordem
| # | Item | Depende de | Custo estimado |
|---|---|---|---|
| 1 | RAM do convidado acima de 32 MB (+ medir 4 vistas) — **feito em 2026-10-19** | — | baixo |
| 2 | HUD dos inimigos: vida e energia juntas, retrato à esquerda — **feito em 2026-10-19** | — | baixo (base pronta) |
| 3 | Save em JSON legível, por nomes — **feito em 2026-10-19** | — | médio |
| 4 | Pasta `mods/` que sobrepõe arquivos do disco | — | baixo/médio |
| 5 | Sem tela de "memory card 1 ou 2"; slots extras no JSON | 3 | baixo/médio |
| 6 | Ícone de salvamento automático | 3 | baixo |
| 7 | Câmera moderna (orbital) com movimento relativo à câmera | — | alto |
| 8 | Mods que adicionam skins e monstros (sem substituir) | 3, 4 | médio/alto |
| 9 | Modo treino com hitboxes e dano na tela | — | médio |
| 10 | Chefes jogáveis | 8, 9 | alto |
| 11 | "2 players" vira "multiplayer", até 4 jogadores | 1 | alto |
| 12 | Seleção com prévia 3D de todos os selecionados | 1, 11 | médio |
| 13 | Campanha (adventure) com até 4 jogadores | 11, 7 | o mais alto |

**Em qualquer brecha** (baratos, sem dependência): remapear controles no launcher, modo foto, gráficos de PC (seção "Brechas").

Por que essa ordem: 1 destrava metade da lista e é barato. 2 é ganho rápido em cima de um mod que já existe. 3 e 4 vêm juntos porque
definem como os mods se referem às coisas (nomes, não índices); 3 → 5 → 6 usam o mesmo gancho de gravação. 7 vem antes do multiplayer
porque 4 vistas com a câmera antiga ficam piores, e porque a campanha em grupo precisa dela. 9 vem antes de 10 porque balancear chefes
e monstros de mods pede ver hitboxes e dano. 10 é um caso particular de "adicionar monstro" (8). A campanha com 4 fica por último:
cinema, chefes e regras foram feitos para 1 jogador.

### 1. RAM acima de 32 MB
- Medido em `investigacoes/2026-10-18/16_monstros.md`: com 8 monstros a carga da METRO vai até 0x23B4300 (35,7 MB) e congela
  (`g_frame` 205). As outras fases cabem; San Francisco e Vegas ficam com ~2 MB de folga.
- É o "degrau 2" daquele plano: `PS2_RAM_SIZE` do runtime é paramétrico (`ps2_memory.h`, potência de 2). Subir para 64 MB e mover o
  teto da área de carga (hoje ~0x1F10000) por gancho em `fileOnlyNgpFile`/`getNextNgpLoadAddr`. Ponteiros de 32 bits e banco plano:
  a relocação do `dbs` não muda. A camada nativa já usa `wotm::Ram{rdram, PS2_RAM_SIZE}`.
- Mexe numa costura do PS2Recomp: regenerar `tools/runtime_patch/wotm-runtime.patch`.
- Conferir junto: `fileStatus` com 14 slots (fase + 12 monstros), `texInfo` com 1000, VRAM residente do `.RTX`.
- **Medir aqui o custo de 4 vistas** (cena desenhada 4 vezes; com 8 monstros a CPU já está em ~9 ms por quadro) antes de investir na
  interface do item 11.

**Feito em 2026-10-19.**
- **RAM de 64 MB, o jogo vê 32.** No patch do runtime (`ps2_memory.h`): `PS2_RAM_SIZE` 64 MB e `PS2_GUEST_RAM_SIZE` 32 MB. A pilha da
  thread principal (`SetupThread` com pilha -1), o `GetMemorySize`, o heap do runtime e a pilha dos callbacks assíncronos seguem o
  segundo (`System.cpp`, `ps2_runtime.cpp/.h`), então tudo o que o jogo aloca fica onde estava; os 32 MB de cima só recebem o que os
  ganchos mandam para lá. O endereçamento é por máscara (`PS2_RAM_MASK`) e o código traduzido não tem 32 MB fixo. Os dois executáveis
  mudam (o código do jogo é compilado uma vez para os dois).
- **Carga acima de 32 MB** (`wotm_overrides.cpp`, `loadMonstersDBHigh`): na entrada do `Shell::LoadMonstersDB` o próximo endereço de
  carga (`fileStatus.ngpAddr[ngpLoaded]`, logo depois do NGP da fase) vira 0x2000000. Os NGP dos monstros e, atrás deles, os RTX e TEX
  da fase e dos monstros vão para cima; embaixo fica só o NGP da fase. A relocação dos monstros (`dbsRelocateViaPtrListFile`) soma
  "endereço - 0xA00000" em 32 bits e vale em qualquer endereço. `WOTM_HIGHRAM=0` volta à carga contígua do PS2. A linha
  `[wotm] carga da fase` diz quanto sobra até o fim da RAM.
- **Conferido:** Metro com 8 monstros (antes congelava na carga) carrega até ~0x2B10000 (21 MB de folga) e luta 60 s nos dois
  executáveis, sem vigia; menu pelo roteiro de pad até a luta; corpus sem piora. Os instantâneos de 32 MB do corpus continuam
  abrindo (o replay aceita RAM menor que a do build e zera o resto); os novos ficam com 64 MB no quadro inteiro. Mesma luta de 8 no Centro
  com e sem `WOTM_HIGHRAM`: mesmos objetos divergentes na checagem VU1 x nativo, nenhuma textura diferente.
- **Custo por vista** (luta IA x IA no Centro, 70 s, `wotm.exe`, monitor de 180 Hz; `WOTM_FIELDLOG` com `vistas=`/`hier=`/`cap=` e
  `PS2X_STATS` com `[native] redesenho:`; a 2ª vista vem de `WOTM_PLAYERS=2`, que alterna tela dividida e unificada):

  | Monstros | Vistas | EE por quadro (mediana / p99) | Redesenho nativo por quadro (parede / CPU) |
  |---|---|---|---|
  | 2 | 1 | 5,8 / 8,0 ms | 8,9 / 7,2 ms |
  | 2 | 2 | 8,2 / 12,0 ms | 15,1 / 14,0 ms |
  | 8 | 1 | 10,0 / 14,5 ms | 11,7 / 10,5 ms |
  | 8 | 2 | 13,8 / 18,3 ms | 18,6 / 17,9 ms (96,5% dos quadros em 1 field) |

  Cada vista a mais custa ~2,5 ms ao EE com 2 monstros e ~3,8 ms com 8 (o `hier` e o que ele puxa), e ~6,5 a 7 ms de CPU à thread da
  janela: o desenho nativo é limitado pela CPU (envio pelo raylib), não pela GPU, e a meia largura da vista quase não o barateia. As
  duas threads correm em paralelo, então o teto é a mais lenta. Extrapolando para 4 vistas: EE ~13 ms com 2 a 4 monstros (cabe), mas o
  redesenho ~27 ms (não cabe em 16,7). **Antes do item 11, o desenho nativo precisa ficar mais barato por vista** (o que se repete por
  vista: coleta e envio de cada objeto; candidatos: malhas estáticas da fase em buffers na GPU, lote por textura, corte por vista antes
  do envio). Com 8 monstros e 2 vistas a janela já passa de 16,7 ms hoje.

### 2. HUD dos inimigos
- Base pronta: `WOTM_ENEMYBARS=1` (`NATIVO.md`, tabela de variáveis) já troca o ícone de vida por barras de vida (`m_health`/
  `m_maxHealth`) e energia (`StaminaMeter`, +0x460), desenhadas no nativo pelo gancho em `Hud::drawHealthIcon`/`ScreenPolys::addPoly`.
- **Feito em 2026-10-19.** Um bloco só sobre a cabeça de cada inimigo: retrato na altura toda à esquerda; à direita, vida em cima e,
  embaixo, o botão do especial e a energia (mais fina e mais curta). Cada peça tem contorno preto próprio, a faixa cinza entre elas é
  comum e há um contorno preto por fora.
- Retrato: última entrada do `.RTX` do monstro (T8H 128x64; metade esquerda normal, direita fúria). O gancho acha o arquivo do
  monstro pelo `epNode` do `m_cs` (+0xC) e os intervalos de `fileStatus.ngpAddr`, e procura no NGP o TEX0 desse retrato
  (`tbp0 == maxResAddr[i-1]`); cache por carga de fase. Fúria = `m_boostFull` (+0xF4), a mesma chave do `hierSetSwitch` do
  `Hud::update`. A textura fica de cabeça para baixo na VRAM (o jogo desenha com V = 0x80 - v): o retângulo vai de V 64 a 0.
  Especial aceso = `m_specialWeapon` (+0xF3) fora do modo 9, como o `isSpecialAvailable`, no vermelho do ícone do HUD do jogador.
- O nativo ganhou retângulo de tela com textura (`ScreenRect::tex0`, `wotmBarTex`), respeitando o corte da tela dividida.
- Conferido: lutas de 3 inimigos (`WOTM_MATCH=3,1,2,5,1` e `1,1,2,3,1`), os 3 retratos achados e em pé como o do jogador; fúria
  forçada num build de teste mostra a outra metade; especial forçado aceso em vermelho, igual ao do jogador com `WOTM_CHEATS=esp`.
  Fotos em `docs/screenshots/2026-10-19/2026-10-19_hud_inimigos_bloco*`.
- Em aberto: um bloco perto do topo da tela pode ficar por cima do HUD do jogador (a posição vem da cabeça, limitada pelo jogo).

### 3. Save em JSON
- Hoje o stub `sceMc*` do runtime (`ps2xRuntime/src/lib/Kernel/Stubs/MemoryCard.cpp` do fork) grava e lê arquivos em `mc0/` do host
  (porta 1 em `mc1/`, ao lado). Quem monta os blocos é o `game/MonsterMc` (as páginas e o que vai em cada arquivo); o `game/McFile`
  (tarefas `McLoadFile`/`McSaveFile`/`McSetupGameDir`/`McScrambleIcon` sobre `sceMc*`) e o `game/McPage` (menus) são plataforma.
- **Layout mapeado em 2026-10-19 pelo asm** (sem precisar de diferença entre gravações) e conferido no save que existe em
  `discroot/mc0/BASCUS-97197` (decodifica sem sobra). `python tools/mcsave.py <pasta>` mostra os dois arquivos como JSON por nome.

**Arquivos** (pasta `BASCUS-97197`, título "War of the Monsters"):

| Arquivo | Bytes | Conteúdo | Monta / aplica |
|---|---|---|---|
| `BASCUS-97197` | 0x380 | opções, battle tokens e destraves (`_SettingsData`) | `MonsterMc::getSettingsData` / `setSettingsData` |
| `savegame.dat` | 0x280 | 10 slots da aventura (`_SaveGameData`) | `MonsterMc::getSaveData(slot)` / `setSaveData(slot)` |
| `icon.sys`, `ICON.ICO` | 964, 61120 | ícone do PS2 (item 6) | `McSetupGameDir`; o `McScrambleIcon` regrava o `icon.sys` a cada gravação das opções |

**Sem checksum.** As duas leituras (`McLoadSettings::update`, `McSelectReel::refreshSaveData`) só conferem a marca `0x12345678` no fim
do bloco; marca errada vira erro do cartão. Nenhum ponteiro: tudo é `int` de 32 bits, little-endian.

**`BASCUS-97197` (opções):**

| Off | Origem | No save de exemplo |
|---|---|---|
| 0x00 | `TheGame+0x1203CC` (`m_matchMode`; o `Shell` acende `m_difficultySw[valor]`: é a dificuldade) | 0 |
| 0x04, 0x08 | `Shell::m_sfxVolume`, `m_musicVolume` (0..0x400) | 1024, 830 |
| 0x0C | `Shell+0x2A40`: o construtor põe 50 e ninguém mais lê (opção morta?) | 50 |
| 0x10 | `Shell::m_mono` (ao carregar vira 0/1) | 0 |
| 0x14, 0x18 | `Shell::m_ctlrCfg[2]` | 1, 1 |
| 0x1C, 0x20 | `Shell::m_vibration[2]` | 1, 1 |
| 0x24 | battle tokens (`Shell+0x2B24`) | 55975 |
| 0x28..0xE7 | `monsterSelectMode[i].state` dos 48 `SelectSlot` (só o `state`; `taken`, preço e nomes ficam no ELF) | ver abaixo |
| 0xE8..0xF3 | `TheGame+0x1204B0..B8`, 3 ints: não identificado (o `game.h` põe aí o fim de `m_actuator`, inferido; o `Weapon` lê vizinhos como tweaks) | 0, 0, 0 |
| 0xF4..0x34B | **não é escrito**: o buffer é da pilha e esse trecho grava lixo (32 palavras não nulas no exemplo) | — |
| 0x34C | marca `0x12345678` | |

Os 48 flags são 12 grupos de 4. Grupos 0 a 9 são os monstros na ordem do `aiMonsters` de `Shell::RandomlySelectAI` (Raptros, Togera,
Preytor, Congar, Robo-47, Agamo, Ultra V, Magmo, Kineticlops, Zorgulon), e a entrada `j` do grupo é o traje `j`. Os grupos 10 e 11 são
as fases do `levelUnlocked`: entrada 40 Volcano, 41 Mini Baytown, 42 Capitol, 43 UFO, 44 Dodge Ball, 45 Big Shot, 46 Crush; a 47 sobra.
O `McCheckForSweetTooth` do boot procura o `BASCUS-97101/globalFile` (save do Twisted Metal: Black) no cartão e, se achar, liga a
entrada 23 ("Agamo Costume 4", o Mecha Sweet Tooth; `ROCK3`) e toca `ShellSound::playSweetUnlockedSound`. No PC esse destrave é a
chave `unlocks.mechaSweetTooth` do JSON, que vale mais que `agamo.skins[3]` (criada com o estado do cartão quando falta).
Os nomes e preços de cada entrada estão na própria tabela (`SelectSlot::name`/`cost` no ELF) e confirmam a ordem dos grupos; a entrada
47 é uma "Level 4" de 5000 tokens que nenhum código lê.

**`savegame.dat` (aventura):** +0 número de slots usados (o maior índice gravado + 1); slots em `0x10 + i*0x3C`; marca em 0x268; o resto
é zero. Cada slot usa só 0x14 dos 0x3C bytes:

| Off no slot | Origem | Ao carregar (`setSaveData`) |
|---|---|---|
| +0x00 | `Shell::m_levelNum` (fase onde parou) | idem; as IAs vêm de `m_levelNumAIs[fase]` e `levelMonsters[fase]` |
| +0x04 | `Shell::m_monsterSel[0]` (`monstro << 5 \| variante`) | idem |
| +0x08 | `Shell::m_lives[0]` | idem |
| +0x0C | `Shell+0x2B28 + Shell+0x2B2C` (pontos da fase + acumulado) | tudo em `+0x2B2C` |
| +0x10 | `Shell::m_costume[0]` | idem; e `m_numPlayers = 1`, `m_mode = 1` |

Gravar um slot (`McSaveAdventure::onEnter`) lê o `savegame.dat` inteiro, troca só aquele slot (`copySaveData`) e grava os 0x280 de
novo; com o arquivo ausente (`McSaveAdventureBlank`) começa de um bloco zerado com a marca. Os 10 slots são as "fitas" da tela de
carregar (`McSelectReel`, `McLoadGameBoxes`); o cartão 2 dá outros 10.

**Quando o jogo grava** (sequências de páginas montadas em `MonsterMc::update*`): `updateBootLoad` (boot: `McBootPage` →
`McLoadSettings` → `McCheckForSweetTooth`), `updateSettingsLoad`/`updateSettingsSave` (menu de opções), `updateUnlockSave` (depois de
comprar na tela de destrave: `McAskToSave` e grava as opções, onde ficam os destraves), `updateEndOfStorySave`, `updateStoryModeSave`/
`updateStoryModeLoad` (aventura) e `updateEndureSave`. Toda gravação passa por `McChooseSlot` (cartão 1 ou 2), `McCheckSpace`,
`McCheckFormat` e `McCheckOverwrite` e grava as opções (`McSaveSettings`); as de opções, destrave, endurance e fim de história também
criam o `savegame.dat` em branco se faltar (`McSaveAdventureBlank`), e a da aventura escolhe o slot (`McSelectSaveSlot`) e grava o
slot (`McSaveAdventure`). Ou seja: tokens e destraves vão para o cartão em qualquer gravação.

**Feito em 2026-10-19** (`native/runtime/wotm_save.inc`, JSON em `native/wotm_json.hpp`; `WOTM_SAVEJSON=0` desliga).
- Sem mexer no fork nem decompilar: ganchos na entrada do `MonsterMc::stdInit` e do `MonsterMc::stdExit`, que abrem e fecham toda
  sequência de páginas do cartão (boot, opções, destrave, endurance, aventura). No `stdInit` os binários dos dois cartões (`mc0/`,
  `mc1/`) são refeitos a partir do `save.json`; no `stdExit`, se o jogo gravou algum binário (data ou tamanho mudou), o JSON é
  atualizado a partir dele. O jogo continua vendo os mesmos 0x380/0x280 bytes; o lixo da pilha (0xF4 em diante, menos a marca) vai zero.
- **Um JSON só**, ao lado da pasta do cartão (`discroot/save.json`): `options`, `battleTokens`, `unlocks` (monstros e trajes, fases) e
  `unknown` (os campos sem nome) uma vez, iguais nos dois cartões; `saves` com 20 slots, 0 a 9 no cartão 1 e 10 a 19 no 2 (`null` =
  slot vazio; o array termina no último ocupado). Chaves por nome (`"congar": {"skins": [...]}`, `"levels": {"ufo": true}`, fase e
  monstro dos saves por nome), nunca por índice: um mod que adiciona monstro ou skin (item 8) só acrescenta chaves. Chaves que o código
  não conhece ficam no arquivo quando ele é reescrito (conferido com uma chave `modData`). Skins ainda por posição dentro do monstro;
  dar nome a cada traje fica para o item 8.
- Migração: sem JSON, o primeiro `stdInit` o cria a partir dos cartões; com JSON sem os slots de um cartão que tem aventuras, elas são
  importadas em vez de apagadas. JSON que não abre (erro de sintaxe) é avisado no log, os cartões ficam como estão, e antes de o jogo
  reescrevê-lo o arquivo do usuário vai para `save.json.bad`. A gravação é por arquivo temporário e troca.
- O `[wotm] save: no jogo, N tokens, dificuldade D` no fim de cada sequência diz o que o jogo tem na RAM.
- **Conferido:** JSON gerado do save antigo igual ao do `tools/mcsave.py`; JSON editado (tokens, dificuldade, traje) e binários
  apagados → o jogo sobe com os valores do JSON e o `savegame.dat` refeito sai byte a byte igual ao original; pelo menu (o dono):
  "accept and save settings" e compra na tela de destrave atualizam o JSON na hora, mantendo a chave do mod; ao reabrir, as opções
  voltam; "load game" no cartão 2 abre a fita que veio dele; gravar as opções no cartão 2 também atualiza o JSON.
- Cuidado: editar o `save.json` com o jogo aberto pode atropelar uma gravação (um editor com o arquivo aberto desde antes regrava a
  versão velha; aconteceu no teste). O certo é editar com o jogo fechado.
- Próximo (combinado com o dono): opções, destraves e tokens salvos sozinhos (os `MonsterMc::update*Save` viram gravação direta da RAM
  para o JSON, o boot lê direto do JSON; os tokens que o `Shell::EvaluateOnePlayerStoryStatus` soma no fim da fase também precisam ir
  para o arquivo), com o ícone do item 6 no mesmo gancho; a campanha fica como a única escolha de slot (20 fitas), que é o item 5, junto
  com a revisão dos 25 textos de memory card.
- Em aberto: o que é `Shell+0x2A40` e `TheGame+0x1204B0..B8`; o que exatamente o `McScrambleIcon` muda no `icon.sys`.

### 4. Pasta `mods/`
- O carregador procura cada arquivo primeiro em `mods/` (mesmo caminho relativo do disco) e, se não achar, lê o da ISO. Trocar skin,
  textura, som ou fase vira mod sem código. Gancho na leitura de arquivos do runtime (CD/`file*`), na camada do host.
- É o primeiro degrau do item 8: aqui só substitui; acrescentar sem substituir vem lá.

### 5. Sem escolha de memory card
- Ganchos nas telas do Shell que perguntam o cartão (achar pelo `screenTransition`, como no `wotm_padsel.inc`) para pular direto.
- Hoje, ao carregar do cartão 2, o jogo anima a segunda caixa das fitas de vídeo 3D. No lugar do cartão 2, o JSON aceita mais slots no
  array. **Em aberto:** como o jogador escolhe entre N slots sem a tela do cartão (decidir quando chegar aqui).

### 6. Ícone de salvamento automático
- O ícone 3D do save do PS2 (formato de ícone do memory card) vem junto do que o jogo grava. Extrair e desenhar no nativo no canto
  inferior direito por 1 s, disparado pelo gancho de gravação do item 3. Detalhes quando for fazer.

### 7. Câmera moderna (orbital)
- Hoje, na câmera padrão, esquerda/direita giram a câmera. Quando os monstros estão perto (vista unificada, `gUseUnifiedView`,
  `LeaveUnifiedView`, POV da câmera em `+0xCF8` de cada bloco de 0xEB0 em `Cameras::m_cameras`, `game.h`) o movimento já é relativo à
  câmera: o começo é levar esse movimento para a câmera padrão.
- Não é só isso: órbita pelo analógico direito/mouse, o que acontece com o alvo travado, colisão da câmera com prédios (o jogo já tem
  os prédios "fantasma" por linha de visada), transições com cinema e agarrão, e a câmera de cada vista na tela dividida.
- `Cameras::Update` (tabela `_6Camera$UPDATE_FUNK`) ainda está só em asm; câmera já é superfície de decomp do port. A câmera livre de
  debug (F5, `wotm_freecam.h`/`.inc`) tem a convenção de eixos pronta.

### 8. Mods que adicionam skins e monstros
- Trajes em `m_costume` (o `[2 + j]` atropela `m_mode` a partir da IA 10), tabela de seleção em `monster_select.h` (grupos de 4 por
  monstro), `texInfo` com 1000 entradas, nomes internos em `MonsterNames.h` (17 entradas). Monstro novo passa por `MonsterParse` e
  `Shell::Load*`, que já são a superfície de decomp do port.

### 9. Modo treino
- Hitboxes e volumes de colisão desenhados no nativo, dano de cada golpe na tela. Ajuda a balancear os chefes jogáveis (10) e os
  monstros de mods (8): quem mexe em `MonsterParse` vê o efeito na hora. Os modos de debug que já existem (`NATIVO.md`: modo deus,
  vencer a luta, `WOTM_AIVSAI`) são a base para um adversário parado ou controlado.

### 10. Chefes jogáveis
- Os chefes já estão na tabela de tipos (`assboss`, `jelly`, `final` em `MonsterNames.h`, índices 13 a 15); a seleção hoje vai de 1 a 9
  (até 10 com o bônus, `findAvailableMonster`).
- Alguns chefes não são `Monster` comum (`NATIVO.md`, "Vencer a luta"; câmera própria em `InitPlantBossCamera`): precisam de estados
  de controle, golpes e escala que caibam numa luta normal. Usa o caminho do item 8 para entrar na seleção.

### 11. Multiplayer até 4
- Já existe: tela dividida para 2 (vistas `viewDef[1]`/`[2]`, `ViewCtx`/`ViewScope`), controles por porta (`wotm_pad.inc`,
  `g_ps2PadDeviceMap`) e escolha de lados (`wotm_padsel.inc`); tudo em `NATIVO.md`. A engine carrega até 16 monstros.
- Falta: portas 3 e 4 no mapa de controles; 4 vistas (quadrantes) e o HUD de 4; a escolha de lados com 4 colunas; o texto do menu
  ("2 players" → "multiplayer").
- Atenção: em `LoadMonstersDB` os jogadores 3/4 montam o caminho com o monstro do jogador 2 (peculiaridade do retail,
  `16_monstros.md`); o gancho novo não pode herdar isso.

### 12. Prévia 3D na seleção
- Hoje a seleção mostra o monstro de cada jogador; com 4 são 4 modelos carregados no Shell ao mesmo tempo.

### 13. Campanha com até 4
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no
  Capitólio, 0x1A0) precisam ser revistas.

## Brechas (baratos, em qualquer momento)
- **Remapear controles no launcher:** pendência do `RUNTIME.md` ("Pendências": gamepad e remapeamento). Os controles já passam pelo
  mapa por porta do `wotm_pad.inc`.
- **Modo foto:** a câmera livre do F5 (`wotm_freecam.h`/`.inc`) já existe; falta pausar o jogo e esconder o HUD.
- **Gráficos de PC:** escala de resolução, MSAA e filtro anisotrópico no desenho nativo. A interpolação de quadros já está na fila
  (`PROXIMOS_PASSOS.md`).

## Fora da ordem
- **Online:** não é prioridade. Com o multiplayer local de 4 (item 11), o Steam Remote Play Together e o Parsec já permitem jogar
  online sem netcode: só testar e documentar. Netcode próprio (rollback) fica descartado por ora: exigiria provar que a lógica dá
  sempre o mesmo resultado com os mesmos comandos.
- **Viewer web de monstros com várias skins:** ideia em `VIEWER_WEB.md`. Exige extrair assets: a extração roda na máquina do usuário a
  partir da ISO dele; nada extraído vai para o repo (regra 1 do `CLAUDE.md`).

## Ideias do dono ("viagens")
- **Modo Nintendo DS:** LOD sempre no nível mais alto (o LOD é escolhido no nativo, `NATIVO.md`); filtro de áudio em tempo real no motor
  de som nativo (`wotm_snd.hpp`); talvez duas telas, jogo em cima e HUD embaixo com o retrato (o HUD do `hier` já é desenhado no nativo,
  então dá para mandá-lo para outra área da janela).
- **Pedestres do GTA SA e o CJ jogável em tamanho real:** os pedestres de hoje são partículas (fx 91–99, `VU1CHECK.md`), então trocar
  por NPCs 3D é desenho e animação novos. O CJ depende do item 8. Assets de outro jogo não entram no repo: viriam dos arquivos do
  próprio usuário, carregados como mod.
