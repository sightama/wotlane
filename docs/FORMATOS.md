# Formatos de arquivo do jogo (o que o código decompilado revela)

Fonte: `Shell::LoadLevelFiles`, `LoadLevelDB`, `LoadMonstersDB`, `LoadResTexture`, `LoadTexture` (`src/game/Shell.cpp`),
`dbsRelocateFileZero`, `dbsRelocateViaPtrListFile` (`src/common/dbs.cpp`), `dbInitDb`, `dbProcInteractive` (`src/game/db.cpp`).
Tudo abaixo foi lido do código; o que está marcado (?) ainda é suposição.

## Arquivos por fase e por monstro
| Extensão | Conteúdo | Carga |
|---|---|---|
| `lvl\<fase>.NGP` | imagem do nível (hierarquia de nós), comprimida em zip | `zipInflateAll(path, 0xA00000)`; depois `fileOnlyNgpFile` |
| `lvl\<fase>.TEX` | texturas do nível | `getNextNgpLoadAddr` + `fileOnlyTexFile` |
| `lvl\<fase>.RTX` | texturas de recurso (`res`) do nível | `fileOnlyResFile` |
| `lvl\<fase>.PTR` | lista de correções do arquivo zero (ver abaixo) | `dbsRelocateFileZero` |
| `<monstro><costume>.ngp/.tex/.rtx` | um monstro (modelo, textura, recursos); `<monstro>` vem de `MonsterLongNames`, `<costume>` é o número do traje | `fileReadf(path, getNextNgpLoadAddr/Res/Tex())` |
| `mon\<nome>.ptr` | correções de ponteiros do monstro `<nome>` | `dbsRelocateViaPtrListFile` |
| `shell\shell|load|ui|shella|preshell.ptr` (+ `.ngp/.tex/.rtx`) | telas do shell (menus, loading, UI) | idem arquivo zero |
| `host0:monster.ngp/.tex/.rtx` | caminho de desenvolvimento (PC host), usado quando o índice da fase é >= 0x1D |

Nomes dos monstros (`MonsterLongNames`, 8 bytes por entrada, índice = monstro): `null, monkey, robot, lizard, shogun, mantis, spider,
energy, lava, dragon, rock, alien, ogre, assboss, jelly, final, temp16` (17 entradas; o `default` logo antes no binário é a fase 0). A seleção de cada vaga é `(monstro << 5) | variante`
em `Shell::m_monsterSel[]` (4 jogadores, depois IAs); o traje fica em `m_costume[]`. No modo 6 (mini-games?) carrega os monstros 1..5 e 7..11.
Quirk do retail: para o 3º e o 4º jogador o caminho do arquivo usa o monstro do jogador 2.

### Nomes de arquivo (`Shell::formatFilename`, `formatFilename1`)
Todos começam pelo prefixo da raiz do disco (`D_006F81E0`, string vazia no retail), então os caminhos ficam `\LVL\...` e `\MON\...`.
| Chamada | Tipo | Caminho |
|---|---|---|
| `formatFilename(dst, nome, ext, tipo)` | `SH_FILE_0` (fase) | `\LVL\<nome>.<ext>` |
| | `SH_FILE_PLAYER` | `\MON\<nome>.<ext>` |
| | `SH_FILE_AI` | `\MON\<nome>0.<ext>` ou `\MON\<nome>1.<ext>` (abaixo) |
| `formatFilename1(dst, nome, traje, ext, tipo)` | `SH_FILE_PLAYER`/`SH_FILE_AI` | `\MON\<nome><traje>.<ext>` (traje em decimal) |
| | `SH_FILE_0` | `\LVL\<nome>.<ext>` |
| `Shell::formatFilename(dst, dir, nome, ext)` (estática) | arquivos dos point tools no host | `<dir>/<nome>.<ext>` |

Sufixo das IAs em `formatFilename(..., SH_FILE_AI)`: `1` se `m_mode` é 0 ou 1 e `m_levelNum == 3`; senão `0` se `m_mode != 1`; no modo 1, se a
IA da vaga 4 ou 5 usa o mesmo monstro do jogador 1 (`m_monsterSel[0]`), `1` quando o jogador 1 está no traje 0 e **nada é escrito** quando
ele está em outro traje (quirk do retail: o `dst` fica como estava); sem esse conflito, `0`. Ou seja, a IA carrega a outra pele para não
ficar igual ao jogador 1.

O `LoadLevelFiles` procura `<raiz>\LVL\<fase>.NGP;1` e `.TEX;1` no CD (`fileCdSearchFile`) para saber os tamanhos (barra de progresso) e
carrega, nesta ordem: nível (`.ngp`), monstros, texturas de recurso, texturas; depois `dbsRelocateFileZero` e `dbInitDb`.

## Filmes `.PSS`
`disc/MOV/*.PSS` (52 arquivos). MPEG-2 program stream em pacotes de 2048 bytes (cabeçalho de pacote `00 00 01 BA` de MPEG-2, um cabeçalho de sistema `BB`; `tools/pss.py` e o `ffprobe` mostram a estrutura):
- **Vídeo:** fluxo `E0`, MPEG-2 MP@ML a 29,97 quadros/s. Tela cheia em 640x368 com proporção 16:9: `INTRO_M`, `INTRO_S`, `INT_MAIN`, `INT_ACT1..3`, `INT_TRAN`, `<monstro>_G` (finais), `<monstro>_P`, `CREDITS`. Os outros são pequenos e sem áudio: 256x128 (`SHELDEMO`, o filme do menu, com 173 s; nomes de monstro e de fase como `ALIEN`, `CENTRAL`, `TOKYO`: prévias, provavelmente), 254x128 (`3-MILE`, `AIRPORT`, `ISLAND`) e 256x160 (`MANTIS`, `ROBOT`).
- **Com áudio** (nos primeiros 4 MB): `INTRO_M`, `INT_MAIN`, `CREDITS` e os `_G`/`_P`, menos `ALIEN_G` e `DRAGON_G`. `INT_ACT1..3` não têm (o `playMovie` os chama com `comAudio` = 0; ele também cita um `int_act4.pss` que não existe no disco); `INTRO_S` e `INT_TRAN` também não, e não aparecem no `playMovie`.
- **Áudio:** fluxo privado `BD`, subfluxo `FF`, 4 bytes de cabeçalho por PES. Começa com `"SShd"`, tamanho (0x18), tipo (1 = PCM 16 bits little-endian), taxa (48000), canais (2), intercalação (0x200) e dois `-1`; depois `"SSbd"`, tamanho dos dados e os dados: blocos de 512 bytes alternando canal esquerdo e direito (256 amostras cada). O FFmpeg não reconhece esse áudio (só vê o vídeo).
- **No jogo:** o retail toca por `sceMpegDemuxPssRing` + IPU; o nativo (`native/wotm_fmv.hpp`) lê o arquivo direto. O filme do menu vira a textura de um objeto do menu 3D: `moviesSetScreenObject` troca o texId por 999 (o último de `texInfo[1000]`, reservado) e o TEX0 por CT32 em 0x3BA0, largura 320 (`VramAddrForMovies` 0xEE800 palavras / 64, `VramWidthForMovies` 0x140), com `TW`/`TH` = 8/7.

