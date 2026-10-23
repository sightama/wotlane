# Goliath Prime na seleção: o que é indexado por tipo de monstro (2026-10-23)

Pedido do dono: "esquema de monstro custom". Um mod acrescenta um monstro à seleção pelo botão CUSTOM da barra
(`kCellCustom`, `native/runtime/wotm_padsel.inc`). O primeiro é o Goliath Prime: tipo 13 (`assboss`, `m_typeBits` 0x1A0), arquivos
`MON/ASSBOSS0.*` e `ASSBOSS1.*`. Só leitura de código, asm e disco. Nada rodado: o runtime estava em uso.

## Resumo
- **A carga e a luta já aceitam o 13.** `LoadMonstersDB`/`LoadResTexture` aceitam tipo < 16, `MonsterLongNames[13]` = "assboss",
  `MonsterParse`/`AddMonster` vão pelo id do nó. O `WOTM_MATCH` não limita o índice. Por isso `WOTM_MATCH=13,1,1` carrega.
- **O retrato do HUD não sai de tabela nenhuma.** `Hud::initForReplay` e `Hud::update` põem como filhos do switch 0x1F4D do HUD
  (`m_node1F4D`, +0xEC) os objetos `Monster+0x6860`/`+0x6864`. Quem os preenche é o `Monster::addAttachment`: são os nós
  `tipo|7` e `tipo|8` do `.NGP` do próprio monstro. Os dois arquivos do Goliath têm esses nós (0x1A7/0x1A8) e um retrato próprio
  na última entrada do `.RTX` (normal e fúria), com o TEX0 e o texId nas listas do `.PTR`. Conferido no disco. **O retail mostraria
  o Goliath.** O Congar na luta de teste é, portanto, um problema do port ou da carga. A causa não foi confirmada (veja "Pendente").
- O que trava de verdade é **a interface e o que é nativo**: as 10 colunas da seleção (`monsterNametomonsterId`, `monsterSelectMode`,
  `kPortraits`), as tabelas de nome e arquivo do port, que param no 11, e o banco de som (`Assemb_s.bnk`), que só a fase do
  Goliath carrega.

## Inventário (tabela, tamanho, o que acontece com o 13, o que o port faz)

### Jogo (`src/`, `include/`, `asm/`)
| Onde | Tamanho | Com 13 hoje | Port |
|---|---|---|---|
| `MonsterNewNames` (`include/game/MonsterNames.h`; 5 cópias: Hud, CrushLevel...) | [17][12] | "assboss". Só texto na tela: "%s CRUSHED" (`Hud::addMessage`), "%s WINS" (`CrushLevel`) | nome de exibição: trocar o texto da entrada 13 nas cópias na RAM ("goliath" cabe em 11 letras) ou ganchar os dois `sprintf` |
| `MonsterLongNames` (3 cópias, `Shell.cpp`) | [17][8] | "assboss": nome do arquivo (`formatFilename1` + traje) | nada |
| `monsterNametomonsterId` (0x2A1A40) | 32 palavras | colunas 1..10 da seleção. Depois 0xC0 (spider), 0x180, **0x1A0 (índice 12)**, 0x1C0, 0x1E0 e zeros | o jogo nunca lê além da 10. O custom não passa por ela |
| `monsterSelectMode` (`monster_select.h`) | 48 `SelectSlot` = 12 grupos de 4 | grupos 0..9 = colunas; 10 e 11 = destrave das fases (40..46). **Não tem grupo para o 13** | destrave e trajes do custom ficam fora dela (JSON, como as skins) |
| Menus do retail (telas 3, 6, 7, 13; `findAvailableMonster`, `MG_FIND_COSTUME`, `CS_WRAP`) | colunas 1..10, trajes 0..3 | não alcançam o 13 | a casa CUSTOM é só nativa (veja o desenho) |
| `Shell::RandomlySelectAI` `aiMonsters[10]` | 10 | IA sorteada nunca é o 13 | custom como CPU: depois |
| `levelMonsters[][12]` / `levelMonsterModels[][4]` | aventura | fase 3 (canyon2), slot 4 = 0x1A0. O traje lê `[3][4]`, além da linha (= `[4][0]` = 0): a aventura carrega o **ASSBOSS0**, salvo outro código | conferir no jogo |
| `monsterIdtomonsterSlot` (ShellFinished) | switch | 0x1A0 → 10. O diálogo da eliminação grava `taken` = 1 no slot 40 (flag da fase volcano) | eliminação com custom: depois |
| `LoadResTexture`/`LoadMonstersDB` modo 6 (eliminação) | 1..5 e 7..11 fixos | o 13 não é carregado | eliminação: depois |
| `on_bit` (`MonsterIsChosen`/`IsLocked`) | 32 bits | bit 13, ok | nada |
| `setReelSwitches` (rolos dos saves do menu do jogo) | switch do SHELL | `hierSetSwitch(…, 13)`: filho além do fim | só se o save da aventura vier com o Goliath |
| `SoundManager::loadMonsterBank`/`loadLoRes…` | casos 0x20..0x160 | **0x1A0 não carrega banco**. Os sons do Goliath usam o `BANK_MECH` (22, `mechBankName` = "Assemb_s.bnk"), que só o `canyon2` carrega (`loadMechBossSoundBank`) | ao entrar na luta, se há um 0x1A0 e a fase não é a 3, chamar `loadMechBossSoundBank` |
| `StreamingSoundManager::requestTaunt` | switch por tipo | 0x1A0 cai no padrão (sem voz de provocação em stream) | aceitável |
| Casos especiais por `== 0x1A0` (~40 funções: `addAttachment` resto 29, `takeHit`, `knockBack`, `Grapple::transitionFeasible`, `StateTaunt`, `TopSpin`, `Ai::init` → `initAssBoss`, `SetAIMonster`) | — | rodam com o jogador. `StateTaunt` lê `Ai::getAssBossState` (global, sem ponteiro: não quebra). Agarrar o Goliath e derrubá-lo segue as regras de chefe | equilíbrio: depois. Goliath como CPU fora do canyon2: IA de chefe (`initAssBoss`), a testar |
| `Monster::addAttachment` (anexos `tipo|1..31`) | 31 casos | ASSBOSS0 não tem o resto 10 (retícula pegajosa, +0x6BFC) nem o 11 (+0x6874, lido sem checagem por `PadFlags::checkCombos`). Tem dois nós com o id do Congar (0x23, 0x24: arquivo clonado). ASSBOSS1 tem o 10 | usar o **ASSBOSS1** como traje principal |

