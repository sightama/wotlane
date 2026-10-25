# Modo treino: números dos golpes comuns a todos os monstros (2026-10-25)

Investigação só de leitura (decomp em `src/game/*.cpp` quando existe, asm quando não; nada rodado). Complementa
`635fd3c:docs/investigacoes/2026-10-23/treino_hitboxes.md` (socos) e `docs/FORMATOS.md` "Personagem".
Offsets "Monster +X" são relativos ao objeto `Monster` (0x11190 bytes); os estados ficam embutidos nos offsets da
tabela `kStates` de `native/runtime/wotm_training.inc`. Tudo o que vem do `.CHR` já está na RAM dentro do estado, então o
treino lê de lá; a chave do `.CHR` fica anotada só para referência (o `charreset::readKey` grava direto nesses offsets).

## 0. Como ler o comando (esquema de controle 0, relativo ao monstro)

`PadFlags::updateInput0ConfigCR` (asm, `asm/game/PadFlags_2.s`) converte os bytes do `GamePad` (Monster +0x5024) no
`PadEntry` do quadro (anel em `PadFlags` = Monster +0x5040; `[0]` = quadro atual, `[1]` = anterior) e
`updateActions0` gera o `curMappedAction` (Monster +0x6754). Contadores de "segurando há N quadros" por botão em
`PadFlags` +0x1612 quadrado, +0x1614 bola, +0x1616 triângulo, +0x1618 X, +0x1658 L1, +0x165A L2, +0x165E R1,
+0x1660 R2, +0x1668/6A/6C/6E cima/baixo/esquerda/direita; "toque recente" = contador ≤ `PadFlags` +0x16C4.

| byte do `GamePad` | botão | campo do `PadEntry` |
|---|---|---|
| 0 / 1 / 3 / 4 | L1 / L2 / R1 / R2 | 0x50 / 0x52 / 0x56 / 0x58 (crus) |
| 6 | X | 0x10 (`face[3]`), 0x2A (`jump`) |
| 7 | bola (O) | 0xC (`face[1]`), 0x2E (`action`) |
| 8 | triângulo | 0xE (`face[2]`), 0x2C (`f2C`) |
| 9 | quadrado | 0xA (`face[0]`), 0x30 |
| 0xC/0xD/0xE/0xF | esquerda/direita/cima/baixo | 0x6/0x8/0x2/0x4 (e 0x16/0x18/0x12/0x14) |

Campos derivados: **0x34 defesa** = R2 segurado (contador +0x1660 > 0); **0x46 contra-ataque** = defesa + toque recente
de quadrado ou triângulo; **0x36** = triângulo + bola juntos (recentes), **0x38** = quadrado + X juntos (recentes); os dois
anulam a ação mapeada (`setCurrentAction(20, 15)`); **0x3E corrida (dash)** = `dashDoubleTap` (dois toques para a
frente) e continua enquanto o analógico para a frente passa de 0xE1; **0x40 investida** = 0x3E e triângulo (zera o 0x2C);
**0x44 pisão (butt slam)** = cima apertado agora, solto há `+0x16C4`+1 quadros (toque recente de cima) e triângulo.

`curMappedAction` (toque recente do botão; a direção é um toque recente no direcional):

| botão | sozinho | + cima | + baixo | + esquerda | + direita |
|---|---|---|---|---|---|
| quadrado | 0 | 1 | 2 | 3 | 4 |
| triângulo | 5 | 6 | 7 | 8 | 9 |
| bola | 0xA | 0xB | 0xC | 0xD | 0xE |

(15 = nenhuma; `MAPPED_ACTION_NONE` do header é 0, mas 0 é "quadrado" nos estados: eles testam `== 0` como soco fraco.)

Stamina: todo `AttackState` tem em +0x14/+0x18 (do estado) o ganho ao acertar: `creditStaminaForAttack(f)` =
`Monster::creditStamina(+0x14 + +0x18 * f)`; quase sempre é chamado com `f = 0`, então o ganho é o float em
estado +0x14. `StaminaMeter::drain(x, ...)` tira `x` (medidor em Monster +0x1FE0.. `m_stamina`, `cur` = +0x10 do medidor).

## 1. ButtSlam (Monster +0xFA1C), `src/game/MovementStates.cpp`

