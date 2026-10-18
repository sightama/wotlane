# Corda do Ultra-V: é a partícula tipo 12 (VisChain), que o nativo não desenha

Fotos `<runtime>\fotos\2026-10-17_224038\foto_001` e `foto_002` (VEGAS, `SHOGUN0`/`SHOGUN1`). Refeitas com
`ps2EntryRunner --vu1cmp <cópia> --dump <prefixo>` numa pasta de rascunho (cópias dos `.vu1snap`, já apagadas).

## Resultado (2026-10-17, corrigido; conferido ao vivo pelo dono)

Feito em `native/wotm_fx.hpp` pela alternativa sem refatorar do passo 2: `FX_CHAIN = 12` ("corrente") e o `arcQuads` com um modo
corrente (cabeçalho em `pos − 0x4C`, 2 cores em `pos − 0x28`, sem subdivisão nem leitura do `w` dos elos); `forEachQuad` e `draw` tratam
o tipo 12 como o arco. A medição corrigiu três pontos da receita abaixo:

- **Largura:** com a fórmula do arco (`sz = 0,5 · minSize`) e o q265.x = 10 do pacote (conferido na RAM: `pos − 0x4C` = 10,0), a fita
  saía com o **dobro** da largura do VU1 (meio-desvio (28,1; 5,0) contra (14,06; 2,5) no ponto 0). A corrente usa `sz = 0,25 · minSize`.
  A conta da seção "Largura" abaixo errou por um fator 2 em algum passo (não fui atrás de qual).
- **Pontos coincidentes:** são os pontos 11 a 30 (não 12 a 30). Para não perder o trecho 10–11, na corrente uma tangente de comprimento zero
  vira o vetor zero (no VU, 0 · RSQRT(0) = 0) em vez de descartar o ponto; os quadrados 11 e 12 do VU1 (o fim recolhido, poucos pixels)
  continuam faltando.
- **Textura:** o U vai de −4,5 a −0,9 e o GS repete a `ChainTex`; as texturas de partícula do nativo ficam em CLAMP (saía uma faixa lisa).
  REPEAT no S só durante o desenho da corrente.

Checagem: foto 002, fx 82 de 0/12 para **10/12** casados; foto 001, 6/8; a corrente recolhida (fx 83) sem nada, como no VU1. A corda
torcida aparece igual à referência (`docs/screenshots/2026-10-17/2026-10-17_corda_ultrav_corrigida.png`). Corpus contra
`resumo_2026-10-17_232701.csv`: nada piorou. Foto 002 guardada no corpus como `ultrav_corda`. O dump da checagem (`fxp`) agora grava os
pontos de sistemas sem `vel`/`age` (a corrente). Conferido ao vivo pelo dono em 2026-10-17: funcionou.

## Conclusão

A "corda" é o **sistema de partícula tipo 12 (`VisChain`, `include/hieri_types.h`)**, criado por `chainCreate` (`src/common/chain.cpp`)
no construtor do `OgreMace` (`__8OgreMaceP7Monster`, o mangual do Ultra-V; o `update__8OgreMace` chama `updateShogunSpecialSound`).
O pacote é montado por `chainPacketInit`/`chainPacketBuild` (`asm/common/partdraw.s`) e desenhado pelo **microprograma 0x1D00** do VU1
(`build/vu1/particle.txt`), que chama a mesma sub-rotina de fita **0x1F70** do arco elétrico e do stripe. Textura `ChainTex` (id 20,
a corda torcida), TRISTRIP texturizada (PRIM 0x7C: IIP, TME, FGE, ABE; STQ), `alphaContext` 0x44.

- **O runtime gera a corda:** o VU1 do replay a desenha nos dois instantâneos (a imagem `ref` mostra a corda torcida da mão do Ultra-V
  até o inimigo).
