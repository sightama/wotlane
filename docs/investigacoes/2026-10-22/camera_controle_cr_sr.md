# Controle relativo ao monstro (CR) x relativo à tela (SR), e o botão do `TogglePOV`

Investigação de 2026-10-22 (branch `camera-orbital`), para a câmera orbital (`camera_orbital_plano.md`). Fontes: `asm/` (fora do repo),
rascunhos do m2c e o C++ de `src/game/` (`PadFlags.cpp`, `Cameras.cpp`, `Grapple*.cpp`, `MovementStates.cpp`, `TheGame.cpp`).
Nada aqui foi testado no jogo; é leitura de código.

**Resumo:** o relato do dono está certo e a frase do plano ("a entrada já é relativa à câmera na luta") está errada. Na vista normal
o jogador humano usa a configuração **CR** (controle tipo tanque: esquerda/direita giram o monstro). A **SR** (relativa à tela) só é
pedida na vista unificada, no Crush (modo 9) e no fim do planeio dentro da vista unificada. A SR serve para qualquer câmera, mas só
funciona bem com uma câmera que **não** corre para as costas do monstro; por isso o jogo não a usa com o `normalUpdate`. Com a órbita
(que não persegue o heading) ela vira exatamente o controle moderno. O `f5C` (`TogglePOV`) é código morto no retail.

## 1. `f16E4`, `f16E8`, `f16EC`

Em `PadFlags` (embutido no `Monster` em +0x5040, por isso aparecem como `Monster +0x6724/+0x6728/+0x672C`):

| Campo | Monster | Significado |
|---|---|---|
| `+0x16E4` | +0x6724 | configuração **ativa**: 0 = CR, 1 = SR |
| `+0x16E8` | +0x6728 | configuração **pedida** |
| `+0x16EC` | +0x672C | índice do esquema de botões do menu de opções (0, 1, 2) |

`+0x16EC` vem de `Shell +0x2B8C + 4 * jogador` menos 1 (`Monster::playerInit`, `TheGame::SetPlayerMonster`); a HUD usa o mesmo número
para escolher as dicas de controle (`Hud::updateTextBoxMessage`, `PowerUps::TakeHit` no canyon).

### `PadFlags::interpretInputs` (0x177D20)
```c
if (f16E8 != f16E4 && okToChangeMap())   // pedido pendente: só troca com o analógico parado ou num movimento brusco
    f16E4 = f16E8;
switch (f16EC) {                          // 1 e 2 explícitos; qualquer outro valor cai no 0
case 1:  f16E4 ? updateInput1ConfigSR(pad) : updateInput1ConfigCR(pad); break;
case 2:  f16E4 ? updateInput2ConfigSR(pad) : updateInput2ConfigCR(pad); break;
default: f16E4 ? updateInput0ConfigSR(pad) : updateInput0ConfigCR(pad); break;
}
view = gUseUnifiedView ? 2 : owner->m_cameraView;          // +0x6CD8
if (Cameras::m_cameras[view].pov == 2) {                   // +0xCF8, POV 2 = roamUpdate
    clear(ringIndex, 0); curButtonAction = 0x14; curMappedAction = 0xF; f17E0..f17EC = (0,0,0,1);
}
```
`okToChangeMap` (0x178120) devolve verdadeiro se nenhuma das quatro direções (`PadEntry +0x02..+0x08`) está apertada, ou se alguma
mudou mais que `tweak16E0` desde o quadro anterior. Ou seja: a troca CR↔SR espera o jogador soltar o analógico (ou dar um tranco),
para o "para cima" não mudar de sentido no meio de uma corrida.

`saveAndClear` (início de toda configuração) zera `f17E0..f17E8` só quando `f16E8 == f16E4`; durante uma troca pendente o vetor de
movimento do quadro anterior fica preso (a SR também só o recalcula com os dois iguais).

