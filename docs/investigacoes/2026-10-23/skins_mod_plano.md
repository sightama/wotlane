# Item 8, parte 1: skins de mod que acrescentam (plano)

Base: `skins_mod_telas.md`, `skins_mod_carga.md` e `skins_mod_save.md` (mesma pasta). Monstro novo fica para a parte 2.

## Decisões do dono (2026-10-23)
- Trajes 0..3 continuam por posição; traje de mod é uma string (`"togera/green"`). Sem nome para os trajes padrão.
- **Skin de mod é sempre aberta**: não passa pela tela de destrave (21), não tem preço, o save não guarda destrave dela.
- O painel de 4 botões não serve: o seletor de N trajes é nosso (nativo).
- Fallback quando a string não é achada (mod removido): **traje 0** (recomendação dos relatórios; o dono sugeriu aleatório; a
  confirmar). O 0 está sempre aberto, a lista de saves e a luta mostram o mesmo, e é o que um build velho faz.

## O que os relatórios mostraram
- O traje só monta o nome do arquivo (`Shell::formatFilename1`, `MON\<PREFIXO><n>.*` com `%d`; `.PTR` por `mon/<nome>.ptr`). Depois
  vira `m_skinNum`, lido só pelo som, por igualdade (traje >= 4 cai no som padrão). Carga, relocação, `MonsterParse`, HUD: nada a mudar.
- Com `m_costume = 4` e `LIZARD4.*` em `mods/` (a pasta já serve arquivo novo) nada quebra antes da luta. O que barra é chegar a 4:
  menu do retail (4 switches, `CS_WRAP`, grupos de 4 do `monsterSelectMode`) e os `& 3`/`% 4` do nativo.
- Com a RAM alta a carga dos `MON/*` é contígua: tamanho só importa contra os tetos reais (VRAM residente do `.RTX`, `texInfo[1000]`,
  buffer de 0x30D40 do `.PTR`). Nome do arquivo até 8 caracteres.
- O próprio disco já faz "mesmo modelo, outra textura" (`LIZARD1/2`, `ALIEN0/1`: `.NGP`/`.PTR` idênticos).

## Formato do mod
`mods/<mod>/skins/<id>/skin.json`:
```json
{ "monster": "togera", "id": "green", "name": "GREEN TOGERA", "base": 1, "ai": false }
```
- `monster`: nome do jogo (ou o prefixo, `LIZARD`); `id` em `[a-z0-9_]`, único por monstro; `name`: o que a seleção mostra (só o que a
  fonte do menu tem); `base`: traje do disco que dá o que falta e o `m_skinNum`; `ai`: se as IAs sorteadas podem usar.
- Na pasta: só PNGs (modelo e texturas do base, PNG por cima), `.TEX`+`.RTX` (texturas próprias, mesmos texIds do base) ou
  `.NGP`+`.PTR` (modelo próprio, mesmo esqueleto e animações do traje 0).
- O runtime dá os índices 4, 5, ... na carga (por sessão) e cria os nomes `MON/<PREFIXO><n>.*`: arquivo da pasta ou alias para o
  do traje base. PNGs de `skins/` ficam fora da busca global (senão pintariam o traje base de verdade). Mesmo `monstro/id` em dois
  mods: vence o último em ordem alfabética, com log.

## Etapas
0. **Prova de carga** (nativo, sem gate): `WOTM_MATCH` aceita traje >= 4 (`wotm_overrides.cpp`), saem os `& 3` do caminho dos
   jogadores 3 e 4 (`wotm_split4.inc`). Teste com `mods/teste/MON/LIZARD4.*` = cópia do `LIZARD3.*` (malha diferente, visível).
1. **Registro de skins**: leitura dos `skin.json`, índices, nomes virtuais com alias ao base, PNGs por skin, `Model::load` (prévias 3D)
   passando pelo registro, `WOTM_MATCH` aceitando `togera/green`. Log `[mods] skin togera/green -> LIZARD4 (base 1)`.
2. **Save**: o traje do slot da aventura grava a string para traje de mod e lê com fallback; nunca vai ao binário um índice que não
   exista na sessão (com o mod sumido o binário traria `.PTR` inexistente e relocação com lixo). Corrige junto o `adventureToJson`,
   que hoje perde chaves desconhecidas dentro do slot. Lista de saves sem `& 3`.
3. **Seletor nativo de N trajes**: lista com o monstro 3D no lugar do painel, ligada só quando há skin de mod instalada (sem mods, as
   telas ficam como hoje). Cursor "procurador": o cursor do jogo fica num traje 0..3 aberto, o nativo guarda o índice real e grava
   `m_costume[p]` na confirmação. Ordem: seleção nativa de 3 e 4 (`wotm_padsel.inc`) e tela 3 (1 jogador), depois 6 e 7, depois
   18 e 19. Mouse junto.
4. **Acabamento**: IAs sorteadas (endurance) com skins `"ai": true`; aviso de carga contra os tetos reais (VRAM residente,
   `texInfo`); `MODS.md` com o guia de skin.

Fora: tela de destrave (decisão), IAs da aventura (`levelMonsterModels` fica com os trajes do retail), som por traje (vem do `base`).
