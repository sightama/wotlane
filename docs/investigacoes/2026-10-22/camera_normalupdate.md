# `Camera::normalUpdate` e companhia: o que a câmera do jogo faz (etapa 0 da câmera orbital)

Investigação de 2026-10-22 para o plano `camera_orbital_plano.md` (mesma pasta). Fontes: `asm/nonmatchings/game/Cameras/*.s` (lido à
mão; o m2c só serviu de mapa), `src/game/Cameras.cpp`, `MonsterInit::initCameraData`, `TheGame::gameCheckForCloseCombat`,
`PadFlags::interpretInputs`/`updateInput0ConfigSR`, `native/runtime/wotm_split4.inc`. Tudo em prosa e pseudo-C; nenhum trecho de asm.

Convenções: `C+0xNNN` = campo da `Camera` (0xEB0 bytes, `Cameras::m_cameras + vista*0xEB0`); `M+0xNNN` = campo do `Monster`;
`cs` = `_cs` da câmera (`C+0xCF0`): `trans` +0x10, `mat` +0x20 (linhas direita, frente, cima), `rot` +0x60 (x = pitch, y = roll,
z = heading). Heading: em torno de Z, 0 = +Y, positivo para +X; a direção do heading `h` é `(sin h, cos h)`.
Grau de certeza: **[certo]** lido no asm até o fim; **[provável]** dedução com uma ponta não conferida; **[a medir]** conferir no runtime.

## 0. Tabela de modos (`Camera::UPDATE_FUNK`)

**[certo]** `Cameras::Update` chama `UPDATE_FUNK[C+0xCF8]` (o POV), depois `checkCollision` (só pontos de visada) e `C+0xE34 += 1`
(relógio do POV, em quadros). A tabela (rodata 0x6E8A28, 10 ponteiros para membro) é:

| POV | função | quem põe |
|---|---|---|
| 0 | `normalUpdate` | padrão; `TogglePOV` (L2), `ReattachCamera`, fim de arremesso/levantar/pisão |
| 1 | `closeCombatUpdate` | câmera 2 na vista unificada (`InitCCCamera*`) |
| 2 | `roamUpdate` | câmera de depuração no pad 0 (lê o analógico direito, `inputFixAnalogValue(2/3)`); não achei quem a ligue |
| 3 | `staticUpdate` | estacionada: `DetachCamera` (volume de câmera da fase, agarrado morto), `TogglePOV`, planta-chefe, câmera 2 fora da unificada |
| 4 | `getupUpdate` | `StateThrowBack` (levantando depois de arremessado) |
| 5 | `knockbackUpdate` | `StateThrowBack` (voando arremessado), `StateGrappled::breakFree` (câmera 2), planta-chefe |
| 6 | `topdownUpdate` | `StateButtSlam` com a câmera na frente do monstro |
| 7 | `victoryUpdate` | `StateVictory` (câmera 2 com `gUseUnifiedView = 1`, ou câmera 0 nos modos 8/9) |
| 8 | `topdownUpdate` | `StateThrowBack` quando `M+0xF8` está ligado (variante do 5) |
| 9 | `crushUpdate` | fase de esmagar (modo 9) |

## 1. `normalUpdate` (0x11F630, 0x10F0 bytes)

### Parâmetros (POV carregado por `SetCameraPOV` da tabela `M+0x6CF0 + split*0x640 + pov*0xA0`)

Valores de `Monster::initCameraData` (os mesmos para os 10 POVs e todos os monstros; não achei quem os sobrescreva:
`getCameraData` não tem chamador por `jal`; **[a medir]** ler `M+0x6CF0` no runtime para confirmar):

