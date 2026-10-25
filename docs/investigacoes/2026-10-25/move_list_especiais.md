# Move list do modo treino: golpes especiais (2026-10-25)

Investigação só de leitura do asm (`asm/nonmatchings/game/SpecialStates/`, `MonsterStates/`, `MonsterInit/initDynamics`,
`PadFlags_2`, `charreset_2`, `HomingBug/`, `FireBreath/`, `Weapon.cpp` decompilado). Nada de assembly aqui: só offsets e
palavras. Offsets de Monster relativos ao objeto (0x11190 bytes); offsets de estado relativos ao estado (base na tabela
`kStates` de `native/runtime/wotm_training.inc`). Frames = campos de 60 Hz (`animationSetTotalRunFrames`).

## Resumo do que vale para todos

- **Quem usa o quê**: `Monster::initDynamics` escolhe o estado especial pelo tipo (`m_typeBits`, Monster +0x14 = índice << 5;
  o índice é o de `MonsterNewNames`: 1 Congar, 2 Robo-47, 3 Togera, 4 Ultra-V, 5 Preytor, 7 Kineticlops, 8 Magmo,
  9 Raptros, 10 Agamo, 11 Zorgulon, 13 Goliath Prime/assboss, 14 jelly, 15 final) e grava o ponteiro em **Monster +0x7980**
  (`m_specialState`). Quadro: Congar → SonicRoar; Robo-47 → RobotSpecial; Togera **e Raptros** → FireBreath; Ultra-V →
  GrappleHook; Preytor → BugAttack; Kineticlops → ZapAttack; Magmo → LavaBlast; Agamo → RockSpecial; Zorgulon → AirStrike;
  assboss → CannonHands; jelly e final → UltraTazer; Spider e tipos sem caso → 0. (Atenção: o pedido desta tarefa listava
  Togera=1, Congar=2, Robo-47=3; no jogo é Congar=1, Robo-47=2, Togera=3.) Monster +0x797C guarda o golpe de agarrão por
  cima (MonkeyOHAttack para o Congar, GrappleOHAttack para os outros).
- **Comando (medidor cheio)**: o `transitionOK` de todos lê a entrada 0 do anel de `PadFlags` (`operator[](0)`, Monster
  +0x5040) e testa o short **+0x36** da `PadEntry`. Em `updateInput0ConfigCR` (`asm/game/PadFlags_2.s`) esse campo é 1
  quando **círculo e triângulo** estão apertados juntos: os contadores de quadros segurando cada botão (`PadFlags` +0x1614
  círculo, +0x1616 triângulo; +0x1612 quadrado, +0x1618 X; contados em `updateHistory0` a partir dos bytes do `GamePad`
  Monster +0x502B círculo, +0x502C triângulo, +0x502D quadrado, +0x502A X) estão ambos entre 1 e o tweak `PadFlags`
  +0x16C4 (janela de "apertou junto"), ou o +0x36 do quadro anterior já valia 1 e os dois botões (`face[1]` +0xC e
  `face[2]` +0xE da entrada) continuam apertados. Ao marcar, zera a ação de agarrar (+0x2E) e o triângulo (+0x2C) e põe
  `curButtonAction` 0x14 / `curMappedAction` 0xF (Monster +0x6750/+0x6754). O mesmo bloco marca **+0x38** para
  **quadrado + X** juntos (zera +0x30 e +0x2A): é o comando do `StateCrowdControl` (ver abaixo). Confiança alta para o
  mapa de config 0; as configs 1, 2 e SR não foram lidas (devem ser iguais no par de botões).
- **Condições comuns** (`transitionOK`): `m_attacksEnabled` (Monster +0x4A) ≠ 0; `!isHoldingLarge` (não vale para
  CannonHands/UltraTazer); as três animações do especial existem (`m_anims` Monster +0x1CF0 + id×0x10: ids **0xA0**
  windup = Monster +0x26F0, **0xA1** laço = +0x2700, **0xA2** winddown = +0x2710; CrowdControl/TopSpin usam 0xA3..0xA5 =
  +0x2720/+0x2730/+0x2740); `isSpecialAvailable` (= `m_specialWeapon` Monster **+0xF3** ≠ 0 e `TheGame::m_gameMode`
  ≠ 9); depois o botão. Não há teste de stamina nem de "no chão" nos especiais de medidor (o RockSpecial e o CrowdControl
  tratam o ar à parte).
- **Custo**: `Monster::drainSpecial` (chamado quando o windup acaba e o efeito começa) zera +0xF3 e, se a stamina atual
  (Monster +0x460) está abaixo de +0x470, chama `StaminaMeter::drain(+0x470 − atual)`. Não há custo de stamina além disso.
- **Janela**: percentual da sub-animação corrente (`animationGetCurrentPercent`), com o limiar fixo **0.99** (0x3F7D70A4)
  para trocar de fase (o GrappleHook dispara a 0.87). O laço dura "iterações + resto" calculados no `transitionInto`
  (duração em frames ÷ frames do ciclo, guardados no estado).
- **Dano**: o padrão é `HitEvent::create()` + `Interactives::dispatchHit(dir, dano, idAtacante, idAlvo)`; o `HitEvent`
  leva tipo (+0xC: 2 derruba/knockback, 3 projétil, 4 fogo, 5 choque, 7 atordoa), fonte (+0x10: 5 = especial) e subtipo
  (+0x14). Os valores vêm do **construtor do estado** e são sobrescritos pelo **`.CHR`** (`charreset::readKey`, chaves
  abaixo): o treino deve ler da RAM, no offset do estado.
