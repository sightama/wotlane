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
| 1 | Órbita básica (aprovada, na master em 2026-10-22) | gancho na saída do `normalUpdate` (vista 0, sem alvo travado): mantém distância e altura do jogo, o ângulo em volta do monstro passa a ser do analógico direito; sem correr atrás das costas; recentralizar devagar |
| 2 | Alvo travado (aprovado, na master em 2026-10-22) | enquadramento do jogo, com o analógico direito deslocando um pouco |
| 3 | Chão e prédios (aprovado, na master em 2026-10-22) | câmera não entra no chão nem em prédio; conferir se os prédios transparentes bastam |
| 4 | Transições (conferida em 2026-10-22, sem código novo) | ao voltar de agarrão, cena, aproximação (vista unificada), levantar etc., a órbita recomeça do ângulo atual, sem salto |
| 5 | Tela dividida, 3 e 4 jogadores (conferida por roteiro em 2026-10-22, falta o teste do dono com controles) | estado de órbita por vista, cada uma pelo controle do seu jogador |
| 6 | Painel (feita em 2026-10-22, aba CAMERA; falta o teste do dono) | aba nova ou FEATURES: CAMERA (MODERN/CLASSIC), sensibilidade, inverter Y, recentralizar, CLOSE-UP CAMERA (ligar/desligar a vista unificada); `WOTM_CAMERA` etc. |
| 7 | Mouse na câmera (feita em 2026-10-22; falta o teste do dono) | mouse gira a câmera do jogador do teclado; cursor preso na luta, solto nos menus e na pausa |
| 8 | Teclado novo (feita em 2026-10-22, testada pelo dono) | layout padrão com WASD e ataques nos botões do mouse; botões do mouse no rebind (`wotm_settings.hpp`, aba CONTROLS); proposta de layout para o dono aprovar antes |
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

### Segundo teste do dono (2026-10-22)
Relato: soltar o lock-on andando corrigido; "shift" estranho começando a andar para trás, girando no lugar e ao travar/soltar; com o
analógico direito para baixo a câmera subiu e ficou mostrando à frente do monstro (foto da janela pausada: olhando quase direto para
baixo, robô no alto da tela); lock-on um pouco seco.
- Causa comum: a órbita ainda usava a inclinação e o balanço do `normalUpdate`, que mudam com o estado do monstro (virando no lugar e
  começando a andar para trás o jogo mira direto no monstro; travar/soltar refaz a rampa de 30 quadros). Agora a pose é toda nossa:
  distância do `D10` do POV, guinada e inclinação para um ponto ~120 acima do pivô (onde a câmera do jogo mira), que travado anda para
  o alvo com peso suave. Subir a câmera 27° deixa o monstro no centro.
- Lock-on: 0,25 → 0,15 por quadro (~50 ms a mais), no ângulo e na mira.
- Controle relativo à tela liga na hora, mesmo andando (antes esperava o analógico parado e o monstro girava ao sair da câmera de perto).
- Sem o balanço do jogo, rampa e escalada não aproximam nem inclinam a câmera: conferir escalando.

### Terceiro teste do dono (2026-10-22): aprovado
"Funcionando bem agora." Com a câmera de perto religada, o analógico direito girava o monstro (o jogo usa o X no controle relativo à
tela). Com a câmera moderna, o X do analógico direito não chega mais ao jogo em vista nenhuma; o Y chega fora da órbita e mexe a cabeça.
Aprovado e levado para a master. Seguem as etapas 3 em diante.

## Etapa 3: chão e prédios (2026-10-22, falta o teste do dono)
Branch `camera-etapa3`. A órbita chama do próprio código nativo o `hdLosTest` e o `hdHatTest` do jogo (dados e pilha numa área abaixo
da pilha atual do `Cameras::Update`), `collide` em `wotm_orbit.inc`, opção `WOTM_CAMCOLLIDE`.
- **Medição** (`WOTM_CAMCOLLIDE=1`, câmera girando sozinha na Midtown, 1P contra IA): com o monstro no telhado e a câmera baixa, ela
  entrava no prédio (a tela mostrava o teto por baixo); baixando a câmera na rua ela ia até 42 abaixo do chão. A visada da mira até a
  câmera distingue os casos: atrás de um prédio, por fora, a ida bate na face da frente e a volta (da câmera para a mira) na de trás;
  dentro do prédio ou abaixo do chão a volta não sai de nada depois do ponto da ida.
- **Regra:** dentro de algo, a câmera encolhe até 25 antes do contato (na hora; volta devagar, 5% por quadro); por fora, fica, e o
  jogo deixa o prédio transparente (já basta, nas fotos). Chão: pelo menos 30 acima, descendo no máximo 3 por quadro.
- Conferido por roteiro (sem crash; câmera parando na beira do telhado, transparência dos prédios do lado de fora). Falta a sensação
  com o controle, em especial o encolher perto de paredes.

