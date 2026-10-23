# Skins de mod (traje >= 4): do `m_costume` à luta, regiões de carga e o que muda entre trajes

Investigação de 2026-10-23 para o item 8 do `POS_FRONTEIRA.md` (mods que **adicionam** trajes). Só leitura: nada foi mudado nem
rodado. Relatórios irmãos do mesmo dia: `skins_mod_save.md` (save/JSON) e `skins_mod_telas.md` (menus).

## Resumo
- O traje **só escolhe arquivo**. Do `m_costume` em diante ele vira o número no nome (`\MON\<PREFIXO><traje>.ngp/.rtx/.tex`, mais
  `mon/<PREFIXO><traje>.ptr`) e o `Monster::m_skinNum`, que só o som lê, por igualdade. Não há tabela indexada pelo traje no
  caminho de carga nem na luta. A identidade do monstro (tipo, `dup`, câmera, IA, golpes) vem do id do nó raiz do `.NGP`.
- Com `m_costume[0] = 4` e `mods/<x>/MON/LIZARD4.{NGP,PTR,RTX,TEX}` (cópia de LIZARD0), a carga e a luta deveriam funcionar
  hoje, sem mudar código: nada quebra antes da luta. O que barra o teste é **chegar** a `m_costume = 4`: menu, `WOTM_MATCH`,
  launcher, seleção de 3/4 jogadores e prévias 3D cortam em 0..3 (`& 3`, `% 4`, `> 3`).
- A RAM não é problema: acima de 32 MB a carga é **contígua**, e cada arquivo começa onde o anterior terminou. Os tetos fixos
  ficam em VRAM (`.RTX` residente e cache de texturas), em `texInfo[1000]`, no buffer do `.PTR` (0x30D40 bytes) e no nome de 8
  caracteres do `fileStatus`.

## 1. Do `m_costume` ao monstro na luta

### Carga (`Shell::LoadLevelFiles`, `src/game/Shell.cpp:1834`)
A ordem é `LoadLevelDB` → `LoadMonstersDB` → `LoadResTexture` → `LoadTexture` → `dbsRelocateFileZero` → `dbInitDb` (`:1850-1871`).
- **`LoadMonstersDB`** (`Shell.cpp:1396`). Jogadores 1 e 2 (`:1429`, `:1432`) e IAs (`:1454`) montam o caminho com
  `formatFilename1(path, MonsterLongNames[sel >> 5], m_costume[k], "ngp", ...)`, que é só `"%s\\MON\\%s%d.%s"` (`:1736-1742`).
  Em seguida registram o nome `"%s%d"` (`D_006F81D8`, `:1430/:1433/:1458`) com `fileAddName`. Jogadores 3 e 4 usam
  `formatFilename` sem traje e com o monstro do jogador 2 (peculiaridade do retail, `:1435`), e o `wotm_split4.inc` os desvia pelo
  caminho das IAs.
- **`LoadResTexture`** (`:1226`; `.rtx`, `:1265/:1267/:1284`) e **`LoadTexture`** (`:1302`; `.tex`, `:1340/:1342/:1357`) fazem o
  mesmo, com o mesmo `m_costume`.
- O modo 6 (minijogos, `:1243`, `:1319`, `:1403`) carrega sempre o traje 0 dos monstros 1-5 e 7-11.
- **Regra "0/1" das IAs (`formatFilename`, `Shell.cpp:1709-1727`, o "~1707" do pedido):** é código morto para as IAs. Nenhum
  chamador passa `SH_FILE_AI` ao `formatFilename` (as IAs vão pelo `formatFilename1`; no asm, só os loaders acima o chamam, todos com
  `SH_FILE_PLAYER` ou `SH_FILE_0`). Nada a enganchar ali.
- **`.PTR`**: `dbsRelocateViaPtrListFile` (`src/common/dbs.cpp:436`) lê `"mon/%s.ptr"` com `getGenericName(idx)`, o nome do
  `fileAddName`, e não o traje (`:455`). Com o traje no nome (`LIZARD4`) o `.PTR` certo é achado. **O nome tem no máximo 8
  caracteres** (`fileAddName`, `src/common/file.cpp:385-393`): `LIZARD10` cabe; `ASSBOSS10` seria truncado para `ASSBOSS1` e
  carregaria o `.PTR` de outro traje.

