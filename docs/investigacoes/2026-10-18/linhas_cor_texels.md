# Linhas (FX_LINE), cor das partículas e texels: investigação de 2026-10-18

Três itens do "Inventário do port" (`docs/passos/VU1CHECK.md`, "Em aberto"). Só leitura: nada em `native/`, `tools/` ou no corpus foi mudado.
Medidas com `tools\vu1cmp.ps1 -NoPng -Full -Dump` sobre os instantâneos do corpus (`ultrav_corda2`, `luta_centro`, `congar_olhos`,
`destrocos` e uma cópia de `congar_cabos`), com `WOTM_VU1CMP_RAM` para ter a RAM do quadro. Os scripts de análise ficaram na pasta de
rascunho do agente (`tail_fit.py`, `arcinfo.py`, `fxinfo.py`, `velw.py`, `histcmp.py`, `whois.py`). Os dumps de RAM foram apagados.

## Resumo

| Item | Melhor teoria | Confiança |
|---|---|---|
| 1. Cauda das linhas | Cauda = `prevViewMat · (pos − vel − vento)`. A matriz do quadro anterior basta: erro mediano de 0,05 px nos 120 segmentos. O nativo projeta `pos − vel` com a câmera atual. | alta (medido) |
| 2a. Arco (`luta_centro`) | Não é bug do nativo. As cores do VU1 batem com a fórmula do nativo; a checagem casa quadrados vizinhos de um arco de menos de 1 px. | alta |
| 2b. Stripe preset 106 | Com `info.attribs & 0x200`, o RGB vem de `info.useThisColor` (fx+0x2E58) = (60, 50, 50, 1,0); o nativo usa a tabela. | alta (medido) |
| 2c. Alfa do strite (preset 37) | A posição na tabela de cores do strite é `vel.w` (float, com espalhamento por partícula), não `idade · colorsPerFrame`. | alta (medido) |
| 2d. Alfa do sprite (preset 29) | Não é bug do nativo: dois sprites idênticos no mesmo lugar (idade 0 e 1) e a checagem os casou cruzados. | média-alta |
| 3. Texels | Os dados das `tex 304/311` não estão em lugar nenhum da VRAM do instantâneo, e o que ocupa o TBP não é nenhuma textura do `texInfo`. O upload delas nunca chegou ao GS do runtime, ou algo que não é textura do nível escreveu por cima. | média (o fato é medido; a causa ainda não) |

---

## 1. Cauda das linhas (FX_LINE)

### O que o retail faz

- **Pacote** (`asm/common/partdraw.s:521-665`, `linePacketInit`; `:671-736`, `linePacketBuild`):
  - cores em V4-32 no q520 (`0x208`), até 16, RGBA float; depois da última, alfa 0 (`:543-592`);
  - UNPACK de 6 qwords no q536 (`:598-646`):
    - q536 = (taxa de cor = (nc − 1)/vida, palavra 1 do GIF tag, REGS 0x5151, n);
    - q537 = **vento** = `field.wind.xyz + Gusts · field.wind.w`. O `field` fica em fx+0x2DE0 e o `wind` em fx+0x2DF0 (`include/hieri_types.h:1039`); o vento é escrito a cada quadro pelo `linePacketBuild` (`:683-705`);
    - q538–q541 = a matriz em fx+0x2EC0, que o `particleDraw` recopia a cada quadro (`asm/common/particle.s:8335-8347`) de uma tabela por vista (`D_0078F5A0`);
  - `pos[]` vai para o q8 e `vel[]` para o q264.
- **GIF:** PRIM 0x49 = LINE + IIP + ABE, **sem FGE**; REGS RGBAQ, XYZF2, RGBAQ, XYZF2.
- **Microprograma** (`build/vu1/particle.txt`, MPG 0x0000; laço de 0x0058 a 0x02B8, linhas 13-89 do arquivo):
  - linhas 17-28: a matriz q538–q541 recebe z·1/16, e a translação é corrigida por `vf14 −= M3x3 · vento` (MSUBAx/y, MSUBz). Fica `M′·p = M·(p − vento)`;
  - linhas 30-31 e 58, 64: `vf18 = pos − vel`; a posição na tabela é `vel.w · taxa` (MULx.w com q536.x);
  - linha 37: **ponta** = `worldToScreen(q0–q3) · pos` (câmera atual);
  - linhas 39-49: **cauda** = `M′ · (pos − vel)` = `prevViewMat · (pos − vel − vento)`;
  - linhas 50-53: cor = lerp(tabela[i], tabela[i + 1], frac) + 0,5;
  - linhas 32, 54-55, 59, 63: **alfa** = alfa da cor · (fogFA + fogFB/W) · pos.w / 255, com teto de 255. A neblina entra no alfa, não pelo FGE;
  - linhas 67 e 71: some com alfa < 1. Some também se a ponta ou a cauda sair da banda (FCAND 0xBEF);
  - linhas 34 e 69: a cauda leva o RGB da cor com alfa 0. Nisso o nativo já está certo.

