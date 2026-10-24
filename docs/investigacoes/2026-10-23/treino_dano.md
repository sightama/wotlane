# Modo treino: volumes de quem recebe o golpe, dano e boneco de treino (2026-10-23)

Investigação para o item 9 do `POS_FRONTEIRA.md` (lado de quem **recebe**; as hitboxes de ataque estão em outro relatório). Nada
foi editado. Fontes: `src/game/Monster.cpp` (`takeHit`, `takeDamage`, `update`), `src/game/MonsterMeters.cpp`, `src/common/hd*.cpp`,
m2c de `game/collision` e `collisTestCloseRangeAttack`, `native/runtime/wotm_overrides.cpp` e `wotm_custom.inc`, dados do disco
(`disc/MON/*0.NGP`, `disc/LVL/TRAINING.*`, listas de fases do ELF). Endereços em `native/wotm_syms.h` (`wotm::fn::`/`wotm::sym::`).

## 1. Volumes do corpo do monstro

O monstro tem **três volumes diferentes**, cada um para uma coisa:

| Volume | Forma | Onde | Usado por |
|---|---|---|---|
| **Hurtbox** (o que os golpes acertam) | casco convexo de 6 planos (uma caixa, às vezes inclinada), nó op 10 `COLLISION_VOLUME` no hier do monstro, filho de um `ANIM_XFORM` com `bits.hasColl` | arquivo do monstro (`MON/*.NGP`) | `hdSphereTest`/`hdLosTest` dos ataques, armas, projéteis (todo `dispatchHit` que sai de um `_hdResult`) |
| **Cápsula do corpo** (empurra monstro contra monstro) | cápsula `p + eixo·[0, len]`, raio r | `Monster` +0x3F0 (ponto no espaço do cs), +0x400 (len), +0x404 (raio) | `collisResolveCsToCsCollisions` → `hdCapsuleVsCapsule` (também passa fogo/choque por contato) |
| **Esferas do cenário** (andar, paredes, chão) | até 11 esferas `{x, y, z, r}` no espaço do cs | `Monster` +0x2B0, contagem +0x360; conjunto alternativo +0x370 (3 esferas de r 17 em z 37/60/83), contagem +0x3A0 | `collisTestEnvironmentCollisions` → `hdBundledSphereTest` |

### Hurtbox (casco no hier)
- O ataque corpo a corpo (`PunchSwipeConfigBase::collisTestCloseRangeAttack`, 0x14EA90) varre uma esfera do ponto de contato do
  atacante pela grade (`hdSphereTest`, pulando o próprio cs pelo `skipId2`). A grade devolve os cs registrados nela
  (`hdReparentCsGrid` com raio `Monster`+0x3A4, 63 na maioria) e o `hdTraverseHier` desce o hier de cada um até os nós de colisão
  (op 10, 18 BSP, 42 poly). Num `ANIM_XFORM` só desce se `bits.hasColl` (+0xC2 bit 2) e visível, usando a matriz da animação
  (`updateAnimTransMatrix` = a mesma `animOutput.atMat[matrixIdx]` que o desenho nativo já lê). O `_hdResult.ids` traz o id do
  interactive (`m_id`, +0x20 do Monster), o `Interactives::getInteractive` acha o monstro (tipo 1) e o `dispatchHit` chama o
  `takeHit` virtual.
- Nó op 10 (`_hiercollisionvolume`, `include/hieri_types.h`): `numPlanes` +4, `sphereCenter` +0x10, `sphereRadius` +0x1C, planos
  `{n.xyz, d}` de 16 bytes a partir de +0x20 (dentro: `n·p <= d`). Hulls achados nos arquivos (script de rascunho, só leitura):

| Arquivo | Hulls | Esfera envolvente (centro z, r) | Obs. |
|---|---|---|---|
| MONKEY0 (Congar) | 1 | 47,7 / 89,2 | caixa alinhada, z −14..109, x ±51, y ±39 |
| ROBOT0 | 1 | 52,8 / 82,1 | z −6..112 |
| LIZARD0 | 2 | 45 e 60 | dois cascos sob o mesmo xform |
| MANTIS0 | 3 | — | 1 no corpo + 2 sem xform (provável peça/arma) |
| ROCK0 | 2 | — | 1 no corpo + 1 sem xform |
| ALIEN, DRAGON, ENERGY, FINAL, JELLY, LAVA, SHOGUN | 1 | — | JELLY: caixa grande (r 201), centro em z −83 |
| ASSBOSS0 (Goliath) | 1? | — | leitura incoerente (0 planos): o chefe tem cascos próprios, conferir ao vivo |

  Ou seja: **uma caixa rígida presa a um osso** (o `ANIM_XFORM` raiz do corpo, com 2 a 12 filhos), que segue o quadril na animação
  mas não se deforma; braços, cauda e cabeça ficam fora dela.
