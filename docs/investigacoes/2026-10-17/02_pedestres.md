# Pedestres da METRO: parados no nascimento e de cabeça para baixo

Investigação de 2026-10-17 sobre o bug 1 de "Em aberto" (`docs/passos/VU1CHECK.md`), foto `<runtime>\fotos\2026-10-17_004304\foto_002` (METRO, MANTIS0, `g_frame` 2951). Nada foi editado nem compilado. A RAM foi lida do próprio `.vu1snap`, aplicando o registro `A` e as deltas de cada cadeia. A checagem foi refeita a partir de uma cópia do instantâneo com `tools\vu1cmp.ps1 -Dump -NoPng`.

**Aplicado em 2026-10-18:** seção 2 pelo caminho mínimo (gerador do WSL emite `FPU_DIV_S`/`FPU_RSQRT_S`; `ps2_fpu_rsqrt` no patch; 394 arquivos do `runner/` regenerados, só as 746 linhas de `div.s` e 134 de `rsqrt.s` mudaram; as macros do VU0 continuam sem saturar) e seção 3 como proposto. Corpus sem piora; luta na METRO sem NaN e com o grupo visível andando. Falta conferir ao vivo.

## Resumo

São **dois defeitos independentes**:

| Sintoma | Onde está | Certeza |
|---|---|---|
| Todos no ponto de nascimento, parados e sem animação | **Lógica do jogo no runtime, não no desenho.** No `PedGroup::update` recompilado, uma divisão por zero dá ±Inf, que vira NaN, e os pedestres que já passaram pelo update somem. O PS2 satura em ±FLT_MAX e não produz NaN. | Alta para a causa (Inf/NaN do `div.s`/`rsqrt.s` gerado sem saturação). Média-alta para o ponto exato que dispara (`div.s` em 0x135968 com o vizinho igual a ele mesmo). |
| Espelhados na vertical | **Camada nativa**, `native/wotm_fx.hpp:1216-1217`: o V do carimbo está invertido em relação ao VU1. | Alta (UV do VU1 medido no dump) |

O "99,8% casados" não pegava nenhum dos dois. A posição está igual nos dois lados, porque o VU1 desenha o mesmo estado podre da RAM. A checagem de partículas compara só os cantos na tela (até 1 px), não o UV.

## 1. Como o jogo desenha os pedestres

- **`PedGroup`** (0x190 bytes; `_8PedGroup$s_allPedGroups` 0x7CFFB0, `$s_numPedGroups` 0x6F7B00; METRO: 8 grupos de 125 pedestres). O construtor `__8PedGroupUcR8_fvector` (0x134C18) chama:
  1. `pedsCreate` (0x21BD88), que cria um `_ParticleType` do tipo visual 14 e o ponteiro em `+0x164`. As posições ficam em `+0x168` (o próprio `pos[]` da partícula) e os dados de caminho, 0x30 por pedestre, em `+0x16C`.
  2. `PathNet::getClosestNode` (`_7PathNet$ped` 0x7D0000), e depois `initPath` (0x134E88).
  3. `scatter` (0x134F88). As posições ficam no segmento do caminho. `pos.w` = float(variante) (`idx % vis.numColors`) com a meia-palavra baixa = quadro da animação. Velocidade e aceleração (`+0x00` e `+0x10` do dado de caminho) começam em `vf0` = (0,0,0,1).
  4. A tarefa `update__8PedGroupPv` vai para o `TaskManager`.
- **Pacote:** `stampPacketInit` (0x21BA48) monta o pacote dentro do `fx[i]`, com GIF tag, cor, `info.minSize` (largura), `info.maxSize` (altura), 500,0 e 1/quadros, 1/variantes e o meio texel em U e V (`0.5/(du>>4)`, `0.5/(dv>>4)` do `TextureInfo`). O microprograma do VU1 (prog 0x1373 no dump) expande cada pedestre em 2 triângulos: um carimbo em pé, virado para a câmera. Textura `PedestrianTex` (texInfo 30, 128×128, `TEX0 20037985dd30b7ff`), com 8 quadros em U e 8 variantes em V.
- **Animação e movimento** (`update__8PedGroup`, 0x135738; só roda com `isVisible` e `moviePlaying == 0`):
  - Uma janela de 8 pedestres por chamada (`+0x180`/`+0x184`) recebe a física: vizinhos por `findInRange`, separação, seguir o caminho, velocidade e posição.
  - Outra janela (`+0x188`/`+0x18C`) anda o quadro da animação: `(quadro+1)&7` se `|v|² > (MAX_SPEED*0,15)²`, senão fica no quadro 6, que é a pose parada.
  - Depois vêm o `sortIndex` e o `pedsUpdateFOVbox`, que recalcula a caixa em `fx+0x2C80`.