- **O nativo não tem o tipo 12:** `FxType` (`native/wotm_fx.hpp:176`) não tem 12; o `collect` (l. 364–372) aceita o sistema, e
  ele cai no caminho genérico de sprites do `forEachQuad` (l. ~1245: `half = |pos.w|`). Os pontos da corrente têm `w` ≈ 0 (a cabeça
  leva o inteiro 31 nos bits do `w`, 4,3e-44 como float), então sai **um quadrado de tamanho zero por ponto**: invisível.
- **O rastro vermelho que aparece no nativo é outro sistema:** fx 64, preset 56, **stripe (tipo 5)**, 9/9/9 quadrados casados. O
  ponto mais novo dele (618,5; −2181,1; 289,2) é o mesmo `pos[0]` da corrente: é o rastro da ponta da corrente, que o nativo já desenha
  certo.

Certeza: **alta** (95%) de que a falta é só o tipo 12 não implementado no nativo. Média-alta (~85%) de que a receita abaixo (a fita do
arco com o cabeçalho da corrente) reproduz o VU1 sem ajustes; a largura, o U e o centro já foram conferidos à mão contra os vértices do VU1
(abaixo).

## Evidência

### Checagem (`vu1cmp`)

| foto | g_frame | sistema | VU1 | nativo | casados |
|---|---|---|---|---|---|
| 002 | 1651 | fx 82, tipo 12, n = 31 | 12 quadrados (24 triângulos) | 31 quadrados degenerados | 0 |
| 002 | 1651 | fx 64, preset 56, stripe | 9 | 9 | 9 |
| 001 | 548 | fx 82, tipo 12, n = 31 | 8 | 22 | 0 |
| 001 | 548 | fx 83, tipo 12, n = 31 | 0 (corrente recolhida) | 31 | 0 |

Linha de tipos: `12 ? 0/12/31` (002) e `12 ? 0/8/53` (001). No dump do 002, cada `natq` do fx 82 tem os quatro cantos no mesmo ponto
(tamanho zero), e os `vut` do VU1 vêm do programa 0x1D00 (`hdr,627,1d00`), com TEX0 `20034cc5593063d2`, cor `8030383c` nos três
vértices, ALPHA 0x44, ABE 1. Na 001 há duas correntes (fx 82 e 83, os dois Ultra-V); a 83 estava recolhida (pontos coincidentes) e o VU1 não emite nada.