- **Fogo/choque continuados**: `Monster::lightOnFire(count, damage, source)` escreve Monster +0x6CB8 (quadros) e +0x6CBC
  (dano) e `updateOnFire` cobra por quadro; o choque usa +0x6CC4/+0x6CC8 pelo `takeHit` tipo 5.

### Chaves do `.CHR` que escrevem nos estados (`charreset::readKey`, tabela `jtbl_006E8C50`, chaves 0xFA1..0xFE3)

| chave | destino | campos (offset na chave → offset no estado) |
|---|---|---|
| 0xFA9 | SonicRoar | +0x8..+0x2C → +0x1C..+0x40 em ordem; +0x38 → +0x44; +0x3C → +0x48; +0x30 → +0x4C; +0x34 → +0x50 |
| 0xFAA | BugAttack e estáticos do `HomingBug` | +0x8..+0x24 → +0x1C..+0x38; em seguida (18 floats, ordem) `s_homingFactor`, `s_avoidanceFactor`, `s_maxForce`, `s_maxVel`, `s_initialVel`, `s_initialHealth`, `s_attachRange`, `s_attachMinAngle`, `s_attachFields`, `s_maxSeekFields`, `s_headRadius`, `s_feelerRadius`, `s_collisRadius`, `s_damageRadius`, `s_damagePerSecond`, `s_maxMonsterDamage`, `s_targetLockMaxRange`, `s_targetLockMinAngle` (símbolos em `.sbss`, `asm/data/common/ColGrid.sbss.s`; valores só na RAM ou no `MANTIS.CHR`) |
| 0xFAB | RobotSpecial, onda de choque | +0x10 → +0x58 (nº de anéis), +0x14/+0x18/+0x1C/+0x20 → +0x60/+0x5C/+0x64/+0x68, +0x8/+0xC → +0x74/+0x78, +0x24..+0x50 → anéis seguintes (+0x88..+0xCC) |
| 0xFAD | FireBreath | +0x18..+0x24 → +0x1C..+0x28; +0x30 → +0x2C; +0x34 → +0x30; +0x3C → +0x34; +0x8..+0x14 → +0x38..+0x44; +0x28/+0x2C → +0x48/+0x4C |
| 0xFBC | RockSpecial | +0x8..+0x28 → +0x20..+0x40 em ordem (a onda de choque fica com o padrão do construtor) |
| 0xFC4 | ZapAttack | +0x8..+0x24 → +0x1C..+0x38; +0x30/+0x34 → +0x3C/+0x40; +0x28/+0x2C → +0x44/+0x48; +0x38/+0x3C → +0x4C/+0x50 |
| 0xFC5 | GrappleHook | +0x8..+0x14 → +0x1C..+0x28; +0x18/+0x1C → +0x30/+0x34; +0x30 → +0x38; +0x28/+0x2C → +0x3C/+0x40; +0x20/+0x24 → +0x44/+0x48; +0x34..+0x4C → +0x68..+0x80 |
| 0xFC7 | AirStrike | +0x8..+0x20 → +0x1C..+0x34; +0x24 → +0x174; +0x28 → +0x178 |
| 0xFC8 | LavaBlast | +0x8..+0x18 → +0x1C..+0x2C; +0x2C → +0x30; +0x24/+0x28 → +0x34/+0x38; +0x1C/+0x20 → +0x3C/+0x40; +0x30..+0x48 → +0x190..+0x1A8 |
| 0xFCF e 0xFE0 | RobotSpecial | +0x8..+0x14 → +0x1C..+0x28; +0x18..+0x34 → +0x34..+0x50; +0x38 → +0x444 |
| 0xFDF e 0xFE1 | CrowdControl | +0x8..+0x24 → +0x1C..+0x38 |

CannonHands, TopSpin e UltraTazer não têm chave: ficam com o construtor.

## StateSonicRoar (Monster +0xF6F4): Congar

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0x20 | Congar |
| comando | `transitionOK` | condições comuns + círculo+triângulo (`PadEntry` +0x36) |
| fases (+0x54 = id da anim) | `transitionInto`/`update` | 0xA0 windup (+0x20 = 40 frames; `animationTransitionInto` com +0x1C = 96); a 0.99: `reset` da `HitHistory` (+0x58), `drainSpecial`, laço 0xA1, mira ligada (+0x6C3C = 1), `Cameras::StartShake` 20; laço: +0x2C = 180 frames (iterações em +0x25C, resto em +0x260); depois 0xA2 (+0x50 = 30 frames) e Idle a 0.99 |
| alcance | `update` (laço) | +0x264 cresce +0x30 (50) por quadro até +0x34 (500); na fase 6 com `craterSwitched` vale 1000 |
| cone | `transitionInto` | +0x38 = 41.44° de abertura total: +0x268 = cos(+0x38/2 em rad) = cos(20.7°), +0x26C = tan(20.7°) |
| forma do teste | `testConeCollis` (0x1CA760), todo quadro do laço | 1) linha de visada da boca (Monster +0x3E50) pelo eixo +0x33D0 × alcance (`hdLosCSTest`): destrutível (kind 3) acumula +0x40 (10)/`updateRate` por quadro na `HurtHistory` +0xA0 e, passando de 5, leva um `HitEvent` tipo 2 fonte 5 subtipo 0xC de 5.01; 2) `hdCsCollect` numa esfera centrada em boca + eixo × alcance/2, raio alcance/2, até 20 cs: cada interactive com distância² à boca ≤ alcance² e cos(ângulo ao eixo) ≥ +0x268 está no cone; 3) pedestres: `PedGroup::squishGlobal` em pontos ao longo do eixo (passo 1.25 × alcance × tan) |
| dano | `testConeCollis` | +0x3C = **15** por acerto (`HitEvent` tipo 2 fonte 5 subtipo 0xC, direção = −eixo, posição = trans do alvo); um acerto por alvo por rugido (`HitHistory::newHit`), exceto alvo **bloqueando** (fora de Recoil 0x1C): leva o evento todo quadro (o bloqueio absorve); +0x44 = 350 e +0x48 = 250 não são lidos no `testConeCollis` (provavelmente força de knockback passada por outro caminho; não confirmado) |
| efeito | — | derruba (tipo 2); `creditStaminaForAttack(0)` no primeiro acerto; interrompido por `handlePreemption` (mata fx +0x274/+0x278/+0x27C) |
| aceita golpe | `acceptHit` | no windup só tipo 2/5 ou fonte 5; no laço nada; no winddown tudo |

