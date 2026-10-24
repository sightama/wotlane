# Modo treino: entradas do monstro (mostrar, boneco com comportamento fixo, gravar e reproduzir) (2026-10-23)

Investigação só de leitura para o item 9 do `POS_FRONTEIRA.md` (complementa `treino_hitboxes.md` e `treino_dano.md`). Fontes:
`src/game/{Monster,GamePad,PadFlags,Ai,AiReflex,MonsterStates,MovementStates}.cpp`, `include/game/{pad_flags,ai,game}.h`, rascunhos do
m2c de `GamePad::loadPadInputs`, `PadFlags::interpretInputs`, `updateInput0ConfigCR`, `updateHistory0`, `updateActions0`,
`Ai::updateInputs`, `AiNavigator::updateSteering`, e os ganchos `native/runtime/wotm_overrides.cpp` (lock-on alternado),
`wotm_orbit.inc`, `wotm_pad.inc`, `wotm_training.inc`. Também `75f5be1:docs/investigacoes/2026-10-22/camera_controle_cr_sr.md` (CR x SR).
Endereços por nome em `native/wotm_syms.h`. Nada foi testado no jogo.

## Resumo

- A entrada do monstro é **um `GamePad` de 0x1C bytes em `Monster +0x5024`**: um byte por botão (0 solto, 0xFF apertado, ou o valor
  analógico 0..255), mais as quatro direções do analógico esquerdo em bytes separados. O jogador o enche com `loadPadInputs`, a IA
  escrevendo 0xFF/valores nele. Os dois passam depois pelo mesmo `PadFlags::interpretInputs`.
- **Para "apertar" um botão basta escrever o byte do `GamePad`** entre o `clearInputs` e o `interpretInputs` do `Monster::update`. No
  boneco (IA), o ponto é um gancho em **`Ai::updateInputs` (0x101788)**. Os contadores, o histórico, a fila de ações e os combos
  saem disso sozinhos.
- O controle normal (configuração **CR**) é **tanque, relativo ao próprio monstro**, para o jogador na vista normal e para a IA. Por
  isso **reproduzir os bytes no boneco já dá o mesmo movimento em relação a quem ele olha**: "frente" continua sendo para o oponente.
  Não há conversão a fazer, a não ser quando a câmera moderna (órbita) põe o jogador na SR (relativa à tela). Nesse caso, converter o
  analógico gravado para o referencial do monstro (fórmula na seção 4).
- **Defesa = R2 (byte 4)**, segurado. **Pulo = X (byte 6)**, que precisa de borda (soltar e apertar de novo). **Agachar não existe**
  (o "crouch" do código é só a preparação do pulo). Lock-on = L1+R1 (bytes 0 e 3) segurados.

## 1. Layout do `GamePad` (Monster +0x5024, 0x1C bytes)

`GamePad::loadPadInputs(porta)` (0x13A020) monta os bytes com `inputGetInput(máscara, porta)` (0x20BE08; os botões 0x100..0x8000 já
passam pelo remapeamento do menu) e `inputFixAnalogValue(eixo, porta)` (0x20C390). `GamePad::clearInputs` zera +0x00..+0x17.

| byte | botão (máscara do pad) | o que faz na configuração 0 (CR) |
|---|---|---|
| 0x0 | L1 (0x400) | andar de lado para a esquerda (sozinho); com R1, lock-on |
| 0x1 | L2 (0x100) | olhar/mirar (o analógico cima/baixo vai para +0x1E/+0x20 e frente/trás zeram) |
| 0x2 | L3 (2) | cru em PadEntry +0x54 |
| 0x3 | R1 (0x800) | andar de lado para a direita (sozinho); com L1, lock-on |
| 0x4 | R2 (0x200) | **defesa** (segurado); com triângulo ou quadrado novo, contra-ataque |
| 0x5 | R3 (4) | cru em +0x5A |
| 0x6 | X (0x4000) | **pulo/voo** (PadEntry +0x2A) |
| 0x7 | círculo (0x2000) | agarrar/arremessar (+0x2E) |
| 0x8 | triângulo (0x1000) | golpe forte (+0x2C) |
| 0x9 | quadrado (0x8000) | golpe fraco/tiro (+0x30) |
| 0xA | Start (8) | — |
| 0xB | Select (1) | provocação (`Monster +0xF0` `m_wantsTaunt`; o resto do quadro fica vazio) |
| 0xC / 0xD | esquerda / direita (0x80 / 0x20) | girar (tanque); travado, andar de lado |
| 0xE / 0xF | cima / baixo (0x10 / 0x40) | frente / trás |
| 0x10 / 0x11 | analógico direito X (−/+) | girar, quando o esquerdo não gira |
| 0x12 / 0x13 | analógico direito Y (+/−) | olhar cima/baixo |
| +0x14 (int) | "analógico em uso" | 1 quando o analógico esquerdo deu algum valor no quadro |
| +0x18 (int) | modo do pad | 0: digital (todo valor ≠ 0 vira 0xFF); ≠ 0: pressão (L1/R1 e esquerda/direita escalados pela sensibilidade) |

