# Câmera moderna (orbital): plano (item 7 do `POS_FRONTEIRA.md`)

Combinado com o dono em 2026-10-22. Branch `camera-orbital`. Cada etapa vira um commit; o dono testa entre elas.

## O que já se sabe
- **Na câmera normal o controle é relativo ao monstro** (dono, 2026-10-22): esquerda/direita giram o monstro (tipo tanque); só na
  vista unificada (câmera de perto) ele anda para onde se aponta (configuração relativa à tela, `updateInput0ConfigSR`, `PadFlags
  +0x16E4`). A suposição do comentário do mod de lock-on ("ConfigSR é o normal na luta") deve estar errada; a etapa 0 confere. O
  `Camera::normalUpdate` (1084 instruções, só em asm) não lê o controle e vai se ajeitando atrás do monstro, na direção em que ele
  olha (`smoothEasyInAngle`, `mathfHeadingFromPointToPoint`). Então a etapa 1 troca as duas coisas: o modo do controle fora da vista
  unificada e a câmera.
- **O encanamento existe:** a câmera livre do F5 (`wotm_freecam.h`/`.inc`, `wotm_overrides.cpp`) escreve a pose no `_cs` da vista
  depois do `Cameras::Update` e na entrada do `viewUpdate`, então o jogo inteiro usa a câmera nova (matrizes, recorte, LOD, partículas,
  som, controle relativo). Convenção de eixos pronta (heading em torno de Z, 0 = +Y).
- **Os outros modos são funções à parte** (`Camera::UPDATE_FUNK`, POV em `+0xCF8`): `closeCombatUpdate` (vista unificada),
  `getupUpdate`, `knockbackUpdate`, `victoryUpdate`, `crushUpdate`, `topdownUpdate`, `staticUpdate`, `roamUpdate`. A órbita substitui só
  o `normalUpdate`; os outros continuam do jogo.
- **Prédios:** o jogo já deixa transparentes os prédios na linha de visada (`hierSetCameraLosPoint`); `Camera::checkCollision` e
  `posInFOV` já estão em C++ (`src/game/Cameras.cpp`).

## Decisões do dono (2026-10-22)
- **Alvo travado:** enquadra os dois, como o jogo já faz; o analógico direito só desloca um pouco.
- **Recentralizar:** sim, devagar (depois de ~1,5 s sem mexer na câmera e com o monstro andando), com opção para desligar.
- **Padrão:** câmera **moderna**. A clássica fica no painel. (Lote e testes automáticos: decidir se fixam a clássica para continuar
  comparáveis.)
- **Teclado e mouse:** o mouse mexe a câmera sempre (cursor preso na janela durante a luta). Isso pede um **layout padrão novo do
  teclado** (a mão direita vai para o mouse) e **menus selecionáveis com o mouse**.

## Etapas
| # | Etapa | Entrega |
|---|---|---|
| 0 | Ler o `normalUpdate` | distância, altura e inclinação por POV (`MonsterCameraData`), enquadramento com alvo (`hasPinTarget`), suavizações, colisão; confirma a hipótese acima. Relatório `camera_normalupdate.md` nesta pasta |
| 1 | Órbita básica (feita, falta o teste do dono) | gancho na saída do `normalUpdate` (vista 0, sem alvo travado): mantém distância e altura do jogo, o ângulo em volta do monstro passa a ser do analógico direito; sem correr atrás das costas; recentralizar devagar |
| 2 | Alvo travado (primeira versão, falta o teste do dono) | enquadramento do jogo, com o analógico direito deslocando um pouco |
| 3 | Chão e prédios | câmera não entra no chão nem em prédio; conferir se os prédios transparentes bastam |
| 4 | Transições | ao voltar de agarrão, cena, aproximação (vista unificada), levantar etc., a órbita recomeça do ângulo atual, sem salto |
| 5 | Tela dividida, 3 e 4 jogadores | estado de órbita por vista, cada uma pelo controle do seu jogador |
| 6 | Painel | aba nova ou FEATURES: CAMERA (MODERN/CLASSIC), sensibilidade, inverter Y, recentralizar, CLOSE-UP CAMERA (ligar/desligar a vista unificada); `WOTM_CAMERA` etc. |
| 7 | Mouse na câmera | mouse gira a câmera do jogador do teclado; cursor preso na luta, solto nos menus e na pausa |
| 8 | Teclado novo | layout padrão com WASD e ataques nos botões do mouse; botões do mouse no rebind (`wotm_settings.hpp`, aba CONTROLS); proposta de layout para o dono aprovar antes |
| 9 | Menus com mouse | primeiro os nativos (menu principal em texto, painel, escolha de controles do multiplayer); depois as telas do jogo (options, saves, pausa, seleção de monstros), que precisam da posição de cada item na tela |