## StateFireBreath (Monster +0xF974): Togera e Raptros

O estado só dispara a animação e chama `Monster::breathFire` → `FireBreath::Activate` (objeto `FireBreath` em Monster
+0x68C0, atualizado por `TaskManager`); o sopro corre sozinho enquanto o monstro volta ao Idle.

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipos 0x60 e 0x120 | Togera, Raptros |
| comando | `transitionOK` | comuns (anim 0xA0 basta) + `FireBreath` inativo (Monster +0x68C0 == 0) + círculo+triângulo |
| janela | `update` | windup 0xA0 com +0x3C = 40 frames (transição +0x38 = 96); quando a animação para: laço 0xA1 (transição +0x40 = 96), `breathFire`, `drainSpecial`, um `AnimBlend` de mira (+0x54) e **Idle no mesmo quadro** |
| parâmetros do sopro (`breathFire` lê do estado) | `Activate(f12..f18)` → `FireBreath` +0x98.. | duração +0x1C = **180 frames** (→ FB +0x98, decrementada por campos); alcance +0x20 = **500** (→ +0x9C; 1000 na fase 6 com `craterSwitched`); dano direto a monstro +0x24 = **20 por segundo** (→ +0xA0, dividido por campos×60); dano a cenário/outros +0x28 = **40/s** (→ +0xA4, dividido pelo `updateRate`); quadros em chamas +0x30 = **240** (→ +0xA8); dano do fogo +0x2C = **9.6** (→ +0xAC); fator +0x34 = 0.5 (→ +0xB0, gravado em `MonsterDynamics` da vítima +0xB4 = Monster +0x1B4, lentidão) |
| forma | `FireBreath::TestCollis` (0x134678), todo quadro | **um** `hdLosCSTest` da boca (Monster +0x3E50) recuada 20 no eixo +0x33D0 até boca + eixo × alcance (sem raio); o primeiro contato decide |
| dano | `TestCollis` | monstro: `dispatchHit` de 20/s (`HitEvent` tipo 4 fonte 5, subtipo 0xF Togera / 0x10 Raptros), e **só se o dono é Raptros** `lightOnFire(240, 9.6, id)` (o Togera não incendeia); vibração no pad se a vítima é jogador; destrutível: `HurtHistory` (+0x8 do FireBreath) acumula 40/`updateRate` e a cada 5 acumulados dispara 5; outros: 40/`updateRate` |
| efeito | — | tipo 4 (fogo) não derruba; Raptros incendeia por 4 s a 9.6 (custo por quadro em `updateOnFire`) |
| aceita golpe | `acceptHit` | windup: tipo 2/5 ou fonte 5; depois do windup o monstro já está no Idle |

## StateBugAttack (Monster +0xF9D8): Preytor

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0xA0 | Preytor (também recebe Fly 0xF0B0) |
| comando | `transitionOK` | comuns + círculo+triângulo + `HomingBug::isAvailable(monstro)` (há um bug no pool `s_bugPool` deste dono com estado 0) |
| janela | `update` | windup 0xA0 (+0x20 = 30 frames): a pct ≥ +0x24 (0.5) pega a instância (`getInstance`), `spawn` e `drainSpecial`; a 0.99 laço 0xA1 (+0x2C = 5 frames): a pct ≥ +0x30 (0.75) `launch(bug, alvo travado se for monstro)`; a 0.99 winddown 0xA2 (+0x38 = 10 frames) → Idle |
| projétil | `HomingBug` (0xC0 bytes cada, `s_numBugs`) | `spawn` na boca (Monster +0x3E50), vida `s_initialHealth`; `launch`: velocidade inicial `s_initialVel`, estado 3 (seek); `updateSeek`: procura alvo (`findTarget`, cone `s_targetLockMinAngle`, alcance `s_targetLockMaxRange`), homing `s_homingFactor`/`s_maxForce`/`s_maxVel`, desvio por `hdSphereTest` com `s_feelerRadius` (10 resultados); bate em cenário → morre; chega a `s_attachRange` da cabeça do alvo (+0x3E60) → `attach` |
| dano | `updateAttached` | **`s_damagePerSecond`/60 por campo** via `Monster::takeDamage(vítima, dano, 0, dono)` (sem `HitEvent`, sem bloqueio) enquanto grudado; solta quando o acumulado (vida inicial − atual) passa `s_maxMonsterDamage`, quando o alvo fica com +0xF6 ≠ 0 ou depois de `s_attachFields` campos; o bug morre com golpes ≥ 0.1 que esgotem `s_initialHealth` (`takeHit`) |
| efeito | — | não derruba; vibração no pad da vítima jogador; vítima com +0x130 = 1 (fora dos chefes 0x1A0/0x1C0) |
| valores | `.CHR` chave 0xFAA | os estáticos acima; defaults do construtor zero (`.sbss`): ler na RAM pelos símbolos |