| campo da câmera | origem | tela cheia | dividida | papel |
|---|---|---|---|---|
| `D10` (vetor) | `+0x00` | (0, −274, 160) | (0, −240, 160) | deslocamento "atrás" no referencial de guinada do monstro (x direita, y frente, z cima) |
| `D50` | `+0x10` | (0, 250, −120) | igual | deslocamento extra com `D40 > 0` (olhando para cima): aproxima e abaixa |
| `D60` | `+0x20` | (0, 270, 100) | igual | deslocamento extra com `D40 < 0` (olhando para baixo): sobe por cima |
| `D70/D74` | `+0x30` | +45°, −45° | igual | inclinação extra em rampa (chão tipo 3) |
| `D78/D7C` | `+0x38` | +5°, −6° | +3°, −5,5° | inclinação extra subindo / caindo no pulo |
| `D80/D84` | `+0x40` | +10,5°, −16,5° | +12°, −28° | inclinação extra escalando para cima / para baixo |
| `D00` | `+0x48` | −10° | igual | inclinação base (olha 10° para baixo) |
| `DB8` | `+0x4C` | 60° | igual | meia-largura do setor traseiro (ver "correr para as costas") |
| `DB0`/`DB4` | `+0x60/0x64` | 280 / 320 | igual | faixa da "coleira" (distância 3D) |
| `DA8`/`DAC` (int) | `+0x68/0x6C` | 300 / 60 | igual | quadros parado até voltar às costas / quadros do "volta às costas" |
| `DCC` | `+0x70` | 0,05 | igual | taxa da inclinação |
| `DC4` | `+0x74` | 0,08 | igual | taxa da posição |
| `DD4`, `DD0` | `+0x78/0x7C` | 0,08, 0,016 | igual | taxas da vista unificada |
| `E48` | `+0x5C` | 3,0 | igual | largura do contorno do `translateCamera` |
| `DF8` (int) | `+0x88` | 30 | igual | quadros arrastando até mandar às costas |
| `DFC`/`E00` | `+0x8C/0x90` | 300 / 450 | igual | distância de entrar / sair da vista unificada |
| `E04`, `E08`, `E44` | `+0x94/98/9C` | 45°, 1000, 30 | igual | FOV do teste de aproximação, ?, duração da transição de POV |

### O que ele faz, em ordem **[certo]**

