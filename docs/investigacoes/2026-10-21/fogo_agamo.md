# Fogo da cabeça do Agamo (ROCK): de onde vem e como reproduzir sem o jogo

Lido em 2026-10-21. Fontes: `src/common/particle.cpp` (TU inteiro em C++, equivalentes "untuned"), `src/common/hier.cpp`
(`hierParticleNode`), `src/common/mathf.cpp`, `src/game/db.cpp` (`dbProcInteractive`), a travessia `asm/common/texm.s`, os `.NGP`
de `disc/MON/` e `disc/LVL/` e o ELF (`disc/SCUS_971.97`). Conferido num dump de luta com o Agamo na Ilha (retail, PCSX2:
`retail.bin` de 2026-10-11 no scratchpad de uma sessão antiga, fora do repo; sistema `fx[81]`).
Legenda: **[C]** confirmado (código + dados ou dump), **[H]** hipótese.

## 1. Quem cria e onde fica preso

- **Não é código de monstro.** O fogo é um nó `PARTICLE_EMITTER` (opcode 22, `HierParticleEmitter`, 176 bytes) dentro do próprio
  `ROCKn.NGP`. A travessia (`hierTraverseAsm`) chama `hierParticleNode(nó, view, índiceDaMatriz)` quando passa por ele; na primeira
  passada (`flag.isActivated == 0`) ela chama `particleCreateModeledFx(&posMundo, nó)` → `particleInitFx(nó.particleEnum, escala)`
  e liga `isActivated`. **[C]**
- Emissores achados no `ROCK0.NGP` (iguais em ROCK1/2/3, mesmos endereços na base 0xA00000):

| nó | preset | pai imediato | caminho | canal de visibilidade |
|---|---|---|---|---|
| `0xA001A0` | **141 `IdolHeadFlame`** | `TRANSLATE 0xA00250` t=(0, −0,269, −1,407) | … → `ANIM_XFORM 0xA00750` (cabeça, mIdx 3) → `ANIM_XFORM 0xA00270` (mIdx 5) → TRANSLATE | canal **24** (`vidx` em +0x10 do ANIM_XFORM 0xA00270) |
| `0xA00340` | 45 `AttachedFlames` | `TRANSLATE 0xA003F0` (mesmo t) | cabeça → `ANIM_XFORM 0xA00410` (mIdx 6) | canal 25 (0 no idle) |
| `0xA05800` | 141 | `TRANSLATE 0xA058B0` | `ANIM_XFORM 0xA07A40`/`0xA058D0` (mIdx 27/28) | canais 122/123 (0 no idle: é a cabeça que voa no especial) |

  A cadeia da cabeça: `SKEL_BONE 0xA057B0` → `ANIM_XFORM 0xA056D0` (mIdx 0, t=(0; 3,289; 62,222)) → `0xA04D80` (mIdx 1) →
  `0xA04B70` (mIdx 2) → `0xA00750` (mIdx 3, a cabeça) → `0xA00270` (mIdx 5, identidade, só visibilidade) → `TRANSLATE 0xA00250` → emissor. **[C]**
- **O fogo da cabeça é o preset 141 (`IdolHeadFlame`), não o 44/45.** No idle (0x5A) e nas variantes 0x5F/0x60 o canal 24 vale 1 em
  todos os quadros e o 25 vale 0. Muitas animações zeram o 24, inteiras (ids 5, 36..38, 150..153, 174, 209, 210…) ou por trechos (83, 84, 120, 131, 192, 267); a 192 (0xC0) liga o 25 e a 131 liga o 122. **[C]**
  Com o canal 0 a subárvore não é percorrida, `cachedPosition.w` não volta a 1 e o emissor para (ver "handshake" abaixo); as
  partículas vivas terminam a vida. **[C]** (o pulo de subárvore com vis 0 é o que o `pose.py` faz, conferido contra dumps.)
- Campos do nó 141 no disco: `particleEnum`=141, `size`=0 (escala 1), `velocity`=0, `position`=(0; −3,37502; 6,8606),
  `spread`=0, `emissionHeading`=0, `emissionPitch`=1,5708 (π/2), `emissionRate`=0, `duration`=0, `onsetDelay`=0, `particleLifetime`=0,
  `gravity`=0, `airResistance`=0, expansões 0, `flags`=0xF (`isMoving|isFading|isFluctuating|isAutoStart`). Como os campos são 0,
  `particleCreateModeledFx` não sobrescreve nada da tabela; só posição, normal e o modo "preso". **[C]**
