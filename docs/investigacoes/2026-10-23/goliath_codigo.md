# Goliath Prime (`assboss`, tipo 13, `m_typeBits` 0x1A0) como monstro de jogador: o que o código faz de especial

Investigação de 2026-10-23, só leitura de código (`src/game`, `asm/` lido e não colado) e dos arquivos do disco do dono
(`disc/MON`, `disc/RST`). Nada rodado no runtime. Objetivo: jogar com o Goliath numa luta comum (POS_FRONTEIRA item 10).

## Resumo

- **O motivo principal do teste ruim é o `TheGame::SetAIMonster` (0x137B10, `src/game/TheGame.cpp:353-389`):** a cada IA que
  entra, ele percorre **todos** os slots e desliga quem tem `m_typeBits == 0x1A0` (`disableMonster`: `m_playerNum = 0`,
  `drawMe = 0` no `m_cs`, na sombra e nos `_cs` de +0x71F8/+0x71FC). Não importa se o Goliath é jogador. Com `m_playerNum` 0, o
  `TheGame::Update` (`TheGame.cpp:147-170`) nem chama `update`/`updatePosition` dele: fica invisível, congelado, e a câmera do
  jogador segue um monstro que nunca andou nem animou. Na aventura é o que esconde o chefe até a cena (`canyon2BossIntro`).
- **Mesmo religado, ele não toma dano das IAs:** `Monster::takeHit` (0x15F048, `src/game/Monster.cpp:923-932`) só deixa o
  dano passar no 0x1A0 se o estado global do chefe (`attackState`, 0x6F7974) for 4 ou 10, ou se quem bateu for
  `game->m_monsters[0]` (o jogador 1). Com o Goliath sendo o jogador 1, ninguém o machuca.
- **Use o traje 1 (`ASSBOSS1`).** O `WOTM_MATCH=13,1,1` sem `:` pega o traje 0 = `ASSBOSS0`, protótipo de mar/2002: 53
  animações (faltam andar 1/7, arremesso 52, golpe forte 75, 150-155 e 194-197) e retrato do HUD de 64x32 em vez de 128x64.
- A IA de chefe (`Ai::updateAssBoss`) só roda se o Goliath tiver o cérebro de IA (`Ai::init` liga `initAssBoss` pelo tipo).
  Com jogador humano ela nunca é chamada. O `getAssBossState()` dos estados lê um global e não usa `this`, então não quebra com
  jogador.

## 1. Como a aventura cria o Goliath

- **Fase:** 3, `canyon2` (Rosdale Canyon), modo 1 (aventura). `levelMonsters[3]` = 3 × `mantis` (Mutant Ants, traje 1) +
  `assboss` (GLOSSARIO.md). Não tem carregador de chefe próprio: ele é uma IA comum da lista da fase, criada por
  `Shell::InitPlayers` → `spawnAIs` → `TheGame::SetAIMonster` (`src/game/Shell.cpp:394-424, 465-468`), com os arquivos
  carregados como os de qualquer monstro (`MON/ASSBOSS<traje>.*`).
- **Cérebro:** `SetAIMonster` → `Monster::aiInit` (0x166038) → `Ai::init` (0x101470). Logo depois do `AiNavigator::init`,
  se o tipo é 0x1A0 ele chama `Ai::initAssBoss` (0x103760), que grava **`assBoss = monstro`** (único lugar que escreve esse
  global), zera os globais do chefe e chama `setAssBossState(9)`. Na mesma passada do `SetAIMonster`, o `disableMonster` o
  esconde.
- **Por quadro:** `Ai::updateInputs` (0x101788): para o 0x1A0 chama `Ai::updateAssBoss` (0x103C30) no lugar da IA normal. Essa
  função toca o som das hélices e roda `updateAssBossPatternZero/One/Two` (as três partes da luta; `assBossTier1/2Health`
  separam as partes). `setAssBossState` (0x1037F0) é uma máquina de 15 estados: grava `attackState`, usa o `AiNavigator` do
  próprio `Ai`, os pontos `retreatPts` (0x25C4C0, coordenadas de canyon2), o global `assBoss` e força estados com
  `enterNewState` (Taunt, TopSpin, CannonHands).