## 2. Parados no nascimento: NaN no `PedGroup::update`

### Evidência (RAM no fim do quadro 2951)

| grupo | sistema `fx` | posições finitas | quadros (meia-palavra baixa) | velocidade dos finitos |
|---|---|---|---|---|
| 0, 1, 2, 4, 5 | 98…94 (5 = fx 93, o bolo da foto) | 62/125 | 4–7 | (0,0,0,0) |
| 3 | | 109/125 | 0, 6 | |
| 6, 7 | fx 92, 91 (longe, W 1.406 e 1.665) | 125/125 | 0 | (0,0,0,1), nunca atualizados |

- Nos grupos 0–5, os pedestres 0–62, que já passaram pela janela da física, estão com **`pos` = NaN** e o dado de caminho = `(NaN, NaN, NaN, 3,4028235e38)` / `(~1,7e38, ~1,7e38, 0, 1)`. O valor 3,4028235e38 (0x7F7FFFFF) é o `ps2_fpu_fix` do runtime convertendo um ±Inf que passou por `FPU_MUL_S`/`FPU_ADD_S`. Os números perto de 1,7e38 são FLT_MAX/2. Ou seja, **houve um infinito**, e no caminho pelos registradores do VU0 ele virou NaN.
- Os pedestres 63–124 continuam finitos, todos dentro de um retângulo de ~5 × 1 unidade (o espalhamento do `scatter`), com velocidade 0 e o quadro 6 (pose parada). Ainda não chegaram à janela da física e, quando chegarem, viram NaN também.
- O nativo pula as posições não finitas (`wotm_fx.hpp:1206`) e o VU1 também não as mostra. Na tela sobram **só os que ainda não andaram, todos no ponto de nascimento, na pose parada**: é exatamente o sintoma. A imagem de referência do runtime (GS em software, `foto_002_2951_ref.png`) mostra o mesmo bolo.
- `moviePlaying` = 0 e `pedsTurnedOff` = 0 (não é o gate do update). No PCSX2 os pedestres andam (`docs/passos/TESTE_PCSX2.md`: andavam "em fila" só por causa do `PathNode` das equivalentes).

### Causa no código gerado

O runtime do `build_dev` é o `PS2Recomp` local (base 2c5fbb9 mais o patch do repo), e os `.cpp` gerados em `PS2Recomp/ps2xRuntime/src/runner/` são de 9 de outubro.

- `div.s` é emitido em IEEE puro (`ps2xRecomp/src/lib/fpu_translator.cpp:56`): `if (ft == 0) fd = copysignf(INFINITY, fs*0) else fd = fs/ft`. O resultado é ±Inf, e 0/0 também dá +Inf. Ele **não** usa o `FPU_DIV_S` → `ps2_fpu_div` que o `wotm-runtime.patch` já define e que satura em ±FLT_MAX como o EE.
- `rsqrt.s` sai como `fs / sqrtf(fabsf(ft))`, também sem saturação: ft = 0 dá ±Inf, e com fs = 0 dá NaN.
- As operações de macro do VU0 (`PS2_VADD`/`PS2_VMUL`, `ps2_runtime_macros.h:144-148`) são `_mm_add_ps`/`_mm_mul_ps` cruas. 0 × Inf = NaN, enquanto no PS2 0 × FLT_MAX = 0.

**Ponto exato (o mais provável):** `update__8PedGroup_0x135738.cpp`, laço dos vizinhos (0x135910–0x135988).

1. Para cada índice devolvido pelo `findInRange`, o código calcula `d = pos[vizinho] − pos[eu]` (z = 0) e `f2 = dx² + dy²`.
2. Se `f2 < NEIGHBOR_RANGE²`, faz `f0 = −1/f2` (o `div.s` em 0x135968), e depois `sep += d * f0` por `vaddax`/`vmaddx`, no VU0.
3. Se o próprio pedestre volta na lista, ou se dois coincidem, `d = 0` e `f2 = 0`. No PS2 isso dá `f0 = −FLT_MAX` e `0 × −FLT_MAX = 0`, e nada acontece. No runtime dá `f0 = −Inf`, `0 × −Inf = NaN`, e o `sep` vira NaN.
4. Daí o NaN passa para a aceleração (`+0x10`), a velocidade (`+0x00`) e a posição. O `rsqrt.s` da normalização logo abaixo (0x135a6c: `|sep|² = 0` → Inf → `vmaddx` 0 × Inf) é um segundo gatilho do mesmo tipo.