## Som: `.BNK`, `.VPK`, `.VAG` e a API do 989snd
Lido com `tools/snd989.py` (bancos, estatísticas, exportação WAV) e reproduzido por `native/wotm_snd.hpp` (`native/sndplay.cpp` grava WAV sem o jogo).

**Caminho no EE.** `game/Sound` (`SoundManager`, sons de monstro, veículos, armas, fases), `game/StreamingSoundManager` (músicas e falas em stream) e `common/SoundBank` (`SoundBankManager`: carga e descarga de bancos) chamam as `snd_*` da `lib989snd`. Cada `snd_*` empacota os argumentos e manda um comando numerado ao driver 989snd no IOP por `sceSifCallRpc` (`snd_SendIOPCommandAndWait`/`NoWait`, com lote em `snd_SendCurrentBatch`, comando RPC 0x4D). Exemplos: 0x09 `SetMasterVolume`, 0x11/0x12 `PlaySoundVolPanPMPB` (com/sem espera), 0x15 `StopSound`, 0x18 `StopAllSounds`, 0x19 `SoundIsStillPlaying`, 0x21 `SetSoundParams`, 0x2A `InitVAGStreamingEx`, 0x2C `PlayVAGStreamByLoc`, 0x2F `StopVAGStream`, 0x38 leitura de CD "stream-safe", 0x4A `SRAMGetFreeMem`. As variantes `_CB` entregam o resultado chamando `cb(valor, user)` no EE (o jogo quase sempre passa `soundPlayCB`, que grava `*user = valor`); as `_A` não devolvem nada. Os IRX `LIBSD`, `989SND` e `989ERR` sobem em `inputInit`. O jogo usa umas 40 `snd_*`; não usa MIDI (`snd_MMDLoad*` nunca é chamada).
- `snd_BankLoadByLoc(lsn, 0)` / `_CB`: banco pelo setor do disco (`\SND\<NOME>;1` via `fileCdSearchFile`); devolve um ponteiro do IOP usado só como identificador.
- `snd_PlaySoundVolPanPMPB(banco, som, vol, pan, pitchMod, pitchBend)`: vol 0..0x400 (0x400 = volume do banco), pan em graus (0 = frente, 90 = direita, 270 = esquerda; -1 = o do banco). Devolve um handle; `SoundIsStillPlaying(h)` devolve `h` ou 0.
- `snd_SetSoundParams(h, máscara, vol, pan, pm, pb)`: máscara 1 = volume, 4 = pan, 8 = pitch mod, 0x10 = pitch bend (deduzida das chamadas do jogo: 5, 0xD, 0x15).
- `snd_SetMasterVolume(grupo, vol)`: grupos usados 0 (efeitos), 1 (música), 3, 4, 5, 6, 8. Cada som do banco tem seu grupo; os streams recebem o grupo na chamada.
- `snd_PlayVAGStreamByLoc(lsn, lsn2, off1, off2, vol, pan, grupo, fila, flags)` (+ `cb, user` na pilha no `_CB`): o handle vale para `SetSoundParams`, `StopSound` e `SoundIsStillPlaying`. Nas fases, a música ambiente (`*AMB.VPK`) e a de batalha (`*BAT.VPK`) começam juntas com volume 0 no grupo 1 e o `StreamingSoundManager` faz o crossfade por `SetSoundParams`.
- `snd_StreamSafeCd*`: só desviam para o IOP depois de `snd_InitVAGStreamingEx`; antes disso viram `sceCdRead`/`sceCdSync`.

**`.BNK` (`disc/SND`, 43 arquivos).** Cabeçalho: `u32 tipo (3)`, `u32 nChunks (2)`, `{u32 offset, u32 tamanho}` por chunk; chunk 0 = bloco `SBlk`, chunk 1 = amostras ADPCM do SPU2 (o que o driver copia para a SRAM). `SBlk` versão 1 (o mesmo formato que o 989snd do Jak & Daxter, lido pelo OpenGOAL): `"SBlk"`, versão, flags (4 em todos: sem nomes nem dados de usuário), id de 4 letras ao contrário (`LEHS` = SHEL), `s16` número de sons/grains/VAGs em 0x16, `FirstSound` (0x1C) e `FirstGrain` (0x20). Som (12 bytes): `s8 vol, s8 grupo, s16 pan, s8 nGrains, s8 limite de instâncias, u16 flags (1 = laço), u32 primeiro grain`. Grain v1 (0x28 bytes): `u32 tipo, s32 atraso` em ticks de 240 Hz e 0x20 bytes de parâmetros. O `TONE` (tipo 1) toca uma amostra: `s8 prioridade, vol, CenterNote, CenterFine; s16 pan; s8 mapLow, mapHigh, pbLow, pbHigh; u16 ADSR1, ADSR2, flags; u32 offset da amostra`. Volume e pan negativos leem registradores (-1..-4 do som, -5 aleatório, -6.. globais). Os efeitos tocam na nota 60; `CenterNote` negativa é nota de PS2 (base 48 kHz), positiva de PS1 (44,1 kHz), e o `CenterFine` soma ao fine (`sceSdNote2Pitch`).
- Contagem nos 43 bancos: 851 sons; grains `TONE` 2593, `RAND_PB` 368, `SET_REG_RAND` 158, `RAND_DELAY` 101, `LFO` 90, `RAND_PLAY` 63, `MARKER` 62, `TEST_REG` 56, `GOTO_MARKER` 43, `LOOP_START`/`END` 39, e poucos `ADD_REG`, `GOTO_RANDOM_MARKER`, `PLAY_CYCLE`, `STARTCHILD`, `CONTROL_NULL`, `ON_STOP_MARKER`, `ADD_PB`, `SET_REG`, `KEY_OFF`, `WAIT_ALL_VOICES`, `PB`, `STOP`, `DEC_REG`. Nenhum XREF, plugin nem `BRANCH`.
- Nomes: `<MONSTRO>_S` (efeitos do monstro) e `<MONSTRO>_LS` (versão de menor qualidade, `loadLoResMonsterBank`); `<FASE>_S`; `GENAMB_S`/`GENAM_LS` (ambiente comum), `GENMON_S`, `SHELL_S` (menu), `COUNT_S`, `FINAL_S` etc.
- **`SHELLMUS.BNK` (1,7 MB) não é sequência MIDI:** 1 som com laço, 2 grains `TONE` (pan 270 e 90 = canal esquerdo e direito) e 2 amostras longas a 44 058 Hz. É a música do menu, sampleada.

