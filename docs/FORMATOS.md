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
energy, lava, dragon, rock, alien, ogre, assboss, jelly, final, temp16, default`. A seleção de cada vaga é `(monstro << 5) | variante`
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

## Filmes `.PSS` (2026-10-11)
`disc/MOV/*.PSS` (52 arquivos). MPEG-2 program stream em pacotes de 2048 bytes (cabeçalho de pacote `00 00 01 BA` de MPEG-2, um cabeçalho de sistema `BB`), conferido com `tools/pss.py` e o `ffprobe`:
- **Vídeo:** fluxo `E0`, MPEG-2 MP@ML a 29,97 quadros/s. Tela cheia em 640x368 com proporção 16:9: `INTRO_M`, `INTRO_S`, `INT_MAIN`, `INT_ACT1..3`, `INT_TRAN`, `<monstro>_G` (finais), `<monstro>_P`, `CREDITS`. Os outros são pequenos e sem áudio: 256x128 (`SHELDEMO`, o filme do menu, com 173 s; nomes de monstro e de fase como `ALIEN`, `CENTRAL`, `TOKYO`: prévias, provavelmente), 254x128 (`3-MILE`, `AIRPORT`, `ISLAND`) e 256x160 (`MANTIS`, `ROBOT`).
- **Com áudio** (nos primeiros 4 MB): `INTRO_M`, `INT_MAIN`, `CREDITS` e os `_G`/`_P`, menos `ALIEN_G` e `DRAGON_G`. `INT_ACT1..3` não têm (o `playMovie` os chama com `comAudio` = 0; ele também cita um `int_act4.pss` que não existe no disco); `INTRO_S` e `INT_TRAN` também não, e não aparecem no `playMovie`.
- **Áudio:** fluxo privado `BD`, subfluxo `FF`, 4 bytes de cabeçalho por PES. Começa com `"SShd"`, tamanho (0x18), tipo (1 = PCM 16 bits little-endian), taxa (48000), canais (2), intercalação (0x200) e dois `-1`; depois `"SSbd"`, tamanho dos dados e os dados: blocos de 512 bytes alternando canal esquerdo e direito (256 amostras cada). O FFmpeg não reconhece esse áudio (só vê o vídeo).
- **No jogo:** o retail toca por `sceMpegDemuxPssRing` + IPU; o nativo (`native/wotm_fmv.hpp`) lê o arquivo direto. O filme do menu vira a textura de um objeto do menu 3D: `moviesSetScreenObject` troca o texId por 999 (o último de `texInfo[1000]`, reservado) e o TEX0 por CT32 em 0x3BA0, largura 320 (`VramAddrForMovies` 0xEE800 palavras / 64, `VramWidthForMovies` 0x140), com `TW`/`TH` = 8/7.

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

## Malhas (`HierObject::polyPkt`) — decodificado em 2026-10-10
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
- Nós percorridos pelo port: `GROUP` (filhos em +0x20, contagem u16 em +8), `LOD` (lista de `{fade, switchOutDis, child, pad}` em +0x20; o 0 é o mais detalhado), `SWITCH` (filho `whichChild` em +0xC), `CONTROL` (`child1` em +4), `ACTION_DATA` (filho em +4), `CHAR_INSTANCE` (filhos em +0x2C, contagem em +0x28), `INTERACTIVE` (filho em +0xC), `DESTRUCTIBLE` (estados em +0xC, 12 bytes cada; o 0 é o intacto).

## Texturas e a VRAM do GS (em andamento)
O `TEX0` de cada objeto (`gsTexCtx[0].tex0`) usa o formato do GS (`TBP0` bits 0..13, `TBW` 14..19, `PSM` 20..25, `TW` 26..29, `TH` 30..33, `CBP` 37..50, `CPSM` 51..54, `CSM` 55, `CSA` 56..60). Os pixels chegam à VRAM por uploads GIF `IMAGE` do `.TEX` (e o `.RTX`).
`native/wotm_gs.hpp` decodifica `CT32/CT24/CT16/T8/T4` a partir de um dump da VRAM (4 MB); a paleta de 256 entradas (CSM1) está em memória linear com as entradas 8..15 e 16..23 de cada grupo de 32 trocadas.
O runtime grava `ram_<quadro>.bin` e `vram_<quadro>.bin` com `PS2X_DUMP_RAM="quadro,..."` (`PS2X_NO_GS=1 PS2X_GS_UPLOADS=1` processa só os uploads, sem rasterizar).

## Partículas e efeitos (`common/particle`, `common/partdraw`) — lido em 2026-10-11
Leitores: `native/wotm_fx.hpp` (C++, desenha) e `tools/fx.py` (Python: sistemas ativos de um dump de RAM e `--presets` com o catálogo do ELF).
O layout de `_ParticleType` (12000 bytes), `EmitterType`, `FieldInfoType`, `ParticleInfoType` e `VisualInfoType` está em `include/hieri_types.h` (stabs do `.mdebug`).
- **Pool**: `fx[100]` em `0x5BA280` (stride `0x2EE0`). Lista ativa: `fxActiveTop` (`0x6F88EC`) é o primeiro índice e `fxList[i]` (`0x6DF200`, 12 bytes) guarda o próximo em `+8`; `-1` termina. `fxInactiveTop` (`0x6F88F0`) é a lista livre.
- **Presets**: `particleInitFx(tipo, ...)` copia as linhas `tipo` de `EmitterInfo` (`0x5B2020`, 80 bytes), `FieldInfo` (`0x5AEFF0`, 80), `ParticleInfo` (`0x5B4F70`, 96) e `VisualInfo` (`0x5ADD00`, 32) para o sistema; são 151 presets, dados inicializados do ELF. O índice do preset parece ficar em `emit.fxId` (`+0x2DDC`): `particleMove` o compara com 0x91 e 0x93 para efeitos de borda; ainda não confirmado em dump. Os nomes estão no enum `FxId` do próprio jogo (`include/hieri_types.h`): 0 `Dust`, 2 `OpaqueDust`, 5 `DustTrail`, 9..12 explosões (`WhiteExplosionFx`, `SmallExplosionFx`, `ExplosionFx`, `SmokeExplosionFx`), 15..18 vapor e fumaças, 21..26 faíscas, 28..32 clarões (`CounterFlash` .. `BlueWhiteFlash`), 43..48 fogo (`Flames`, `AttachedFlames`...), 105 `FootStomp`, 146 `WaterWake`. O `vis.texInfoIndex` é um `FxTextureId` (`Smoke1`..`Smoke5`, `TexExplosion`, `Fireball`, `TexFlames`, `Spark`, `DustTexAlpha`, `CompositedDust`...).
- **Por partícula** (ponteiros dentro do próprio pacote `pkt`, montados no `*PacketInit`): `pos[i]` (`+0x2EB0`, `FVECTOR`; **`w` é o tamanho**, tratado como raio por `particleFOVcalcMaxRadius`, que soma `maxSize + expansionRate * lifetime`), `vel[i]` (`+0x2EB4`; `vel.w` é a taxa de expansão somada a `pos.w` por campo) e `age[i]` (`+0x2EB8`, `int16`, em campos). `numParticles` em `+0x2ED0`, `maxParticles` (256 nos sprites) em `+0x2ED4`. Os bits baixos de `pos.w` são preservados em `particleMove` (máscara 3, ou 0xF com `info.attribs & 0x2000`) e escolhem um dos 4 ou 16 conjuntos de cantos de UV que o `spritePacketInit` sobe.
- **Tipo de desenho** = `vis.attribs` (`+0x2E90`), pela tabela de saltos do `particlePrepDraw`: 1 `line` (sobe `pos` e `vel`), 2 `sweepTrail`, 3 `sprite`, 4 `texSprite`, 5 `stripe`, 6 `strite`, 8 `sprine`, 9 `kite`, 13 `frag`, 15 `dustTrail`, 16 `wake`; 7, 10..12 e 14 não montam pacote. Arcos elétricos (`monArcParticle`), correntes e lens flares têm caminho próprio.
- **Quem desenha** (`particleDraw`): `info.attribs & 0x400` (pacote montado neste quadro), `numParticles >= 1` (`sprine`: `>= 2`), tipo diferente de 11 e a esfera `FOVsphere`/`FOVextent` (`+0x2C80`/`+0x2C90`) dentro da vista. A ordem é do mais longe ao mais perto pela profundidade (eixo Y do olho) de `emit.ParticleToEmitVec` (`+0x2CA0`) mais `vis.priority`; sistemas com `alphaContext == 0` (opacos) recebem a chave 50000 + prioridade e vão primeiro.
- **Cor** (`loadColors`, igual no microcódigo dos sprites): tabela RGBA em `0x256C10` (`0x80` = 1,0, alfa incluso), `vis.numColors` cores a partir de `vis.colorIndex`. `t = age * info.colorsPerFrame` (4.12); a cor é a interpolação entre `cor[t >> 12]` e `cor[(t >> 12) + 1]` por `(t & 0xFFF) / 4096`. Zero cores = `0x80808080`; uma cor = constante. Quase todas as tabelas terminam com alfa caindo, e é assim que a partícula some.
- **Mistura**: `vis.alphaContext` é o registrador `ALPHA` do GS escrito no contexto do pacote: `0x44` = `(Cs - Cd) * As + Cd` (normal), `0x48` = `Cs * As + Cd` (aditivo: fogo, faíscas, clarões).
- **Textura**: `TextureInfo[vis.texInfoIndex]` (`0x5B9E80`, 8 bytes: `numTextures`, `texIndex`, `du`, `dv`; preenchida em tempo de execução por `particleParseTexture`). `texId[texIndex]` (`0x6DFAB0`, `u16`) é o índice no `texInfo` global (pixels do pacote de upload, como nos objetos) e `TEX0[texIndex]` (`0x6DF6B0`, `u64`) é o registrador `TEX0` (psm, CBP). O `texSprite` escolhe um quadro por partícula: `info.attribs & 1` aleatório, `& 2` `uvI[i] >> 2`, senão `(age * info.mapsPerFrame) >> 12`, travado no último com `& 4` ou em módulo `vis.texMask`.
- **Microcódigo de partículas do VU1**: não é o microcódigo dos objetos (`0x257C10`); a `displayList` das partículas chama um pacote próprio em `0x253060` (954 qwords: 1884 instruções por `MPG` e, no fim, o contexto padrão em `0x256B90`, 8 qwords: matriz mundo→tela, constantes e `vf27` = focos 320/160). Cada `*PacketBuild` termina com `MSCAL` no seu programa (`D_58` linha, `D_2C8` sprite, `D_800` dustTrail, `D_A88` strite, `D_F18` texSprite, `D_1180` frag, `D_13A0` kite, `D_1708` stripe, `D_19F8` arco elétrico, `D_1D00` corrente, `D_2770` sprine, `D_2B20` lens flare, `D_3318` sweepTrail, `D_3470` wake; endereços em bytes, `MSCAL` usa /8). Lido: no **sprite**, o meio-lado na tela é `pos.w * foco / profundidade`, limitado a `foco / 2` (então `pos.w` é o meio-lado em unidades de mundo e não passa de metade da profundidade); os bits baixos de `pos.w` indexam os 4 conjuntos de UV do `spritePacketInit` (3 normal, 1 espelha U, 2 espelha V, 0 os dois; com `0x2000`, 16 conjuntos); a cor vem da tabela pela idade. Na **linha**, a cauda é `pos - vel`, projetada com a matriz do quadro anterior (o contexto leva as duas: borrão de movimento da câmera). No **strite**, cabeça `pos` e cauda `pos - vel` projetadas e esticadas 2x para cada lado (de `pos + 2*vel` a `pos - 3*vel`), largura por `pos.w`. O `sprine` é uma spline cúbica por 4 partículas consecutivas, subdividida no VU1 por diferenças progressivas (com um desvio aleatório semeado por `pos.w`); o port liga as partículas com segmentos retos. Ainda não lidos: `stripe`, `sweepTrail`, `kite`, `wake`, `frag`, `dustTrail` (o port os aproxima: faixas ligando partículas consecutivas, o resto como sprites).