## StateRobotSpecial (Monster +0xFE5C): Robo-47

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0x40 | Robo-47 |
| comando | `transitionOK` | comuns + círculo+triângulo + `SpecFxAnim::isInstanceAvailable(monstro, 0x11FF)` (efeito da explosão livre) |
| fases (+0x300; anim em +0x2FC) | `enterSubState` | 0 windup (0xA0, +0x20 = 40 frames); 1 **mira** (anim 0xA6 = Monster +0x2750, retícula; blends +0x304/+0x36C seguem +0x6C40/+0x6C44); 2 **disparo** (laço 0xA1, `drainSpecial`, `setVulnerable(0)` até a anim acabar via `restoreVulnerability`); 3 winddown (0xA2, +0x38 = 30 frames) |
| janela | `update` | windup a 0.99 → mira; na mira, **soltar** o par de botões (+0x36 da entrada 0 == 0) com o contador de quadros do estado (+0x8) ≥ +0x3C (0) dispara; bloquear (`PadEntry` +0x34) cancela para o winddown; a trava da retícula (+0x440, soma campos enquanto `m_reticleState` +0x68A0 == 2 e o alvo não muda) chega a +0x40 (**180 campos** = 3 s) → míssil teleguiado (id do alvo = `m_pinTarget` +0x6C04); winddown a 0.99 → Idle |
| projétil | `Weapons::CreateStaticWeapon(0xD, dono, alvo, pos Monster +0x3F00, rot para a retícula +0x6BF8)` | `InitWeapon` caso 13 lê do estado: vida +0x48 (**10 s**), velocidade +0x4C (**700 mph**), homing +0x50 (0.5); `WeaponHD` varre esfera raio 0 de `m_prevPos` à posição |
| dano | `DetonateWeapon` caso 13 | o míssil **não** faz `dispatchHit` direto: ativa `ShockWave::Activate(pos, anéis em RobotSpecial +0x58, causa 5, dono, fx 0x11FF)`; `ExpandingRing`: nº em +0x58 (**6**), anel i em +0x78 + i×0x1C com (+4 dano, +8 filtro, +0xC/+0x10 forças de knockback → `HitEvent` +0x38/+0x34): **20**/44/600/900, **15**/44/490/804, **10**/44/390/500, **8**/44/300/330, **7**/44/290/300, **5**/6/110/110; alcances em +0x2A8..+0x2BC = 25, 100, 150, 200, 225, 250; `timeScale` (+0x2F8) recalculado na detonação; velocidade do fx +0x444 (1.0) |
| efeito | — | derruba (onda tipo 2); invulnerável durante o disparo |
| aceita golpe | `acceptHit` | fases 0 e 1: tipo 2/5 ou fonte 5; fase 3: tudo; fase 2: nada |

## StateRockSpecial (Monster +0x102A8): Agamo

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0x140 | Agamo |
| comando | `transitionOK` | **arremesso da cabeça**: comuns + cabeça não ativa (objeto +0x2E8, byte +0xC == 0) + círculo+triângulo **recém-apertados** (entrada 0 com +0x36 e entrada 1 sem) + +0x1C == 0 (não está no meio do especial); **detonação**: com a cabeça ativa (+0x2E8 +0xC == 1) e o Agamo **não** segurando-a (Monster +0x68A4 ≠ pickup da cabeça +0x2EC +0x14), um novo aperto do par dispara `ShockWave::Activate` no local da cabeça (objeto +0x2EC +0xC +0x10) com os anéis de +0x44, causa 5, solta o pickup (`LevelPickups::dropPickup`), apaga a cabeça e devolve 0 (não entra no estado) |
| fases (+0x2F0) | `update` | windup 0xA0 (+0x24 = 40 frames, transição +0x20 = 96): a 0.99 `drainSpecial`, laço 0xA1 (velocidade +0x2C = 1.0, transição +0x28), a cabeça vira o pickup segurado (`m_pickup` +0x68A4 = +0x2EC +0x14; `grabPickup`), objeto +0x2E8 +0xC = 1; a 0.99 winddown 0xA2 (+0x34 = 30 frames) → Idle ou estado de salto se no ar (+0x280) |
| cabeça | `transitionInto` | pavio da cabeça +0x2EC +0x180 = +0x38 (**4 s**) × 60 campos; o Agamo solta o que segurava (`dropPickup`); depois o jogador arremessa a cabeça como pickup (fluxo de arremesso comum, `Weapon` 0x17) e detona com o par de botões |
| dano | onda em `transitionOK` (detonação) | `ExpandingRing` em +0x44: nº +0x44 (**6**), anéis em +0x64 + i×0x1C (dano/filtro/knockback): **20**/60/600/900, **17**/50/480/804, **15**/40/390/500, **10**/40/300/330, **8**/30/290/300, **5**/20/0/0; alcances +0x294..+0x2A8 = 50, 100, 150, 200, 225, 250; `timeScale` +0x2E4 = 10. Getters: `getKnockBackUp` +0x3C (250), `getKnockBackOut` +0x40 (350), `getHeadDuration` +0x38 |
| efeito | — | derruba; a onda só existe se o jogador detonar (ou a cabeça explodir sozinha pelo pavio, caminho do pickup não lido aqui) |