```c
void Camera::normalUpdate() {
  if (!cs || !monster) { shake = 0; return; }
  mcs = monster->m_cs;
  if (*monster->m_state == 0x1E /* Idle */) DA4++;                 // relógio "parado"
  // câmera 2 no POV 0 durante a vista unificada (agarrão): par longe demais e ninguém em estado 0x10 -> sai
  if (E0C && E10 && gUseUnifiedView && dist3(E0C, E10) > E00 && !(E0C->state.flags & 0x10) && !(E10->state.flags & 0x10))
    { Cameras::LeaveUnifiedView(2); return; }
  if (hasPinTarget() != E64) { E34 = 0; DE8 = 0; E40 = 1/E44; }   // travou/destravou: a guinada recomeça a rampa
  rampa das taxas: durante E34 < E44, DE0/DE4/DE8 sobem até DC4/DCC/1 (passos E38/E3C/E40); depois ficam DC4, DCC, 1;
                   D9C == 1 -> as três = 1 (instantâneo) neste quadro; D9C == 2 -> DE0 = 0,5.
  frame = { direita = mcs->mat[0], frente = normalize(cima_mundo x direita), cima = direita x frente };   // guinada do monstro
  if (D98) {   // "corte": SetCameraPOV pôs 1 (fora dos casos de rampa), TogglePOV, ReattachCamera, Init
    cs->rot = (0, 0, mcs->rot.z);  cs->mat = frame;  cs->trans = D20 = mcs->trans + frame*D10;
    DD8 = mcs->rot.z; DE0 = 0,09; DA4 = DEC = DF0 = 0; shake = 0; D98 = 0;
  }
  h = mcs->rot.z;  A = dir(h + DB8 + 90°);  B = dir(h - DB8 - 90°);       // bordas do setor traseiro
  d = cs->trans - mcs->trans;  rH = |d.xy|;  dist = |d|;  n = d/dist;
  a = dot(A, n);  b = dot(B, n);
  if (a > 0 && b > 0) E60 = 0;            // câmera dentro de ±60° das costas: arma a trava do setor
  if (D14 > 0) E60 = 1;                   // POV com a câmera na frente: sem trava
  alvoSetado = false; direto = false;
  if (D40 != 0) {                         // inclinação extra em curso: o deslocamento balança
    k = sqrt(|D40| / (pi/2));
    D30 = D10 + k * (D40 < 0 ? D60 : D50);
    D20 = mcs->trans + frame*D30;  DA4 = 0;  alvoSetado = true;  direto = (D30.y >= 0);
    if (D40 > 0) DE0 = rH' < 50 ? 1 : rH' < 100 ? 0,2 : rH' < 150 ? 0,1 : DE0;   // rH' = |D20 - monstro|.xy
  } else if (a < 0,1 && E60 == 0) {       // passou da borda A: gira a câmera em volta do monstro até a borda
    D20 = ponto de cs->trans girado em volta do monstro (mesmo raio rH) pelo ângulo asin(|a|);
    if (a < -0,01) { cs->trans = D20 /* teleporte */; DA4 = 0; DF4++; }
    else { if (DF4 > DF8) DA4 = DA8; DF4 = 0; }
  } else if (b < 0,1 && E60 == 0) {       // idem do outro lado
    ...
  } else { if (DF4 > 0) { if (DF4 > DF8) DA4 = DA8; DF4 = 0; } }
  if (!alvoSetado) {
    if (monster->m_freeFalling && m_fallTimeBeforePitch < m_fallTime) D20 = mcs->trans + frame*D10;   // caindo: às costas
    else if (DA4 >= DA8) { D20 = mcs->trans + frame*D10; if (DA4 > DA8 + DAC) DA4 = 0; DF4 = 0; }  // "volta às costas"
    else if (dist < DB0 || dist > DB4) {  // coleira 280..320
      s = (dist < DB0 ? DB0*DB0 : DB4*DB4) / (dist*dist);
      if (escalando (0x1F) || no pulo (m_state == M+0x7978)) d *= s; else d.xy *= s;   // z fica onde está
      D20 = mcs->trans + d;  DE0 = 1;                                                 // pulo seco para a faixa
    }                                     // dentro da faixa: D20 fica o do quadro anterior
  }
  D30 = 0;
  contornou = translateCamera(D20, DE0, direto || CFC == 6);   // suaviza; contorna o alvo se o monstro sairia do quadro
  if (hasPinTarget() || !sShakeEnabled) shake = 0;
  shakeCamera();
  rotRet = rotação da câmera até m_reticleCS->trans;          // com alvo travado o retículo fica no alvo [provável]
  if (hasPinTarget() && |mcs.z - pinTarget.z| < m_pinMuckingDist) rotRet.x = clamp(rotRet.x, ±m_pinMaxPitch);
  rotMon = rotationFromVector(mcs->trans - cs->trans);        // pitch para o monstro
  // GUINADA: olha para 150 à frente do monstro
  cs->rot.z = smoothEasyInAngle(cs->rot.z, heading(cs->trans -> mcs->trans + mcs->mat[1]*150), DE8, 0,001);
  // INCLINAÇÃO
  p = D00 + (pulo: fase 1/2 -> D78; caindo há tempo (fase 6/8 ou fora do pulo com fallTime) -> D7C; senão 0);
  extraEscalada = escalando ? (vel(M+0xF078) > 0 ? D80 : < 0 ? D84 : 0) : 0;
  extraRampa = (no chão, chão tipo 3, não escalando) ? (frente·normal < 0 ? D70 : > 0 ? D74 : 0) * (1 - normal.z) / 0,3 : 0;
  alvoD40 = (!hasPinTarget() && m_aimPitch == 0) ? extraEscalada + extraRampa : rotRet.x - p - m_targetingMod;
  D40 = smoothEasyInAngle(D40, alvoD40, 0,09, 0,003);
  p += D40;
  if (estado 0x26 ou 7, ou estado 0x20 com fallTime) p = rotMon.x;  // olha direto para o monstro
  cs->rot.x = smoothEasyInAngle(cs->rot.x, clamp(p, ±1,55), DE4, 0,003);
  mathfRotMatrixRPH(cs->mat, &cs->rot);                          // roll não é tocado (0 desde o corte)
  if (sPinTestFOV && hasPinTarget() && m_pinTime > 60 && E34 > E44 && typeBits in {0x40,0x160,0x100} && m_state == m_specialState)
    { posInFOV(retículo, 0, false); posInFOV(M+0x3E30, 0, false); }  // gira para caber alvo e monstro
  else if (!hasPinTarget() && contornou) posInFOV(M+0x3E30, 0, false);
  E64 = hasPinTarget();
}
```