- **Como desenhar em mundo:** no `Scene::walk` do nativo (`native/wotm_native.hpp`), o caso 17 já monta `mul(local, m)` com o
  `atMat`; o GROUP (caso 1) só desce grupos visuais (`flags & 1`). Para o treino: com a opção ligada, descer também grupos com
  `flags & 2` (colisão) dentro do cs dos monstros e, no op 10, guardar `(matriz m, planos)` numa lista da cena. Vértices da caixa =
  interseção de cada trio de planos que fica dentro dos outros três (6 planos → 8 vértices); arestas = pares de vértices que
  compartilham dois planos. Barato: 1 a 3 cascos por monstro. Alternativa grosseira: a esfera `sphereCenter/sphereRadius`.
  O caminho do jogo para conferir é `hdInitTraverseHier`(0x202888)+`hdTraverseHier`(0x2022C8) (matrizes mundo→local).

### Cápsula do corpo (monstro × monstro)
- `collisResolveCsToCsCollisions` (0x12D860), para cada outro monstro com `cs.testCollision` (+0xD): ponto base =
  `cs.mat`(linhas +0x20/+0x30/+0x40)·(`Monster`+0x3F0) + `cs.trans`(+0x10); eixo = linha 2 da matriz (`cs`+0x40, o z local, para
  cima); `len` = +0x400; raio = +0x404. Valores fixos por tipo em `collisInitPoints` (0x12B158), p.ex. padrão `len 30, r 50`; tipo
  0x1A0 (Goliath) `145/90`; 0x1C0 `170/102`; 0x1E0 `90/50`.
- Desenho: `DrawCapsuleWires(p, p + eixo·len, r)` lendo os quatro campos e o cs a cada quadro (thread da janela pela captura, como
  o resto da cena).

### Esferas do cenário
- `collisInitPoints` preenche +0x2B0.. por tipo (`m_typeBits` +0x14): em geral 2 esferas no eixo z (p.ex. `{0,5,45,45}` e
  `{0,5,70,45}`), 1 a 3 nos maiores. O conjunto +0x370 vale quando o estado atual tem a flag 0x10 (estados de reação: arremessado,
  etc.) e `estado+0x260` não é 4 nem 5. Em mundo: `cs.mat·{x,y,z} + cs.trans`, raio w. Opcional no treino (não é hurtbox).

## 2. Dano: de `takeHit` até a vida

### Onde está a vida
- `Monster::m_maxHealth` +0x448 e `m_health` +0x44C são os dois primeiros floats do `HealthMeter` embutido (+0x448: `max`, `cur`,
  `recent[3]` +0x450, `idx` +0x45C). `init` põe `max = 100`; o valor por monstro vem dos dados do personagem depois (o lote de
  2026-10-17 mostra 117 no Togera; a origem exata do 0x448 não achei: hipótese é a tabela de tweaks do `charreset`/`MonsterTweaks`,
  ler em tempo de execução). `reset` (na inicialização) enche.
- `HealthMeter::drain(n)` (0x16D2D8): **valor absoluto**, guarda `n` em `recent[]`, prende em 0. `credit(n)` soma **n % do máximo**
  (power-up de vida); `creditFull` (0x16D278) enche.
- Stamina/fúria é outro medidor (`StaminaMeter` +0x460: max +0, cur +0x10, exhausted +0x20).

### Caminho
1. Atacante monta o `HitEvent::s_takeHitInfo` (0x7CD528: `pos*` +0, `result*` +4, `type` +0xC, `source` +0x10, `subtype` +0x14,
   `sourceKind` +0x18, `+0x1C` id do golpe, `+0x20` tipo do atacante, `+0x24` id do atacante, forças +0x34/+0x38) e chama
   `Interactives::dispatchHit(dir, dmg, attackerId, targetId)` (0x147240) → `takeHit` virtual.