- **Quem liga o resto** (`asm/game/canyon2.s`): `TheGame::InitBeforeDbLoad` e `canyon2InitBefore/After` zeram
  `assBossActive`. `canyon2Update` chama, conforme o roteiro:
  - `canyon2BossIntro` (0x122320): `canyon2LevelProgression = 2`, carrega o banco de som `Assemb_s.bnk`
    (`SoundManager::loadMechBossSoundBank`), filme/roteiro 0x226F, esconde as 3 formigas (`m_monsters[4..6]`: `drawMe` 0 e
    `m_playerNum` 0), liga `drawMe` do `m_cs` do `assBoss` e chama `aiInit` de novo (`m_playerNum` 2).
  - `canyon2EnableBoss` (0x122438): progressão 3, **`assBossActive = 1`**, `setAssBossState(0)`.
  - `canyon2FinalBattle` (0x122470): progressão 6, `setAssBossState(0)`, `setTrans(0, 0, 700)` (nasce no alto, no centro do
    mapa), `dropPickup`, som da represa.
  - `Shell::InitPlayers` (`Shell.cpp:469-485`): ao reiniciar na progressão 3 ou 6, restaura a vida do `assBoss` (+0x448 máx.,
    +0x44C atual) e reconstrói os pilares.
- **Escala:** nenhuma no código. O modelo já vem grande no `.NGP` (seção 4).
- **Câmera própria:** não tem (nada em `canyon2.s` chama `Cameras`; só a planta tem `InitPlantBossCamera`). Na aventura a câmera
  segue o jogador. Os golpes do Goliath **não** abrem a câmera de combate de perto: `StatePunch::startChain` e
  `StateGetupAttack::update` pulam o `Cameras::InitCCCamera` para 0x1A0 e 0x1C0.

## 2. Todas as ramificações por tipo 0x1A0 / `assBoss` / `assBossActive`

Legenda do risco com jogador humano: **OK** = inofensivo, **bloqueia** = impede jogar, **muda** = jogável com outro
comportamento.

