# Skins de mod que acrescentam (item 8): save, destraves e formato do mod

Investigação de 2026-10-23, sem código mudado. Escopo: o primeiro degrau do item 8 do `POS_FRONTEIRA.md` (trajes novos sem substituir);
monstros novos ficam para depois. Decisão do dono que serve de base: trajes 0–3 por posição; traje de mod referido por string; string
não achada cai num padrão.

## 1. Onde o traje entra hoje

### Carga do traje (o jogo)
- O número do traje só vira nome de arquivo: `Shell::formatFilename1` monta `\MON\<PREFIXO><traje>.<ext>` com `%s%d`
  (`src/game/Shell.cpp:1736-1742`). Usado em `LoadResTexture` (`.RTX`, `Shell.cpp:1265,1267,1284`), `LoadTexture` (`.TEX`,
  `:1340,1342,1357`) e `LoadMonstersDB` (`.NGP`, `:1429-1433,1454-1458`), que também registra o nome `"%s%d"` (`lizard4`) por
  `fileAddName`. Nenhum limite em 0..3 nesse caminho.
- O `.PTR` é lido pelo nome registrado: `sprintf(name, "mon/%s.ptr", getGenericName(idx))` (`src/common/dbs.cpp:455`). O nome tem no
  máximo 8 caracteres (`fileAddName`, `src/common/file.cpp:385-393`): `LIZARD` + 2 dígitos cabe; `ASSBOSS10` seria truncado para
  `ASSBOSS1` (colide). Para os 10 monstros jogáveis (prefixos de até 6 letras) os índices 4..99 cabem.
- O traje chega ao `Monster` como `m_skinNum` (`TheGame::SetPlayerMonster`, `src/game/TheGame.cpp:316-338`). Só o som o lê no C++:
  Preytor traje 1 (formigas) e Agamo traje 3 (Mecha) trocam sons (`src/game/Sound.cpp:2104,2113,2168`). Com traje 4+ o monstro usa
  os sons padrão (nada quebra). Leitura direta de `+0x30` em código ainda em asm não foi caçada.
- O mod de item 4 já serve arquivo que não existe no disco ("arquivo novo", `native/runtime/wotm_mods.inc:39-46`). Ou seja: hoje um
  mod com `MON/LIZARD4.NGP/.PTR/.TEX/.RTX` e `m_costume[0] = 4` (save editado ou `WOTM_MATCH`) já carregaria. O que falta é tudo
  em volta: seleção, destrave, save e a camada nativa.
- Arquivo ausente: `fileReadf` imprime "não achei" e devolve 0 (`src/common/file.cpp:128-142`); o `dbsRelocateViaPtrListFile` avisa
  "Could not read pointer file" e segue relocando com o buffer da pilha não lido (`dbs.cpp:455-490`). Resultado provável:
  memória corrompida e travamento, ou monstro inválido.

### Seleção e destrave (o jogo, tudo em 4)
- `monsterSelectMode`: 12 grupos de 4 `SelectSlot` (0x2C: `taken`, `state`, `cost`, `costText[7]`, `name[25]`;
  `include/game/monster_select.h`). Não há onde pôr uma 5ª entrada sem deslocar as fases (grupos 10 e 11).
- Seleção de 1 jogador: cursor de traje dá a volta em 0..3 (`CS_WRAP`, `src/game/screen.cpp:729`), desenhado pelos 4 switches do
  `SHELL.NGP` (`Shell::m_costumeSw[4]`, `include/game/shell.h:25`); confirma em `m_costume[0]` (`screen.cpp:921`). A seleção
  nativa de vários jogadores também é fixa em 4 (`native/runtime/wotm_padsel.inc:45,465-473,575-578`).
- Tela de destrave (`screenUnlocker`, `screen.cpp:4313`): 4 itens por grupo (`:4327`), cursor 0..3, compra põe
  `g_monsterToUnlock = grupo`, `g_skinToUnlock = item` e chama `saveUnlocks` (`:4424-4440`). A compra é efetivada no
  `McSaveSettings::update`, que no PC virou `applyPendingUnlock` (`native/runtime/wotm_save.inc:615-627`), calculando a entrada como
  `m * 4 + s`: **um `g_skinToUnlock >= 4` destravaria a entrada de outro monstro**. A extensão nativa da tela não pode passar por
  essas duas globais; tem que gravar a compra de mod direto no JSON.
- Mecha Sweet Tooth: entrada 23 (`kSweetToothSlot`, `wotm_save.inc:46`); a tela recusa comprá-la ("CANNOT UNLOCK THIS ITEM HERE",
  `screen.cpp:4427`); no PC vale a chave `unlocks.mechaSweetTooth` (`wotm_save.inc:144-147`, `:227-229`, migração em `:409-414`).
  É o precedente de "traje destravado fora da tela", útil para skins de mod "não compráveis".

