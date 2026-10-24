# Skins de mod: onde o traje é escolhido e como passar de 4 (item 8 do POS_FRONTEIRA)

Investigação só de leitura (2026-10-23). Nada foi mudado. Base: `src/game/screen.cpp` (quase tudo em C++ `NON_MATCHING`, com o
`INCLUDE_ASM` no `#else`), `src/game/Shell.cpp`, `native/runtime/*.inc`. Os endereços no nativo vêm de `native/wotm_syms.h`
(`wotm::fn::screenCharSelect1P__Fv`, `wotm::sym::curCostume1_2833` etc.).

## 0. Resumo

- O traje é um índice simples até o disco: `m_costume[p]` → `Shell::formatFilename1` monta `\MON\<NOME><traje>.<ext>` com `%d`
  (`src/game/Shell.cpp:1736`), também para as IAs (`Shell.cpp:1284`, `:1357`, `:1454`). O `m_skinNum` do `Monster` (gravado em
  `TheGame::SetPlayerMonster`/`SetAIMonster`, `src/game/TheGame.cpp:331`, `:368`) só é lido pelo som (`src/game/Sound.cpp:2104`,
  `:2113`, `:2168`: traje 1 do Ultra V / traje 3 do Agamo); traje ≥ 4 cai no som padrão. **Depois da seleção não há limite de 4.**
- O limite de 4 está todo na **seleção**: tabela `monsterSelectMode` (12 grupos × 4 `SelectSlot`, stride 0xB0) com `state`
  (destravado) e `taken` (já escolhido por alguém), cursores estáticos `curCostume*` presos em 0..3 por `CS_WRAP` a cada quadro, e
  4 switches de painel por jogador no `SHELL.NGP`.
- Proposta: **cursor do jogo vira "procurador"** (fica num traje 0..3 aberto) e o nativo guarda o índice real; o painel do jogo é
  escondido e um seletor nativo (lista com o monstro 3D) mostra os N trajes; na confirmação o nativo grava o índice real em
  `m_costume[p]`. Não precisa mexer na decomp nem no ROM.

## 1. Telas onde o traje é escolhido ou visto

Despacho das telas: `src/game/ui.cpp:330-351` (`currScreen` → função).

| Tela | Função | Estado | Quem escolhe | Cursor de traje | Grava |
|---|---|---|---|---|---|
| 3 | `screenCharSelect1P` (`screen.cpp:762`) | C++ NM (34/1089) | 1P: aventura (m_mode 1), free-for-all sem IA, resistência (m_mode 4) | `curCostume1.2833` | `m_costume[0]` (`:921`) |
| 17 | `screenSelect1AI` (`:996`) | C++ NM | IA do 1P, uma por vez | `curCostume1.2847` | `m_costume[2 + AINum]` (`:1227`) — **inalcançável** (só ela chama `changeScreen(…,17)`; `75f5be1:docs/investigacoes/2026-10-22/mouse_telas_monstro.md`) |
| 6 | `screenCharSelect2P` (`:2267`) | C++ NM (44/3238) | 2P; 1P + 1 IA (modo 2: o jogador escolhe a IA no 2º painel); eliminação (modo 6: traje sempre 0, sem painel) | `curCostume1.2894`, `curCostume2.2895` | `m_costume[0]` (`:2556`, `:2592`), `[1]` (`:2707`), IA `[2]` (`:2578`) |
| 18 | `screenCharSelect3P` (`:1680`) | C++ NM (reescrita sem afinar) | 2P + 1 IA; 1P + 2 IAs (modo 2) | `curCostume1/2/3.2868..2870` | `m_costume[0]` (`:2069`), `[1]` (`:2176`), `[2]` (`:2016`, `:2039`), `[3]` (`:2053`) |
| 19 | `screenCharSelect4P` (`screen.cpp:4464`, só `INCLUDE_ASM`; 0x5788 bytes) | **só asm** | 2P + 2 IAs; 1P + 3 IAs | 4 estáticos `curCostume1..4.3007..3010` (`wotm::sym::curCostume1_3007`…) | `m_costume[0..4]` (lê/grava Shell+0x2BE0..0x2BF0); usa os painéis `m_cs7C[0..2]`, `m_costumeBoxCs/2` e os switches `m_costumeSw`, `m_costumeSw2`, `m_swD8[0..2]`; chama `isSlotOpen` 24×, `findAvailableMonster` 24×, `modelsLeft`, `resetSelectables` |
| 7 | `screenMinigames2P` (`:2792`) | C++ NM (35/1989) | 2P dos minigames | `curCostume1/2.2912/2913` | `m_costume[0]` (`:2916`), `[1]` (`:2988`) |
| 21 | `screenUnlocker` (`:4313`) | C++ NM (12/867) | compra de trajes e fases | `s.2990` (linha), `m.2989` (grupo) | `g_monsterToUnlock`/`g_skinToUnlock` → `saveUnlocks` (`:4422-4440`) |
| 6 (3–4 jog.) | `native/runtime/wotm_padsel.inc` (seleção nativa no telão) | nativo | 3 ou 4 jogadores, sem IA | `g_pick[p].costume` (`:98`) | `m_costume[0..1]` e `wotmMultiSetCostume` p/ 3 e 4 (`:510-521`) |

