# TODO

O que falta fazer, e só isso. Relatos de bug passam na frente. Item terminado sai deste arquivo: a história vai para a mensagem do commit e o que passou a existir vai para o doc de referência (no presente). Regras dos docs: `CLAUDE.md`, "Documentação".

## Ordem
| # | Item | Depende de | Custo |
|---|---|---|---|
| 8 | Mods que adicionam monstros (as skins já existem) | — | médio/alto |
| 10 | Chefes jogáveis | 8 | alto |
| 13 | Campanha (adventure) com até 4 jogadores | — | o mais alto |

10 é um caso particular de "adicionar monstro" (8). A campanha com 4 fica por último: cinema, chefes e regras foram feitos para 1 jogador.

### 8. Mods que adicionam monstros
Como funciona hoje: `docs/passos/MODS.md` ("Skins", "Monstros custom").
- **Monstros com arquivos próprios:** campo `files` no `monster.json`, com um tipo livre (6, 12 ou 16) ou remapeando um tipo. Hoje o custom usa o tipo de um monstro do jogo. Modelo novo depende de o `.NGP` voltar pelo kit (hoje ele só sai).
- **Custom na seleção de 3 e 4 jogadores** (a casa CUSTOM só existe nas seleções de 1 e 2).
- **Skins no elimination (modo 6):** o `Shell::LoadMonstersDB` carrega o traje 0 dos 10 monstros uma vez, e os jogadores trocam de monstro entre as rodadas sem recarregar. Escolher traje ali exige mudar a carga (um arquivo por monstro escolhido), não só a tela.
- **Seleção nativa, pendências para colaborador** (`wotm_padsel.inc`; `NATIVO.md`, "Seleção de monstros de 1 e 2 jogadores"):
  - Togera desalinhado para a esquerda no quadro único do adventure (tela 3): o enquadramento (`wotm::savepics::Drawer::framingOf`, `shiftX`/`shiftY`/`zoom`) é o dos quadros da lista de saves, e o da seleção tem outra proporção (`selBoxW`, até 46% da largura). Conferir os outros monstros; `shiftX` pela proporção do quadro ou enquadramento próprio da seleção.
  - Quadros dos jogadores fora do telão: hoje vão numa textura colada nele (`drawSelect` -> `g_rt`), pequenos e inclinados. O dono os quer em 2D sobre a imagem, entre a faixa preta de cima e a barra de monstros. Nas duas seleções nativas (telas 3/6 e 3 e 4 jogadores).
  - Tela 17 (`screenSelect1AI`) sem a seleção nativa (não aparece nos caminhos das telas 18 e 19).
- **Arquivo de mod maior que o do disco** fora de `MON/*`: o jogo carrega em regiões de tamanho fixo e pode sobrescrever o que vem depois (o log avisa). Abrir espaço por tipo de arquivo.
- **Aviso dos tetos para skin com `.TEX`/`.RTX`/`.PTR` próprios:** a skin não tem original para comparar, então hoje nada avisa. O log de carga deveria comparar com os tetos reais: `texInfo[1000]` (`maxTexId` final), VRAM residente do `.RTX` e o buffer de 0x30D40 do `.PTR` (`0f091a4:docs/investigacoes/2026-10-23/skins_mod_carga.md`, item 8).


### 10. Chefes jogáveis
- Os chefes já estão na tabela de tipos (em `MonsterNames.h`, índices 13 a 15); a seleção vai de 1 a 9 (até 10 com o bônus, `findAvailableMonster`).
- Alguns chefes não são `Monster` comum (`NATIVO.md`, "Vencer a luta"): precisam de estados de controle, golpes e escala que caibam numa luta normal. Entram na seleção pelo caminho do item 8.
- Goliath Prime já está feito, restam as 3 fases de Cerebulon. Plantboss não é viável.

### 13. Campanha com até 4 jogadores
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no Capitólio, 0x1A0) precisam ser revistas.

## Brechas (baratas, em qualquer momento)

