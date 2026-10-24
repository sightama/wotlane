# Modo treino: como o jogo decide que um golpe acertou (2026-10-23)

Investigação só de leitura (asm + rascunhos do m2c + `.CHR` do disco). Endereços em `native/wotm_syms.h` (`wotm::fn::`/`wotm::sym::`).
Offsets de Monster relativos ao objeto (0x11190 bytes); estados embutidos nos offsets do comentário de `include/game/game.h`.

## Resumo

- **Não há caixa de golpe dedicada.** Quase todo ataque de contato é uma **esfera varrida** (`hdSphereTest(from, to, r, res, skipId, max)`,
  0x2017E0) do ponto de um **localizador** (osso nomeado, ids 0x802..0x832) do quadro anterior até o do quadro atual, contra a grade de
  colisão do mundo (cascos/hulls de cenário e dos `_cs` dos monstros). O alvo vem do resultado: `_hdResult.ids & 0x7FF` = id de
  interactive (0 = cenário, só raspa); `Interactives::getInteractive(id)->kind == 1` = monstro.
- A **"hurtbox"** é o casco de colisão do corpo do alvo (os mesmos cascos que o movimento usa); não existe outra.
- **Janela**: percentual da animação do golpe (`animationGetCurrentPercent`) entre `start` e `end` da config, fora de transição de animação.
- O dano não sai na hora do teste: `HitEvent::create()` + `Interactives::dispatchHit(dir, dmg, attackerId, targetId)` (0x147240) →
  `Monster::takeHit` (bloqueio, estado que recusa) → `Monster::takeDamage` (0x15FE40; escala de dificuldade da IA, god mode) →
  `HealthMeter::drain`. Golpes sem geometria (pisão, golpes de agarrão) chamam `takeDamage` direto.
- Os números (raio, dano, janela, localizadores) vêm do **`RST/<MONSTRO>.CHR`** (chaves lidas por `charreset::readKey`, 0x126620) e
  **dá para ler** (script de exemplo no fim; valores do Congar conferidos).

## Localizadores (posição de mão/pé em mundo)

`Monster::handleLocator(id, mat, trans)` (0x161EF8), callback do hier a cada desenho/atualização, para `id` em 0x802..0x832 (49):
copia o atual para o anterior e grava o novo. Índice `k = id - 0x802`:

| tabela | offset no Monster | stride |
|---|---|---|
| matriz atual | 0x3180 + k*0x40 (`getLocatorMat`) | 0x40 |
| translação atual (mundo) | 0x3DC0 + k*0x10 (`getLocatorTrans(id)` = this + id*16 - 0x4260) | 0x10 |
| matriz anterior | 0x40D0 + k*0x40 | 0x40 |
| translação anterior | 0x4D10 + k*0x10 (= atual + 0xF50) | 0x10 |

Ids vistos nos golpes: 0x802/0x803 = mãos (2050/2051), 0x804/0x805 = pés (2052/2053) (`AnimContact::getLimb` usa a mesma
numeração; 0x816/0x817 são os outros pés). 0x807/0x808/0x80A prendem pickups segurados. Id < 0x7D0 (ex.: -1, 1999) = sem localizador.

## Tabela de ataques