## StateZapAttack (Monster +0x1059C): Kineticlops

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0xE0 | Kineticlops |
| comando | `transitionOK` | comuns + círculo+triângulo |
| fases (+0x54) | `update` | windup 0xA0 (+0x20 = 40 frames, transição +0x1C = 15): a 0.99 `drainSpecial`, laço 0xA1 (+0x2C = **120 frames**, iterações +0x58, resto +0x5C); fim → winddown 0xA2 (+0x48 = 30 frames) → Idle a 0.99 |
| alvo e alcance | laço | `Monster::getTarget(pos, 1)` (alvo da retícula); se for monstro: precisa de linha de visada de Monster +0x3E30 a +0x3E30 do alvo (`hdLosCSTest`) e distância Manhattan ≤ +0x34 (**1200**); senão o raio vai ao ponto da retícula e só acerta se o interactive for kind 5/6/7, com 1/30 de chance por quadro, dano fixo 6.0 (tipo 5 fonte 5) |
| forma | laço | arcos (`electricArc`, amplitude +0x4C = 15, +0x30 = 0.25) das mãos (localizadores 0x802/0x803 alternando) a localizadores aleatórios do alvo (0x802..0x805); **corrente**: cada outro monstro cujo localizador fica a ≤ +0x34 do localizador da vítima e com visada também é atingido |
| dano | laço, todo quadro | `dispatchHit` de +0x38 (**1.0**) ÷ (campos × 60) por campo, `HitEvent` **tipo 5** (choque) fonte 5 subtipo 0x13: o alvo entra em `Shocked` (Monster +0x10BA0) e perde vida por quadro pelo caminho do choque; vítima no chão recebe +0x1E8 += 250 (`MonsterDynamics`) e `KnockBack` +0x524 = atacante; ao **terminar** o laço: `Monster::knockBack(dir, +0x3C = 350, +0x40 = 250)` em todos os atingidos |
| efeito | — | atordoa/choca durante 2 s, derruba no fim; `creditStaminaForAttack(0)` por quadro |
| chefes | laço | contra o plantBoss (fase 6, `craterSwitched`) e o finalBoss (fase 11) usa pontos fixos e tipo 3 |

## StateGrappleHook (Monster +0x10600): Ultra-V

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0x80 | Ultra-V |
| comando | `transitionOK` | comuns + círculo+triângulo |
| fases (+0x4C) | `update` | windup 0xA0 (+0x20 = 40 frames): a **0.87** `drainSpecial`, laço 0xA1 (velocidade +0x28), corrente (`OgreMace` em +0x88) modo 1, ponta começa na mão; a direção parte da rotação do corpo com pitch +20° se o alvo está a > 1000 (2D), senão +5°; laço dura +0x2C frames = +0x30 ÷ (+0x80 × 0.0244) (alcance ÷ passo por quadro: 2500 ÷ 36.6 ≈ 68 frames; iterações +0x50, resto +0x54); fim ou acerto → winddown 0xA2 (+0x48 = 30 frames) → Idle a 0.99 |
| alvo | `transitionInto` | monstro travado (+0x6C04, kind 1) ou `getClosestMonsterToOrientation(+0x30 em rad… 2500)`; ponto alvo +0x84 (= alvo +0x3E20 ou retícula) |
| forma | `testCollis` (0x1C78E8), todo quadro do laço | `hdSphereTest` **raio 0** de +0x90 (ponta no quadro anterior) a +0xA0 (ponta agora), 1 resultado; homing da ponta (`updateHoming`): yaw/pitch (+0xC0/+0xC8) vão ao alvo com taxa +0x7C (0.1) quando o alvo está a > 1000 (2D), senão 0.5; velocidade +0xB4 sobe até +0x80 (**1500 mph** × 0.0244 unidades/quadro) com taxa +0x78 (0.1) |
| dano | `testCollis` | +0x34 (**15**) + dano do pickup segurado (`LevelPickups::s_info[tipo]` +0x10 se gancho em monstro, +0x14 senão; o pickup é consumido e o jogador ganha 10 tokens "Pickup Use"); `HitEvent` tipo 3 (7 para alguns ids de cenário) fonte 5 subtipo 0x12; monstro atingido e no chão: +0x1E8 += +0x70 (**500**), +0x64 = 1 (fisgado) |
| efeito | — | **puxa** a vítima: no winddown a posição dela é levada à do Ultra-V com taxa +0x68 (0.08) por quadro (z para z+150 se estiver abaixo, taxa +0x6C 0.01); sem acerto a corrente recolhe para a mão (localizador 0x803, Monster +0x3DD0); +0x3C/+0x40 (350/250) e +0x44 (5) não lidos aqui |
| treino hoje | `NATIVO.md` | o treino já desenha a linha do gancho (commit c76f3a1) |

