# TODO

O que falta fazer, e só isso. A lista é do dono: ele decide a ordem, e relatos de bug dele passam na frente (bugs gráficos abertos
ficam em `docs/passos/VU1CHECK.md`). Item terminado sai deste arquivo: a história vai para a mensagem do commit e o que passou a
existir vai para o doc de referência (no presente). Regras dos docs: `CLAUDE.md`, "Documentação".

## Ordem
| # | Item | Depende de | Custo |
|---|---|---|---|
| 8 | Mods que adicionam monstros (as skins já existem) | — | médio/alto |
| 9 | Modo treino: extras | — | médio |
| 10 | Chefes jogáveis | 8, 9 | alto |
| 11 | Multiplayer de 4: custo das vistas e testes com controles físicos | — | alto |
| 13 | Campanha (adventure) com até 4 jogadores | 11 | o mais alto |

9 vem antes de 10 porque balancear chefes e monstros de mods pede ver hitboxes e dano. 10 é um caso particular de "adicionar
monstro" (8). A campanha com 4 fica por último: cinema, chefes e regras foram feitos para 1 jogador.

### 8. Mods que adicionam monstros
Como funciona hoje: `docs/passos/MODS.md` ("Skins", "Monstros custom").
- **Monstros com arquivos próprios:** campo `files` no `monster.json`, com um tipo livre (6, 12 ou 16) ou remapeando um tipo. Hoje o
  custom usa o tipo de um monstro do jogo. Modelo novo depende de o `.NGP` voltar pelo kit (hoje ele só sai).
- **Custom na seleção de 3 e 4 jogadores** (a casa CUSTOM só existe nas seleções de 1 e 2).
- **Custom como CPU** (`"ai"`): a IA do chefe precisa da fase dele.
- **Animações que o ASSBOSS não tem** para o Goliath Prime (giro, olhar, dardo e retrato por PNG já existem).
- **Skins no elimination (modo 6):** o `Shell::LoadMonstersDB` carrega o traje 0 dos 10 monstros uma vez, e os jogadores trocam de
  monstro entre as rodadas sem recarregar. Escolher traje ali exige mudar a carga (um arquivo por monstro escolhido), não só a tela.
- **Seleção nativa, pendências para colaborador** (`docs/investigacoes/2026-10-23/selecao_nativa_pendencias.md`, itens 6 a 8):
  Togera desalinhado no quadro único do adventure; quadros dos monstros fora do telão; a tela 17 (`screenSelect1AI`) sem a seleção
  nativa.
- **PNG de mod** nas partículas (as UVs dos sprites são em texels) e no monstro 3D da lista de saves.
- **Arquivo de mod maior que o do disco** fora de `MON/*`: o jogo carrega em regiões de tamanho fixo e pode sobrescrever o que vem
  depois (o log avisa). Abrir espaço por tipo de arquivo.

### 9. Modo treino: extras
Como funciona hoje: `docs/passos/NATIVO.md`, "Runtime: modo treino".
1. Desenho do pisão e do agarrão (sem geometria: vítima por distância/cone, `getClosestMonsterToPunch`), da investida (esferas do
   corpo, Monster +0x2B0), do fogo (`FireBreath::TestCollis`, `hdLosCSTest`) e dos especiais de cone (`StateSonicRoar::testConeCollis`).
2. Boneco que pula (X em ciclos; agachar não existe no jogo). Dados de quadro do bloqueio (vantagem no bloqueio: reação 0x1C com o
   flag de recuo bloqueado).
3. Tabela dos golpes do monstro (raio, janela, dano, atordoamento) lida do `RST/<MONSTRO>.CHR` (chave 0xFCC; virar
   `tools/chr_dump.py`) e, com a janela do golpe em curso (`StatePunch` +0x2A00), mostrar a esfera antes do teste.
4. Entrada pelo launcher. Goliath: o casco sai com 0 planos no `.NGP` (conferir ao vivo).

### 10. Chefes jogáveis
- Os chefes já estão na tabela de tipos (`assboss`, `jelly`, `final` em `MonsterNames.h`, índices 13 a 15); a seleção vai de 1 a 9
  (até 10 com o bônus, `findAvailableMonster`).
- Alguns chefes não são `Monster` comum (`NATIVO.md`, "Vencer a luta"; câmera própria em `InitPlantBossCamera`): precisam de estados
  de controle, golpes e escala que caibam numa luta normal. Entram na seleção pelo caminho do item 8 (o Goliath Prime é o primeiro).

### 11. Multiplayer de 4
Como funciona hoje: `docs/passos/NATIVO.md`, "Tela dividida de 3 e 4 jogadores" e "Menu multijogador de 2 a 4".
- **Custo de 3 e 4 vistas:** plano em `docs/investigacoes/2026-10-23/performance_vistas.md`. Falta: partículas uma vez por
  redesenho, cache de vértices entre vistas, malhas estáticas da fase na GPU e o lado do EE (o `hier(v, 0)` custa ~4 ms por vista;
  a montagem dos pacotes do VU1 não serve ao desenho nativo). Num Ryzen 3 3200G a lógica fica a 30 com 3 e 4 jogadores.