Caminhos: 1P modos → tela 3 (`screen.cpp:619-700`); 1P free-for-all com IA e todo multijogador passam pela 15 ("first to") e
`BO_NEXT_SCREEN` escolhe 6/18/19 pelo número de monstros (`screen.cpp:3893-3906`).

Sem escolha de traje (mas usam o traje):
- **Aventura (m_mode 1):** IAs vêm de `levelMonsters`/`levelMonsterModels[fase][4]` (`screen.cpp:796-799`, `Shell.cpp:619`, `:753`).
  O save da aventura guarda o traje (`native/runtime/wotm_save.inc:781`, `:972`, `:1003`) e a lista de saves desenha o monstro com
  `costume & 3` (`wotm_save.inc:918`, `native/wotm_savepics.hpp:182`, `:343`).
- **Resistência (m_mode 4):** `Shell::RandomlySelectAI` (`Shell.cpp:345-361`) sorteia monstros olhando só o traje 0 de cada grupo
  e não grava `m_costume[2 + i]` (fica o que estava).
- `WOTM_MATCH` (`native/runtime/wotm_overrides.cpp:204-247`): trajes com `& 3`.

### Suposições de "4" (todas teriam de aceitar índice ≥ 4 ou ser contornadas)

No jogo (C++ em `screen.cpp`; a 19 tem as mesmas no asm):
- `CS_WRAP` (`:729`, `:1465`): `<0 → 3`, `>3 → 0`, roda **a cada quadro** no laço "anda até um traje aberto" (`:856-862`,
  `:874-890`, `MG_FIND_COSTUME` `:1491-1507`, `C3_FIND` `:1611-1620`). Escrever 4+ no `curCostume*` volta a 0 no quadro seguinte.
- `CS_SLOT(sel, c) = monsterSelectMode[(sel-1)*4 + c]` (`:730`); `isSlotOpen` endereça `group*0xB0 + slot*0x2C` (`:4836-4843`): slot 4
  já é o traje 0 do monstro seguinte.
- Laços `i < 4` para acender switches e contar abertos (`:814`, `:838`, `:867`, `MG_SHOW_COSTUMES`/`MG_COUNT_COSTUMES`
  `:1472-1489`, `C3_SHOW`/`C3_COUNT` `:1591-1608`); `i = 3..0` ao marcar `taken` (`:942`).
- `modelsLeft` conta 4 (`:4869-4882`); `findAvailableMonster` pula monstro sem traje livre (`:4925-4962`); `resetSelectables` 12×4
  (`:4848-4856`; o `screenCharSelect1P` chama a cada quadro, `:832`).