## StateAirStrike (Monster +0x10790): Zorgulon

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0x160 | Zorgulon |
| comando | `transitionOK` | comuns + **sinalizador não ativo** (objeto +0x184, byte +0xC == 0) + círculo+triângulo |
| fases (+0x38) | `update` | windup 0xA0 (+0x20 = 40 frames): a 0.99 alvo = `getTarget(+0x190, 1)` (com visada de +0x3E30 ao alvo; sem visada o alvo cai e fica o ponto; sem alvo, Monster +0x6C60), laço 0xA1 (+0x2C = **200 frames**), `drainSpecial`, sinalizador ativo partindo de Monster +0x3F20, tarefa `flyBeacon`; a 0.99 winddown 0xA2 (+0x34 = 30 frames) e **chamada dos discos**; Idle a 0.99 |
| sinalizador | `flyBeacon` (tarefa) | voa suavizado (taxa +0x180 sobe até 0.1) para o alvo (monstro +0x3E60 + 50 em z, ou o ponto; chefes têm pontos próprios); `hdSphereTest` raio 0 do passo: bateu → vira ponto fixo (+0x188 = 0, +0x190 = contato); se o jogador saiu do estado antes dos discos, apaga-se |
| discos | `update` (fim do laço) | 5 `UfoPickup` de `alienSaucers[5 × m_dupId .. +5]`, só se o primeiro está no estado 5 (livre): cada um solta o que carregava, é posto a ±500 do alvo (ou ±250 do ponto) a 1500..2000 de altura, `UfoNav::init`, duração +0x178 (**600 campos** = 10 s; ×3 sem alvo monstro), `enterState(1)`, alvo = +0x188 |
| dano | `UfoPickup::fire` → `CreateStaticWeapon` tipo 0x24 ou **0x26** | o disco atira; o tipo 0x26 lê o dano de `TheGame` +0x1204AC, que o AirStrike escreve com +0x174 (**2.0** por tiro); vida 2 s, 1200 mph, homing 0.07 (`m_params[38]`); explosão = `dispatchHit` tipo 3 fonte 0xC |
| efeito | — | tiros fracos e muitos, por 10 s; não derruba por si |

## StateLavaBlast (Monster +0x10930): Magmo

| campo | onde | valor ou offset |
|---|---|---|
| quem | `initDynamics` tipo 0x100 | Magmo |
| comando | `transitionOK` | comuns + círculo+triângulo |
| fases (+0x48) | `enterSubState`/`update` | 0 windup (0xA0, +0x20 = 40 frames): a > 0.99 → 1 **disparo** (`drainSpecial`, blends de mira); 2 volta da mira; 3 winddown (0xA2, +0x40 = 30 frames) → Idle |
| alvo | `transitionInto` | monstro travado (+0x6C04) ou `getClosestMonsterToOrientation(15°, 5000)`; ponto alvo +0x1AC (alvo +0x3E30 ou retícula); id em +0x188 |
| disparo | `update` fase 1 | a cada quadro, com `mathfRand(0,15) == 0` (1/16), uma bola: direção da boca (Monster +0x3E50) à retícula com espalhamento aleatório (alvo a > 1500 em 2D: yaw ±45°, pitch 15..45°; perto: yaw ±15°, pitch 5..20°); `CreateStaticWeapon(0x15, dono, alvo, boca, rot)`; conta em +0x18C até +0x190 (**10 bolas**) e passa à fase 2 |
| projétil | `InitWeapon` caso 0x15 | dano = LavaBlast +0x19C (**30**), velocidade +0x1A0 (**1500 mph**), homing `m_params[21]` 0.02 (horizontal +0x194 = 0.01 e vertical +0x198 = 0.1 têm getters); `WeaponHD`: esfera varrida raio 15; `DetonateWeapon` 0x15: `HitEvent` fonte 5 **tipo 2** e `dispatchHit` de 30 no atingido |
| fogo | armas 0x27..0x29 (`Detonate`) | quando o dono é Magmo (tipo 0x100), `lightOnFire(+0x1A8, +0x1A4, id)`: +0x1A4 = **3.0**, +0x1A8 = 1500 campos no construtor (o `.CHR` deve baixar; conferir na RAM) |
| efeito | — | derruba a cada bola que acerta (tipo 2); 10 bolas em ~2.7 s em média |

## StateCrowdControl (Monster +0xF3F0): todos, segundo especial

| campo | onde | valor ou offset |
|---|---|---|
| quem | `transitionOK` | qualquer monstro com as anims 0xA3/0xA4/0xA5 (Monster +0x2720/+0x2730/+0x2740) e com a anim do efeito (+0x2E8 = `animationGetHandle(m_typeBits + 3, m_dupId)`) parada; o Robo-47 (tipo índice 2, Monster +0x18) só quando o estado corrente da IA (ponteiro em Monster +0x7C0) é o de offset +0x730 dessa IA |
| comando | `transitionOK` | `m_attacksEnabled`, `!isHoldingLarge`, `isSpecialAvailable` (**gasta o medidor**) + **quadrado + X** (`PadEntry` +0x38) |
| fases (+0x2E0) | `transitionInto`/`update` | 0xA3 windup (+0x20 = 40 frames; variantes no ar 0xC0/0xC1 se existirem em +0x28F0/+0x2900); a 0.99 → 0xA4; em 0xA4 a pct ≥ +0x38 (**0.6**) `ShockWave::Activate(pos do corpo, anéis de +0x3C, causa 6, dono, anim +0x2E8, cs +0x2FC)` e `drainSpecial`; a 0.99 → 0xA5 (transição +0x30 = 96) → Idle (ou estado de salto se no ar) quando a anim do efeito passa de +0x300 |
| invulnerável | `transitionInto` | Monster +0x49 = 0 durante, volta a 1 no fim e no `handlePreemption` |
| dano | anéis em +0x3C | nº +0x3C (**5**), anel i em +0x5C + i×0x1C (dano/filtro/knockback): **10**/40/350/380, **9**/30/350/375, **8**/30/350/250, **5**/20/250/250, **2**/0/0/0; alcances +0x28C..+0x29C = 50, 100, 200, 250, **300** (`getMaxRange` = último); `timeScale` +0x2DC = 10 |
| efeito | — | onda em volta do corpo que derruba quem está perto (até 300) |

