# TODO

O que falta fazer, e só isso. Relatos de bug passam na frente. Item terminado sai deste arquivo: a história vai para a mensagem do commit e o que passou a existir vai para o doc de referência (no presente). Regras dos docs: `CLAUDE.md`, "Documentação".

## Ordem
| # | Item | Depende de | Custo |
|---|---|---|---|
| 8 | Mods que adicionam monstros (as skins já existem) | — | médio/alto |
| 10 | Chefes jogáveis | 8 | alto |
| 11 | Multiplayer de 4: custo das vistas e testes com controles físicos | — | alto |
| 13 | Campanha (adventure) com até 4 jogadores | 11 | o mais alto |
| 14 | Adicionar os mapas não listados nas seleções de fases | ? | médio? |

10 é um caso particular de "adicionar monstro" (8). A campanha com 4 fica por último: cinema, chefes e regras foram feitos para 1 jogador.

### 8. Mods que adicionam monstros
Como funciona hoje: `docs/passos/MODS.md` ("Skins", "Monstros custom").
- **Monstros com arquivos próprios:** campo `files` no `monster.json`, com um tipo livre (6, 12 ou 16) ou remapeando um tipo. Hoje o custom usa o tipo de um monstro do jogo. Modelo novo depende de o `.NGP` voltar pelo kit (hoje ele só sai).
- **Custom na seleção de 3 e 4 jogadores** (a casa CUSTOM só existe nas seleções de 1 e 2).
- **Skins no elimination (modo 6):** o `Shell::LoadMonstersDB` carrega o traje 0 dos 10 monstros uma vez, e os jogadores trocam de monstro entre as rodadas sem recarregar. Escolher traje ali exige mudar a carga (um arquivo por monstro escolhido), não só a tela.
- **Seleção nativa, pendências para colaborador** (`docs/investigacoes/2026-10-23/selecao_nativa_pendencias.md`, itens 6 a 8): Togera desalinhado no quadro único do adventure; quadros dos monstros fora do telão; a tela 17 (`screenSelect1AI`) sem a seleção nativa.
- **Arquivo de mod maior que o do disco** fora de `MON/*`: o jogo carrega em regiões de tamanho fixo e pode sobrescrever o que vem depois (o log avisa). Abrir espaço por tipo de arquivo.


### 10. Chefes jogáveis
- Os chefes já estão na tabela de tipos (em `MonsterNames.h`, índices 13 a 15); a seleção vai de 1 a 9 (até 10 com o bônus, `findAvailableMonster`).
- Alguns chefes não são `Monster` comum (`NATIVO.md`, "Vencer a luta"): precisam de estados de controle, golpes e escala que caibam numa luta normal. Entram na seleção pelo caminho do item 8.
- Goliath Prime já está feito, restam as 3 fases de Cerebulon. Plantboss não é viável.

### 11. Multiplayer de 4
Como funciona hoje: `docs/passos/NATIVO.md`, "Tela dividida de 3 e 4 jogadores" e "Menu multijogador de 2 a 4".
- **Seleção de 3 e 4 perdida em merges:** CPUs (tela 13) e a lista de modos (eliminação, minigames) com 3 e 4 sumiram do `wotm_padsel.inc`; refazer em cima do arquivo atual e corrigir o `NATIVO.md`, que as descreve como existentes (`docs/investigacoes/2026-10-25/multiplayer_merges_perdidos.md`).
- **Custo de 3 e 4 vistas:** plano em `docs/investigacoes/2026-10-23/performance_vistas.md`. Falta: partículas uma vez por redesenho, cache de vértices entre vistas, malhas estáticas da fase na GPU e o lado do EE (o `hier(v, 0)` custa ~4 ms por vista; a montagem dos pacotes do VU1 não serve ao desenho nativo). Num Ryzen 3 3200G a lógica fica a 30 com 3 e 4 jogadores.
- **Testes que faltam:** alguém jogar com 4 controles físicos; pontos de verdade no big shot e no crush (a IA não joga os minigames).
- **`TheGame::m_tokens`:** o `include/game/game.h` diz `[2]` seguido de um pad, mas são 4 `TokenManager` (0x1004 cada, de 0x11C370 a 0x120380, inicializados no `InitBeforeDbLoad`). Corrigir o header pede o gate.
- **Barras dos inimigos:** um bloco perto do topo da tela pode ficar por cima do HUD do jogador (a posição vem da cabeça).

### 13. Campanha com até 4 jogadores
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no Capitólio, 0x1A0) precisam ser revistas.

### 14. Adicionar os mapas não listados nas seleções de fases
Atualmente tem vários mapas (e.g. minibaytown) que não são selecinoáveis pela interface. Precisamos adicioná-los como opções a mais na seleção de fases, além de uma opção Random.
Cada mapa precisaria de um screenshot se eles já não tiverem. O random precisa de uma arte própria.