- Direções: o direcional entra como 0/0xFF. Se o direcional está solto, o analógico esquerdo (eixos 2 e 3) entra com o valor
  (0..255 por lado) e o vetor fica limitado a um comprimento máximo (`s_len`). Ou seja: **as direções são quatro bytes não
  negativos**, x = D − C e y = E − F.
- Os botões de face entram como 0xFF (modo digital) ou pela pressão.

### `PadFlags` (Monster +0x5040, 0x1814 bytes) e o `PadEntry` do quadro
- `interpretInputs(gp)` (0x177D20): troca a configuração ativa (`+0x16E4` ← `+0x16E8` quando `okToChangeMap`) e chama
  `updateInput{N}Config{CR|SR}` com N = `+0x16EC` (esquema de botões do menu de opções, 0..2) e CR/SR = `+0x16E4` (0 CR, 1 SR):
  CR0 0x178D58, SR0 0x179638, CR1 0x17AEF8, SR1 0x17B7C0, CR2 0x17CFD0, SR2 0x17D6E8. Cada uma faz:
  1. `saveAndClear(0)`: avança `ringIndex` (`+0x16F0`, anel de 60 `PadEntry` de 0x5E bytes em `+0x0`) e zera o novo;
  2. copia os bytes do `GamePad` para o `PadEntry` (tabela abaixo);
  3. `updateHistory{N}(gp)` (0x17AC58 / 0x17CD30 / 0x17EA98): **contadores de quadros segurados** por botão (`+0x160A` cima, `+0x160C`
     baixo, `+0x160E` esquerda, `+0x1610` direita, `+0x1612` quadrado, `+0x1614` círculo, `+0x1616` triângulo, `+0x1618` X,
     `+0x1658` L1, `+0x165A` L2, `+0x165C` L3, `+0x165E` R1, `+0x1660` R2, `+0x1662` R3, `+0x1668..+0x166E` direções acima de 0x80,
     `+0x1650..+0x1656` analógico direito ≥ 0xFB, e `+0x1632..+0x1638` para os botões de face);
  4. derivados (lock-on, defesa, contra-ataque, dash, investida, especial...);
  5. `updateActions{N}(gp)` (0x17A4E0 / 0x17C650 / 0x17E3C8): **ação mapeada** do quadro (`+0x1710` botão, `+0x1714` ação, `+0x1718`
     hora): quadrado 0 (neutro) e 1..4 (com cima/baixo/esquerda/direita), triângulo 5 e 6..9, círculo 0xA e 0xB..0xE; 0x14/0xF =
     nenhuma. O neutro sai ao soltar ou ao segurar mais que `tweak16C4` quadros; o direcional só sai se a direção foi apertada há no
     máximo `tweak16C4` quadros junto com o botão. Para isso a direção tem que passar de **0xE1** quando o pad é analógico
     (`gp+0x14`) **ou o dono é IA** (`m_playerNum` 2); com o direcional digital do jogador, basta ser ≠ 0;
  6. `checkCombos(gp)`.

`PadEntry` (só os campos que importam; `include/game/pad_flags.h`):

