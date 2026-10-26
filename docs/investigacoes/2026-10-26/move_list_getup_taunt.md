# MOVE LIST: levantar atacando (GetupAttack) e provocação (Taunt)

Pesquisa para as dicas da MOVE LIST do modo treino (`native/runtime/wotm_training.inc`). Fecha os dois itens "Em aberto" de
`80b3802:docs/investigacoes/2026-10-25/move_list_comuns.md`. Leitura estática do asm e do C++ já decompilado; nada testado ao vivo.

## Botões no pad

O `GamePad::loadPadInputs` monta um byte por botão a partir da palavra crua do `scePadRead` (bits de botão com os dois bytes
trocados): byte 0 L1, 1 L2, 2 L3, 3 R1, 4 R2, 5 R3, 6 X, 7 círculo, 8 triângulo, 9 quadrado, 0xA Start, 0xB **Select**,
0xC/0xD esquerda/direita, 0xE/0xF cima/baixo. O `updateInput0ConfigCR`/`SR` (esquema 0, o padrão) copia para a `PadEntry` do
quadro:

- `+0x00` = Start, `+0x01` = Select;
- `+0x0A/+0x0C/+0x0E/+0x10` (`face[0..3]`) = quadrado, círculo, triângulo, X;
- `+0x2A` = X (pulo), `+0x2C` = triângulo (HEAVY), `+0x2E` = círculo (agarrar), `+0x30` = quadrado (LIGHT).

No esquema 1 o `+0x2C` é o círculo e o `+0x2E` o triângulo; no 2 os golpes vão para os ombros. Para a dica, o que vale é o papel:
`+0x2C` é o golpe pesado e `+0x30` o leve em todos os esquemas.

Mais adiante na mesma função, combinações apagam os botões de golpe do quadro: triângulo + círculo juntos (especial, `+0x36`)
zeram `+0x2C` e `+0x2E`; quadrado + X juntos (crowd control, `+0x38`) zeram `+0x30` e `+0x2A`; corrida (`+0x3E`) com triângulo vira
investida (`+0x40`) e zera `+0x2C`.

## 1. Levantar atacando

**Onde se marca.** O `StateThrowBack::update` (base do KnockBack +0x7EA0, BeingSlammed +0xDE30, BeingThrown +0xE3E0 e
GrappleBreak +0xE990), nos sub-estados deitado (`SUB_LANDED`/`SUB_LYING`, sub-estado em +0x260), considera que houve um toque
quando algum dos quatro botões de face (`face[0..3]`: quadrado, círculo, triângulo, X) está apertado neste quadro e nenhum estava
no anterior (`mashing`, +0x528). Em cada toque:

- credita `mashStamina` (+0x58, 2 nos valores vistos) de stamina;
- adianta o fim do tempo deitado: `lieStart` (+0x534) recua `lieTime` (+0x48, 60 fields) × `mashSpeedup` (+0x5C, 0,1);
- **regrava** `getupAttack` (+0x558) com 1 se, nesse quadro, LIGHT (`+0x30`) ou HEAVY (`+0x2C`) está apertado, senão com 0.

Ou seja: o que decide é o **último toque** antes de levantar. Apertar X ou círculo depois de um quadrado desfaz a marca. Segurar
não renova nada (sem borda), mas ao entrar no chão o `enterSubState` zera `mashing` e `getupAttack`, então um LIGHT/HEAVY que
já estava segurado na queda conta como toque no primeiro quadro deitado. Apertar quadrado e outro botão no mesmo quadro também
marca. Triângulo + círculo juntos não marcam (a combinação do especial zera o `+0x2C`), a não ser que o quadrado venha junto.

**Janela.** Só enquanto está deitado: de `lieStart` até `lieStart + lieTime` fields (cada toque encurta 10% de `lieTime`). Morto,
levanta 10 fields depois.

**Como vira GetupAttack.** No `enterSubState(SUB_GETUP)`, com `getupAttack` ligado e `StateGetupAttack::transitionOK` verdadeiro,
o monstro entra no GetupAttack (+0x10C40) em vez de levantar normal; nesse caminho não se agenda o `restoreVulnerability` (a
invulnerabilidade de `invulnFields`, +0x68, do levantar normal). O `transitionOK` recusa no dodgeball (`m_gameMode` 7) e no Big
Shot (8) e exige as animações 0xF2 e 0xF3 carregadas (`m_anims[0xF2]` em Monster +0x2C10 e `m_anims[0xF3]` em +0x2C20 diferentes
de zero).