| ataque (estado, offset no Monster) | forma | onde testa | janela | gancho |
|---|---|---|---|---|
| Soco/combo `StatePunch` (0x8470) | esfera varrida, raio `cfg+0x74` (0 → `Punch+0x2980`), de `locPrev` a `locCur` para até 2 localizadores (`cfg+0x7C`, `cfg+0x80`) | `PunchSwipeConfigBase::collisTestCloseRangeAttack(cfg, locId, Monster*, HitHistory&)` 0x14EA90, chamado por `StatePunch::update` 0x154EA8 | `cfg+0xC <= pct <= cfg+0x10` → `Punch+0x2A00 = 1` (`inCollisionWindow`) | wrapper em `collisTestCloseRangeAttack` (args = tudo) |
| Golpe com pickup `StateBatSwipe` (0xAE90) | pickup comum: igual ao soco, 1 localizador `cfg+0x7C`; pickup tipo 6 ou 8 (longos): **esferas estáticas** do pickup (`Pickup+0x110`, {x,y,z,r} locais × matriz do cs, `Pickup+0x160` = quantas) | `collisTestCloseRangeAttack` ou `Pickup::collisTestBatAttack` 0x180680 (de `StateBatSwipe::update` 0x14FE68) | `cfg+0xC..0x10` → `Swipe+0x2DC0` | wrappers nas duas |
| Ataque ao levantar `StateGetupAttack` (0x10C40) | esfera varrida no localizador da entrada atual; raio = `StatePunch::getFistSize` (do soco corrente/padrão) | inline em `StateGetupAttack::update` 0x1538E0 (chama `hdSphereTest` direto) | entrada `i = +0x64`, base `this + i*0x24`: janela +0x14..+0x18, localizador +0x1C, anim +0x20; HitHistory +0x68 | wrapper em `hdSphereTest` com marcação |
| Pisão `StateStompAttack` (0x10CF0) | **sem geometria**: vítima escolhida em `transitionFeasible` por `getClosestMonsterToPunch` (cone/distância) e guardada em +0x4C | `StateStompAttack::update` 0x158850 → `takeDamage` direto quando pct >= +0x34 (+0x38 com pickup); dano +0x3C (ou do pickup) | um disparo por pisão (+0x54) | `takeDamage` |
| ButtSlam `StateButtSlam` (0xFA1C) | no toque no chão: monstro mais próximo dentro de `reach[0]` leva o anel 0 direto (esfera centrada no monstro); senão solta **ShockWave**: esfera estática no centro, raio = `timeScale * idade` crescendo até `reach[n-1]`, filtro de altura `rings[0].f0` | `StateButtSlam::handleCollis` (src/game/MovementStates.cpp) e `ShockWave::update` 0x1B0BD8 (`hdSphereTest` estático 0x2017B0, até 100 resultados) | enquanto a onda vive | ler o pool `ShockWave::m_wavePool` 0x417F90 (10 × 0x330) |
| Investida `StateRamAttack` (0xFD40) | o **corpo**: o feixe de esferas do movimento (`hdBundledSphereTest` em `Monster::collisTestEnvironmentCollisions`) bate num monstro → `handleCollis` | `StateRamAttack::handleCollis` (HitHistory + dispatchHit) | subestado de carga | desenhar o feixe do corpo (não mapeado aqui) |
| Agarrão (GrappleOHAttack 0x10AE0, MonkeyOHAttack 0x10B34, GrappleThrow 0xDD74/0xDDA0) | **sem geometria**: a vítima é o agarrado; dano por `takeDamage` em pontos da animação; o arremesso solta a vítima (StateBeingThrown 0xE3E0), que bate pelo corpo | `update__20StateGrappleOHAttack` etc. | animação | `takeDamage` |
| Projéteis/pickups arremessados (`Weapon`, 100 em `Weapons::m_weapons`, TheGame+0x112490+0xC00, stride 0x170) | esfera varrida `m_prevPos (+0x90)` → `m_cs->trans` (`+0x150`), raio 0, 15 ou o do pickup (tipo 0x17: esfera 0 do pickup ou `LevelPickups::s_info[t].radius`); explosão: `DispatchRangeBasedDamage` com raio `m_dyn.radius (+0x24)` | `Weapon::WeaponHD` (src/game/Weapon.cpp, decompilado) | vida do projétil (`m_active +0x40`) | ler o pool por quadro |
| GunAttack (0x10E90) | cria `Weapon` (`Weapons::CreateStaticWeapon`) → ver linha acima | `StateGunAttack::update` | — | idem |
| FireBreath (`FireBreath`, estado 0xF974) | segmento(s) de linha de visada (`hdLosCSTest`) — geometria exata a confirmar | `FireBreath::TestCollis` 0x134678 | `timeLeft` | wrapper em `TestCollis` ou em `hdLosCSTest` com marcação |
| Especiais | SonicRoar: cone (`StateSonicRoar::testConeCollis` 0x1CA760); GrappleHook: `testCollis` 0x1C78E8; CannonHands/AirStrike/LavaBlast/etc. via Weapon | SpecialStates | — | depois |

## `StatePunch` e `PunchConfig` (offsets)

