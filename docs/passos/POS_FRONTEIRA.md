# Depois da fronteira de hardware: prioridades (dono, 2026-10-19)

É a lista que guia o trabalho (a fronteira de hardware caiu em 2026-10-19, `FRONTEIRA.md`, e o inventário do port terminou; o antigo
`PROXIMOS_PASSOS.md` foi apagado em 2026-10-22, o índice dos docs está no `CLAUDE.md`). A lista é do dono; a ordem e as notas foram
combinadas com ele. Cada item diz o que o repo já sabe e o que falta descobrir. Relatos de bug do dono passam na frente.

## Ordem
| # | Item | Depende de | Custo estimado |
|---|---|---|---|
| 1 | RAM do convidado acima de 32 MB (+ medir 4 vistas) — **feito em 2026-10-19** | — | baixo |
| 2 | HUD dos inimigos: vida e energia juntas, retrato à esquerda — **feito em 2026-10-19** | — | baixo (base pronta) |
| 3 | Save em JSON legível, por nomes — **feito em 2026-10-19** | — | médio |
| 4 | Pasta `mods/` que sobrepõe arquivos do disco — **feito em 2026-10-22** (gancho, kit e PNG direto; guia em `MODS.md`) | — | baixo/médio |
| 5 | Sem tela de "memory card 1 ou 2"; slots extras no JSON — **feito em 2026-10-21** (monstro 3D incluído) | 3 | baixo/médio |
| 6 | Ícone de salvamento automático — **feito em 2026-10-20** | 3 | baixo |
| 7 | Câmera moderna (orbital) com movimento relativo à câmera — **feito em 2026-10-22** (etapas 0 a 9 na master, com teclado novo e menus com mouse: `investigacoes/2026-10-22/camera_orbital_plano.md`) | — | alto |
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
- **Passo c, feito em 2026-10-20** (`WOTM_SAVEAUTO=0` volta as telas): opções, destraves e tokens se gravam sozinhos, sem tela do
  cartão. Os chamadores (`screen init`, `saveSettings`, `saveUnlocks`, `loadSettings`, `wonStoryOverlay`, `lostEnduranceOverlay`) só
  repetem o `MonsterMc::update*` até `MonsterMc+4`; os seis `update*` que não são da aventura (`BootLoad`, `SettingsLoad`,
  `SettingsSave`, `UnlockSave`, `EndOfStorySave`, `EndureSave`) foram trocados por uma cópia direta (a mesma do
  `getSettingsData`/`setSettingsData`) entre a RAM e o `save.json`, que liga `+0`, `+4` e `McPageMgr+0x130` (sequência concluída: a
  tela de opções só volta com ele) na mesma chamada. Dois detalhes do retail que a troca mantém: a compra da tela de destrave só é
  efetivada dentro do `McSaveSettings::update` (`g_monsterToUnlock`/`g_skinToUnlock`: liga o `state` e desconta o preço; a tela põe -1
  nos dois logo depois), e o boot normaliza o mono e aplica volume e vibração depois que a sequência termina (isso continua no
  chamador). Os tokens que o jogo soma fora dessas sequências (`Shell::EvaluateOnePlayerStoryStatus`, fim de fase da história: as duas
  únicas escritas em `Shell+0x2B24` fora do cartão, do endurance e do construtor) são pegos por uma vigia no `sceGsSyncV`: tokens ou
  destraves diferentes da última gravação vão para o JSON (só eles: as opções do arquivo ficam, o menu de opções pode estar com valores
  não aceitos). A aventura (`updateStoryModeSave`/`Load`) segue com as telas e os binários, como antes (item 5).
- A campanha como a única escolha de slot e a revisão dos textos de memory card viraram o item 5 (feito).
- Em aberto: o que é `Shell+0x2A40` e `TheGame+0x1204B0..B8`; o que exatamente o `McScrambleIcon` muda no `icon.sys`.

### 4. Pasta `mods/`
- O carregador procura cada arquivo primeiro em `mods/` (mesmo caminho relativo do disco) e, se não achar, lê o da ISO. Trocar skin,
  textura, som ou fase vira mod sem código. Gancho na leitura de arquivos do runtime (CD/`file*`), na camada do host.
