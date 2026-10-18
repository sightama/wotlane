# Projéteis do Plantboss explodem logo ao nascer (bug 6 dos "Bugs vistos jogando")

Fotos `2026-10-17_171318`/013, 014, 015 e 025 (3-MILE), em `C:\Users\TwistZero\wotm-recomp-win\fotos\`.

## Conclusão

**Não é colisão nem ordem de atualização. Era o `rand()` de 15 bits do runtime, que já foi corrigido depois das fotos.**
O `mathfRandf` do jogo usa os 23 bits baixos do `rand()` como mantissa. Com o `std::rand() & 0x7FFF` antigo do PS2Recomp, ele
só devolvia `[lo, lo + 0,004·(hi-lo))`, ou seja, sempre o mínimo da faixa. Para os projéteis do Plantboss isso dá:

- os 5 filhos de cada estouro saem **no mesmo ângulo** (desvio sempre -25° e -45°/-60°), um em cima do outro: parece um só, e
  quando estoura parece que "colidiram entre si";
- a vida dos netos (tipo 0x2C) fica sempre no mínimo, **6 fields (0,1 s)** em vez de 6 a 90: eles estouram logo depois de nascer;
- a vida dos filhos (0x2B) fica sempre em 62 fields, e a velocidade sempre em 200 em vez de 200 a 300.

A correção é o commit `336c38f` (2026-10-18 00:57, "runtime: rand do jogo com 31 bits", o LCG da newlib do SDK em
`PS2Recomp/ps2xRuntime/src/lib/Kernel/Stubs/LibC.cpp:1083-1098`, que está no `tools/runtime_patch/wotm-runtime.patch:742`). Ela
veio por causa dos pedestres, que nasciam todos no mesmo ponto. As fotos são de 2026-10-17 17:13, antes dela. O
`build_dev\...\ps2EntryRunner.exe` atual é mais novo que o `LibC.cpp` corrigido.

**Confiança: alta** (~90%) de que o sintoma das fotos vem daí e de que já sumiu no build atual. O resto (~10%) é conferir ao
vivo se sobra alguma coisa.

## Como o jogo faz (retail, pelo `src/` e pela tabela na RAM)

1. A cabeça no estado 18 (`src/game/PlantBoss.cpp:911-927`) lança, com 80% da animação, uma arma do tipo **0x2A**, com dono = id
   da cabeça (`m_stemCs->epNode->id2`) e alvo = jogador 1.
2. Tabela `Weapons::m_params` (`src/game/Weapon.cpp:405-422`, conferida no asm e na RAM das fotos):
   - 0x2A: vida 0,05 s = **3 fields** (de propósito: o esporo estoura na boca), velocidade 400;
   - 0x2B: vida 1,2 s = 72 fields, ±10 em `InitWeapon` (`Weapon.cpp:1418-1425`), velocidade 250 ±50, sem homing;
   - 0x2C: vida `mathfRandf(6, 90)` **fields** (`Weapon.cpp:1426-1445`), velocidade 800 ±50, homing 0,03.
3. Quando a vida acaba, `UpdateWeapons` (`Weapon.cpp:439-451`) detona 0x2A, 0x2B e 0x2C no lugar. `DetonateWeapon`
   (`Weapon.cpp:1724-1747`) cria então **5 armas do tipo seguinte** (0x2A→0x2B, 0x2B→0x2C), apontadas para o jogador com
   `r.x += mathfRandf(-25, 25)°` e `r.z += mathfRandf(-45 ou -60, +45 ou +60)°`.
4. Irmãos não colidem entre si: `CreateStaticWeapon` põe `testCollision = 0` no cs da arma (`Weapon.cpp:1027`). O
   `hdSphereTest` do `WeaponHD` pula o dono (`Weapon.cpp:808`, `902-903`), e todos os descendentes herdam o mesmo `m_ownerId`.
   Não existe máscara, tempo de imunidade nem contador entre irmãos, e não precisa: eles não se enxergam.
5. `mathfRandf` (`src/common/mathf.cpp:960-967`) faz `(rand() & 0x7FFFFF) | 0x3F800000` → `[1,2)` → `-1`. Ele precisa de 23
   bits aleatórios. O `mathfRand` inteiro usa só `& 0x7FFF` e funcionava com o rand antigo, por isso o resto do jogo parecia normal.

## Evidência (RAM dos `.vu1snap`, script `weapons.py` na pasta de rascunho)

A RAM foi localizada no instantâneo procurando 64 bytes do ELF em 0x200000. O array de armas é lido em
`game + 0x112490 + 0xC00` (stride 0x170).

- **013** (g 13530) e **014** (g 18135): 5 armas 0x2B vivas, idade 16 e 22. As 5 têm `lifetime` 62,00–62,08 (= 72 − 10 + ~0),
  velocidade 200,0–200,3 (= 250 − 50 + ~0) e posições a menos de 0,3 unidade umas das outras. Os 5 irmãos estão empilhados.
- **025** (g 20320): as mesmas 5 armas 0x2B com idade 62 e vida 62,0x. Estouram no quadro seguinte **pela vida**, todas juntas e
  no mesmo ponto. A nota "no frame seguinte já irão explodir" bate exatamente.
- **015** (g 18366): 25 armas 0x2C (5×5) com idade 1–2 e vida **6,03–6,31** (em vez de 6 a 90), velocidade 750,0–750,4, todas
  dentro de ~1,5 unidade. Elas estouram uns 4 fields depois: é o "explodem logo ao nascer".
- Nas quatro fotos, `fieldsLastFrame` = 1 e `updateRate` = 60. Não há salto de dt; o tempo do port está normal ali.

A largura observada (≤ 0,004 da faixa) é exatamente 32767 / 2²³ = 0,0039, a assinatura do rand de 15 bits.

## Teorias descartadas

- **dt/fields** (`wotm_overrides.cpp:585-611`, `788-799`): o limite do `updateRate` não toca o `timerGetFieldsLastFrame`, e
  as fotos mostram 1 field por quadro. A vida curta do 0x2A (3 fields) é do retail.
- **Colisão entre irmãos**: impossível pelo código (item 4). Não há hook em `native/` que mexa em `Weapon*`, `hdSphereTest` ou
  `testCollision` (grep vazio).
- **Ordem de atualização**: os filhos podem ser criados num slot de índice maior e atualizados no mesmo `UpdateWeapons`, mas isso
  é igual no retail e não encurta a vida.

## Próximo passo concreto

1. Com o `build_dev` atual (que já tem o `336c38f`), o dono enfrenta o Plantboss de novo (3-MILE) e confere: os 5 projéteis
   pequenos saem em leque, viajam ~1,2 s e cada um solta 5 teleguiados que vivem tempos diferentes (até 1,5 s). Se quiser números,
   uma foto F8 com eles no ar: rodando `weapons.py` no `.vu1snap`, as vidas dos 0x2C devem variar entre 6 e 90 e as velocidades
   entre 750 e 850.
2. Se confirmar, tirar o item 6 do `VU1CHECK.md` e apagar as fotos 013, 014, 015 e 025 (sobra a 005, do Magmo). Se algo ainda
   parecer cedo demais, comparar com o PCSX2: o esporo 0x2A estoura na boca de propósito (3 fields).
