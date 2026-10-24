# Mods: como fazer

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
- Alfa: o PNG usa a mesma escala da textura original, como o export a grava. A maioria das texturas do jogo vai de 0 a 0xFF (o dobro
  do opaco 0x80 do GS); nelas o alfa do PNG vale cru, 255 = 0xFF, e o log diz "alfa cru, 255 = 0xFF". Nas que param em 0x80,
  255 = 0x80 (o resto / 2). Na prática: 255 é o opaco que o jogo usa, e o export e o `pack --inject` vão e voltam sem perder nada.
  Até 2026-10-23 o export achatava tudo acima de 0x80 em 255 e o jogo lia 255 como 0x80: o logo do título (SHELL t140/t141, que o
  jogo desenha com o vértice em meia opacidade e conta com o 0xFF) saía com metade da opacidade. PNG antigo opaco continua igual;
  PNG antigo com semitransparência em textura de escala 0xFF sai mais opaco (exporte de novo). Texturas sem alfa no jogo
  (TEX0.TCC = 0) ignoram o alfa do PNG.
- Imagem dividida em várias texturas (o logo do título são duas de 256 × 256 lado a lado, de cabeça para baixo): escale a imagem
  inteira uma vez e corte no meio exato, sem sobreposição; cortes feitos à mão com sobra repetem um pedaço na emenda.
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
- No menu (seleção nativa das telas 3, 6, 7 (minigames de 2), 18 e 19 e de 3 e 4 jogadores): cima/baixo passa pelos trajes do disco e depois pelas skins do
  monstro, sempre abertas, e mais de um jogador pode escolher a mesma. Pelo `WOTM_MATCH` também (`WOTM_MATCH=4:roxo,...`) e pelo
  launcher (a caixa de skin de cada monstro lista as skins de mod dele). O modelo 3D da seleção e da lista de saves mostra os PNGs da skin.
- Na luta a skin usa os sons do traje `base` (o Preytor base 1 e o Agamo base 3 têm sons próprios). Log:
  `[mods] skin ultra-v/roxo (traje 4) na luta com os sons do traje 3`.
- **Save:** a aventura guarda a skin pelo nome (`"costume": "ultra-v/roxo"`; os trajes do disco seguem como 0 a 3). Sem o mod, o slot
  carrega com o traje 0 (log `[wotm] save: traje ... não achado (mod ausente?); usando traje 0`, e "skin missing" na lista de saves);
  gravar a mesma aventura mantém o nome, e a skin volta quando o mod voltar.
- **IAs sorteadas** (resistência): com `"ai": true`, a skin entra no sorteio do traje da IA daquele monstro, junto com o traje que o
  jogo escolheria (log `[mods] IA N sorteada com a skin ...`). As IAs da aventura ficam com os trajes do jogo.
- **Kit:** `python tools/modkit.py skin new <mod> <monstro> <id> --base N [--name "GREEN TOGERA"] [--ai]` cria o `skin.json` e exporta as
  texturas do traje base para `mods/_work/skins/<id>/`; edite os PNGs e rode `modkit.py pack <mod>`: só os editados vão para a pasta da skin.
- Ainda não: o elimination. Nele o jogo carrega o traje 0 de todos os monstros uma vez só e os
  jogadores trocam de monstro entre as rodadas sem recarregar: escolher traje ali pede mudar a carga, não só a tela.

Conferido em 2026-10-23 (`WOTM_MATCH=4:roxo,4:3,1`, IA): a skin com o PNG recolorido sai verde e o traje 3 ao lado continua roxo;
skin só com `skin.json` (`togera/demonio`, base 3) carrega o modelo do traje 3. Foto:
`docs/screenshots/2026-10-23/2026-10-23_skin_mod_png_so_na_skin.png`.

## Monstros custom (um monstro a mais na seleção)

Um monstro custom é uma pasta em `mods/<mod>/monsters/<pasta>/` com um `monster.json`. Hoje ele usa o código, o modelo e os arquivos
de um tipo do jogo (`base`); arquivos próprios (outro prefixo em `MON/`, um tipo livre do jogo) ficam para depois, e o formato reserva
`files` para isso. O primeiro é o Goliath Prime (`mods/goliath/monsters/goliath/`, fora do repo):