| +off | de onde (CR0) | uso |
|---|---|---|
| 0x02/0x04/0x06/0x08 | E/F/C/D cru | direções cruas (escalada, ações direcionais) |
| 0x0A/0x0C/0x0E/0x10 | quadrado/círculo/triângulo/X | botões de face |
| 0x12/0x14 | E/F | frente/trás (zera com L2) |
| 0x16/0x18 | C/D (ou o analógico direito X) | girar esquerda/direita |
| 0x1A/0x1C | L1/R1 sozinhos, ou C/D travado sem L2/R2 | andar de lado |
| 0x1E/0x20 | L2 + cima/baixo, ou o analógico direito Y | olhar |
| 0x2A | X | pulo/voo |
| 0x2C/0x2E/0x30 | triângulo/círculo/quadrado | forte, agarrar, fraco |
| 0x32 | L1 e R1 segurados mais que `tweak16C4` | **lock-on** |
| 0x34 | R2 segurado (`+0x1660 ≠ 0`) | **defesa** |
| 0x36 | triângulo + círculo | especial |
| 0x38 | quadrado + X | "crowd control" |
| 0x3E | toque duplo para a frente, ou analógico > 0xE1 seguindo um dash | dash |
| 0x40 | dash + triângulo | investida (RamAttack) |
| 0x44 | cima novo + triângulo | butt slam |
| 0x46 | R2 + triângulo/quadrado apertado há ≤ `tweak16C4` | **contra-ataque** |
| 0x48..0x4E | analógico direito ≥ 0xFB | |
| 0x50..0x5A | L1, L2, L3, R1, R2, R3 crus | |

`PadFlags +0x1810` = dono (Monster*). Os estados leem `m_padFlags[0]` (quadro atual) e `[1]` (anterior, para borda).

### Ordem no `Monster::update` (0x15B488, `src/game/Monster.cpp`)
```
clearInputs/updateDeathSequence se morto; senão:
  m_playerNum (+0x18) == 1 (humano): m_unkF9 (+0xF9) ? playerUpdateInputs() [loadPadInputs(*m_playerInfo)] : gp.clearInputs()
  m_playerNum == 2 (IA):             gp.clearInputs(); if (m_unkF9) Ai::updateInputs()   (Ai embutido em Monster +0x4E0)
  m_padFlags.interpretInputs(gp)      <- embrulhado pelo lock-on alternado e pela órbita (wotm_overrides.cpp / wotm_orbit.inc)
  ... estados (que leem o PadEntry)
```
O ponto certo para "apertar" no boneco é **dentro do `Ai::updateInputs`** (depois do `clearInputs` e antes do `interpretInputs`).

### Como a IA escreve
- `Ai +0x44` é o ponteiro para o `GamePad` do monstro (o construtor `Ai(Monster&, GamePad&)` recebe `Monster +0x5024`).
- `Ai::updateInputs` (0x101788; a0 = `Monster +0x4E0`): `updateRelevances`, `updateOpponentList`, e pelo global `Ai::s_mode`
  (0x6F791C): **0** (normal) = `AiBrain::update` (as ações chamam `Ai::block`, `lightPunch` etc.) e `AiNavigator::update(gp)`;
  **1** = só persegue o jogador 1; **2** = `loadPadInputs(gp, 0)`, ou seja, toda IA segue o controle 1 (depuração); **3** = nada (pad
  vazio: um boneco parado já pronto, para todas as IAs). `toggleMode` está vazio no retail. Goliath (0x1A0) e o tipo 0x1C0 têm caminho
  próprio.
- Botões (`src/game/Ai.cpp`): `targetPin` 0+3, `block` 4, `counter` 4+8, `jump` 6, `grab/toss/thrash` 7, `heavyPunch` 8,
  `lightPunch/fireProjectile` 9, `taunt` 0xB, `specialAttack` 8+7, `crowdControl` 6+9, `buttStomp` 0xE+8. Sempre 0xFF.
- Analógico: `AiNavigator::updateSteering` (0x113FB8) escreve **girar** em C/D, **frente/trás** em E/F e **andar de lado** em L1/R1
  (bytes 0/3), cada um com |v|·255 (até 255). Ou seja, a IA também pilota tanque, no referencial do monstro.

### Referencial do analógico
- **CR** (o normal na luta, para o humano e para a IA): cima/baixo = frente/trás **do monstro**, esquerda/direita = **girar**;
  travado (lock-on), esquerda/direita = andar de lado e a frente é para o alvo (o estado vira o monstro para o alvo).
- **SR** (`+0x16E4 = 1`): relativa à câmera da vista (`Cameras::m_cameras[view] +0xCF0`, frente = `rot.z`, direita = linha 0 da
  matriz). O jogo só a usa na vista unificada e no Crush. **No port, a câmera moderna (`wotm_orbit.inc`) força a SR para o jogador**
  durante o `interpretInputs` (grava `+0x16E4/+0x16E8 = 1` na entrada e devolve na saída) e zera o analógico direito (bytes
  0x10..0x13) antes do jogo ler. A direção pedida no mundo fica em `PadFlags +0x17E0` (z = 0).
