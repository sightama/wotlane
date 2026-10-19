# Nativo "~5% mais claro" que a referência (pista aberta de 2026-10-19)

Investigação só de leitura, sem build nem mudança de código. Scripts e saídas em
`C:\Users\TWISTZ~1\AppData\Local\Temp\claude\C--Users-TwistZero-WoTM\b542b3fa-bf42-49f1-96bd-ea9b0bc70c47\scratchpad\brilho\`
(`razao.py`, `dif.py`, `mapa.py`, `classes.py`, `modelo.py`, `pares.py`). Dados: os `_ref.png`/`_nat.png` que já estavam em
`<runtime>\corpus\` e um `vu1cmp -Full -Dump` de `congar_olhos` e `ultrav_corda` (cópias apagadas depois).

## Conclusão

**Não é uma escala de ~5%. É um deslocamento aditivo de ~2 níveis (de 255).** Ele vem das etapas inteiras do GS, que **truncam**,
enquanto o shader do nativo faz as mesmas contas em float e só arredonda na escrita de 8 bits. Num quadro escuro (a maior parte dos
pixels da luta fica entre 16 e 64), 2 níveis dão a razão de 0,92 a 0,97 que a pista registrou.

As etapas, com o viés medido num modelo (`modelo.py`, 2 milhões de amostras, Tc de 0 a 255, Cv de 30 a 128):

| etapa do GS (gs_cpu_backend.cpp do runtime, igual às fórmulas do manual do GS) | nativo (shader do `Scene::draw`) | viés acumulado nat − ref |
|---|---|---|
| modulação `(Ct·Cv) >> 7` | `texel · Cv · 1,9922`, float | +0,49 |
| cor de vértice Gouraud truncada a inteiro por pixel (`clampU8(int(...))`) | interpolada em float | +0,99 |
| neblina `(F·C) >> 8 + ((255 − F)·Cf) >> 8`: com F = 255 dá **C − 1** para qualquer C ≥ 1 | `mix(fogColor, c, F/255)`: com F = 255 dá C exato | **+1,98 (moda +2, 64%)** |
| mistura comum `((A − B)·C) >> 7 + D` (o `>>` aritmético arredonda o negativo para baixo) | blend do GL, exato | +2,47 com As = 131 (textura 0xFF, vértice 0x42) |

## Evidência

1. **É aditivo, não multiplicativo.** Mediana de nat − ref por canal, nos pixels com |dif| < 24, em todo o corpus menos a tela dividida
   (`dif.py`): **+3 em todas as faixas de brilho** (4–16, 16–32, …, 128–192), e a moda é +2 em quase todo quadro (`ultrav_corda` 52%
   exatamente +2, `luta_3ia` 29%, `robo47_flare_central` 29%, `dragon_vermelho` 26%). O ajuste ref = a·nat + b dá a ≈ 0,96–1,00 e
   b ≈ −2 a −3 (`airport_ceu` 0,995/−2,43; `destrocos_683` 0,980/−2,95; `robo47_flare_central` 0,995/−1,89). Por faixa, a razão ref/nat
   sobe com o brilho: 0,90 em 12–32, 0,93 em 32–64, 0,97 em 64–96 e 0,98–0,99 acima de 128. É o que um deslocamento fixo produz.
2. **Bate com o modelo.** Por classe de triângulo (`classes.py`: rasteriza os triângulos do VU1 do dump na ordem dos programas e compara
   os pixels de cada dono), no `ultrav_corda`:
   - opaco (ABE 0), Gouraud e com neblina: média +1,94, **moda +2 (51%)** (modelo, etapa 3: +1,98, moda +2 com 64%);
   - domo do céu (`10e7f80`: T8 com CLUT CT32, vértice 0x80 plano, ALPHA 0x44, Av 0x42): o pixel é chapado, (49, 14, 29) no nativo e
     (47, 12, 27) na referência, **−2 exato nos três canais, qualquer que seja o valor** (`pares.py`, 57 mil pixels). Com Cv = 0x80 a
     modulação é exata. A neblina com F = 255 tira 1. Como o As fica abaixo de 1, a mistura realimentada pelo quadro anterior para em
     Cs − 1 (`(Cs − x)·As < 128`). Modelo, etapa 5: moda +2 em 99%.
   - Todas as CLUTs do quadro são CT32 (`cpsm0`), então a expansão de 5 bits do CT16 não entra (ver descartados).
3. **Vértice, textura e neblina são os mesmos dos dois lados.** No `vu1cmp` do `congar_olhos`: cor difere 40/9.694, F difere 0, FGE
   difere 0, texels iguais 122/148. A diferença nasce na rasterização, não nos dados.

## Por que o `congar_olhos` dá 1,0

Ele tem o mesmo +2 de todo o corpus, mas três coisas locais deixam o nativo **mais escuro** e o anulam na mediana (`mapa.py` gera
`congar_olhos_13705_sinal.png`: verde para nativo mais claro, vermelho para mais escuro; o vermelho cobre o chão perto da câmera):

| dono (pkt) | o que é | nat → ref (média) | ajuste ref = a·nat + b |
|---|---|---|---|
| `1095e80` (triângulos de tbp 3e60, ALPHA 0x42 = Cd − Cs·As) | sombra projetada do Congar (subtrativa) no chão | 43,1 → 45,1 (nat −2; moda −4) | 0,96 / +3,7 |
| `112db80` (tex 78, flags 2310, Av 0x42, ALPHA 0x44, sem nada por baixo no quadro) | chão perto da câmera | 17,2 → 24,8 (nat ~30% mais escuro) | 1,32 / +2,2 |
| `11c75d0` (tex 331, skinned, Av 0x80) | o Congar skin 2 | 44,2 → 47,5 | 0,90 / +7,9 |

Sem a sombra, o resto do quadro já fica com mediana +1 (média +0,15). O 1,0 é coincidência de sinais. O quadro não está certo.
- A sombra mais escura no nativo já tinha sido notada (`VU1CHECK.md`, sombra do Kineticlops: o fudge escurecia a do Congar).
- O `112db80` tem As ≈ 0x42/0x80 ≈ 0,52 sobre um destino que o nativo limpa de preto. Na referência do replay, esse destino é o
  quadro anterior. Pode ser artefato do replay ou o caso do "chão mais próximo" da lista de falsos alarmes. Fica como pista à parte.
- O Congar mais escuro no VEGAS (cor de vértice igual, ver o item 3 da evidência) também fica como pista à parte.

## Descartados

- **Conversão 0x80 = 1,0:** o shader usa 1,9922 ≈ 255/128 = 1,99219 (erro de 6·10⁻⁶). O texto usa 2,0 (+0,4%), desprezível.
- **Paleta/CT16:** o nativo expande 5 bits como `x·255/31` (`gs::from16`) e o GS como `x << 3`, o que daria até +7 (2,8%). Mas
  nenhum objeto dos quadros medidos usa CLUT ou textura CT16: todos `cpsm0`, com PSM T8, T4 ou CT32. Continua como diferença real
  para quem usar CT16, só que não é esta pista. Se aparecer, o certo é `x << 3`, como o GS.
- **Filtro da textura:** o bilinear do GS em software arredonda (`lround`) e o do GL também. Nenhum dos dois puxa para um lado.
- **Gama/sRGB:** o raylib não liga `GL_FRAMEBUFFER_SRGB`. A diferença não depende do brilho como uma curva de gama dependeria
  (é +2 de 14 a 49 no céu).
- **Alfa da textura/As > 1:** o As > 1 já sai exato (três passes). A truncagem da mistura soma +0,5 (modelo 4), mas não explica o +2
  das superfícies opacas.
- **Apresentação (PMODE):** já descartada na pista. O framebuffer é CT32 (FBP 0x46/0x8C, 640×224), sem os 5 bits do CT16.
- **Cor de vértice/luz:** iguais (item 3 da evidência). O nativo trunca no `uint8_t`, como o FTOI0.

## O PS2 de verdade

A referência é o GS em software do runtime, mas as fórmulas truncadas dele são as do manual do GS: `>> 7` na modulação e na
mistura, `>> 8` na neblina, cor do DDA sem a fração. O PS2 deve, portanto, ser tão escuro quanto a referência, e quem está mais claro
é o port. Não conferi no PCSX2. Uma conferência barata: no VEGAS, o céu chapado da `ultrav_corda` deve sair (47, 12, 27) e não
(49, 14, 29). A neblina com F = 255 é o ponto menos certo (o PCSX2 HW não tira 1 ali, se me lembro bem), e é ela que dá 1 dos 2
níveis.

## Correção proposta (não aplicada)

No fragment shader do `Scene::beginShader` (`native/wotm_native.hpp`, ~linha 2303), reproduzir as etapas inteiras no modo GS. Atrás
de uma variável `WOTM_GSTRUNC` (nova; entra na tabela do `NATIVO.md`), passada como uniform para poder comparar antes e depois:

```diff
-                "uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 fogColor; uniform float fogSplit; uniform float alphaRef; out vec4 finalColor;\n"
+                "uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 fogColor; uniform float fogSplit; uniform float alphaRef; uniform float gsTrunc; out vec4 finalColor;\n"
                 "void main() { vec4 t = texture(texture0, fragTexCoord) * fragColor * colDiffuse;\n"
                 "  float as = t.a;\n"