```json
{ "id": "goliath", "name": "GOLIATH PRIME", "base": "assboss", "costume": 1,
  "camera": { "from": "togera", "scale": 200 }, "narrator": "goliathprime",
  "portrait": "goliathprime.png" }
```

- `id`: letras minúsculas, dígitos e `_` (sem ele, o nome da pasta). É o que o `WOTM_MATCH` aceita (`WOTM_MATCH=goliath,3,1`).
- `name`: o que a seleção e o HUD mostram. O HUD tem 11 letras: vale a última palavra que couber, em minúsculas ("goliath").
- `base`: o tipo do jogo, pelo nome ou prefixo: os dez jogáveis (`togera`, `LIZARD`...) ou o `assboss` (13, o Goliath; o único chefe
  em `kCustomBases`, `native/wotm_mods.hpp`). Um custom de base jogável roda o código e a IA desse monstro; só o Goliath foi testado
  (para trocar só a aparência, use uma skin).
- `costume`: o traje do disco (`ASSBOSS1`; o `ASSBOSS0` é um protótipo com metade das animações e o retrato pequeno).
- `camera`: a câmera de outro monstro (`from`, pelo nome do jogo) com as distâncias escaladas (`scale` em %, 10 a 1000; o JSON do
  port só lê inteiros). O `ASSBOSS.CHR` nunca foi usado pelo jogo (na aventura a câmera é a da fase) e deixa a câmera em cima do
  monstro; a do Togera 2x fica como a de um monstro comum.
- `narrator`: `narrator/<nome>.mp3` dito na confirmação (vazio = o do tipo).
- `ai`: se uma CPU pode usá-lo (padrão não). Um custom de chefe como CPU rodaria a IA do chefe (`Ai::updateAssBoss`), que precisa
  dos pontos da fase dele; a seleção mostra "PLAYERS ONLY" no quadro da CPU. Com `"ai": true` (pensado para base jogável) a CPU
  pode escolhê-lo na casa CUSTOM da seleção; isso ainda não foi testado numa luta, e o launcher só oferece customs ao jogador.
- **Na seleção** (telas 3, 6, 7, 18 e 19): a casa CUSTOM da barra de monstros. Cima e baixo percorrem os customs, X confirma. O
  cursor do jogo fica na coluna de onde o quadro saiu, que faz as vezes dele (a vaga dela não é tomada). A seleção de 3 e 4
  jogadores ainda não tem a casa.
- **Launcher:** os customs aparecem em "Your monster", depois do Random (só para o jogador).
- **O que o port faz pelo Goliath** (`native/runtime/wotm_custom.inc`): ele fica visível e controlável com IAs na luta (o
  `SetAIMonster` o desligava), toma dano delas (o `takeHit` só aceitava golpe do jogador 1), carrega o banco de som do chefe fora
  da aventura e põe o nome no HUD. Também: X+Quadrado faz o giro do chefe (`StateTopSpin`, no lugar do `StateCrowdControl` dos
  comuns; o golpe cheio vale contra a IA mais perto), a torre não segue mais o inimigo o tempo todo (o "olhar" do chefe gira o
  tronco inteiro; com o lock ela mira) e o dardo (empalar) vira um golpe comum em quem não tem as animações de empalado (antes
  travava). Investigações (encerradas): `git show 18bb1d2:docs/investigacoes/2026-10-23/goliath_topspin.md`, `goliath_lockon.md`, `goliath_impale.md`.
- **Retrato do HUD:** o do `ASSBOSS1.RTX` (texId 6) é provisório, com a cara do Congar. `"portrait": "goliathprime.png"` (na pasta
  do monstro ou na raiz do mod; 128 × 64 ou maior na mesma proporção, ex. 512 × 256) troca a imagem do retrato do traje do custom
  (`"portraitTex"`: o texId no `.RTX`; padrão 6 para o `ASSBOSS1`). Vale também para o chefe da aventura, que usa o mesmo arquivo.
  Log: `[mods] ASSBOSS1.RTX t6 <- Goliath Prime/goliathprime.png`.