- É o primeiro degrau do item 8: aqui só substitui; acrescentar sem substituir vem lá.
- **Etapa 1, o gancho (feita em 2026-10-21; `native/wotm_mods.hpp`, `native/runtime/wotm_mods.inc`):** cada subpasta de `mods/`
  (ao lado do ELF, ou seja, na raiz do disco extraído; `WOTM_MODS=<pasta>` troca, `=0` desliga) é um mod e repete a árvore do disco
  (`mods/meu_mod/MON/LIZARD0.TEX`). Pasta com `_` ou `.` no começo fica desligada; dois mods com o mesmo arquivo: vence o último
  em ordem alfabética (o log diz quem perdeu). Sem nenhum arquivo em `mods/` nenhum gancho é instalado.
  - Todo arquivo que o jogo lê passa pelo `fileCdSearchFile` (fases, monstros, `SHELL`, bancos, músicas, filmes) e depois pelo
    `sceCdRead` (`fileReadf/a/s`, `zipInflateAll`, `snd_StreamSafeCdRead`). O gancho da busca roda a original e, se o arquivo
    estiver em `mods/`, devolve um LSN falso (faixa a partir de 0x800000, além de qualquer DVD) e o tamanho do mod; o do
    `sceCdRead` atende essa faixa lendo do arquivo do PC. O som nativo (`readLsnRaw`) lê bancos e músicas pela mesma faixa. A
    camada nativa que lê o disco por nome também procura antes em `mods/`: paletas do `.RTX` (`Scene::loadResources`, que lê o
    arquivo direto e não da RAM), filmes (`wotm_fmv.hpp`), ícone, fotos e monstro da lista de saves (`wotm::disc::readFile`).
  - Log: `[mods] N arquivo(s) de <mods> em <pasta>` no boot e, na primeira busca de cada arquivo, `[mods] MON/LIZARD0.TEX <-
    teste_togera (110080 bytes; disco 110080)`, com AVISO quando o do mod é maior: o jogo carrega em regiões de tamanho fixo e
    pode sobrescrever o que vem depois (abrir espaço fica para o item 8). Arquivo que não existe no disco também é servido
    ("arquivo novo"), mas ninguém o pede ainda.
  - Conferido (`WOTM_MATCH=3,1`, IA, fotos aos 12 e 20 s): Togera com a textura 4 do `LIZARD0.TEX` recolorida pelo
    `tex.py inject --new-palette` (`.TEX` + `.RTX` no mod) sai vermelho, e sem o mod (`WOTM_MODS=0`) verde; com cópias idênticas
    de `LVL/CENTRAL.NGP` (zip, lido em blocos pelo `fileReads`), `SND/LIZARD_S.BNK` e `VAG/GEN1AMB.VPK`/`GEN1BAT.VPK` (que no
    `discroot` são vazios: vieram da ISO) a fase, o banco e as duas músicas saem do mod (`sound: LSN 8389878 ... (mods)`). Dois
    mods juntos. Corpus sem mudança. Fotos: `docs/screenshots/2026-10-21/2026-10-21_mods_togera_vermelho.png` e `..._mods_desligado_togera_verde.png`.