### Respostas da pergunta 1

- **Pivô:** a origem do monstro (`mcs->trans`); a mira de guinada é um ponto 150 à frente dele (`mat[1]`), não a cabeça. A
  inclinação mira à parte (`D00` = −10° + extras), e o ponto `M+0x3E30` só entra no `posInFOV`. **[certo]**
- **Distância e altura:** não há "zoom" contínuo. São três regimes **[certo]**:
  1. *coleira* (o normal andando no plano): enquanto a distância 3D está entre 280 e 320 a câmera só desliza (0,08 por quadro) para o
     último alvo; saiu da faixa, pula seca para dentro dela **na mesma direção**, e a altura (z) fica onde estava (só escalando ou no
     pulo a coleira escala z também). Ou seja, a altura "gruda" **[a medir: conferir subindo rampa/prédio]**;
  2. *às costas* (`frame*D10` = 274 atrás e 160 acima, distância ≈ 317): depois de 300 quadros parado (`DA8`), depois de ter sido
     arrastada mais de 30 quadros (`DF8`), ou caindo há tempo; dura 60 quadros (`DAC`);
  3. *balanço* (`D40 ≠ 0`): rampa, escalada, mira ou alvo travado inclinam a câmera e o deslocamento vai de `D10` para `D10 + D50`
     (olhando para cima: chega a (0, −24, 40), perto e baixa) ou `D10 + D60` (olhando para baixo: (0, −4, 260), por cima), com peso
     `sqrt(|D40|/90°)`. É o único "zoom automático" do jogo.
  Voando não há regime próprio (o voo usa o estado do pulo `M+0x7978` [provável]); com alvo travado, a distância é a mesma coleira.
- **Inclinação:** `D00` (−10°) + pulo (+5°/−6°) + `D40` suavizado (rampa ±45° escalado pela inclinação do chão, escalada +10,5°/−16,5°,
  ou o ângulo até o retículo com alvo/mira), limitada a ±89°, suavizada a 0,05/quadro. Independe da guinada. **[certo]**
- **"Correr para as costas"** **[certo]**: não é um `smoothEasyInAngle` sobre o heading do monstro. São duas peças:
  1. *trava do setor traseiro* (`E60 == 0`): a câmera fica a até ~55–60° (`DB8` = 60°) das costas do monstro; se o monstro gira e
     ela sai do setor, é girada em volta dele, mesmo raio horizontal, até a borda, **teleportada** (sem suavizar) quando passou mais
     de ~0,6°; cada quadro arrastado soma `DF4`;
  2. *volta às costas*: arrastada mais de 30 quadros (e soltou) ou 300 quadros em Idle, o alvo vira `frame*D10` por 60 quadros, a
     0,08 por quadro (com o contorno do `translateCamera`, `E48`).
  A guinada sai da posição: a cada quadro `rot.z` = rumo até 150 à frente do monstro, com taxa `DE8` = 1 (instantâneo) fora da rampa
  de 30 quadros que segue uma troca de POV ou de trava.
- **Alvo travado (`hasPinTarget`)** **[certo, menos a 1ª linha]**: (a) o monstro travado encara o alvo e a trava do setor põe a câmera
  atrás dele, então o ponto 150 à frente cai na direção do alvo [provável: o giro do monstro para o alvo não foi lido]; (b) a
  inclinação mira o retículo (limitada por `m_pinMaxPitch` quando os dois estão na mesma altura) e o deslocamento balança com isso;
  (c) sem tremor; (d) ao travar/destravar a guinada refaz a rampa de 30 quadros; (e) para alguns tipos (`typeBits` 0x40/0x160/0x100
  no estado especial, com `sPinTestFOV`) gira até caberem alvo e monstro.
- **`getFootHDResult`**: só para o extra de rampa (chão tipo 3 na byte alta de `+2`, normal em `+0x20`, `normal.z` em `+0x28`).
  `posInFOV` como acima; `translateCamera(D20, DE0, direto)`; `shakeCamera()` antes da orientação. **Não há teste de chão** (nenhum
  `hdHatTest`); só o `victoryUpdate` e o `closeCombatUpdate` levantam a câmera do chão.