- Ainda não: animações que o ASSBOSS não tem (escalar, alguns bloqueios e contra-ataques; `18bb1d2:docs/investigacoes/2026-10-23/goliath_codigo.md`,
  seção 3) fazem o jogo segurar a pose anterior; elimination e minigames não foram testados com ele; o save da aventura não guarda
  custom (a aventura não oferece a casa).

Conferido em 2026-10-23 por roteiro: seleção da tela 6 (1 jogador contra CPU), CUSTOM → GOLIATH PRIME, a CPU recusada na casa,
luta em Midtown Park com câmera, som e nome. Foto: `docs/screenshots/2026-10-23/2026-10-23_selecao_casa_custom_goliath.png`.

## Por dentro (para quem mexe no código)

- **Arquivos** (`native/wotm_mods.hpp`, `native/runtime/wotm_mods.inc`): todo arquivo que o jogo lê passa pelo `fileCdSearchFile`
  (fases, monstros, `SHELL`, bancos, músicas, filmes) e depois pelo `sceCdRead` (`fileReadf/a/s`, `zipInflateAll`,
  `snd_StreamSafeCdRead`). O gancho da busca roda a original e, se o arquivo estiver em `mods/`, devolve um LSN falso (faixa a partir
  de 0x800000, além de qualquer DVD) e o tamanho do mod; o do `sceCdRead` atende essa faixa lendo do arquivo do PC. O som nativo
  (`readLsnRaw`) lê bancos e músicas pela mesma faixa. A camada nativa que lê o disco por nome também procura antes em `mods/`:
  paletas do `.RTX` (`Scene::loadResources`, que lê o arquivo direto e não da RAM), filmes (`wotm_fmv.hpp`), ícone, fotos e monstro
  da lista de saves (`wotm::disc::readFile`). Sem nenhum arquivo em `mods/` nenhum gancho é instalado. Arquivo que não existe no disco
  também é servido ("arquivo novo").
- **Log:** `[mods] N arquivo(s) de <mods> em <pasta>` no boot e, na primeira busca de cada arquivo, `[mods] MON/LIZARD0.TEX <-
  teste_togera (110080 bytes; disco 110080)`, com AVISO quando o do mod é maior (fora de `MON/*`, cuja carga é contígua acima de
  32 MB).
- **PNG** (`native/wotm_texmods.hpp`, `Scene::modTexture`): o jogo segue carregando o `.TEX`/`.RTX` do disco; o desenho nativo troca a
  imagem no `textureId` (objetos) e no `residentTexture` (quadrados de tela: barras dos inimigos, fonte). O texId do disco sai do
  deslocamento do pacote de `texInfo[id]` dentro do arquivo carregado (`fileStatus.texAddr`/`resAddr`, nome em `names`); as
  residentes, pelo DBP. PNG maior que a original ganha mipmaps. Fora: partículas, monstro da lista de saves e o GS emulado (para
  esse, `modkit.py pack --inject`).

## Cuidados

- Arquivo de mod maior que o do disco (`.TEX`, `.RTX`, `.NGP`, `.BNK`): o jogo carrega em regiões de tamanho fixo e pode sobrescrever
  o que vem depois (o log e o `check` avisam). Com PNG isso não acontece: a textura do jogo não muda de tamanho.
- Os monstros custom usam um tipo do jogo; monstros com arquivos próprios ainda não existem (`TODO.md`, item 8).

Conferido em 2026-10-22 (`WOTM_MATCH=3,1`, `3,1,2,5,1` e `1,3,1` com IA): a pele do Togera em PNG 4x (1024 × 1024 no lugar de 256)
e o retrato 2x no HUD do jogador e na barra do inimigo; kit com export, edição, pack (PNG, `.VAG`, um som do `SHELL_S.BNK`),
re-export sem perder edição e check. Fotos: `docs/screenshots/2026-10-22/2026-10-22_mods_png_*`.