**`.VPK` (`disc/VAG`, 51 arquivos).** `" KPV"`, `u32 bytes por canal`, `u32 início dos dados` (0x800), `u32 tamanho do par de blocos` (0xA000), `u32 Hz`, `u32 canais` (2 em todos), e `{u16 vol, u16 pan}` por canal (127/270 e 127/90). Os dados alternam blocos de **0x5000** bytes de cada canal (o campo é o par esquerdo+direito: os meio-blocos 0 e 1 têm correlação ~0,6 entre si e ~0 com o seguinte, em CITYAMB, TOKYOBAT e CONGAI), em ADPCM do SPU2 sem flags de fim nem de laço. `*AMB`/`*BAT` (música das fases, ~2 min a 44,1 kHz); os outros (falas, cinemas, `*I` de introdução, `*D`, `*T`) a 48 kHz.

**`.VAG`** (`DEFEAT`, `VICTORY`, 2 KB cada): `"VAGp"` big-endian com 0x30 bytes de cabeçalho (tamanho dos dados em 0x0C, Hz em 0x10, nome em 0x20); praticamente vazios.

**De volta para o jogo (`tools/snd989.py`).** `vpk`, `vag` e `bnk-replace` codificam WAV (PCM 8/16/24/32 bits ou float) no ADPCM do SPU2. O codificador simula o decodificador bloco a bloco (filtro e shift de menor erro), então o que ele grava é exatamente o que o jogo reconstrói; ~40 dB de SNR em música, ~1 s de processamento por segundo de áudio estéreo. No `.VPK` o campo de bytes por canal vai em blocos inteiros de 0x5000 (o leitor do nativo lê só `início + bytes por canal × canais`), o que deixa até 0,8 s de silêncio no fim. O `bnk-replace` reamostra (linear) para a taxa do `TONE`, mantém o laço da amostra original e corrige os offsets dos outros sons, o tamanho do chunk e os campos de tamanho do `SBlk` (0x28 e 0x2C, que acompanham o total das amostras; significado exato desconhecido). Falta testar no jogo com arquivos reais.

**ADPCM do SPU2.** Blocos de 16 bytes: `(filtro << 4) | shift`, flags (1 = fim do trecho, 2 = repete a partir do início do laço, 4 = início do laço) e 14 bytes com 28 amostras de 4 bits; filtros `(0,0) (60,0) (115,-52) (98,-55) (122,-60)` / 64.

**`.MRS` (`disc/RST`) não é áudio:** 48 KB de floats por fase.

## Imagem `.NGP`
Pré-ligada para o endereço **0xA00000** (a de nível é descompactada lá). Palavra 0 = N (número de nós-raiz), palavras 1..N = ponteiros
para os nós-raiz da hierarquia (`_hierhead`; opcode nos 6 bits baixos da primeira palavra, id do objeto nos 14 bits altos).

## Correções (`.PTR`)
Três (arquivo zero) ou quatro (monstros) listas, cada uma `contagem` seguida de `contagem` offsets em bytes dentro da imagem (0 = não usado):
- **arquivo zero** (`dbsRelocateFileZero`): as duas primeiras listas são puladas; a terceira lista os doublewords com `TEX0` do GS, e soma
  `vram / 64` ao campo `TBP0` (14 bits, bits 37..50 do doubleword).
- **monstro** (`dbsRelocateViaPtrListFile`, `idx` > 0): (cabeçalho) palavras 1..N += `endereço_real - 0xA00000`; (1) ponteiros (mesma soma);
  (2) ids de textura de 16 bits += maior id do arquivo anterior; (3) palavras do GS: `TBP0` += fim das texturas do arquivo anterior, e as
  marcadas 0x1B/0x2C/0x24 somam ao campo baixo de 14 bits o fim dos recursos do arquivo anterior.

## Ids de objeto do nível (`dbProcInteractive`)
Id = 14 bits altos da primeira palavra do nó. Os nós de opcode 0x1F e 0x27 são sempre destrutíveis.
| Faixa | Significado |
|---|---|
| 1 | raiz do mundo (`viewSetWorldEpNode`) |
| 2 | céu (`viewSetSkyEntry` x4) |
| 18 | textura da fonte |
| 0x20..0x400 | monstros (`TheGame::MonsterParse`) |
| 0xBB8 | grupo de pedestres (`PedGroup`, 0x190 bytes na pilha de memória; byte `(head>>7)&0xFF` = tamanho) |
| 0x7D0..0x897 | ignorados aqui |
| 0xFA0..0x11F7 | armas (`Weapons::AddWeaponEpNode`) |
| 0x11F8..0x12BF | efeitos: 0x11F8/0x1203 `HomingBug`, 0x11FC bola da `OgreMace`, 0x11F9..0x11FA/0x11FD/0x11FE/0x11FF/0x1201 `SpecFxAnim` |
| 0x12C0..0x13EB | pickups (`LevelPickups::createPickup`); o id vira o "hat id" do pickup |
| 0x13EC..0x144F | power-ups |
| 0x1450..0x1B57 | destrutíveis (`Destructibles::AddDestruct`) |
| 0x1B58..0x1C1F | texturas de partícula |
| 0x1C20..0x2133 | elementos do HUD |
| 0x2134..0x2327 | scripts/ações (0x2134/0x2135/0x2136 só em certos modos; 0x2260.. scripts do usuário) |
| 0x2328..0x270F | pontos de fim do shell (`Shell::AddEpNode`) |
| 0x2710..0x3E7F | pontos de fim por fase (threemile, tokyo 0x2743, canyon2, airport, FinalBoss, BigShot) |