### Quem escreve (todas as gravações no ELF)
| Onde | O quê |
|---|---|
| `PadFlags::PadFlags`, `PadFlags::init` | E4 = E8 = EC = 0 |
| `Cameras::InitCCCamera`, `InitCCCameraFromThrow`, `InitCCMonsters` (vista unificada) | E8 = 1 para o monstro com `m_playerNum == 1` (humano); `InitCCMonsters` antes zera E8 do par anterior |
| `Cameras::LeaveUnifiedView`, `LeaveUnifiedTask` | via `InitCCMonsters(0, 0)`: E8 = 0 do par que sai |
| `Cameras::SetCameraPOV` (POV 1 com o par do Crush) e `InitCrushMonsters` | E8 = 1 (no `InitCrushMonsters`, dos dois, inclusive IA) |
| `StateFly::enterSubState` saindo do planeio (sub-estado 7) | E8 = E4 = 1 (troca imediata) se humano e (vista unificada sem alvo, ou modo 9) |
| `StateGrappled::detach` | E8 = 0 fora da vista unificada |
| `StateGrappleLift::transitionInto` / `update` | E8 = 0 (levantador e vítima) quando o agarrão põe a câmera 2 em POV 0 (que segue o levantador) e no fim do levantamento |
| `StateDeath::transitionInto` | E8 = 0 |
| `TheGame::ResetLevel`, `UnpauseLevel` | E8 = 0 se não está na vista unificada (e, no unpause, fora do modo 9) |
| `interpretInputs` | E4 = E8 quando `okToChangeMap` deixa |

Situações:
- **Vista normal (POV 0, `normalUpdate`)**: E8 = E4 = 0 → **CR**. Ninguém pede SR fora da vista unificada/Crush.
- **Vista unificada (`gUseUnifiedView`, câmera 2 em POV 1)**: humano recebe E8 = 1 → SR depois do `okToChangeMap`; ao sair, volta a CR.
- **Agarrão**: força CR quando a câmera que segue o levantador é a de perseguição (POV 0) e no fim do levantamento.
- **Voo**: só o fim do planeio mexe, e só já dentro da vista unificada (ou Crush).
- **IA** (`m_playerNum == 2`): E8 = 0 sempre, exceto no Crush. A IA preenche o `GamePad` em `Ai::updateInputs` pensando no tanque.
- **Cinemas**: não há escrita específica; pausar/despausar (`UnpauseLevel`) e `ResetLevel` voltam a CR fora da vista unificada.

