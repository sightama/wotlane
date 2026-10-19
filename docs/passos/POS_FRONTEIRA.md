# Depois da fronteira de hardware: prioridades (dono, 2026-10-19)

Vale quando a fronteira de hardware cair (`FRONTEIRA.md`, trabalho em outra sessão). Até lá continua a ordem do `PROXIMOS_PASSOS.md`
(inventário do port). A lista é do dono; a ordem e as notas foram combinadas com ele na mesma conversa. Cada item diz o que o repo já
sabe e o que falta descobrir.

## Ordem
| # | Item | Depende de | Custo estimado |
|---|---|---|---|
| 1 | RAM do convidado acima de 32 MB (+ medir 4 vistas) | — | baixo |
| 2 | HUD dos inimigos: vida e energia juntas, retrato à esquerda | — | baixo (base pronta) |
| 3 | Save em JSON legível, por nomes | — | médio |
| 4 | Sem tela de "memory card 1 ou 2"; slots extras no JSON | 3 | baixo/médio |
| 5 | Ícone de salvamento automático | 3 | baixo |
| 6 | Câmera moderna (orbital) com movimento relativo à câmera | — | alto |
| 7 | Mods que adicionam skins e monstros (sem substituir) | 3 | médio/alto |
| 8 | Chefes jogáveis | 7 | alto |
| 9 | "2 players" vira "multiplayer", até 4 jogadores | 1 | alto |
| 10 | Seleção com prévia 3D de todos os selecionados | 1, 9 | médio |
| 11 | Campanha (adventure) com até 4 jogadores | 9, 6 | o mais alto |

Por que essa ordem: 1 destrava metade da lista e é barato. 2 é ganho rápido em cima de um mod que já existe. 3 → 4 → 5 usam o mesmo
gancho de gravação, e o formato do 3 precisa nascer pronto para o 7 (nomes, não índices). 6 vem antes do multiplayer porque 4 vistas
com a câmera antiga ficam piores, e porque a campanha em grupo precisa dela. 8 é um caso particular de "adicionar monstro" (7). A
campanha com 4 fica por último: cinema, chefes e regras foram feitos para 1 jogador.

### 1. RAM acima de 32 MB
- Medido em `investigacoes/2026-10-18/16_monstros.md`: com 8 monstros a carga da METRO vai até 0x23B4300 (35,7 MB) e congela
  (`g_frame` 205). As outras fases cabem; San Francisco e Vegas ficam com ~2 MB de folga.
- É o "degrau 2" daquele plano: `PS2_RAM_SIZE` do runtime é paramétrico (`ps2_memory.h`, potência de 2). Subir para 64 MB e mover o
  teto da área de carga (hoje ~0x1F10000) por gancho em `fileOnlyNgpFile`/`getNextNgpLoadAddr`. Ponteiros de 32 bits e banco plano:
  a relocação do `dbs` não muda. A camada nativa já usa `wotm::Ram{rdram, PS2_RAM_SIZE}`.
- Mexe numa costura do PS2Recomp: regenerar `tools/runtime_patch/wotm-runtime.patch`.
- Conferir junto: `fileStatus` com 14 slots (fase + 12 monstros), `texInfo` com 1000, VRAM residente do `.RTX`.
- **Medir aqui o custo de 4 vistas** (cena desenhada 4 vezes; com 8 monstros a CPU já está em ~9 ms por quadro) antes de investir na
  interface do item 9.

### 2. HUD dos inimigos
- Base pronta: `WOTM_ENEMYBARS=1` (`NATIVO.md`, tabela de variáveis) já troca o ícone de vida por barras de vida (`m_health`/
  `m_maxHealth`) e energia (`StaminaMeter`, +0x460), desenhadas no nativo pelo gancho em `Hud::drawHealthIcon`/`ScreenPolys::addPoly`.
- Falta: as duas barras "grudadas" num bloco só, como no HUD do jogador, e o retrato do monstro à esquerda (as texturas de retrato
  do HUD do jogador já são desenhadas no nativo; reaproveitar a mesma fonte por tipo de monstro).

### 3. Save em JSON
- Hoje o stub `sceMc*` do runtime grava e lê arquivos em `mc0/` do host (`FRONTEIRA.md`, "Pad, CD, memory card"). O jogo
  (`game/McFile`, 59 funções, só asm, fim da fila de decomp) monta um bloco binário e manda gravar.
- Caminho mais barato: converter na camada do host (bloco ↔ JSON) sem decompilar o `McFile`. O jogo continua vendo o mesmo binário;
  o disco guarda o JSON. Precisa conhecer o layout do bloco.
- Primeiro passo: mapear o layout por diferença (gravar, destravar uma coisa, gravar de novo, comparar os arquivos de `mc0/`). Devem
  estar no save: monstros destravados (`monster_select.h`, `state`), fases da tabela `monsterSelectMode` (`levelUnlocked`,
  `ESPINHA.md`), battle tokens (`Shell+0x2B24`), trajes, opções. Conferir se há checksum.