- **Estado que persiste entre quadros** (o que importa para o gancho): a pose em si (`cs->trans` é o ponto de partida da suavização;
  `cs->rot.x/z`), `D20` (alvo, fica de um quadro para o outro dentro da coleira), `D40` (inclinação extra), `E60` (trava armada),
  `DA4` (relógio parado/volta às costas), `DF4` (quadros arrastando), `DE0/DE4/DE8` (taxas), `E34` (relógio do POV), `E64` (trava do
  quadro anterior), `E50..E5C` (quatérnio do contorno do `translateCamera`), `D88` (tremor). Grava também `D30` (zerado; o
  `shakeCamera` o usa como rascunho), `DD8` (heading do monstro no corte), `DEC/DF0`. Lê ainda `CFC` (POV anterior: 6 = sem
  contorno).

## 2. Onde a câmera final é escrita e onde enganchar

- O `_cs` de `C+0xCF0` **é** o `_cs` da vista: `Cameras::Init` faz `C+0x0 = C+0xCF0 = viewGetViewCs(i)`. O `normalUpdate` grava
  `cs->trans` (via `translateCamera` + `shakeCamera`), `cs->rot.x/z` e refaz `cs->mat` com `mathfRotMatrixRPH`. O `viewUpdate(v)`
  do quadro seguinte monta as matrizes de `trans` e `mat`. Na vista unificada, `viewGrappleConfig` faz `viewInfo[0].cs =
  viewInfo[2].cs` (a vista 0 mostra a câmera 2). **[certo]**
- **Não existe campo de guinada para sobrescrever.** O "heading atual" é o próprio `cs->rot.z`, recalculado a cada quadro a partir da
  posição (olhar para 150 à frente do monstro, taxa 1). O que persiste é a posição `cs->trans`; mexer só no `rot.z` dura um quadro.
  **[certo]**
- O jogo briga com uma órbita em três pontos: (1) a trava do setor traseiro teleporta a câmera de volta para ±60° das costas quando
  `E60 == 0`; `E60` só zera com a câmera dentro do setor, então gravar `E60 = 1` depois do `normalUpdate` desliga a trava enquanto a
  câmera estiver fora dele, mas dentro dele o próprio jogo rearma; (2) `DA4 >= DA8` (300 quadros em Idle) e `DF4 > DF8` mandam a
  câmera às costas: zerar `DA4` e `DF4` depois de cada quadro os desliga (e escrever `DA4 = DA8` **liga** a volta às costas, útil
  para recentralizar do jeito do jogo); (3) a guinada sempre aponta 150 à frente do monstro, então com a câmera de lado o monstro sai
  do centro: o gancho tem de recalcular `rot.z` de qualquer jeito.
- O controle relativo à tela lê esta mesma câmera: `updateInput0ConfigSR` usa `m_cameras[gUseUnifiedView ? 2 : m_cameraView].cs`
  (`rot.z` e a linha 0 da `mat`). Escrever a pose no `cs` basta para o analógico seguir a órbita **na configuração SR**. **[certo]**
- **Achado importante para a órbita** **[certo]**: fora da vista unificada o jogo usa a configuração **relativa ao monstro**
  (`updateInput0ConfigCR`, `PadFlags +0x16E4 = 0`). Quem liga a SR é o `M+0x6728`, que é `m_padFlags.f16E8` (0x5040 + 0x16E8): o
  `InitCCCamera*` o põe em 1 para os jogadores humanos, `InitCCMonsters` (e daí `LeaveUnifiedView`) o zera, o agarrão o zera, e o
  `interpretInputs` copia `f16E8` para `f16E4` quando `okToChangeMap()` deixa. A "sensação de que esquerda gira a câmera" é isto: na
  CR esquerda vira/anda o monstro para a esquerda dele e a trava do setor arrasta a câmera. Com a órbita, a câmera sai das costas e a
  CR fica errada (cima = frente do monstro, não da tela): a câmera moderna precisa forçar `f16E8 = 1` para os humanos (o `okToChangeMap`
  troca no próximo momento seguro). O `WOTM_LOCKTOGGLE` já distingue CR e SR (`NATIVO.md`).