2. **`Monster::takeHit(dir, dmg, attackerId)`** (0x15F048; registradores: `a0` vítima, `a1` dir, `f12` dano base, `a2` id do
   atacante; decompilado em `Monster.cpp`):
   - o estado atual pode recusar/absorver o golpe (virtual vt+0x34, só com `m_unk49` +0x49); se recusar, só dá dano quando o estado
     é o `m_stateRef` (+0x7980) e ele aceita;
   - Goliath (0x1A0): só com `attackState` 4/10 ou vindo do jogador 1 (`wotm_custom.inc` já contorna);
   - dodgeball (modo 7): só `type` 8 passa; bloqueio contra atordoamento (`type` 7) segurando algo é anulado;
   - **modificadores do atacante:** `m_boostFull` (+0xF4) × `m_boostDamageMod` (+0x48C); fúria `m_unkF5` (+0xF5) ×
     `m_rageDamageMod` (+0x49C); `m_unkEC` (+0xEC) do atacante → dano 1000;
   - vítima morta (`m_dead` +0xE8) → sai;
   - **bloqueio** (`isBlocking` e `StateBlock::isBlockable`, `type` ≠ 7): segurando pickup, o pickup leva o dano (pode quebrar) e a
     vítima 0; com alvo agarrado (`m_target` +0x68B4) o agarrado leva; senão `× m_damageModifier` (+0x69A8), que o
     `StateBlock::chooseBlock` põe em `damageScale` (StateBlock +0x20) ou `damageScaleArmed` (+0x24) — tweak do monstro;
   - `type` escolhe a reação (1 recuo, 2 bloqueável/empurrão, 3 grande, 5 choque, 7 atordoa, 8 só empurra, 10 queima) e no fim
     **`takeDamage(dano, false, atacante)`**; golpe corpo a corpo (`source` 0xB) dá stamina ao atacante.
3. **`Monster::takeDamage(dano, force, source)`** (0x15FE40, `f12` dano, `a1` force, `a2` Monster* ou 0): ignora no Crush antes da
   morte súbita, com `game+0x120464`, fonte morta, estado 0x40, sem `force` e `m_unk49` 0, **`m_godMode` (+0xE9) ou `m_unkEA`
   (+0xEA)**, jogador 1 em filme/contagem; dodgeball conta 3 acertos = 1000. **Dificuldade:** fonte IA (`m_playerNum` 2) ×
   `Ai+0x152C` (= `Monster`+0x1A0C), que o `Ai::init` põe em 1,0 no fácil/normal e, no difícil (`game+0x1203CC` = 2), 1,15 (1,3
   para os tipos 13/14/15). Depois `drain`, `m_hitFlash`, e com vida ≤ 0 `blowUpMonster(source)` (0x160440).
4. Dano que **não passa** pelo `takeHit`: fogo (`updateOnFire`, 0x1626B0, `force`), choque (`updateBeingShocked`, 0x1627C8),
   batida no cenário de quem foi arremessado (`collisTestEnvironmentCollisions` 0x12BC70, `StateThrowBack::handleDamage`
   0x1D59C8), "vencer a luta" do nativo. O `ra` da chamada ao `takeDamage` diz qual.

### Melhor ponto para o nativo registrar "golpe X tirou N"
- **Embrulhar `takeHit`** (entrada): guardar contexto do golpe atual — vítima, atacante (`s_dbInteractive[a2]` 0x7D3620 quando o
  tipo é 1; id do jogador por `m_monsters[]`), dano base `f12`, `type`/`source`/`subtype`/`+0x1C` do `s_takeHitInfo`, ponto
  `pos`, `isBlocking`, vida antes. Na saída: vida depois → evento completo (base, aplicado, bloqueado sim/não, reação).
- **Embrulhar `takeDamage`**: `f12` na entrada é o dano depois dos modificadores do atacante e do bloqueio (falta só a
  dificuldade); `m_health` antes/depois é o exato. Se estiver dentro do `takeHit` em curso, anexa ao evento; senão é evento
  "contínuo" (fogo/choque, somar por segundo) ou de cenário, classificado pelo `ra`.
- Hoje o `WOTM_DAMAGEPOP` mede a vida na entrada do `Monster::update` e só pega o tipo do `takeHit` (`wotm_overrides.cpp`,
  `takeHitDamage`/`monsterUpdateDamage`): serve para o número na tela, mas junta golpes do mesmo quadro e não sabe base nem
  bloqueio. O treino deve usar os dois embrulhos acima (e pode alimentar o mesmo `wotmDamagePopup`).
- Já há três ganchos no `takeHit` (modo deus, Goliath custom, números de dano) e um no `takeDamage` (invencível): o novo encadeia
  pelo `lookupFunction` como eles; instalar depois das trapaças (como o `DAMAGEPOP`) para registrar o golpe recusado pelo deus
  também, se interessar.

## 3. Modo deus, boneco parado e vida infinita

- **Já existe** (`NATIVO.md`, menu F1; `wotm_overrides.cpp`): invencível (`takeDamage` volta na entrada no jogador), modo deus
  (`m_godMode` do jogo + recusa de `takeHit`/`knockBack`/`takeAdditiveRecoil` e dos estados de reação), vencer a luta
  (`takeDamage(100000, force, jogador)` nas IAs). Só valem para jogadores (`isPlayerMonster`).