- **Etapa 2, PNG direto e kit (feitos em 2026-10-22; como usar em `MODS.md`):**
  - **PNG:** `<arquivo>_tNNNN.png` (do `.TEX`) e `<arquivo>_rtx_tNNNN.png` (do `.RTX`) em qualquer pasta do mod. O jogo segue
    carregando o `.TEX`/`.RTX` do disco; o desenho nativo (`native/wotm_texmods.hpp`, `Scene::modTexture`) troca a imagem no
    `textureId` (objetos) e no `residentTexture` (quadrados de tela: barras dos inimigos, fonte), em qualquer resolução. O texId
    do disco sai do deslocamento do pacote de `texInfo[id]` dentro do arquivo carregado (`fileStatus.texAddr`/`resAddr`, nome em
    `names`); as residentes, pelo DBP. PNG maior que a original ganha mipmaps. Fora: partículas, monstro da lista de saves e
    o GS emulado (para esse, `modkit.py pack --inject`).
  - **Kit:** `tools/modkit.py export/pack/check` sobre o `tex.py` e o `snd989.py` (o `.BNK` volta pelo `bnk-replace`, vários sons
    num banco só). O `pack` só leva o que mudou (sha1); o re-export não atropela edição. Interface: `tools/modkit_gui.py`
    (botão "Mod kit" do launcher). O `tex.py export` de um `.RTX` passou a
    nomear `<arquivo>_rtx_tNNNN.png` (e `<arquivo>_rtx_manifest.json`), sem colidir com o `.TEX` na mesma pasta.
  - Conferido: Togera com a pele em PNG 4x (grade fina visível) e retrato 2x no HUD do jogador e na barra do inimigo (fotos
    `docs/screenshots/2026-10-22/2026-10-22_mods_png_*`); kit com PNG, `.VAG` e um som do `SHELL_S.BNK`.
  - Em aberto: `.NGP` só sai (modelo novo é o item 8); PNG nas partículas (as UVs dos sprites são em texels); o monstro 3D da
    lista de saves ler o PNG.

### 5. Sem escolha de memory card
- **Lista de saves (feito em 2026-10-20, merge 4da9c64; `wotm_save.inc`):** as caixas e as fitas saíram (a troca de caixa
  não ficou boa; decisão do dono). Carregar continua indo até as caixas no mapa do menu (`McPrepBoxScreen`), e o update do
  `McLoadGameBoxes` vira um modal com a moldura do `McBackground` e o texto do `McTextBox` (fontes do `McPage` reduzidas, sem
  cruzamento ao trocar de linha): à direita a lista do `save.json` (até 99 slots, 10 visíveis, cima/baixo e esquerda/direita de
  10 em 10), à esquerda o save escolhido ("<monstro> at <fase>", vidas, progresso = fases concluídas / 11, sem decimal; a fase do
  save é a próxima a jogar; "N of 11 levels" na linha de baixo). X carrega pelo `MonsterMc::setSaveData`; triângulo/círculo é o "voltar" da página. Gravar: o update
  do `McChooseSlot` (depois do "salvar?") vira o mesmo modal; slot vazio grava direto no JSON, ocupado pede um segundo X. Foto:
  `docs/screenshots/2026-10-20/2026-10-20_lista_de_saves_carregar.png`.
- Na fase (gravar depois da luta) a moldura sai mais baixa que no menu (o tamanho vem da animação do `McBackground`; o
  `setDestDims` não passa de 1): as alturas do texto são comprimidas para caber. A moldura vai direto ao tamanho final, porque ali o
  `timerGetFieldsLastFrame` não anda e ela não cresceria (o `McPageMgr::update` só roda a página com ela pronta); o `+0x274` do
  `McChooseSlot` (o `stateIsOk`) é ligado como no original. Os primeiros 100 ms não leem o pad (o X do "salvar?" chegava aqui e
  gravava). Cima/baixo dão a volta nas pontas. Foto: `docs/screenshots/2026-10-20/2026-10-20_lista_de_saves_gravar_na_fase.png`.
- Tela preta no "salvar?" depois da cena final de Tsunopolis (o disco voador leva o monstro e sobram 18 itens na fase): o nativo
  recusava instantâneos com menos de 20 itens. Agora vale também quando há diálogo na lista de alta prioridade (`wotm_live.inc`).
- A fonte do menu não tem `%` nem `:` (o progresso sai "4 of 11 levels"; nem `/`).
- **Logo e foto da fase (2026-10-20, `native/wotm_savepics.hpp`):** à esquerda, de cima para baixo e centrados: o logo do monstro,
  "on <fase>", a foto larga da fase, "Score N" e o resto. As imagens saem do `SHELL/SHELL.TEX` + `SHELL.RTX` do disco (pasta ou
  ISO; depois da luta o menu não está na memória): logos 256x128 nos texIds 121..129 e 52 (Raptros); fotos de 512x256 em dois
  pedaços T8 de 256x256 (a foto quadrada no pedaço anterior não é usada). As paletas são entradas 16x16 do `.RTX` alocadas na ordem
  das texturas (4 por T8, 1 por T4; o `.NGP` não liga essas, o menu as troca por switch). As linhas estão de baixo para cima. O
  modal posta a lista a cada quadro e a janela desenha entre a moldura (lista de alta prioridade) e o texto, com 0,2 s de entrada.
  Sem logo (Spider, Ogre...) sai o nome em texto. Foto: `docs/screenshots/2026-10-20/2026-10-20_lista_de_saves_logo_e_fase.png`.