| Onde | O que faz | Com jogador |
|---|---|---|
| `TheGame::SetAIMonster` (`TheGame.cpp:379`) | desliga todo slot 0x1A0 (`disableMonster`) | **bloqueia** (invisível, parado) |
| `Monster::takeHit` (`Monster.cpp:923-932`) | dano só com `attackState` 4/10 ou de `m_monsters[0]` | **bloqueia** (invulnerável às IAs) |
| `Monster::takeHit`, tipos 5/1/7/10 (`Monster.cpp:1072, 1096, 1155, 1190, 1225`) | choque, recuo, atordoar e queimar viram recuo aditivo | muda (não se abala) |
| `Monster::takeHit` (`Monster.cpp:1245`) | golpe de um 0x1A0 grava `attackSuccessful = 1` (global da IA) | OK |
| `Monster::knockBack` (`Monster.cpp:1345`) | só `StateBigTakeHit` (anim 0x4B), e não se estiver bloqueando | OK (ASSBOSS1 tem a 75; ASSBOSS0 não) |
| `Monster::getClosestMonsterToPunch` (`Monster.cpp:1628`) | com `assBossActive`, nada a socar 100 acima do chefe | OK (`assBossActive` só em canyon2) |
| `Monster::updateBoostAndRage` (`Monster.cpp:485`) | sem fúria para 0x1A0/0x1C0 | muda (sem fúria) |
| `Monster::updateBoundingSphere` (0x15BB68) | raio mínimo 50 (outros 10) | OK |
| `Monster::updatePowerUpGlow`, `Monster::addAttachment` | brilho e anexo (+0x8000+0x7A80) próprios | OK |
| `Monster::initDynamics` (0x166258) | especial = `StateCannonHands` (+0x10F60), arma 0xA (+0x6878) | OK: o especial dele |
| `collisInitPoints` (0x12B158) | pontos de colisão maiores (seção 4) | OK |
| `StateRun::init` (`MovementStates.cpp:2652`) | corrida 400 mph, andar 142 | OK |
| `StateJump::transitionOK` (`MovementStates.cpp:1979`) | não pula com +0x68B4 (vítima agarrada) | OK |
| `StateJump::update` (`MovementStates.cpp:2078`) | soltar o botão não corta a subida | muda |
| `StateButtSlam::transitionOK` (`MovementStates.cpp:528`) | sem butt slam (mesmo com as anims 261/262) | muda (opcional liberar) |
| `StateButtSlam::handleCollis` (`MovementStates.cpp:704`) | poeira 10x maior | OK |
| `StateRamAttack::transitionInto/handleCollis/acceptHit` (`MovementStates.cpp:2352, 2429-2450, 2573`) | investida atravessa prédios (hats 0x20..0x400), aceita todo golpe e derruba `m_monsters[0]` se ele estiver no hat batido | muda: com o Goliath como jogador 1, o derrubado é **ele mesmo** (caso raro) |
| `StateIdle::update` (0x14F418) | não chama `updateTurn(true)` | OK |
| `StateTaunt::transitionInto/update` (`MonsterStates.cpp:916-951`) | anim 0x5D se `getAssBossState()==8`, senão 0x5C; vira para o alvo | OK (global; ASSBOSS1 tem 92 e 93) |
| `StatePunch::transitionOK` (0x154BC0) | um teste só de IA (`m_playerNum` 2) pulado para 0x1A0 | OK |
| `StatePunch::handleCollis` (0x155D70) | caso próprio que olha `m_monsters[0]` (hat sob o jogador 1) | a conferir (lógica de chefe) |
| `StateBatSwipe::update` | não pausa a anim no impacto | OK |
| `StateTopSpin::update` (0x1CC4A8) | giro do chefe; chama `setAssBossState(owner->m_ai, 0xE/5)` | **perigoso**, mas só a IA do chefe entra nele (nenhuma transição comum o alcança) |
| `StateGrapple::transitionFeasible` (`Grapple.cpp:219`) | ninguém agarra 0x1A0/0x1C0 | muda (ele agarra, não é agarrado) |
| `HomingBug::attach` (`HomingBug.cpp:583`) | inseto não gruda | OK |
| `PowerUps::TakeHit` (`PowerUps.cpp:488, 521`) | não pega vida nem stamina | muda |
| `PowerUps::update` (`PowerUps.cpp:180`), `Hud::update`, `Hud::addHealthIndicators`, `TheGame::gameCheckForCloseCombat`, sons de torreta e mísseis | só na fase 3 + aventura ou com `assBossActive`; o HUD lê `assBoss` (barra do chefe) | OK numa fase comum |
| `RigidPickup::takeHit` | som de impacto próprio, ignora derrubar pelo objeto | OK |
| `MonsterSound` (`Sound.cpp:2659-3480`) | quase todos os sons do 0x1A0 vêm de `gSoundBanks[BANK_MECH]` (22) | muda: mudo fora de canyon2 (o banco só carrega no `canyon2BossIntro`; `loadMonsterBank` não tem caso 0x1A0) |
| `Ai::init` / `Ai::updateInputs` | `initAssBoss` / `updateAssBoss` no lugar da IA normal | só se o Goliath estiver sob IA (`WOTM_AIVSAI` ou como oponente) |

Chamadas `AI_OF(owner)->getAssBossState()`: `getAssBossState` (0x1037E8) só devolve o global 0x6F8B84 e ignora `this`. Com
o `m_ai` embutido de um jogador, não quebra. Perigo real só em `setAssBossState` (usa o `Ai`, o navegador, `retreatPts` e
desreferencia `assBoss`, que fica nulo se o `initAssBoss` nunca rodou). Só o `StateTopSpin` e a própria IA do chefe chamam essa
função. Com jogador humano, o TopSpin não tem como começar.

## 3. Animações (`.NGP`) do ASSBOSS x monstros comuns

Lido com `tools/pose.py <ngp> --list` (ids = `m_anims[id]`). O `.NGP` tem a mesma estrutura dos outros: raiz 0 `GROUP` com o
id do tipo (0x1A0), raiz 1 `GROUP` 0, `COLL_GRID` e `ANIM_MGR`. Também existe `disc/RST/ASSBOSS.CHR` (o arquivo de
parâmetros do `charreset`, do mesmo tamanho e formato dos outros `.CHR`).