Os pickups guardam o id do slot de `Interactives` no `hierhead`: `Interactives::addInteractive` devolve esse índice.

## Malhas (`HierObject::polyPkt`)
Leitor de referência: `tools/ngp.py` (Python, offline, direto do disco) e `native/wotm_native.hpp` (`decodeObject`, C++, lê a RAM do EE).
Um nó `OBJECT` (`_hierobject`, 0xA0 bytes) aponta com `polyPkt` (+4) para uma sub-cadeia de DMA chamada pelo `pktAddVu1ObjAsm` (tag `CALL`; o pacote termina em `RET`):
- Palavra 0: tag DMA com `QWC` nos 16 bits baixos e `ID=6` (`ret`) em `0x60000000`; o pacote tem `QWC + 1` quadwords. As palavras 2 em diante são um **fluxo VIF** para o microcódigo do VU1.
- **Posições**: `UNPACK V3-32` em `0xB5` (até 190 vértices, floats XYZ). **Normais**: `UNPACK V3-8` em `0x175`, bytes com sinal (/127), 3 por normal sem preenchimento.
- Uma ou mais **seções de strip**, cada uma terminada por `MSCNT`. A seção começa com uma **tag GIF** (`UNPACK V4-32`, 1 elemento, em H = `0x238`/`0x2C8`/`0x358`, alternando por causa do buffer duplo do VU1):
  `NLOOP` (bits 0..14) = número de vértices, `PRIM` (bits 47..57; triângulo-strip com Gouraud, textura e fog) e `REGS` = ST, RGBAQ, XYZF2. Depois, com `STCYCL CL=3 WL=1`, cada vértice ocupa 3 quadwords:
  `S-8 @H+1+3k` **índice** da posição (0..189), `V4-8 @H+2+3k` **RGBA** (`0x80` = 1,0; o alfa também), `V2-16 @H+3+3k` **UV** em ponto fixo (`4096` = 1,0; negativo e fora de [0,1] repete a textura).
- **Quebra de strip (ADC)**: `STMASK 0xBFBFBFBF` seguido de `UNPACK S-8 num=2 @H+3+3k` marca os vértices `k` e `k+1` como "não desenha" (bit ADC do XYZF2): o strip recomeça em `k`. Strips de 8 vértices são comuns (cilindros/tubos). Junções por vértice repetido geram triângulos degenerados (descartar).
- Uma nova `UNPACK V3-32 @0xB5` troca a tabela de posições (objetos grandes têm vários lotes de até 190 vértices).
- `_hierobject.bits.numVerts` (13 bits) é o total de vértices do objeto; `texId[5]`/`numTextures` e `gsTexCtx[i]` (+0x70, 0x30 bytes cada: `TEX0`, `TEX1`, `MIP`) descrevem as texturas. Esfera de colisão/visibilidade em +0x10 (centro) e +0x1C (raio); `sphereRadiusSqrd` em +8.
- Convenções do mundo: vetores-linha (`p' = p * M`), **Z para cima**; matrizes de `ROTATE` em +0x10 (4x4), `TRANSLATE` soma `trans` (+0x10), `SCALE` aplica `scale` (+0x20) depois `trans` (+0x10).
- Nós percorridos pelo port: `GROUP` (filhos em +0x20, contagem u16 em +8), `LOD` (lista de `{fade, switchOutDis, child, pad}` em +0x20; o 0 é o mais grosseiro e o último o mais detalhado; o jogo escolhe o maior índice cujo `switchOutDis`, uma distância ao quadrado, é maior que a distância ao centro do LOD), `SWITCH` (filho `whichChild` em +0xC), `CONTROL` (`child1` em +4), `ACTION_DATA` (filho em +4), `CHAR_INSTANCE` (filhos em +0x2C, contagem em +0x28), `INTERACTIVE` (filho em +0xC), `DESTRUCTIBLE` (estados em +0xC, 12 bytes cada; o 0 é o intacto).

## Texturas e a VRAM do GS (em andamento)
O `TEX0` de cada objeto (`gsTexCtx[0].tex0`) usa o formato do GS (`TBP0` bits 0..13, `TBW` 14..19, `PSM` 20..25, `TW` 26..29, `TH` 30..33, `CBP` 37..50, `CPSM` 51..54, `CSM` 55, `CSA` 56..60). Os pixels chegam à VRAM por uploads GIF `IMAGE` do `.TEX` (e o `.RTX`).
`native/wotm_gs.hpp` decodifica `CT32/CT24/CT16/T8/T4` a partir de um dump da VRAM (4 MB); a paleta de 256 entradas (CSM1) está em memória linear com as entradas 8..15 e 16..23 de cada grupo de 32 trocadas.
O runtime grava `ram_<quadro>.bin` e `vram_<quadro>.bin` com `PS2X_DUMP_RAM="quadro,..."` (`PS2X_NO_GS=1 PS2X_GS_UPLOADS=1` processa só os uploads, sem rasterizar).

**Arquivos `.TEX` e `.RTX` (lidos do `src/common/texm.cpp`).** Ferramenta: `tools/tex.py` (`list`, `export` para PNG com
`manifest.json`, `inject` de PNG/TGA).
- Podem vir crus ou num zip de um arquivo com o magic `IE\3\4` (o `Shell` lê `shell/*.rtx|tex` por `zipInflateAll`).
- Cabeçalho `_texfile`: `u32 numSizes`, `u32 headerQw` (o primeiro pacote fica em `headerQw * 16`) e `{u32 count, u32 blkSize}` por
  faixa de tamanho (as "bins" da VRAM, `texmLoadTexture`).
- Pacote `TexPkt` (16 bytes de cabeçalho): `u32 size` (o próximo está `(size >> 2) * 16` bytes adiante; 0 = último), `u32`,
  `u16 texId`, `u16 tbp` (bloco de 64 palavras da VRAM), e os bits `psm` 0..5, `noUpload` 6, `skip` 7, `logW` 8..11, `logH` 12..15,
  `numMips` 16..18, dois campos de 4 bits ainda sem nome (19..22 vai ao `texmSetupTexture`) e `bin` 27..31.
- Pixels lineares (linhas de `w` pixels; T4 com o nibble baixo primeiro). No `.TEX` começam em `+0x80` (o jogo monta o
  `sceGsLoadImage` do SDK em `+0x20`). No `.RTX`, os formatos altos (T8H/T4HL/T4HH: telas do menu, fontes, retratos) também em `+0x80`;
  o resto (paletas CT32 16x16 e 8x2) em `+0x10` (`texmLoadResTexture`).