- **ROCK3**: tem os mesmos três emissores nos mesmos nós (e um quarto, 141 com size 4, sob a raiz 1, `GROUP` id 5021, faixa de
  pickups 0x12C0..0x13EC). O idle do ROCK3 também põe o canal 24 em 1. Pelos dados, o traje 3 **também** teria o fogo; não achei
  código que o desligue. **[H, em aberto]**: conferir no jogo. Se o traje 3 de fato não tem fogo, a causa não está no `.NGP` nem na animação.

### Fixação (matriz)
- `isMoving`: `parentVec = &nó.cachedPosition`, `parentMat = nó.cachedOrientation`, `StaticVec = nó.position`; `emit.info.attribs |= 0x800200`.
  A cada travessia `hierParticleNode` copia a matriz do topo da pilha (`matStack[idx]`, rotação no mundo do nó TRANSLATE pai) para
  `cachedOrientation` e grava `cachedPosition` = origem do nó no mundo, com `w = 1`. **[C]** pelo código; que seja a origem do TRANSLATE
  0xA00250 no mundo é **[H]** forte (bate com o dump: as linhas da `parentMat` têm módulo 1 e a linha 2 aponta para cima da cabeça).
- `utilUpdateEmitterMatrices` (vetores-linha, como no resto do jogo): `ParticleToEmitVec = parentVec + StaticVec·parentMat`
  (`t = x·M[0] + y·M[1] + z·M[2]`), `ParticleToEmitMat = StaticMat·parentMat`. Conferido no dump: (−743,1; −1302; 75,65) + StaticVec·M
  dá (−746,4; −1304,9; 81,9) contra (−746,3; −1305; 81,66) gravado (diferença de um quadro). **[C]**
- `StaticMat = mathfMatrixFromNormal(n)` com `n = (cos p·sin h, cos p·cos h, sin p)` = (0, ~0, 1): **linha 1 = normal**; com n vertical
  a função põe linha 0 = (1,0,0), linha 2 = (0, −1, ~0). No dump: StaticMat = [(1,0,0), (0,7,6e−8,1), (0,−1,7,6e−8)]. **[C]**
  Ou seja, no espaço do nó: linha 0 = +X local, linha 1 = +Z local (normal, "para cima da cabeça"), linha 2 = −Y local.
- Handshake (`attribs & 0x800000`, em `particleEmit`): só emite se `parentVec.w == 1` (e zera); senão `onsetDelay = 1` e sai. Na prévia:
  emita só nos quadros em que o canal 24 ≥ 1 (o nó foi percorrido). **[C]**

## 2. Parâmetros do preset 141 `IdolHeadFlame` (ELF; depois do `particleInit`)

Tabelas: `VisualInfo` 0x5ADD00 + 32·141 = **0x5AEEA0**, `FieldInfo` 0x5AEFF0 + 80·141 = **0x5B1C00**, `EmitterInfo` 0x5B2020 + 80·141 =
**0x5B4C30**, `ParticleInfo` 0x5B4F70 + 96·141 = **0x5B8450**. Todos os valores abaixo batem com o `fx[81]` do dump. **[C]**

- **Visual**: `attribs` = 3 → **sprite** (não texSprite); `texInfoIndex` = 8 = **`Fireball`**; `numColors` = 16, `colorIndex` = 608;
  `alphaContext` = **0x48 (aditivo: Cs·As + Cd)**; `priority` 0; `lightColor` 0 (sem luz pontual). `texMask` vira `numTextures` = 1.
- **Field**: `attribs` = 0x420 (0x400 = vento com rajadas `Gusts`, 0x20 = arrasto linear em |v|); `gravity` 0; `airResistance` = 0,0390625;
  `dragPhaseIn` = 5 (`dragPhaseInFactor` = 0,2); `wind` = (0, 0, **3,2**, w = 1).