## Estados de chefe (sem botão ou só IA)

### StateCannonHands (Monster +0x10F60): Goliath Prime (assboss)
- `transitionOK`: `m_attacksEnabled`, anims 0xA0..0xA2, `isSpecialAvailable`, +0x36 (par de botões, se humano) e **Monster +0x68B4 ≠ 0** (`m_target`).
- Fases (+0x58): 0 windup (0xA0, +0x20 = 40 frames); 1 **mira e tiro** (anim 0xA6, cabeça vira para o jogador 0): contador +0x50 = 360 campos; a cada ciclo de 20 campos (+0x48) atira no campo 14 do localizador Monster +0x3F10 e no 6 do +0x3F00: `CreateStaticWeapon(0xA, dono, jogador 0, loc, rot para a retícula + mira)`; 2 winddown (0xA2, +0x38 = 30 frames) → Idle.
- Dano: arma tipo 10 = projétil padrão do monstro (`InitWeapon` casos 0..11/20: dano Monster **+0x69AC** `m_dpDamage`, duração +0x69B0 × 60, velocidade fixa 1000, homing +0x69B8). 36 tiros em 6 s, só contra o jogador 0. Não gasta `drainSpecial` (não vi a chamada): confiança média.

### StateTopSpin (Monster +0x10EBC): Goliath Prime (assboss)
- `transitionOK`: só exige as anims 0xA3..0xA5 (entrada pela IA, `Ai::setAssBossState`).
- Fases (+0x4C): 0xA3 windup (+0x20 = 150 frames); 0xA4 laço (+0x2C = 900 frames); 0xA5 winddown (+0x48 = 150 frames).
- Teste: `hdSphereTest` do localizador 0x802 (Monster +0x3DC0) ao 0x803 (+0x3DD0) com **raio 25**, até 10 resultados, todo quadro do laço; `HitHistory` em +0x50.
- Dano: jogador 0 atingido → `HitEvent` tipo 2 fonte 0xE com forças +0x3C = 600 / +0x40 = 400, `dispatchHit` +0x38 (**15**); se o jogador está no ButtSlam (0xFA1C) a < 50 (2D) o chefe é interrompido (IA 5, subestado 0x4B); se o jogador bloqueia, o chefe entra em Recoil (10, 45). Cenário: tipo 2 fonte 0xE, dano 5.1.

### StateUltraTazer (Monster +0x11130): jelly e final
- `transitionOK`: anims 0xA0..0xA2, não estar em Recoil (0x7E30), `isSpecialAvailable`; sem botão.
- Fases (+0x40): 0xA0 windup (+0x20 = 60 frames); a 0.99 laço 0xA1 (transição 200) e `drainSpecial`; contador +0x2C = **180 campos**; depois winddown 0xA2 (+0x3C = 60 frames) com `knockBack(jogador 0, dir, 300, 500)`; Idle a 0.99 ou se o jogador 0 morreu.
- Dano: todo quadro do laço, arcos da boca (Monster +0x3E50) ao jogador 0 (cabeça +0x3E60 e localizadores aleatórios 0x802..0x809), `HitEvent` **tipo 5** fonte 5 e `dispatchHit` de **0.15** por quadro (9/s) + choque; jogador no chão fora do KnockBack: +0x1E8 += 250. Só contra o jogador 0.

## Golpes que não são de medidor (ficam para a lista dos comuns)

Entram por `curMappedAction` (Monster +0x6754) ou por animação, sem `isSpecialAvailable`:
- **StateJavelin** (+0xFE38): anim 54 (Monster +0x2050) e círculo recém-apertado (`PadEntry` +0x2E nas entradas 0 e 1) segurando pickup tipo 7 (`m_pickup` +0x68A4 → info +0xA0 == 7). Dano pelo arremesso (`Weapon` 0x17, `useDamage`/impalar).
- **StateStompAttack** (+0x10CF0): `curMappedAction` 0 ou 5; `transitionFeasible`: no Idle (id 3) ou estado 0x29, sem `m_target`, sem pickup ou pickup tipo 6, anim 240 (Monster +0x2BF0), `StaminaMeter::hasEnough(+0x20)`, vítima por `getClosestMonsterToPunch(+0x30, +0x2C, +0x30)` **caída** (flag 0x10 do estado dela e +0x260 == 2/3) com |Δz| ≤ 50. Dano direto: ver `treino_hitboxes.md` (+0x3C, pct +0x34/+0x38).
- **StateTazerAttack** (+0x10D50) e **StateGunAttack** (+0x10E90): só exigem a anim 244 (Monster +0x2C30); criam `Weapon` (`launchProjectile`/`CreateStaticWeapon`).
- **StateShieldAttack** (+0x10D90): `m_attacksEnabled`, pickup tipo 0x10 segurado, `curMappedAction` 5 ou 0, `transitionFeasible` = anim 245 (Monster +0x2C40).

## Tabela-resumo

