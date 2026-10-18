# Céu do drive-in com área preta no nativo (bug 1 dos "Bugs vistos jogando")

Foto `2026-10-17_224038`/003 (SHELL, `g_frame` 361), em `C:\Users\TwistZero\wotm-recomp-win\fotos\2026-10-17_224038\`.

## Resposta curta

- **É anterior ao 3c18a36 e não tem relação com ele.** O domo do drive-in (`tex 174`, OBJECT `a1df80`, pacote `a1e080`) não passa
  pelo `Scene::sky`: no mapa de donos ele aparece como `nat pkt a1e080 tex 174` (tipo 1, objeto comum), não como `ceu/icone pkt`
  (tipo 6, que o 3c18a36 dá a tudo o que está em `Scene::sky`; `native/runtime/wotm_vu1check.inc:2468`). Além disso, a
  `foto_003_tela.png` (janela ao vivo, gravada em 2026-10-17 às 22:43, antes do commit de 2026-10-18 01:52) já mostra as mesmas cunhas
  pretas que o replay de hoje (`foto_003_361_nat.png`, 03:33).
- **Melhor teoria (confiança alta no mecanismo, média na causa exata):** a parte de cima do domo tem alfa de vértice 0. O nativo
  descarta esses pixels no teste de alfa (`aref` 8, `as * 128 <= alphaRef` → `discard`) e aparece o preto da limpeza; o GS da
  referência desenha esses mesmos pixels. Ou o TEST_1 em vigor para o domo no GS não descarta (ATE 0, ATST ALWAYS ou AFAIL
  FB_ONLY, que o `groupState` ignora), ou o `aref` herdado pelo nativo nessa travessia do menu está errado.

## Evidência

1. **O domo cobre a cunha, mas não aparece.** Com `WOTM_IDCOLOR=2` (cor = nó, alfa 255, sem mistura) no replay da foto (cópia do
   instantâneo numa pasta de rascunho), os pixels da cunha preta, por exemplo (300,20), (200,50) e (560,40) na imagem 640x448, saem
   com o nó `a1df80` (o domo). No desenho normal eles têm valor 0 exato (a limpeza preta), com bordas retas em diagonal. Ou seja, o
   triângulo é rasterizado, mas o fragmento é jogado fora. Com abe 0 o domo não tem mistura (`native/wotm_native.hpp:2438`), então
   o único `discard` é o teste de alfa do shader (`native/wotm_native.hpp:2106`: `if (as * 128.0 <= alphaRef) discard;`).
2. **Alfa de vértice do domo por anel** (dump `-Dump` da checagem, triângulos do VU1 do pacote `a1e080`; o nativo casa 41/41):

   | v do anel | alfa do vértice | linha na tela (imagem 640x448) |
   |---|---|---|
   | -1,000 (base) | 0x00 | ~361-366 |
   | -0,850 … -0,186 | 0x42 | 107-334 |
   | **-0,022 (topo)** | **0x00** | **46-87** (x 170 a 588) |

   O anel de alfa 0 do topo está na tela, justo na altura das pontas da cunha. Acima dele (o que sobra do domo até o zênite) o
   alfa é ~0, e entre os anéis -0,186 e -0,022 ele cai de 0x42 para 0. Com TCC=1 e TFX=MODULATE (TEX0 `20038f462131201d`),
   As ≈ At·Av/128; o nativo descarta onde As·128 ≤ 8. A fronteira do descarte é uma linha de alfa constante dentro de cada triângulo
   grande: daí as bordas retas em diagonal. Embaixo, o anel de alfa 0 da base também é descartado, mas a cerca, as árvores e os
   carros cobrem a área.
3. **A referência desenha esses pixels.** Nas linhas 48-65 da `ref.png` (as únicas da cunha fora do letterbox da referência, que
   corta as linhas 0-47), o céu continua com o mesmo brilho (lum 14-22) e estrelas, sem emenda. No PS2 o domo passa ali, mesmo com
   alfa de vértice ~0.
4. **A checagem não via o problema:** `project()` só aceita triângulos inteiros na tela (`native/runtime/wotm_vu1check.inc:217`,
   `inScreen`). Os triângulos grandes do topo do domo cruzam a borda e ficam fora da comparação e do mapa de donos do nativo (o
   `nat nenhum` das linhas `inv px` de `tex 189` não quer dizer nada aqui). Resultado da foto: 99,9% casados, `TFX/TCC fora do
   padrao 56`, sem nada no domo.
5. **O `aref` do nativo é o padrão.** `WOTM_TEXINFO=174` no replay: `node a1df80 blend 68 abe 0 ... aref 8`, pos (0,0,0), fudge 1,
   burn 55555 (sem neblina). Nenhum ctx de GROUP/TRANSLATE acima dele trouxe TEST_1 (`Scene::groupState`,
   `native/wotm_native.hpp:2834`); o nativo assume o 0x8D (`native/wotm_native.hpp:392`, A > 8, AFAIL KEEP).
   O `groupState` lê ATE, ATST e AREF, mas **não lê o AFAIL** (bits 12-13): um TEST_1 com AFAIL FB_ONLY (desenha a cor, não grava Z)
   sairia no nativo como descarte.

Descartado: as placas `tex 189` (`a1fdf0`, `a20220`, quadrados de ~6.700 x 3.900 com textura preta e alfa) projetam nas linhas
119-386, atrás do telão, e não na cunha. A neblina também fica de fora (burn 55555).

## O que falta confirmar

Qual TEST_1 o GS usa ao desenhar o domo nesta tela. A checagem já guarda isso por triângulo (`t.test = g_test[ctx]`,
`native/runtime/wotm_vu1check.inc:677`), mas só imprime para as partículas (`:1080`); a linha do dump dos objetos (`:1891`) não traz o
`test`.

## Próximo passo concreto

1. **Confirmar:** acrescentar `t.test` à linha do dump por objeto (`wotm_vu1check.inc:1891`, um `%x` a mais) e rodar
   `tools\vu1cmp.ps1 <cópia de foto_003.vu1snap> -NoPng -Dump <prefixo>`; ler o TEST dos triângulos `a1e080`.
   - ATE 0 / ATST 1 → o `aref` herdado na travessia do menu está errado: procurar de onde vem esse TEST (ctx padrão do
     `ShellRtLoop`/`userintMain` diferente do `ctxGetDefaultGSCtx`, ou um ctx num nó que o `CtxScope` não olha) e herdar
     `kGsAlphaNoTest`.
   - ATE 1 com AFAIL 1 (FB_ONLY) → ensinar o `groupState` a ler o AFAIL: o fragmento que falha não é descartado, só deixa de gravar
     Z (no shader: em vez de `discard`, desenhar com a máscara de profundidade desligada, ou passar o objeto sem teste se ele não
     grava Z de qualquer forma).
2. **Teste rápido sem descobrir a causa:** forçar `aref = kGsAlphaNoTest` só no `a1df80` (diagnóstico com variável de ambiente) e
   rodar a foto: a cunha deve sumir e o `diff` do alto da tela deve zerar.
3. Depois da correção, pôr esta foto no corpus (`shell_ceu`, `g_frame` 361) e conferir ao vivo com o dono no menu principal.
   Ver se o céu escuro da AIRPORT (bug 2, `ceu_airport.md`) não é a mesma família (domo com alfa de vértice baixo e teste de alfa).

Arquivos de rascunho desta investigação (fora do repo): `C:\Users\TwistZero\.claude\jobs\ab2fc446\tmp\ag1\` (dump `d_361.csv`, a
imagem `idc_nat.png` com `WOTM_IDCOLOR=2` e os scripts). A pasta da foto não foi alterada: as imagens novas foram para uma cópia do
instantâneo.