- **Boneco parado, sem IA, tomando dano:** partida com 1 IA (`WOTM_MATCH=<jog>,<ia>,<fase>`). Na entrada do `Monster::update` do
  boneco, zerar `m_unkF9` (+0xF9, `setPadEnabled`): o `update` então só limpa o `GamePad` e não chama `Ai::updateInputs`; os
  estados continuam rodando, então recua, cai, é agarrado e arremessado normalmente. Reescrever a cada quadro (o jogo põe 1 na
  vitória e no `initAfterDbLoad`). Variantes: gancho no `Ai::updateInputs` (0x101788) que escreve um padrão fixo no `GamePad`
  (+0x5024) em vez da IA (boneco que **bloqueia sempre**, pula, agacha), ou a IA normal (CPU), ou o segundo controle
  (`WOTM_PLAYERS=2`, já existe).
- **Vida infinita/recarga do boneco:** não usar `m_godMode`/`m_unkEA` (o `takeDamage` sai antes do `drain`: some o número). Opções:
  (a) gancho no `blowUpMonster` (chamado só pelo `takeDamage` e pelo `collisResolveEnvCollis`): no boneco, quando vier do
  `takeDamage` (pelo `ra`), pôr `m_health = m_maxHealth` e voltar sem morrer; (b) recarga estilo jogo de luta: na entrada do
  `Monster::update`, sem golpe há N quadros (ou vida abaixo de X), `m_health = m_maxHealth` (o `HealthMeter::creditFull`). As duas
  juntas: nunca morre e volta cheio depois do combo. Na morte por cenário (`collisResolveEnvCollis`, queda fora do mapa) deixar o caminho normal.
- **Jogador:** as trapaças existentes (inv/deus/stamina/especial) bastam; a stamina cheia ajuda a testar golpes caros.

## 4. A fase "training"

- `m_levelNum` 12 = `training` (`LEVELS.TXT` linha 14; `GameLevelLongNames[12]` "Training"); arquivos `LVL/TRAINING.{NGP,PTR,RTX,TEX}`,
  `PTS/TRAINING.{AIP,PUP,SPT}`, `RST/TRAINING.MRS`. NGP de 62 KB (o Centro tem 2,4 MB; o dodgeball 141 KB): 259 OBJECTs, uma
  grade de colisão com 1 grupo de polígonos, sem cascos; 3 pontos de partida no `.SPT`, malha de IA mínima (`.AIP` 136 bytes).
- **Não está em nenhuma lista dos menus** (lidas do ELF): 1P/MP free for all = 1–11, 14, 15; eliminação = 3, 5, 9, 10, 14, 15;
  endurance = 1, 3, 5, 9, 2, 14, 10, 11, 15. É uma arena pequena da equipe, sem uso no jogo final.
- **O tutorial do jogo é outro:** caixas de texto (`Hud::addTextBoxMessage`) na aventura (modo 1), primeira fase (agarrão 0x16/0x1D,
  0x1A, 0x1C no Centro, power-ups 0x18...). Não há modo treino próprio.
- **Serve de fase?** Provavelmente sim, como arena vazia e leve (bom para ver volumes sem prédios na frente), mas ninguém a carregou
  ainda. Testar: `WOTM_MATCH=3,1,12` (jogador Togera, IA Congar; o 12 vai direto ao `m_levelNum`, `wotm_overrides.cpp`) e ver no
  log se carrega, se os monstros nascem nos 3 pontos e se há chão/limites. Código específico por fase (`levels.h`) pode faltar para
  o 12. Plano B: Centro ou Tóquio DM (arenas conhecidas) ou o dodgeball (25) sem o modo 7 — no modo 7 o dano vira "3 acertos".

## Recomendações de ganchos (resumo)

| O quê | Gancho | Nota |
|---|---|---|
| Hurtbox | `Scene::walk`: grupos `flags & 2` e op 10 nos cs dos monstros | planos → caixa; matriz já vem do `atMat` |
| Cápsula | ler `Monster` +0x3F0/+0x400/+0x404 e o cs | linha de arame na cena |
| Esferas | ler +0x2B0/+0x360 (ou +0x370/+0x3A0) | opcional |
| Evento de golpe | embrulho de `takeHit` (contexto) + `takeDamage` (vida antes/depois) | `ra` classifica o dano fora do `takeHit` |
| Boneco parado | `m_unkF9 = 0` na entrada do `Monster::update` do boneco | ou `Ai::updateInputs` com entrada fixa (bloqueio) |
| Vida infinita | `blowUpMonster` no boneco (vindo do `takeDamage`) + recarga por inatividade | não usar `m_godMode`/`m_unkEA` |
| Fase | testar `WOTM_MATCH=...,12` | não está nos menus |
