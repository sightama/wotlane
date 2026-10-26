# Move list: esquemas de controle e tabela estática do `.CHR` (2026-10-26)

Continuação de `80b3802:docs/investigacoes/2026-10-25/move_list_especiais.md` e `move_list_comuns.md` (seções "Em aberto"):
o par de botões do especial e do crowd control nos outros esquemas de controle, e uma tabela estática por monstro
(`tools/chr_dump.py --specials`) com as chaves dos especiais, os anéis do CrowdControl e o ganho de stamina dos `AttackState`.
Leitura do asm (`PadFlags`, `PadFlags_2..4`, `charreset_2`, `TheGame`, `MonsterInit`, `MonsterStates`) e de dois dumps de RAM de
luta que já estavam em `build/dumps_live/` (`ram_congar2.bin`, `ram_energy_robot.bin`). Sem assembly aqui.

## A) Esquemas de controle

### Quem escolhe o esquema

`PadFlags` fica em Monster +0x5040, então o esquema `PadFlags` +0x16EC é o mesmo campo que Monster +0x672C. O único leitor que
decide a entrada é `PadFlags::interpretInputs`: valor 1 chama `updateInput1Config*`, valor 2 chama `updateInput2Config*` e
**qualquer outro valor** (0, -1, 3...) cai no `updateInput0Config*`. Dentro de cada esquema, `PadFlags` +0x16E4 escolhe a
variante: 0 = `ConfigCR` (relativa ao monstro), diferente de 0 = `ConfigSR` (relativa à tela; o jogo a liga na vista unificada e
no Crush, `75f5be1:docs/investigacoes/2026-10-22/camera_controle_cr_sr.md`). Ou seja, "SR" não é um esquema: é a variante de tela
do mesmo esquema, e o esquema 0 tem a dele (`updateInput0ConfigSR`).

Escritas de Monster +0x672C / `PadFlags` +0x16EC no ELF:
- `PadFlags::PadFlags` e `PadFlags::init` zeram (é o que a IA fica usando).
- `Monster::playerInit`: `m_ctlrCfg[m_index] − 1` (Shell +0x2B8C; só duas vagas, os jogadores 3 e 4 caem no `m_vibration`).
- `TheGame::SetPlayerMonster`: **depois** de chamar o `playerInit`, regrava +0x672C com `m_ctlrCfg[i] − 1`, onde `i` é o índice
  do monstro em `m_slots` (não o do jogador), só para `i` < 2.
- O menu de pausa (`ShellFinished`), na troca de CONTROL TYPE, que o port bloqueia.

Leitores além do `interpretInputs`: o HUD (texto das dicas) e o `PowerUps` (caixa de texto no canyon); nenhum muda a entrada.

### Par de botões por esquema

Contadores de quadros segurados (`updateHistoryN`, a partir dos bytes do `GamePad` que o `loadPadInputs` monta): +0x1658 L1,
+0x165A L2, +0x165C L3, +0x165E R1, +0x1660 R2, +0x1662 R3, +0x1612 quadrado, +0x1614 círculo, +0x1616 triângulo, +0x1618 X.
A regra é a mesma nos seis: os dois contadores entre 1 e a janela `PadFlags` +0x16C4, ou a flag já ligada no quadro anterior e os
dois ainda apertados.

| esquema | especial (`PadEntry` +0x36) | crowd control (`PadEntry` +0x38) |
|---|---|---|
| 0 (tipo A), CR e SR | círculo + triângulo; zera agarrar (+0x2E) e triângulo (+0x2C) | quadrado + X; zera +0x30 e pulo (+0x2A) |
| 1 (tipo B), CR e SR | círculo + triângulo | quadrado + X |
| 2 (tipo C), CR e SR | R1 + R2; zera agarrar (+0x2E) e defesa (+0x34) | L1 + L2; zera +0x32 e pulo (+0x2A) |

As variantes SR usam os mesmos contadores que a CR do mesmo esquema.

### Resposta prática