-                "  if (fragGsA > 0.5) { t.rgb = min(t.rgb * 1.9922, vec3(1.0)); as = min(t.a * 3.9690, 1.9922); t.a = min(as, 1.0); }\n"
+                "  if (fragGsA > 0.5 && gsTrunc > 0.5) {\n"
+                // GS: texel inteiro (o bilinear arredonda), cor do DDA truncada, (Ct*Cv)>>7 e (At*Av)>>7 limitados a 255
+                "    vec4 tx = floor(texture(texture0, fragTexCoord) * 255.0 + 0.5);\n"
+                "    vec4 cv = floor(fragColor * colDiffuse * 255.0 + 0.001);\n"
+                "    t.rgb = min(floor(tx.rgb * cv.rgb / 128.0), 255.0) / 255.0;\n"
+                "    as = min(floor(tx.a * cv.a / 128.0), 255.0) / 128.0; t.a = min(as, 1.0);\n"
+                "  } else if (fragGsA > 0.5) { t.rgb = min(t.rgb * 1.9922, vec3(1.0)); as = min(t.a * 3.9690, 1.9922); t.a = min(as, 1.0); }\n"
 ...
                 "  float f = gl_FragCoord.x < fogSplit ? 1.0 : fragFog;\n"
-                "  vec3 c = mix(fogColor, t.rgb, f);\n"
+                "  vec3 c = mix(fogColor, t.rgb, f);\n"
+                // GS: F interpolado truncado; (F*C)>>8 + ((255-F)*Cf)>>8 (com F = 255 sai C - 1)
+                "  if (fragGsA > 0.5 && gsTrunc > 0.5 && f < 0.99999 + step(fogSplit, gl_FragCoord.x)) {\n"
+                "    float F = floor(f * 255.0 + 0.001);\n"
+                "    c = (floor(F * floor(t.rgb * 255.0 + 0.5) / 256.0) + floor((255.0 - F) * floor(fogColor * 255.0 + 0.5) / 256.0)) / 255.0;\n"
+                "  }\n"
```

(A condição da neblina tem de seguir a do vertex shader: só com FGE. Hoje `fragFog` vale 1,0 sem FGE, e com FGE e F = 255 também
vale 1,0. Para distinguir os dois casos, passe o FGE num varying próprio, por exemplo `fragFge = mod(fl, 2.0)`, e use
`fragFge > 0.5` no lugar do teste com `f`.) Mais um `gsTruncLoc_` com `GetShaderLocation`, ligado uma vez por
`std::getenv("WOTM_GSTRUNC")`.

- Os valores saem inteiros/255, então a escrita de 8 bits do GL não arredonda mais nada.
- As partículas usam o mesmo shader (`shaderNormal(..., gsA=true)`) e ganham o mesmo tratamento.
- Fica de fora a truncagem da mistura `>> 7` (+0,5 nas camadas misturadas), que o blend fixo do GL não faz. Dá para imitar com um
  viés de −0,5/255 no Cs dos passes com ABE, se a medida pedir.
- Custo: algumas instruções por fragmento. Os modos 1 a 5 (doubled, As > 1) leem `as` e `t` depois disso e continuam valendo.

## Como validar

1. `tools\corpus.ps1 -Check` antes (a base).
2. Aplicar o diff, `build_runtime.bat` e `$env:WOTM_GSTRUNC=1; tools\corpus.ps1 -Check -Baseline <csv da base>`. Nada deve piorar na
   geometria, e os `pixels diferentes` (limiar 24) quase não mudam.
3. Rodar `python dif.py` e `python razao.py` (pasta de rascunho acima; vale levar para `tools/` se a correção entrar) nos PNGs novos.
   Esperado: moda de nat − ref de +2 para 0, mediana de +3 para ~+1 (sobra a mistura) e razão ref/nat de 0,92–0,97 para ~0,98–1,0
   nas faixas escuras. Pela classe opaca da `ultrav_corda` (`classes.py` com um `-Dump` do `vu1cmp`), a moda deve ir para 0. O céu
   deve ficar em −1, porque a realimentação do As < 1 não é imitada.
4. Conferir no PCSX2 um pixel chapado (o céu do VEGAS) antes de ligar por padrão. Se o PCSX2 não tirar 1 na neblina com F = 255,
   manter só a modulação e o Gouraud (viés +1 em vez de +2).

## Aplicado e conferido (2026-10-19, mesmo dia)

- O diff acima entrou como `WOTM_GSTRUNC=1`, desligado por padrão e documentado no `NATIVO.md`. O FGE vai num varying próprio
  (`fragFge`).
- Corpus com `WOTM_GSTRUNC=1` contra `resumo_2026-10-19_152801.csv`: a geometria não muda. Os pixels diferentes (limiar 24) ficam
  iguais, exceto no `congar_olhos`, que piora de 50.150 para 53.267: os três pontos locais mais escuros, que antes se anulavam com o
  +2, deixam de ser compensados. O céu chapado da `ultrav_corda` vai de (49, 14, 29) para **(48, 13, 28)**. A referência dá
  (47, 12, 27), e o nível que sobra é a realimentação da mistura, como previsto.
- PCSX2, renderizador Software, numa luta no VEGAS (cena e câmera diferentes das do corpus,
  `docs/screenshots/2026-10-19/2026-10-19_pcsx2_sw_vegas_ceu.png`): o tom base do céu sai **(49, 13, 28)**, com G e B iguais ao
  `GSTRUNC` e R igual ao nativo em float. O renderizador de hardware dá (48, 13, 28), mas em 4x e sem a neblina do GS.
  - Teste com a neblina no formato do PCSX2 Software (`Cf + ((C − Cf)·F) >> 8`), que com F = 255 deixa o canal sem perda onde C ≤ Cf:
    o céu continuou em (48, 13, 28). A diferença no R não vem da forma da neblina, e o teste foi retirado.
  - Conclusão: o PS2 emulado fica **cerca de 1 nível abaixo do nativo em float**, entre o nativo e a referência. Como o quadro não é o
    mesmo, o R pode ser outro texel ou outro destino da mistura. Para fechar, falta comparar o mesmo quadro (um GS dump do PCSX2 ou um
    savestate na cena do corpus).
- Fica desligado por padrão até essa conferência.

## O que muda na pista

- A orientação "escale o nativo pelo fator do quadro" deve virar **"subtraia ~2 do nativo"** (ou ligue `WOTM_GSTRUNC`). A escala
  corrige os escuros e erra os claros.
- Itens novos (separados): sombra do Congar mais escura no VEGAS (`1095e80`); chão perto da câmera com As ≈ 0,52 sobre destino vazio
  (`112db80`, `congar_olhos`); Congar skin 2 ~7% mais escuro no VEGAS (`11c75d0`) com cor de vértice igual.
- A tela dividida (`split_*`) e os quadros de fade do `menus` não servem para essa medida: as vistas diferem e a referência é preta.