- **Emitter**: `attribs` = 0x1000163 (+0x800200 do modo preso; 0x100 cai depois da 1ª atualização); `onsetDelay` 1; `duration` 0
  (contínuo, bit 2 apagado); `preAgeFactor` 0,5; `rate` = **2**; `spread` 180 na tabela → `particleInit` converte (bit 4 apagado) para
  `(1 − cos(90°))/2` = **0,5** (hemisfério); `normalScale` = **0**; `minVel` = `maxVel` = **−1**; `velocityInherit` 0,8; `normOffsScale` 0;
  `minOffset` = `maxOffset` = **5**; `lifetimeToLightDuration` 0,8.
- **Particle**: `attribs` 0 (0x400 liga em tempo de execução: pacote montado); `minSize` **7**, `maxSize` **15**; `initExpansionRate` **1**,
  `expansionRate` **−0,5**, `expansionRateTransition` **10**; `lifetime` **20** fields; `recipLifetime` 0,05;
  `colorsPerFrame` = ((16−1) << 12) / 20 = **3072**; `maxRadius` 0 (sprite).
- **Cores** (`0x256C10 + 4·608`, u32 little-endian = R,G,B,A em bytes, 0x80 = 1,0):
  `00808080 28808080 4b808080 66808080 80808080 80808080 7d808080 72808080 64808080 54838180 457d7f80 37687580 284b6880 19355f80 0c21537d 01124276`
  (alfa sobe de 0 a 1 nas 4 primeiras, fica em 1, cai até ~0; RGB branco que esfria para laranja-escuro no fim). Índice pela idade:
  `t = age·3072` (4.12) → `cor[t>>12]`…`cor[(t>>12)+1]` por `(t & 0xFFF)/4096`, ou seja, 0,75 cor por field.
- Para comparação, o 45 `AttachedFlames` (o outro emissor, desligado no idle): sprite, `TexFlames` (11), mesmas cores, aditivo, tamanho 3,
  vida 40 na tabela mas o nó do disco põe `particleLifetime` = 1 (recalcula `colorsPerFrame` = 61440, como no dump), `size` 2,5, `velocity` 10, `rate` 60.

## 3. Textura

- `particleParseTexture(idx, nó)` é chamada pelo `dbProcInteractive` (`src/game/db.cpp`) para todo nó com `id1` em **0x1B58..0x1C1F**:
  `idx = id1 − 0x1B58` é o `FxTextureId`. Se o nó é GROUP, usa o filho 0 (OBJECT): copia `gsTexCtx[0].tex0` para `TEX0[texIndex]`, `texId[0]`
  para `texId[texIndex]`, e soma 1 em `TextureInfo[idx].numTextures` (vários nós com o mesmo id = quadros de animação, com `id2` = número
  do quadro). **[C]**
- **Esses nós estão no `.NGP` da fase, não no do monstro nem no do menu.** Todas as 19 fases de `disc/LVL/` têm o `Fireball` (id1 = 0x1B60):
  um OBJECT **T8 32×32**, `texId` 5 (ISLAND: 6; CANYON1: 4), `TEX0` = 0x200001A555304000 (TW = TH = 5, PSM 0x13, CBP 0xD). `SHELL/*.NGP`,
  `MON/*.NGP` e `PRESHELL/LOAD/UI` não têm nenhum nó nessa faixa. **[C]**
  Ex.: `disc/LVL/CENTRAL.NGP`, nó GROUP `0x10EE5A0` → OBJECT `0x10EE450`; pixels no `CENTRAL.TEX` (pacote texId 5, offset 0x3280, T8 32×32),
  paleta no `CENTRAL.RTX` (entrada CT32 16×16 com tbp 13, offset 0xC80). `python tools/tex.py export disc/LVL/CENTRAL.TEX <saída>` gera
  o PNG (`central_t0005.png`: bola de fogo laranja com alfa). O dump da Ilha confirma `TextureInfo[8]` = {n 1, texIndex 41, du = dv = 512}
  e `texId[41]` = 6 (ISLAND). **[C]**
- Para a prévia: carregar uma fase só para a textura é caro. Saída prática: decodificar só esse pacote de um `.TEX`/`.RTX` de fase
  (CENTRAL: texId 5 + paleta tbp 13) uma vez. A rotina de achar o nó (id1 = 0x1B58 + 8, filho 0, `texId[0]`, CBP) serve para qualquer fase.