| campo | onde | valor ou offset |
|---|---|---|
| comando | `transitionOK` + `transitionFeasible` | `PadEntry[0]` 0x44 (cima recente + triângulo); só no estado do ar (`m_state == m_stateRef`, pulo/voo), não assboss, anims 0x105 e 0x106, `attacksEnabled`, stamina ≥ custo, sem alvo agarrado, sem objeto de duas mãos, não subindo mais que `maxRiseSpeed` (+0x1C = 0xFA38), não mais que `maxDropFromApex` (+0x20 = 0xFA3C) abaixo do ápice, altura sobre a sombra ≥ `minHeight` (+0x24 = 0xFA40) ou sobre um hat 0x20..0x400 |
| custo de stamina | `transitionInto` | float em +0x4C (0xFA68), `drain(x, true, false)` |
| ganho de stamina | `handleCollis` | `creditStaminaForAttack(0)` = +0x14 (0xFA30) |
| janela | `update`/`handleCollis` | não é fração: o impacto é o **primeiro toque no chão** depois do empurrão (`boostPending` 0 em +0x310 = 0xFD2C); empurrão em `boostPercent` +0x30 (0xFA4C) da anim 0x105; relançamento em `launchPercent` +0x3C (0xFA58) da anim 0x106 |
| alcance direto | `handleCollis` | `getClosestMonster(reach[0])`: monstro a menos de `shock.reach[0]` = +0x68+0x250 = **0xFCD4** leva o anel 0 direto (`takeHit`, HitEvent tipo 2, source 7) |
| dano direto | idem | `shock.rings[0].damage` = +0x68+0x20+0x4 = **0xFAA8**; knockback `rings[0]` +0xC/+0x10 (0xFAB0/0xFAB4) → HitEvent +0x38/+0x34 |
| onda | `ShockWave::Activate(trans, shock, causa 1, id, 0)` | config copiada de `shock` (+0x68 = 0xFA84, 0x2A4 bytes): nº de anéis 0xFA84, anéis em 0xFAA4 (stride 0x1C: +0 filtro de altura, +4 dano, +0xC/+0x10 knockback, +0x14 atordoamento, +0x18 → HitEvent +0x4C), `reach[i]` em 0xFCD4 + 4i, `timeScale` 0xFD24 |
| onda: raio e anel | `ShockWave::update` (asm, `asm/game/ShockWave.s`; pool `ShockWave::m_wavePool` 0x417F90, 10 × 0x330: +4 idade, +0x10 centro, +0x20 dono, +0x84 ExpandingRing) | raio = `timeScale` × idade (campos), esfera estática `hdSphereTest(centro, raio, 100 resultados)`; a onda morre quando o raio passa `reach[n-1]`; cada alvo novo (HitHistory) recebe o anel `i` em que `reach[i-1] < dist ≤ reach[i]` (distância real do centro da onda ao alvo: a raiz sai de um `sqrt.s` codificado como `.word`, por isso não aparece como instrução); só alvos com |Δz| < `rings[0].f0`; dano = `rings[i].damage` (anel +4) pelo `takeHit` virtual; `isBlocking` do alvo é consultado |
| efeito | `handleCollis` | derruba (tipo 2); `rumble`, partículas; hats 10001/10025..27/10075 disparam eventos de fase |
| `.CHR` | `readKey` | chave 0xFDF (ou 0xFB9, layout antigo): +0x1C..+0x48 do estado, nº de anéis, `timeScale` e os anéis |

## 2. RamAttack (Monster +0xFD40), `MovementStates.cpp`

| campo | onde | valor ou offset |
|---|---|---|
| comando | `transitionOK` | `PadEntry[0]` 0x40 (corrida 0x3E + triângulo); `attacksEnabled`, anim 0x10B, pés no chão (`m_freeFalling` 0), stamina ≥ custo, sem objeto de duas mãos. (A corrida 0x3E é o `StateDash` 0x7A90, dois toques para a frente; `StateDash::wantRam` +0x40 marca que a investida ficou possível) |
| custo de stamina | `enterSubState(CHARGE)` | +0x48 (0xFD88) |
| ganho | `handleCollis` ao acertar monstro | `creditStaminaForAttack(0)` = +0x14 (0xFD54) |
| janela | `update` | subestado CHARGE (+0x5C = 0xFD9C, valor 0) por `duration` +0x38 (0xFD78) campos, enquanto segura triângulo (0x2C) ou até 6 quadros caindo; depois COAST (1), IMPACT (2, anim 0x10B), DONE (3) |
| alcance | `handleCollis` | **o corpo**: o feixe de esferas do movimento (`hdBundledSphereTest` do `Monster`) tocando um interactive; parede só de frente (normal · frente < -0,7071); kind 5 sempre |
| dano | `handleCollis` | `hitDamage` +0x40 (0xFD80) + |vel · normal| × `hitDamagePerSpeed` +0x44 (0xFD84); velocidade em unidades/s (Monster +0x1E0); zero se a vítima está num soco tipo 0/2/5 dentro da janela ou também investindo (os dois vão para IMPACT) |
| efeito | idem | HitEvent tipo 2 (derruba), source 9; um alvo por investida (HitHistory +0x64); velocidade de carga `maxSpeed` +0x34 (0xFD74; `setMaxSpeedMPH` grava mph em +0x30 e recalcula); ao impactar a velocidade cai a 1/4 |
| `.CHR` | `readKey` | chave 0xFB4: +0x1C..+0x58 |