Teste do dono: a câmera não entra mais no telhado nem no chão (aprovado).

### Saída da câmera de perto andando (relato do dono, 2026-10-22)
Contra a IA, afastando-se do outro monstro, a câmera de perto só voltava à normal quando o jogador parava. Não era regressão: o
`closeCombatUpdate` (1P, nenhuma IA a menos de `E00` por `DA8` quadros) agenda o `Cameras::LeaveUnifiedTask`, que só sai com o monstro
parado (`isIdle(7)`) ou travado; no PS2 a troca andando mudaria o controle de relativo à tela para tanque. Com a câmera moderna o
controle segue relativo à tela: o gancho do `LeaveUnifiedTask` (`wotm_orbit.inc`) sai na hora (fora de queda livre) pelo próprio
`LeaveUnifiedView` (com o `+0xE68` zerado ele não remove a tarefa em curso; motivo e alfa do borrão restaurados). Em tela dividida (2
jogadores) a saída é outra e não esperava. Conferido pelo dono e no log (`camera de perto: saida sem esperar o monstro parar`).

## Etapa 4: transições (conferida em 2026-10-22)
A entrada suave da etapa 1 já cobre a volta: ao religar, a órbita parte do ângulo em que o jogo deixou a câmera e desliza da pose do
jogo para a dela em ~20 quadros. Para conferir os dois sentidos, o `WOTM_ORBITLOG` ganhou um medidor de salto (pose final de cada vista,
quadro a quadro; linha `[orbit] camera N: salto` acima de 60 de distância ou 20 graus). Duas lutas de 150 s, Togera parado contra o
Congar da IA (`WOTM_MATCH=3,1,1`), moderna e clássica:
- arremessado (POV 5, ida e volta, 4 vezes), entrada e saída da câmera de perto: nenhum salto na vista do jogador;
- os saltos que sobram são do próprio jogo e aparecem igual na clássica: a câmera da vista unificada (vista 2) entrando, as câmeras da
  introdução e o recomeço de rodada (corte do jogo; a órbita recomeça das costas, como planejado).

Não passou no teste: escalada e agarrão do jogador (o medidor fica ligado com o `WOTM_ORBITLOG` para quando o dono notar algo).

### Câmera do agarrão (relato do dono, 2026-10-22)
Segurando outro monstro, a câmera era a de tanque do jogo: o `StateGrappleLift` liga a vista unificada com a câmera 2 no POV 0
(`normalUpdate`) seguindo quem agarra, e a órbita se desligava com qualquer vista unificada. Agora, na unificada, a câmera 2 no POV 0
com monstro humano também é da órbita (a de perto é o POV 1 e a da vitória o 7: continuam com o jogo). Troca de câmera com o mesmo
monstro (vista 0 para a câmera 2 e de volta) continua o estado da órbita, sem recomeço (`[orbit] camera N: continua da camera M`).
Isso também resolveu, para o dono, o lock-on perdido no agarrão e a demora para voltar à órbita normal (a saída da unificada do
agarrão é a do jogo, mas a câmera já é a mesma órbita). O `WOTM_ORBITLOG` ganhou `alvo 0x.. -> 0x..` (troca do `m_pinTarget`).
Validado pelo dono.

## Etapa 5: tela dividida, 3 e 4 jogadores (2026-10-22)
O estado da órbita já era por câmera (`g_cam[vista]`) e o analógico por jogador (`g_in[jogador]`, do `PadFlags` dono do
`interpretInputs`): não precisou de código novo. Para conferir sem controles, o `WOTM_ORBITSTICK` aceita `@jogador` em cada passo.
Roteiro: `WOTM_MATCH=3,1,2,7,1 WOTM_PLAYERS=4` (quadrantes), giro de ~170 graus do jogador 1, depois do 2, do 3 e do 4, subir o 2 e
descer o 3: cada câmera mudou só no passo do seu jogador (log e fotos); a de perto não existe com 3 e 4 jogadores (`wotm_split4.inc`).
Falta: o dono conferir com controles de verdade (cada analógico direito girando só a sua vista), em 2 e em 3–4 jogadores.

## Etapa 6: painel (2026-10-22)
Aba nova CAMERA no "Recompiled options", entre FEATURES e CONTROLS (`wotm_mainmenu.inc`): CAMERA STYLE (MODERN/CLASSIC,
`WOTM_CAMERA`), SENSITIVITY (1 a 10, `WOTM_CAMSENS`, padrão 5 = a velocidade de antes, cada ponto 20%), INVERT Y (`WOTM_CAMINVERT`),
AUTO CENTER (`WOTM_CAMRECENTER`) e CLOSE-UP CAMERA (`WOTM_CLOSECAM`). Gravadas no `settings.ini` como as outras; a órbita lê a cada
quadro. Conferido por roteiro: a aba abre e os valores mudam e são registrados (`[settings] SENSITIVITY = 7`); na luta, a sensibilidade
10 girou 86 graus em 15 quadros (43 com a 5), e com o eixo invertido o mesmo comando para cima desceu a câmera (-8 graus em vez de +14).


