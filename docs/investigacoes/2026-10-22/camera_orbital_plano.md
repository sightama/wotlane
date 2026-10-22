# Câmera moderna (orbital): plano (item 7 do `POS_FRONTEIRA.md`)

Combinado com o dono em 2026-10-22. Branch `camera-orbital`. Cada etapa vira um commit; o dono testa entre elas.

## O que já se sabe
- **A entrada já é relativa à câmera na luta.** O jogo usa a configuração relativa à tela (`updateInput0ConfigSR`, `PadFlags +0x16E4`
  diferente de 0; `NATIVO.md`, linha do `WOTM_LOCKTOGGLE`). A sensação de "esquerda gira a câmera" vem do `Camera::normalUpdate`
  (1084 instruções, só em asm): ele não lê o controle e vai se ajeitando atrás do monstro, na direção em que ele olha
  (`smoothEasyInAngle`, `mathfHeadingFromPointToPoint`). O monstro vira para a esquerda, a câmera corre para as costas dele e a tela
  gira. A etapa 0 confirma.
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
| 1 | Órbita básica | gancho na saída do `normalUpdate` (vista 0, sem alvo travado): mantém distância e altura do jogo, o ângulo em volta do monstro passa a ser do analógico direito; sem correr atrás das costas; recentralizar devagar |
| 2 | Alvo travado | enquadramento do jogo, com o analógico direito deslocando um pouco |
| 3 | Chão e prédios | câmera não entra no chão nem em prédio; conferir se os prédios transparentes bastam |
| 4 | Transições | ao voltar de agarrão, cena, aproximação (vista unificada), levantar etc., a órbita recomeça do ângulo atual, sem salto |
| 5 | Tela dividida, 3 e 4 jogadores | estado de órbita por vista, cada uma pelo controle do seu jogador |
| 6 | Painel | aba nova ou FEATURES: CAMERA (MODERN/CLASSIC), sensibilidade, inverter Y, recentralizar; `WOTM_CAMERA` etc. |
| 7 | Mouse na câmera | mouse gira a câmera do jogador do teclado; cursor preso na luta, solto nos menus e na pausa |
| 8 | Teclado novo | layout padrão com WASD e ataques nos botões do mouse; botões do mouse no rebind (`wotm_settings.hpp`, aba CONTROLS); proposta de layout para o dono aprovar antes |
| 9 | Menus com mouse | primeiro os nativos (menu principal em texto, painel, escolha de controles do multiplayer); depois as telas do jogo (options, saves, pausa, seleção de monstros), que precisam da posição de cada item na tela |

Etapas 7 a 9 podem andar em paralelo à 2–5 (não dependem da órbita além do gancho do mouse).

## Perguntas em aberto
- O que o analógico direito e o L2 fazem hoje no jogo (o L2 é a "câmera" do jogo: `TogglePOV` entre POV 0 e 3, `Monster.cpp`); com a
  órbita, o L2 ainda faz sentido?
- Zoom (distância) pela roda do mouse ou pelo POV?
- Vista unificada (os dois monstros perto): fica a do jogo ou também ganha órbita?