### IAs
- História: `m_costume[2 + i] = levelMonsterModels[fase][i]` (`Shell.cpp:619,753`; `screen.cpp:798`). É desenho de fase (as Mutant
  Ants são Preytor traje 1).
- `RandomlySelectAI` (`Shell.cpp:346-362`) sorteia só o monstro (`taken` impede repetir) e não escreve `m_costume[2 + i]`: o traje da
  IA sorteada é o que estiver ali. O desafio escolhe o traje da IA na tela 17.

### Save (`native/runtime/wotm_save.inc`)
- Destraves: `settingsToJson` grava `unlocks.monsters.<nome>.skins` com 4 booleanos por grupo (`:132-140`), pelo `at()`, que
  preserva as chaves irmãs (`modSkins` sobreviveria). `jsonToSettings` lê só `k < 4` (`:217-222`). `ramToJson` (`:568-595`) refaz
  sobre o JSON carregado: chaves desconhecidas ficam.
- Aventura: o slot guarda `"costume": <int>` e `"variant"` (`m_monsterSel & 31`, outra coisa). Caminhos que leem ou gravam o traje:
  - `adventureToJson` (binário → JSON, `:162-191`, traje em `:187`): **zera cada slot (`dst = Value()`, `:174`) e o refaz**. Hoje
    já perde chaves desconhecidas dentro de um slot sempre que o jogo grava o `savegame.dat` (só com `WOTM_SAVEAUTO=0` ou na
    importação de cartão).
  - `jsonToAdventure` (JSON → binário, `:243-268`, `wrField(o + 16, "costume")` em `:265`): valor não inteiro é ignorado e o
    campo fica 0 (o bloco começa zerado).
  - Lista de saves: `buildRows` (`:739-767`, `savesInt` → 0 se não for inteiro), `loadRow` → `MonsterMc::setSaveData`
    (`:964-975`), `saveRow` (`:978-1008`) com `rowFromRam` (`Shell+0x2BE0`, `:770-783`).
  - Monstro 3D da lista: `mon | (costume & 3) << 8` (`:918`) e `(id >> 8) & 3` em `native/wotm_savepics.hpp:182,343`: traje 4+
    sai como `traje & 3` (outro traje, errado mas sem travar).
- `MonsterMc::setSaveData` (asm, `0x1679A8`) copia o `+0x10` do slot para `m_costume[0]` sem conferir nada.

### Traje >= 4 no binário com o mod removido
Carregar a aventura põe o número em `m_costume[0]`; a carga da fase pede `MON/LIZARD7.*`, que não existe: `fileReadf` devolve 0, o
`.PTR` não é lido e a relocação usa lixo. Travamento ou monstro quebrado. O índice cru não pode ir para o binário sem antes ser
resolvido; o fallback tem que rodar **antes** do `setSaveData` (e antes de qualquer `m_costume` chegar ao `LoadLevelFiles`).

## 2. Formato do mod de skin

### Manifesto
`mods/<mod>/skins/<id>/skin.json` (uma pasta por skin; o `wotm_json.hpp` dá conta: objetos, strings, inteiros e booleanos; sem
fração, que não é preciso):

```json
{
  "monster": "togera",
  "id": "green",
  "name": "GREEN TOGERA",
  "base": 1,
  "unlock": 3000,
  "ai": false
}
```

- `monster`: nome do `MonsterNewNames` (`include/game/MonsterNames.h`; `kMonsters` em `wotm_save.inc:50`), aceitando também o
  prefixo (`LIZARD`). Só os 10 jogáveis por enquanto.
- `id`: `[a-z0-9_]`, único **dentro do monstro**. A identidade no save é `"<monstro>/<id>"` (`"togera/green"`); dois mods podem ter
  "green" para monstros diferentes.
- `name`: o que a seleção e a tela de destrave mostram; só o que a fonte do menu tem (maiúsculas, dígitos, espaço; sem `%`, `:`, `/`),
  até 24 caracteres (`SelectSlot::name[25]`).
- `base`: traje do disco (0..3) que dá o que faltar na pasta, o `m_skinNum` (sons e qualquer comportamento por traje: base 3 do Agamo
  = sons do Mecha) e o fallback preferido (ver 3).
- `unlock`: `true` (sempre aberto), inteiro (preço em battle tokens na tela de destrave) ou `false` (só pela chave do save, como o
  Mecha). Exige o monstro destravado, como o retail ("UNLOCK %s FIRST").
- `ai`: se as IAs sorteadas podem usá-la (ver 3).

### Arquivos: base + o que a pasta trouxer
Na pasta da skin, por extensão, com qualquer nome (`togera_green.tex`) ou pelo nome do traje base (`LIZARD1.TEX`, o que o kit
exporta). O que faltar vem do traje `base` do disco:

| Pasta tem | Serve |
|---|---|
| nada além de PNGs | `.NGP/.PTR/.TEX/.RTX` do base; PNGs trocam as texturas |
| `.TEX` + `.RTX` | modelo do base, texturas e paletas novas |
| `.NGP` + `.PTR` (+ `.TEX/.RTX`) | modelo próprio |

`.NGP` sem `.PTR` (ou o contrário) é erro de manifesto: os dois andam juntos.

O próprio disco faz isso: `LIZARD1.NGP` = `LIZARD2.NGP` e `ALIEN0.NGP` = `ALIEN1.NGP` byte a byte, com `.PTR` iguais e só `.TEX`/`.RTX`
diferentes. `LIZARD0` e `LIZARD1` têm o mesmo tamanho e diferem em 8712 bytes, provavelmente estado de material. Logo "modelo do
base + `.TEX`/`.RTX` próprios" é seguro **desde que o `.TEX` novo tenha os mesmos texIds e tamanhos do base** (o `.NGP` aponta texIds;
o `modkit pack --inject` já regrava assim).

### Como o runtime liga tudo (carga)
1. `Registry::scan` (`native/wotm_mods.hpp`) passa a ler os `skins/*/skin.json` e monta, por monstro, a lista de skins de mod em
   ordem (pasta do mod em ordem alfabética, depois `id`). Índices **4, 5, 6...** atribuídos ali, só para esta sessão (o save guarda a
   string).
2. Para cada skin o registro cria **nomes virtuais** `MON/<PREFIXO><n>.{NGP,PTR,TEX,RTX}`: arquivo da pasta, se houver; senão um
   **alias para o arquivo do disco do traje base** (o gancho do `fileCdSearchFile` roda a original com o nome do base e devolve o
   LSN real; o `.PTR` passa pelo mesmo caminho, `mon/lizard4.ptr`). O mesmo alias vale no `wotm::disc::readFile`
   (`native/wotm_disc.hpp:113`), para o monstro 3D da lista de saves.
3. PNGs da pasta da skin entram no índice do `wotm_texmods.hpp` com o stem trocado para o nome virtual (`LIZARD1_t0004.png` ou
   `t0004.png` na pasta → chave `LIZARD4/t0004`). O nome registrado pelo jogo é `lizard4`, então o PNG só pega o monstro desse
   traje, mesmo com o `LIZARD1` de verdade em campo. **Cuidado:** hoje o `Index::build` casa PNG pelo nome do arquivo em qualquer pasta
   do mod; os PNGs de `skins/` têm que ficar fora dessa varredura global, senão `LIZARD1_t0004.png` dentro da skin pintaria também o
   traje 1 de verdade.
4. Conflito: mesmo `monster/id` em dois mods → vence o último em ordem alfabética, como os arquivos, com log
   (`[mods] skin togera/green: 20_b vence 10_a`). Como o índice é atribuído pelo runtime, não há conflito de número entre mods.
5. Teto: até o índice 99 (nome de 8 caracteres) e um teto prático por monstro (sugiro 16) para a interface.

### Vale a forma barata (base + PNGs)?
Sim, e deve ser a primeira entregue: não muda tamanho de nada na RAM (o aviso de "maior que o do disco" não se aplica), funciona
em qualquer resolução e não exige `.NGP` válido. Limites herdados do item 4: não vale nas partículas, na imagem do GS emulado
(checagem VU1) nem no monstro 3D da lista de saves (o `wotm_model.hpp` lê o `.TEX`; ver "Em aberto"). A forma com `.TEX`/`.RTX` próprio
cobre esses casos e é o mesmo código (o alias só deixa de valer para a extensão que a pasta trouxer).

## 3. Save com trajes de mod

### Destraves
```json
"togera": {"skins": [true, true, false, false], "modSkins": {"green": true}}
```
- `modSkins` por `id` (o monstro já é a chave de fora). Só entradas compradas ou destravadas à mão; skin com `unlock: true` não
  precisa de entrada. Chave de skin de mod removido fica no arquivo (o JSON já preserva) e volta a valer se o mod voltar.
- Leitura e escrita fora do bloco de 0x380: um mapa nativo `(monstro, id) → aberto` carregado no `jsonToRam` e gravado no `ramToJson`
  (as duas funções já reescrevem sobre o JSON carregado). A vigia `poll` (`:1210-1221`) não vê esse mapa; a compra de mod grava
  na hora.
- Compra pela tela de destrave estendida (nativa): desconta tokens em `Shell+0x2B24` e grava `modSkins`, **sem** `g_skinToUnlock`
  (ver 1).

### Aventura
```json
{"level": "tokyo", "monster": "togera", "variant": 0, "lives": 3, "score": 1200, "costume": "togera/green"}
```
- `costume` continua inteiro para 0..3 e vira string para mod. Build velho lê string como 0 (`wrField`/`savesInt` ignoram não
  inteiro): degrada para o traje 0 sem travar.
