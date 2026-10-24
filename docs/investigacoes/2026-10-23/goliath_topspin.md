# Goliath Prime jogador: o giro (StateTopSpin) no X+Quadrado

Investigação de 2026-10-23, só leitura (`asm/` lido, não colado; nada rodado). Continua `goliath_codigo.md` (seção 2).

## Resumo

- **X+Quadrado é o `StateCrowdControl`** (o ataque giratório de área dos monstros comuns), não um pulo+soco. E ele usa **as
  mesmas anims 163-165** do TopSpin: o TopSpin é a versão de chefe do mesmo golpe.
- No Goliath o CrowdControl não começa porque o `transitionOK` dele exige também a anim de efeito do estado (`+0x2E8`, criada no
  `init` com o tipo `m_typeBits + 3` e a anim 164; o Goliath não deve ter esse modelo de efeito). Conferir na RAM:
  `READ32(m + 0xF3F0 + 0x2E8) == 0`.
- A IA entra no TopSpin pelo `setAssBossState(ai, 12)` (só no `updateAssBossPatternTwo`), que faz `enterNewState(owner,
  owner + 0x10EBC)`. O `transitionOK` do TopSpin só olha as anims 163-165 (o ASSBOSS1 tem). **Entrar direto pelo
  `enterNewState` funciona com jogador.**
- O que quebra no `update` com jogador: (1) chama `setAssBossState` (0xE e 5) a cada contato, e o caso 0xE mexe no navegador do
  `Ai` e lê `retreatPts`; (2) o golpe "de verdade" (dano 15, empurrão 600/400, som, recuo de quem bloqueia) só acerta
  `game->m_monsters[0]`, que com o Goliath jogador 1 é ele mesmo. As IAs caem no ramo de "objeto": 5,1 de dano por quadro de
  contato, sem empurrão.

## 1. Botões → estados

- `GamePad::loadPadInputs` (0x13A020) grava os botões em bytes: 0 L1, 1 L2, 2 L3, 3 R1, 4 R2, 5 R3, **6 X, 7 bola, 8 triângulo,
  9 quadrado**, A Start, B Select, C..F direcional, 0x10..0x13 analógico direito.
- `PadFlags::interpretInputs` → `updateInput0ConfigCR` (0x178D58, config 0; as SR/1/2 seguem o mesmo padrão) monta o quadro
  do anel (`PadEntry`, 0x5E bytes, `Monster+0x5040`): `+0x2A` X (pulo), `+0x2C` triângulo, `+0x2E` bola (ação), `+0x30`
  quadrado. Combinações pelos contadores de histórico (`updateHistory0`, janela `tweak16C4`):
  - **triângulo+bola → `+0x36` (especial)**, e zera `+0x2C`/`+0x2E`;
  - **quadrado+X → `+0x38`**, e zera `+0x30`/`+0x2A` (por isso X+Quadrado não pula nem soca);
  - com `+0x36` ou `+0x38`, `setCurrentAction(0x14, 0xF)`.
- Não existe tabela de transições: cada estado tem seu `handleStateTransitions`, que testa em ordem o `transitionOK` dos
  candidatos (ex.: `JumpFlyBase::handleStateTransitions`, `MovementStates.cpp:1493`) e chama `enterNewState`. O CrowdControl
  está nas listas do Idle (0x14F5D0), da corrida (0x17613C) e do pulo/voo (no chão e no ar), depois do soco e do especial.
- Leitor do `+0x38`: só o `StateCrowdControl::transitionOK` (0x1C5FC0). Ele pede: `m_attacksEnabled` (+0x4A), não carregar
  objeto grande, anims 163/164/165 (`m_anims` em +0x1CF0, 0x10 cada: +0x2720/+0x2730/+0x2740), anim de efeito `+0x2E8`
  presente e parada, um teste só de IA (`m_playerNum` 2), `isSpecialAvailable` e o `+0x38` do quadro atual.
- Especial do Goliath: `StateCannonHands` (+0x10F60) pelo `+0x36` (triângulo+bola), sem dependência da IA.

## 2. Como a IA entra no TopSpin

- `Ai::updateAssBoss` (0x103C30) → `updateAssBossPatternTwo` (vida abaixo de `assBossTier2Health`), que chama
  `setAssBossState(this, 0xC)` em 0x105188 e 0x1055A8.
- `setAssBossState` (0x1037F0; `a0` = `Ai*` em `Monster+0x4E0`, `Ai+0` = o dono) grava `attackState` (0x6F7974) e zera
  `attackSuccessful`; switch de 15 casos (`jtbl_006E7720`): 0 Taunt (+0xFE20), 2 `tag` atrás do jogador 1, 3/10/13 `seek` no
  jogador 1, 4 recursão + `jump`, 5/7/9/14 recuo para o `retreatPts` mais longe do jogador 1 (`tag`), 6 CannonHands (+0x10F60),
  8 RamAttack (+0xFD40), **12 TopSpin**: desliga o navegador, `assBoss+0x470 = assBoss+0x460` (desreferencia o global),
  `transitionOK` e `enterNewState(owner, owner+0x10EBC)`, `D_006F8BB8 = 0`, `D_006F8B90 = 180`.
- Objeto do estado: **`Monster+0x10EBC`** (`StateTopSpin`, vtable 0x6F0EF0; `init` em 0x167330 para todo monstro). Campos do
  ctor: +0x1C/+0x24 100, +0x20/+0x48 150, +0x28 1,0 (velocidade), **+0x2C 900 (duração do giro em quadros de anim)**, +0x34 175,
  **+0x38 15 (dano)**, +0x3C 600 e +0x40 400 (empurrão), +0x4C anim corrente, +0x50 `HitHistory`.

