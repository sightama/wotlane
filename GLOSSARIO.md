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
| 6 | Spider | `SPIDER` | nenhum | sem arquivos em `MON/` |
| 7 | Kineticlops | `ENERGY` | ENERGY0–3 | |
| 8 | Magmo | `LAVA` | LAVA0–3 | |
| 9 | Raptros | `DRAGON` | DRAGON0–3 | |
| 10 | Agamo | `ROCK` | ROCK0–3 | |
| 11 | Zorgulon | `ALIEN` | ALIEN0–3 | |
| 12 | Ogre | `OGRE` | nenhum | placeholder não usado (informado pelo dono) |
| 13 | Goliath Prime | `ASSBOSS` | ASSBOSS0–1 | nome informado pelo dono; o código o chama de `assboss` |
| 14 | ? | `JELLY` | JELLY0 | nome do jogo desconhecido |
| 15 | ? | `FINAL` | FINAL0 | nome desconhecido; o boss final tem 3 fases (informado pelo dono) |
| 16 | — | `temp16` | — | slot temporário |
| 17 | — | `default` | — | padrão |

Nomes em minúsculas no código (`"robo-47"`, `"ultra-v"`); a capitalização da coluna "Nome do jogo" é nossa.

### Pendências
- Existe um **boss planta** no meio do jogo que não sabemos qual da lista é; nome e arquivo ainda não identificados. Ele aparece em `3-Mile` no modo história (`plantBoss`, 3 cabeças de 0x250 bytes; veja Fases). Candidatos a investigar: `JELLY` e `FINAL` (nomes desconhecidos), ou um modelo carregado por outro caminho.
- Descobrir quem são `JELLY` e `FINAL` e como as 3 fases do boss final são carregadas.

## Fases

O **índice** é o do jogo (`GameLevelNames`, 0x6EF748: 28 nomes de 10 bytes) e é o que fica em `Shell::m_levelNum`. O **nome interno** é o dos arquivos `disc/LVL/<NOME>.NGP/.TEX/.RTX/.PTR`. As fases jogáveis também têm `MOV/<NOME>.PSS`, `PTS/<NOME>.AIP/.FOP/.PED/.PUP/.SPT` e `RST/<NOME>.MRS`. `disc/LEVELS.TXT` lista as 15 com `.ngp`. Os nomes que aparecem no jogo ainda não foram levantados.

| # | Nome do jogo | Nome interno | Arquivos no disco | Observação |
|---|---|---|---|---|
| 0 | — | `default` | — | fase da linha de comando (`whichLevel`, `UseCommandLineLevel`) |
| 1 | ? | `central` | `CENTRAL` | a fase a que o roteiro `-Auto` chega |
| 2 | ? | `vegas` | `VEGAS` | |
| 3 | ? | `canyon2` | `CANYON2` | no modo história, luta contra o Goliath Prime (`assBoss`, `canyon2LevelProgression` em `Shell::InitPlayers`) |
| 4 | ? | `metro` | `METRO` | |
| 5 | ? | `airport` | `AIRPORT` | |
| 6 | ? | `3-Mile` | `3-MILE` | no modo história, luta contra o boss planta (`plantBoss`, `craterSwitched`, `threeMileInitAi`) |
| 7 | ? | `sanfran` | `SANFRAN` | |
| 8 | ? | `island` | `ISLAND` | |
| 9 | ? | `tokyo` | `TOKYO` | |
| 10 | ? | `ufo` | `UFO` | |
| 11 | ? | `capitol` | `CAPITOL` | no modo história, luta contra o boss final (`finalBoss`, monstros `FINAL`/0x1C0/0x1E0) |
| 12 | ? | `training` | `TRAINING` | treino |
| 13 | ? | `canyon1` | `CANYON1` | |
| 14 | ? | `minisf` | `MINISF` | |
| 15 | ? | `volcano` | `VOLCANO` | |
| 16 | ? | `tokyodm` | `TOKYODM` | |
| 17–24 | — | `dylan`, `david`, `lars`, `jenn`, `yeon`, `jacob`, `steve`, `pierre` | nenhum | nomes de pessoas: provavelmente fases de teste da equipe, sem arquivos |
| 25 | — | `dball` | `DBALL` | minijogo dodgeball (`m_mode` 7) |
| 26 | — | `bigshot` | `BIGSHOT` | minijogo bigshot (`m_mode` 8) |
| 27 | — | `crush` | `CRUSH` | minijogo crush (`m_mode` 9) |

O caminho de `Shell::LoadResTexture` mostra que índices acima de 0x1C caem em `host0:monster.rtx`.

## Outros termos
(a preencher)
