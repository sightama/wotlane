# Seleção nativa de monstros: pendências para o contribuinte (2026-10-23)

Relatos do dono sobre a seleção nativa das telas 3 e 6 (1 e 2 jogadores, branch `skins-mod`, `native/runtime/wotm_padsel.inc`,
seção "selecao de 1 e 2 jogadores") e sobre a luta. Os itens 1 e 2 do mesmo relato (barra de monstros fora do telão, faixa de
trajes vertical) foram feitos na própria branch; os de baixo ficam para o contribuinte.

Contexto do código:
- A seleção nativa é um "procurador": o jogo segue com a lógica das telas 3 (`screenCharSelect1P__Fv`) e 6
  (`screenCharSelect2P__Fv`); o nativo esconde a tela, desenha os quadros e intercepta cima/baixo no `screenGetInput`
  (`soloInput`). `soloPanels` decide quantos quadros e de onde vêm cursor, traje e "pronto" de cada um.
- 3 e 4 jogadores humanos usam a outra seleção nativa (`selectStep`, `g_multi`), que já existia antes das skins.
- `WOTM_NATSEL=0` desliga a nativa das telas 3 e 6 (volta a tela do jogo).

## 3. Robo-47: glitches pretos no céu atrás do mapa

Só com o Robo-47 (ROBOT) na luta, o céu atrás do mapa ganha manchas pretas. Não foi testado com Zorgulon (ALIEN) nem Raptros
(DRAGON): desbloqueie os dois no `save.json` (destraves) para conferir se é só o Robo-47. Não tem relação com as skins (traje do
disco). Pistas: o que o Robo-47 desenha a mais que os outros (laser, brilho, partículas aditivas) e se algo dele escreve na
profundidade ou no alfa do céu; comparar com a checagem VU1 (`docs/passos/VU1CHECK.md`, `WOTM_VU1CHECK`) numa luta
`WOTM_MATCH` com o Robo-47.

## 4. Free-for-All com 2 ou 3 CPUs não mostra a seleção nativa (feito em 2026-10-23, falta o dono conferir o de 2 jogadores)

Com mais monstros do que cabem na tela 6, o "first to" (tela 15) manda à tela 18 (`screenCharSelect3P`, 3 monstros) ou 19
(`screenCharSelect4P`, 4): 1 jogador com 2 CPUs → 18, com 3 → 19; 2 jogadores com 1 CPU → 18, com 2 → 19. As duas agora usam a
seleção nativa (`soloPanels` monta até 4 quadros; ganchos `select3PHook`/`select4PHook`). Quadros, tirados do C++ da 18 e do asm
da 19: modo 2 (1 jogador), o controle 1 escolhe o dele e depois cada CPU na mesma coluna (`curCostume2/3/4`, `aiOne/Two/ThreeIsDone`,
`m_costume[2..4]`, `m_monsterSel[4..6]`); 2 jogadores, cada controle o seu, e a CPU 1 é do controle 1 depois dos dois prontos; na 19
a CPU 2 é do controle 2 (coluna dele, `curCostume4.3010`) depois do pronto dele. Rótulos CPU1, CPU2... com mais de uma CPU. Escondidos
também os painéis `m_cs7C[0..2]` e os cursores de monstro (`shell + 0x1C0..0x1D8`). Conferido por roteiro (`PS2X_PAD`,
`WOTM_SHOTS`) no single player com 2 e 3 CPUs até a seleção de fase; o multiplayer de 2 com CPUs precisa de dois controles.
Foto: `docs/screenshots/2026-10-23/2026-10-23_selecao_nativa_tela19_3cpus.png`. A tela 17 (`screenSelect1AI`) não aparece nesses
caminhos e continua sem a nativa.

## 5. Transição esquisita ao confirmar em modos com seleção de fase

Fora do adventure, ao confirmar os monstros a câmera do menu tenta voltar à posição padrão e, ao mesmo tempo, ir para a tela de
seleção de fase. Conferir o que o `soloExit` / o fim do `transitionHook` fazem com a câmera e com o alfa quando a tela seguinte não
é a carga da luta (no adventure ela vai direto para a carga), e comparar com a mesma transição com `WOTM_NATSEL=0`.

## 6. Togera desalinhado para a esquerda no adventure

O Togera 3D aparece deslocado para a esquerda no quadro único do adventure (tela 3). O enquadramento vem de
`wotm::savepics::Drawer::framingOf(monstro | traje base << 8)` (`shiftX`/`shiftY`/`zoom`), calibrado para os quadros da lista de
saves, e o quadro único da seleção tem outra proporção (`selBoxW`: no máximo 46% da largura). Conferir os outros monstros no
mesmo quadro; talvez o `shiftX` precise ser por proporção do quadro, ou o quadro da seleção precise de um enquadramento próprio.

## 7. Multiplayer > Elimination não deixa escolher skins

No elimination (modo 6) a seleção nativa esconde o traje (`noCostume` em `soloPanels`), como o jogo, que não escolhe traje nesse
modo. Com skins de mod o dono quer poder escolher o traje também ali. Ver onde o jogo grava o traje de cada jogador no
elimination (ou se usa o traje 0 fixo) e liberar cima/baixo e a faixa de trajes nesse modo.

## 8. Quadros dos modelos 3D presos ao telão

Os quadros dos jogadores (rótulo colorido, monstro 3D, nome, traje) são desenhados numa textura colada no telão
(`drawSelect` -> `g_rt`, depois o quad do telão), o que deixa tudo pequeno e inclinado. O dono quer os quadros fora do telão, em
cima da imagem, confinados entre a faixa preta de cima e a barra de monstros nova (que fica logo acima da faixa preta de baixo).
Vale para as duas seleções nativas (telas 3/6 e 3/4 jogadores).