- ASSBOSS1: 70 ids distintos; ASSBOSS0: 53 (todos contidos nos do 1). LIZARD0: 151, MONKEY0: 169.
- **Tem** (ASSBOSS1): andar e correr (1, 2, 5, 7), pulo (30, 33-38, sem os 31/32 de "pé preferido"), arremesso (52, 55),
  bloqueio básico (60), arremessado/derrubado (87, 88, 250-256), golpe forte (75), idle (90), provocação e vitória (92, 93),
  agarrão e ataque agarrado (170, 171, 185), butt slam (261, 262), investida (266, 267), CannonHands (160-162), TopSpin
  (163-165), além de 0, 3, 6, 8-12, 39, 67-71, 120, 145, 150-155, 166-167, 172, 177, 194-197 (provavelmente a sequência de
  socos e os golpes de chefe; os ids dos socos vêm das tabelas `PunchConfig`, não conferidas).
- **Não tem** (os dois comuns têm): escalar (100-106), variantes de bloqueio e contra-ataque (61-64), choque (74), "countered"
  (76), dash (264, 265), correr carregando algo grande (173), ser agarrado (174, 186, 280), empalado (142, 144), pisão (240),
  especiais de arma (244, 245), dardo (54), segundo arremesso (56), 13-21, 41/42, 49, entre outros.
- Efeito: os `transitionOK` checam `m_anims[id].ci != 0`, então os estados sem animação simplesmente não começam (não escala,
  não dá dash, não contra-ataca). Ser agarrado já é proibido pelo tipo. Nada aqui deve travar o jogo, mas o conjunto é menor
  que o de um monstro comum.

## 4. Tamanho

- Malha na pose do idle (`pose.py`, caixa das posições): ASSBOSS 306 × 250 × 201 (altura 250); Togera 91 × 100 × 179,
  Congar 117 × 92 × 55, Robo-47 107 × 107 × 74. Uns **2,5× mais alto** e 3× mais largo.
- `collisInitPoints`: alturas dos pontos de colisão 90 / 125 / 190 / 110 e raio (+0x360) 45. Togera: 45 / 80, raio 35;
  Congar: 45 / 60, raio 30. O 0x1C0 (chefe final) usa 100 / 175 e raio 48.
- `updateBoundingSphere`: raio mínimo 50 (outros 10).
- Câmera: os parâmetros por ponto de vista vêm do `.CHR` do monstro (`charreset::readKey` grava `Monster+0x6CF0`, 0xA0 por
  POV, mais 0x640 para a tela dividida; `Cameras::SetCameraPOV` lê dali). O `ASSBOSS.CHR` existe, mas na aventura a câmera nunca
  segue o Goliath. Esses valores **nunca foram usados pelo jogo**: podem estar afinados ou podem ser os padrões do editor. A
  câmera de luta normal deve funcionar, talvez perto demais. Não dá para afirmar sem ler a RAM.

## 5. Câmera parada no céu e Goliath invisível (hipóteses, da mais à menos provável)

1. **`disableMonster` no `SetAIMonster`** (quase certo). Na RAM: `m = READ32(game + 0x120380)` (`m_monsters[0]`), `m+0x14` =
   0x1A0, `m+0x18` (`m_playerNum`) = **0** e o byte `drawMe` (+0xC) do `_cs` em `m+0xC` = 0. Teste rápido sem RAM:
   `WOTM_MATCH=13:1,1` (sozinho, sem IA): `spawnAIs` não chama o `SetAIMonster`, e ele deve aparecer e andar.
2. **Câmera com parâmetros do `ASSBOSS.CHR` ruins ou pensados para outro tamanho.** Comparar os 0xA0 bytes de `m+0x6CF0`
   (POV atual) do Goliath com os do Congar no mesmo dump.
3. **Traje 0 (ASSBOSS0), protótipo:** faltam animações de andar e outras. Não explica o céu, mas atrapalha o teste seguinte.
   Usar `13:1`.