## 3. Agarrão: StateGrapple (+0xDCE0) → GrappleLift (+0xDD54) → golpes/arremessos

### 3a. Pegar (StateGrapple +0xDCE0, `src/game/Grapple.cpp`)

| campo | onde | valor ou offset |
|---|---|---|
| comando | `transitionOK` | `curMappedAction == 0xA` (bola sozinha); não tipo 0x1C0, não modo de jogo 9 |
| condições | `transitionFeasible` | `attacksEnabled`, anim 0xAA, sem alvo, sem objeto; vítima = `getClosestMonsterInFOV(from, feasibleDist, eye, cos(fov/2), dist)`; vítima não boss (0x1A0/0x1C0/0x1E0), com linha de visada (`EnemyInfo`) e `StateGrappled::transitionOK(id)` dela aceita |
| cone | idem | `from` = trans + cima × `eyeUp` (+0x18 = 0xDCF8) + frente × (`reach` +0x14 (0xDCF4) + Monster +0x3F4); `eye` = trans + cima × eyeUp + frente × `eyeBack` (+0x20 = 0xDD00, -50); ângulo `fov` +0x1C (0xDCFC, graus); alcance `feasibleDist` +0x24 (0xDD04) no teste de entrada, `attachDist` +0x38 (0xDD18) durante a animação |
| janela | `update` | fração da anim 0xAA entre `attachStart` +0x30 (0xDD10) e `attachEnd` +0x34 (0xDD14), fora de transição; passou sem pegar → anim de erro 0xB9 com ataques desligados pela duração dela |
| recusa da vítima | `StateGrappled::transitionOK` (+0xDDCC) | não pega quem acabou de bater nela (estado de reação), nem se o agarrador tem stamina ≤ `minGrapplerStamina` (Grappled +0x18 = 0xDDE4); soco/golpe com objeto que ainda não acertou e tem knockback ou atordoamento > 100 **pode** ser pego; estado 0x26 sempre; senão não pega quem já está agarrado/agarrando ou com `m_unk49` 0 |
| dano | — | nenhum ao pegar |
| `.CHR` | `readKey` | 0xFDA (Grapple +0x14..+0x58), 0xFA3 (Grappled +0x14..+0x48), 0xFCB (Lift +0x14/+0x18) |

### 3b. Segurando (StateGrappleLift +0xDD54, `GrappleThrow.cpp`) e a vítima (StateGrappled +0xDDCC)

- Lift: anim 0xAB a `speed` +0x18 (0xDD6C); `queued` +0x1C (0xDD70) = 0xF enquanto nada; jogador 1 enfileira a ação 0xA ou 0xB
  lida durante a animação; no fim (99%) vai para GrappleThrow (0xA) ou GrappleThrow1 (0xB), senão Idle. Entrada no Lift pelo
  `transitionOK`: segurando alguém e toque novo de bola (`PadEntry` 0x2E agora, 0 antes).
- Vítima: cada toque novo de qualquer face button drena `struggleCost` (Grappled +0x24 = 0xDDF0) da stamina do agarrador; ela
  se solta (`breakFree`) quando a stamina dele cai a `minGrapplerStamina` (+0x18) ou ele levou `breakDamage` (+0x1C = 0xDDE8)
  desde o agarrão; é largada (`release(false)`) quando ela mesma levou `releaseDamage` (+0x20 = 0xDDEC). `struggleDisable`
  (+0x50 = 0xDE1C) são campos em que o esperneio não conta (zerado a cada golpe do agarrador).

### 3c. Golpes no agarrão: GrappleOHAttack (+0x10AE0) e MonkeyOHAttack (Congar, +0x10B34), `GrappleOHAttack.cpp`