### Medida (`ultrav_corda2`, g 548, fx 66 preset 112)

Os 120 segmentos do VU1 foram pareados pela ponta com as partículas do dump (`linep`): todos os 120 têm par. A cauda foi recalculada como
`P · (pos − vel)`, com `P` = `linemat` (prevMat cru, colunas x/y/z/translação, como no `striteTriangle`): **erro mediano de 0,05 px e máximo de
0,11 px**. Ajustando também o vento por mínimos quadrados, sai (0,08; 0,02; −0,07) e o erro cai para 0,026/0,046 px. Nesta fase o vento é
quase nulo.

Logo, a frase do `VU1CHECK.md` "nem `pos − vel` pela `prevViewMat`" está errada: a conta offline anterior deve ter aplicado a matriz em outra
convenção (linhas em vez de colunas, ou com o olho subtraído). O erro de 7 a 61 px vem do nativo, que projeta `pos − vel` com a câmera atual
(`native/wotm_fx.hpp:1512-1528`; o comentário ali já diz "matriz do quadro anterior", mas o código não faz isso). A checagem
(`native/runtime/wotm_vu1check.inc:309-330`) imita o nativo.

### Correção sugerida

Fazer como o `striteTriangle` (`native/wotm_fx.hpp:293-320`):
1. Calcular `T = P·(pos − vel − vento)`, com `P` de fx+0x2EC0 e o vento de q537 (`pos − 0x60` no pacote, ou fx+0x2DF0 + `Gusts`·fx+0x2DFC).
2. Levar T de volta ao mundo pela matriz atual, na profundidade `T.w`, e desenhar a linha até esse ponto.

Na checagem, projetar a cauda direto com `P`.

Dois detalhes do mesmo microprograma, ainda sem medir:
- a cor usa `vel.w · (nc − 1)/vida`, não a idade (ver 2c);
- o alfa é multiplicado por `F · pos.w / 255`, e o nativo aplica neblina pelo shader numa primitiva que no retail não tem FGE.

No `ultrav_corda2` as duas coisas não aparecem: os dois lados saturam em `ff`.

---

## 2. Cor das partículas

### 2a. Arco elétrico (`luta_centro`, 25 de 55 com cor diferente)

As cores de vértice do VU1 dos fx 76, 78 e 79 foram recalculadas com a fórmula do nativo (`arcQuads`, `native/wotm_fx.hpp:937-947, 1048`),
usando a tabela e o clut do pacote na RAM do quadro (`pos − 0x13C`, `pos − 0x1C`). Resultado: **20 de 21 pontos iguais em cada sistema**.
O 21º difere por 1 de arredondamento (`a0c0b030` contra `9fbfaf30`).

O microprograma também foi conferido (`particle.txt:1061-1100`): índice = 138 + clut + k·(nc − 1)/S, lerp, + 0,5, FTOI0. É o mesmo que o
nativo faz.

O exemplo do `VU1CHECK.md` (`15538015` contra `1a6a801a`) é a cor do ponto k + 1 contra a do ponto k do fx 78. Esses arcos são minúsculos:
os quadrados do dump têm lados de 0,2 a 1 px (`fx76,vuq`). Com a tolerância de 1 px, o `matchQuads`/`vuQuads`
(`wotm_vu1check.inc:869-891, 1040-1068`) casa o quadrado de um passo com o vizinho, e o vértice mais próximo pega a cor do ponto ao lado.
O "nativo satura em ff onde o VU1 dá d3" é o mesmo deslocamento: `ffffff40` e `bcd3d335` são os pontos 0 e 1 do fx 79.

Nas outras lutas os arcos são maiores ou de tabela lisa e dão 0 diferenças (g 1024, 1256, 8788).

Próximo passo: na checagem, ignorar a cor nos quadrados de arco com extensão abaixo de ~2 px. O mesmo critério do `degenerate()` já
explicado em "Explicadas" serve. Ou desempatar pela cor, como nos triângulos.

### 2b. Stripe do preset 106 (rastro de poeira; `congar_olhos`, 16 de 16)