- Relocação na carga: `texmRelocate` soma ao `texId` de cada `.TEX` depois do primeiro o maior id do anterior (e remapeia `bin`);
  `texmResRelocate` soma ao `tbp` das paletas a base da VRAM e o fim do `.RTX` anterior, e ao `tbp`/`texId` dos formatos altos o
  fim dos anteriores. O `TEX0` dos objetos do `.NGP` liga cada textura (`texId`) à paleta (`CBP` = `tbp` da entrada do `.RTX`).
  Como os números no `.NGP` podem já estar somados, o `tex.py` acha os dois deslocamentos pela maior quantidade de objetos que
  casam formato e tamanho; precisa de conferência com arquivos reais.

## Partículas e efeitos (`common/particle`, `common/partdraw`)
Leitores: `native/wotm_fx.hpp` (C++, desenha) e `tools/fx.py` (Python: sistemas ativos de um dump de RAM e `--presets` com o catálogo do ELF).
O layout de `_ParticleType` (12000 bytes), `EmitterType`, `FieldInfoType`, `ParticleInfoType` e `VisualInfoType` está em `include/hieri_types.h` (stabs do `.mdebug`).
- **Pool**: `fx[100]` em `0x5BA280` (stride `0x2EE0`). Lista ativa: `fxActiveTop` (`0x6F88EC`) é o primeiro índice e `fxList[i]` (`0x6DF200`, 12 bytes) guarda o próximo em `+8`; `-1` termina. `fxInactiveTop` (`0x6F88F0`) é a lista livre.
- **Presets**: `particleInitFx(tipo, ...)` copia as linhas `tipo` de `EmitterInfo` (`0x5B2020`, 80 bytes), `FieldInfo` (`0x5AEFF0`, 80), `ParticleInfo` (`0x5B4F70`, 96) e `VisualInfo` (`0x5ADD00`, 32) para o sistema; são 151 presets, dados inicializados do ELF. O índice do preset parece ficar em `emit.fxId` (`+0x2DDC`): `particleMove` o compara com 0x91 e 0x93 para efeitos de borda; ainda não confirmado em dump. Os nomes estão no enum `FxId` do próprio jogo (`include/hieri_types.h`): 0 `Dust`, 2 `OpaqueDust`, 5 `DustTrail`, 9..12 explosões (`WhiteExplosionFx`, `SmallExplosionFx`, `ExplosionFx`, `SmokeExplosionFx`), 15..18 vapor e fumaças, 21..26 faíscas, 28..32 clarões (`CounterFlash` .. `BlueWhiteFlash`), 43..48 fogo (`Flames`, `AttachedFlames`...), 105 `FootStomp`, 146 `WaterWake`. O `vis.texInfoIndex` é um `FxTextureId` (`Smoke1`..`Smoke5`, `TexExplosion`, `Fireball`, `TexFlames`, `Spark`, `DustTexAlpha`, `CompositedDust`...).
- **Por partícula** (ponteiros dentro do próprio pacote `pkt`, montados no `*PacketInit`): `pos[i]` (`+0x2EB0`, `FVECTOR`; **`w` é o tamanho**, tratado como raio por `particleFOVcalcMaxRadius`, que soma `maxSize + expansionRate * lifetime`), `vel[i]` (`+0x2EB4`; `vel.w` é a taxa de expansão somada a `pos.w` por campo) e `age[i]` (`+0x2EB8`, `int16`, em campos). `numParticles` em `+0x2ED0`, `maxParticles` (256 nos sprites) em `+0x2ED4`. Os bits baixos de `pos.w` são preservados em `particleMove` (máscara 3, ou 0xF com `info.attribs & 0x2000`) e escolhem um dos 4 ou 16 conjuntos de cantos de UV que o `spritePacketInit` sobe.
- **Tipo de desenho** = `vis.attribs` (`+0x2E90`), pela tabela de saltos do `particlePrepDraw`: 1 `line` (sobe `pos` e `vel`), 2 `sweepTrail`, 3 `sprite`, 4 `texSprite`, 5 `stripe`, 6 `strite`, 8 `sprine`, 9 `kite`, 13 `frag`, 15 `dustTrail`, 16 `wake`; 7, 10..12 e 14 não montam pacote ali. Mesmo assim, a checagem VU1 x nativo (`docs/passos/VU1CHECK.md`) viu o VU1 desenhar sistemas do tipo 7 e do tipo 14 (pedestres) a partir do `pkt` do próprio `fx[i]`, então outro código monta esses pacotes. Arcos elétricos (`monArcParticle`), correntes e lens flares têm caminho próprio.
- **Quem desenha** (`particleDraw`): `info.attribs & 0x400` (pacote montado neste quadro), `numParticles >= 1` (`sprine`: `>= 2`), tipo diferente de 11 e a esfera `FOVsphere`/`FOVextent` (`+0x2C80`/`+0x2C90`) dentro da vista. A ordem é do mais longe ao mais perto pela profundidade (eixo Y do olho) de `emit.ParticleToEmitVec` (`+0x2CA0`) mais `vis.priority`; sistemas com `alphaContext == 0` (opacos) recebem a chave 50000 + prioridade e vão primeiro.
- **Cor** (`loadColors`, igual no microcódigo dos sprites): tabela RGBA em `0x256C10` (`0x80` = 1,0, alfa incluso), `vis.numColors` cores a partir de `vis.colorIndex`. `t = age * info.colorsPerFrame` (4.12); a cor é a interpolação entre `cor[t >> 12]` e `cor[(t >> 12) + 1]` por `(t & 0xFFF) / 4096`. Zero cores = `0x80808080`; uma cor = constante. Quase todas as tabelas terminam com alfa caindo, e é assim que a partícula some.
- **Mistura**: `vis.alphaContext` é o registrador `ALPHA` do GS escrito no contexto do pacote: `0x44` = `(Cs - Cd) * As + Cd` (normal), `0x48` = `Cs * As + Cd` (aditivo: fogo, faíscas, clarões).
- **Textura**: `TextureInfo[vis.texInfoIndex]` (`0x5B9E80`, 8 bytes: `numTextures`, `texIndex`, `du`, `dv`; preenchida em tempo de execução por `particleParseTexture`). `texId[texIndex]` (`0x6DFAB0`, `u16`) é o índice no `texInfo` global (pixels do pacote de upload, como nos objetos) e `TEX0[texIndex]` (`0x6DF6B0`, `u64`) é o registrador `TEX0` (psm, CBP). O `texSprite` escolhe um quadro por partícula: `info.attribs & 1` aleatório, `& 2` `uvI[i] >> 2`, senão `(age * info.mapsPerFrame) >> 12`, travado no último com `& 4` ou em módulo `vis.texMask`.
- **Microcódigo de partículas do VU1**: não é o microcódigo dos objetos (`0x257C10`); a `displayList` das partículas chama um pacote próprio em `0x253060` (954 qwords: 1884 instruções por `MPG` e, no fim, o contexto padrão em `0x256B90`, 8 qwords: matriz mundo→tela, constantes e `vf27` = focos 320/160). Cada `*PacketBuild` termina com `MSCAL` no seu programa (`D_58` linha, `D_2C8` sprite, `D_800` dustTrail, `D_A88` strite, `D_F18` texSprite, `D_1180` frag, `D_13A0` kite, `D_1708` stripe, `D_19F8` arco elétrico, `D_1D00` corrente, `D_2770` sprine, `D_2B20` lens flare, `D_3318` sweepTrail, `D_3470` wake; endereços em bytes, `MSCAL` usa /8). Lido: no **sprite**, o meio-lado na tela é `pos.w * foco / profundidade`, limitado a `foco / 2` (então `pos.w` é o meio-lado em unidades de mundo e não passa de metade da profundidade); os bits baixos de `pos.w` indexam os 4 conjuntos de UV do `spritePacketInit` (3 normal, 1 espelha U, 2 espelha V, 0 os dois; com `0x2000`, 16 conjuntos); a cor vem da tabela pela idade. Na **linha**, a cauda é `pos - vel`, projetada com a matriz do quadro anterior (o contexto leva as duas: borrão de movimento da câmera). No **strite**, cabeça `pos` e cauda `pos - vel` projetadas e esticadas 2x para cada lado (de `pos + 2*vel` a `pos - 3*vel`), largura por `pos.w`: um triângulo equilátero por partícula com ST (0,5; 1,5) na ponta e (1,366; 0) e (−0,366; 0) na base, que circunscreve o quadrado da textura (o disco fica inscrito); o alfa da cor sai multiplicado por K/(len + K), com `len` o comprimento do rastro na tela e K = 2·foco·`pos.w`/w (rastro longo fica mais fraco; `Particles::striteTriangle`). O `sprine` é uma spline cúbica por 4 partículas consecutivas, subdividida no VU1 por diferenças progressivas (com um desvio aleatório semeado por `pos.w`); o port liga as partículas com segmentos retos. O **wake** é um triângulo por partícula no plano das linhas 0 e 2 da `StaticMat` do emissor (`fx+0x2D20`; X/Y do mundo com `info.attribs & 0x4000`), com tamanho e alfa por curvas da idade e escala/ST pelos 12 bits baixos de `pos.w` (detalhes em `docs/passos/VU1CHECK.md`, "Wake como o microprograma 0x3470"). Ainda não lidos: `stripe`, `sweepTrail`, `kite`, `frag`, `dustTrail` (o port os aproxima: faixas ligando partículas consecutivas, o resto como sprites).