- Unlock: `unlockItems = 4` (3 nos grupos ≥ 11), cursor com volta 0..3, confirmação só aceita `sel` 0..3 (`:4327`, `:4400-4421`,
  `:4424`). "FREEBIE" para trajes 0 e 1 dos monstros 1..8 (`:4356`).
- Dados: `SelectSlot` (`include/game/monster_select.h`) tem `taken`, `state`, `cost`, `costText`, `name` só para 48 entradas; o save
  binário guarda 48 `state` (`92af448:docs/passos/POS_FRONTEIRA.md`:120-131`), o JSON `"<monstro>": {"skins": [4 bools]}` (`wotm_save.inc:133-139`).

No nativo:
- `wotm_padsel.inc`: `firstCostume` `c < 4` (`:470-475`), `(costume + dir + 4) % 4` (`:575-578`), contagem `c < 4` (`:341-346`),
  modelo 3D `id = monstro | (traje & 3) << 8` e `load(pre + ((id >> 8) & 3))` (`:1109`, `:1136`), pontos de traje (`:1201-1208`).
- `wotm_split4.inc:881`: `g_extraCostume = costume & 3`.
- `wotm_gamescreens.inc` (mouse nas telas 3/6/7): caixa de 4 switches (`:205-240`), `columnValid` `c < 4` (`:337-344`).
- `wotm_overrides.cpp:231-247` (`WOTM_MATCH`), `wotm_save.inc:918`, `wotm_savepics.hpp:182`, `:343`.

## 2. Como o painel de 4 é desenhado e como o nativo já esconde coisas

- **Painel:** um `_cs` por painel (GROUP do `SHELL.NGP`, ligado por `Shell::AddEpNode`, `Shell.cpp:1526-1570`): `m_costumeBoxCs`
  (ep 240), `m_costumeBoxCs2` (241), `m_costumeBoxCsMG[2]` (350, 355), `m_cs7C[0..2]` (237..239). `drawMe` em cs+0xC
  (`hierSetCsDrawMe`), alfa float em cs+0x7C (fade feito pela tela: `screen.cpp:829`, `:1750`, `:2336`).
- **Botões:** 4 `_hierswitch` por painel (`include/game/shell.h:25-29`): `m_costumeSw[4]` (242..245), `m_costumeSw2[4]` (246..249),
  `m_costumeSwMG[2][4]` (351..359), `m_swD8[3][4]` (360..371). Quadro 0 fechado, 1 aberto, 2 cursor; o quadro é o byte +8 do switch.
  As caixinhas são genéricas (mesma arte para todo monstro; traje 0 em cima). **O retail não mostra prévia do traje**: a imagem
  grande (`m_sw5CC`) é por coluna/monstro (`screen.cpp:891`, `:910-912`).
- **O jogo reescreve tudo a cada quadro** (switches, `drawMe 1`, alfa), então esconder exige reaplicar na **saída** da função da
  tela, como já faz o nativo:
  - `wotm_padsel.inc:619-629` `hideSelect2P`: alfa 0 nos itens da tela 6, `drawMe = 0` nos dois painéis (+0xC), marcas "1"/"2" dos
    retratos zeradas; ganchos de entrada/saída do `screenCharSelect2P` por RA (`select2PHook`, `:697-740`; tabela `:1680`). Troca de
    tela pedida pelo nativo só na saída (`g_after`, `:106-110`).
  - `wotm_mainmenu.inc:8-15` e `wotm_optscreen.inc:7-20`: alfa 0 (+0x4C) nos objetos dos switches a cada quadro e lista desenhada
    numa textura própria posta no plano dos itens; o cursor continua sendo o do jogo, o gancho do `screenGetInput` consome
    cima/baixo e acerta o `currentSelection` (ou deixa o +1/−1 do jogo cair na linha certa).
  - Desenho no telão (`wotm_padsel.inc:1081-1160`): monstro 3D por jogador com `wotm::model::Renderer` + cache por
    (monstro, traje), `Model::load` lê via `wotm_disc` e **os arquivos de `mods/` já vencem o disco** (`native/wotm_disc.hpp:3`,
    `:115`); um `LIZARD4.NGP` novo carregaria na prévia se o `& 3` sair.
  - Mouse (`wotm_gamescreens.inc`): acha as caixinhas projetando os switches e anda pelos códigos do pad (`Goal`, `:320-420`).

O padrão serve: esconder o painel (drawMe 0 do cs) na saída da tela e desenhar um seletor nativo no lugar, com a entrada pelo
gancho do `screenGetInput` que já existe.

## 3. Proposta

### Modelo de dados (nativo)
- **Descoberta:** um traje de mod é `MON/<PREFIXO><n>.NGP/.PTR/.RTX/.TEX` com n ≥ 4 no registro de mods (`wotm::mods::Registry`;
  prefixos de `wotm::savepics::monsterFile`/`MonsterLongNames`). Opcional: `skins.json` no mod (nome, preço, "grátis") para a tela 21.
- **Estado por (monstro, n ≥ 4):** `unlocked` (JSON: `"togera": {"skins": [...4], "modSkins": {"LIZARD4": true}}`, por nome,
  como o save já prevê, `92af448:docs/passos/POS_FRONTEIRA.md`:163-168`) e `taken` (espelho do `SelectSlot::taken`, zerado onde o jogo chama
  `resetSelectables`: gancho de saída dele).
