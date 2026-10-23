# Mods: como fazer (POS_FRONTEIRA.md, item 4)

Um mod é uma pasta dentro de `mods/` (ao lado do ELF, na raiz do disco extraído que o jogo usa: `<runtime>\discroot\mods\`). Ela
repete a árvore do disco e o que estiver lá vale no lugar do arquivo do disco. Nada do jogo é alterado nem vai para o repo.

```
discroot/mods/
  meu_mod/
    MON/lizard0_t0004.png      textura do Togera em PNG (qualquer resolução)
    MON/lizard0_rtx_t0003.png  retrato do Togera no HUD
    VAG/CITYAMB.VPK            música trocada
    SND/LIZARD_S.BNK           banco de sons trocado
  _trabalho/                   pasta de trabalho do kit (começa com '_': o jogo não lê)
```

Regras do carregador (`native/wotm_mods.hpp`): pasta que começa com `_` ou `.` fica desligada; dois mods com o mesmo arquivo, vence o
último em ordem alfabética (dá para usar `10_base`, `20_extra`); `WOTM_MODS=<pasta>` troca a pasta e `WOTM_MODS=0` desliga tudo. O
log (`logs\live.err.log`) diz cada arquivo e cada textura que veio de um mod (`[mods] ...`).

## O kit (`tools/modkit.py`)

```
python tools/modkit.py export MON/LIZARD0                 # .TEX e .RTX do Togera em PNG
python tools/modkit.py export SND/LIZARD_S.BNK VAG/CITYAMB.VPK
  (edite os arquivos em mods/_trabalho/)
python tools/modkit.py pack meu_mod                       # leva o que mudou para mods/meu_mod/
python tools/modkit.py check                              # confere nomes, tamanhos e texIds de todos os mods
```

- Lê o disco extraído do repo (`disc/`, ou `--disc`/`WOTM_DISC`) e grava nos mods do runtime (`.wotm_recomp_dir` + `discroot\mods`,
  ou `--mods`). Curingas valem (`MON/LIZARD*`); sem extensão pega `.TEX` e `.RTX`.
- O `pack` só leva o que mudou desde o export (sha1 em `_trabalho/modkit.json`). Exportar de novo não sobrescreve o que já foi editado.

| No disco | Na pasta de trabalho | O `pack` grava |
|---|---|---|
| `.TEX` | `<arquivo>_tNNNN.png` (um por textura) | o PNG como está |
| `.RTX` | `<arquivo>_rtx_tNNNN.png` (retratos, telas, fontes) | o PNG como está |
| `.VPK` / `.VAG` | `<arquivo>.wav` | `.VPK`/`.VAG` (mesma taxa e volumes do original) |
| `.BNK` | `<banco>_sNNN.wav` (um por som com amostra) | o banco com todos os sons editados trocados |
| outros | cópia | cópia |

## Texturas em PNG

- O jogo continua carregando o `.TEX`/`.RTX` do disco; o desenho nativo troca a imagem na hora de desenhar
  (`native/wotm_texmods.hpp`). Por isso o PNG pode ter **qualquer resolução** (texturas HD): os UVs dos objetos são normalizados, e os
  quadrados de tela (retratos das barras, fonte) escalam o trecho pela razão entre o PNG e a textura original. Mantenha a
  proporção da original (o `check` avisa).
- Vale em qualquer pasta do mod (o nome do arquivo basta) e para todas as paletas que usam a textura.
- Alfa: 255 é opaco. Texturas sem alfa no jogo (TEX0.TCC = 0) ignoram o alfa do PNG.
- Não vale: nas partículas (fogo, fumaça, poeira), no monstro 3D da lista de saves e na imagem do GS emulado
  do `ps2EntryRunner` (a referência da checagem VU1; a janela dele usa o PNG). Para essa, `pack --inject` também regrava o `.TEX`/`.RTX` do mod quando o PNG tem o
  tamanho original (as cores vão para a paleta atual).
- Como o nativo acha o arquivo: o pacote de upload de `texInfo[texId]` está dentro do `.TEX` (ou `.RTX`) carregado em
  `fileStatus.texAddr[i]` (`resAddr[i]`); o deslocamento dentro dele dá o texId do disco, e o nome é `fileStatus.names[i]`. As
  residentes (T8H/T4HL/T4HH) vêm pelo DBP, como no `decodeResident`.

## Cuidados

- Arquivo de mod maior que o do disco (`.TEX`, `.RTX`, `.NGP`, `.BNK`): o jogo carrega em regiões de tamanho fixo e pode sobrescrever
  o que vem depois (o log e o `check` avisam). Com PNG isso não acontece: a textura do jogo não muda de tamanho.
- Monstros e skins novos (sem substituir) são o item 8.

Conferido em 2026-10-22 (`WOTM_MATCH=3,1`, `3,1,2,5,1` e `1,3,1` com IA): a pele do Togera em PNG 4x (1024 × 1024 no lugar de 256)
e o retrato 2x no HUD do jogador e na barra do inimigo; kit com export, edição, pack (PNG, `.VAG`, um som do `SHELL_S.BNK`),
re-export sem perder edição e check. Fotos: `docs/screenshots/2026-10-22/2026-10-22_mods_png_*`.