`StatePunch` (Monster+0x8470): configs em `+0x20 + chain*0x420 + step*0xB0` (10 cadeias × 6 passos); `+0x2988` cadeia atual,
`+0x298C` passo atual (`getCurrentConfig`); `+0x2974/+0x2978/+0x297C` ângulo/alcance/dist. máx. de `getClosestMonsterToPunch`
(`getSphereRadius` devolve +0x297C: é o raio de busca de alvo, **não** o do golpe); `+0x2980` raio padrão do punho; `+0x29A0` HitHistory
do golpe; `+0x2998` já acertou neste passo; `+0x2A00` na janela de colisão; `+0x2A04` na janela de contra-ataque; `+0x29FC` início da pausa.

`PunchConfig` (0xB0; `PunchSwipeConfigBase` com vtable em +0x88):

| cfg+ | campo | origem no `.CHR` (chave 0xFCC, base `key + 0x10 + c*0x3C0 + s*0xA0`) |
|---|---|---|
| 0x00 | MonsterAnim (handle em Monster+0x1CF0 + anim*0x10) | +0x28 |
| 0x0C / 0x10 | início / fim da janela de colisão (fração da anim) | +0x2C / +0x30 |
| 0x14 / 0x18 | janela de contra-ataque (0,0 → usa 0x0C..0x10) | +0x34 / +0x38 |
| 0x28 / 0x2C | janela de entrada do próximo golpe (campos) | +0x48 / +0x4C |
| 0x30 | ponto de troca para o próximo golpe | +0x50 |
| 0x44 | avanço do monstro durante 0x54..0x58 | +0x74 |
| 0x64 / 0x68 | força de knockback (≠0 → HitEvent.type 2, senão 1) → HitEvent+0x38/+0x34 | +0x8C / +0x90 |
| 0x6C | atordoamento (>100 → type 7, `isStunHit`) | +0x94 |
| 0x70 | pausa ao acertar (ms; `getHitPause`) | +0x98 |
| 0x74 | **raio da esfera do punho** (`getFistSize`) | +0xA8 |
| 0x7C / 0x80 | **localizadores** testados (int) | +0x10 / +0x14 |
| 0x84 | 1 = golpe com pickup (dano do `LevelPickups::s_info`) | — |
| 0x90 | **dano** (`PunchConfig::damageCaused`) | +0x20 |
| 0x94..0xA8 | 6 floats por tipo de próximo golpe (>0 = permitido) | +0x54.. (6×4) |

(A chave 0xFD8 é o mesmo layout sem os 0x10 de deslocamento; o retail usa 0xFCC. `Punch+0x2980` vem da chave 0xFA1 +0x30.)

`StateBatSwipe` (Monster+0xAE90): configs em `+0x20 + chain*0x480 + step*0xC0`, cadeia/passo em `+0x2D48/+0x2D4C`, janela
`+0x2DC0`, HitHistory `+0x2D60`, `getDamageMod` = cfg+0xB0, `damageToPickup` = cfg+0x94; mesmo começo de layout (anim +0, janela
+0xC/+0x10, localizador +0x7C, raio +0x74). Chaves do `.CHR`: 0xFA4/0xFAC/0xFD7/0xFE3 (o Congar traz a 0xFE3; não decodifiquei).

## Fluxo do teste (`collisTestCloseRangeAttack`, em palavras)

1. `hdPolyCheckBackface(true)`; `n = hdSphereTest(locPrev(id), locCur(id), cfg->radius, res[50], idDoProprioCs, 50)`.
2. Para cada resultado: id 0 → `newHit` conta erro e toca raspão. Senão, se `HitHistory::newHit(id)` é novo (um alvo por golpe):
   monstro (kind 1) com `m_unk49` (vulnerável) e não IA×IA proibido; se ele está em Counter (0x37) e `counterSuccess` → cancela;
   efeito do soco no ponto `locCur`; `registerComboHit`; senão (objeto) partículas.
3. `HitEvent`: pos = `locCur`, type 1/2/7 como na tabela, source 2 (3 com pickup), strengths de cfg+0x68/+0x64; dano = virtual
   `damageCaused` (cfg+0x90) ou `s_info[pickup].dano × getDamageMod`.
4. `dir = (locCur - locPrev) * updateRate`; `dispatchHit(dir, dano, idAtacante, idAlvo)`; retorna 1 se algo levou.

## Valores lidos (Congar, `disc/RST/MONKEY.CHR`)