### Montagem (`dbInitDb`, `src/game/db.cpp:110`)
- Para cada `.NGP`: relocação pelo `.PTR`, depois `animationSetGlobal` nas raízes de animação do **próprio arquivo** (`:140-157`).
  Cada traje traz as suas animações.
- `dbProcInteractive` (`db.cpp:197`) manda ids 0x20..0x400 para `TheGame::MonsterParse` (`src/game/TheGame.cpp:243`). O tipo é
  `id - (id & 0x1F)`. Com `id & 0x1F == 0` é a raiz do monstro e cria `AddMonster(cs, tipo, dup)`, em que `dup` é a ordem de carga
  daquele tipo. Com `id & 0x1F != 0` é um anexo (`addAttachment`) do monstro já criado.
- **A "variante" de `m_monsterSel = monstro << 5 | variante` não tem a ver com o traje.** Os 5 bits baixos são o número do anexo
  no espaço de ids do `.NGP`. `SetPlayerMonster`/`SetAIMonster` comparam `m_typeBits == type` (`TheGame.cpp:325`, `:363`), então
  `m_monsterSel` tem que ter a variante 0 (o `RandomlySelectAI`, `Shell.cpp:348`, e o `WOTM_MATCH`, `wotm_overrides.cpp:243`, só
  gravam `monstro << 5`). O campo `variant` do JSON do save repete esses bits.

### Jogadores e IAs (`Shell::InitPlayers`, `Shell.cpp:427`)
- Monta a tabela `seen` (tipo → próximo `dup`) na mesma ordem da carga (jogadores, depois IAs). Chama
  `SetPlayerMonster(i, sel, dup, i, m_costume[i != 0])` (`:461`) e, para as IAs, `SetAIMonster(j, sel, dup, m_costume[2 + j])`
  (`spawnAIs`, `:421`; offset 0x2BE8).
- `SetPlayerMonster`/`SetAIMonster` (`TheGame.cpp:316`, `:355`) só gravam `m->m_skinNum = skin` (`:331`, `:368`). O resto depende do
  tipo e do `dup`.
- Reentradas com o traje: `EvaluateGameStatus` modo 0 (`Shell.cpp:614`, sorteia o monstro e mantém `m_costume[0]`), continues e
  coop (`:915`, `:946`, `:954`), e o `EvaluateMultiPlayerBattleStatusNoAI` (asm, lê `m_costume[0..1]` para o
  `SetPlayerMonster`). Todas repassam o valor sem olhar.
- IAs da aventura: `m_costume[2 + i] = levelMonsterModels[fase][i]` (`Shell.cpp:619`, `:753`; `screen.cpp:798`, `:3287`), uma
  tabela fixa de 0..3 no ELF.
- `RandomlySelectAI` (`Shell.cpp:346`) não mexe no traje. O `printf` dele lê `m_costume[2 + randomAiPick]`, índice errado mas só no
  log.

### Quem lê `m_skinNum` (Monster +0x30)
- `getSkinNum`/`setSkinNum` (`src/game/Monster.cpp:2428`, `:2787`) não têm chamadores no asm.
- Leituras diretas achadas: só `MonsterSound::playSignatureSound` e `playTakeHitSound` (`src/game/Sound.cpp:2104`, `:2113`,
  `:2168`). Preytor (0xA0) com `skin == 1` usa outro rugido e outro grito; Agamo (0x140) com `skin == 3` (Mecha Sweet Tooth) usa outro
  rugido. São comparações por igualdade: o traje 4 cai no som padrão do monstro. **Não há banco de som por traje**: os bancos
  (`gSoundBanks[n]`) são por tipo.
- Varredura heurística (`lw ..., 0x30(...)` junto de 0xA0/0x140 nos asm de `game/`): só as funções acima e outras que não são
  Monster. Como ~1450 funções ainda estão em asm, uma leitura de `+0x30` sem constante de tipo por perto escaparia. Nenhuma
  apareceu, mas a busca não é prova de ausência.
- **Tweaks por traje: nenhum.** Golpes, estados e tweaks vão por `m_typeBits`.

### Retrato do HUD
`monsterPortrait` (`native/runtime/wotm_overrides.cpp:1811-1870`) acha o retrato pelo arquivo que contém o nó raiz do monstro
(`fileStatus.ngpAddr[i]..[i+1]`) e o `maxResAddr` do arquivo anterior. Não usa o traje: um `LIZARD4.RTX` com o retrato na última
entrada funciona igual.