| campo | onde | GrappleOHAttack (base 0x10AE0) | MonkeyOHAttack (base 0x10B34) |
|---|---|---|---|
| comando | `transitionOK` | `curMappedAction` 0 (quadrado) = golpe normal anim 0xB1; 5 (triângulo) = especial anim 0xBA | igual; normal sorteia 0xB1/0xB2 |
| condições | `transitionFeasible` | `attacksEnabled`, alvo agarrado em `isOverhead` (subestado < 3), stamina ≥ custo; o normal vira especial sozinho quando a vítima já perdeu ≥ `damage` desde o agarrão (`healthDeltaAchieved`) | igual |
| custo | `transitionInto` | normal `cost` +0x2C (**0x10B0C**), especial `specialCost` +0x40 (**0x10B20**) | +0x2C (**0x10B60**), +0x40 (**0x10B74**); golpe emendado paga `cost` de novo |
| janela (ponto do golpe) | `update` | `hitPct` +0x24 (**0x10B04**) da anim 0xB1; `specialHitPct` +0x38 (**0x10B18**) da 0xBA; um disparo (`hit` +0x48) | +0x24 (**0x10B58**), +0x38 (**0x10B6C**); percentual + `pctOffset` +0x54 |
| dano | `update` → `takeDamage(dmg, false, owner)` direto | normal `damage` +0x28 (**0x10B08**), especial `specialDamage` +0x3C (**0x10B1C**) | +0x28 (**0x10B5C**), +0x3C (**0x10B70**) |
| alcance | — | sem geometria: a vítima é `m_target` | idem |
| efeito | `update` | normal: vítima no subestado 2 do Grappled (anim 0xD2) e `struggleDisable` = +0x44 (0x10B24) campos sem esperneio; especial: `StateGrapple::release(true)` (solta com `throwFwd`/`throwUp` do Grapple +0x44/+0x48 = 0xDD24/0xDD28) para o GrappleBreak (0xE990; o impacto no chão dá dano pelo `StateThrowBack::handleDamage`, ver 3d); `creditStaminaForAttack(0)` (= +0x14) e `registerComboHit` | igual; emenda: toque novo de triângulo (`PadEntry` 0x2C) entre `chainPct` +0x48 (0x10B7C) e `endPct` +0x4C (0x10B80) e stamina ≥ `cost` |
| defaults do construtor | header do `.cpp` | 100/180/0,5/6/3 e especial 50/40/0,5/10/10 quando zerado | 100/180/0,5/6/3, chain 0,5..1,0 |

Observação: o cabeçalho de `GrappleOHAttack.cpp` diz "Monster+0xDE68" para o GrappleOHAttack; `game.h`, a `kStates` e os
`enterNewState` dos outros estados usam 0x10AE0. Confio no 0x10AE0 (o treino já identifica o estado por ele).

### 3d. Arremessos do agarrão: GrappleThrow (+0xDD74, bola) e GrappleThrow1 (+0xDDA0, cima + bola)

| campo | onde | GrappleThrow (0xDD74) | GrappleThrow1 (0xDDA0) |
|---|---|---|---|
| comando | `transitionOK` | `curMappedAction` 0xA (bola) com alvo, anim 0x37 | 0xB (cima + bola), anim 0x38 |
| janela | `update` | `releasePct` +0x20 (**0xDD94**) da anim 0x37 | +0x20 (**0xDDC0**) da 0x38 |
| trajetória | `throwToTarget` / `update` | balística até o alvo da mira (`getTarget`): velocidade horizontal `speed` +0x28 (0xDD9C), tempo `s_tMod × dist / speed`, clamp em `maxSpeed` +0x24 (0xDD98) | velocidade do agarrador + frente × `fwd` +0x28 (0xDDC8) + cima × `up` +0x24 (0xDDC4) |
| dano | `StateThrowBack::handleDamage` da **vítima** (estado BeingThrown +0xE3E0, `StateThrowBack.cpp`) | velocidade `v` = normal · velocidade do ponto de contato do corpo rígido. Primeiro toque no chão: `groundDamageBase` +0x50 (**0xE430**) + v × `groundDamageScale` +0x54 (**0xE434**); primeiro toque em parede/objeto: +0x20 (**0xE400**) + v × +0x24 (**0xE404**); cada monstro novo atingido: +0x6C (**0xE44C**) + v × +0x70 (**0xE450**); + `fallBonus` +0x84 (0xE464) se caiu mais que `bigFallHeight` +0x88 (0xE468) do ponto mais alto. O que ela acerta também leva: objeto `objectDamage` +0x7C/+0x80 (10 se 0); monstro leva o mesmo `dmg` com o arremessador como atacante | igual (mesmo estado da vítima) |
| efeito | idem | vítima em `StateBeingThrown` (reação; `throwSource` +0x524 = agarrador, combo creditado a ele); HitEvent source 0xD (dano nela) e 8 (no que ela acerta) | idem; vista unificada ganha câmera do arremesso |
| `.CHR` | `readKey` | chave 0xFDB (os dois: +0x14 e +0x1C..+0x28); 0xFCD (BeingThrown +0x14..+0x88) | |