- **Monstro 3D sobre a foto (2026-10-21, merge 5a92844; `native/wotm_model.hpp` + `wotm_savepics.hpp`):** o monstro do save, no
  traje do save, anima em 3/4 sobre a foto da fase, montado direto do `.NGP`/`.TEX`/`.RTX` do disco sem o Shell. Reproduz as
  misturas do GS por nó (com a foto como fundo), a textura rolando do Kineticlops (camada 0x91), o LOD como o `hierLod` (Togera
  traje 3), o fogo da cabeça do Agamo (emissor op 22, preset 141) e o lens flare (HierLight op 5: olhos do Robo-47 e do Congar).
  Togera e Raptros têm a câmera mais perto (cauda e asas). Regras em `passos/MODELOS_3D.md`. Não reproduzido: molas do pelo/cauda,
  oclusão do flare, os outros presets de fogo. Foto: `docs/screenshots/2026-10-21/2026-10-21_saves_agamo_fogo.png`.
- Textos de memory card na aventura: **conferido pelo dono em 2026-10-22**, nenhuma mensagem de memory card aparece mais no jogo.
- O `savegame.dat` dos cartões só leva os 20 primeiros slots; os outros só existem no JSON.

### 6. Ícone de salvamento automático
- **Feito em 2026-10-20** (`native/wotm_saveicon.hpp`; `WOTM_SAVEICON_TEST=1` mostra a cada 3 s): o ícone é o `MEMCRD/ICON.ICO` do
  disco (uma fita de filme; 1158 vértices, uma forma, textura 128x128 comprimida; o formato está no cabeçalho do arquivo). A cada
  gravação do `save.json` o `wotm_save.inc` soma `g_wotmSaveCount`, e o `wotmDrawScene` desenha a fita girando no canto inferior
  direito por 1 s (com 0,15 s de fade). Lê da pasta do disco, da ISO (`native/wotm_disc.hpp`, o leitor que era do FMV) ou, sem os
  dois, da cópia que o jogo gravou no cartão. As luzes do `icon.sys` não são usadas (só a cor dos vértices e a textura).

### 7. Câmera moderna (orbital)
- Hoje, na câmera padrão, esquerda/direita giram a câmera. Quando os monstros estão perto (vista unificada, `gUseUnifiedView`,
  `LeaveUnifiedView`, POV da câmera em `+0xCF8` de cada bloco de 0xEB0 em `Cameras::m_cameras`, `game.h`) o movimento já é relativo à
  câmera: o começo é levar esse movimento para a câmera padrão.
- Não é só isso: órbita pelo analógico direito/mouse, o que acontece com o alvo travado, colisão da câmera com prédios (o jogo já tem
  os prédios "fantasma" por linha de visada), transições com cinema e agarrão, e a câmera de cada vista na tela dividida.
- `Cameras::Update` (tabela `_6Camera$UPDATE_FUNK`) ainda está só em asm; câmera já é superfície de decomp do port. A câmera livre de
  debug (F5, `wotm_freecam.h`/`.inc`) tem a convenção de eixos pronta.
- **Feito (2026-10-22), plano, decisões do dono e pendências:** `investigacoes/2026-10-22/camera_orbital_plano.md`: padrão
  moderna, lock-on enquadra os dois, recentralizar devagar com opção, mouse gira a câmera (pede teclado padrão novo e menus com mouse,
  etapas 7 a 9).

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