Com +0x672C em 0, o jogo só chama `updateInput0ConfigCR` ou `updateInput0ConfigSR`, e os dois usam círculo + triângulo e
quadrado + X. O SR não é outro par de botões. **O item "conferir 1, 2 e SR" não se aplica ao port**: a MOVE LIST pode
continuar mostrando ○+△ e □+✕.

Duas ressalvas sobre o "+0x672C = 0 sempre":
- O gancho do port grava 0 na saída do `playerInit`, mas o `SetPlayerMonster` regrava o campo logo em seguida (slots 0 e 1) com
  `m_ctlrCfg − 1`. Fica 0 porque o `m_ctlrCfg` vale 1: o `screenWaitForStart` troca 0 por 1, e o `frame` dos menus em texto
  (`wotm_mainmenu.inc`) baixa para 1 o que passar de 1. Com `WOTM_TEXTMENU=0` esse `frame` não roda, e um save com
  `controllerConfig` 2 ou 3 (o JSON grava e devolve o Shell +0x2B8C) poria os slots 0/1 no esquema B ou C. Saída barata: gravar
  0 também na saída do `SetPlayerMonster`, ou fixar 1 no `m_ctlrCfg` ao carregar o save.
- Se o `m_ctlrCfg` chegar a 0, o campo fica −1: a entrada continua no esquema 0 (o `interpretInputs` aceita), mas o lock-on
  alternado do port (`wotm_overrides.cpp`, `remap` só com `PadFlags` +0x16EC == 0) desliga para esse jogador.

Nos dois dumps de luta, o jogador 0 está com +0x672C = 0.

## B) `tools/chr_dump.py --specials`

Uso: `python tools/chr_dump.py disc/RST/*.CHR --specials`. Para cada monstro imprime, na ordem do arquivo:
- cada chave de especial com o estado de destino (offset no Monster) e, por campo, o offset no estado, o valor e o papel (com
  `?` quando o asm do estado não confirma);
- os estáticos do `HomingBug` da chave 0xFAA;
- a onda de choque do CrowdControl (0xFE1) e do RobotSpecial (0xFCF): número de anéis, `timeScale`, a tolerância vertical
  (+0x0 do anel 0) e uma linha por anel com alcance, dano, os campos +0x8/+0xC/+0x10/+0x14/+0x18;
- a linha do ganho de stamina dos `AttackState`.

OGRE e SPIDER usam outra numeração de chaves (a 0xFCC deles não tem os 0x26B0 bytes; a 0xFE1 do OGRE tem o tamanho de uma
tabela de socos). A ferramenta os marca e pula; a tabela de socos também passou a recusar uma 0xFCC desse tamanho.

### Mapa do `charreset::readKey` (correções à tabela anterior)

O rastreio de todos os casos (offset lido da chave → offset escrito no Monster) confirma a tabela de
`move_list_especiais.md` para 0xFA9, 0xFAA, 0xFAD, 0xFBC, 0xFC4, 0xFC5, 0xFC7 e 0xFC8, com estas correções:
- **0xFAB não é do RobotSpecial**: escreve no `StateKnockBack` (+0x7EA0, campos +0x14..+0x88). Todos os monstros a têm.
- **0xFAD**: o +0x3C da chave só vai para FireBreath +0x34 (lentidão) se for diferente de zero.
- **CrowdControl é a 0xFE1** (todos os monstros do disco); a 0xFDF faz o mesmo, mas nenhum `.CHR` a usa. A 0xFE2 não é do
  crowd control: é do `StatePickUp` (+0xDC70).
- **RobotSpecial é a 0xFCF** (só o ROBOT a tem); a 0xFE0 faz o mesmo e não é usada. As duas só copiam se `m_typeBits` == 0x40.
- **Anéis na chave**: na 0xFE1 e na 0xFCF cada anel ocupa 0x1C bytes (7 floats: alcance e os campos +0x4..+0x18 do anel); nas
  variantes sem uso (0xFDF, 0xFE0) são 5 floats (sem +0x14/+0x18). O número de anéis é float na chave e vira inteiro no estado.
  CrowdControl: anéis a partir da chave +0x34, contagem na +0x28, `timeScale` na +0x2C (estado +0x2DC), +0x0 do anel 0 na +0x30.
  RobotSpecial: anéis a partir da +0x48, contagem na +0x3C, `timeScale` na +0x40 (estado +0x2F8), +0x0 do anel 0 na +0x44.
