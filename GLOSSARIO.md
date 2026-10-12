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
- Existe um **boss planta** no meio do jogo que não sabemos qual da lista é; nome e arquivo ainda não identificados. Candidatos a investigar: `JELLY` e `FINAL` (nomes desconhecidos), ou um modelo carregado por outro caminho.
- Descobrir quem são `JELLY` e `FINAL` e como as 3 fases do boss final são carregadas.

## Fases
(a preencher)

## Outros termos
(a preencher)