- **Código:** `stripePacketInit` (`asm/common/partdraw.s:2215-2248`). Com `info.attribs & 0x200`, as cores do pacote saem de
  `*info.useThisColor` (fx+0x2E58; `include/hieri_types.h:1112`): o RGB é o do vetor para todas as entradas, e o alfa é o alfa da tabela vezes o w
  do vetor. Sem o bit, a tabela é copiada.
- **Medido** na RAM do `congar_olhos` (g 13705): os 12 sistemas tipo 5 do preset 106 têm `info.attribs = 0x600` e `useThisColor = 0x5B4F60`,
  com o vetor (60, 50, 50, 1,0) = **`3c3232`**, exatamente a cor do VU1. A tabela (ci 736) é `70 67 57` com alfa 80, 61, 48, 3a… e é ela que o
  nativo usa (`colorAt`, `native/wotm_fx.hpp:781-796`, via `quadColor`, `:1438-1461`).
- O alfa bate porque w = 1. O `7d` contra `80` do exemplo é o lerp por idade. Isso explica a faixa clara do item 4 dos bugs vistos jogando.
- O `fragPacketInit` tem o mesmo desvio (`partdraw.s:1777-1800`). Com o bit 0x200, o RGBA vira (x, y, z, w·128) do vetor. Vale conferir o frag
  (tipo 13) com o bit ligado.

**Correção sugerida:** no `quadColor` (stripe) e na cor do frag, com `infoAttribs & 0x200` e `useThisColor` válido:
- stripe: RGB do vetor (float truncado, como o lerp + 0,5 do VU1), alfa = alfa da tabela × w;
- frag: (x, y, z, w·128).

Atenção: o `stripePacketInit` só roda quando o pacote é iniciado. Se o jogo mudar o vetor depois, o VU1 continua com o valor antigo.

### 2c. Alfa do strite (preset 37, `destrocos` g 684: 11 de 15)

- **Código:** o microprograma do strite (entrada 0xA88) lê a cor por `vel.w`, não pela idade (`particle.txt:350, 363, 371-372, 378-379`):
  - `vf5 = vel[i]` (q263 + i);
  - índice = FTOI0(`vel.w`), frac = `vel.w` − índice;
  - lerp das cores no q520.
- **Medido** (RAM do g 684, fx 72: nc 16, `colorsPerFrame` 1,25, vida 12, alfa da tabela `ff…ff e9 cb b1 93 76 58 3c 26 00`): `vel.w` é sempre
  maior que `idade · 1,25` (ex.: idade 9 dá 11,25 e `vel.w` 11,56/11,44/12,39/12,08). É um envelhecimento com espalhamento por partícula.
- Aplicando a razão alfa(`vel.w`)/alfa(idade) ao alfa nativo do dump (`fxp`, já com o `alphaK`), o resultado bate com o VU1 em todos os pares
  olhados:
  - idx 14: `2b` · 0x56/0x6f = 33 → VU1 `22`;
  - idx 12: `50` → 67 = `43`;
  - idx 11: `25` → 26,5 → VU1 `1b`;
  - idx 9: `11` → 6 = `06`;
  - idx 13 (`vel.w` 10,0059 ≈ 10,0): igual, `3f`.
- No g 3079 (`strite_robo47`) dá 0 diferenças, provavelmente porque ali a tabela é lisa onde as partículas estão.

**Correção sugerida:** no strite, usar `colorPos = vel.w` (lerp em float, + 0,5, truncado) no lugar de `colorAt(idade)`. A linha faz o mesmo
com `vel.w · (nc − 1)/vida` (item 1). O sprite **não**: o `spritePacketBuild` sobe as idades em V4-16 no q264 (`partdraw.s:1033-1095`), e no
g 684 o sprite do preset 105 casa 12 de 12 com `vel.w` bem diferente da idade. Vale olhar o stripe, o sprine e o kite pelo mesmo critério:
qual qword o microprograma lê para a cor.

### 2d. Alfa do sprite (preset 29, `destrocos` g 683: 2 de 2)

O fx 71 tem 2 partículas **na mesma posição e com o mesmo tamanho** (`fxp,71`: pos (524,266; −312,237; 111,451), w 240). Elas só diferem na
idade: 1 com alfa `6f` e 0 com alfa `80`. Os quadrados do VU1 e do nativo coincidem a 0,05 px, e a checagem os pareou cruzados. As **2**
diferenças, em vez de 1, confirmam isso: se o VU1 desse `80` nos dois, só um par sairia diferente.

Próximo passo: desempatar pela cor no `matchQuads` quando a geometria empata (o dump `vuq` não traz a cor do SPRITE; um `vuq` com RGBA fecharia
a questão).