Etapas 7 a 9 podem andar em paralelo à 2–5 (não dependem da órbita além do gancho do mouse).

## Respostas do dono (2026-10-22, segunda rodada)
- **Zoom:** sem zoom manual; distância automática, como a câmera de qualquer jogo (a que o jogo já calcula, ajustada se preciso).
- **Vista unificada:** o que for mais fácil (manter a do jogo, se for o caso) e uma **opção para desligar a câmera de perto** de vez.
- **L2:** o dono não conhecia o `TogglePOV` (POV 0 ↔ 3); a etapa 0 descobre o botão e o que o POV 3 faz.

## Etapa 0: resultado (2026-10-22)
Relatórios: `camera_controle_cr_sr.md` (controle) e `camera_normalupdate.md` (câmera). Resumo:
- **Controle:** `PadFlags +0x16E4` = configuração ativa (0 CR, 1 SR), `+0x16E8` = pedida (Monster +0x6724/+0x6728). Só a vista
  unificada (`InitCCCamera*`), o Crush e o fim do planeio pedem SR; agarrão, morte, saída da vista unificada voltam a CR. A SR usa o
  `_cs` da câmera da vista do jogador (`rot.z` e linha 0 da matriz), então funciona na câmera normal; o jogo não a usa ali porque o
  `normalUpdate` corre para as costas e o monstro andaria em círculo. Com a órbita, forçar SR nos humanos dá o controle moderno.
- **O "L2 da câmera":** o `TogglePOV` é código morto (`updateViewChanges` não tem chamador). L2 segurado é olhar/mirar para cima e para
  baixo; o analógico direito gira o monstro (X) e olha (Y), e terá de ser zerado no `GamePad` com a órbita.
- **Câmera normal:** pivô na origem do monstro; distância por coleira de 280 a 320; volta às costas depois de 300 quadros parado ou
  arrastada; trava de setor traseiro de ±60° que gira a câmera junto quando o monstro gira (é o "correr para as costas"); balanço
  perto/baixo ou por cima com rampa, escalada, mira e alvo (o único zoom automático); sem teste de chão. Não há campo de heading:
  o que persiste é a posição do `_cs`.
- **Câmera de perto:** todas as entradas passam por `m_camUnify`, que o `split4` já zera para 3 e 4 jogadores: a opção de desligar
  reaproveita isso (efeitos: IA gira mais devagar de perto, tela dividida de 2 nunca vira uma só).

### Desenho da etapa 1
- Embrulho do `Cameras::Update` (o mesmo da câmera livre): antes do original, ressincroniza o ângulo da órbita quando o POV volta a 0 e
  neutraliza o jogo (`E60` = 1, `DA4` = `DF4` = 0, `D98`/`D9C` = 0); depois do original, nas câmeras com POV 0, monstro humano e sem vista
  unificada, refaz a posição com o deslocamento do jogo (`D10` mais o balanço) girado pelo ângulo nosso, `rot.z` para o monstro,
  `rot.x` do jogo, matriz refeita; recentraliza levando o ângulo para as costas depois de ~1,5 s sem mexer, com o monstro andando.
- Controle: no gancho do `interpretInputs` (o do lock-on), com a órbita ativa no jogador, `+0x16E4`/`+0x16E8` = 1 durante a chamada e
  restaurados na saída; os bytes do analógico direito (0x10..0x13 do `GamePad`) zerados e lidos pela órbita.
- Alvo travado (etapa 2): pose do jogo, com desvio limitado pelo analógico direito.