**Tela dividida de 3 e 4 feita em 2026-10-20** (`native/runtime/wotm_split4.inc`; ligada por `WOTM_PLAYERS=3` ou `4` com
`WOTM_MATCH`, `NATIVO.md`). Conferido no `ps2EntryRunner` com IA em todos (`WOTM_AIVSAI=1`): 4 jogadores no Centro
(`WOTM_MATCH=3,1,2,5,1`) por 145 s, com 3 rodadas, mortes e renascimentos, as 4 câmeras nos seus monstros (nenhum quadro na origem
em 3634) e os 4 HUDs com retrato, vida, "WINS" e mensagens no quadrante certo; 3 jogadores e 1 IA (`3,1,2,7,1`) por 95 s; 2
jogadores sem mudança. Com controles de verdade nos jogadores 3 e 4 ainda não. O teste achou e corrigiu, além do que segue:
o `Hud::initAfter`/`update` sair com índice ≥ 2 (sem HUD nas vistas de baixo), o fim de rodada do modo 3 em laço (com 3 ou 4 vivos
ninguém renascia) e as câmeras 2 e 3 indo à origem no POV 3. Na luta de 4 o vigia acusa trechos de ~24 quadros/s do jogo (o custo
de 4 vistas, abaixo). O que o motor já tinha: `viewInfo`, `worldCtx`, `viewDb`, os pontos de visada e
a `viewPrevMat` das partículas com 5 vistas, 4 câmeras, 4 HUDs, `m_monsters[0..3]` para jogadores, e o `inputInit` abre 2 portas × 4
slots (controle = porta + 2 × slot). O `viewToggleSplitScreen` dos cinemas não mexe com 3 ou mais vistas. Os ganchos:
- **Vistas:** na saída do `gameReestablishViews` (modos 3, 6, 7) as vistas 0 a 3 viram quadrantes de 320 × 112 (coluna `v & 1`, linha
  `v >> 1`; com 3 o de baixo à direita fica preto). Os 4 `ViewDef` moram no fim da RAM de cima (o retail só tem 8, nenhum em
  quadrante), com a distância de tela da tela cheia pela metade (mesmo FOV dos 640 × 224); `viewSetFov` refaz planos e matrizes.
  `viewInfo[v].viewport` fica 1/2 (as câmeras usam a tabela de POV da tela dividida). `m_numCameras` = jogadores. O
  `viewGrappleConfig` devolvia a vista 0 ao `viewDef` da metade: o gancho reaplica o quadrante.
- **Sem vista unificada:** a câmera 2 (a unificada do retail) é a do jogador 3. O `SetOkToUnify` de cada quadro zera o `m_camUnify`
  (o `InitCCCamera*` volta sem unificar). A pose de vitória ainda unifica (tela cheia na câmera 2); quando a tela volta a dividir, as
  câmeras 2 e 3 voltam aos jogadores 3 e 4. O `ResetLevel` não troca mais o POV da câmera 2.
- **Câmera 3 com 4 jogadores:** o `SetAIMonster` punha toda IA na câmera 3 (`gameInitCamera(3, ia)`); aqui a IA só recebe
  `m_cameraView` 3. Sobra: estados que chamam `SetCameraPOV(m_cameraView)` sem olhar se é humano (fim do arremesso,
  `StateThrowBack`) mexem no POV da câmera do jogador 4 quando é uma IA.
- **Carga:** do `LoadLevelFiles` à entrada do `InitPlayers` os jogadores 3 e 4 passam pelo caminho das IAs (`m_monsterSel[4..]`,
  `m_costume[2..]`, mesmo arquivo do disco) e depois voltam a jogadores; o `SetPlayerMonster` recebe o traje deles no lugar do
  `m_costume[1]`. Os jogadores 3 e 4 tiram a configuração de controle do `m_vibration[0..1]` (o `Monster::playerInit` lê
  `m_ctlrCfg[m_index]`, que só tem 2).