---

## 3. Texels: referência com outra textura na VRAM

Medido no `congar_cabos` (g 1800), numa cópia do instantâneo:
- `vu1cmp` com `WOTM_VU1CHECK_TEXELS=304`: 15 pares diferem (`tex 304`: tbp 1e36, T8 64x64, 97,5% dos texels; `tex 311`: tbp 31fe, T8 256x256,
  88,2%). A paleta é igual nos dois lados.
- **Pacote de upload correto:** o `texInfo` das duas aponta um pacote com BITBLTBUF DPSM 0x13 (T8 direto, sem truque de CT32): 304 com DBP 1e36,
  DBW 1, TRXREG 64x64; 311 com DBP 31fe, DBW 4, 256x256 (`texpkt.py`).
- **Os dados não estão em lugar nenhum da VRAM:** o histograma de índices é invariante ao entrelaçamento do GS. A `tex 326` (Congar, T8
  256x256 em 2ffe, também com base fora do alinhamento de página) tem histograma **idêntico** (diferença 0) no seu DBP. A `tex 311` não bate
  nem no DBP (diferença 107%) nem em nenhum lugar da VRAM inteira (busca com passo de 2 blocos, limiar de 5%: nada). Também não é a mesma
  textura com os blocos trocados: nenhum ladrilho de 16x8, 8x8 ou 4x4 da VRAM existe na textura (`tiles.py`).
- **O que está lá não é textura do nível:** nenhuma das 331 entradas do `texInfo` com upload tem histograma parecido com o conteúdo de 31fe ou
  1e36. A menor diferença é ~107% (`whois.py`). Nenhuma outra textura do `texInfo` sobrepõe esses DBPs (`texover.py`).
- Isso descarta: ordem de replay (o `TEXREC=1` já dava o mesmo), problema de endereçamento de T8 em base não alinhada (a 326 funciona) e troca
  entre duas texturas do `texInfo` no mesmo TBP.

**Teoria:** o upload dessas texturas do nível não chegou ao GS do runtime, e a VRAM guarda o que ficou de antes. Hipóteses, da mais à menos
provável:
1. Upload feito antes do primeiro processamento do GS ou por um caminho fora da cobertura do `PS2X_GS_UPLOADS`. O `static const` do patch
   é lido na primeira chamada (`tools/runtime_patch/wotm-runtime.patch`, `s_uploads`/`s_gsUploads`). Precisa conferir se as texturas do nível
   sobem na carga (`Shell::LoadLevelFiles`) por um DMA que o runtime não entrega ao GS naquele estado (`wotmLoadFreeze`, PATH3 mascarado, VIF1
   sem o `g_wotmVif1Keep`).
2. Escrita posterior por algo que não está no `texInfo`: cartão de carga, `.RTX` do menu, quadro de filme. O cartão `LOAD` redesenhado depois
   do `LoadLevelFiles` (NATIVO.md, "Carga da fase") é um bom suspeito, e a ordem das escritas no runtime pode diferir da do PS2.

**Próximo passo concreto:** um diagnóstico no GS do runtime, por exemplo `WOTM_GSUPLOADLOG=<DBP>[,<DBP>]`:
- registrar todo IMAGE que escreva em blocos de uma faixa, com caminho (PATH2/PATH3), BITBLTBUF, `g_frame` e o PC do EE que montou o pacote;
- registrar também toda gravação de raster (FRAME/ZBUF) nessa faixa;
- rodar do boot até a luta do `congar_cabos` (CENTRAL) com o instantâneo ligado.

Se o upload de 31fe não aparecer, é a hipótese 1. Se aparecer e for sobrescrito, o log diz por quem. O teste do histograma
(`histcmp.py`/`whois.py`) pode virar uma checagem barata no próprio `--vu1cmp`: "textura ausente da VRAM" contra "outra textura no lugar".

---

## Arquivos citados
- `asm/common/partdraw.s` (`linePacketInit/Build`, `stripePacketInit`, `fragPacketInit`, `spritePacketBuild`, `electricArcPacketUpdate`), `asm/common/particle.s:8335-8347`, `src/common/particle.cpp:1242-1271` (`arcMove`)
- `build/vu1/particle.txt` (linhas: MPG 0x0000; strite 0xA88; fita do arco 0x1F70)
- `include/hieri_types.h:1039, 1042, 1074, 1112`
- `native/wotm_fx.hpp:293-320, 781-796, 937-1048, 1438-1461, 1512-1528`; `native/runtime/wotm_vu1check.inc:309-330, 869-891, 1040-1068`
