# Goliath Prime jogador "sempre em lock-on": é o olhar (look-at), não o lock-on

Investigação de 2026-10-23, só leitura (`src/game`, `asm/` lido e não colado, `disc/RST/*.CHR`, `disc/MON/*.NGP` com
`tools/pose.py`). Nada rodado no runtime. Relato: com o Goliath (tipo 13, `m_typeBits` 0x1A0, ASSBOSS1) como jogador contra
CPU, ele parece travado mirando o inimigo o tempo todo.

## Resumo

- **O lock-on de verdade não liga sozinho.** Não há teste de tipo 0x1A0 no caminho do lock-on, e o `.CHR` não tem flag de
  lock automático. O lock só existe com o botão (ou o modo alternado `m_pinMode`, que nenhum código liga).
- **O que o dono vê é o `Monster::updateLookAt` (0x15DD70).** Todo monstro vira "a cabeça" para o inimigo mais próximo no campo
  de visão (`getLookAtTarget`, 0x1617A8) com duas camadas de animação: a anim 154 no `AnimBlend` de +0x2FE0 e a 155 no de +0x3048
  (`Monster::initAfterDbLoad`: handles +0x2690 e +0x26A0 = `m_anims[0x9A]` e `[0x9B]`). No Togera as 154/155 mexem
  3 canais do pescoço em ±20° a ±50°. **No ASSBOSS1 cada uma mexe um canal só, em ±90°, e esse canal move 28 das 30 peças
  rígidas do modelo** (`pose.py`: 154 canal 8, 155 canal 6, de +π/2 a −π/2). Ou seja, no chefe o "olhar" é a torre inteira
  (tronco, braços e canhões) girando para o alvo: no chefe da aventura é a mira dele. Como jogador, o corpo fica o tempo todo
  apontado para a IA mais próxima, o que parece lock-on.

## 1. Como o lock-on do jogador funciona

- **Botão:** `PadEntry::f32` (+0x32 da entrada do anel do `PadFlags`, `Monster+0x5040`). Nas configs de controle
  (`PadFlags::updateInput0ConfigCR/SR`, em `asm/game/PadFlags_2.s`): um gatilho sozinho (pressão em `PadFlags+0x1658`) = desvio para a esquerda (`f1A`), o outro (+0x165E) =
  para a direita (`f1C`), **os dois juntos = lock** (`f32 = 1`; provavelmente L2/R2, a conferir no mapa do pad). Na SR também liga `f32` na cratera da usina (fase 6, `craterSwitched`,
  planta viva), sem relação com tipo.
- **Modo alternado:** `Monster::m_pinMode` (+0x6C38) e `m_pinToggle` (+0x6C34): com `m_pinMode`, cada aperto inverte o toggle
  (`Monster::update`, `src/game/Monster.cpp:168`). Só os construtores escrevem +0x6C38, e com zero: na prática é sempre "segurar".
  `Monster::isTargetPinning` (`Monster.cpp:2684`) resume as duas formas.
- **Alvo:** `Monster::updateLock(MonsterReticleState)` (0x15D328), chamada pelos `update` de quase todos os estados (Idle, Run,
  Dash, Jump, Fly, Punch, Block, Throw, especiais). Sem o botão grava `m_pinTarget` (+0x6C04) = 0 e sai. Com o botão pega
  `getClosestTargetable(6, 0, 6000)`, mantém o alvo enquanto válido, calcula `m_aimPitch` (+0x6C44)/`m_aimHeading` (+0x6C40) e
  vira o `_cs` (heading em `m_cs+0x68`) para o alvo: direto, ou suavizado por +0x6C1C (no chão) / +0x6C18 (voando, byte +0x280).
  `hasPinTarget` (`Monster.cpp:2044`) = `m_pinTarget` com o bit 2 de +4.
- **Câmera:** `Camera::normalUpdate` só usa o lock com `hasPinTarget` (pitch limitado por `m_pinMaxPitch` +0x6C20 quando a
  diferença de altura passa de `m_pinMuckingDist` +0x6C24). A câmera de combate (`TheGame::gameCheckForCloseCombat` 0x138840 →
  `Cameras::InitCCCamera` 0x11C2B0, vista unificada, `Monster+0x6728 = 1` = controles SR) usa o POV 1 da tabela de câmera do
  monstro (+0x6D90: +0x8C distância de entrada, +0x90 de permanência, +0x68 quadros para sair). Vale para todo monstro e não tem
  teste de tipo.
- **Giro:** `MonsterDynamics::updateTurn` usa `m_turnMaxRate/m_turnAccel/m_turnDrag` (+0xC8/+0xCC/+0xD0, chave 0xFA1 do `.CHR`).
  ASSBOSS 45 / 5,1 / 30,5, Togera 42,1 / 5,1 / 37. Não trava o giro.

## 2. O que liga o "lock" no 0x1A0