- **HUD:** o `Hud::initAfter` e o `Hud::update` saem logo com índice ≥ 2. O HUD 2/3 vira uma cópia do HUD da mesma coluna
  (`i & 1`) e roda o update como ele, com o `m_viewSlot`, o `_cs` da câmera e a barra trocados pelos da própria vista durante a
  chamada; os dois dividem o `_cs` e as animações da barra, que cada vista refaz logo antes do seu `hier`. Com `m_numMonsters` ≠ 2 a
  barra fica na posição da tela cheia, que é a forma do quadrante. O texto do HUD é posto como numa vista de 224 linhas a partir do
  topo do quadrante: no nativo, o que cai na metade de baixo (o "WINS") sobe 112 (só quando mais de uma vista manda texto; o
  diálogo de pausa fica como está).
- **Fim de rodada (modo 3):** o `EvaluateMultiPlayerBattleStatusNoAI` só trata 1 ou 0 vivos e o `...AI` renasce os slots 0/1 como
  jogador e o resto como IA. Com os quadrantes os dois viram: meta de vitórias como no `...AI`, senão cada morto de `m_monsters`
  renasce pelo `playerInit` (jogadores) ou `aiInit` (IAs).
- **POV 3 das câmeras 2 e 3:** o POV 3 é a câmera estacionada em `+0xD20` (`DetachCamera`, `TogglePOV`); as câmeras 2/3 nunca
  tinham essa posição e iam para a origem. Ao entrar no POV 3 ela estaciona onde está; a câmera 2 sai do POV 3 inicial (repouso da
  unificada) para o 0.
- **Cinema (apresentação dos monstros antes da luta):** com 2 jogadores o `doCinema` junta a tela (`viewToggleSplitScreen`); com 3
  ou mais vistas o toggle não faz nada e só a vista 0 seguia o cinema, no quadrante dela. No começo de cada quadro, cinema ligado põe
  a tela numa vista só (`viewNumViews` 1, vista 0 com o `viewDef[0]`, 640 × 224) e cinema desligado volta aos quadrantes, como o 2
  jogadores do retail (conferido: apresentação em tela cheia, depois os 4 quadrantes). Na volta todas as vistas são refeitas: o
  `endCinema` chama o toggle, que com 1 vista recria as vistas 0 e 1 como metades (a vista 1 ficava 320 × 224, com HUD e texto fora
  do quadrante).
- **Largada:** no `gameGetStartPoint` (modos 2 a 6) o primeiro nascimento de humano usa a lista 1 do `StartPoints` e o de IA a
  lista 2, em rodízio; os renascimentos, a lista 3. A lista 1 só tem os pontos dos 2 jogadores do retail (Centro: 2; as listas 2 e
  3 têm 4, um em cada canto), e os jogadores 2, 3 e 4 nasciam no mesmo ponto. O jogador além dos pontos da lista 1 nasce como IA
  (`m_playerNum` 2 durante a chamada), na lista 2 (conferido no Centro: 3 e 4 em dois cantos do mapa).
- **Motion blur:** o `MotionBlurAA::init` monta um pacote de 0x100 bytes por vista num bloco do bss onde só cabem 2 (a 3ª passaria
  do fim); o init fica com 2 vistas e o `drawOverlay` das vistas 2 e 3 não manda nada (o nativo não desenha o borrão).
- **Controles:** o stub do runtime só tem o slot 0; as `scePad*` com slot 1 são respondidas como o slot 0 da porta, e o `scePadRead` do
  slot 1 lê os dispositivos do jogador porta + 3 (`wotm_pad.inc`): terceiro e quarto controle do raylib (`PS2_PAD_MAX_GAMEPADS` = 4).
- **Nativo:** `Snapshot::views[3]` no lugar do `view1`; captura das vistas 2 e 3 na saída do `hier(v, 0)` (reserva: entrada do
  `hier(v, 1)`); texturas de quadrante compostas com as faixas pretas em cruz; FOV vertical pela altura da vista (só nos `ViewDef`
  acima de 32 MB); texto, barras sobre os inimigos, flares (`lensFlarePacket[v & 1]`, uma cópia por vista) e a matriz anterior das
  partículas para 4 vistas.