## Animação (curvas no `.NGP` e o mixer em tempo de execução)
Fonte: `src/common/AnimCurve.cpp` e `src/common/animation.cpp` (TU inteiro em C++; as equivalentes ainda não passaram por difftest/oracle, então o que depende só delas está marcado (?)).
A leitura reproduz 128 de 130 canais de uma luta (`tools/animcurve.py`). Montar a pose do monstro só com o disco (esqueleto, paleta, texturas): `passos/MODELOS_3D.md`.
Os nomes de struct e de campo vêm de `include/hieri_types.h` (gerado dos stabs); aqui está o que os valores significam.

**Dados (no `.NGP`):** `CHARACTER_INSTANCE` (`_animCharInstance`) -> `character` (+0x20, `_animCharacter`: `numAnims`, `numChannels`, `animations[]`) -> cada `_animation` (`startTime`/`endTime` em quadros da
animação, `numChannels` curvas em `channels[]`). Cada curva escreve um **canal** (`AnimCurveHeader.dataIdx`): um float no vetor `animOutput.val` da instância, que os nós `ANIM_TRANSFORM`
(`rXidx`..`tZidx`, `visidx`) leem para montar as matrizes. `AnimChannelIdMap` (ordenado por `id`, busca binária) leva um id de canal ao índice. `angularChannelBits` marca os canais que são ângulo
(normalizados por `boundEulerAngle` antes de misturar): com até 16 canais os bits ficam nos 2 bytes do próprio campo; acima disso o campo é um offset a partir do `_animCharacter`.

**Curva** (`AnimCurveHeader`, 4 bytes): `curveType` (`kHermite` 0, `kBroken` 1, `kPwl` 2 = linear com chaves igualmente espaçadas, `kStatic` 3, `kSmooth` 4, `kStepped` 5), `dataType`
(`kFloat` 0, `k16bit` 1, `k8bit` 2), `preInfinity`/`postInfinity` (`kConstant` 0, `kLinear` 1, `kCycle` 2, `kCyclePlusOffset` 3, `kOscillate` 4), `dataIdx`. Depois do cabeçalho: `u16 numKeys` e as chaves.
- `kStatic`: um float logo depois do cabeçalho, sem chaves.
- Chaves por tipo (`T` = float, u16 ou u8; `S` = float ou código de 16 bits): `kPwl` `{T value}` (o tempo da chave `i` é `baseTime + i*deltaTime`); `kStepped` `{T time, T value}`;
  `kSmooth` `{T time, T value, S tangent}`; `kBroken` `{T time, T value, S tangent[2]}` (tangentes de saída/entrada do segmento); `kHermite` `{T time, T value, S coef[3]}` (coeficientes cúbico, quadrático e linear do segmento; só existe com float).
- **Quantizadas** (`k16bit`/`k8bit`): as 16 bytes **antes** do cabeçalho são um `AnimNormData` `{baseTime, deltaTime, baseVal, deltaVal}`; tempo = `base + q*delta`, valor idem.
- **Código de tangente** (s16): `s >> 14` = 0 ou -1 -> `s / 16384`; 1 -> `16384 / (32768 - s)`; -2 -> `16384 / (-32768 - s)` (as duas últimas cobrem inclinações íngremes).
- Fora do intervalo das chaves: `kConstant` repete o valor da ponta; `kCycle` repete a curva; `kCyclePlusOffset` repete somando a diferença entre as pontas a cada volta; `kOscillate` vai e volta;
  `kLinear` **devolve 0** no nosso C++ (as instâncias de `animCurveEval` são equivalentes, não idênticas) (?).