### Tudo que corta o traje em 0..3 (fora do caminho de carga)
| Onde | O quê |
|---|---|
| `native/runtime/wotm_overrides.cpp:2272` | parse do `WOTM_MATCH`: `c > 3` **invalida a partida inteira** (cai no menu) |
| `wotm_overrides.cpp:231-236, :245, :247` | `& 3` ao escolher e gravar `m_costume` e `g_extraCostume` |
| `native/runtime/wotm_split4.inc:881` | `wotmMultiSetCostume`: `costume & 3` (jogadores 3 e 4) |
| `native/runtime/wotm_padsel.inc:466-475, :577, :1136` | `costumeOpen` indexa `monsterSelectMode[(col-1)*4 + c]` (com c >= 4 lê o slot do monstro seguinte), `% 4`, prévia `& 3` |
| `native/wotm_savepics.hpp:182, :343` e `native/runtime/wotm_save.inc:918` | modelo 3D da lista de saves: `(id >> 8) & 3` (traje 4 vira 0) |
| `native/wotm_model.hpp:325-329` | `Model::load` lê **só do disco** (`wotm::disc::readFile`), não da pasta `mods/`: as prévias 3D (lista de saves, seleção de 3/4) não veem nem os trajes substituídos |
| `src/game/screen.cpp:729-730` | menus do retail: `CS_WRAP` 0..3, `CS_SLOT` = `monsterSelectMode[(sel-1)*4 + c]`; `Shell::m_costumeSw[4]`, `m_costumeSw2[4]`, `m_costumeSwMG[2][4]` são os 4 switches do SHELL (`AddEpNode` 242-249, 351-359, `Shell.cpp:1533-1565`) |
| `include/game/monster_select.h` e destrave | 48 `SelectSlot` (12 grupos de 4), save `monsterSelectMode.state`, `g_skinToUnlock`, JSON `skins` com 4 entradas |
| `tools/launcher/WotmLauncher.cs:196, :220` | combo "Skin 1..4", `Min(3, n)` |

O retail não deixa o traje passar de 3 em lugar nenhum, mas nenhum código do retail na carga ou na luta **depende** disso.

## 2. Regiões de carga: o que é fixo e o que não é

- **RAM (NGP/RTX/TEX dos monstros): não é fixa.** `fileAddNgpFile`/`fileAddResFile`/`fileAddTexFile` (`file.cpp:343-384`) põem o
  próximo endereço = este + tamanho (alinhado a 0x80). `fileReadf` devolve o tamanho real do `sceCdlFILE`, que o gancho de mods já
  troca pelo do mod (`wotm_mods.inc:33-34`). Com `loadMonstersDBHigh` (`wotm_overrides.cpp:936`) tudo depois do NGP da fase mora a
  partir de 0x2000000, com ~21 MB de folga no pior caso medido (Metro com 8, `NATIVO.md`). Um NGP/TEX maior só empurra os
  seguintes. `wotmCheckLoadEnd` (`:950`) avisa perto do fim. O AVISO do `wotm_mods.inc:8` ("regiões de tamanho fixo") **não vale
  para `MON/*` com a RAM alta ligada**. Vale com `WOTM_HIGHRAM=0` (teto 0x1F10000) e talvez para outros tipos de arquivo (bancos de
  som, SHELL), fora do escopo aqui.
  - Detalhe: o `fileReadf` lê setores inteiros e transborda até 2047 bytes depois do fim. O arquivo seguinte é carregado por cima,
    então só o último arquivo da carga transborda, como no retail.