- O lock-on alternado (`WOTM_LOCKTOGGLE`) troca bytes na entrada do `interpretInputs` (L2→0, R2→3, L1→4; L2 do jogo solto),
  **guarda os originais e os devolve na saída**, e decide o lock-on forjando os contadores `+0x1658/+0x165E` na saída do `updateHistory`.
  Na CR travada, ele também reescreve C..F para o referencial do alvo (`lockCameraStick`). A IA não passa por nenhum dos dois (ambos
  testam `isPlayerMonster`).

## 2. Caminho do jogador e onde gravar

```
raylib (teclado/controle; wotm_pad.inc: g_ps2PadKeyboard, g_ps2PadGamepadButtons) -> scePadRead (0x232B58, buffer do SDK)
 -> inputGetInput / inputFixAnalogValue (remapeamento do menu) -> GamePad::loadPadInputs (no Monster::update, playerUpdateInputs)
 -> interpretInputs [orbita: SR + analógico direito zerado; lock-on alternado: troca de bytes + contadores]
 -> updateInput{N}Config{CR|SR}(pf, gp) -> PadEntry
```
- **Ponto de gravação recomendado: a entrada de `updateInput{N}Config{CR|SR}`** (as seis). São as funções mais de dentro, ninguém as
  embrulha, e na entrada os bytes do `gp` (a1) são exatamente o que o jogo vai ler, já com o remapeamento do lock-on alternado e o
  analógico direito zerado pela órbita. A função chamada diz N e CR/SR. Na saída, ler o `PadEntry` do quadro (`pf + ringIndex·0x5E`)
  para gravar também `+0x32` (lock-on, que no lock-on alternado não está nos bytes, só nos contadores forjados).
- O formato gravado é o mesmo que a IA escreve: os 0x18 bytes do `GamePad` (incluindo o int `+0x14`), mais
  `{N, sr, lockOn, frame}`.
- Gravar na saída do `Monster::update` também funciona para mostrar, mas para o jogador os bytes já voltaram aos originais do
  `loadPadInputs`, antes do remapeamento. Para reproduzir, isso está errado com o lock-on alternado ligado.

## 3. Defesa

- `StateBlock` (Monster +0x7DA0; id 0x1B). `transitionOK`: animação 0x3C existe, `m_attacksEnabled`, sem queda livre, e
  **`PadEntry +0x34` (R2 segurado)**. É testado a partir de Idle, Run, Dash, Punch, BatSwipe, Recoil e JumpFly.
  `update`: R2 segurado → `blocking = 1` depois de `raiseFrames`; R2 + triângulo/quadrado novo → Counter; soltou → Punch/Grapple/Idle.
- Vindo de um recuo (`StateRecoil`, 0x1C) com o recuo bloqueado (`recoil +0x64`), a defesa já entra levantada.
- `Monster::isBlocking` (0x162A98) = estado 0x1B com o flag de bloqueio ou 0x1C com o flag de recuo bloqueado. `takeHit` só reduz o
  dano (`× m_damageModifier`) se `isBlocking` e `StateBlock::isBlockable(hit)` (o source 6/0x14 e o 5 com 0x40 não são bloqueáveis;
  atordoamento, `type` 7, também não). Agarrões não passam pelo bloqueio.
- IA: `AiBlockReflex` (`src/game/AiReflex.cpp`). Entra pela relevância (ameaça na frente), espera `getReflexDelay` (`Ai +0x1518..`),
  faz `targetPin` se o alvo está bem no campo de visão e depois `block()` a cada quadro. Há também o `StateBlock::autoBlock`
  (monstro mais perto num estado de ataque 3, 0x1A, 0x28, 0x29).
- **Forçar o bloqueio só pela entrada funciona:** `gp[4] = 0xFF` a cada quadro. Limites: os `raiseFrames` iniciais, a necessidade de
  estar num estado que testa o Block (no ar ou caído não defende), e o que é inbloqueável por natureza.

## 4. Direção ao reproduzir