**Qual entrada.** O `StateGetupAttack::transitionInto` olha Monster +0x3358: a componente z do eixo y da matriz do localizador
0x809 (o "pin", `getLocatorMat(0x809)` = +0x3340, mesmo ponto que `getPinTrans` +0x3E30 e o `updateOnFire` usam). Com z ≤ 0
entra na entrada 0 (animação 0xF2), senão na 1 (0xF3); o `enterSubState` grava o índice em GetupAttack +0x64. O
`StateThrowBack` usa o mesmo critério com o eixo y do cs para escolher deitar (0x55/0x56) e levantar (0x57/0x58), e o
`StateDeath` também lê +0x3358 para a morte deitada. Como o eixo y do monstro é a frente (o `SUB_GETUP` refaz o vetor de frente
+0x60 a partir dele), z ≤ 0 é **peito para baixo**: entrada 0 = de bruços, entrada 1 = de costas. Isto é inferência pela
orientação do localizador; o comentário atual do `wotm_training.inc` diz o contrário ("de costas / de brucos"). Para confirmar:
no treino, cair de bruços e de costas e ler GetupAttack +0x64 (ou o sinal de +0x3358).

**Números da entrada** (base GetupAttack + 0x14 + i × 0x24; no Monster, +0x10C54 + i × 0x24): +0x00 início e +0x04 fim da janela
de acerto (porcentagem da animação, 0..1), +0x08 localizador da esfera (0x802 no construtor), +0x0C animação, +0x10 velocidade
da animação, +0x14 e +0x1C os dois valores que a MOVE LIST mostra como empurrão e atordoamento, +0x20 dano. Coincide com os
`e + 0x14 … e + 0x34` que o código atual lê a partir de `m + 0x10C40`. Durante o golpe o monstro gira para o monstro mais
próximo dentro de GetupAttack +0x60 (300).

## 2. Provocação (Taunt)

**Botão: Select.** O `PadFlags::checkTaunt` é vazio no retail (só retorna). Quem pede a provocação é o começo de todas as
`updateInput{0,1,2}Config{CR,SR}`: com Select (`PadEntry +0x01`) apertado, liga `m_wantsTaunt` (Monster +0xF0) e sai sem ler o
resto do pad (o quadro fica todo zerado, só o `checkCombos` roda); sem Select, zera +0xF0. A IA faz igual: `Ai::taunt` aperta o
byte 0xB do GamePad.

**Quando entra.** O `StateTaunt::transitionOK` exige a animação 0x5C (Monster +0x22B0 diferente de zero) e `m_wantsTaunt`; toca o
som da provocação e consome o pedido. É testado no `StateIdle::update` (parado, antes da defesa) e no
`JumpFlyBase::handleStateTransitions` no sub-estado 9 (ao pousar de um pulo/voo, sem carregar monstro ou objeto de duas mãos).
Correndo não provoca. O estado (id 0x2A, blend +0x14 = 250) toca a 0x5C até o fim e volta ao Idle.

**Efeito: só com o medidor de especial cheio.** A provocação em si não dá stamina nem especial. O efeito está no
`Monster::updateBoostAndRage`. O medidor de especial é a stamina acima da base: `m_stamina` em +0x460 (`max` +0x460,
`maxLevel` +0x468, `cur` +0x470); fração = max(cur − max, 0) / (maxLevel − max). Cheio, liga `m_boostFull` (+0xF4) e o especial
(+0xF3). Com +0xF4 ligado, no estado 0x2A, quando a 0x5C passa de 95%, começa a **fúria** (`m_unkF5`, +0xF5): dois efeitos 0x6E
no corpo e o medidor drena por quadro `m_rageDrain` (+0x498) = (maxLevel − max) / (duração × 60), duração `m_rageDuration`
(+0x494) em segundos, 20 quando 0. Acaba com o medidor vazio ou com a morte. Monstros 0x1A0 e 0x1C0 (chefes) ficam de fora.

Durante a fúria:

- dano dado: o `Monster::takeHit` da vítima multiplica por `m_boostDamageMod` (+0x48C, com +0xF4) e por `m_rageDamageMod`
  (+0x49C, com +0xF5), cada um só se > 0. Como +0xF4 só cai com o medidor vazio, na fúria valem os dois juntos;
- velocidade: `m_rageSpeedMod` (+0x4A0) no lugar de `m_boostSpeedMod` (+0x490) (`MonsterDynamics::updateFootMotion` e
  `updateAirMotion`);
- projéteis do monstro saem teleguiados (`Weapon`, +0xF5) e o tiro padrão mira sozinho o monstro à frente (30°, 4000; em
  `launchDefaultProjectile`).

Como a fúria gasta o mesmo medidor do especial, provocar com ele cheio troca o especial pela fúria. A IA só provoca com +0xF4
ligado (`AiTaunt::getEntryRelevance`).