## Etapa 1: resultado (2026-10-22, falta o teste do dono)
`native/runtime/wotm_orbit.inc`, chamado pelos ganchos que já existiam (`Cameras::Update` da câmera livre, `interpretInputs` do
lock-on) e por dois novos (`shakeCamera`, para manter o tremor, e `SetOkToUnify`, para a opção de desligar a câmera de perto).
Opções `WOTM_CAMERA` (padrão moderna), `WOTM_CAMRECENTER` (padrão ligado), `WOTM_CLOSECAM` (padrão ligada), ainda sem linha no painel
(etapa 6). Com `WOTM_AIVSAI` fica a clássica: lotes e corpus continuam comparáveis.

Conferido no runtime (luta 1P com e sem IA, `WOTM_ORBITLOG`, analógico simulado por `WOTM_ORBITSTICK`, pad por `PS2X_PAD`):
- a órbita liga no começo da rodada atrás do monstro, o analógico gira a câmera e, andando para a frente, o monstro vai para onde a
  câmera olha (configuração SR forçada funcionando);
- recentraliza devagar andando, depois de ~1,5 s sem mexer;
- arremessado (POV 5) a câmera é a do jogo; ao voltar ao POV 0 a órbita retoma de onde o jogo deixou, sem salto;
- vista unificada: entra e sai como antes; com `WOTM_CLOSECAM=0` ela não entra e a luta de perto segue na órbita.

Ainda não conferido: agarrão, escalada, planeio, lock-on (etapa 2), tela dividida (etapa 5), sensação com o controle na mão.

### Primeiro teste do dono (2026-10-22) e correções
Relato: giro vertical rápido demais; a câmera girava sozinha andando devagar de lado; ao soltar o lock-on (ou o alvo morrer)
andando, a câmera ficava "doida"; ao travar, a câmera demorava a enquadrar o alvo. Teste do dono sugerido para reproduzir: luta de 2
jogadores, mexendo só no 1 (câmera normal e de perto sem a IA no meio): `WOTM_PLAYERS=2`, `WOTM_MATCH=2,1,1`.
- **Giro vertical:** 0,03 → 0,012 rad/quadro (~40°/s; o horizontal fica ~170°/s).
- **Zona morta radial de 0,2 no analógico direito:** o controle do jogador 2, parado, subia a câmera dele sozinho (folga do analógico).
- **Câmera doida ao soltar o lock-on:** era o recentralizar. Travado, o monstro anda para trás olhando o alvo; solto, ele se vira
  para onde anda, o recentralizar levava a câmera para as costas dele, o "para trás" mudava e ele virava de novo. Agora o recentralizar
  só age com o monstro se afastando da câmera (até 60° da direção dela), de frente para onde anda, por pelo menos 20 quadros seguidos:
  começar a andar de lado vindo de frente para a câmera tem ~1 s de deslocamento da animação de virar (até 2,5 por quadro para longe
  da câmera), que antes puxava a câmera ~15°.
- **Câmera subindo com o alvo travado:** o balanço do jogo (`D40`) segue a inclinação até o retículo medida da câmera; com a câmera
  subindo junto, a inclinação crescia e a câmera ia quase para cima do monstro (distância 42, altura 219). Travado, sem o balanço.
- **Lock-on (etapa 2, primeira versão):** travado, o ângulo vai para trás do monstro na linha do alvo a 0,25 por quadro (~10
  quadros); o analógico direito desloca até ~35° e volta ao soltar; a guinada olha para um ponto 30% do caminho até o alvo, até 120; a
  inclinação é nossa (ponto ~120 acima do pé do monstro, mais 30% da diferença de altura até ±80), com entrada e saída de ~10 quadros,
  em vez da do jogo, que refaz a rampa de 30 quadros a cada travar. Solto, o ângulo continua de onde estava.
- O lock-on alternado (`WOTM_LOCKTOGGLE`) já solta quando o alvo morre; segurando L1+R1 (o do jogo), o `updateLock` passa ao monstro
  mais próximo.

Conferido em lutas de 2 jogadores por roteiro: travar e andar para trás, soltar andando, andar de lado, andar para longe
(recentraliza), atacar de perto (a vista unificada entra).

## Perguntas em aberto
- Com a órbita, o `TogglePOV` ainda faz sentido?