- Resolução string → índice num ponto só (`resolveCostume(monstro, Value)`), chamado por `jsonToAdventure` (binário refeito no
  `beforeMc`), `buildRows`/`loadRow` (lista de saves) e o monstro 3D da lista. Nenhum caminho põe no binário ou no `m_costume`
  um índice que não exista nesta sessão.
- Gravação: `rowFromRam` lê `m_costume[0]`; se for >= 4, `saveRow`/`adventureToJson` gravam a string da skin desse índice.
- **Preservar a string com o mod ausente:** na aventura o traje não muda depois da seleção. Ao carregar um slot com string não
  resolvida, guardar `(monstro, string, índice usado)`. Ao gravar a mesma aventura, se `m_costume[0]` e o monstro ainda forem esses,
  gravar a string de volta, não o fallback. Assim, mod removido e depois devolvido traz o traje de volta. No `adventureToJson`
  (`WOTM_SAVEAUTO=0`), trocar o "zera o slot" por "só reescreve se os números mudaram"; isso também conserta a perda de chaves
  desconhecidas por slot, que já existe hoje.

### Fallback: aleatório ou traje 0?
Recomendo o **traje 0**, determinístico. Se a string trouxer o base (ver abaixo), usar o base quando ele estiver destravado, senão o 0.
- O traje 0 está aberto sempre que o monstro está (freebie em 1..8; em Raptros e Zorgulon é o próprio destrave do monstro). Um
  aleatório teria de sortear só entre os destravados, e mesmo assim daria trajes diferentes a cada carga, na lista de saves (o
  monstro 3D mudaria de cor cada vez que a lista abre) e entre a lista e a luta.
- O build velho e qualquer leitor que ignore a string já dão 0: o mesmo save mostra o mesmo traje em todo lugar.
- Aleatório não ajuda a perceber o problema; um aviso ajuda. Log `[wotm] save: traje togera/green não achado (mod ausente?); usando
  traje 0` e uma linha na lista de saves ("skin missing") avisam.
- Opcional e barato: gravar o base junto (`"costume": "togera/green", "costumeBase": 1`) para o fallback ficar mais perto do
  visual. Se o dono quiser o aleatório, cabe como opção (`options.missingSkin: "random"`) sobre a mesma função.

Onde roda: na resolução (acima), ou seja, no `beforeMc` (binário dos cartões), no `buildRows`/`loadRow` (lista, antes do
`setSaveData`) e na seleção, quando o cursor lembrado aponta para um índice que sumiu. Não no `stdInit` do jogo: o binário
já sai resolvido.

### IAs
- História (`levelMonsterModels`): nunca. É desenho de fase.
- `RandomlySelectAI`/modo 4: hoje nem traje sorteia (fica o que estiver em `m_costume[2 + i]`). Proposta: um gancho depois dela que,
  para cada IA, sorteia entre os trajes **destravados** do monstro, incluindo skins de mod com `"ai": true` (padrão `false`, porque
  uma skin de mod pode estar crua ou pesar na RAM). Evitar o mesmo traje do jogador no mesmo monstro (a regra do retail de "outra
  skin" para espelho, `formatFilename`, `Shell.cpp:1706-1723`).
- Desafio (tela 17): o jogador escolhe o traje da IA; com a seleção estendida, as skins de mod entram naturalmente.

## Dependências e em aberto
- **Interface:** a seleção (1P e `wotm_padsel.inc`) e a tela de destrave precisam de entradas além das 4 (os switches do
  `SHELL.NGP` só têm 4). Sugestão: no 1P, depois do traje 3 o cursor entra numa lista nativa (nome em texto, os 4 switches apagados);
  no `padsel`, que já é nativo, é estender o `% 4`. Na tela de destrave, uma página extra por monstro com as skins de mod
  compráveis.
- `m_skinNum = base` para as skins de mod: gancho na saída do `SetPlayerMonster` (ou no `InitPlayers`). Falta confirmar se algum
  código ainda em asm lê `Monster+0x30` direto.
- Monstro 3D da lista de saves: alargar o `& 3` (`wotm_save.inc:918`, `wotm_savepics.hpp:182,343`) e fazer o `wotm_model.hpp` usar o
  alias e os PNGs (hoje lê só o `.TEX`).
- `.NGP` próprio maior que o do base: os monstros carregam em sequência (`getNextNgpLoadAddr`), então o limite é o total da fase
  (item 1, RAM), não o tamanho do arquivo base. Conferir com uma luta de 4 + IAs.
- `tools/modkit.py`: um `skin new <monstro> <id> --base N` que exporta o base para `mods/_work/` e o `pack` grava em
  `mods/<mod>/skins/<id>/` com o `skin.json`.