4. Retrato do HUD do jogador com a cara do Congar: o ASSBOSS0 só tem retrato de 64x32 (o ASSBOSS1 e os outros têm 128x64, a
   última entrada do `.RTX`). O retrato do HUD lê 128x64 na área residente do primeiro arquivo de monstro, e o arquivo seguinte
   (Congar) começa logo depois. Hipótese: com o retrato menor, o que aparece é o começo do retrato do Congar. A conferir com o
   ASSBOSS1. O retrato nativo das barras de inimigo (`monsterPortrait`) só aceita 128x64 e não acharia o do ASSBOSS0.

## 6. Plano mínimo (ganchos em `native/runtime`, sem decomp obrigatória)

**Necessário:**
1. **Gancho no `TheGame::SetAIMonster`** (`wotm::fn::SetAIMonster__7TheGameiiii`). Na entrada, guardar, para cada jogador
   `m_monsters[0 .. m_numPlayers-1]` com tipo 0x1A0, o `m_playerNum` e os 4 bytes `drawMe` que o `disableMonster` zera (`m_cs`,
   sombra, `_cs` de +0x71F8 e +0x71FC). Na saída, restaurar. Encadear com o `setAIMonsterCounted` do `WOTM_AIVSAI` (o mesmo
   endereço já é trocado ali). A partir daí o `m_numAIs` fica certo: o `SetAIMonster` só conta os slots com `m_playerNum` 2.
2. **Dano das IAs:** gancho na entrada do `Monster::takeHit` (`wotm::fn::takeHit__7MonsterP8_fvectorfi`) quando `this` é um
   jogador 0x1A0. Opções, da mais simples à mais completa:
   - (a) gravar `attackState` = 4 durante a chamada e restaurar depois. Ele toma o dano cheio sem reação nenhuma (nem bloqueio).
     Fica um "chefe inabalável".
   - (b) fazer o teste `attackerId == m_monsters[0]->m_id` passar (trocar `game+0x120380` pelo atacante durante a chamada).
     Segue o caminho normal (bloqueio, `StateBigTakeHit`, recuos aditivos), mas mexe num ponteiro que outras partes do
     `takeHit` podem ler. Conferir no asm antes.
   - Preferir (b) se o bloqueio dele tiver que funcionar; (a) basta para o primeiro teste.
3. **Traje 1** por padrão para o tipo 13 na `WOTM_MATCH` e na seleção (o 0 é o protótipo).

**Opcional:**
- Som: chamar `SoundManager::loadMechBossSoundBank` (0x1B2768, `this` = `shell + 0x2C30`) depois da carga da fase quando houver
  um 0x1A0, conferindo a RAM do IOP para o `Assemb_s.bnk`. Sem isso ele fica mudo (passos, golpes, morte).
- Butt slam: gancho no `StateButtSlam::transitionOK` (0x16E338) pulando o teste de tipo (as anims 261/262 existem).
- Fúria e pickups de vida e stamina: o tipo os bloqueia (`updateBoostAndRage`, `PowerUps::TakeHit`). Liberar com ganchos que
  troquem o `m_typeBits` só durante a chamada, se o dono quiser.
- Investida: o caso do `StateRamAttack::handleCollis` que derruba `m_monsters[0]` acertaria ele mesmo. Raro; ignorar ou trocar
  o alvo.
- Câmera: se a do `ASSBOSS.CHR` ficar ruim, copiar ou ajustar os POVs de `m+0x6CF0` depois do `playerInit` (ex.: os de um monstro
  grande com a distância escalada por ~2,5).
- Goliath como **oponente** de IA numa fase comum: o `Ai::updateInputs` roda a IA de chefe (padrões e `retreatPts` de canyon2).
  Para uma IA genérica, um gancho que mascare o tipo no `Ai::init`/`updateInputs` (sem `initAssBoss`/`updateAssBoss`). Aí também é
  preciso religar o slot (item 1 vale para a IA também) e decidir o dano (item 2: com o Goliath como IA, só o jogador 1 o
  machuca, o que numa luta 1x1 já basta).
- Golpes de chefe (TopSpin, salvas de mísseis): dependem de `setAssBossState`. Para um jogador, só reimplementando a entrada no
  TopSpin sem a IA, o que fica para depois. O CannonHands (especial, anims 160-162) já é o especial normal dele
  (`m_specialState`) e não depende da IA.