Os arquivos: ASSBOSS0 (mar/2002, `.TEX` de 91 KB, retrato 64x32) e ASSBOSS1 (set/2002, `.TEX` de 836 KB, retrato 128x64 como o dos
outros monstros). O 1 parece o final.

### Port (`native/`, `tools/`)
| Onde | Tamanho | Com 13 hoje | Precisa |
|---|---|---|---|
| `wotm_padsel.inc` `kMonsterNames[17]` | 17 | "ASSBOSS" | nome de exibição pelo registro |
| `wotm_padsel.inc` `kPortraits` (retratos 64x64 do SHELL) | 10 | sem entrada: `g_portrait[13]` vazio (cai no texto) | retrato do mod (PNG) ou, sem ele, a metade esquerda do retrato do HUD do `.RTX` do monstro (o `decodePortrait` já lê pacotes de `.RTX`) |
| `wotm_padsel.inc` `columnOpen`/`stepColumn`/`costumeList`/`monsterOfCol` | colunas 1..10 | custom: lista vazia | lista de trajes do custom (disco + skins do tipo) |
| `soloDecide` (telas 3 e 6), X no CUSTOM | — | "COMING SOON", `return 0` | veja o desenho |
| `selectFinish` (seleção de 3 e 4) | — | grava `monsterNametomonsterId[col-1]` | gravar o tipo do custom |
| `wotm_savepics.hpp` `monsterFile` (prefixo em MON/) | 0..11 | `nullptr`: **sem monstro 3D** na seleção nativa (`selModel`) nem na lista de saves | "ASSBOSS" (pelo registro) |
| `wotm_savepics.hpp` `kFraming`, `kLogoPics` | por tipo | padrão / sem logo | enquadramento do Goliath (é grande); logo opcional do mod |
| `wotm_save.inc` `kMonsters[17]` (nome no JSON) | 17 | "assboss": vai e volta | nada para a 1ª entrega |
| `wotm_save.inc` `kGroupMonsters[10]` (destraves) | 10 | — | destrave do custom no JSON (como `skins`) |
| `wotm_narrator.inc` `namesOf` | 16 | "goliathprime", "assboss" (os dois MP3 estão em `narrator/`) | campo `narrator` opcional no `monster.json` |
| `wotm_mods.hpp` `kMonsters` (monstros das skins) | 10 | skin com `"monster": "goliath"` é recusada | acrescentar `{13, "goliath", "ASSBOSS"}` (prefixo de 7 letras: só trajes 4..9) |
| `wotm_overrides.cpp` `monsterPortrait` (barras dos inimigos) | — | procura T8H **128x64**: com o ASSBOSS0 (64x32) a barra sai sem retrato | aceitar 64x32 (`tw`/`th` do próprio TEX0) |
| `wotm_overrides.cpp` `WOTM_MATCH` | sem teto | `13[:<traje>]` funciona | aceitar o id do custom ("goliath") |
| `wotm_modes4.inc` `monsterSlot` | 10 | 0x1A0 → 10 | eliminação: depois |
| `tools/launcher/WotmLauncher.cs` `MonsterIds`/`MonsterNames`/`Prefixes`, `modSkins[10]` | 10 | não oferece o 13 | ler `mods/*/monsters/*/monster.json` e pôr no fim das listas |