- **A conferir:** controles de verdade nos jogadores 3 e 4, custo (4 vistas: o redesenho estimado acima passa de 16,7 ms; o vigia
  viu ~24 quadros/s do jogo em trechos), sons por jogador (`m_index` até 3), o `TokenManager m_tokens[2]` (só 2) e a pose de
  vitória com meta de vitórias (não testada: o `WOTM_MATCH` não tem meta).
- ~~Falta: a escolha de lados com 4 colunas e o menu ("2 players" → "multiplayer").~~ **Feito em 2026-10-21** (`wotm_padsel.inc`;
  `NATIVO.md`, "Menu multijogador de 2 a 4"): "2 player" vira "multiplayer"; escolha de controles com 4 colunas; com 3 ou 4 o free for
  all sem CPU, direto ao "first to", e uma seleção de monstros nativa no telão no lugar da tela 6; a tela dividida de 3 e 4 fica sempre
  instalada. Conferido pelo roteiro de pad do boot à luta de 4. Pendentes: controles de verdade nos jogadores 3 e 4; CPU junto com
  3 ou 4 jogadores; eliminação e minigames com 3 ou mais. Testado e descartado com o dono: 5 a 8 jogadores (câmera compartilhada).
- **Relato de 2026-10-22 (colaborador, jogando com 3):** corrigidos o indicador de fúria/special do jogador 3 no HUD do jogador 1
  (animações do HUD por vista, `wotm_split4.inc`), a volta da seleção de monstros que prendia na tela 6, e a seleção redesenhada com o
  monstro 3D de cada jogador e texto legível (`NATIVO.md`, "Menu multijogador de 2 a 4"). **Custo, medido num Ryzen 3 3200G + RX 570:**
  o redesenho nativo de 3 vistas caiu de ~60 para ~36 ms (o teste de oclusão do lens flare lia a profundidade com `glReadPixels`
  síncrono em cada vista; agora os flares das vistas vêm depois de todas, com a profundidade do redesenho anterior por PBO). A lógica
  continua a 30 com 3 e 4 jogadores nessa CPU: o `hier(v, 0)` do jogo custa ~4 ms por vista (2P: quadro de ~17 ms, já no limite; 3P:
  ~21 ms), medido por marcas de tempo no quadro (a captura nativa é ~0,3 ms por vista). Pendente: baratear o `hier` por vista (a
  montagem dos pacotes do VU1 não serve ao desenho nativo) e o desenho nativo por vista (~12 ms de CPU por quadrante, contra ~7 ms
  da tela cheia).

### 12. Prévia 3D na seleção
- Hoje a seleção mostra o monstro de cada jogador; com 4 são 4 modelos carregados no Shell ao mesmo tempo.
- Base pronta (2026-10-21): `native/wotm_model.hpp` monta qualquer monstro/traje animado direto do disco, sem o Shell
  (`passos/MODELOS_3D.md`), com misturas do GS, textura rolando, LOD, fogo do Agamo e lens flare; já usado na lista de saves
  (item 5). Para a seleção falta: um `Renderer` por jogador, trocar de traje recarregando o `.NGP` e a luz do jogo.

### 13. Campanha com até 4
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no
  Capitólio, 0x1A0) precisam ser revistas.

## Brechas (baratos, em qualquer momento)
- **"Recompiled options" no menu principal** (pedido em 2026-10-21): o menu principal em texto (fonte provisória: a do jogo, a da lista
  de saves; a definitiva o dono escolhe) com um item novo que abre as opções do port. Etapas:
  1. Menu em texto com o item abrindo um painel vazio: **feito** (`wotm_mainmenu.inc`, `NATIVO.md`, "Menu principal em texto"), junto
     com as telas de modos de 1 jogador e do multiplayer (options, unlocks e load game levam a telas próprias e ficam como estão).
  2. Painel navegável com as opções do launcher valendo ao vivo, gravadas num arquivo próprio:
     **feito** (abas VIDEO, FEATURES e CONTROLS; `settings.ini`, `wotm_settings.hpp`; `NATIVO.md`, "Painel Recompiled options"); saíram do
     launcher.
     Ficaram fora do painel: partida direta, trapaças (o menu F1 já tem), opções de execução e diagnóstico; pular a intro vira opção
     do executável mais tarde. O logo do jogo no telão só aparece na tela de "press start"; nas outras o telão é todo do menu.
  3. Rebind de teclas e botões: **feito** (aba CONTROLS; `NATIVO.md`, "Rebind"). Os eixos dos gamepads seguem fixos.
  4. Estilo dos ícones (Xbox, PS, Nintendo) e qual botão confirma nos menus: **feito** (linhas BUTTON ICONS e MENU CONFIRM da aba
     CONTROLS; ícones 2D do Kenney no lugar dos 3D; `NATIVO.md`, "Ícones de botão 2D").
  5. Tirar a configuração de controle do Options do jogo: **feito** (2026-10-22; o Options foi redesenhado sem controle, vibração e
     LOAD SETTINGS; `NATIVO.md`, "Retorno do dono sobre os menus").