- Na CR tudo já é relativo ao monstro. **Reproduzir os bytes como estão** faz o boneco repetir a manobra no referencial dele:
  andar para a frente do jogador vira o boneco andar para a frente (para o jogador, se ele estiver virado para lá), girar à esquerda
  vira girar à esquerda dele. Isso é exatamente o espelho que se espera de dois monstros frente a frente, sem trocar esquerda por
  direita. Uma opção "espelhar lados" seria trocar C↔D, 0↔3 (L1/R1) e 0x10↔0x11.
- Gravação em malha aberta: sem lock-on, o rumo vai se desviando (colisões, empurrões). **Para ficar robusto, reproduzir com lock-on**:
  se o jogador estava travado (`+0x32` gravado), escrever 0 e 3 = 0xFF. O boneco trava no monstro mais perto, que é o jogador, e então
  frente = para o alvo e lados = andar de lado. O lock-on só liga depois de `tweak16C4` quadros segurados (`PadFlags +0x16C4`), então
  convém já segurar 0/3 por alguns quadros antes do primeiro quadro gravado. Com o lock-on alternado, os bytes 0/3 gravados vêm
  zerados ou com outro sentido: use o `+0x32` gravado.
- Quadro gravado em **SR** (câmera moderna ligada): converter C..F para o referencial do monstro, com o mesmo cálculo da SR:
  ```
  x = (D - C)/255, y = (E - F)/255
  w = x·camDir + y·camFrente          (camFrente = (sin rot.z, cos rot.z) do _cs da câmera da vista; camDir = linha 0 da matriz)
  frente = lockOn ? normalize(alvo - eu) : (sin m_cs.rot.z, cos m_cs.rot.z);  dir = crossUp(frente)
  a = dot(w, frente), b = dot(w, dir)
  E = max(a,0)·255, F = max(-a,0)·255, D = max(b,0)·255, C = max(-b,0)·255
  ```
  Calcule isso na hora da gravação, com a câmera e o monstro do jogador daquele quadro, e grave já convertido. Diferença
  conhecida: na SR solta, puxar para trás vira e corre; na CR, anda de ré. Se incomodar, com `a < 0` e solto, grave girar forte para
  o lado de b em vez de F. Alternativa mais simples: desligar a órbita durante a gravação.
- O esquema de botões: o `gp` gravado só vale para o mesmo `+0x16EC` (N). No boneco, durante a reprodução, escrever
  `PadFlags +0x16EC` (Monster +0x672C) = N gravado e devolver o valor depois, ou gravar só no esquema 0. O boneco (IA) fica com
  `+0x16E4 = 0` (CR), como a gravação convertida espera.
- Diferença de limiar: o `updateActions` exige direção > 0xE1 para a IA. A gravação do jogador com analógico já usa 0xE1
  (`gp+0x14`); com o direcional digital o byte é 0xFF. Então o golpe direcional sai igual. Grave e reproduza também o `+0x14`.
- Tempo: os contadores do `PadFlags` são por quadro (1 gravação = 1 `Monster::update`), mas a fila de ações e alguns combos usam
  `timerGetFieldCount`. Com queda de quadros diferente entre gravar e reproduzir, um combo no limite da janela pode sair diferente.

## 5. Pulo e agachar

- **Pulo** = X (byte 6 → `PadEntry +0x2A`). `StateJump::transitionOK` / `StateFly`: `+0x2A` apertado e o `transitionFeasible`, que no
  voo exige `+0x2A` solto no quadro anterior (borda). Também entra "sozinho" com 6 quadros caindo (`Monster +0x288`). No ar, um
  toque novo bate as asas (se a stamina deixar; `StateFly`). Para "pular sempre", **pulsar**: X por k quadros (k maior pula mais alto
  se o pulo depende do segurado; conferir), solto por M quadros, e repetir.
- **Agachar não existe** como comando: os `crouchBlend`/"crouch" do `MovementStates.cpp` são a preparação do pulo (sub-estado 1).
  Os comandos mais próximos: defesa (R2) e o butt slam no ar (cima novo + triângulo).

## Recomendação de ganchos