## Brechas (baratas, em qualquer momento)
- Páginas do cartão com `WOTM_SAVEAUTO=0` sem mouse (o gancho do `McPage::updateMenuPos` só é instalado com o salvamento automático). Na verdade SAVEAUTO=0 parece ser resquício de debugging. Não há porque não ter autosave ligado.
- Jogadores 3 e 4 leem o esquema de controle do `m_vibration[0..1]`; sem a linha CONTROL SCHEME na pausa, ninguém muda esse valor.
- Pular a intro como opção do executável em menu principal -> Recompiled Options -> Features -> SKIP INTROS.
- **Narrador: opção OWN MONSTER CRY** na linha NARRATOR (aba FEATURES do "Recompiled options", hoje OFF ou GODZILLA SAVE THE EARTH): ao confirmar um monstro, tocar o grito do próprio monstro (o som do jogo) em vez do `<nome>.mp3`. Por onde começar: `native/runtime/wotm_narrator.inc` e a opção `Announcer` em `native/wotm_settings.hpp`; falta achar no banco de som do monstro qual é o grito (`wotm_snd.hpp`). As fases continuam com o MP3 ou ficam em silêncio nessa opção (o dono decide).

## Contínuas
- **Interpolação de quadros (escolhida pelo dono):** a apresentação já vai na taxa do monitor (`NATIVO.md`, "Taxa de apresentação"), mas repete o quadro do jogo. Para quadros intermediários, o nativo desenharia a mistura dos dois últimos instantâneos (câmera, matrizes dos objetos, paleta de ossos), com um quadro de atraso; partículas e objetos que nascem ou somem entre os dois pedem tratamento à parte. Rodar a lógica acima de 60 não serve: o `updateRate` sai de 60/fields e muita coisa conta em quadros. Estudo em `docs/investigacoes/2026-10-19/interpolacao.md`.
- **Fora do caminho crítico:** PR #277 no PS2Recomp (`SQRT.S`/`RSQRT.S`); oracle para funções com chamadas; cobertura com gameplay para realimentar o `boot_coverage_asm.csv` (`docs/investigacoes/2026-10-19/pendencias_oracle_cobertura.md`).
- **Migrar os docs de referência para o presente** (sem datas nem história): `tools/doclint.py` lista os que faltam.

## Fora da ordem (até o dono escolher)
- **Seleção nativa com a cara do jogo:** a seleção de monstros nativa (telas 3, 6, 7, 18, 19 e a de 3 e 4; `wotm_padsel.inc`, `NATIVO.md`, "Seleção de monstros de 1 e 2 jogadores") ficar mais parecida com a tela do retail (molduras, cores, fonte, posição dos quadros). Comparar com `WOTM_NATSEL=0` (a tela do jogo) e as fotos de `docs/screenshots/2026-10-23/`.
- **Logos dos monstros na seleção nativa:** onde hoje sai só o nome do monstro em texto, desenhar o logo do jogo (recortado do `SHELL/SHELL.RTX`, como os retratos de `kPortraits`; achar quais texIds são os logos). Nada do jogo vai para o repo: o recorte sai do `.RTX` na hora. Monstros custom e de mod: campo `"logo"` no `monster.json` (PNG, como o `"portrait"`; `MODS.md`, "Monstros custom"), e sem ele fica o nome em texto.
- **Até 7 CPUs pelo menu:** no single player a tela 12 já vai até 7 (`NATIVO.md`, "Até 7 CPUs pelo menu"), mas as CPUs 4 a 7 saem sorteadas: a seleção (tela 19, `soloPanels` do `wotm_padsel.inc`, `kMaxPlayers` 4) precisa de até 8 quadros para o jogador escolher todas (os `curCostume`/`aiNIsDone` do retail só cobrem 3 CPUs: as extras precisam de estado nativo; trocar o sorteio do `extraCpus` pelo que foi escolhido). No multiplayer (tela 13), calcular quantas CPUs cabem com 2, 3 e 4 jogadores (RAM da carga: a linha `[wotm] carga da fase`; custo das vistas: item 11) e limitar o menu a esse número.



## Descartado pelo dono (não propor de novo)
- Tela de destrave para skins de mod (skin de mod fica sempre aberta) e som próprio por skin.
- 5 a 8 jogadores (câmera compartilhada).
- Opção no painel para fixar o LOD.
- Entrada no modo treino pelo launcher (a linha TRAINING do menu e `WOTM_TRAINING=1` bastam).
- Netcode próprio (rollback): exigiria provar que a lógica dá sempre o mesmo resultado com os mesmos comandos.
- Fazer o Goliath Prime ou outros bosses funcionarem por inteiro (CPU, colisão, animações que o ASSBOSS não tem): monstros custom são mods/experimentais.