## 3. O que no TopSpin depende da IA do chefe

| Função | Dependência | Com o Goliath jogador 1 |
|---|---|---|
| `transitionOK` 0x1CC340 | só anims 163-165 | OK |
| `transitionInto` 0x1CC378 | nenhuma (anim 163, 0x98 voltas = 900 / quadros da 164) | OK |
| `update` 0x1CC4A8 | `s1 = m_monsters[0]`; `hdSphereTest` raio 25; acerto com `id1 == s1->m_typeBits` → golpe cheio em `s1` e `setAssBossState(0xE)`; `s1` em ButtSlam a menos de 50 → `setAssBossState(5)`; outro acerto → `dispatchHit` 5,1 no objeto e, se `s1` a menos de 250, `setAssBossState(0xE)`; no fim, `disable` do navegador se `Ai+0x80+0x18` ≠ 0 e `enterNewState(Idle +0x7984)` | `s1` é ele mesmo: o golpe cheio nunca sai; toda batida chama `setAssBossState(0xE)` (navegador `tag` + `retreatPts` de canyon2, `attackState` = 0xE) |
| `handlePreemption` 0x1CCA78 | som do giro | OK |
| `acceptHit` 0x1CCAB0 | golpe tipo 7 aceito se `m_monsters[0]` a menos de 50 | aceita sempre (distância 0); menor |

O `Ai` de um jogador é construído (`Monster::__7Monster` chama `Ai::Ai`), então o navegador está zerado e o `disable` do fim não
roda. O `getAssBossState` (global) não quebra. O que precisa contornar: o `setAssBossState` e a troca de alvo do golpe.

## 4. Proposta de ganchos (`native/runtime/wotm_custom.inc`)

Endereços por nome (`wotm::fn::`): `transitionOK__17StateCrowdControl`, `enterNewState__7MonsterP12MonsterState`,
`setAssBossState__2Aii`, `update__12StateTopSpin`, `transitionOK__12StateTopSpin`, `isHoldingLarge__7Monster`;
`wotm::sym::assBoss`. Chamar funções do jogo pelo `call(...)` do `wotm_modes4.inc` (incluído antes). "Goliath jogador" =
`m_typeBits == 0x1A0 && isPlayerSlot(m)` (já existe).

**(a) X+Quadrado inicia o TopSpin** (o botão está livre: só o CrowdControl o lê, e ele nunca começa no Goliath).
1. Gancho no `StateCrowdControl::transitionOK` (`ctx->pc` = entrada; `m = READ32(a0 + 0xC)`). Se Goliath jogador: não chamar
   a original; `v0 = 1` se o quadro atual tem `+0x38` ≠ 0 (`e = m + 0x5040 + READ32(m+0x5040+0x16F0) * 0x5E`, `READ16(e+0x38)`),
   `READ8(m+0x4A)` ≠ 0, `call(isHoldingLarge, {m}) == 0`, `call(transitionOK TopSpin, {m+0x10EBC})` ≠ 0 e, para não girar no ar,
   `READ32(m+0x34)` igual a `m+0x7984` (Idle) ou `m+0x79AC` (Run) (o pulo chama no chão e no ar; liberar no ar é escolha). Senão
   `v0 = 0`. `ctx->pc = ra`. Outro monstro: original.
2. Gancho no `Monster::enterNewState`: se `a1 == a0 + 0xF3F0` e `a0` é Goliath jogador, trocar `a1` por `a0 + 0x10EBC` e chamar a
   original. (Opcional, antes: `WRITE32(a0+0x10EBC+0x2C, float 240)` para um giro mais curto; o objeto é só desse monstro. A
   paridade com a IA, `owner+0x470 = owner+0x460`, fica de fora até saber o que é.)

**(b) Neutralizar o `setAssBossState` fora do chefe.** Gancho na entrada: `owner = READ32(a0)`; se `READ32(sym::assBoss) == 0`
ou `owner != READ32(sym::assBoss)`, voltar sem fazer nada (`ctx->pc = ra`; a função é `void`). Na aventura o chefe é o
`assBoss` e passa intacto; com `WOTM_AIVSAI` o `initAssBoss` grava o `assBoss` e a IA também passa.

**(c) Golpe nas IAs (recomendado).** Gancho no `StateTopSpin::update`: se o dono é Goliath jogador, durante a chamada trocar
`game+0x120380` (`m_monsters[0]`) pela IA viva mais próxima (`m_playerNum == 2`, não morta) e restaurar na saída. Assim o ramo
original acerta a IA com dano 15, empurrão, som e recuo quando ela bloqueia. O `update` do `MonsterState`, o `updateTurn`, o
`updateMove` e o `updateLock` não leem `m_monsters[0]`; o `takeHit` da vítima (não 0x1A0) também não. As outras IAs continuam
no ramo de 5,1 por quadro. Opcional: a mesma troca no `acceptHit`.

**Testar:** `WOTM_MATCH=13:1,1` com uma IA, X+Quadrado parado e correndo; log de cada gancho; giro dirigido pelo analógico
(`updateTurn/updateMove(true)`); fim no Idle; dano e empurrão na IA. Depois, a aventura em canyon2 (fase 3, terceira parte da
luta) para ver o giro do chefe igual.