Os quatro estados de reação derivados do ThrowBack (KnockBack 0x7EA0, BeingSlammed 0xDE30, BeingThrown 0xE3E0,
GrappleBreak 0xE990) têm cada um os seus próprios floats de dano nos mesmos offsets relativos; o treino deve ler do
estado em que a vítima está.

## 4. Arremesso de objeto: Throw (+0xEF40) e TwoHandedThrow (+0x1074C), `MonsterStates.cpp` + `Weapon.cpp`

| campo | onde | Throw (0xEF40) | TwoHandedThrow (0x1074C) |
|---|---|---|---|
| comando | `transitionOK` | toque novo de bola (`PadEntry[0]` 0x2E e `[1]` 0x2E = 0), anim 0x34, `attacksEnabled`, objeto que não é tipo 7 (javelin → `StateJavelin` 0xFE38) nem de duas mãos | `curMappedAction` 0xA (bola) → anim 0x37; 0xB (cima + bola) → anim 0x38 só com objeto corpo rígido (bit 2 do `Pickup+0x50`); objeto de duas mãos (bit 1), anim 0x37, `attacksEnabled` |
| janela (solta) | `update` | `releasePercent` +0x98 (**0xEFD8**) da anim 0x34 → `Monster::throwPickup(0x80A, 0)` | 0xA: +0x20 (**0x1076C**, padrão 0,35) → `throwPickup(0x807, 0)`; 0xB: +0x28 (**0x10774**, 0,9) → `LevelPickups::throwRBPickup` com ângulo +0x2C (0x10778, graus) e velocidade +0x30 (0x1077C, mph) |
| dano | `Weapon::InitWeapon` (tipo 0x17) | `m_dyn.damage` = `LevelPickups::s_info[t].damage` × `Monster::m_puLaunchDamageMod[t]`: `s_info` em **0x7C9B90**, stride **0x54**, 28 entradas (`t` = `Pickup+0xA0`); dano em +0x08, raio +0x0C, duração +0x2C, ganho de stamina +0x30, velocidade +0x34 (mph), gravidade +0x38; multiplicadores em Monster +0x6A38 + 4t (dano), +0x6B18 (duração), +0x6B88 (velocidade) | igual para 0xA (mesmo caminho `throwPickup`); 0xB lança o corpo rígido do próprio objeto (dano pela colisão dele: não decodificado) |
| alcance | `Weapon::WeaponHD` | esfera varrida de `m_prevPos` (+0x90) à posição atual (+0x150 → trans) com raio `s_info[t].radius`; tipo 7 usa a esfera 0 do pickup | idem |
| efeito | `Weapon::DetonateWeapon` | HitEvent tipo 3, source 0xB, subtipo = tipo do pickup; `dispatchHit(dir, m_dyn.damage)`; o arremessador recebe `s_info[t].staminaGain` ao acertar qualquer interactive; tipo 7 empala; tipo 0xE (gerador) eletrocuta; escudo (0x10) do alvo em bloqueio reflete | idem |
| onde ler o `Weapon` vivo | pool `Weapons::m_weapons` | `TheGame` +0x112490 + 0xC00, 100 × 0x170; `m_active` +0x40, `m_dyn.damage` +0x10, `m_type` +0x4C (0x17), `m_ownerId` +0x120, `m_pickup` +0x154 | |
| `.CHR` | `readKey` | chave 0xFB0 (Throw +0x84..+0x98) | (sem chave vista; defaults do construtor) |

## 5. GetupAttack (+0x10C40), asm `asm/nonmatchings/game/MonsterStates/*GetupAttack*`