- A busca da chave começa no índice guardado em `AnimControlNode::prevKey[canal]` (cache por canal).

**Tempo:** a unidade de tempo do jogo é o **field** (`timerGetFieldCount`, 60 por segundo). Um `_animControlNode` (opcode 0x21, um por animação, em `animCtx[i]` da instância) tem
`tempo = startTime + deltaTime * (fieldAtual - startField)` enquanto `active`; pausado, `startField` guarda os fields já decorridos. `deltaTime` é quadros de animação por field (negativo = de trás para
frente; `animationSetTotalRunFrames(h, n)` faz durar `n` fields). Ao passar do fim: com `loop`, volta ao começo e incrementa `iterations`; sem `loop`, para no fim (e pausa se for a árvore principal).

**Mixer por instância** (`activeTree` +0x24 aponta um `AnimPlayer` de 0x18 bytes):
- `mainTree`: a animação principal, um `_animControlNode` ou um `_animBlendNode` (opcode 0x22) de transição entre a anterior (`blendFrom`) e a nova (`blendTo`). Uma transição durante outra
  encadeia: a antiga fica como `blendFrom` da nova e o tempo dela cai para 40%.
- `activeList`: blends extras em lista duplamente ligada, ordenada por `priority` crescente, aplicados **depois** da principal, por cima do resultado dela. `mainTreePureOut` guarda a saída da principal
  sem eles. Nós de procedimento (opcode 0x2B, `_animprocnode`) chamam `procCallback(nó, instância)`.
- `blendType` (`AnimBlendTypes`): `TRANSITION` 1 (interpola de `blendFrom` para `blendTo`; terminado, o nó é recolhido e devolvido ao pool), `FREEZETRANS` 7 (transição que põe o destino na
  porcentagem atual dele e, no fim, devolve o `deltaTime` guardado e o reativa) (?), `STATIC` 2 (mistura o destino sobre o que já está no canal com peso `weight`), `OVERRIDE` 3 (**escreve `destino * peso`** no canal, sem misturar) (?),
  `ADDITIVE` 4 (soma `peso * curva`), `PROCEEDURAL` 6 (função vazia no retail), `SUBTRACTIVE` 5 e `NONE` 0 zeram o peso.
- Peso no tempo: `r = (ms desde blendStartField) / blendTime` (`blendTime` em **milissegundos**, field = 16,667 ms), passado pela curva global `s_blendCurve` (0 linear, 1 cosseno, 2 `r²`, 3 `r(2-r)`).
- Os blends vêm de um pool global de **128** `_animBlendNode` com pilha de índices livres; os buffers temporários de cada mistura saem da pilha da scratchpad (`0x70000000`, 16 KB).
- Busca de animação: `animationGetHandle(id1, id2, animId)` percorre os gerenciadores de animação de cada `.NGP` carregado (`ANIMATION_MGR`, `charInstance[]`), acha a instância pelos ids do
  `HierHead` e a animação cujo `head.id1 == animId`. O handle (`ci`, `ctrl`, `proc`, `animIdx`) é o que o código de jogo guarda (os monstros têm uma tabela deles, ver `Monster::m_anims` em `include/game/game.h`).

## Destrutíveis (`INTERACTIVE_STATE` + `DESTRUCTIBLE`)
Fonte: `game/Destructible` (`Destructibles::AddDestruct`, `Destructible::takeHit`, `GenericTakeHit`, `ChangeState`, `SwitchState`; o TU ainda está em asm, lido à mão) e a travessia `hierTraverseAsm`
(asm manuscrito, em `asm/common/texm.s`). A estrutura dos nós vale para as 19 fases de `LVL/`.

**No disco:** todo `DESTRUCTIBLE` (op 0x1F) é filho único de um `INTERACTIVE_STATE` (op 0x27): `_interactivestate.child` (+0xC) aponta para ele (1566 de 1566 nas fases). O `DESTRUCTIBLE` tem
`hitPointClass` (+4), `hitPoints` (+6), `hpThreshold` (+8), `numKids` (+0xA) e `numKids` estados de 12 bytes a partir de +0xC: `{child, actions, damageThreshold (u16), behaviorType (u16)}`.
O estado 0 é o intacto. `behaviorType` vale 0 em todos os 3426 estados das fases (não usado). `whichChild` do `INTERACTIVE_STATE` vale 0 no disco.

**O que desenhar (o que o nativo lê da RAM):** a travessia guarda o `INTERACTIVE_STATE` ao passar por ele e, no `DESTRUCTIBLE` logo abaixo, desce só para
`child[whichChild].child`, isto é, `u32(destr + 0xC + 12 * u32(istate + 4))`. O estado atual é **`_interactivestate.whichChild` (+4) do pai**, escrito pelo jogo em tempo de execução.

**Vida** (`Destructibles::AddDestruct`, um objeto `Destructible` de 0xA0 bytes por destrutível, até 1500): pelo `hitPointClass` do nó:
| `hitPointClass` | vida (`+0x14` e máxima em `+0x18`) | dano mínimo (`+0x1C`) |
|---|---|---|
| 0 | `hitPoints` do nó | `hpThreshold` do nó |
| 1 | 10 | 0 (e limpa o bit 3 das flags em `+4`) |
| 2, 3, 4, 5 | 50, 100, 150, 250 | 5 |
| 6 ou mais | 3000 | 5 |
Nas fases aparecem as classes 0 (349), 1 (1041), 2 (109), 3 (28) e 4 (39). O `Destructible` guarda ainda o nó (`+0x24`), o `INTERACTIVE_STATE` (`+0x28`), o índice em `Interactives` (`+0x2C`,
também gravado nos bits 7..17 da primeira palavra do `INTERACTIVE_STATE`), `hitPointClass` (`+0x30`), a posição (`+0x40`), a matriz (`+0x50`) e o estado pedido (`+0x90`).