- **Testes que faltam:** alguém jogar com 4 controles físicos; pontos de verdade no big shot e no crush (a IA não joga os minigames).
- **`TheGame::m_tokens`:** o `include/game/game.h` diz `[2]` seguido de um pad, mas são 4 `TokenManager` (0x1004 cada, de 0x11C370 a
  0x120380, inicializados no `InitBeforeDbLoad`). Corrigir o header pede o gate.
- **Barras dos inimigos:** um bloco perto do topo da tela pode ficar por cima do HUD do jogador (a posição vem da cabeça).

### 13. Campanha com até 4
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no
  Capitólio, 0x1A0) precisam ser revistas.

## Brechas (baratas, em qualquer momento)
- Páginas do cartão com `WOTM_SAVEAUTO=0` sem mouse (o gancho do `McPage::updateMenuPos` só é instalado com o salvamento automático).
- Jogadores 3 e 4 leem o esquema de controle do `m_vibration[0..1]`; sem a linha CONTROL SCHEME na pausa, ninguém muda esse valor.
- Pular a intro como opção do executável.

## Contínuas
- **Interpolação de quadros (escolhida pelo dono):** a apresentação já vai na taxa do monitor (`NATIVO.md`, "Taxa de apresentação"),
  mas repete o quadro do jogo. Para quadros intermediários, o nativo desenharia a mistura dos dois últimos instantâneos (câmera,
  matrizes dos objetos, paleta de ossos), com um quadro de atraso; partículas e objetos que nascem ou somem entre os dois pedem
  tratamento à parte. Rodar a lógica acima de 60 não serve: o `updateRate` sai de 60/fields e muita coisa conta em quadros. Estudo em
  `docs/investigacoes/2026-10-19/interpolacao.md`.
- **Decomp só na superfície de mod** (`Shell::Load*`, `MonsterParse`, câmera); fora dela, a fila é `config/boot_coverage_asm.csv`.
  Verificação: `tools/verify_tu.py`; funções com VU0 ou `min/max/madd`, `tools/recomp_oracle` (só folhas).
- **Fora do caminho crítico:** PR #277 no PS2Recomp (`SQRT.S`/`RSQRT.S`); oracle para funções com chamadas; cobertura com gameplay
  para realimentar o `boot_coverage_asm.csv` (`docs/investigacoes/2026-10-19/pendencias_oracle_cobertura.md`).
- **Migrar os docs de referência para o presente** (sem datas nem história): `tools/doclint.py` lista os que faltam.

## Fora da ordem (até o dono escolher)
- **Online:** com o multiplayer local de 4, o Steam Remote Play Together e o Parsec já permitem jogar online sem netcode: testar e
  documentar.
- **Mod low-poly:** os modelos com níveis de LOD baixos; base em `docs/investigacoes/2026-10-23/lod_fixo.md` (onde o nível é
  escolhido, quais modelos têm níveis baixos).
- **Modo Nintendo DS:** LOD sempre no nível mais alto (o LOD é escolhido no nativo); filtro de áudio em tempo real no som nativo
  (`wotm_snd.hpp`); talvez duas telas, jogo em cima e HUD embaixo com o retrato (o HUD do `hier` já é desenhado no nativo).
- **Pedestres do GTA SA e o CJ jogável em tamanho real:** os pedestres de hoje são partículas (fx 91–99, `VU1CHECK.md`), então NPCs
  3D são desenho e animação novos. O CJ depende do item 8. Assets de outro jogo não entram no repo: viriam dos arquivos do usuário,
  como mod.
- **"Build your own city":** o jogo não tem editor; a fase é o `LVL/<fase>.NGP` (hier com os interativos, `dbInitDb`/
  `dbProcInteractive`; `docs/FORMATOS.md`), com colisão, caminhos da IA (`PathNode`) e prédios destrutíveis. Primeiro degrau: um mod
  que muda de lugar, tira ou duplica prédios de uma fase existente; um editor visual (com a câmera livre do F5) vem depois.

## Descartado pelo dono (não propor de novo)
- Tela de destrave para skins de mod (skin de mod fica sempre aberta) e som próprio por skin.
- 5 a 8 jogadores (câmera compartilhada).
- Opção no painel para fixar o LOD.
- Netcode próprio (rollback): exigiria provar que a lógica dá sempre o mesmo resultado com os mesmos comandos.
- Mudar a luz do monstro 3D da seleção (aprovada como está; caminho em `8498ce0:docs/investigacoes/2026-10-24/luz_selecao_3d.md`).