- **Tipo:** os únicos testes de 0x1A0 são os da tabela de `goliath_codigo.md`. Nenhum está em `updateLock`, `PadFlags`, `Cameras`,
  `MonsterDynamics`, `updateLookAt` ou `getLookAtTarget`. O `StateIdle::update` (0x14F418) pular o `updateTurn(true)` só tira o
  giro parado. O `StateTaunt` vira para o alvo só durante a provocação.
- **`.CHR` (chave 0xFAE, a dos parâmetros de mira):** o ASSBOSS difere do LIZARD em `m_pinMuckingDist` (750 contra 80),
  `m_targetingMod` +0x6C14 (0,21 contra 0,10), giro do lock +0x6C1C (0,2 contra 0,1), +0x284 (50 contra 60), +0xE4 (110 contra 120)
  e +0x6978 (6 contra 9). Todos só agem com o lock ligado ou na mira. Nenhum liga lock sozinho.
- **Troca de câmera do port (`setActive` em `wotm_custom.inc`):** copia os 20 POVs do Togera e escala só `m_00/m_10/m_20`
  (distâncias). Os campos de combate (POV 1 +0x68/+0x8C/+0x90) ficam iguais aos do Togera, então a câmera de combate entra e sai
  como a de qualquer monstro. Não é a causa.
- **IA:** o jogador não tem `Ai` de chefe (`initAssBoss`/`updateAssBoss` só sob IA), então não é a IA.
- **Causa:** o look-at com as anims 154/155 do ASSBOSS1 (seção "Resumo"). Em `updateLookAt`, o alvo do olhar é, nesta ordem:
  `getTarget` se `m_launchCounter` (+0x6980) < 20 (logo depois de atirar); o mesmo `getTarget` com o direcional de olhar
  (`f1E`/`f20`) ou o lock; senão o `getLookAtTarget` (o inimigo mais próximo no cone de 75°, com linha de visão). Se não houver alvo,
  as duas camadas voltam a 50% (pose neutra, `s_lookAtReleaseSmoother`). O olhar só roda em alguns estados (tabela por id do
  estado no começo da função), entre eles o parado e o correr.

## 3. Gancho mínimo proposto

**`Monster::getLookAtTarget`** (`wotm::fn::getLookAtTarget__7Monster`, 0x1617A8). Só o `updateLookAt` chama essa função.

- Condição: `ctx->pc == entrada` e `goliathPlayer(rdram, ctx, runtime, a0)` (o helper que já existe em `wotm_custom.inc`: tipo
  0x1A0 num slot de jogador; equivale a `READ32(a0 + 0x14) == 0x1A0` e `READ32(a0 + 0x18) == 1`).
- Ação: `ret(ctx, 0)` (v0 = 0, `pc = ra`), sem chamar o original. Senão, encadear com o original.
- Efeito: sem lock, a torre volta ao centro e acompanha o corpo, como a cabeça de um monstro sem alvo. Com o lock (os dois gatilhos), o
  `updateLookAt` usa o `getTarget` (o alvo travado) e a torre mira, que é o lock normal. Depois de um tiro (+0x6980 < 20) ela
  aponta para a retícula por um instante, o que combina com a mira do chefe. A IA do Goliath (oponente, `m_playerNum` 2) continua
  mirando como no retail.
- Registro em `wotmCustomInstall`, no padrão dos outros:
  `if ((g_origLookAt = runtime.lookupFunction(kLookAt))) runtime.registerFunction(kLookAt, &lookAt);`

**Alternativa, se o dono quiser a torre mais solta e não presa ao centro:** em vez de zerar o alvo, deixar o original rodar e,
depois dele, multiplicar o desvio das duas camadas em relação a 0,5 por ~0,25 (os ±90° viram ±22°, perto do pescoço do Togera).
Isso exige um gancho na **saída** do `updateLookAt` (0x15DD70): chamar o original e reescrever o percent dos `AnimBlend` de
`a0+0x2FE0` e `a0+0x3048` (o layout do percent fica no `AnimBlend`, a conferir antes).

**Opcional, sem relação com o lock:** o giro parado. O `StateIdle::update` não chama `updateTurn(true)` para 0x1A0, então o
Goliath só gira andando. Se o dono notar, um gancho na entrada do `StateIdle::update` que troque o `m_typeBits` do dono do estado
(`READ32(a0 + 0xC)`) durante a chamada liberaria o giro. Isso mexe em todo o resto da função, então é melhor conferir antes.

## Como conferir

1. `WOTM_MATCH=13:1,<oponente>`: parado perto da IA, a torre não deve mais seguir o inimigo. Com o lock, deve travar e mirar.
2. RAM: no jogador (`READ32(game + 0x120380)`), os percent dos blends +0x2FE0/+0x3048 devem ficar em ~0,5 sem lock, e
   `m_pinTarget` (+0x6C04) deve ser 0 (confirma que o lock de verdade nunca esteve ligado).