- **`.PTR`: buffer de 0x30D40 bytes** reservado na `MemoryStack` (`dbs.cpp:450-460`). Acima disso só sai um `printf` ("Prepare for
  crash"). O maior do disco é `ALIEN0.PTR`, com 164.376 bytes.
- **Nome do arquivo: 8 caracteres** (`fileStatus.names`, `file.cpp:389-392`), e o `.PTR` é achado por esse nome (veja acima).
- **VRAM residente (`.RTX`)**: `texmResInit`/`texmResRelocate` (`src/common/texm.cpp:180`, `:618`) empilham as `.RTX` em VRAM, uma
  depois da outra (`maxTexAddr`/`maxResAddr` do arquivo anterior, TBP de 14 bits). Se a área das paletizadas passa da base, o jogo
  **sai** ("TRYING TO LOAD TOO MUCH TEXTURE INTO SPACKLE TEXTURE LOCATION", `:678-681`). Uma `.RTX` maior empurra para cima o
  início do cache de texturas (`startVramAddr`, `:241-247`).
- **Cache de texturas (`.TEX`)**: os bins são distribuídos entre `startVramAddr` e `lastVramTexAddr` (`texmInit`, `:156-175`;
  `texmLoadTexture`, `:256-300`). Se estoura, sai só um `printf` ("Texture bin allocation overflow"). `texmRelocate` (`:549`) soma os
  `texId` de todos os `.TEX` em sequência.
- **`texInfo[1000]`** (0x50F100, `texm.cpp:91`): os `texId` acumulam fase + `.TEX` e `.RTX` de cada monstro. Em 1000 sai só um
  `printf` (`:293`), sem trava: passar disso escreve depois do array.
- **`fileStatus`**: até 13 arquivos por tipo (fase + 8 monstros hoje usam 9). Não depende do traje.
- **Cópia exata de LIZARD0 como LIZARD4**: mesmos tamanhos, mesmos `texId`, mesma VRAM que escolher o traje 0. Nenhum dos tetos
  acima muda.

### O que aconteceria hoje com `m_costume = 4` e `LIZARD4.*` em `mods/`
1. `LoadMonstersDB` pede `\MON\LIZARD4.ngp`. O `fileCdSearchFile` original não acha e loga "Could not find file" (`file.cpp:611`).
   O gancho (`wotm_mods.inc:22-53`) põe o LSN falso e o tamanho do mod e loga `[mods] MON/LIZARD4.NGP <- ... (arquivo novo)`. O
   `sceCdRead` lê do mod. O nome registrado é `LIZARD4`.
2. O mesmo para `.rtx` (`LoadResTexture`) e `.tex` (`LoadTexture`).
3. `dbInitDb`: `dbsRelocateViaPtrListFile` lê `mon/LIZARD4.ptr` (o `fileAdjustFileName` vira `\MON\LIZARD4.PTR`, a chave do mod
   `MON/LIZARD4.PTR`). Raiz do `.NGP` com id 0x60 → `AddMonster(…, 0x60, dup)`.
4. `InitPlayers` → `SetPlayerMonster(0, 0x60, 0, 0, 4)` → `m_skinNum = 4` → som padrão do Togera.
5. Retrato, HUD, câmera, IA: tudo pelo tipo/arquivo.

**Nada quebra antes da luta.** O que fica errado é só cosmético e fora da luta: a prévia 3D da lista de saves mostra o traje 0, lido
do disco.

## 3. O que muda entre trajes nos arquivos
- LIZARD0/1/2: `.NGP` e `.PTR` do **mesmo tamanho**. Os `.PTR` são idênticos byte a byte. Os `.NGP` diferem em ~8,7 KB, todos entre
  0x5000 e 0x16000 do arquivo (provavelmente malha ou cor de vértice; não decodificado). `.RTX`: ~700 bytes (paletas). `.TEX`: 4
  bytes entre 0 e 1 (o traje 1 é recolorido pela paleta da `.RTX`), 65 KB entre 0 e 2.
- LIZARD3 (e ALIEN3, ENERGY3, DRAGON3, LAVA2/3, MANTIS1-3): `.NGP` de outro tamanho e `.PTR` diferente. É **malha diferente** (o
  `MODELOS_3D.md` registra um LOD extra no Togera 3), com o mesmo esqueleto e os mesmos ids de animação, já que o mesmo código de
  Monster as usa.
- Cada `.NGP` é autossuficiente: monstro (raiz 0), retratos do HUD e extras (raiz 1, GROUP id 0), COLL_GRID e ANIM_MGR com as
  animações (`MODELOS_3D.md`, "Arquivos"). Vem pré-ligado para 0xA00000, e o `.PTR` só desloca (`dbs.cpp:428-433`).
- **Traje de mod = modelo novo?** Em princípio sim, já que tudo vem do arquivo. Na prática o código de Monster assume, para o tipo:
  as animações por id (`m_anims[0..299]`, idle 0x5A), os anexos por id (`tipo | 1..31`, `addAttachment`), os canais e nós que o
  código de estado e as molas (`BoneSpring`) usam, e a colisão. O caminho seguro é **o mesmo esqueleto e as mesmas animações do
  traje 0**, mudando malha e texturas, como os trajes 3 do retail. Esqueleto novo é o problema do "monstro novo" do item 8.
  Ferramentas: `tools/ngp.py`/`pose.py`/`gltf.py`/`monglb.py` só leem. `tools/modkit.py` injeta PNG em `.TEX`/`.RTX` existentes
  (`inject_png`), o que basta para um traje recolorido a partir de uma cópia. Não há escritor de `.NGP` nem gerador de `.PTR`.

## O que precisa de gancho para traje >= 4
1. **Escolha do traje**: o menu do retail (4 switches do SHELL, `CS_WRAP`, `monsterSelectMode` de 4 por monstro) e a seleção
   nativa de 3/4 (`wotm_padsel.inc`: `costumeOpen` fora do grupo, `% 4`, `& 3`). Precisa de uma lista de trajes por monstro (disco
   + `mods/`) e de um slot de destrave fora do `monsterSelectMode` (relatório `skins_mod_telas.md`).
2. **`WOTM_MATCH` e launcher**: aceitar figurino >= 4 (`wotm_overrides.cpp:2272` e os `& 3` de `:231-247`; `WotmLauncher.cs`).
3. **Jogadores 3 e 4**: `wotmMultiSetCostume`/`g_extraCostume` com `& 3` (`wotm_split4.inc:881`, `wotm_overrides.cpp:245`).
4. **Prévias 3D**: `wotm_savepics.hpp`/`wotm_padsel.inc` (`& 3`), e `Model::load` precisa consultar o registro de mods.
5. **Save/JSON**: o valor passa sem corte (`MonsterMc::setSaveData` copia o slot+0x10 direto para `m_costume[0]`; `setReelSwitches`
   não usa o traje). Falta nome por traje e destrave no JSON (relatório `skins_mod_save.md`).
6. **Nome curto**: prefixo + número <= 8 caracteres (`fileAddName`). Traje >= 10 só com prefixo de até 6 letras.
7. **Opcional**: som por traje (hoje, igualdade com 1/3 em `Sound.cpp`). IAs da aventura com traje de mod (`levelMonsterModels`).
8. **Tetos para trajes maiores que o original** (não para uma cópia): `texInfo[1000]`, VRAM residente (sai do jogo), buffer de
   0x30D40 do `.PTR`. Vale um aviso no log de carga com o `maxTexId` final e a VRAM usada, e trocar o texto do AVISO do
   `wotm_mods.inc:8`.

Não precisa de gancho: `formatFilename1`, `LoadMonstersDB`/`LoadResTexture`/`LoadTexture`, relocação, `MonsterParse`/`AddMonster`,
`SetPlayerMonster`/`SetAIMonster`, `InitPlayers` e retrato do HUD.

## Teste mínimo proposto
Arquivos: `mods/teste/MON/LIZARD4.NGP`, `.PTR`, `.RTX`, `.TEX` = cópias de `disc/MON/LIZARD0.*`.

- **A, sem mudar código (pelo menu, passo a passo para o dono):** num `discroot/save.json`, um slot de aventura com
  `"monster": "togera"`, `"variant": 0` e `"costume": 4`. Carregar esse slot pela tela de carregar. Esperado no log: as 4 linhas
  `[mods] MON/LIZARD4.* <- ... (arquivo novo ...)`, `Reading \MON\LIZARD4.NGP`, `set player monster 96 0` e a luta normal com o
  Togera do traje 0. A prévia da lista de saves mostra o traje 0 (corte `& 3`), o que é esperado.
- **B, com uma mudança pequena em `native/` (sem gate), que já é o ponto 2 da lista acima:** o parse do `WOTM_MATCH` aceita
  figurino 0..9, e os `& 3` de `wotm_overrides.cpp:231-247` saem (com `c > 3` como "não toma"). Depois
  `WOTM_MATCH=3:4,3:0,1` (Togera traje 4 contra Togera traje 0 em central): confere dois arquivos do mesmo tipo com `dup` 0/1 e o
  traje de mod ao lado do original.
- Para ver diferença na tela: repetir B com `LIZARD4.*` = cópia de `LIZARD3.*` (malha e `.PTR` diferentes) ou com um PNG
  injetado no `LIZARD4.TEX` pelo `modkit.py`. A cópia de LIZARD0 testa só o caminho de carga.
