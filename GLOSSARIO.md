# Glossário do WotM

Referência rápida para falar dos mesmos nomes no projeto. Fontes: `include/game/MonsterNames.h` (`MonsterNewNames`), `docs/FORMATOS.md` (`MonsterLongNames`) e o conteúdo de `disc/MON/` (não versionado). O que veio do dono do projeto está marcado como tal.

## Monstros

O **índice** é o do jogo (`MonsterLongNames`/`MonsterNewNames`). O **nome interno** é o prefixo dos arquivos `disc/MON/<PREFIXO><traje>.NGP/.TEX/.RTX/.PTR`; o número no fim é o **traje** (0 a 3), não o monstro (`LIZARD1` é o Togera com outro traje).

| # | Nome do jogo | Nome interno | Arquivos no disco | Observação |
|---|---|---|---|---|
| 1 | Congar | `MONKEY` | MONKEY0–3 | |
| 2 | Robo-47 | `ROBOT` | ROBOT0–3 | |
| 3 | Togera | `LIZARD` | LIZARD0–3 | |
| 4 | Ultra V | `SHOGUN` | SHOGUN0–3 | |
| 5 | Preytor | `MANTIS` | MANTIS0–3 | |
| 6 | Spider | `SPIDER` | nenhum | cortado; restos abaixo |
| 7 | Kineticlops | `ENERGY` | ENERGY0–3 | |
| 8 | Magmo | `LAVA` | LAVA0–3 | |
| 9 | Raptros | `DRAGON` | DRAGON0–3 | |
| 10 | Agamo | `ROCK` | ROCK0–3 | |
| 11 | Zorgulon | `ALIEN` | ALIEN0–3 | |
| 12 | Ogre | `OGRE` | nenhum | cortado; restos abaixo |
| 13 | Goliath Prime | `ASSBOSS` | ASSBOSS0–1 | nome informado pelo dono; o código o chama de `assboss` |
| 14 | ? | `JELLY` | JELLY0 | boss final, 1ª forma: a água-viva (`FinalBoss::updateJelly`) |
| 15 | ? | `FINAL` | FINAL0 | boss final, 2ª forma: o mecha (`FinalBoss::updateMecha`) |
| 16 | — | `temp16` | — | só o nome; o carregador recusa índice >= 16 |

São 17 nomes (0 a 16): `MonsterNewNames` tem 0xCC bytes (17 × 12) e `MonsterLongNames` 0x88 (17 × 8). O `default` que aparece logo antes no binário é `GameLevelNames[0]` (fase 0), não um monstro.

Nomes em minúsculas no código (`"robo-47"`, `"ultra-v"`); a capitalização da coluna "Nome do jogo" é nossa.

### Monstros cortados e slots especiais (levantamento de 2026-10-14)
Tudo lido do ELF (strings, stabs, símbolos) e do disco; o que é dedução está marcado.

- **Nenhum dos dois tem arquivos.** Não há `SPIDER*`/`OGRE*` em `MON/`, nem `.AIC`, nem banco de som (`loadMonsterBank` só conhece 10 bancos, `Ape_s.bnk`..`Alien_s.bnk`; os slots 5 e 11 de `gSoundBanks` ficam vazios), nem VPK de música de introdução.
- **Fora da seleção de monstros.** `monsterNametomonsterId` (0x2A1A40, ordem das colunas) é `dragon, lizard, mantis, monkey, robot, rock, shogun, lava, energy, alien, spider, ogre, assboss, jelly, final`; a tela só anda pelas 10 primeiras colunas (`initSelectSwitches`: `[first, last)` com `last` <= 10). Spider e ogre estão logo depois do Zorgulon, prontos para virar coluna.
- **Excluídos de propósito.** O modo demo (`EvaluateGameStatus`, `m_mode` 0) sorteia `mathfRand(1, 12)` e troca 0xC0 (spider) e 0x180 (ogre) pelo Congar; o modo 6 carrega 1..5 e 7..11 (pula o spider; o ogre fica fora do intervalo); `aiMonsters[10]` em `Shell.cpp` também não tem os dois.
- **Restos de código do Ogre:**
  - `MonsterSound::updateMonsterSound` ainda despacha 0x180 para `updateOgreSound`, que é vazia.
  - `Monster::updatePowerUpGlow` tem um `case 0x180` próprio (pulso com mínimo 0x20 e base 0x80, diferente dos outros); o spider (0xC0) divide o caso do Kineticlops (0xE0).
  - A classe `OgreMace` (TU `game/OgreMace`: bola com corrente, ep de nível 0x11FC em `dbProcInteractive`) é usada pela `StateGrappleHook` em `Monster+0x10600`, que `initDynamics` liga ao **Ultra V** (0x80). Dedução: a maça com corrente foi escrita para o Ogre e reaproveitada como o gancho do Ultra V.