- Formato: destraváveis como booleanos; `saves` como array de objetos; `options` como objeto. **Chaves por nome** (`"congar": {...}`,
  skins por nome), nunca por índice: um mod que adiciona monstro ou skin (item 7) só acrescenta chaves e não embaralha saves antigos.

### 4. Sem escolha de memory card
- Ganchos nas telas do Shell que perguntam o cartão (achar pelo `screenTransition`, como no `wotm_padsel.inc`) para pular direto.
- Hoje, ao carregar do cartão 2, o jogo anima a segunda caixa das fitas de vídeo 3D. No lugar do cartão 2, o JSON aceita mais slots no
  array. **Em aberto:** como o jogador escolhe entre N slots sem a tela do cartão (decidir quando chegar aqui).

### 5. Ícone de salvamento automático
- O ícone 3D do save do PS2 (formato de ícone do memory card) vem junto do que o jogo grava. Extrair e desenhar no nativo no canto
  inferior direito por 1 s, disparado pelo gancho de gravação do item 3. Detalhes quando for fazer.

### 6. Câmera moderna (orbital)
- Hoje, na câmera padrão, esquerda/direita giram a câmera. Quando os monstros estão perto (vista unificada, `gUseUnifiedView`,
  `LeaveUnifiedView`, POV da câmera em `+0xCF8` de cada bloco de 0xEB0 em `Cameras::m_cameras`, `game.h`) o movimento já é relativo à
  câmera: o começo é levar esse movimento para a câmera padrão.
- Não é só isso: órbita pelo analógico direito/mouse, o que acontece com o alvo travado, colisão da câmera com prédios (o jogo já tem
  os prédios "fantasma" por linha de visada), transições com cinema e agarrão, e a câmera de cada vista na tela dividida.
- `Cameras::Update` (tabela `_6Camera$UPDATE_FUNK`) ainda está só em asm; câmera já é superfície de decomp do port. A câmera livre de
  debug (F5, `wotm_freecam.h`/`.inc`) tem a convenção de eixos pronta.

### 7. Mods que adicionam skins e monstros
- Trajes em `m_costume` (o `[2 + j]` atropela `m_mode` a partir da IA 10), tabela de seleção em `monster_select.h` (grupos de 4 por
  monstro), `texInfo` com 1000 entradas, nomes internos em `MonsterNames.h` (17 entradas). Monstro novo passa por `MonsterParse` e
  `Shell::Load*`, que já são a superfície de decomp do port.

### 8. Chefes jogáveis
- Os chefes já estão na tabela de tipos (`assboss`, `jelly`, `final` em `MonsterNames.h`, índices 13 a 15); a seleção hoje vai de 1 a 9
  (até 10 com o bônus, `findAvailableMonster`).
- Alguns chefes não são `Monster` comum (`NATIVO.md`, "Vencer a luta"; câmera própria em `InitPlantBossCamera`): precisam de estados
  de controle, golpes e escala que caibam numa luta normal. Usa o caminho do item 7 para entrar na seleção.

### 9. Multiplayer até 4
- Já existe: tela dividida para 2 (vistas `viewDef[1]`/`[2]`, `ViewCtx`/`ViewScope`), controles por porta (`wotm_pad.inc`,
  `g_ps2PadDeviceMap`) e escolha de lados (`wotm_padsel.inc`); tudo em `NATIVO.md`. A engine carrega até 16 monstros.
- Falta: portas 3 e 4 no mapa de controles; 4 vistas (quadrantes) e o HUD de 4; a escolha de lados com 4 colunas; o texto do menu
  ("2 players" → "multiplayer").
- Atenção: em `LoadMonstersDB` os jogadores 3/4 montam o caminho com o monstro do jogador 2 (peculiaridade do retail,
  `16_monstros.md`); o gancho novo não pode herdar isso.

### 10. Prévia 3D na seleção
- Hoje a seleção mostra o monstro de cada jogador; com 4 são 4 modelos carregados no Shell ao mesmo tempo.

### 11. Campanha com até 4
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no
  Capitólio, 0x1A0) precisam ser revistas.

## Fora da ordem
- **Viewer web de monstros com várias skins:** ideia em `VIEWER_WEB.md`. Exige extrair assets: a extração roda na máquina do usuário a
  partir da ISO dele; nada extraído vai para o repo (regra 1 do `CLAUDE.md`).

## Ideias do dono ("viagens")
- **Modo Nintendo DS:** LOD sempre no nível mais alto (o LOD é escolhido no nativo, `NATIVO.md`); filtro de áudio em tempo real no motor
  de som nativo (`wotm_snd.hpp`); talvez duas telas, jogo em cima e HUD embaixo com o retrato (o HUD do `hier` já é desenhado no nativo,
  então dá para mandá-lo para outra área da janela).
- **Pedestres do GTA SA e o CJ jogável em tamanho real:** os pedestres de hoje são partículas (fx 91–99, `VU1CHECK.md`), então trocar
  por NPCs 3D é desenho e animação novos. O CJ depende do item 7. Assets de outro jogo não entram no repo: viriam dos arquivos do
  próprio usuário, carregados como mod.
