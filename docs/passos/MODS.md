# Mods: como fazer (POS_FRONTEIRA.md, item 4)

Um mod é uma pasta dentro de `mods/` (ao lado do ELF, na raiz do disco extraído que o jogo usa: `<runtime>\discroot\mods\`). Ela
repete a árvore do disco e o que estiver lá vale no lugar do arquivo do disco. Nada do jogo é alterado nem vai para o repo.

```
discroot/mods/
  my_mod/
    MON/lizard0_t0004.png      textura do Togera em PNG (qualquer resolução)
    MON/lizard0_rtx_t0003.png  retrato do Togera no HUD
    VAG/CITYAMB.VPK            música trocada
    SND/LIZARD_S.BNK           banco de sons trocado
  _work/                       pasta de trabalho do kit (começa com '_': o jogo não lê)
```

Regras do carregador (`native/wotm_mods.hpp`): pasta que começa com `_` ou `.` fica desligada; dois mods com o mesmo arquivo, vence o
último em ordem alfabética (dá para usar `10_base`, `20_extra`); `WOTM_MODS=<pasta>` troca a pasta e `WOTM_MODS=0` desliga tudo. O
log (`logs\live.err.log`) diz cada arquivo e cada textura que veio de um mod (`[mods] ...`).

## Interface (`tools/modkit_gui.py`, botão "Mod kit" do launcher)

`pythonw tools/modkit_gui.py`, ou o botão **Mod kit** do `WotM_Launcher.exe`. Precisa do Python 3 com Pillow (`pip install pillow`). A janela é em inglês.

1. **Pick from the disc** (esquerda): monstros por nome e traje (Togera › Costume 1, e os bancos de som dele), chefes, fases pelo nome do
   jogo, menu e telas, bancos de som, músicas e falas. Campo de busca; Ctrl/Shift escolhem vários; duplo clique ou "Extract selected".
2. **Edit in the work folder** (direita): um grupo por arquivo do disco ("MON/MONKEY0 (Congar, costume 1)"); depois de extrair, o
   grupo novo abre e os outros fecham. Miniaturas, tamanho e estado ("edited" = mudou desde a extração). Textura que o export não
   conseguiu colorir sai em cinza com `_sempaleta` no nome; o `pack` grava sem o sufixo (o jogo aceita os dois). Prévia
   da imagem (com xadrez atrás do alfa) ou a forma de onda do som. Som toca na própria janela ao clicar ("play sounds on click"
   desliga) ou no duplo clique; "Stop" para. Duplo clique numa textura abre no editor escolhido em **Editors...** (um programa por
   tipo, `.png` e `.wav`; vazio = programa padrão do Windows; com editor de `.wav` definido, o duplo clique no som abre nele).
   Botão direito na lista: Play, Open in ..., **Open with...** (escolhe o programa e pergunta se vira o padrão), **Replace with...** (só com um arquivo
   escolhido: troca pelo arquivo que você escolher e grava com o nome certo; JPG, BMP, TGA etc. viram PNG; avisa se a proporção
   muda; som só aceita WAV), Show in folder, **Revert to original** (só aparece se há editados na seleção; num grupo, volta os
   editados dele; extrai de novo do disco e confirma antes),
   **Save to folder...** (copia os selecionados, ou o grupo inteiro, para uma pasta) e Remove (tecla Del também). Clicar numa
   seção só a seleciona: abrir e tocar valem só para arquivos escolhidos um a um; na seção, o botão direito salva ou remove tudo.
3. **Mod** (embaixo): nome do mod (os existentes na lista), "Pack edited files into mod" e "Check mods". O log fica embaixo, com
   divisória arrastável para cima e para baixo.

As pastas (disco e mods), o último mod, os editores, a altura do log e a última pasta do "Save to folder" ficam em
`~/.wotm_modkit.json`. Por baixo são as mesmas funções do `modkit.py` abaixo.

## O kit na linha de comando (`tools/modkit.py`)

```
python tools/modkit.py export MON/LIZARD0                 # .TEX e .RTX do Togera em PNG
python tools/modkit.py export SND/LIZARD_S.BNK VAG/CITYAMB.VPK
  (edite os arquivos em mods/_work/)
python tools/modkit.py pack my_mod                        # leva o que mudou para mods/my_mod/
python tools/modkit.py check                              # confere nomes, tamanhos e texIds de todos os mods
python tools/modkit.py revert MON/lizard0_t0004.png       # volta um arquivo da pasta de trabalho ao original do disco
```

- Lê o disco extraído do repo (`disc/`, ou `--disc`/`WOTM_DISC`) e grava nos mods do runtime (`.wotm_recomp_dir` + `discroot\mods`,
  ou `--mods`). Curingas valem (`MON/LIZARD*`); sem extensão pega `.TEX` e `.RTX`.
- O `pack` só leva o que mudou desde o export (sha1 em `_work/modkit.json`). Exportar de novo não sobrescreve o que já foi editado.

| No disco | Na pasta de trabalho | O `pack` grava |
|---|---|---|
| `.TEX` | `<arquivo>_tNNNN.png` (um por textura) | o PNG como está |
| `.RTX` | `<arquivo>_rtx_tNNNN.png` (retratos, telas, fontes) | o PNG como está |
| `.VPK` / `.VAG` | `<arquivo>.wav` | `.VPK`/`.VAG` (mesma taxa e volumes do original) |
| `.BNK` | `<banco>_sNNN.wav` (um por som com amostra) | o banco com todos os sons editados trocados (amostra que outro som também usa: o editado ganha uma cópia própria e o outro fica com a original) |
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

## Skins (trajes a mais, sem substituir)

Uma skin é uma pasta em `mods/<mod>/skins/<pasta>/` com um `skin.json`:

```json
{ "monster": "ultra-v", "id": "roxo", "name": "PURPLE ULTRA V", "base": 3, "ai": true }
```

- `monster`: nome do monstro jogável (`congar`, `robo-47`, `togera`, `ultra-v`, `preytor`, `kineticlops`, `magmo`, `raptros`,
  `agamo`, `zorgulon`) ou o prefixo do arquivo (`SHOGUN`).
- `id`: letras minúsculas, dígitos e `_` (sem ele, o nome da pasta). O save guarda `"ultra-v/roxo"`.
- `name`: o que a seleção mostra. `base`: traje do disco (0 a 3) que dá o que a pasta não trouxer, e os sons do traje.
  `ai`: se as IAs sorteadas podem usar (padrão não).
- Skin de mod é sempre aberta: não passa pela tela de destrave.
- O que vai na pasta, do mais simples ao mais completo:
  - **só PNGs**: modelo e texturas do traje base; os PNGs trocam as texturas. Nome como o export do traje base
    (`shogun3_t0010.png`, `shogun3_rtx_t0011.png`) ou só `t0010.png`/`rtx_t0011.png`. Valem **só para a skin**: o traje 3 de verdade
    continua com a textura dele, mesmo na mesma luta.
  - **`.TEX` e/ou `.RTX`** (qualquer nome, pela extensão): texturas próprias, com os mesmos texIds do base (o `modkit pack --inject`
    grava assim).
  - **`.NGP` + `.PTR`** (sempre os dois): modelo próprio, com o mesmo esqueleto e animações do traje 0.
- O jogo vê cada skin como mais um traje do monstro (`MON/SHOGUN4.*`, `SHOGUN5.*`...; números dados a cada boot, na ordem
  mod/id). Mesmo monstro e id em dois mods: vence o último em ordem alfabética. O log diz o que entrou:
  `[mods] skin ultra-v/roxo ("PURPLE ULTRA V", meu_mod) = traje 4 (SHOGUN4); base 3, 2 png`.
- No menu (seleção nativa das telas 3 e 6 e de 3 e 4 jogadores): cima/baixo passa pelos trajes do disco e depois pelas skins do
  monstro, sempre abertas, e mais de um jogador pode escolher a mesma. Pelo `WOTM_MATCH` também (`WOTM_MATCH=4:roxo,...`).
  O modelo 3D da seleção já mostra os PNGs da skin.
- Ainda não: o save da aventura com skin e as IAs sorteadas (plano em `docs/investigacoes/2026-10-23/skins_mod_plano.md`).

Conferido em 2026-10-23 (`WOTM_MATCH=4:roxo,4:3,1`, IA): a skin com o PNG recolorido sai verde e o traje 3 ao lado continua roxo;
skin só com `skin.json` (`togera/demonio`, base 3) carrega o modelo do traje 3. Foto:
`docs/screenshots/2026-10-23/2026-10-23_skin_mod_png_so_na_skin.png`.

## Cuidados

- Arquivo de mod maior que o do disco (`.TEX`, `.RTX`, `.NGP`, `.BNK`): o jogo carrega em regiões de tamanho fixo e pode sobrescrever
  o que vem depois (o log e o `check` avisam). Com PNG isso não acontece: a textura do jogo não muda de tamanho.
- Monstros novos (sem substituir) são o item 8, parte 2.

Conferido em 2026-10-22 (`WOTM_MATCH=3,1`, `3,1,2,5,1` e `1,3,1` com IA): a pele do Togera em PNG 4x (1024 × 1024 no lugar de 256)
e o retrato 2x no HUD do jogador e na barra do inimigo; kit com export, edição, pack (PNG, `.VAG`, um som do `SHELL_S.BNK`),
re-export sem perder edição e check. Fotos: `docs/screenshots/2026-10-22/2026-10-22_mods_png_*`.