Não confirmei qual dos dois dispara primeiro, porque isso exige rodar o update. A classe do defeito está confirmada pelos valores na RAM (FLT_MAX do `fix`, NaN do VU0).

**Alcance:** o defeito não é só dos pedestres. No `runner/` há 746 `div.s` com `copysignf(INFINITY…)` em 344 funções e `rsqrt.s` sem saturação em 75 arquivos. O que estiver errado em outros lugares (carros da PathNet, câmera, IA) pode ter a mesma origem.

### Como resolver

O upstream já corrigiu isso:

- **PR #272** "Emulate PS2 float semantics for EE FPU results and DIV.S/RSQRT.S" (commit `eb8688b`, 14 de outubro), presente em `<runtime>\fork-wt` (branch `fix/vu-vsqi-vsqd`).
- Lá o `div.s` emite `FPU_DIV_S` (→ `ps2_fdiv`), o `rsqrt.s` emite `FPU_RSQRT_S` (→ `ps2_frsqrt`), e o VU0 usa `ps2_vmul`/`ps2_vadd` com a mesma semântica.

Passos:

1. **Levar a correção ao PS2Recomp do build.** A opção preferível é cherry-pick do `eb8688b` no `<runtime>\PS2Recomp`, que resolve os conflitos com o `ps2_fpu_fix`/`ps2_fpu_div` do patch; o `ps2_fdiv` do upstream é mais completo e pode substituí-los. O mínimo é, em `ps2xRecomp/src/lib/fpu_translator.cpp`:
   - trocar o `COP1_S_DIV` por `fd = FPU_DIV_S(fs, ft)`, mantendo a flag DZ;
   - trocar o `RSQRT` por uma `ps2_fpu_rsqrt(fs, ft)` que devolve `±FLT_MAX` (com o sinal de fs) quando o expoente de ft é 0 e passa o resto por `ps2_fpu_fix(fs / sqrtf(fabsf(ft)))`.
   As macros do VU0 (`PS2_VADD/VSUB/VMUL/VMULQ`) também deveriam saturar, como o `ps2_vmul` do upstream. Para os pedestres, só o `div.s` e o `rsqrt.s` já bastam, porque 0 × FLT_MAX dá 0 mesmo no SSE.
2. **Regenerar o `runner/`** com o recompilador corrigido. Isso é necessário porque o texto IEEE está nos `.cpp` gerados, não em macro. O `ninja` não rastreia isso; é preciso um build limpo dos objetos do runner.
3. **Regenerar `tools/runtime_patch/wotm-runtime.patch`** (é uma costura do PS2Recomp, regra do `CLAUDE.md`).
4. **Conferir:**
   - Na METRO, ler `fx[91..99]` com `python tools/fx.py <ram> -v` em quadros seguidos (F6 / rajada). Não pode haver `nan`, e a meia-palavra baixa de `pos.w` tem que girar entre 0 e 7 nos que andam.
   - Rodar `tools\corpus.ps1 -Check -Baseline <csv>`. A mudança é global e mexe em tudo que divide por zero, então pode mudar outros resultados, idealmente para melhor.
   - Rodar `tools\lote.ps1` para ver se não aparece travamento novo.
   - O `PathNet` dos carros vale conferir junto.

## 3. Espelhados na vertical: o V do carimbo no nativo

### Evidência

No dump do `vu1cmp` (`fx93`, pedestre 63: variante 6, quadro 6; prog 0x1373), os dois triângulos do VU1 trazem:

| canto na tela | U | V |
|---|---|---|
| embaixo, à esquerda (2135,8; 2121,3) | 0,7539 | 0,7539 |
| embaixo, à direita (2154,1; 2121,6) | 0,8711 | 0,7539 |
| em cima, à direita (2154,7; 2112,2) | 0,8711 | 0,8711 |
| em cima, à esquerda (2136,3; 2111,9) | 0,7539 | 0,8711 |