- **Modo foto: feito em 2026-10-22** (`wotm_photo.inc`; `NATIVO.md`, "Modo foto"): F9 ou L3 + R3 pausa e solta a câmera com zoom;
  HUD, lens flare e barras dos inimigos seguem a câmera com o jogo parado; controles na legenda de ícones; esconder o HUD e salvar PNG.
  Correções do mesmo dia, depois dos testes do dono: sair não deixa mais o jogo congelado (a volta da pausa esperava o tempo da pausa); o lens flare é refeito pela câmera
  livre (acende o de luzes fora da tela na pausa, como o olho do Robo-47 visto de frente); o fogo do Agamo aparece mesmo com ele fora
  da tela na pausa. Conferido ao vivo pelo dono em 2026-10-22; na master (merge f045a2f).
- **Gráficos de PC: feito em 2026-10-22, falta o teste do dono** (branch `graficos-pc`; `NATIVO.md`, "Gráficos de PC"): na aba VIDEO,
  RENDER SCALE (0.5X a 2X, supersampling acima de 1X), ANTI-ALIASING (MSAA 2X, 4X, 8X), TEXTURE FILTER (bilinear, trilinear,
  anisotrópico 2X a 16X) e WINDOW na RESOLUTION (o quadro segue a janela). Em 2026-10-23, prévias em imagem ao lado das
  opções da VIDEO (a mesma quina de prédio com o AA ou a resolução escolhida, o chão com o filtro escolhido etc.; `wotm_preview.hpp`). A interpolação de quadros está em "Pendências contínuas", abaixo.
- **Ícones de botão presos no mundo: feito em 2026-10-22** (`d9fd7fa`, `wotm_menu.inc`): depois do Start antecipado no menu
  principal, os ícones da legenda são realinhados à câmera a cada quadro enquanto a animação 1 anda (o `showButtons` só alinhava nos
  quadros do fade).

## Pendências contínuas (vieram do `PROXIMOS_PASSOS.md`)
- **Interpolação de quadros (escolhida pelo dono em 2026-10-18):** a apresentação já vai na taxa do monitor (`NATIVO.md`, "Taxa de
  apresentação"), mas repete o quadro do jogo. Para quadros intermediários, o nativo desenharia a mistura dos dois últimos instantâneos
  (câmera, matrizes dos objetos, paleta de ossos), com um quadro de atraso; partículas e objetos que nascem ou somem entre os dois pedem
  tratamento à parte. Rodar a lógica acima de 60 não serve: o `updateRate` sai de 60/fields e muita coisa conta em quadros. Estudo em
  `docs/investigacoes/2026-10-19/interpolacao.md`.
- **Decomp só na superfície de mod** (`Shell::Load*`, `MonsterParse`, câmera); fora dela, a fila é `config/boot_coverage_asm.csv`.
  Verificação: `tools/verify_tu.py`; funções com VU0 ou `min/max/madd`, `tools/recomp_oracle` (só folhas).
- **Fora do caminho crítico:** PR #277 no PS2Recomp (`SQRT.S`/`RSQRT.S`); oracle para funções com chamadas; cobertura com gameplay para
  realimentar o `boot_coverage_asm.csv` (`docs/investigacoes/2026-10-19/pendencias_oracle_cobertura.md`).

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