**Ponto de gancho recomendado:** embrulhar `Cameras::Update` (já enganchado pela câmera livre, `kCamerasUpdate` em
`wotm_overrides.cpp`; encadear antes da escrita da câmera livre) e tratar cada câmera com `C+0xCF8 == 0` e monstro humano. Antes do
original: guardar o POV de cada câmera e detectar `D98` (corte pendente). Depois do original: impor a pose. Alternativa: embrulhar
`normalUpdate__6Camera` (0x11F630), que dá `this` e já se limita ao POV 0, mas ele é chamado por ponteiro (`UPDATE_FUNK`): conferir se
o override do runtime pega o `jalr` antes de contar com isso.

## 3. Vista unificada ("câmera de perto")

- **Entrada** **[certo]**: `TheGame::gameCheckForCloseCombat` (no `gameUpdate`, fora dos modos 7 e 9), sem `gUseUnifiedView`:
  - *1 jogador:* com a câmera 0 há pelo menos 15 quadros no POV, jogador no chão, fora do pulo, sem flag 0x10 no estado, sem agarrar
    (`m_target`), fora dos estados 0x26/0x27/0x28: a IA mais próxima a menos de `DFC` do POV 1 (300), dentro de ±45° (`E04`) da
    frente do jogador, com linha de visada, viva, no chão, sem agarrar, fora de 0x26/0x27/0x28 (e, se travado, a do alvo) ->
    `InitCCCamera(jogador, ia)`;
  - *2 jogadores:* as duas câmeras há 15 quadros no POV, os dois no chão e livres, a menos de 300, um vendo o outro (FOV e visada) e
    o estado do outro atacando (0x2D/0x2E, ou flag 0x10 com `+0x260 == 2`) -> `InitCCCamera(j1, j2)`: a tela dividida vira uma só
    [provável: condições do 2P lidas do rascunho do m2c].
  - Também chamam `InitCCCamera`: `StatePunch::startChain`, `StateBatSwipe::startChain`, `attachPickupImpaler`, `StateGetupAttack`,
    `StateThrowBack` (volta), `StateGrappleLift` (agarrão: câmera 2 no **POV 0** seguindo quem agarra), `InitCCCameraFromThrow`.
  - Todas passam pelo portão `if (a->m_camUnify == 0 || b->m_camUnify == 0) return;`. `TheGame::SetOkToUnify` (começo de cada
    quadro do `rtMain`) põe `m_camUnify = 1` em todos; o volume de gatilho `doCamUnifyVolume` o zera.
- **Saída:** `closeCombatUpdate` (1P: nenhum jogador no par -> `Leave(0xB)`; nenhuma IA viva a menos de `E00` = 450 por `DA8`
  quadros -> agenda `LeaveUnifiedTask`, que espera o jogador parado; travado e sozinho -> `Leave(0xE)`; 2P: par longe/alto demais para
  o FOV -> `Leave(8/9)`), o `normalUpdate` da câmera 2 no agarrão (par a mais de 450), `StateClimb` (escalando de costas para a
  câmera), `StateThrowBack`, `StateGetupAttack`, `doCamVolume`, morte e `Shell` (fim de rodada, reset). A saída copia a pose da
  câmera 2 para a câmera 0 (1P) ou faz o cruzamento com borrão (2P). **[certo nos nomes; condições do `closeCombatUpdate` resumidas
  do m2c, não linha a linha]**
- **Desligar ("nunca entrar")**: o caminho seguro **já existe** em `native/runtime/wotm_split4.inc` (3–4 jogadores): no gancho do
  `SetOkToUnify`, gravar `m_camUnify = 0` em todos os monstros (`M+0x7970`) em vez de 1. Todos os `InitCCCamera*`/`InitCCMonsters`
  voltam sem unificar, `gUseUnifiedView` nunca liga por aproximação, e o `gameCheckForCloseCombat` fica inofensivo. **[certo]**