Daí se tiram três coisas:

- **No VU1 o V cresce para cima.** O pé fica no V menor (`variante/8`) e a cabeça no V maior (`(variante+1)/8`), porque a folha guarda os bonecos de cabeça para baixo. O U cresce para a direita, igual ao nativo.
- Há um **recuo de meio texel** em cada borda: 0,7539 = 0,75 + 0,5/128 e 0,8711 = 0,875 − 0,5/128. São os campos `0.5/(du>>4)` e `0.5/(dv>>4)` que o `stampPacketInit` sobe no pacote.
- A coluna é o quadro (6) e a linha é a variante (6), como o nativo já faz.

O nativo (`native/wotm_fx.hpp:1213-1217`) põe `v0 + dv` nos cantos de baixo (`c[0]`, `c[1]`, na base `pos`) e `v0` nos de cima (`c[2]`, `c[3]`, em `+H`). É o contrário do VU1, e por isso os bonecos saem de cabeça para baixo. Os cantos na tela batem (por isso os 99,8%), e a checagem de partículas não compara o UV.

### Como resolver

Em `native/wotm_fx.hpp`, no bloco `FX_PEDS` (`Particles::forEachQuad`):

```cpp
// VU1 (prog 0x1373): V cresce para cima (pe em v0, cabeca em v0 + dv); meio texel de recuo do stampPacketInit
const float hu = 0.5f / texW, hv = 0.5f / texH;   // pacote: 0.5/(du>>4) da regiao do TextureInfo; aqui regiao = textura inteira
uv[0][0] = u0 + hu;      uv[0][1] = v0 + hv;        // embaixo, esquerda
uv[1][0] = u0 + du - hu; uv[1][1] = v0 + hv;        // embaixo, direita
uv[2][0] = u0 + du - hu; uv[2][1] = v0 + dv - hv;   // em cima, direita
uv[3][0] = u0 + hu;      uv[3][1] = v0 + dv - hv;   // em cima, esquerda
```

O recuo é opcional, mas é o que o VU1 faz e evita sangrar o quadro vizinho com filtro bilinear. Aqui a região do `TextureInfo` é a textura inteira (128 texels), então `0.5/(du>>4)` normalizado dá `0.5/texW`. Se aparecer folha de pedestre com região menor que a textura, conferir a fórmula com o dump.

Para conferir:
- refazer `tools\vu1cmp.ps1 <cópia de foto_002.vu1snap> -Dump <pasta>` e comparar o UV das linhas `vut` com o do nativo;
- olhar a tela com os pedestres andando, depois da correção da seção 2.

Vale também ensinar a checagem a comparar o UV das partículas (o `Tri` do VU1 já tem `u`/`v`; falta o nativo registrar o UV no `Quad`). Assim esse tipo de erro sai do inventário em vez de depender do olho.

## Arquivos e números de referência

- Instantâneo: `<runtime>\fotos\2026-10-17_004304\foto_002.vu1snap`.
- `PedGroup`: `+0x164` fx, `+0x168` pos[], `+0x16C` caminho (0x30 por pedestre: `+0x00` velocidade, `+0x10` aceleração, `+0x22`/`+0x23` bytes), `+0x17C` contagem, `+0x180..+0x18C` janelas.
- Código gerado: `<runtime>\PS2Recomp\ps2xRuntime\src\runner\update__8PedGroup_0x135738.cpp` (`div.s` nas linhas 209, 625, 941, 1045 e 1801; `rsqrt.s` nas linhas 888, 992, 1096, 1185, 1253 e 1911) e `initPath__8PedGroupPQ28PedGroup7PathDat_*.cpp` (`div.s` 0x134EE0, `rsqrt.s` 0x134F34).
- Gerador: `<runtime>\PS2Recomp\ps2xRecomp\src\lib\fpu_translator.cpp:55-79`. A versão corrigida está em `<runtime>\fork-wt\ps2xRecomp\src\lib\fpu_translator.cpp:55-80` e em `fork-wt\ps2xRuntime\include\ps2_runtime_macros.h` (`ps2_fdiv`, `ps2_frsqrt_slow`, `ps2_vmul`).
- Desenho nativo: `native/wotm_fx.hpp:1184-1222`.
- Pendência antiga que isto confirma: `docs/ANALYSIS.md:140` ("Suspeitos não verificados: `div.s` por zero").