## Contínuas
- **Interpolação de quadros (escolhida pelo dono):** a apresentação já vai na taxa do monitor (`NATIVO.md`, "Taxa de apresentação"), mas repete o quadro do jogo. Para quadros intermediários, o nativo desenharia a mistura dos dois últimos instantâneos (câmera, matrizes dos objetos, paleta de ossos), com um quadro de atraso; partículas e objetos que nascem ou somem entre os dois pedem tratamento à parte. Rodar a lógica acima de 60 não serve: o `updateRate` sai de 60/fields e muita coisa conta em quadros. Estudo em `docs/investigacoes/2026-10-19/interpolacao.md` (etapa 0 medida na seção 7: cabe a 180 Hz com 1 e 2 vistas, +75-90% de CPU). Junto com a etapa 1 entra a linha `INTERPOLATE FRAMES` (OFF/ON, padrão OFF) na aba FEATURES do painel, ajuda `SMOOTHER MOTION ON MONITORS ABOVE 60 HZ. MAY ADD INPUT DELAY`; apagada em monitor de 60 Hz; no treino, com esferas ou contornos ligados, sem mistura.
- **Fora do caminho crítico:** oracle para funções com chamadas; cobertura com gameplay para realimentar o `boot_coverage_asm.csv` (`57d4afd:docs/investigacoes/2026-10-19/pendencias_oracle_cobertura.md`, seções 2 e 3).

## Fora da ordem (até o dono escolher)
- **Seleção nativa com a cara do jogo:** a seleção de monstros nativa (telas 3, 6, 7, 18, 19 e a de 3 e 4; `wotm_padsel.inc`, `NATIVO.md`, "Seleção de monstros de 1 e 2 jogadores") ficar mais parecida com a tela do retail (molduras, cores, fonte, posição dos quadros). Comparar com `WOTM_NATSEL=0` (a tela do jogo) e as fotos de `docs/screenshots/2026-10-23/`.
- **Logos dos monstros na seleção nativa:** onde hoje sai só o nome do monstro em texto, desenhar o logo do jogo (recortado do `SHELL/SHELL.RTX`, como os retratos de `kPortraits`; achar quais texIds são os logos). Nada do jogo vai para o repo: o recorte sai do `.RTX` na hora. Monstros custom e de mod: campo `"logo"` no `monster.json` (PNG, como o `"portrait"`; `MODS.md`, "Monstros custom"), e sem ele fica o nome em texto.
- **4 jogadores sempre a 60:** com 4 vistas, ~80% dos quadros saem a 60 e o resto a 30; quem segura é o EE (mediana ~14 ms, p90 ~17,5 ms). Candidatos, pelo perfil das amostras do EE (`PS2X_HOSTPROF=2` + `_RAW=1`, resolvido pelo `wotm.map` do mesmo build): o envoltório do VU0 traduzido (`executeVU0Microprogram` copia ~1 KB de estado em cada sentido, ~3%) e o `checkpointDue` fora de linha (~3%), ambos costura do PS2Recomp; a montagem do pacote do VU1 que o `wotm.exe` descarta (`pktAddVu1ObjAsm`, ~4%, arriscado: item G3 de `723b110:docs/investigacoes/2026-10-23/performance_vistas.md`). Na janela, as partículas ainda no modo imediato do rlgl (~0,6 ms por vista). Comparar alternando na mesma corrida (a luta IA x IA não se repete).



## Descartado pelo dono (não propor de novo)
- Tela de destrave para skins de mod (skin de mod fica sempre aberta) e som próprio por skin.
- 5 a 8 jogadores (câmera compartilhada).
- Opção no painel para fixar o LOD.
- Entrada no modo treino pelo launcher (a linha TRAINING do menu e `WOTM_TRAINING=1` bastam).
- Netcode próprio (rollback): exigiria provar que a lógica dá sempre o mesmo resultado com os mesmos comandos.
- Fazer o Goliath Prime ou outros bosses funcionarem por inteiro (CPU, colisão, animações que o ASSBOSS não tem): monstros custom são mods/experimentais.