| campo | onde | valor ou offset |
|---|---|---|
| comando | `StateThrowBack` (deitado, `subState` +0x260 = 3) | levanta atacando quando `getupAttack` (ThrowBack +0x558) foi marcado no esperneio: um botão de ataque segurado enquanto aperta para levantar (qual botão não confirmei no asm do `StateThrowBack::update`); `transitionOK` só exige anims 0xF2/0xF3 e modo de jogo ≠ 7/8 |
| entrada | `transitionInto` | `subState` +0x64 (0x10CA4) = 0 se Monster +0x3358 ≤ 0, senão 1 (deitado de costas/de bruços, provavelmente); câmera e HitHistory (+0x68) zerados; vira para o monstro mais perto dentro de +0x60 (0x10CA0) |
| tabela de entradas | `enterSubState`/`update` | entrada `i` em `this + i*0x24`: janela **+0x14/+0x18** (0x10C54/0x10C58; i=1: 0x10C78/0x10C7C), localizador **+0x1C** (0x10C5C / 0x10C80), anim **+0x20** (0x10C60 / 0x10C84), velocidade +0x24, knockback **+0x28/+0x2C** (0x10C68/0x10C6C / 0x10C8C/0x10C90) → HitEvent +0x34/+0x38, atordoamento **+0x30** (0x10C70 / 0x10C94) → HitEvent +0x3C, **dano +0x34** (0x10C74 / 0x10C98); blend +0x5C (0x10C9C) |
| janela | `update` | fração da anim da entrada entre +0x14 e +0x18 |
| alcance | `update` | esfera varrida do localizador (anterior Monster +0x4D10 + k·0x10 → atual +0x3DC0 + k·0x10, k = id − 0x802) com raio `StatePunch::getFistSize()` (raio do soco corrente ou padrão Punch +0x2980); até 50 resultados; um alvo por entrada (HitHistory +0x68) |
| dano/efeito | `update` → `dispatchHit(dir, dano, id, alvo)` | HitEvent tipo 2 se knockback ≠ 0 senão 1, source 0x12; vítima em Counter (0x1B) não leva; `dir` = (atual − anterior) × updateRate; sem custo nem crédito de stamina vistos |

## 6. CrowdControl (+0xF3F0), asm `asm/nonmatchings/game/SpecialStates/*CrowdControl*`

| campo | onde | valor ou offset |
|---|---|---|
| comando | `transitionOK` | `PadEntry[0]` 0x38 (**quadrado + X juntos**) com `Monster::isSpecialAvailable()` (medidor de especial); `attacksEnabled`, não `isHoldingLarge`, anims 0xA3/0xA4/0xA5, a animação de efeito própria (+0x2E8, handle da anim 3 do hier do monstro) parada; IA só no estado do cérebro certo |
| janela | `update` | anim 0xA3 (preparo) até 99% → 0xA4 (ou 0xC0 no ar); dispara uma vez quando a fração da 0xA4 ≥ **+0x38 (0xF428)** e `pending` +0x2E4 = 1; depois 0xA5 até 99% → Idle |
| onda | `ShockWave::Activate(trans, ring, causa 6, id, anim +0x2E8, cs +0x2FC)` | ExpandingRing copiado de **+0x3C** (0xF42C): nº de anéis 0xF42C, anéis em 0xF44C (stride 0x1C, dano em +4, knockback +0xC/+0x10, atordoamento +0x14), `reach[i]` em **0xF67C** + 4i, `timeScale` 0xF6CC; `getMaxRange` = `reach[n−1]`; mesma lógica de anel/raio do item 1 |
| custo | `update` | `Monster::drainSpecial()` (o medidor inteiro) |
| dano | onda | `rings[i].damage` conforme a distância |
| `.CHR` | `readKey` | chaves 0xFE0/0xFE2: +0x1C..+0x38 (blends, velocidades, percentual); o anel provavelmente vem junto (não conferi o resto do case) |

## 7. Counter (+0x7DD8), Block (+0x7DA0), Taunt (+0xFE20), Climb (+0xEFE0), BatSwipe (+0xAE90)

