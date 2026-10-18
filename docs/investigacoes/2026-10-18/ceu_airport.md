# Céu da AIRPORT escuro (bug 2 dos "Bugs vistos jogando")

Foto `2026-10-18_021641`/001 (AIRPORT, Robo-47, `g_frame` 1444), em `C:\Users\TwistZero\wotm-recomp-win\fotos\`.

## Teoria (confiança alta)

O domo do céu da AIRPORT (pkt `c36780`, `tex 136`, nó `c36680`) tem **`fogBurnThru` = 1e7** (OBJECT +0x2C; o jogo usa isso para o céu
nunca pegar neblina). O nativo manda o burn ao shader pela normal do vértice, `rlNormal3f(burn, 1, flags)` (`native/wotm_fx.hpp:68-70`),
e o raylib **normaliza** a normal (`rlgl.h`, `rlNormal3f`: divide pelo comprimento). Com burn = 1e7 a componente y vira ~1e-7, abaixo do
limite do vertex shader (`native/wotm_native.hpp:2086`, `if (vertexNormal.y > 1e-6)`). Resultado: o shader zera **burn e todos os flags**
(FGE, **GS**, PM, modo). Sem o flag GS, o domo é desenhado como objeto comum com a textura em alfa cru:

- cor = texel × cor do vértice 0x80/255 = 0,502·T (deveria ser ×1,99 → T);
- alfa = alfa cru do texel (TCC = 0 → 0x80/255 = 0,502, `decodeTex`, `wotm_native.hpp:809-818`) × Av/255 = 0,252 no alto (Av = 0x80),
  em vez de As = Av/128 = 1,0.

Sobre o fundo preto: nativo = 0,502 × 0,252 × T = **0,126·T**; referência = T.

A segunda camada (nuvens, pkt `c37600`, `tex 137`) tem burn 3500 (y ≈ 2,9e-4, passa no limite) e sai certa; por isso as nuvens
escuras ainda aparecem no nativo por cima do domo apagado.

## Evidência

- `vu1cmp` da foto: geometria 100%, `cor/alfa/ABE/ALPHA difere` 0 no domo (os atributos do pacote batem; o erro é só no shader).
  `inv px`: `ref pkt c36780 tex 136 | nat nenhum` 42.572 px e `ref c37600 tex 137 | nat nenhum` 8.592 px.
- Dump (`WOTM_VU1CHECK_DUMP`) do domo: TEX0 `200379c1dd30aa6a` (T8 128x128, **TCC 0**, TFX MODULATE), ABE 1, ALPHA 0x44, FGE 1;
  vértices com cor 0x80 e alfa 0x42 → 0x20 → 0x0D → 0x00 descendo para o horizonte (a fusão com a skyline).
- RAM do instantâneo: nó `c36680` +0x2C = 1e7 (domo); nó `c37520` +0x2C = 3500 (nuvens). Neblina: FA −18,36, FB 317.096, FOGCOL 1b/07/1b.
- Razão nativo/referência no alto do céu (x 300-600, y 0-100 da imagem de 448 linhas): **0,13 em R, G e B igualmente**, constante em
  todas as linhas (a cor do nativo tem o mesmo matiz, só mais escura), que é o 0,126 previsto acima. Neblina errada mudaria o matiz
  (FOGCOL roxo); textura errada também.
- A textura 136 decodificada da VRAM do instantâneo é o próprio pôr do sol (laranja 255/183/67 embaixo, lilás 103/39/101 em cima;
  média 171/107/78), e a referência no alto (180/119/63) é ela com alfa 1. A textura está certa; o que falta é a modulação do GS.
- Os framebuffers 0x46 e 0x8C da VRAM de entrada estão pretos: a referência não está clara por realimentação do quadro anterior.

## Próximo passo

Limitar o burn antes do `rlNormal3f` em `shaderNormal` (`native/wotm_fx.hpp:69`), por exemplo `std::min(burn, 1024.f)` (e um piso
simétrico): com FB > 0, burn ≥ 255 − FA já satura F em `maxVal`, então o valor exato acima disso não muda nada, e y fica em ~1e-3.
(Alternativa mais limpa: mandar burn/flags por um atributo que não é normalizado, como a 2ª coordenada de textura.) Conferir com
`tools\vu1cmp.ps1` nesta foto: as linhas `ref c36780 | nat nenhum` devem sumir e a razão no alto do céu ir para ~1; depois
`tools\corpus.ps1 -Check -Baseline ...`. Vale procurar outros objetos com burn enorme (o mesmo bug apagaria qualquer um): o
`WOTM_TEXINFO` já imprime `burn` por objeto (`wotm_native.hpp:2469`).