A frase do comentário em `native/runtime/wotm_overrides.cpp` ("ConfigSR, o normal na luta; o jogo alterna entre as duas, por
exemplo nas cinemas") e a da linha `WOTM_LOCKTOGGLE` do `NATIVO.md` estão erradas: o normal na luta é CR; SR é a vista unificada.
O código do lock-on continua certo (lê `+0x16E4` e só converte na CR); só o comentário precisa de correção.

### Campos do `PadEntry` que as configurações preenchem (os que importam aqui)
`+0x12/+0x14` frente/trás; `+0x16/+0x18` girar esquerda/direita; `+0x1A/+0x1C` andar de lado; `+0x1E/+0x20` olhar para cima/baixo
(lido por `Monster::updateLookAt`/`updateReticle`); `+0x32` lock-on (L1+R1 na configuração 0); `+0x52` L2 cru. Bytes do `GamePad`
montados por `GamePad::loadPadInputs` (máscara do `inputGetPadButtons`, que lê o pad com o byte 2 do `scePadRead` em baixo):
0 L1, 1 L2, 2 L3, 3 R1, 4 R2, 5 R3, 6 X, 7 círculo, 8 triângulo, 9 quadrado, 0xA Start, 0xB **Select**, 0xC/0xD/0xE/0xF
esquerda/direita/cima/baixo (direcional ou analógico esquerdo), 0x10/0x11 analógico direito X, 0x12/0x13 analógico direito Y.
Os botões 0x100..0x8000 passam pelo remapeamento do menu (`D_00779F20`); Select, Start, L3, R3 e as direções não.

**CR** (`updateInput0ConfigCR`, 0x178D58): cima/baixo → frente/trás, esquerda/direita → girar (tanque); travado e sem L2/R2, o girar
vira andar de lado. Select (byte 0xB) apertado: `Monster +0xF0` (`m_wantsTaunt`) = 1 e o resto do quadro fica vazio: **Select é a
provocação**.

## 2. Como a SR converte o analógico (`updateInput0ConfigSR`, 0x179638; 1 e 2 têm a mesma estrutura)

```c
view = gUseUnifiedView ? 2 : owner->m_cameraView;
if (game->m_gameMode == 9) view = 0;                       // Crush
_cs *cam = Cameras::m_cameras[view].cs;                    // +0xCF0
f17C0 = normalize(sinf(cam->rot.z), cosf(cam->rot.z), 0);  // frente da câmera no chão (heading, 0 = +Y)
f17D0 = normalize(cam->mat[0]);                            // direita da câmera (linha 0 da matriz)
saveAndClear(0); ...                                       // Select = provocação, como na CR
// referencial do monstro:
if (lockOn && !L1 && !R1) fwd = normalize(owner->m_reticleCS->trans - owner->m_cs->trans);   // +0x6BF8: para o alvo
else                      fwd = normalize(sinf(owner->m_cs->rot.z), cosf(owner->m_cs->rot.z), 0);
right = mathfVectorCrossUp(fwd);
// analógico: y = cima - baixo, x = direita - esquerda (0..1), com um atraso de partida:
//   saindo do repouso, cada direção só conta depois de segurada mais que tweak16DC quadros
if (f16E8 == f16E4)
    f17E0 = normalize(x * f17D0 + y * f17C0), f17E8 = 0;   // direção pedida no mundo
if (f17E0 == 0) { frente = trás = girar = 0; }
else {
    a = dot(f17E0, fwd); b = dot(f17E0, right);
    if (lockOn) { a > 0 ? frente = a*255 : trás = -a*255;  b > 0 ? ladoDir = b*255 : ladoEsq = -b*255; }
    else        { if (a > 0) frente = a*255;               b > 0 ? girarDir = b*255 : girarEsq = -b*255; }
}
// o analógico direito X entra como girar só se o esquerdo não girou; o resto (botões, L2/olhar, R2/defesa, ações) é igual à CR
```
E `PadFlags::computeMotionRot` (0x177F00, chamado por `MonsterDynamics`): se `f17E0` tem x e y diferentes de zero, o dono é humano
(`m_playerNum == 1`), sem lock-on, sem os botões de `+0x30`/`+0x2C` e fora de estado especial (`m_state != m_specialState`), o heading
do monstro vai suavemente para `mathfHeadingFromVector(f17E0)` (`smoothEasyInAngle`, taxa em `Monster +0x7974`). Na CR `f17E0` é zero,
então isso não age.

Respostas:
- **Referencial**: o `_cs` da câmera da vista do jogador (`Cameras::m_cameras[m_cameraView] +0xCF0`, o mesmo `_cs` da vista que a
  câmera livre do F5 escreve), usando `rot.z` para a frente e a linha 0 da matriz para a direita. `PadFlags +0x17E0` é a **saída**
  (direção pedida no mundo, z = 0), não a entrada. `+0x17C0`/`+0x17D0` guardam os eixos da câmera do quadro.
- **Dependência de `gUseUnifiedView`** dentro da SR: só na escolha da câmera (vista 2 x vista do jogador). Não há outro teste.
  Forçando E8 = E4 = 1 na vista normal, a SR usaria a câmera do jogador e o monstro andaria relativo a ela.
- **Mas com o `normalUpdate` do jogo isso não presta**: a câmera de perseguição vai para as costas do monstro conforme ele vira.
  Esquerda → o monstro gira para a esquerda → a câmera gira junto → "esquerda" continua apontando para o lado → o monstro anda em
  círculos. É por isso que o próprio jogo força CR quando a câmera 2 vira POV 0 no agarrão. Com a órbita (heading da câmera
  controlado pelo analógico direito/mouse, recentralizar devagar) a SR é o controle moderno pronto, inclusive com lock-on (frente =
  para o alvo, lados = andar de lado), o que tornaria o `lockCameraStick` do mod desnecessário nesse modo.
- Ordem no quadro: `Monster::update` (e com ele `interpretInputs`) roda antes de `Cameras::Update` no `TheGame`, então a SR lê a pose
  do quadro anterior. A escrita da órbita precisa gravar também `rot.z` (+0x68) do `_cs`, não só `mat`/`trans`: a frente vem de
  `rot.z`. O `freeCamWrite` já faz isso.
- Diferenças de sensação: na SR, puxar para trás não anda de ré (vira e corre); há um atraso de `tweak16DC` quadros para sair do
  repouso; `computeMotionRot` ignora direções exatamente nos eixos do mundo (exige x e y diferentes de zero; raro com float).

## 3. Gancho proposto (runtime nativo)

Ponto: **entrada e saída de `PadFlags::interpretInputs`** (`wotm::fn::interpretInputs__8PadFlagsR7GamePad`), que já é ganchado pelo
lock-on alternado (`interpretInputsLockToggle`). Integrar no mesmo gancho (ou ser o gancho externo), porque o lock-on lê `+0x16E4`
na entrada para decidir se converte o analógico; a SR forçada precisa estar escrita antes dessa leitura.

```c
// a0 = PadFlags, a1 = GamePad
Monster *m = pf->owner;                                       // +0x1810
bool want = modernCamOn(m->m_cameraView)                      // a órbita está dirigindo esta vista
         && m->m_playerNum == 1 && !m->m_dead                 // só humano; IA continua no tanque
         && !gUseUnifiedView && game->m_gameMode != 9          // nesses o jogo já decide
         && Cameras::m_cameras[m->m_cameraView].pov == 0       // só POV 0 (normalUpdate substituído); getup/knockback/vitória ficam do jogo
         && pf->f16E8 == 0 && pf->f16E4 == 0;                  // o jogo não está no meio de uma troca
// troca suave como o jogo: só liga num quadro de analógico parado (bytes 0xC..0xF do GamePad zerados) ou se já estava ligado
st.on = want && (st.on || padDirsZero(gp));
saved8 = pf->f16E8; saved4 = pf->f16E4;
if (st.on) pf->f16E8 = pf->f16E4 = 1;
orig_interpretInputs();
if (st.on && pf->f16E8 == 1 && pf->f16E4 == 1) { pf->f16E8 = saved8; pf->f16E4 = saved4; }   // estado do jogo intacto
```
Por que assim:
- Restaurar E8/E4 na saída deixa a máquina de estados do jogo sem rastro: as escritas do jogo (agarrão, morte, vista unificada,
  unpause) continuam valendo, e desligar o modo moderno não exige limpeza. Com os dois iguais a 1 durante a chamada, `saveAndClear`
  e a SR recalculam `f17E0` a cada quadro, sem a trava de transição.
- `computeMotionRot` roda depois (em `MonsterDynamics`, dentro do estado) e lê `f17E0`, que a SR deixou preenchido, então o giro
  suave para a direção pedida funciona mesmo com E8/E4 restaurados.
- Cuidado com a fatia de tempo do runtime (o mesmo `if (ctx->pc == ra)` do lock-on): restaurar só quando a função de fato voltou.
- A desligar quando a órbita não manda na vista (câmera clássica, POV != 0, vista unificada). A vista unificada continua com a SR do
  jogo (câmera 2 em POV 1), o que dá continuidade.
- Testar em particular: agarrão humano x IA na vista normal (o levantador fica em POV 0 e a SR passaria a valer durante o
  levantamento; se ficar estranho, excluir `m_state` de agarrão), escalada (usa as direções cruas `+0x02..+0x08`, iguais nas duas
  configurações), planeio, lock-on (a SR já projeta para o alvo; o `lockCameraStick` deixa de agir porque vê `+0x16E4 != 0`) e as
  três configurações de botões (todas têm SR).
- Alternativa equivalente: trocar a chamada de `updateInputNConfigCR` pela `SR` correspondente; é pior porque a cauda do
  `interpretInputs` e o lock-on continuam vendo E4 = 0.

## 4. `f5C`, `TogglePOV` e os botões de câmera

- `PadEntry +0x5C` só é escrito por `PadFlags::updateViewChanges` (0x178028), e **nenhuma função chama `updateViewChanges`** no retail
  (nenhum `jal`, nenhuma referência fora da própria definição). `saveAndClear` zera o slot a cada quadro, então `f5C` é sempre 0 e o
  `Cameras::TogglePOV` do `Monster::update` nunca roda. Código de depuração deixado no jogo; o dono nunca o viu por isso.
- O que `updateViewChanges` faria: com **Select** (byte 0xB) apertado num quadro novo, **Select + baixo** (byte 0xF) → `f5C = 1`
  (`TogglePOV`); com 2 ou mais monstros (`m_numMonsters`), **Select + esquerda** (byte 0xC) → `viewToggleSplitScreen` e atualiza
  `Cameras::m_numCameras`. No jogo normal, Select é a provocação (CR/SR põem `m_wantsTaunt`).
- `TogglePOV(view)` alterna a câmera da vista entre POV 0 (`normalUpdate`) e POV 3 (`staticUpdate`). **POV 3** = câmera parada: vai
  suavemente (`smoothEasyIn`) até a posição guardada em `+0xD20` e gira só para olhar o monstro seguido (heading e pitch, sem roll).
  `TogglePOV` não grava `+0xD20`, então ela pararia onde o último `DetachCamera` deixou. Usos reais do POV 3: `DetachCamera` (morte no
  agarrão), `LeaveUnifiedView`/`LeaveUnifiedTask` (câmera 2 estacionada ao sair da vista unificada), `InitPlantBossCamera`
  (câmera 0 num marcador do chefe planta), `ResetLevel` fora da vista unificada.
- A "câmera" do jogo no L2 (citada no `NATIVO.md` e no plano) é outra coisa: **L2 segurado** (contador `+0x165A` acima de
  `tweak16C4`) desvia cima/baixo do analógico para `+0x1E/+0x20` (olhar/mirar para cima e para baixo, lidos por `updateLookAt` e
  `updateReticle`) e zera frente/trás; não mexe no POV. O **analógico direito** já faz parte disso: Y vai para olhar (`+0x1E/+0x20`,
  com inversão por `tweak16D8`) quando o L2 não está em uso, X vai para girar (`+0x16/+0x18`) quando o esquerdo não gira. Com a
  órbita, o analógico direito terá de ser tirado desses campos (zerar os bytes 0x10..0x13 do `GamePad` na entrada do
  `interpretInputs`, como o lock-on faz com outros bytes) para não girar o monstro junto com a câmera.
- Outros botões que mexem na câmera: L1+R1 (lock-on, `+0x32`), que faz o `normalUpdate` enquadrar o alvo; e as transições
  automáticas (vista unificada, agarrão, getup, knockback, vitória). Nenhum outro botão troca POV.

## Grau de certeza
1. Alto (todas as gravações listadas por busca no ELF; fluxo do `interpretInputs` lido no asm).
2. Alto para a mecânica da SR; médio-alto para "funciona fora da vista unificada" (dedução do código e da escolha do jogo no agarrão;
   não testado).
3. Médio: o gancho é simples e reversível, mas o comportamento em agarrão, voo e escalada precisa de teste no jogo.
4. Alto: `updateViewChanges` não tem chamador, então `f5C` é sempre 0; o mapeamento Select/baixo/esquerda vem do `loadPadInputs` e do
   `inputGetPadButtons`.