| estado | comando | números | efeito |
|---|---|---|---|
| **Counter** | de dentro do Block: `PadEntry[0]` 0x46 (R2 segurado + toque recente de quadrado ou triângulo); stamina não esgotada (`exhausted`), sem alvo travado, anim 0x40, sem objeto de duas mãos | janela em campos: `counterStart` +0x3C (0x7E14) e `counterEnd` +0x40 (0x7E18) (`getCounterStartField/EndField`); blend +0x24 (0x7DFC); chave 0xFA6 (+0x14..+0x30) | `counterSuccess` (asm VU0, não lido) marca `landed` +0x44; a vítima vai para Countered (0x11114, anim 0x4C, ataques desligados) e perde o objeto para quem contra-atacou se `stealsPickup` +0x4C; HUD 6. Nenhum `takeDamage`/dano numérico encontrado nos arquivos lidos (confiança baixa: pode estar no `counterSuccess`) |
| **Block** | R2 segurado (`PadEntry` 0x34); anim 0x3C, `attacksEnabled`, pés em algo | `raiseFrames` +0x2C (0x7DCC) quadros até bloquear de fato (`blocking` +0x30); escala de dano `damageScale` +0x20 (0x7DC0), `damageScaleArmed` +0x24 (0x7DC4) com objeto ou alvo travado → `Monster::m_damageModifier`; `autoBlockRange` +0x14 (IA); chave 0xFC2 | sem custo de stamina na entrada; fontes 6, 0x14 e choque 0x40 não bloqueáveis (`isBlockable`); soltar R2 → Punch/Grapple/Idle |
| **Taunt** | `m_wantsTaunt` (Monster +0xF0) = 1, posto por `PadFlags::checkTaunt` (combinação não decodificada aqui) e anim 0x5C | só `blendTime` +0x14 (0xFE34) | nenhum efeito de jogo além de animação e som (`update` não mexe em stamina, vida nem especial) |
| **Climb** | toque novo de bola (`PadEntry` 0x2E) com mãos livres perto de parede (`checkWallContact`) | velocidades +0x28..+0x38, `reach` +0x54; chave 0xFBA | não ataca: só escala/pendura/sobe; de lá o jogador pula (volta ao estado do ar, de onde o ButtSlam vale) |
| **BatSwipe** | `curMappedAction` ∈ {0,1,2,5,6,7} → tipo +0x2D4C igual ao soco (5→0, 6→1, 7→2, 1→3, 2→4, 0→5); `attacksEnabled`, Monster +0x68B4 = 0, objeto de **uma mão** (bit 1 do `Pickup+0x50` zerado); quadrado sozinho exige alvo: `getClosestMonsterToPunch(Punch +0x297C, Swipe +0x2D30, Swipe +0x2D34)` ou destrutível na mira (`getDestructibleFromReticle(Swipe +0x2D34)`); IA só no estado do cérebro certo | config e dano já em `FORMATOS.md`/`NATIVO.md` ("Tabela dos socos") | `Swipe +0x2DD8` = 1 quando a ação foi 0 ou 5 (sem direção) |

## Resumo