**Regra de troca** (`takeHit` -> `GenericTakeHit` -> `ChangeState`):
1. Um golpe só tira vida se o dano for **maior** que o dano mínimo (ou se o `HitEvent::s_takeHitInfo` tiver `source` 5 e `subtype` 15 ou 16). Exceções por id: 0x1A90..0x1B57 não perdem vida; na fase 6 (`TheGame::m_levelId`, Three Mile), os ids
   0x2712 e 0x2715..0x2718 (respiradouro e chaminés) só aceitam dano de monstro do jogador (?); na fase 7 (San Francisco), o id 0x2762 só aceita dano exatamente 100.
2. `q = (int)vida * 65535 / (int)vidaMáxima` (fração da vida em 1/65535). O novo estado é o **primeiro** `i` com `damageThreshold[i] < q`, mas o laço para em `numKids - 1` e em
   `estadoAtual + 1`: **um golpe avança no máximo um estado**. Nas fases os limiares típicos são `62258` (95%), `49151` (75%), `32767` (50%) e `0`; `65534` faz o estado mudar no primeiro dano.
3. Se `i` passou do atual: grava `+0x90 = i`, executa as `actions` do estado `i`, derruba os monstros que estavam em cima (`shedMonsters`), dá 50 tokens ("Destructibles") ao monstro que bateu e
   agenda `SwitchState` no `TaskManager` (atraso 2), que copia `+0x90` para `whichChild`. Ou seja, **o desenho muda alguns quadros depois do golpe** (?).
4. Voltar para um estado menor grava `whichChild` direto: é o que `rebuildCanyon2Pillars`/`rebuildCapitolPillars` fazem com `ChangeState(0, 1, 0)` (reconstroem os pilares).
5. Com o terceiro argumento de `ChangeState` ligado, o jogo também põe em 1 o `whichChild` de um `SWITCH` no começo do novo estado (o próprio `child` ou o primeiro filho de um `GROUP`), e
   a vida vai para `damageThreshold[i] * vidaMáxima / 65535 - 0,001`.

## Save do memory card (`BASCUS-97197`, `savegame.dat`)
Pasta `BASCUS-97197` no cartão (título "War of the Monsters"). O stub `sceMc*` do runtime grava em `mc0/` do host (porta 2 em
`mc1/`). Quem monta os blocos é o `game/MonsterMc`; o `game/McFile` (`McLoadFile`/`McSaveFile`/`McSetupGameDir`/`McScrambleIcon`) e
o `game/McPage` (menus) são plataforma. `python tools/mcsave.py <pasta>` mostra os dois cartões como JSON por nome (só lê). No PC o
jogo usa o `save.json` (`docs/passos/NATIVO.md`, "Save em JSON").

| Arquivo | Bytes | Conteúdo | Monta / aplica |
|---|---|---|---|
| `BASCUS-97197` | 0x380 | opções, battle tokens e destraves (`_SettingsData`) | `MonsterMc::getSettingsData` / `setSettingsData` |
| `savegame.dat` | 0x280 | 10 slots da aventura (`_SaveGameData`) | `MonsterMc::getSaveData(slot)` / `setSaveData(slot)` |
| `icon.sys`, `ICON.ICO` | 964, 61120 | ícone do PS2 | `McSetupGameDir`; o `McScrambleIcon` zera o `icon.sys` durante a gravação das opções e o refaz igual depois |

**Sem checksum.** As duas leituras (`McLoadSettings::update`, `McSelectReel::refreshSaveData`) só conferem a marca `0x12345678` no fim
do bloco; marca errada vira erro do cartão. Nenhum ponteiro: tudo é `int` de 32 bits, little-endian.

**`BASCUS-97197` (opções):**

| Off | Origem |
|---|---|
| 0x00 | `TheGame+0x1203CC` (`m_matchMode`; o `Shell` acende `m_difficultySw[valor]`: é a dificuldade) |
| 0x04, 0x08 | `Shell::m_sfxVolume`, `m_musicVolume` (0..0x400) |
| 0x0C | `Shell+0x2A40`: o construtor põe 50 e ninguém mais lê (opção morta; provável slider cortado do menu de opções) |
| 0x10 | `Shell::m_mono` (ao carregar vira 0/1) |
| 0x14, 0x18 | `Shell::m_ctlrCfg[2]` |
| 0x1C, 0x20 | `Shell::m_vibration[2]` |
| 0x24 | battle tokens (`Shell+0x2B24`) |
| 0x28..0xE7 | `monsterSelectMode[i].state` dos 48 `SelectSlot` (só o `state`; `taken`, preço e nomes ficam no ELF) |
| 0xE8..0xF3 | `TheGame+0x1204B0..B8`, 3 ints: recorde do endurance por dificuldade (gravado, o jogo nunca o mostra) |
| 0xF4..0x34B | **não é escrito**: o buffer é da pilha e esse trecho grava lixo |
| 0x34C | marca `0x12345678` |

Os 48 flags são 12 grupos de 4. Grupos 0 a 9 são os monstros na ordem do `aiMonsters` de `Shell::RandomlySelectAI` (Raptros, Togera,
Preytor, Congar, Robo-47, Agamo, Ultra V, Magmo, Kineticlops, Zorgulon), e a entrada `j` do grupo é o traje `j`. Os grupos 10 e 11 são
as fases do `levelUnlocked`: entrada 40 Volcano, 41 Mini Baytown, 42 Capitol, 43 UFO, 44 Dodge Ball, 45 Big Shot, 46 Crush; a 47 é
uma "Level 4" de 5000 tokens que nenhum código lê. Os nomes e preços estão na própria tabela (`SelectSlot::name`/`cost` no ELF) e
confirmam a ordem. O `McCheckForSweetTooth` do boot procura o `BASCUS-97101/globalFile` (save do Twisted Metal: Black) no cartão e,
se achar, liga a entrada 23 ("Agamo Costume 4", o Mecha Sweet Tooth; `ROCK3`) e toca `ShellSound::playSweetUnlockedSound`.

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
slot (`McSaveAdventure`). Ou seja: tokens e destraves vão para o cartão em qualquer gravação. A compra da tela de destrave só é
efetivada dentro do `McSaveSettings::update` (`g_monsterToUnlock`/`g_skinToUnlock`: liga o `state` e desconta o preço; a tela põe -1
nos dois logo depois). Fora dessas sequências, os tokens só mudam no `Shell::EvaluateOnePlayerStoryStatus` (fim de fase da
história), no endurance e no construtor.

**`MEMCRD/ICON.ICO`** (o ícone, também no disco): uma fita de filme, 1158 vértices, uma forma, textura 128x128 comprimida; o formato
está no cabeçalho do arquivo (`native/wotm_saveicon.hpp`).