## Desenho proposto: `mods/<mod>/monsters/<id>/monster.json`
No mesmo estilo do `skin.json`: a pasta é o id, as pastas com `_`/`.` ficam de fora, e com o mesmo id em dois mods vence o último
em ordem alfabética.
```json
{ "id": "goliath", "name": "Goliath Prime", "type": "assboss",
  "costumes": [ {"costume": 1, "name": "Goliath Prime"}, {"costume": 0, "name": "Prototype"} ],
  "portrait": "portrait.png", "narrator": "goliathprime", "ai": false, "locked": false }
```
- `type`: o tipo do jogo cujo código o monstro usa (nome ou número). Hoje só tipos que têm arquivos e fora das colunas: 13 (e,
  talvez depois, 15 `final`, 14 `jelly`). Um tipo novo, com esqueleto próprio, fica para depois: o código de Monster é por tipo,
  e `MonsterLongNames`/`MonsterNewNames` têm 17 entradas.
- `costumes`: os trajes do disco que entram, na ordem da lista. Arquivos próprios por traje = skins (`mods/<mod>/skins/` com
  `"monster": "goliath"`, já que o `kMonsters` passa a conhecer o 13).
- `portrait`: PNG quadrado para a barra e o quadro da seleção. Sem ele, a cara normal do retrato do HUD. O retrato do HUD em si
  troca pelo caminho de texturas de sempre (`ASSBOSS1_rtx_t0006.png`).
- Registro: `Registry::monsters()` no `wotm_mods.hpp`, lido na mesma varredura das skins.

**Na seleção:** a casa CUSTOM vira uma lista. Parado nela, cima/baixo anda pelos monstros custom (o nome vai no lugar de "CUSTOM",
o retrato do escolhido na casa) e esquerda/direita sai da casa, como hoje. Com mais de um traje, os trajes vêm na mesma faixa dos
trajes (cima/baixo percorre monstro × traje, o que dá uma lista só e nenhuma tecla nova). No X, o cursor do jogo fica numa coluna
aberta (a de onde saiu) com o primeiro traje dela. O X passa para o jogo, e na confirmação o `soloSync` faz o que já faz para as
skins: grava `monsterDst` = `13 << 5` e `costumeDst` = o traje, e devolve `taken` = 1 ao slot que serviu de procurador. Na seleção
de 3/4 jogadores, o `selectFinish` grava direto. O quadro do telão mostra o modelo 3D (`monsterFile` pelo registro) e o nome.

**WOTM_MATCH:** `goliath[:<traje>]` (o id), além do `13:1` de hoje. **Launcher:** os custom no fim das listas de monstro, valor
`goliath`. **Save:** no `m_monsterSel` o jogo só guarda o tipo, e o JSON já escreve "assboss". Para o JSON reconhecer o custom,
gravar `"custom": "<mod id>"` ao lado quando o tipo for de um custom. Destrave em `"monsters": {"goliath": true}`, se um dia houver
`locked`.

## Primeira entrega (Goliath jogável pelo CUSTOM, luta de 1 ou 2 jogadores contra CPU normal)
1. Registro e `monster.json` (sem `portrait`: retrato do `.RTX`).
2. CUSTOM nas telas 3 e 6: lista, procurador e gravação na confirmação. `monsterFile`/`kMonsterNames` pelo registro (modelo 3D e
   nome). Traje padrão 1 (ASSBOSS1).
3. Som: `loadMechBossSoundBank` ao entrar numa luta com 0x1A0 fora do canyon2 (gancho no fim do `loadRTSoundBanks`).
4. Retrato do HUD: achar a causa do Congar (abaixo) e consertar. `monsterPortrait` aceitando 64x32.
5. Nome na tela: "GOLIATH" nas cópias de `MonsterNewNames` (ou gancho no texto).
6. `WOTM_MATCH=goliath` e a entrada no launcher.

**Depois:** seleção de 3/4 e tela 13 (CPU custom, `ai`), minigames (tela 7), eliminação (carga fixa dos 10 e `monsterSlot`), a
aventura com o custom (rolos do save, `levelMonsters`), o nome na tela de destrave, as skins do Goliath, o equilíbrio (regras de
chefe em agarrar e derrubar) e tipos novos com arquivos próprios.

## Pendente: o retrato do Congar no `WOTM_MATCH=13,1,1`
O caminho do retail está fechado (anexos 7 e 8 do próprio arquivo), e os nós do ASSBOSS0 estão nas listas de relocação. Falta
olhar uma luta: logar o TEX0 e o texId dos dois filhos de `hud->m_node1F4D` e a chave que o `decodeResident`/`texKey` do nativo usa
para eles. Suspeitas:
- (a) o cache de texturas do nativo (`texKey`: texId + CBP, sem TBP) junta o retrato do Goliath com uma textura do Congar;
- (b) o texId/maxTexId das `.RTX` (`texmResRelocate` só conta as entradas residentes, e o `.TEX` soma depois) cai igual ao
  retrato do Congar;
- (c) na luta de teste o traje foi o 0, cujo arquivo é um clone antigo do Congar (nós 0x23/0x24 com o id dele). Vale repetir com
  `13:1`. Se o ASSBOSS1 mostrar o retrato certo, (c) explica.