| estado (offset) | monstro | comando | dano (como obter) | janela | alcance | efeito | confiança |
|---|---|---|---|---|---|---|---|
| SonicRoar 0xF6F4 | Congar | ○+△ com medidor | +0x3C (15) por alvo; cenário +0x40/s | windup 40 fr, laço 180 fr (+0x2C), winddown 30 | cone +0x38 (41.4°), alcance +0x264 até +0x34 (500) | derruba; bloqueado leva todo quadro | alta |
| FireBreath 0xF974 | Togera, Raptros | ○+△ com medidor | +0x24/s (20) a monstro; +0x28/s (40) a cenário; Raptros incendeia +0x30 fr a +0x2C | windup 40 fr; sopro +0x1C (180 fr) autônomo | linha de visada +0x20 (500) da boca | fogo (tipo 4), lentidão +0x34 | alta |
| BugAttack 0xF9D8 | Preytor | ○+△ com medidor, bug livre | `s_damagePerSecond`/60 por campo via `takeDamage` até `s_maxMonsterDamage`/`s_attachFields` | windup 30 fr (solta a 0.5), laço 5 fr (lança a 0.75), winddown 10 | perseguição `s_targetLockMaxRange`, gruda a `s_attachRange` | não derruba, dano contínuo | média (valores só na RAM) |
| RobotSpecial 0xFE5C | Robo-47 | ○+△ (segura: mira; solta: dispara; bloquear cancela) | onda de 6 anéis em +0x58: 20/15/10/8/7/5, alcances 25..250 | windup 40 fr; mira livre; trava 180 campos | míssil 700 mph × 10 s | derruba; invulnerável no disparo | alta |
| RockSpecial 0x102A8 | Agamo | ○+△ (arremessa a cabeça); ○+△ de novo detona | onda de 6 anéis em +0x44: 20/17/15/10/8/5, alcances 50..250 | windup 40 fr; cabeça vive +0x38 (4 s) | onde a cabeça caiu | derruba | alta |
| ZapAttack 0x1059C | Kineticlops | ○+△ com medidor | +0x38 (1.0)/s direto + choque (tipo 5); knockBack 350/250 no fim | windup 40 fr, laço +0x2C (120 fr), winddown 30 | visada e Manhattan ≤ +0x34 (1200); corrente a outros ≤ 1200 | choca 2 s, derruba no fim | alta |
| GrappleHook 0x10600 | Ultra-V | ○+△ com medidor | +0x34 (15) + dano do pickup segurado | windup 40 fr (dispara a 0.87); voo ≈ +0x30/(+0x80×0.0244) fr | +0x30 (2500), ponta raio 0, homing +0x7C | fisga e puxa (+0x68) | alta |
| AirStrike 0x10790 | Zorgulon | ○+△ com medidor, sinalizador livre | tiros dos discos: +0x174 (2.0) cada (arma 0x26 via TheGame +0x1204AC) | windup 40 fr, laço 200 fr, discos +0x178 (600 campos) | sinalizador até o alvo; discos a ±500 | muitos tiros fracos | média (arma do disco inferida) |
| LavaBlast 0x10930 | Magmo | ○+△ com medidor | +0x19C (30) por bola, 10 bolas (+0x190); fogo +0x1A4/+0x1A8 pelas armas 0x27..0x29 | windup 40 fr; 1/16 por quadro até 10 bolas; winddown 30 | 1500 mph (+0x1A0), espalhamento por distância | derruba por bola | alta |
| CrowdControl 0xF3F0 | todos (Robo-47 só num estado da IA) | □+✕ com medidor | onda de 5 anéis em +0x3C: 10/9/8/5/2, alcances 50..300 | windup 40 fr; onda a 0.6 do laço | 300 (+0x29C) | derruba em volta; invulnerável | alta |
| CannonHands 0x10F60 | Goliath Prime | IA (○+△ se humano) + `m_target` | arma 10: Monster +0x69AC por tiro, 36 tiros | 360 campos | projétil 1000 mph | tiros teleguiados no jogador 0 | média |
| TopSpin 0x10EBC | Goliath Prime | IA | +0x38 (15), knockback 600/400 | windup 150 fr, laço 900, winddown 150 | esfera raio 25 entre as mãos | derruba; ButtSlam perto interrompe | alta |
| UltraTazer 0x11130 | jelly, final | IA | 0.15 por quadro (tipo 5) + knockBack 300/500 no fim | windup 60, laço 180 campos, winddown 60 | sempre no jogador 0 | choque contínuo, derruba no fim | alta |
| Javelin, StompAttack, TazerAttack, ShieldAttack, GunAttack | vários | ação mapeada / pickup / anim | ver lista dos comuns | — | — | — | — |

## Em aberto

- Os campos +0x44/+0x48 do SonicRoar (350/250) e +0x3C/+0x40/+0x44 do GrappleHook não são lidos nas funções do estado: podem
  ser forças de knockback consumidas pelo `HitEvent`/`takeHit` (não confirmado).
- Onda de choque: o significado de +0x0 e +0x8 de cada `ShockWaveConfig` (filtro de altura?) vem de `ShockWave::update`
  (0x1B0BD8), não lido aqui; o dano por anel está em +0x4 (confirmado pelo `StateButtSlam::handleCollis` decompilado).
- Valores do `.CHR` por monstro: o treino deve ler da RAM (offsets acima); para uma tabela estática falta um
  `tools/chr_dump.py` com as chaves 0xFA9/0xFAA/0xFAB/0xFAD/0xFBC/0xFC4/0xFC5/0xFC7/0xFC8/0xFCF/0xFE0/0xFDF/0xFE1.
- O par de botões foi lido só na config 0 (`updateInput0ConfigCR`); as outras configs de controle não foram conferidas.