### (a) Mostrar as entradas de cada monstro a cada quadro
- Gancho na **entrada das seis `updateInput{N}Config{CR|SR}`** (a0 = pf, a1 = gp): dono = `pf+0x1810` → slot (`slotOf`); copiar os 0x18
  bytes do gp, N, CR/SR. Na saída (quando a função voltou, `ctx->pc == ra`): `PadEntry` atual (`pf + rd32(pf+0x16F0)·0x5E`) →
  flags `+0x2A` pulo, `+0x32` lock, `+0x34` defesa, `+0x36` especial, `+0x38` crowd, `+0x3E` dash, `+0x40` investida, `+0x44` butt slam,
  `+0x46` contra-ataque, e a ação `pf+0x1714` (0..0xE; 0xF nenhuma). Vale para jogador e CPU, e mostra o que o jogo de fato leu. Com o
  pad desligado (`m_unkF9 = 0`), aparece vazio, como é.
- Empurrar `{frame, slot, bytes, flags, ação}` no `Shared` do `wotm_training.h` (com o mutex, como os `sweeps`). Desenhar na thread da
  janela (`wotm_live.inc`): botões acesos, vetor do analógico (x = D − C, y = E − F), lista rolante estilo jogo de luta (direção como
  numpad + botões, com quantos quadros cada linha durou).

### (b) Boneco com comportamento fixo
- Gancho em **`Ai::updateInputs` (0x101788)**: `m = rd32(a0)` (ou `a0 − 0x4E0`); se `m` é o boneco e o modo é fixo, **não chamar o
  original** e escrever o `GamePad` (`m+0x5024`, já zerado pelo `clearInputs`). Nos outros casos, passar adiante. Nesses modos o
  boneco precisa de `m_unkF9 = 1`: o "parado" de hoje zera o F9 e assim o `Ai::updateInputs` nem roda.
  ```
  sempre bloquear:   gp[4] = 0xFF
  bloquear depois do 1o golpe: no takeHit do boneco (gancho já existe), blockUntil = frame + N (renovado a cada golpe); gp[4] = frame < blockUntil
  pular:             gp[6] = (t % periodo) < k ? 0xFF : 0
  andar frente/trás: gp[0] = gp[3] = 0xFF (lock-on no jogador; segurar desde antes) ; gp[0xE] ou gp[0xF] = 0xFF ; *(int*)(gp+0x14) = 1
  andar de lado:     lock-on + gp[0xC] ou gp[0xD] = 0xFF
  ```
  Sem o lock-on, "frente" é o rumo do boneco. Alternativa: girar para o jogador escrevendo C/D (como o `AiNavigator::orientTo`).
- Atalho de diagnóstico sem gancho: `Ai::s_mode = 3` (0x6F791C) deixa **todas** as IAs com o pad vazio, mas ainda "vivas" no pad
  (o F9 fica como está).

### (c) Gravar e reproduzir
- **Gravar:** com o mesmo gancho do (a), filtrado no slot do jogador, guardar por quadro `{gp[0x18], N, sr, lock(+0x32)}` num
  buffer de N·60 entradas (no máximo ~10 s ≈ 600 × 0x20 bytes). Se `sr`, gravar C..F já convertidos (seção 4). Começa e termina
  pelo menu F2.
- **Reproduzir:** no gancho do `Ai::updateInputs` do boneco (mesmo do (b)), escrever a entrada i e avançar 1 por chamada. Durante a
  reprodução: `pf+0x16EC` do boneco = N gravado (devolver no fim); se `lock` gravado, bytes 0/3 = 0xFF (e ligar alguns quadros
  antes); "espelhar lados" opcional (C↔D, 0↔3, 0x10↔0x11). Em loop ou uma vez, e no fim volta ao comportamento do (b).
- Para não andar à deriva, começar a reprodução com o boneco virado para o jogador. O jeito mais simples é um lock-on curto
  (0/3 por `tweak16C4 + 2` quadros) antes do primeiro quadro, como no jogo de luta que espera a posição inicial.
- Thread: todos esses ganchos rodam na thread do EE, dentro do `Monster::update`. O menu e o desenho só leem cópias pelo `Shared`.

## Em aberto
- Se a altura do pulo depende de segurar o X (ver `StateJump::update`/configuração do pulo) e o valor de `tweak16C4`, `raiseFrames` e
  `tweak16DC` em tempo de execução (copiados do `TheGame +0x12046C` pelo `UpdatePadTweaks`).
- Esquemas 1 e 2 (`updateInput1/2Config*`): não li o mapeamento. A recomendação de gravar/reproduzir com o mesmo N evita depender dele.
- O `+0x18` do `GamePad` (modo digital/pressão) da IA: o `clearInputs` não o zera. A reprodução não precisa escrevê-lo.