- **Tolerância vertical**: a chave grava o +0x0 do anel 0 do CrowdControl. Vale **120** em quase todos os monstros (10 no
  ASSBOSS e no JELLY, 0 no FINAL), e o RAM do Congar mostra 120. Isso contradiz o "sempre zero" de
  `move_list_campos_especiais.md`: o filtro de altura do crowd control está ligado no jogo. No RobotSpecial vale 0.
- ASSBOSS: o `timeScale` do crowd control é 0 (a onda não cresceria); como o chefe não usa o crowd control pelo botão, fica só a
  observação.

### Ganho de stamina dos `AttackState`

O `creditStaminaForAttack(f)` credita estado +0x14 + estado +0x18 × f. O construtor de `AttackState` zera os dois, nenhum dos 28
construtores derivados os grava, nenhum caso do `readKey` escreve +0x14/+0x18 de um `AttackState` (o Counter recebe +0x14/+0x18
da 0xFA6, mas não é `AttackState`), e a busca por escritas nesses offsets absolutos no ELF não acha nada. Os dois dumps
confirmam: +0x14/+0x18 valem 0 em todos os `AttackState` (Punch, BatSwipe, ButtSlam, RamAttack, especiais, GrappleOHAttack,
MonkeyOHAttack, StompAttack...). Os golpes dão `creditStamina(0)`; o ganho de stamina por golpe que existe vem dos objetos
(`s_info` +0x30, já na MOVE LIST), não desses campos.

### Conferência com a MOVE LIST (`native/runtime/wotm_training.inc`)

Todos os offsets que a MOVE LIST lê para o especial e o crowd control batem com o destino do `readKey`: número de anéis
+0xF42C, dano do anel 0 +0xF450, último alcance +0xF67C + 4(n−1), força +0xF458 e atordoamento +0xF460 (+0x14 do anel 0, que o
`ShockWave::update` manda para `HitEvent` +0x3C) do crowd control; onda do RobotSpecial em +0x58 e do RockSpecial em +0x44;
SonicRoar +0x3C/+0x34/+0x2C; FireBreath +0x24/+0x28/+0x20/+0x1C; ZapAttack +0x38/+0x34/+0x2C; GrappleHook +0x34/+0x30;
LavaBlast +0x190 (inteiro, cópia crua da chave) e +0x19C. Nos dois dumps, os anéis do crowd control na RAM são os da 0xFE1 do
MONKEY e do ENERGY, número a número, e o SonicRoar do Congar na RAM é o da 0xFA9 (dano 30, alcance 620, laço 60 quadros).

Erros na MOVE LIST:
- **AirStrike, duração dos discos**: AirStrike +0x178 é inteiro (o construtor e o `update` o tratam como inteiro; o ALIEN.CHR
  traz 720, 12 s). A MOVE LIST o lê com `rdf` e mostra "FOR 0 FIELDS". Corrigir para `rd32`.
- **LavaBlast, dano por bola**: o LAVA.CHR dá 2.6 por bola, que o `%.0f` mostra como 3. Usar `%.1f`.

No jogo (não é erro da MOVE LIST): o LAVA.CHR grava inteiros (120 e 10) nos dois campos de homing do LavaBlast (+0x194/+0x198),
que o jogo lê como float; o homing das bolas fica praticamente zero. A ferramenta marca o caso.

## Em aberto

- O papel dos campos com `?` na ferramenta (blends, `+0x24`/`+0x28` dos laços, +0x44 do FireBreath...), quando algum entrar
  numa dica.
- LavaBlast com homing ~0 e 440 mph: ver numa luta com o Magmo se as bolas de fato saem sem perseguir (dump de RAM).
