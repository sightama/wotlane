# Explosão e Assboss: ordem ou mistura (bug 5 dos "Bugs vistos jogando")

Foto `2026-10-18_025733`/002 (CANYON2, `g_frame` 11872; recursos `CANYON2 ROBOT1 MANTIS1 x3 ASSBOSS1`). Nota do dono: "a textura do assboss parece passar por cima da explosão".
Investigação só pelo código e pelo instantâneo (sem jogo, sem PCSX2). Rodados: `tools\vu1cmp.ps1` numa **cópia** da foto (com `WOTM_LISTITEMS`, `WOTM_ALPHAORDER`, `-Dump`, `WOTM_VU1CMP_RAM`); scripts de análise na pasta de rascunho do agente (ordem dos programas por pacote, cores médias, pais de cada nó com o ctx do GS decodificado). Nada no repo nem no runtime foi alterado.

## Conclusão curta

**A ordem, o teste de Z e a mistura do nativo são os mesmos da cadeia do retail neste quadro.** A cadeia DMA que o próprio jogo montou (a do instantâneo) desenha as cascas da explosão **antes** da segunda camada do Assboss, e essa camada usa mistura comum (As ≈ 0,52) por cima do que já está na tela. Assim, sobre a superfície do chefe, o brilho aditivo da explosão cai pela metade **também no PS2**. O que o dono viu é, muito provavelmente, o comportamento original. A referência deste quadro reforça a impressão errada: nela a segunda camada do chefe sai com uma textura laranja (lixo da VRAM) e parece ser a explosão cobrindo o chefe.

Confiança: **alta** de que a ordem, o estado do GS e a cor de vértice batem com o retail. **Média** de que, no PCSX2, a tela fica igual (resta conferir a textura da explosão e o brilho das luzes pontuais; ver "O que pode ainda diferir").

## Quem é quem no quadro

`WOTM_LISTITEMS`/`WOTM_ALPHAORDER` no replay, mais o ctx dos pais lido da RAM do quadro:

| Peça | Nós | Onde sai | Estado do GS herdado |
|---|---|---|---|
| Explosão: 8 cascas `tex 139` (128 tris cada) e 2 `tex 147` (48), centro (−62, −270, 96) | `dafc00`…`db76a0`, `db8a70`, `db9100` (flags `0x8200`) | OT de alfa, faixas 64 a 68 | ctx de um ANIM_XFORM (`db0c80`, `db8fe0`): `ALPHA_1` 0x48 (Cs·As + Cd, aditiva), `TEST_1` 0x5040C (ATE 0, **ZTE 1, ZTST GEQUAL**), `ZBUF_1` ZMSK 1 (testa Z, não grava) |
| Assboss, camada base `tex 194` (T8 64x64, opaca), skinned `11904f0` (5.732 tris) e partes rígidas | flags `0x2313` | fora da OT, na ordem da travessia | sem ctx próprio: opaca, grava Z |
| Assboss, 2ª camada `tex 191/192/193` (CT32 256x256, **TCC 0**, alfa de vértice 0x42) sobre a mesma malha, `zBufferFudge` 1,0003 | `1166f20` (skinned) e `1138fc0`…`11616b0` (flags `0xA311`) | **OT de alfa**, faixas 31 a 54 (mais perto que a explosão) | GROUP `11904c0`/`115ab30`: `ALPHA_1` 0x44 (mistura comum), `TEST_1` 0x5040C (ZTE 1, GEQUAL), ZMSK 1 |
| Robo-47 `tex 146/142`, (−54, −325) | `ca30d0`… | fora da OT | opaco |

(Os `texId` são da sessão. No `NATIVO.md`, "esfera da explosão (tex 146)" é de outra foto.)

## Evidência

1. **Ordem do retail = ordem do nativo.** No dump (`-Dump`, coluna do índice do programa), a cadeia do jogo executa a base do chefe (`1190590`, programas 388–544, mais o resto dela em 545–597), depois o Robo-47 (598–709). Na OT de alfa vêm as cascas da explosão (`db7780`…`dafd00`, programas 961–1007, e `tex 147` em 946–950) e só depois a 2ª camada do chefe: `1147d80` 1033, `113cf80` 1038, `1142d80` 1042, …, `1167020` 1085–1243, …, `114fa80` 1359. O `WOTM_ALPHAORDER` do nativo dá a mesma sequência: explosão nas faixas 64–68 e, depois, `1147ce0` (54), `113ce90` (53), `1142cb0` (52), … e `1166f20` (48). O nativo segue isso em `native/wotm_native.hpp:2162-2168` (travessia, depois a OT de alfa), `:2230-2245` (`inAlphaOT`, `alphaBin`) e `:2333-2345` (faixa mais longe primeiro, LIFO).
2. **O Z é o mesmo.** Explosão e 2ª camada têm ZTE 1 com GEQUAL e ZMSK 1; só a base grava Z. O nativo deixa o teste de profundidade ligado na travessia e na OT (`wotm_native.hpp:2157`, `:2165`) e aplica o ZMSK item a item (`:2444`). O `groupState` (`:2834-2849`) não lê ZTE/ZTST do `TEST_1`, mas aqui eles são o padrão (GEQUAL), então isso não pesa. (Para o futuro: um ctx com ZTST ALWAYS seria ignorado pelo nativo.)
3. **A mistura é a mesma.** Linha `atributos`: `ABE difere 0`, `ALPHA difere 0`, `TFX/TCC fora do padrao 0`, alfa difere 22 de 22.856. Com TCC 0 o nativo põe o texel em 0x80 (`wotm_native.hpp:809-819`), e o As da 2ª camada sai 3,969·(0x80/255)·(0x42/255) ≈ 0,516, igual ao GS (0x42/128).
4. **A cor de vértice é a mesma em média** (RGB médio por pacote, VU1 contra nativo): base `1190590` 68/57/57 nos dois lados; 2ª camada `1167020` 90/51/65 contra 88/51/65; explosão `dafd00` 202/80/26 contra 195/77/26. Há vértices isolados da 2ª camada com cor diferente (45 de 495 no `1157000`: VU1 `22131a` contra nativo `8a5879`), provavelmente das luzes pontuais da explosão. É local; ver abaixo.
5. **Cálculo que explica a impressão:** onde o chefe está atrás da frente da casca, o GS faz `base + fogo·As_fogo` e depois `0,52·camada + 0,48·(base + fogo)`. O fogo sobre o chefe cai para menos da metade, e a 2ª camada (textura de sujeira escura, `ff191a17`, `ff2b2b2b` no nativo) "passa por cima". É o resultado esperado da ordem do retail, não um defeito de ordem do port.