## Etapa 7: mouse na câmera (2026-10-22)
`native/wotm_mouse.hpp` liga os dois lados. A janela (`wotm_pad::mouseFrame`, a cada apresentação) publica o jogador do teclado (a
regra do `deviceMap`: o 1 com o teclado junto do primeiro controle; com `WOTM_KBSEPARATE`, o seguinte aos controles; na escolha de
controles do multijogador, a coluna em que o teclado entrou), prende o cursor (`DisableCursor`) enquanto a thread do EE avisa e a
janela tem o foco, e acumula o deslocamento. A thread do EE avisa a cada quadro em que alguma câmera segue o monstro do jogador do
teclado (inclusive as do jogo: queda, arremesso, para o cursor não piscar) e no `interpretInputs` dele; a órbita consome o acumulado
na câmera desse jogador: ~0,0025 rad por pixel na sensibilidade 5 (1000 pixels ≈ 140 graus), para cima olha para cima, inverter e
sensibilidade do painel valem; com alvo travado o mouse só desloca, como o analógico. Na pausa, nos menus, no F7 e no modo foto o
aviso para e o cursor é solto em ~150 ms. `WOTM_MOUSECAM=0` desliga; `WOTM_ORBITMOUSE` simula o mouse nos testes.
Conferido por roteiro (1P): 600 pixels para a direita giraram 85,9 graus e 200 para cima levaram a altura a -28,6 graus (o
previsto); cursor preso na luta, solto no menu de pausa (`PS2X_PAD` com start, foto) e preso de novo ao continuar.
Falta: o dono jogar com o mouse de verdade (sensação da velocidade padrão).

## Etapa 8: teclado novo (2026-10-22, testada pelo dono)
Layout 2 do teclado, o do dono (`wotm_settings.hpp`, `kDefaultKb`): WASD anda (analógico esquerdo), mouse gira a câmera, botão
esquerdo ataque fraco (quadrado), direito forte (triângulo), Espaço pula (X), F ou botão lateral agarra (bola), Shift defende, Q ou
botão do meio trava, Tab/R ou a rodinha trocam o alvo, Z/X andam de lado, 1 especial triângulo+bola, 2 especial X+quadrado, Alt
anda devagar, Enter ou Esc pausam (Esc volta nos menus), Ctrl provoca (Select), setas no direcional. Botões do mouse e a rodinha
entram no mapa (códigos 1000+ e 1010/1011) e podem ser escolhidos na aba CONTROLS.
Ações só do teclado (linhas novas na aba CONTROLS, mapa do teclado): travar, defender e andar de lado viram botões do PS2 conforme o
lock-on alternado (ligado: R1, L1, L2/R2, os botões do mod; desligado: L1+R1, R2, L1/R1, os do jogo; supõem o CONTROL TYPE A). A troca
de alvo não passa pelo direcional (solto, ele andaria): o teclado pede direto ao lock-on (`wotm::mouse::g_targetReq`, consumido no
gancho do `interpretInputs`, `lockCycleTarget`), com ou sem o lock-on alternado. A rodinha vira um toque por passo, só na luta.
`settings.ini` de layout anterior: as `KB_` são ignoradas e o arquivo é regravado (`KB_LAYOUT=2`).
**Andar com a câmera moderna:** o `computeTargetSpeed` pesa a velocidade pela inclinação (PadEntry +0x12/+0x14/+0x1A/+0x1C), mas a
configuração relativa à tela (SR), que a órbita impõe, grava a direção normalizada (curso todo): o Alt (analógico a ~40%) e o
analógico do gamepad pela metade corriam. Na saída do `interpretInputs` forçado, os quatro campos do quadro são multiplicados pela
inclinação do analógico esquerdo (`wotm_orbit.inc`, `inputLeave`). Confirmado pelo dono.
Painel: Enter confirma, Esc/Backspace voltam, Q/E e Tab (Shift+Tab) trocam a aba, Delete limpa (L1/R1 ficaram sem tecla); ícones
de teclado mostram Q/E nas abas e a rodinha no botão do meio. `WOTM_KBLOG` registra o que o teclado manda.
Teste por roteiro (mensagens do Windows na janela): o `[kb]` mostrou os botões certos para todas as teclas e botões do mouse e a troca
de alvo funcionou, mas o monstro não andou na tela, embora o log da órbita mostrasse posição mudando (o dono viu parado no spawn): esse
tipo de teste não serve para movimento, e a posição do log precisa ser conferida antes de ser usada de novo. O Shift não dá para
testar assim (o GLFW solta o Shift que não está fisicamente apertado). Valeu o teste do dono: tudo funcionando.

## Perguntas em aberto
- Com a órbita, o `TogglePOV` ainda faz sentido?