Exemplos (cadeia c, passo s): c0s0 anim 11, mão 0x803, janela 0.31–0.90, dano 5, raio 17.1; c0s2 anim 12, mãos 0x803+0x802,
0.15–0.57, dano 8, knockback 695/700; c0s3 pés 0x805+0x804, 0.28–0.56, dano 6; c6s2 pés, raio 20, dano 7, atordoamento 25.
Raio 0 em vários passos (cai no padrão 17.15 da chave 0xFA1). Alcance de busca do alvo 200, alcance 220, ângulo 1.152.

## Recomendação de gancho (nativo)

**A. Fiel (recomendado): marcar o chamador e gravar o que o `hdSphereTest` testou.**
- Wrappers (padrão de `wotm_custom.inc`: guardar `g_orig*`, chamar o original) em:
  `collisTestCloseRangeAttack__20PunchSwipeConfigBaseiP7MonsterR10HitHistoryi` (a0 = cfg, a1 = localizador, a2 = Monster atacante),
  `collisTestBatAttack__6PickupfUi11MonsterAnimR10HitHistoryfff`, `update__16StateGetupAttack`, `update__9ShockWave`, `WeaponHD__6Weapon`,
  `TestCollis__10FireBreath`. Cada um põe um "contexto atual" (tipo de golpe, dono, cfg, dano previsto) e limpa na saída.
- Wrappers em `hdSphereTest__FP8_fvectorT0fP9_hdResultUii` (a0 from, a1 to, f12 raio, a2 resultados, a3 id ignorado, t0 max) e
  `hdSphereTest__FP8_fvectorfP9_hdResultUii` (esfera estática): **só com contexto ligado**, guardar {from, to, r, dono, tipo, nº de
  acertos, ids do resultado} numa lista do quadro. Esse `hdSphereTest` é usado também por câmera e física; sem contexto, ignorar.
- O desenho lê a lista no fim do quadro (cápsula from→to com raio r; verde = sem acerto, vermelho = acertou monstro).
  Lembrar que roda na thread do EE: copiar a lista no ponto de captura do quadro (`wotmCaptureFrame`, ver memória "Captura na thread do EE").

**B. Passivo (sem gancho, também mostra a esfera fora do acerto):** por quadro, para cada `m_monsters[i]` (shellGame+0x120380, 8):
se `*(Monster+0x34) == Monster+0x8470` e `Punch+0x2A00` → cfg = `Punch+0x20+chain*0x420+step*0xB0`; para `id` em cfg+0x7C/0x80 com
`id >= 0x7D0`: centro `Monster + id*16 - 0x4260`, anterior +0xF50, raio cfg+0x74 ou `Punch+0x2980`. Igual para BatSwipe (+0x2DC0).
ShockWave: varrer o pool (10 × 0x330 em 0x417F90; +0x0 ativa, +0x4 idade, +0x10 centro, +0x20 dono, +0x84 ExpandingRing:
nº de anéis +0x84, reach +0x2D4, timeScale +0x324). Weapons: `m_weapons[100]` com `m_active`.

**Dano:** wrapper em `takeDamage__7MonsterfbP7Monster` (a0 vítima, f12 dano, a1 force, a2 fonte): ler `m_health` antes e depois do
original e mostrar o delta (já inclui bloqueio, escala da IA, god mode; fogo/choque chegam com `force` = 1, por quadro). Para o
"dano previsto" do golpe, o `HitEvent`/`dispatchHit` (f12) dá o valor antes do bloqueio.

## Script de leitura do `.CHR`

Formato: cabeçalho de 0xC bytes e depois chaves `{u32 id; u32 tamanho_total; dados}`. Script de rascunho usado aqui (fora do repo):
`C:\Users\TwistZero\.claude\jobs\52a947d6\tmp\chr_dump.py <arquivo.CHR>` lista as chaves e despeja a 0xFCC/0xFD8. Se o modo treino
for mostrar a tabela de golpes, vale virar `tools/chr_dump.py`.

## Em aberto

- Forma exata do FireBreath (`hdLosCSTest`: quantos segmentos e de onde) e do feixe de esferas do corpo (RamAttack / arremessado).
- Layout da SwipeConfig no `.CHR` (chave 0xFE3) e dos especiais (SonicRoar cone, GrappleHook).
- Hurtbox desenhável: cascos dos `_cs` do monstro (listas em Monster+0x1A14, `Monster+0x1A38` = quantas) — não mapeado.