## O que pode ainda diferir do PS2 (em ordem de suspeita)

- **Textura e UV da explosão:** o `corpus\explosao.txt` já anota que "o UV da textura aparece maior que no PS2" (baixa prioridade). Se o fogo é mais espalhado e menos denso no nativo, a explosão parece mais fraca sobre o chefe. Aqui as cascas `tex 139` casam com `uv 0`, então isso valeria para outra peça da explosão.
- **Luzes pontuais na 2ª camada:** os vértices com cor diferente (até 4x mais claros no nativo) clareiam a camada que cobre o fogo. É pequeno no quadro (≤ 10% dos vértices dos pacotes listados).
- **Partícula que só o VU1 desenha:** `fx 98`, tipo 11, preset 53, 2 tris no VU1 e 0 no nativo. É pequena e não explica a cena.

## Bônus: por que a referência deste quadro sai quebrada (sim, é o mesmo caso do `goliath_flare`)

O `goliath_flare` é o mesmo chefe na mesma fase (CANYON2; camada base `tex 187` com flags `0x2313` e 2ª camada `tex 184` com flags `0xA311`, CT32 256x256 em `tbp 325f`). Os dois quadros têm o mesmo padrão:

1. **Os ~2.900 tris "sem dono" são o fim das duas malhas skinned do chefe.** Base: VU1 com 4.240 tris contra 5.723 no nativo. 2ª camada: 4.296 contra 5.723. A soma do que falta (~2.910) bate com os 2.900 órfãos. As MSCAL deles estão em `0x1187200`–`0x11b9100`, nos buffers do próprio modelo e fora do intervalo do `pkt`, e por isso a checagem não as atribui a um objeto. No `goliath_flare` são 4.111 e 4.211 de ~5.690, com 2.890 órfãos. O desenho existe e só a atribuição falha.
2. **A VRAM do instantâneo não tem as texturas da 2ª camada.** A linha `texels` dá 100% dos texels diferentes para `tex 191/192/193` (`tbp 325e/365e/3a5e`, CT32 256x256): VRAM `00000000`, `000660bd` e `00041b00` (alfa 0, tons de laranja) contra o nativo `ff191a17`, `ff2b2b2b` e `ff1d1816`. A referência pinta a 2ª camada do chefe com esse conteúdo laranja, e a imagem parece "explosão por cima do chefe". É a pendência aberta "Texels: referência com outra textura na VRAM" (`VU1CHECK.md`, "Em aberto"): texturas grandes do chefe que a VRAM gravada não tem (upload fora do que o instantâneo pega, ou outra coisa por cima).

## Próximo passo concreto

1. **PCSX2 (o dono):** CANYON2 contra o Assboss, provocar uma explosão grande junto dele e pausar com o fogo na frente do corpo. Conferir:
   - se o corpo do chefe dentro da bola de fogo fica escurecido/texturizado por cima do fogo (como no nativo) ou se o fogo brilha por cima dele;
   - com o GS debugger ou um GS dump (Tools → "GS Dump"), a ordem dos draws: as cascas da explosão (aditivas, `ALPHA` 0x48) devem vir antes da 2ª camada do chefe (`ALPHA` 0x44, TEX0 em `tbp 325e`, CT32, TCC 0). Se vierem antes, o nativo está certo;
   - a textura em `tbp 0x325e` no PCSX2 (escura e suja, como no nativo, ou laranja como na VRAM do instantâneo). Isso também fecha o bônus.
2. **Nativo, A/B sem jogo:** refazer o replay da foto com `WOTM_ONLYTEX=-191,192,193`, que tira a 2ª camada. Se a cena ficar igual à do PCSX2, sobra só o peso dessa camada (cor/luz). Se o PCSX2 mostrar o fogo forte por cima com a camada presente, investigar a cor de vértice dela perto da explosão (`objectPointLights`, os vértices `22131a` contra `8a5879`).
3. Só se o PCSX2 discordar da ordem: comparar o `alphaBin` do nativo com o `pktLinkAlphaObjAsm` para nós skinned. Neste quadro já bate com a cadeia, então isso é improvável.