- **Efeitos colaterais** **[certo, salvo marcação]**:
  - *controle:* sem a unificada o jogador nunca recebe `f16E8 = 1`, fica sempre na CR (relativa ao monstro). Com a órbita isso
    precisa ser forçado à parte (seção 2);
  - *IA:* `MonsterDynamics::updateTurnMotion` dobra a velocidade de giro das IAs (`s_aiTurnFactor` = 2) só com `gUseUnifiedView`;
    sem a unificada a IA gira mais devagar de perto (muda a luta);
  - *mira automática:* `Monster::getClosestMonsterToPunch` só faz a segunda busca (qualquer monstro vivo) com a unificada;
  - *2 jogadores:* a tela dividida nunca vira uma; o agarrão entre os dois não passa para a câmera 2; retículos e destaques ficam por
    vista (`rt.cpp`, `Pickup.cpp`);
  - *agarrão 1P:* o `StateGrappleLift` ainda faz `SetCameraPOV(2, POV_0)` na câmera 2, que não aparece (a vista 0 segue na câmera 0);
  - *vitória:* `StateVictory` liga `gUseUnifiedView = 1` direto (sem `InitCCCamera`) com a câmera 2 no POV 7: continua igual;
  - `checkCollision`, `GrappleThrow`/`Grapple` e o `StateThrowBack` só olham `gUseUnifiedView`, que fica 0: seguem o caminho de tela
    separada, que é o normal fora da luta de perto.

## 4. Os outros modos e a volta sem salto

Todos partem do `cs` atual e o suavizam (nenhum teleporta, fora o `victoryUpdate` e o primeiro quadro do `crushUpdate`):

- `staticUpdate` (3): desliza até `D20` (onde estacionou) e gira para o monstro. Sai por `ReattachCamera` -> `SetCameraPOV(anterior)`
  + `D98 = 1`: **corte** para as costas.
- `getupUpdate` (4): o deslocamento `D10` num referencial `E70` (montado do chão pelo `StateThrowBack`), rampa de 30 quadros. Sai por
  `StateThrowBack::handlePreemption`: `SetCameraPOV(v, 0)` e **zera `D98`/`D9C`**: volta suave (rampa das taxas a partir de onde
  estavam).
- `knockbackUpdate` (5): mantém a câmera entre 280 e 320 do monstro (empurra girando 2°/quadro se perto), olha `M+0x3E30`. Sai como o 4.
- `topdownUpdate` (6/8): por cima do monstro, olhando para baixo, com o heading dele. Do 6 para o 0 (`StateButtSlam`) o
  `SetCameraPOV` vê `CF8 == 6` e zera `D98` e as taxas: volta suave com `ResetPOVChange(20)`.
- `victoryUpdate` (7): posição fixa no referencial do monstro, chão por `hdHatTest`, sem suavizar. Sai pelo reset da rodada (modos 8/9:
  `SetCameraPOV(0, 0)` com corte).
- `crushUpdate` (9) e `roamUpdate` (2): fora do jogo normal.
- `closeCombatUpdate` (1): na saída 1P a câmera 0 recebe a pose da câmera 2 com `E60 = 1` e rampa de 30 quadros: sem corte.

**Corte** = `D98 != 0` no começo do `normalUpdate`: põe a câmera às costas na hora (`SetCameraPOV` liga `D98` sempre, exceto vindo
do 6, indo para o 4, ou na câmera 2; quem quer suave o zera depois). Para a órbita voltar sem salto: (a) guardar o POV de cada câmera a
cada quadro; ao ver uma câmera voltar ao 0 (ou `gUseUnifiedView` desligar), ressincronizar o ângulo da órbita com o `cs` que o outro
modo deixou (`heading(monstro -> câmera)`); (b) se `D98` estiver ligado nesse quadro, decidir: zerar `D98` e `D9C` antes do
`Cameras::Update` (sem corte, continua de onde está; é o que o próprio jogo faz no fim do arremesso) ou deixar o corte e pôr o ângulo
nas costas. O dono pediu "sem salto": zerar. O L2 (`TogglePOV`, 0 <-> 3) passa por esse caminho e precisa da mesma decisão.

## O que conferir no runtime

- Os valores de `M+0x6CF0..` (tabela de POV) de cada monstro, para confirmar que são os padrões de `initCameraData`.
- A altura "grudada" da coleira: logar `cs.z - monstro.z` subindo rampa e prédio.
- Os ids de estado 0x20, 0x26 e 7 (inclinação direto para o monstro) e o tipo de chão 3.
- Se o override de `normalUpdate` pega a chamada por ponteiro do `UPDATE_FUNK`.
