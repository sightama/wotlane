# Port para macOS com partida em rede Windows x Mac: o que decide o determinismo

Pergunta do Lane: portar para os Macs dele (MacBook Neo e MacBook Pro, todos Apple Silicon) e jogar online Mac contra Windows, com
qualquer um hospedando. Decisões: build nativo arm64 com clang (SSE pelo `sse2neon.h` que o PS2Recomp já traz); nada de Rosetta
(a Apple só a mantém inteira até o macOS 27) nem Wine. Esta rodada (no PC Windows) procurou o que faz dois PCs com compilador e
CPU diferentes calcularem estados diferentes; o build do Mac e as medidas lá ficam para a sessão no Mac.

## O que é determinístico por construção

- **FPU (COP1) do jogo traduzido:** `FPU_ADD_S/SUB_S/MUL_S` (`ps2_runtime_macros.h`) passam cada operando e cada resultado pelo
  `ps2_fpu_fix`, que lê os bits do float. Nenhum compilador funde mul+add em FMA nem reordena através disso, então o `/fp:fast` do
  MSVC e o `-ffp-contract` do clang não mudam o resultado. `sqrtf`, divisão, `roundf/floorf/ceilf/nearbyintf` são exatos no IEEE.
  O `madd.s` do R5900 vira `FPU_ADD_S(acc, FPU_MUL_S(...))` (`fpu_translator.cpp`): dois arredondamentos, igual em qualquer CPU.
- **`rand`/`srand`:** stubs com o LCG do newlib dentro do runtime (`LibC.cpp`), não o `rand` do sistema.
- **Protocolo da rede:** `wotm_net_proto.hpp` escreve byte a byte em little-endian. `native/net` (servidor, cliente, testes)
  compila sem aviso com gcc 9 no WSL e os 254 testes passam: o `wotm_server` de Linux/Mac e o de Windows são intercambiáveis.

## O que dessincroniza Windows x Mac: seno e cosseno da libm do sistema

O recompilador troca por stub toda função cujo nome está na `ps2_call_list.h` do runtime (`isStubName` em
`ps2xRecomp/src/lib/ps2_recompiler.cpp`), não importa o `config.toml` (tirar os nomes de lá não muda a tradução). Neste jogo isso pega
três funções do newlib:

| Função (retail) | Stub do runtime |
|---|---|
| `__kernel_sinf` (0x230390) | `::sinf(x + y)` do sistema |
| `__kernel_cosf` (0x22F8E8) | `::cosf(x + y)` do sistema |
| `__ieee754_rem_pio2f` (0x22F3D0) | `n = nearbyintf(x * 2/pi)`, `x - n * pi/2` num arquivo com `/fp:fast`, cauda zerada |

O `sinf`/`cosf` do jogo (0x22D5C8/0x22D4C0, traduzidos) chamam essas três, e o `sinf` está na câmera, no HomingBug, nos quatérnions
(`mathfAxisAngleToQuaternion`), no `energyCrowdControl`... A libm da Microsoft (ucrtbase) e a da Apple não prometem o mesmo último
bit, e a redução pode virar FMA num compilador e não no outro: a mesma luta diverge entre Windows e Mac.

Correção: os três nomes saem da `ps2_call_list.h` (`tools/runtime_patch/wotm-runtime.patch`), e o recompilador traduz o fdlibm do
próprio jogo como o resto do código: igual ao PS2 e sem matemática escrita à mão. O asm do retail (newlib `kf_sin.c`, `kf_cos.c`,
`ef_rem_pio2.c`) é fdlibm sem `madd.s` e na ordem do fonte; para |x| > 2^7 * pi/2 ele chama o `__kernel_rem_pio2f` (Payne-Hanek),
que já é traduzido. O resultado do jogo muda um pouco em relação ao build atual (volta ao do PS2), então o id do build muda e todo
mundo reconstrói.

Conferência: `WOTM_TRIGCHECK=1` (`native/runtime/wotm_trigcheck.inc`) faz o jogo rodar o seu `sinf`/`cosf` em 85 valores e compara os
bits com o retail, rodado no unicorn com o `cvt.w.s` trocado por `trunc.w.s` (o R5900 trunca; o MIPS do unicorn arredonda). Os
valores cobrem zero, os limites de pi/4, 0,3 e 0,78125 dos núcleos, 201,06 (início do Payne-Hanek) e argumentos até 3e38. Com os
stubs a checagem tem que falhar; com o fdlibm traduzido, passar no Windows e no Mac.

Estado ao fim da rodada: o patch e a checagem estão no `wotlane/mac`, **ainda sem build**. O `build_tools` deste PC não compila: o
`CMakeCache.txt` guardou o `cl.exe` 14.32 do Visual Studio Community e o `vcvars` carrega os headers do 14.44 (`STL1001: Unexpected
compiler version`); reconfigurar o `build_tools` do zero com o 14.44.

## O que falta auditar no Mac

- **VU0 nativo (`wotm_vu0_native.cpp`):** arredondamento para zero por `_mm_setcsr` (MXCSR). No arm64 o `sse2neon` leva isso ao
  FPCR; conferir que o modo pega nas operações NEON e que o compilador não move contas para fora do trecho.
- **Interpretador do VU1** e o resto do `sse2neon`: nenhum `_mm_rcp_ps`/`_mm_rsqrt_ps`/`_mm_min_ps`/`_mm_max_ps` no `native/runtime` nem
  no VU; o `ps2_runtime_macros.h` tem MMI inteiras (sem problema de float).
- **Camada nativa que escreve na RAM do jogo** (compilada com `/fp:fast`): a câmera do wotlane para os jogadores 5 a 8 e ganchos que
  mexem no estado. Conta em float que cai na simulação precisa do mesmo conserto ou de um arquivo com FP estrito.
- **`sprintf`/`vfprintf`** são do sistema: os dois CRTs arredondam certo; risco baixo.
- **`tools/build_id.py`** fixa a versão do MSVC no id: Windows e Mac só jogam juntos depois que os hashes por quadro
  (`WOTM_STATEHASH` + `tools/statediff.py`, a mesma entrada gravada nos dois) baterem; aí o id deixa de comparar o compilador.

## Para o Linux (e o Mac) do upstream

`git show 7df1f85:docs/investigacoes/2026-10-30/linux.md` (upstream/master): sete remendos pequenos na camada nativa para compilar
com gcc. O `wotlane/4p` está 50 commits atrás do upstream e a fusão conflita em 7 arquivos (13 trechos: `wotm_split4.inc` contra a
campanha co-op, `wotm_live.inc`, `wotm_overrides.cpp`, `wotm_settings.hpp`, o patch do runtime, README, NATIVO.md): fazer a fusão antes
dos remendos, com o Lane presente.
