# Save: os três campos em aberto (2026-10-24)

Investigação dos pontos que o item 3 do `POS_FRONTEIRA.md` ("Save em JSON") deixou em aberto: `Shell+0x2A40` (offset 0x0C do arquivo
de opções `BASCUS-97197`), `TheGame+0x1204B0..B8` (offsets 0xE8..0xF3) e o que o `McScrambleIcon` faz com o `icon.sys`. Tudo pelo
`asm/` (busca por offset em todos os `.s`, inclusive bases montadas com `lui`/`ori` e com `addiu` de base + deslocamento), pelo `src/`
e pelo `icon.sys` que o runtime deixou em `C:\Users\TwistZero\wotm-recomp-win\discroot\mc{0,1}\BASCUS-97197\`. Nada foi alterado
em `src/`, `include/`, `config/` ou `native/`.

Resumo:

| Campo | O que é | Certeza | Para o `save.json` |
|---|---|---|---|
| `Shell+0x2A40` (0x0C) | opção cortada do menu de opções: valor 0..100 com 50 no meio, provavelmente um terceiro controle deslizante; ninguém lê | alta para "morto", média para "terceiro slider" | morto: pode ficar em `unknown` (sugestão de nome abaixo) |
| `TheGame+0x1204B0..B8` (0xE8..0xF3) | **recorde do endurance por dificuldade** (monstros derrotados pelo jogador 1), um int para cada `options.difficulty` 0, 1, 2 | alta | dar nome (`records.endurance`); e o gancho do `EndureSave` hoje **não atualiza** o recorde |
| `McScrambleIcon` | durante a gravação das opções o `icon.sys` é regravado com 964 bytes zero (sem a marca `PS2D`) e, depois que o arquivo de opções foi gravado, é refeito igual ao original | alta | nada muda no fim; o `icon.sys` sai igual. No port com a gravação automática ele nem roda |

## 1. `Shell+0x2A40`: opção morta

**Acessos.** Em todo o `asm/` há só três instruções com o deslocamento 0x2A40 sobre o ponteiro `shell`: a escrita do valor 50 no
construtor `Shell::Shell` (junto com os outros padrões: sfx 0x400, música 0x33E, `m_killTarget` 0), a leitura em
`MonsterMc::getSettingsData` (vai para o offset 0x0C do bloco) e a escrita em `MonsterMc::setSettingsData` (volta do bloco). Não há
outro acesso: nem com outra base (as bases `shell + 0x29xx`/`0x2Axx` montadas com `addiu` são `m_levelNumAIs` em 0x29E0, o slot 0x2948
do `setSaveData` e o `ShellSound` em 0x2918, que tem só 0x20 bytes e nenhum acesso a +0x128), nem indexado (os índices sobre
`m_monsterFlags` e `m_levelNumAIs` param antes), nem no `src/` decompilado, nem no `native/` (fora do `wotm_save.inc`, que só copia).
O `SoundManager` (em `shell+0x2C30`) lê só 0x2A30 e 0x2A34; o grupo de volume 2 do 989snd nunca recebe valor do Shell.

**O que provavelmente era.** Três pistas apontam para uma opção do menu de opções que foi cortada:
- No arquivo, o campo fica entre o volume da música e o mono, na ordem das opções de áudio; no Shell, logo depois de
  `m_sfxVolumeSaved`/`m_musicVolumeSaved` (a cópia que a tela de opções guarda para o "voltar").
- 50 numa escala 0..100 é "no meio", padrão típico de um controle deslizante; os dois volumes de verdade usam outras escalas
  (0..0x400 e 0..0x33E).
- O banco do shell tem um **terceiro botão de controle deslizante**: o entry point 739 é ativado no `Shell::AddEpNode` como cs
  (`m_cs60C`), exatamente como os dois de volume (737 e 738, `m_sliderCs[0..1]`, que o `setSlider` move), mas o código nunca usa o
  `m_cs60C` depois disso. A tela de opções (`screenOptions`) tem 10 itens (dificuldade, configuração dos dois controles, estéreo/mono,
  volume de efeitos, volume da música, vibração dos dois controles, carregar, gravar) e nenhum lê 0x2A40. Nenhuma string do ELF nomeia
  a opção (os textos do menu são texturas do banco de dados).

Um terceiro volume (voz ou locutor) ou um ajuste de tela (brilho, centralização) são os palpites mais prováveis; não dá para escolher
entre eles sem olhar a textura do item no banco do shell (ep 739 e vizinhos). **Certeza:** alta de que ninguém usa o valor; média de
que era um terceiro slider; baixa sobre qual.

**Para o `save.json`:** o valor não tem efeito em nada; pode continuar como está. Se quiser tirar de `unknown`, um nome honesto seria
`options.unusedSlider` (ou deixar `unknown.shell2A40` e só trocar o comentário no `wotm_save.inc`/`mcsave.py` para "opção cortada do
menu; o construtor põe 50 e ninguém lê"). Gravar sempre 50 seria igualmente fiel.

## 2. `TheGame+0x1204B0..B8`: recorde do endurance por dificuldade

**Acessos.** O global é o ponteiro `game` (`extern TheGame *game`, também lido gp-relativo). Os três ints aparecem só no
`game/MonsterMc`, de três maneiras:
- `MonsterMc::getSettingsData` copia `game+0x1204B0`, `+0x1204B4`, `+0x1204B8` para os offsets 0xE8, 0xEC, 0xF0 do bloco;
- `MonsterMc::setSettingsData` faz o caminho inverso;
- **`McSaveSettings::update`** lê `game+0x1204B0 + dificuldade*4`, com a dificuldade em `game+0x1203CC` (o `m_matchMode`, que é
  `options.difficulty`, 0..2).

Nenhum outro arquivo do `asm/` toca esse trecho (busca pelo endereço absoluto montado com `lui`/`ori`, pelo deslocamento 0x04B0..0x04B8
depois de um `lui 0x12`, e no `src/`). Quem está em volta não tem relação com o `m_actuator` do `game.h`: o `m_actuator` real só tem
dois ints (`game+0x120498` e `+0x12049C`, a vibração de cada controle, lida e trocada pela `screenOptions`); `+0x1204A0..A8` são três
tweaks globais (escritos e lidos pelo `GlobalReset::writeKeys`/leitura de chaves, usados por `doFireVolume` e pela colisão),
`+0x1204AC` é um float que o `StateAirStrike::update` grava, e `+0x1204C0` é o `StreamingSoundManager` (`startCinemaMusic` etc.). Ou
seja, o `int m_actuator[8]` do `game.h` está largo demais (cobre de 0x120498 a 0x1204B8); isso é só nome de header, fica registrado aqui
para quando alguém mexer no `game.h`.

**O que o `McSaveSettings::update` faz.** A página que grava o arquivo de opções lê primeiro o arquivo que já está no cartão e monta o
bloco novo a partir dos dois, conforme cinco flags da página (`McSaveSettings+0x274..0x284`, que cada `MonsterMc::update*` liga ou
desliga antes de abrir a sequência):
- `+0x274` (opções): desligado, os offsets 0x00..0x20 vêm do arquivo antigo; o `updateUnlockSave` desliga (comprar na tela de
  destrave não grava as opções que estão na tela);
- `+0x278` (tokens): desligado, o offset 0x24 vem do arquivo antigo;
- os 48 destraves são o "ou" do antigo com a RAM;
- os três ints de 0xE8 **sempre começam com os do arquivo antigo**; com `+0x280` ligado, o jogo pega o `m_winsThisGame` (Monster+0x3C)
  do primeiro monstro (`game->m_monsters[0]`, o jogador 1) e, se for maior que `game+0x1204B0[dificuldade]`, grava esse número no
  slot da dificuldade;
- `+0x284` (sempre ligado em todas as sequências): desligado, o trecho 0xF4..0x34B viria do arquivo antigo; ligado, fica o lixo da
  pilha que o `POS_FRONTEIRA` já descreve.

O `+0x280` só é ligado pelo **`MonsterMc::updateEndureSave`** (as sequências de opções, destrave, fim da história e aventura o
desligam). O `m_winsThisGame` do jogador é somado no `StateDeath::transitionInto` (o matador ganha um a cada monstro que morre); no
endurance, em que as IAs são trocadas a cada morte e não há meta de vitórias, ele é a contagem de monstros derrotados na corrida. Então
os três ints são **o recorde do endurance, um por dificuldade** (fácil, médio, difícil na ordem dos `m_difficultySw`). **Certeza:
alta** (o índice pela dificuldade, a flag exclusiva do endurance e a comparação "maior que" não deixam outra leitura).

**Ninguém mostra o recorde.** Fora das três funções acima, nada lê esses ints (nem o diálogo de fim do endurance, nem o HUD, nem a
tela de opções). O jogo grava o recorde e nunca o exibe: provavelmente uma tela de recordes cortada, como a opção do item 1.

**Uma esquisitice do retail.** A comparação é com a cópia na RAM (que só é atualizada pelo `setSettingsData`, isto é, no boot ou num
"load settings"), mas o valor de partida é o do arquivo, e a gravação não atualiza a RAM. Numa mesma sessão, uma segunda corrida pior
que a primeira mas melhor que o recorde do boot sobrescreve o recorde da primeira (ex.: boot com 0, corrida de 10 gravada, corrida de 5
na mesma sessão: 5 > 0, o arquivo fica com 5). Como ninguém exibe o número, o defeito nunca apareceu.

**Para o `save.json` e o port.**
- **Nome:** trocar `unknown.game1204B0` por algo como `records.enduranceBest` (ou `records.endurance`), de preferência por nome da
  dificuldade (`{"easy": 0, "medium": 0, "hard": 0}`) ou um array de 3 na ordem de `options.difficulty`. Na leitura, aceitar também a
  chave antiga (`unknown.game1204B0`) para os JSON já criados, e apagá-la na reescrita.
- **O gancho do `EndureSave` não reproduz o recorde.** No `native/runtime/wotm_save.inc`, o `seqReplaced` troca as seis sequências
  por `applyPendingUnlock` + `ramToJson`, e o `ramToBlock` copia `game+0x1204B0..B8` da RAM, que nunca é atualizada pelo jogo. Com
  a gravação automática (`WOTM_SAVEAUTO`, padrão), o recorde fica para sempre no valor do boot (0 em todo save novo). Sugestão (não
  aplicada): no `seqReplaced`, quando `h.seq == Seq::EndureSave`, antes do `ramToJson`, ler `d = game+0x1203CC` (limitado a 0..2),
  `m = game+0x120380` (ponteiro do monstro do jogador 1) e `w = m+0x3C`; se `m` não for nulo e `w` for maior que o recorde guardado
  (o do JSON, não o da RAM, para não herdar a esquisitice acima), gravar `w` em `game+0x1204B0 + d*4` na RAM. Assim o `ramToJson`
  leva o valor novo e a RAM fica coerente com o arquivo. Se a fidelidade exata ao retail importar mais (incluindo o defeito), comparar
  com a RAM e não escrever nela; mas como o número não aparece em lugar nenhum, a versão corrigida é a mais útil (e é a que um mod de
  tela de recordes vai querer).
- `tools/mcsave.py`: só a troca do nome da chave e o comentário; o layout não muda.

## 3. `McScrambleIcon`: apaga o `icon.sys` durante a gravação e o refaz depois

**Formato do `icon.sys`** (o que o runtime gravou, igual em `mc0` e `mc1`, 964 bytes): marca `PS2D` em 0x00; 0x06 = 22, a quebra
de linha do título em bytes; 0x0C = 128, a transparência do fundo; 0x10..0x4F as quatro cores dos cantos do fundo (dois cantos pretos
em cima, dois cinza 32,32,32 embaixo); 0x50..0x7F as três direções de luz; 0x80..0xAF as três cores de luz (avermelhada, alaranjada,
azulada); 0xB0 a luz ambiente 0,5; 0xC0..0x103 o título em Shift-JIS de largura cheia, "Ｗａｒ　ｏｆ　ｔｈｅ　Ｍｏｎｓｔｅｒｓ" (os 22 bytes
da quebra são as 11 letras de "War of the ", então o navegador mostra "War of the" / "Monsters"); 0x104, 0x144 e 0x184 os três nomes
de ícone (lista, cópia e apagar), todos `ICON.ICO`; de 0x1C4 ao fim, zero.

**Quem monta.** `createIconSysSync`/`createIconSysAsync` (em `game/McFile`): zeram um buffer de 0x3C4 bytes, copiam a marca, a
quebra 22 e a transparência 128, as cores e luzes de quatro tabelas constantes (`s_bgcolor`, `s_lightdir`, `s_lightcol`, `s_ambient`),
o título convertido de ASCII para Shift-JIS a partir do `McState+0x8C` e o nome do ícone do `McState+0xAC` nos três campos; abrem
`<pasta>/icon.sys` com criação e gravam. **Nada no conteúdo depende do save**: o `icon.sys` é sempre o mesmo.

**O que o `McScrambleIcon` faz.** É uma tarefa do cartão com uma flag em `+0x8`:
- flag ligada: chama o `createIconSys*` (o `icon.sys` correto, acima);
- flag desligada ("embaralhar"): abre o `icon.sys` existente sem criar e grava 0x3C4 bytes de um buffer em que só zera três bytes,
  0x104, 0x144 e 0x184 (o primeiro caractere de cada nome de ícone). A versão assíncrona, que é a que o `McSync::sync` chama (pela
  vtable, a entrada do `updateAsync`), usa um buffer do `.bss` (`D_0070EC58`, 0x3E8 bytes) que nada mais escreve: **o arquivo vira 964
  bytes zero**, sem a marca `PS2D`. A versão síncrona (não usada por esse caminho) gravaria lixo da pilha com os três nomes vazios.

**Quando.** Só o `McSaveSettings::update` usa a tarefa, e numa ordem fixa quando a pasta já existe: (1) tarefa com a flag desligada,
o `icon.sys` vira zeros; (2) grava o `BASCUS-97197`; (3) passa de página, liga a flag na **mesma** tarefa e a roda de novo, o
`icon.sys` volta a ser o normal. Se a pasta não existe, no lugar dela vai um `McSetupGameDir` (cria a pasta, o `icon.sys` e o
`ICON.ICO`). É o truque comum do PS2 para gravação interrompida: se o console desligar ou o cartão sair entre (1) e (3), o navegador
do PS2 vê um `icon.sys` inválido e mostra o save como corrompido, em vez de mostrar um save bom com o arquivo de opções pela metade. O
`savegame.dat` (aventura) não passa por esse esquema.

**Bytes que mudam:** no estado final, nenhum (o arquivo depois da gravação é igual ao de antes, byte a byte, e não depende de valor
nenhum do jogo). No meio da gravação, todos os 964 bytes ficam zero. **Certeza:** alta (o buffer do `.bss` só tem essas três escritas;
a ordem das tarefas está explícita no `McSaveSettings::update`; o `icon.sys` do disco do runtime bate com as tabelas).

**Para o port.** Com a gravação automática (padrão), as páginas do cartão não rodam e o `icon.sys` não é tocado; com
`WOTM_SAVEAUTO=0`, o stub `sceMc*` do fork grava os zeros e logo depois o arquivo certo, com o mesmo resultado. Nenhuma mudança
necessária no `wotm_save.inc` nem no `mcsave.py`. Opcional, só por robustez: quando o `stdInit` refaz os binários a partir do JSON,
conferir se o `icon.sys` existe e começa com `PS2D` e, se não (gravação interrompida com `WOTM_SAVEAUTO=0`), regravá-lo com o
conteúdo constante acima (ou copiar o do outro cartão). O ícone em si é o item 6.