- **Índice:** o `m_costume[p]` final recebe n (4, 5, …); o `formatFilename1` já gera `LIZARD4` (e `LIZARD10` com `%d`).

### Seletor por tela — opções

| Opção | Como | Prós | Contras |
|---|---|---|---|
| A. Painel do jogo + página | Os 4 botões mostram 0–3; passar do último abre a "página 2" (trajes 4–7) reaproveitando os mesmos switches, com um indicador nativo de página | Visual do retail; pouco desenho | Paginação escondida; o jogo reacende os switches todo quadro (reaplicar na saída); continua sem prévia do traje; 4 painéis por tela na 19 |
| B. **Lista nativa única** | Painel do jogo escondido (`drawMe 0`), lista/carrossel nativo com N trajes e o monstro 3D (como a seleção de 3–4) no lugar do painel | Escala para N; prévia 3D (o retail não tem); mesmo código nas telas 3/6/7/18/19 e na padsel | Mais desenho (geometria do painel por tela/jogador; 2–4 painéis); o mouse da `wotm_gamescreens.inc` precisa do mesmo modelo |
| C. Híbrida | Retail intocado quando o monstro não tem traje de mod; B só quando há N > 4 | Zero risco sem mods | Duas aparências; a troca de coluna alterna entre elas |

**Recomendação:** B, mas ligada só quando existe ao menos um traje de mod instalado (C no nível do jogo inteiro, não por monstro),
começando pela tela 3 e pela padsel (3–4 jogadores, já nativa: basta tirar os `& 3`/`% 4` e ler a lista de trajes), depois 6 e 7,
por fim 18 e 19 (a 19 só tem asm, então tudo por RAM). Unlock (21): linhas virtuais 4+ no mesmo texto, abaixo das 4 do jogo.

### Mecanismo: cursor procurador (sem mudar a decomp)
1. **Entrada (`screenGetInput`, já com gancho em `wotm_mainmenu.inc:859`/`wotm_padsel.inc`)**: com o seletor nativo ativo, cima/baixo
   do jogador da vez são consumidos (devolve 0) e andam o índice nativo 0..N−1 pulando fechados/tomados. Para 0..3 o nativo também
   escreve o `curCostume*` do jogo (só se `isSlotOpen`, como o mouse já faz); para ≥ 4 o `curCostume*` fica num traje base aberto (o
   procurador), que o jogo aceita na confirmação (`CS_SLOT(sel, cur).state == 1`, `screen.cpp:923`).