| estado | comando (esquema 0) | dano (como obter) | janela | alcance | efeito | confiança |
|---|---|---|---|---|---|---|
| ButtSlam 0xFA1C | no ar: cima (recente) + triângulo (`PadEntry` 0x44) | direto: anel 0 em 0xFAA8; onda: anel por distância (anéis 0xFAA4 + 0x1C·i, dano +4) | primeiro toque no chão após `boostPercent` 0xFA4C | direto `reach[0]` 0xFCD4; onda cresce `timeScale` 0xFD24 × idade até `reach[n−1]` | derruba (tipo 2); custo stamina 0xFA68; ganho 0xFA30 | alta |
| RamAttack 0xFD40 | corrida (2 toques frente) + triângulo (`PadEntry` 0x40) | 0xFD80 + |vel·normal| × 0xFD84 | carga de 0xFD78 campos (subestado 0xFD9C = 0) enquanto segura triângulo | o corpo (feixe de esferas do movimento) | derruba (tipo 2, source 9); custo 0xFD88; ganho 0xFD54; 1 alvo | alta |
| Grapple 0xDCE0 | bola (`curMappedAction` 0xA) | — | anim 0xAA entre 0xDD10 e 0xDD14 | cone: alcance 0xDD04 (entrada) / 0xDD18 (anim), ângulo 0xDCFC, olho 0xDCF8/0xDD00, avanço 0xDCF4 + Monster +0x3F4 | vítima → Grappled; agarrador → Lift | alta |
| GrappleLift 0xDD54 | segurando; toque novo de bola / ação 0xA ou 0xB enfileirada (0xDD70) | — | fim da anim 0xAB (99%) | — | vai para Throw/Throw1; vítima drena stamina do agarrador (0xDDF0 por toque) | alta |
| GrappleOHAttack 0x10AE0 | quadrado (0) normal / triângulo (5) especial | `takeDamage` 0x10B08 (normal) / 0x10B1C (especial) | ponto 0x10B04 (0xB1) / 0x10B18 (0xBA) | só a vítima agarrada | normal: para o esperneio por 0x10B24 campos; especial: solta (0xDD24/0xDD28) p/ GrappleBreak; custo 0x10B0C/0x10B20; ganho +0x14 | alta |
| MonkeyOHAttack 0x10B34 | idem; emenda: triângulo entre 0x10B7C e 0x10B80 | 0x10B5C / 0x10B70 | 0x10B58 / 0x10B6C | idem | custo 0x10B60/0x10B74 (de novo na emenda) | alta |
| GrappleThrow 0xDD74 | bola (0xA) | impacto da vítima: ThrowBack 0xE3E0: chão 0xE430 + v·0xE434; parede 0xE400 + v·0xE404; monstro 0xE44C + v·0xE450; + 0xE464 se caiu > 0xE468 | solta em 0xDD94 da anim 0x37 | balística ao alvo da mira: 0xDD9C vel., clamp 0xDD98 | vítima em BeingThrown; quem ela acerta leva o mesmo dano (objeto: 0xE45C/0xE460) | alta (fórmula); média (valores por monstro: ler da RAM) |
| GrappleThrow1 0xDDA0 | cima + bola (0xB) | idem | 0xDDC0 da anim 0x38 | reta: vel. do agarrador + frente × 0xDDC8 + cima × 0xDDC4 | idem | alta |
| Throw 0xEF40 | toque novo de bola com objeto de uma mão (≠ tipo 7) | `s_info[t]` (0x7C9B90 + 0x54·t) +0x08 × Monster +0x6A38 + 4t; `Weapon +0x10` já calculado | solta em 0xEFD8 da anim 0x34 | esfera varrida do `Weapon` com raio `s_info +0x0C` | HitEvent tipo 3 source 0xB; ganho stamina `s_info +0x30` | alta |
| TwoHandedThrow 0x1074C | bola (0xA) anim 0x37 / cima + bola (0xB) só corpo rígido, anim 0x38 | 0xA: igual ao Throw; 0xB: corpo rígido (não decodificado) | 0x1076C / 0x10774 | idem / `throwRBPickup` ângulo 0x10778, vel. 0x1077C mph | idem | alta (0xA) / baixa (0xB) |
| GetupAttack 0x10C40 | deitado, esperneio com botão de ataque segurado (ThrowBack +0x558) | entrada i (`+0x64`): dano em 0x10C74 + 0x24·i | 0x10C54/0x10C58 + 0x24·i da anim 0x10C60 + 0x24·i | esfera varrida do localizador 0x10C5C + 0x24·i, raio `getFistSize` | knockback 0x10C68/6C, atordoa 0x10C70 (+0x24·i); tipo 2/1, source 0x12 | alta (offsets); média (botão) |
| CrowdControl 0xF3F0 | quadrado + X juntos (`PadEntry` 0x38) com especial disponível | onda: anéis 0xF44C + 0x1C·i, dano +4 | dispara em 0xF428 da anim 0xA4 | onda até `reach[n−1]` = 0xF67C + 4(n−1), `timeScale` 0xF6CC | gasta o especial (`drainSpecial`) | alta |
| Counter 0x7DD8 | R2 + quadrado/triângulo (`PadEntry` 0x46), vindo do Block | nenhum dano numérico encontrado | campos 0x7E14..0x7E18 | `counterSuccess` (asm não lido) | vítima → Countered 0x11114; rouba objeto (0x7E24) | média |
| Block 0x7DA0 | R2 segurado (`PadEntry` 0x34) | reduz dano recebido: × 0x7DC0 (ou 0x7DC4 armado/travado) | ativo após 0x7DCC quadros | — | sem custo de stamina; permite Counter | alta |
| Taunt 0xFE20 | `m_wantsTaunt` (Monster +0xF0) via `checkTaunt` | — | anim 0x5C | — | nenhum | alta (efeito) / baixa (comando) |
| Climb 0xEFE0 | toque novo de bola junto a parede | — | — | — | não é ataque | alta |
| BatSwipe 0xAE90 | mesmas ações do soco (0/1/2/5/6/7) com objeto de uma mão | ver `FORMATOS.md` | idem | idem | — | alta |

## Em aberto

- Botão exato que marca `getupAttack` (ThrowBack +0x558) e a combinação do `checkTaunt`.
- `counterSuccess` (VU0) e o dano, se houver, do contra-ataque; `StaminaMeter::drain` continua em asm (só o valor passado importa).
- Dano do arremesso de corpo rígido (`throwRBPickup`, ação 0xB do TwoHandedThrow).
- Chave do `.CHR` dos anéis do CrowdControl (o case 0xFE0/0xFE2 foi lido só até +0x38) e das stamina (+0x14/+0x18) dos `AttackState`.