- Sprite (não texSprite): um quadro só; os 2 bits baixos de `pos.w` (sorteados na emissão, `mask` = 3) escolhem o espelhamento de UV
  (3 normal, 1 espelha U, 2 espelha V, 0 os dois; `docs/FORMATOS.md`). Cor final: textura MODULATE pela cor da tabela (0x80 = 1,0), mistura aditiva.

## 4. Emissão e movimento (por field; na luta 99,8% dos quadros têm 1 field)

Ordem por quadro: a travessia atualiza `cachedPosition`/`cachedOrientation` (w = 1) → `particleMove` move e envelhece as vivas → no
fim do mesmo laço `particleEmit` cria as novas (que não andam neste quadro). **[C]**

**Emissão** (`particleEmit`, `fF` = fields do quadro):
1. `prev = E; E = parentVec + StaticVec·parentMat` (posição do emissor); `R = StaticMat·parentMat` (R0, R1 = normal, R2);
   `disp = E − prev`; `inh = disp · 0,8 / fF`; `vento = (0,0,3,2) + Gusts·1`; `rel = inh − vento`.
2. Quantidade (bit 0x1000000, **por chamada, sem ×fF**): `acc += max(2, |disp|·4/15)`; `n = floor(acc)`; `acc −= n`. Com o monstro
   parado, **2 por quadro**. Máximo 256 vivas.
3. Para cada nova `i` (k = 1..n): `frac = k/n`, `remain = fF·(1 − k/n)`;
   `dir = mathfRandVector(0,5)`: `y` uniforme em [0, 1) (ao longo da normal), `x, z` num círculo de raio `r = √(1 − y²)`
   (aproximação quadrática de seno/cosseno, quadrante por 2 bits); `u = dir.x·R0 + dir.z·R2` (o termo de R1 some porque `normalScale` = `normOffsScale` = 0).
   Então: `vel = rel + u·(−1)`; `pos = prev + u·5 + disp·frac − u·remain` (anel de raio até 5 em volta do emissor, no plano perpendicular à
   normal, com velocidade para dentro; `|u| = r`, enviesado para 1).
4. Tamanho: `pos.w = 7 + 8·U[0,1)` (com os 2 bits baixos sorteados para o espelhamento).
5. Pré-idade: `vel.w = 10·U[0,1)` (`lifetime·preAgeFactor`); `age = (int)vel.w`. Conferido no dump: as recém-nascidas têm `age` = parte
   inteira de `vel.w` (ex.: `vel.w` 6,169 → `age` 6) e as `vel.z` iniciais ≈ −3,2. **[C]** Média de vida efetiva ~15 fields → ~30 vivas
   (o dump tem 30). **[C]**

**Movimento** (`particleMove`, por partícula, `airF` = 0,0390625·fF):
1. `age += fF`; se `age ≥ 20`, morre.
2. Arrasto com o `|v|` **antes** de aplicar: `den = min(1 + airF·|v|, 2)`; se `(int)age < 5`: `s = 1 − 0,2·age + 0,2·age/den`, senão `s = 1/den`;
   `v.xyz *= s`.
3. `pos.xyz += vento·fF + v·fF` (o vento soma na posição, não na velocidade; como a velocidade nasce com −vento e o arrasto a gasta,
   a partícula nasce parada em relação ao emissor e acelera para cima até ~3,2/field). Gravidade 0.
4. `pos.w += fF·(1 − 1,5·min(age/10, 1))` (cresce +1/field no começo, encolhe −0,5/field a partir de 10 fields), mantendo os bits baixos.
5. Cor/alfa pela `age` (tabela acima). `pos.w` é o **meio-lado** do sprite em unidades de mundo (o microcódigo limita o meio-lado na tela a foco/2).

`Gusts` (0x5AEFE0) é o vento global; no dump valia (0,053; 0,047; 0). Na prévia, 0 ou um valor fixo pequeno.

## O que falta / riscos
- ROCK3 sem fogo: não explicado pelos dados (acima).
- `parentVec` = origem do TRANSLATE pai no mundo: hipótese forte, não conferida contra a pose do nativo no mesmo quadro do dump.
- As funções lidas são equivalentes "untuned" (sem difftest); a leitura bate com o dump nos pontos marcados [C].
- Escala do monstro no jogo: a `parentMat` do dump tem linhas de módulo 1, então não há escala no caminho; na prévia, se o modelo for
  desenhado escalado, escalar também `position`, offset (5), velocidades, vento e tamanhos.