- **Restos no enum de efeitos (`FxId`, stabs):** `SpiderHit` (91), `SpiderSpecialHit` (103), `SpiderFlashBang` (114), `OgreSpecialHit` (99), e também `WomanSpecialHit` (98), `ErnieHit` (87) e `ErnieSpecialHit` (100), de monstros que nem aparecem na lista de nomes (outros cortes ou nomes antigos; não identificados). Nenhuma chamada a `particleCreateFx` com esses ids foi achada; `JellyfishSplash` (86) é usado em `updateJelly`.
- **Spider no som:** `MonsterSound::updateSpiderMagnoSpecial`/`terminateSpiderMagnoSpecial` existem mas ninguém as chama, e a primeira nunca dispara o som (só ajusta um handle que nunca é criado). Indica que o especial do spider era algo chamado "magno".
- **Jelly e Final (boss da `capitol`).** Na `capitol`, `levelMonsters` dá como IAs `jelly` e `final`. `TheGame::SetAIMonster` conta os dois como IA, desliga (`disableMonster`) e guarda em `finalBoss+0x74` (jelly) e `+0x78` (final). `FinalBoss::update` chama `updateJelly` (água-viva na água: `StateUltraTazer`, `electricArc`, `playJellyTazerOneShot`, `playWaterSplashSound`, `JellyfishSplash`) ou `updateMecha` (mecha com laser e escudo: `FinalBossSound::updateMechLaser`, `playShieldPowerUp/DownSound`, `playMechaPowerDownSound`, `mechaShadowRadius`). A fase fica em `finalBoss+0x1C`: 0 = luta inicial com pilares (`rebuildCapitolPillars`), 1 = jelly, 2 = final; `Shell::InitPlayers` restaura a vida do boss da fase ao reiniciar. Som: `Final_s.bnk`, `JELLYD.VPK`, `JELLYBT.VPK`, `MECHADT.VPK`. As IAs têm `CAPITOL/JELLY0–2.AIC` e `FINAL0–2.AIC`.
- **Boss planta (`3-Mile`)** não é monstro da lista: são as classes `PlantBoss`/`PlantBossHead`/`PlantBossRoot`/`PlantBossBase` (`plantBoss`, 0x10B0 bytes), com `Plant_s.bnk` e `PLANTD.VPK`; as IAs da fase são 4 Kineticlops (`3-MILE/ENERGY0–2.AIC`).
- **temp16:** só existe nas duas tabelas de nomes. `Shell::LoadResTexture` recusa `sel >> 5 >= 0x10` com "Unknown monster (%d) for player %d.  Nothing will be loaded!".

### Pendências
- Nomes que o jogo mostra para o jelly e o final.
- Quem são `Woman` e `Ernie` do `FxId`.
- Dá para pôr o spider/ogre na seleção (ampliar o `last` de `initSelectSwitches`), mas sem `MON/SPIDER0.*`/`OGRE0.*` o `fileReadf` não acha arquivo; precisaria de um modelo substituto.

## Fases

O **índice** é o do jogo (`GameLevelNames`, 0x6EF748: 28 nomes de 10 bytes) e é o que fica em `Shell::m_levelNum`. O **nome interno** é o dos arquivos `disc/LVL/<NOME>.NGP/.TEX/.RTX/.PTR`. As fases jogáveis também têm `MOV/<NOME>.PSS`, `PTS/<NOME>.AIP/.FOP/.PED/.PUP/.SPT` e `RST/<NOME>.MRS`. `disc/LEVELS.TXT` lista as 15 com `.ngp`. O **nome do jogo** é o que aparece para o jogador, tirado de `GameLevelLongNames` (0x6ED120, mesmo índice; confirmado pelo dono).

| # | Nome do jogo | Nome interno | Arquivos no disco | Observação |
|---|---|---|---|---|
| 0 | Default | `default` | — | fase da linha de comando (`whichLevel`, `UseCommandLineLevel`) |
| 1 | Midtown Park | `central` | `CENTRAL` | a fase a que o roteiro `-Auto` chega |
| 2 | Gamblers Gulch | `vegas` | `VEGAS` | |
| 3 | Rosdale Canyon | `canyon2` | `CANYON2` | no modo história, luta contra o Goliath Prime (`assBoss`, `canyon2LevelProgression` em `Shell::InitPlayers`) |
| 4 | Metro City | `metro` | `METRO` | |
| 5 | Century Airfield | `airport` | `AIRPORT` | |
| 6 | Atomic Island | `3-Mile` | `3-MILE` | no modo história, luta contra o boss planta (`plantBoss`, `craterSwitched`, `threeMileInitAi`) |
| 7 | Baytown | `sanfran` | `SANFRAN` | |
| 8 | Club Caldera | `island` | `ISLAND` | |
| 9 | Tsunopolis | `tokyo` | `TOKYO` | |
| 10 | UFO | `ufo` | `UFO` | |
| 11 | Capitol | `capitol` | `CAPITOL` | no modo história, luta contra o boss final (`finalBoss`, monstros `JELLY`/`FINAL`, 0x1C0/0x1E0) |
| 12 | Training | `training` | `TRAINING` | treino |
| 13 | Lake Powell | `canyon1` | `CANYON1` | |
| 14 | Mini Baytown | `minisf` | `MINISF` | |
| 15 | Volcano | `volcano` | `VOLCANO` | |
| 16 | Tokyo Deathmatch | `tokyodm` | `TOKYODM` | |
| 17–24 | Dylan, David, Lars, Jenn, Yeon, Jacob, Steve, Pierre | `dylan`, `david`, `lars`, `jenn`, `yeon`, `jacob`, `steve`, `pierre` | nenhum | nomes de pessoas: provavelmente fases de teste da equipe, sem arquivos |
| 25 | Dodge Ball | `dball` | `DBALL` | minijogo dodgeball (`m_mode` 7) |
| 26 | Big Shot | `bigshot` | `BIGSHOT` | minijogo bigshot (`m_mode` 8) |
| 27 | Crush | `crush` | `CRUSH` | minijogo crush (`m_mode` 9; o texto do jogo também diz "Crush-o-Rama") |

O caminho de `Shell::LoadResTexture` mostra que índices acima de 0x1C caem em `host0:monster.rtx`.

## Outros termos
(a preencher)
