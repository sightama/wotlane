# Depois da fronteira de hardware: prioridades (definidas pelo dono em 2026-10-19)

Vale quando a fronteira de hardware cair (`FRONTEIRA.md`, trabalho em outra sessão). Até lá a ordem do `PROXIMOS_PASSOS.md`
(inventário do port) continua. A lista é do dono; as notas de cada item são o que já se sabe no repo e o que falta descobrir.

## Ordem principal
1. **RAM do convidado acima de 32 MB** (a METRO trava na carga com 8 monstros).
2. **Save em JSON legível** (no lugar do arquivo do memory card).
3. **Sem tela de "memory card 1 ou 2"**; slots extras no mesmo JSON.
4. **Ícone de salvamento automático** (o ícone 3D do memory card, canto inferior direito, 1 s, a cada gravação).
5. **"2 players" vira "multiplayer"**, até 4 jogadores ao mesmo tempo.
6. **Campanha (adventure) com até 4 jogadores.**
7. **Seleção de monstros com prévia 3D de todos os selecionados.**

Dependências: 1 vem antes de 5, 6 e 7 (mais monstros e prévias carregados ao mesmo tempo); 2 → 3 → 4 usam o mesmo gancho de
gravação; 5 vem antes de 6.

### 1. RAM acima de 32 MB
- Medido em `investigacoes/2026-10-18/16_monstros.md`: com 8 monstros a carga da METRO vai até 0x23B4300 (35,7 MB) e congela
  (`g_frame` 205). As outras fases cabem; San Francisco e Vegas ficam com ~2 MB de folga.
- É o "degrau 2" daquele plano: `PS2_RAM_SIZE` do runtime é paramétrico (`ps2_memory.h`, potência de 2): subir para 64 MB e mover
  o teto da área de carga (hoje ~0x1F10000) por gancho em `fileOnlyNgpFile`/`getNextNgpLoadAddr`. Ponteiros de 32 bits e banco
  plano: a relocação do `dbs` não muda. A camada nativa já usa `wotm::Ram{rdram, PS2_RAM_SIZE}`.
- Mexe numa costura do PS2Recomp: regenerar `tools/runtime_patch/wotm-runtime.patch`.
- Conferir junto: `fileStatus` tem 14 slots (fase + 12 monstros; 8 cabem), `texInfo` 1000, VRAM residente do `.RTX`.

### 2. Save em JSON
- Hoje o stub `sceMc*` do runtime grava e lê arquivos em `mc0/` do host (`FRONTEIRA.md`, "Pad, CD, memory card"). O jogo
  (`game/McFile`, 59 funções, só asm, fim da fila de decomp) monta um bloco binário e manda gravar.
- Caminho mais barato: converter na camada do host (bloco ↔ JSON) sem decompilar o `McFile`. O jogo continua vendo o mesmo
  binário; o disco guarda o JSON. Precisa conhecer o layout do bloco.
- Primeiro passo: mapear o layout por diferença (gravar, destravar uma coisa, gravar de novo, comparar os arquivos de `mc0/`).
  Já conhecidos que devem estar no save: monstros destravados (`monster_select.h`, `state`), fases da tabela `monsterSelectMode`
  (`levelUnlocked`, `ESPINHA.md`), battle tokens (`Shell+0x2B24`), trajes, opções. Conferir se há checksum.
- Formato pedido: destraváveis como booleanos com nome; `saves` como array de objetos; `options` como objeto.

### 3. Sem escolha de memory card
- Ganchos nas telas do Shell que perguntam o cartão (achar por `screenTransition`, como no `wotm_padsel.inc`) para pular direto.
- Hoje, ao carregar do cartão 2, o jogo anima a segunda caixa das fitas de vídeo 3D. No lugar do cartão 2, o JSON aceita mais
  slots no array. Falta decidir como o jogador escolhe entre N slots sem a tela do cartão (decidir quando chegar aqui).

### 4. Ícone de salvamento automático
- O ícone 3D do save do PS2 (formato de ícone do memory card) vem junto do que o jogo grava; extrair e desenhar no nativo no canto
  inferior direito por 1 s, disparado pelo mesmo gancho de gravação do item 2. Detalhes quando for fazer.

### 5. Multiplayer até 4
- Já existe: tela dividida para 2 (vistas `viewDef[1]`/`[2]`, `ViewCtx`/`ViewScope`), controles por porta (`wotm_pad.inc`,
  `g_ps2PadDeviceMap`) e escolha de lados (`wotm_padsel.inc`); tudo em `NATIVO.md`. A engine carrega até 16 monstros.
- Falta: portas 3 e 4 no mapa de controles; 4 vistas (quadrantes) e o HUD de 4; a escolha de lados com 4 colunas; o texto do menu.
- Atenção: em `LoadMonstersDB` os jogadores 3/4 montam o caminho com o monstro do jogador 2 (peculiaridade do retail, em
  `16_monstros.md`); o gancho novo não pode herdar isso.

### 6. Campanha com até 4
- A campanha é de 1 jogador: o modo de jogo (`m_gameMode`), as cenas de cinema e as fases com chefes (slots extras 0x1C0/0x1E0 no
  Capitólio, 0x1A0) precisam ser revistas. Depende do 5.

### 7. Prévia 3D na seleção
- Hoje a seleção mostra o monstro de cada jogador; com 4 são 4 modelos carregados no Shell ao mesmo tempo. Depende do 1 (RAM) e do 5.

## Outros
- **Viewer web de monstros com várias skins:** ideia em `VIEWER_WEB.md`. Exige extrair assets: a extração roda na máquina do
  usuário a partir da ISO dele; nada extraído vai para o repo (regra 1 do `CLAUDE.md`).
- **Mods que adicionam (não substituem) skins e monstros:** trajes em `m_costume` (o `[2 + j]` atropela `m_mode` a partir da IA 10),
  tabela de seleção em `monster_select.h`, `texInfo` com 1000 entradas. Monstro novo passa por `MonsterParse` e `Shell::Load*`, que já
  são a superfície de decomp do port.

## Ideias do dono ("viagens")
- **Modo Nintendo DS:** LOD sempre no nível mais alto (o LOD é escolhido no nativo, `NATIVO.md`); filtro de áudio em tempo real no motor
  de som nativo (`wotm_snd.hpp`); talvez duas telas, jogo em cima e HUD embaixo com o retrato (o HUD do `hier` já é desenhado no nativo,
  então dá para mandá-lo para outra área da janela).
- **Pedestres do GTA SA e o CJ jogável em tamanho real:** os pedestres de hoje são partículas (fx 91–99, `VU1CHECK.md`), então trocar
  por NPCs 3D é desenho e animação novos. O CJ depende de "adicionar monstros". Assets de outro jogo não entram no repo: viriam dos
  arquivos do próprio usuário, carregados como mod.