Imagens (pasta de rascunho `C:\Users\TWISTZ~1\AppData\Local\Temp\claude\C--Users-TwistZero-WoTM\25ec8be3-f267-4d84-b829-0d8d2a10dc34\scratchpad\inv_corda\`):
`foto_002_1651_trio.png` (ref com a corda, nat sem ela) e `foto_001_548_trio.png`.

### Quem cria e com que parâmetros

- `OgreMace` (ctor): `chainCreate(minSize 20.0, 0.3, 4, cor 0x8030383C, ChainTex (0x14), 0.0)` e `chainAddSegment(fx, 31)`: uma
  corrente de 31 pontos. O `MonkeyChains::init` (Congar) usa o mesmo tipo: `chainCreate(20.0, 0.22, 4, 0x8030383C, 0x14, 0.0)` e seis
  `Chain`. Ou seja, **a mesma correção desenha também as correntes do Congar** (hoje recolhidas nas capturas, mas o mesmo buraco).
- `chainCreate` grava `vis.attribs = 12`, `alphaContext = 0x44`, `texInfoIndex = tex`, `info.minSize = 20`, e chama
  `chainPacketInit(p, z = 0.0, y = 0.3)`.

### Layout do pacote (relativo a `pos` = `fx + 0x2EB0`)

Lido de `chainPacketInit`/`chainPacketBuild`:

| endereço | conteúdo | no VU1 |
|---|---|---|
| `pos − 0x5C` | GIF tag (TRISTRIP 0x7C, 5 regs: ST, RGBAQ, XYZF2, ST, XYZF2); o 1º word (NLOOP) é sobrescrito com `numParticles` pelo `chainPacketBuild` | q264 (`ILW.x` = pontos a desenhar) |
| `pos − 0x4C` | (minSize·0,5 = 10; 0; z = segInitU = 0; 642,0) | q265 |
| `pos − 0x3C` | (0 = expansionRate; 0; y = segDeltaU = 0,3; 0 = numColors−1) | q266 |
| `pos − 0x2C` | UNPACK V4-32 ×2 para q642 | |
| `pos − 0x28`, `pos − 0x18` | cor em float 0..255, duas vezes: (R 0x3C, G 0x38, B 0x30, A 0x80) = (60, 56, 48, 128) | q642, q643 |
| `pos − 4` | UNPACK de `n` qwords para q8 (gravado pelo `chainPacketBuild`) | |
| `pos[k]` | pontos; a cabeça de cada corrente tem no `w` o **inteiro** com o número de pontos dela (`chainPacketBuild` grava `pos[n_antigo].w = count`) | q8… |

É o **mesmo formato de cabeçalho do arco** (`arcQuads`: minSize, —, segInitU, clut; expansionRate, —, segDeltaU, numColors−1), com três
diferenças: (1) offsets (arco: `pos − 0x28`/`pos − 0x18` e cores em `pos − 0x13C`); (2) o "minSize" já vem multiplicado por 0,5 pelo EE;
(3) **sem subdivisão** (o 0x1D00 não tem RINIT/jag: lê `w` só da cabeça e passa os pontos direto, com os fantasmas 2P₀ − P₁ e
2Pₙ₋₁ − Pₙ₋₂, para o 0x1F70). A cor é constante (numColors − 1 = 0, clut = 642 = q642).

### Conferência numérica da fita contra o VU1 (foto 002)

- **U:** o VU1 dá S = −4,5; −4,2; …; −0,9 do ponto 0 ao 12 e T = 1/0 nos dois lados, ou seja U_k = segInitU − segDeltaU·S/2 + segDeltaU·k
  com S = n − 1 = 30 (o prólogo faz `z − 0,15·(n − 1)`), igual à fórmula do arco.
- **Centro:** o par do ponto 0 no VU1, (1955,19; 2056,50) e (1983,31; 2061,50), tem meio (1969,25; 2059,0); o nativo projeta `pos[0]` em
  (1969,29; 2059,04).
- **Largura:** meio-desvio (14,06; 2,5) px; em tela isotrópica (y / 0,47) fica perpendicular à tangente (produto escalar ≈ 0) e mede 15,0 px;
  com W ≈ 127,8 do `pos[0]` (`fxcam`), no plano de vista = 15,0 · 127,8 / 192 = **10,0** = q265.x · |v| / 2 com |v| ≈ 2 (fita quase reta).
  Exatamente o `corner()` do `arcQuads` (`sz = 0,5 · (minSize + exp·k/S)`, deslocamento `sz · v`) com minSize = q265.x = 10.
- **Pontos coincidentes:** dos 31 pontos, 12..30 estão no mesmo lugar (a corrente ainda não esticou). O VU1 emite os pares de k = 0 a 12
  (12 quadrados) e nada depois. O `arcQuads` pula pontos com tangente nula (`ok[k]`), então o nativo faria 11 dos 12 (o 12º tem
  comprimento zero, invisível). A checagem deve mostrar 11/12; não é defeito visível.

## Como resolver (passos)

Tudo em `native/wotm_fx.hpp`, sem mexer no runtime nem no patch:

1. `enum FxType` (l. 176): acrescentar `FX_CHAIN = 12`; `fxTypeName` (l. 181): `case FX_CHAIN: return "corrente";` (a checagem deixa de
   imprimir `tipo 12 ?`).
2. Extrair do `arcQuads` (l. 854–975) a parte da fita (pontos `P` → fantasmas → `v` por ponto → `corner` → recorte da banda → `g(k, cs, uv,
   col, mask)`) para um helper, por exemplo `ribbon(view, P, minSize, expRate, segInitU, segDeltaU, colorFn, g)`, e fazer o `arcQuads` chamá-lo
   depois da subdivisão. (Alternativa sem refatorar: um parâmetro `chain` no `arcQuads` que troca os offsets e pula a subdivisão.)
3. Novo `chainQuads(ram, s, view, g)`:
   - lê `minSize = f32(pos − 0x4C)` (já é o meio), `segInitU = f32(pos − 0x44)`, `expRate = f32(pos − 0x3C)`, `segDeltaU = f32(pos − 0x34)`,
     `ncm1 = f32(pos − 0x30)` (0); cor de `pos − 0x28` (RGBA float 0..255, + 0,5 truncado, como o `color()` do arco, com tabela de 2 entradas);
   - percorre as correntes: `i = 0; enquanto i < s.n: cnt = u16(pos[i] + 12)` (o `w` da cabeça, inteiro; 0 ou `i + cnt > n` encerra);
     P = pos[i .. i + cnt − 1] **sem** subdivisão nem leitura do `w` dos elos; `S = cnt − 1`; chama a fita; `i += cnt`;
   - exige `ram.ok(pos − 0x5C, 0x5C + 16·n)`.
4. `forEachQuad` (l. ~1091): ramo `if (s.type == FX_CHAIN) { chainQuads(..., mask == 3 → f(k, cc, uu)); return; }` como o do arco (é o que a
   checagem usa para casar com o VU1).
5. `draw` (l. 1399): estender o ramo do arco para `FX_CHAIN` (mesma TRIANGLES com `tri[2][3]`, `texFor(0)`, cor por ponta, `fogged`);
   a mistura vem do `alphaContext` 0x44 como nos outros.
6. Conferir: `tools\vu1cmp.ps1` nas fotos `2026-10-17_224038` (001 e 002): o fx 82 deve ir de 0/12 para ~11–12/12 casados (002) e a
   imagem `nat` deve mostrar a corda; o fx 83 da 001 deve ficar 0/0. `tools\corpus.ps1 -Check -Baseline <csv>` para ver que nada piorou
   (o `congar_cabos` tem a corrente do Congar recolhida: deve continuar sem nada). Vale guardar a foto 002 no corpus como `ultrav_corda`.
   Depois, ao vivo, o golpe do Ultra-V (e um Congar com as correntes esticadas, se der).

## Riscos e pontas soltas

- O `collect` exige `info.attribs & 0x400`: o `chainPacketBuild` só roda no `chainAddSegment` (uma vez), mas o bit continuava ligado no
  quadro 548 e 1651 (`cru: ... attribs 400`), então persiste. O `inViewFov` usa a esfera/caixa em +0x2C80/+0x2C90; passou nos dois
  instantâneos, mas não conferi se o `OgreMace::update` as atualiza a cada quadro (se não, a corda pode sumir com a câmera em certos ângulos;
  conferir no `particleDraw` se o tipo 12 passa pelo `viewInFOV`).
- O texto do 0x1D00 que pula o 13º par em diante (pontos coincidentes) não foi lido instrução a instrução; a hipótese (tangente nula vira
  vértice fora da banda e o ADC some) é coerente com o que o VU1 emitiu, e a diferença é invisível.
- **Não é deste ponto, mas apareceu na foto 001:** a referência tem muitas linhas vermelhas finas em volta do Ultra-V (o VU1 emitiu 250
  vértices de LINE, `vertices do VU1 nao comparados: linha 250`), que a imagem `nat` do replay não mostra e que não pertencem a nenhum
  `fx[]` da lista (só 72, 82 e 83). Não investiguei de onde vêm (programa e dono); fica como pista separada.
- Não comparei com o PCSX2 (estava pausado; não mexi). A referência aqui é o VU1/GS do runtime, que mostra a corda como o dono descreve.