2. **Saída de cada tela** (`screenCharSelect1P/2P/3P/4P__Fv`, `screenMinigames2P__Fv`; RA como no `select2PHook`): esconder o painel
   (drawMe 0 do cs) e detectar a confirmação pela transição dos estáticos de "pronto" (`monsterIsSelected.2829[0]`,
   `playerOneIsDone.*`, `playerTwoIsDone.*`, `aiOneIsDone.*` … — todos em `wotm::sym::`). Na transição, com índice nativo ≥ 4:
   `m_costume[p] = n`, `taken` nativo do traje n = 0 e o `taken` do procurador volta a 1 (o jogo o zerou). O "desfazer" (5) reverte.
   Atenção na tela 3: a confirmação já faz `changeScreen` no mesmo quadro (`screen.cpp:932-953`); a gravação na saída ainda vale
   porque a carga só lê `m_costume` no `LoadMonstersDB`.
3. **Limite de procurador:** se os 4 trajes base de um monstro estiverem tomados, o jogo pula o monstro (`modelsLeft == 0`) mesmo
   com trajes de mod livres. Só acontece com 5+ do mesmo monstro (19 com IAs, padsel com 4): aceitar como limite ou ganchar
   `modelsLeft__Fi` (folha, `wotm::fn::modelsLeft__Fi`) para contar os de mod.
4. **Unlock (21)**: grupo de monstro com trajes de mod ganha linhas 4+. `s.2990`/`currentMenuSelection.2985` em 4+ deixam o jogo sem
   destaque e a confirmação dele ignora `sel` > 3 (`screen.cpp:4424`); o nativo desenha as linhas (fonte do jogo,
   `wotm_menufont.inc`, mesma coluna x 230/450, y 82 + 11·i), cuida de cima/baixo nessa faixa, desconta `Shell+0x2B24` e grava o
   JSON; a mensagem usa a linha do jogo (`D_006EEA58` + `loop.2993`/`fadingin.2995`). O mouse da 21 já cobre 8 linhas
   (`wotm_gamescreens.inc:21-45`).

### Menor conjunto de ganchos
- Já existentes, a estender: `screenGetInput__Fi` (entrada), `screenCharSelect2P__Fv` (padsel), `screenTransition__Fb` (fade).
- Novos (entrada/saída por RA): `screenCharSelect1P__Fv`, `screenMinigames2P__Fv`, `screenCharSelect3P__Fv`,
  `screenCharSelect4P__Fv`, `screenUnlocker__Fv`; saída de `resetSelectables__Fv` (zera `taken` nativo).
- Opcional: `modelsLeft__Fi` (limite do item 3 acima). Não precisa: `formatFilename1`, `isSlotOpen`, `findAvailableMonster`.
- Sem gancho, só código nativo: tirar `& 3`/`% 4`/`< 4` de `wotm_padsel.inc` (`:341`, `:470-475`, `:575-578`, `:1109`, `:1136`),
  `wotm_split4.inc:881`, `wotm_overrides.cpp:231-247`, `wotm_save.inc:918`, `wotm_savepics.hpp:182`, `:343`, e o mouse da
  `wotm_gamescreens.inc` (`:205-240`, `:337-344`) no novo seletor.

### Fora das telas (lembrar no item 8)
- Arquivo de mod maior que o do disco pode passar da região fixa de carga (`native/runtime/wotm_mods.inc:8-9`); um `LIZARD4` não tem
  original para comparar: o carregador deveria avisar contra o maior traje base do monstro.
- IAs com traje de mod (aventura: `levelMonsterModels`; resistência: `RandomlySelectAI` não grava traje) ficam para depois.
- Jogadores 3/4 na carga usam o caminho das IAs (`wotm_split4.inc:311-345`): o índice ≥ 4 passa se o `& 3` sair.
