# Move list: campos dos especiais que faltavam (2026-10-26)

Continuação de `80b3802:docs/investigacoes/2026-10-25/move_list_especiais.md` (seção "Em aberto"). Pergunta: quem lê
os campos +0x44/+0x48 do `StateSonicRoar`, +0x3C/+0x40/+0x44 do `StateGrappleHook` e +0x0/+0x8 de cada anel do
`ShockWaveConfig`, e se algum deles vale uma frase na dica da MOVE LIST do modo treino
(`native/runtime/wotm_training.inc`, bloco "o especial do monstro").

Método: busca no `asm/` por leituras relativas à base do estado e pelos offsets absolutos no `Monster`
(+0xF738, +0xF73C, +0x1063C, +0x10640, +0x10644); leitura de todos os leitores do ponteiro do especial
(Monster +0x7980) e do `HitEvent` que cada estado monta; leitura completa de `ShockWave::update` (0x1B0BD8) e dos dois
`ShockWave::Activate` em `asm/game/ShockWave.s`.

## StateSonicRoar (Monster +0xF6F4), +0x44 e +0x48

- Escritos só pelo construtor `__14StateSonicRoar` (350 e 250) e pelo `.CHR` (chave 0xFA9, que copia +0x38/+0x3C da
  chave para +0x44/+0x48).
- Nenhuma função do estado os lê (`init`, `transitionOK`, `transitionInto`, `update`, `testConeCollis`,
  `handlePreemption`, `acceptHit`, `inWindUp`, `inWindDown`), nem por ponteiro derivado (`addiu` de +0x44 para passar
  adiante).
- Ninguém de fora os alcança: os offsets absolutos +0xF738/+0xF73C não aparecem no ELF; o `Monster` guarda o estado em
  +0x7980 (ponteiro do especial, montado no `initDynamics`), e todos os leitores desse ponteiro (`takeHit`,
  `handleLocator`, `inSpecialState`, `updateLock`, `updateReticle`, `StateIdle::update`, `StateRun/StateFly`,
  `JumpFlyBase`, `PadFlags::computeMotionRot`, `Camera::normalUpdate`, `AiBlockReflex`, `AiSpecialAttack`,
  `StateGrappled::transitionOK`) só comparam o ponteiro ou chamam métodos virtuais.
- O `HitEvent` que o `testConeCollis` manda (tipo 2, fonte 5, subtipo 0xC) sai do `HitEvent::create`, que zera os 0x40
  bytes do evento estático; as forças +0x34/+0x38 do evento (as que o `TopSpin` e a onda de choque preenchem) ficam em
  zero. A vítima derruba com os números padrão dela, não com 350/250.
- Conclusão: os dois campos têm cara de força de knockback (o `ZapAttack` usa o mesmo par 350/250 de verdade, em
  `Monster::knockBack(dir, +0x3C, +0x40)`), mas no SonicRoar são configuração morta: o código nunca os consome.

## StateGrappleHook (Monster +0x10600), +0x3C, +0x40 e +0x44

- +0x3C (350) e +0x40 (250): mesmo caso do SonicRoar. Escritos pelo construtor `__16StateGrappleHook` e pelo `.CHR`
  (chave 0xFC5, +0x28/+0x2C), sem leitor no estado, no `OgreMace` (a corrente), nem por +0x1063C/+0x10640 absolutos.
  O `HitEvent` do `testCollis` (tipo 3, ou 7 conforme o material atingido; fonte 5, subtipo 0x12) também não recebe
  forças. Configuração morta.
- +0x44 (5): é lido, em `update` e em `testCollis`, como o tempo de mistura (em frames) do
  `animationTransitionInto` quando o estado troca para a próxima animação da sequência (índice em +0x4C). É detalhe de
  animação, sem efeito visível para o jogador.

## ShockWaveConfig: +0x0 e +0x8 de cada anel

Layout como o `ShockWave::update` o usa (`ExpandingRing<ShockWaveConfig>`, copiado por valor para o slot do
`m_wavePool`, onde começa em +0x84): n em +0, anéis de 0x1C bytes a partir de +0x20, alcances em +0x250 + 4i,
`timeScale` em +0x2A0. A cada quadro o raio é (campos desde a ativação × `timeScale`), o teste é um `hdSphereTest`
com esse raio, e para cada contato o anel é escolhido pela distância ao centro (o primeiro alcance maior que ela).
Campos do anel que o `update` consome:

| campo | uso |
|---|---|
| +0x0 | só o do **anel 0** é lido, como tolerância vertical: se for > 0 e o \|Δz\| entre o ponto de contato e o centro da onda passar dele, o contato é ignorado. Os anéis seguintes não têm o +0x0 lido |
| +0x4 | dano (argumento do `takeHit` virtual do atingido, monstro ou cenário) |
| +0x8 | **não é lido** por `ShockWave::update` nem pelos `Activate` (só copiado) |
| +0xC | vai para `HitEvent` +0x38 (força) |
| +0x10 | vai para `HitEvent` +0x34 (força) |
| +0x14 | vai para `HitEvent` +0x3C; se passar de 100 o tipo do evento vira 7 (atordoa) |
| +0x18 | escrito no estado de Recoil do monstro atingido (Monster +0x7E30, campo +0x4C) |

O tipo do evento é 2 (derruba) se +0xC ou +0x10 do anel for diferente de zero, senão 1 (só recoil). O `update` também
limita a um acerto por alvo por onda (`HitHistory`), com exceção para monstro bloqueando.

Valores: a tolerância vertical é sempre zero nos construtores (o `__17StateCrowdControl` grava zero explícito no
+0x0 do anel 0; `RockSpecial` e `RobotSpecial` não gravam o campo), e nenhuma das chaves de `.CHR` da tabela da
investigação anterior aponta para ele no CrowdControl e no RockSpecial. Para o RobotSpecial a chave 0xFAB mexe na
região logo antes dos anéis; vale conferir na RAM se o +0x0 do anel 0 dele fica diferente de zero.

Consequência para a dica: o filtro de altura existe mas está desligado nos especiais conhecidos, e o +0x8 é morto.
Útil de verdade: o último anel do CrowdControl (dano 2, forças 0/0) não derruba, só faz recoil.

## O que entra na MOVE LIST

| campo | quem lê | o que é | dica |
|---|---|---|---|
| SonicRoar +0x44/+0x48 (350/250) | ninguém (construtor e `.CHR` só escrevem) | knockback morto | não vale mostrar |
| GrappleHook +0x3C/+0x40 (350/250) | ninguém | knockback morto | não vale mostrar |
| GrappleHook +0x44 (5) | `update`, `testCollis` | frames de mistura da animação seguinte | não vale mostrar |
| anel +0x0 | `ShockWave::update`, só o do anel 0 | tolerância vertical do contato (0 = desligada) | só se > 0 na RAM: "MISSES TARGETS MORE THAN %.0f ABOVE OR BELOW" (hoje sempre 0: não mostrar) |
| anel +0x8 | ninguém | morto | não vale mostrar |
| anel +0xC/+0x10 (bônus) | `ShockWave::update` | forças do knockback; as duas em 0 = o anel só faz recoil | se o último anel tiver as duas em 0: "THE OUTER EDGE ONLY STAGGERS" |
