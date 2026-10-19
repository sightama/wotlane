# Pendências fora do caminho crítico: PR #277, oracle com chamadas, cobertura com gameplay (2026-10-19)

Investigação só de leitura (nada compilado, nada publicado no GitHub). Pendências de `docs/PROXIMOS_PASSOS.md`, "Pendências fora do caminho crítico".
Rascunhos (scripts e listas) na pasta de rascunho da sessão, fora do repo.

## 1. PR #277 no PS2Recomp (`SQRT.S`/`RSQRT.S`)

### Estado hoje
- `ran-j/PS2Recomp#277`, autor `yanm1103`, aberto em 2026-10-09, um commit (`9a7ca59`), sem atualização desde então.
- **Sem review, sem comentário, sem CI** (o repositório não roda checagem em PR: `statusCheckRollup` vazio).
- **`mergeable: CONFLICTING` (`DIRTY`)**: a `main` andou. O conflito vem do **#272** ("Emulate PS2 float semantics for EE FPU results and DIV.S/RSQRT.S",
  de `drakolordx7`, mesclado em 2026-10-14 13:47), que mexeu nas mesmas linhas do `fpu_translator.cpp`.
- O que o #272 já resolveu: `RSQRT.S` passou a sair `FPU_RSQRT_S(fs, ft)` = `ps2_frsqrt`, que faz `fs / sqrt(|ft|)` com a semântica do PS2
  (sem Inf/NaN, `ft` zero dá ±máx com o sinal de `fs`). **Metade do #277 ficou redundante.**
- O que continua errado na `main` (`eb8688b`): `COP1_S_SQRT` ainda emite `FPU_SQRT_S(ctx->f[fs])`. No R5900 o campo `fs` do `sqrt.s` é 0, então
  a raiz sai de `$f0`. O `ps2_fsqrt` do #272 já toma `|x|` no caminho lento, mas o operando continua o errado. **As 108 funções com `sqrt.s` ainda erram no upstream.**
- Histórico útil para o revisor: o **#218** ("Fix R5900 SQRT.S source register", `GTTeancum`, X-Men Legends) propunha a mesma troca `fs`→`ft`
  e foi fechado sem mesclar em 2026-09-27, sem comentário.
- Relacionados abertos que tocam no mesmo assunto, só no VU0: #198 (VDIV/VSQRT/VRSQRT), #317 (regras da FDIV no VU0), #312 (EFU do VU1).

### O projeto depende dele?
**Não.** O código recompilado que o runtime usa já sai certo:
- O tradutor que gera o C++ do jogo é o clone do WSL `~/wotm-recomp/PS2Recomp` (base `2c5fbb9`, branch `main`) com o `fpu_translator.cpp`
  **modificado e não commitado**: `SQRT.S` lê `ft` com `fabsf` e `RSQRT.S` emite `FPU_RSQRT_S(fs, ft)`. O C++ gerado em
  `wotm-recomp-win\PS2Recomp\ps2xRuntime\src\runner` confirma: 147 `FPU_SQRT_S(fabsf(ctx->f[ft]))` e 134 `FPU_RSQRT_S(fs, ft)`, nenhum na forma velha.
- O `FPU_RSQRT_S(a, b)` de dois argumentos e a semântica de float do PS2 (`ps2_fpu_fix`, `ps2_fpu_div`, `ps2_fpu_rsqrt`, `cvt.w.s` truncando)
  vêm do `tools/runtime_patch/wotm-runtime.patch` (hunk de `ps2_runtime_macros.h`). O `FPU_SQRT_S` do patch continua `sqrtf(a)`; o `fabsf` vem do tradutor.
- O clone do Windows (`wotm-recomp-win\PS2Recomp`) tem o tradutor **sem** a correção, mas lá o recompilador não é compilado (`PS2X_BUILD_RECOMP=OFF`), então não importa.
- O submódulo `third_party/PS2Recomp` (fork `yanm1103`, branch `wotm`, `31bdfe1`, base do `wotm-fmv.patch`) tem a branch do PR, com `RSQRT.S` inline (`fs / sqrtf(fabsf(ft))`).
- O `tools/recomp_oracle/README.md` descreve a correção em prosa (passo 2) e não depende do PR.

Riscos (sem urgência):
1. A correção do tradutor só existe como modificação não commitada no clone do WSL. O `~/wotm-recomp/ps2recomp_fpu_sqrt.patch` guardado ao lado
   **está desatualizado**: ainda tem o `RSQRT.S` inline, não o `FPU_RSQRT_S`. Quem recriar o clone a partir dele gera código que não usa o `ps2_fpu_rsqrt` do patch do runtime.
2. Subir a base do runtime para a `main` atual (≥ `eb8688b`) **vai conflitar** no hunk de `ps2_runtime_macros.h` do `wotm-runtime.patch`: o #272 define
   `FPU_ADD_S`…`FPU_RSQRT_S` com a semântica do PS2. Nesse rebase o certo é largar o nosso hunk (exceto a truncagem/saturação do `cvt.w.s`, que é o #215, ainda aberto)
   e manter só a troca `fs`→`ft` do `SQRT.S` no tradutor até o #277 entrar.

### O que falta para fechar
1. Rebase do `fix/fpu-sqrt-rsqrt-operands` sobre a `main` (`eb8688b`), ficando **só com o `SQRT.S`**: `FPU_SQRT_S(ctx->f[ft])`. O `fabsf` vira opcional porque o `ps2_fsqrt` já trata negativo.
   Na parte do `RSQRT.S`, ficar com a versão do #272.
2. Teste: manter o caso do `sqrt.s $f2,$f2` (`0x46020084`: não ler `f[0]`) e tirar a expectativa do `RSQRT.S` inline, que o #272 já cobre em `ps2_float_semantics_tests.cpp`.
3. Título e descrição novos ("Fix SQRT.S source operand: R5900 reads ft"), citando o #272 (que corrigiu o `RSQRT.S`) e o #218 (mesma correção, fechado sem mesclar).
   Rodar o `ps2x_tests` antes (o #272 informa 444/444).
4. Esperar o mantenedor. Ele mesclou pouca coisa recente (#274 em 10-05, #272 em 10-14) e há 168 issues/PRs abertos. Do lado do projeto não há nada bloqueado.

## 2. Oracle para funções com chamadas

### Quais são as 35
Refiz a seleção do `oracle_all.py` (equivalente, asm com VU0 ou FPU especial, assinatura que o `parse_sig` entende, `DirectCall`/`IndirectCall` no C++ gerado)
com o `config/status.csv` de 2026-10-09 (`97fcda8`, 329 equivalentes). Deu **60 candidatas (2 com assinatura não suportada), 35 não-folha**, os números do README.
**Hoje** (1011 equivalentes) são **233 candidatas (44 com assinatura não suportada) e 173 não-folha**: AiSeek 14, MovementStates 13, Monster 13, AiReflex 13, particle 11, Cameras 10, Weapon 9, HomingBug 9...

Lista das 35, com os chamados de cada uma. O estado vem do `status.csv`, e `+` marca o chamado que também chama outras funções. `jalr` é o número de chamadas
indiretas (virtuais ou por ponteiro). Spec: `p` ponteiro/this, `i` inteiro, `f` float.

| TU | função | spec | jalr | chamados |
|---|---|---|---|---|
| AiAction | `deactivate__13AiActionTupleR2Ai` | pp | 1 | `timerGetFieldCount__Fv` hw |
| AiReflex | `getEntryRelevance__16AiFireProjectileR2Ai` | pp | 0 | `getBestTarget__16AiFireProjectileR2AiRf` matched+; `getMaxLevel__12StaminaMeter` matched |
| AiReflex | `getExitRelevance__16AiFireProjectileR2Ai` | pp | 0 | `overPit__2AiR7Monster` matched; `getTargetRelevance__16AiFireProjectileR2AiP7Monster` equivalent+ |
| AiReflex | `getExitRelevance__15AiGrappleReflexR2Ai` | pp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm; `pinningAi__2Ai` matched |
| AiReflex | `getEntryRelevance__13AiBlockReflexR2Ai` | pp | 2 | `getInfo__9EnemyInfoR7MonsterT1` asm; `getReflexDelay__2Ai` matched+; `getClosestStillApproach__2AiR13DbInteractivef` asm+; `getFovRelevance__2Aiff` equivalent; `getClosestThrownPickup__12LevelPickupsR8_fvectorf` equivalent |
| AiReflex | `getExitRelevance__13AiBlockReflexR2Ai` | pp | 1 | `getReflexDelay__2Ai` matched+ |
| AiReflex | `getExitRelevance__15AiDisposeReflexR2Ai` | pp | 0 | `isHolding__7Monster` matched |
| AiReflex | `enterAction__15AiDisposeReflexR2Ai` | pp | 0 | `target__11AiNavigatorR8_fvectorf` matched; `getThrow__10AiPadClipsv` matched; `rewind__17GamePadClipPlayer` equivalent; `getReflexDelay__2Ai` matched+; `getFieldsSinceEval__13AiActionTuple` matched+ |
| AiReflex | `getEntryRelevance__13AiSmashReflexR2Ai` | pp | 0 | `isHolding__7Monster` matched; `hasEnough__12StaminaMeterf` matched+ |
| AiReflex | `getExitRelevance__13AiThrowPickupR2Ai` | pp | 0 | `pinningAi__2Ai` matched |
| AiReflex | `getBestTarget__11AiRamAttackR2AiRf` | ppp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm; `inFov__9EnemyInfof` asm; `inLos__9EnemyInfo` matched; `getMaxLevel__11HealthMeter` asm; `getCommonGroundProbability__2AiR7Monster` asm+ |
| AiReflex | `getExitRelevance__11AiRamAttackR2Ai` | pp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm; `inFov__9EnemyInfof` asm; `inLos__9EnemyInfo` matched; `pinningAi__2Ai` matched |
| AiReflex | `getExitRelevance__15AiSpecialAttackR2Ai` | pp | 1 | `getInfo__9EnemyInfoR7MonsterT1` asm; `block__2Ai` matched; `isSpecialAvailable__C7Monster` equivalent; `timerGetFieldCount__Fv` hw; `pinningAi__2Ai` matched |
| AiReflex | `getBestTarget__14AiDetonateHeadR2AiRf` | ppp | 10 | `mathfClosestApproach__FR8_fvectorN30` equivalent |
| AiSeek | `getExitRelevance__12AiSeekHealthR2Ai` | pp | 2 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__13AiSeekStaminaR2Ai` | pp | 0 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__13AiSeekSpecialR2Ai` | pp | 0 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__11AiSeekCloakR2Ai` | pp | 0 | `hasEnough__12StaminaMeterf` matched+ |
| AiSeek | `getExitRelevance__12AiSeekPickupR2Ai` | pp | 0 | `getClosestMonsterWithLos__7Monsterf` equivalent |
| AiSeek | `enterAction__12AiSeekPickupR2Ai` | pp | 1 | `setFocus__2AiP13DbInteractive` matched; `arrive__11AiNavigatorR8_fvectorfff` matched+; `getReflexDelay__2Ai` matched+ |
| AiSeek | `updateAction__12AiSeekPickupR2Ai` | pp | 1 | `__vc__8PadFlagsi` matched; `grab__2Ai` matched |
| AiSeek | `getExitRelevance__13AiSeekMonsterR2Ai` | pp | 0 | `overPit__2AiR7Monster` matched |
| AiSeek | `getExitRelevance__12AiDodgeThrowR2Ai` | pp | 3 | `isHolding__7Monster` matched; `inFlight__2AiR13DbInteractive` asm+ |
| AiSeek | `updateAction__12AiDodgeThrowR2Ai` | pp | 1 | `getInfo__9EnemyInfoR7MonsterT1` asm; `targetPin__2Ai` matched; `getClosestStillApproach__2AiR13DbInteractivef` asm+; `mathfVectorCrossUp__FP8_fvectorT0` equivalent; `tag__11AiNavigatorR8_fvectorf` equivalent+ |
| AiSeek | `getEntryRelevance__10AiDodgeRamR2Ai` | pp | 1 | `getInfo__9EnemyInfoR7MonsterT1` asm; `mathfClosestApproach__FR8_fvectorN30fRf` equivalent; `getFovRelevance__2Aiff` equivalent |
| AiSeek | `getExitRelevance__10AiDodgeRamR2Ai` | pp | 0 | `getInfo__9EnemyInfoR7MonsterT1` asm |
| AiSeek | `enterAction__10AiDodgeRamR2Ai` | pp | 0 | `setFocus__2AiP13DbInteractive` matched; `getReflexDelay__2Ai` matched+; `mathfVectorCrossUp__FP8_fvectorT0` equivalent; `tag__11AiNavigatorR8_fvectorf` equivalent+ |
| AiSeek | `enterAction__12AiDodgeStompR2Ai` | pp | 0 | `setFocus__2AiP13DbInteractive` matched; `getReflexDelay__2Ai` matched+; `getInfo__9EnemyInfoR7MonsterT1` asm; `tag__11AiNavigatorR8_fvectorf` equivalent+ |
| AiNavigator | `targetPin__11AiNavigatorR8_fvector` | pp | 0 | `orientTo__11AiNavigatorR8_fvectorf` equivalent |
| HeliVehicle | `updatePosition__11HeliVehicle` | p | 0 | `timerGetUpdateRate__Fv` hw; `resolveForcesEuler__9PointMassf` equivalent |
| Monster | `update__7Monster` | p | 1 | `timerGetFieldsLastFrame__Fv` hw; `clearInputs__7GamePad` matched; `updateDeathSequence__7Monster` matched+; `creditFull__11HealthMeter` matched+; `update__12StaminaMeteri` matched+; `creditFull__12StaminaMeter` matched+; `playerUpdateInputs__7Monster` matched+; `computeHighlight__12LevelPickupsR7Monster` equivalent+; `updateInputs__2Ai` asm+; `interpretInputs__8PadFlagsR7GamePad` asm+; `__vc__8PadFlagsi` matched; `updateOnFire__7Monster` matched+; `updateBeingShocked__7Monster` matched+; `updateAirLegOverride__7Monster` equivalent+; `smoothEasyIn__Fffff` equivalent; `setCloakOff__7Monster` matched+; `TogglePOV__7Camerasi` matched+; `updateReticle__7Monster` asm+; `updateLookAt__7Monster` asm+; `updateBoostAndRage__7Monster` equivalent+; `updateBoundingSphere__7Monster` asm+; `updateAnimContacts__7Monsterb` asm+; `updatePowerUpGlow__7Monster` asm+; `timerGetFieldCount__Fv` hw; `updateMonsterSound__12MonsterSound` matched+ |
| Monster | `takeHit__7MonsterP8_fvectorfi` | ppfi | 3 | `getInteractive__12Interactivesi` matched+; `takeDamage__7MonsterfbP7Monster` equivalent+; `isBlocking__7Monster` equivalent; `isBlockable__10StateBlockR8HitEvent` equivalent; `killPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori` equivalent+; `particleCreateFx__FP8_fvectorifPA3_ff` equivalent+; `playDestructibleSound__23LevelObjectSoundManagerUiP8_fvector` equivalent+; `transitionOK__11StateRecoil` asm; `enterNewState__7MonsterP12MonsterState` equivalent+; `knockBack__7MonsterR8_fvectorff` equivalent+; `transitionOK__12StateShocked` matched; `isHoldingLarge__7Monster` equivalent; `enterSubState__13StateGrappledQ213StateGrappled8SubState` matched+; `takeAdditiveRecoil__7MonsterR8_fvectorf` equivalent+; `inputSetActuator__FiP12ActuatorData` matched+; `transitionOK__12StateStunned` asm; `getStaminaGain__7Monster` matched; `credit__12StaminaMeterf` equivalent |
| MonsterInit | `init__7Monster` | p | 0 | `memset` asm; `dropPickup__7Monster` equivalent+; `dropPickupImpaler__7Monster` matched+; `particleKillFx__FRi` matched; `hierSetCsDrawMe__FP3_csUc` matched; `mathfUnitMatrix__FPA3_f` equivalent; `gameGetStartPoint__7TheGameP7Monster` equivalent+; `getClosestPath__9AiPathNetR8_fvectorUc` asm+; `init__8PadFlagsP7Monster` asm+; `init__15MonsterDynamics` equivalent+; `init__11AnimContactR11_animHandle` asm+; `Init__10FireBreath` matched+; `reset__11HealthMeter` matched; `reset__12StaminaMeter` matched; `initMonsterSound__12MonsterSoundP7Monster` equivalent+; `animationResetCharacter__FR11_animHandle` matched+; `rampIn__9AnimBlendf` matched+; `setSpeed__9AnimBlendf` matched; `setPercent__9AnimBlendf` matched+; `enterNewState__7MonsterP12MonsterState` equivalent+; `setCloakOff__7Monster` matched+ |
| StickShaker | `update__11StickShaker` | p | 0 | `timerGetFieldsLastFrame__Fv` hw; `newHit__10HitHistoryib` equivalent+; `inputSetActuator__FiP12ActuatorData` matched+ |
| hier | `hierSetCameraLosPoint__FP8_fvectorii` | pii | 0 | `viewGetTrans__Fi` matched |

Resumo: 99 chamados distintos (44 matched, 33 equivalent, 19 asm, 3 hw: os `timerGet*`, que só leem globais), 45 deles folha. 21 das 35 têm 1 ou 2 chamados;
3 são grandes (`Monster::update` 25, `Monster::init` 21, `Monster::takeHit` 18). 13 têm `jalr`.

### Como o código gerado chama
Todo `jal` sai como `runtime->dispatchGuestBranch(rdram, ctx, alvo, origem, volta, GuestBranchKind::DirectCall, "JAL")`; `jalr` sai como `IndirectCall`.
O despacho procura o alvo em `g_ps2RecompiledFunctionTable` (indexada por endereço; o `register_functions.cpp` gerado preenche inclusive os pontos de retomada no meio das funções),
roda a função e confere se `ctx->pc` voltou ao `volta`. O harness atual define a própria tabela vazia de 0 a 16 MB, por isso recusa as não-folha.
`hasFunction` falso cai no `reportMissingFunction` (política `SkipCallDebug` pula a chamada).

### Desenho proposto
O modelo já existe no `tools/difftest.py` (unicorn): todo chamado fora da função é **interceptado e registrado** (nome e argumentos, na ordem), devolve um valor
determinístico (`RETS`, `NARGS`, `C_ARGS` para libm, verificação dos registradores callee-saved). O oracle leva esse modelo para o C++ recompilado, em três etapas:

**Etapa 1: interceptar e registrar (cobre as 35).**
- O gerador lê os alvos `DirectCall` dos dois `.cpp` (retail de `output_fix`, NM de `output_nm_all`) e traduz endereço → nome pela tabela de símbolos de cada ELF
  (`game.elf` e `nm_all.elf`/`nm_all.syms`). Os endereços NM diferem dos do retail; casar pelo nome resolve.
- O harness preenche **todo** slot vazio da tabela (os 4 M) com um gravador "alvo desconhecido". Nos endereços de chamados conhecidos põe um gravador com nome.
  São duas tabelas, uma com os endereços retail e outra com os NM, trocadas entre as duas execuções (um `memcpy` da tabela ou de um vetor de slots alterados).
- O gravador registra `(nome, a0..a3/t0..t3, f12..f19)`, limitado ao número de argumentos que a assinatura mangled dá (reaproveitar `spec_from_mangled`/`C_ARGS` do difftest),
  põe `v0`/`f0` de um fluxo pseudo-aleatório semeado por (estado, índice da chamada), igual nos dois lados, e faz `ctx->pc = ra`.
  `jalr` com ponteiro de vtable lixo vai para o "desconhecido" com o endereço do alvo no registro. Os dois lados leem a mesma palavra, então o alvo também é comparado.
- Stubs com semântica real, porque o chamador depende do efeito: `memset`/`memcpy`/`memmove` (o `Monster::init` zera o objeto), placement new `__nw__FUiPv`
  (devolve `a1`; usado com `AnimBlend` em `takeAdditiveRecoil`/`launchDefaultProjectile`) e um alocador `bump` determinístico para `__builtin_new`/`malloc`, zerado a cada estado.
  libm (`cosf`, `sinf`, `atanf`, `acosf`, `tanf`, `sqrtf`, `atan2f`): rodar a retail recompilada, que é determinística, ou gravar como o difftest. Os `timerGet*` só leem globais e podem rodar de verdade.
  `printf`/`inputSetActuator`/som: só gravar.
- Comparação: hoje são retorno + arena + imagem. Passa a ser também a **sequência de chamadas com argumentos**, e os callee-saved (`s0..s7`, `fp`, `sp`, `gp`) na saída.
  Nova classe `calls` (memória igual, chamadas diferentes).
- Estado aleatório: o atual (arena de 16 KB, palavras float/ponteiro/pequeno/lixo) basta nesta etapa, porque nenhum chamado executa. Somar: (a) o `$ra` do teste fora da tabela,
  para o retorno final não ser confundido com chamada; (b) um teto de chamadas por estado (ex.: 10 000) para cortar laço sobre lista aleatória
  (`getBestTarget__14AiDetonateHead` com 10 `jalr`, como o `getClosestMonster` que já estoura o tempo).
- Esforço: **~1 dia** (gerador em `oracle_all.py` + template). Cobre **as 35**, e as outras 138 não-folha de hoje no mesmo passo.
  Qualidade: boa nas 32 pequenas e médias; nas 3 grandes compara a lógica própria e a sequência de chamadas, que é onde o NM costuma errar (`nm_calls.py` já mostrou isso estaticamente).

**Etapa 2: chamados reais do retail (lista de permitidos).**
- Compilar `output_fix/*.cpp` + `register_functions.cpp` uma vez numa `libretail.a` (5.4 mil arquivos, `-O1`, alguns minutos com `-j`) e usar a tabela gerada em vez da do harness.
- Na versão NM, reescrever no texto do `.cpp` os alvos de `DirectCall`/`DirectJump` para o endereço retail do mesmo nome. O `ra` continua o NM, e o
  `dispatchGuestBranch` compara com o `volta` NM, o que bate. Assim os dois lados chamam **o mesmo chamado retail** sobre a mesma imagem retail e só a função testada difere.
- Rodar de verdade só os chamados "puros" e folha: `mathf*`, `smoothEasy*`, `PointMass::resolveForcesEuler`, `StaminaMeter::hasEnough`, `getReflexDelay`, `getFovRelevance`, getters de `AnimBlend`.
  Os outros continuam gravados. Isso tira os falsos positivos de "o retorno aleatório levou a um caminho impossível".
- Esforço: **+1 a 1,5 dia**.

**Etapa 3: estado de um dump real (para AI e Monster).**
- Carregar um `ram_<q>.bin` de luta (`PS2X_DUMP_RAM`) em vez da arena aleatória. `this` vem das listas reais (jogadores do `TheGame`, `Ai` de cada monstro, `Cameras`),
  com vptr, `EnemyInfo`, `Interactives` e `LevelPickups` coerentes, e o aleatório só perturba N campos float/flag do objeto. A comparação passa a ser a RAM inteira (32 MB, `memcmp`).
  O pré-requisito é o mapa classe → onde achar instâncias no dump (já existe parte em `native/wotm_syms.h`).
- Com isso dá para rodar de verdade os chamados com efeito (`enterNewState`, `takeDamage`, `knockBack`), cobrindo `takeHit`/`update`/`init__7Monster` de ponta a ponta.
- Esforço: **+2 dias**.

Pré-requisito comum: **regenerar `output_nm_all`**. O de hoje é de 2026-10-16 (`nm_all.elf` 13:40). Equivalente feita depois disso aparece lá ainda com o asm do retail e
daria `exact` falso. O `output_fix` (2026-10-09) serve, porque o retail não muda e o tradutor já tinha a correção do `sqrt.s`.

## 3. Cobertura com gameplay para realimentar `config/boot_coverage_asm.csv`

### Como o csv foi gerado (2026-10-09)
- Tap no `PS2Runtime::dispatchGuestBranch` do clone do WSL (`~/wotm-recomp/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp`, modificação não commitada):
  `g_wotmCov[alvo>>2] = 1` para todo alvo de chamada, salto entre funções ou retorno. A cada 300 quadros, com `PS2X_COVERAGE=<arquivo>`, o runtime **sobrescreve** o arquivo com
  os endereços marcados, sem contagem.
- Execução: boot até a interface, ~3 min, filmes pulados (`~/wotm-recomp/work/cov.log`) → `cov.txt` (786 alvos) → `~/wotm-recomp/work/covmap.py`
  (alvo → função do `status.csv` por intervalo endereço+tamanho; conta por estado; grava as `asm`) → `cov_asm_pendentes.csv` (601 funções) → copiado para `config/boot_coverage_asm.csv`
  (`docs/ANALYSIS.md`: 786 funções = 97 matched, 31 equivalent, 601 asm, 57 hw). O `covmap.py` **não está no repo** e tem o caminho do Windows fixo.
- Significado: a fila de decompilação "o que o jogo chama de verdade e ainda está em asm", e é o padrão do `tools/m2c_batch.sh`. Hoje 315 das 558 funções distintas do csv
  ainda são `asm` (há nomes repetidos, como os `__static_initialization...` de cada TU). O resto já foi feito.

### O que o runtime do Windows já tem
O `wotm-runtime.patch` evoluiu o tap: `++g_wotmCov[alvo>>2]` (contagem `uint32`) e, com `PS2X_COVERAGE=<prefixo>`, grava `<prefixo>.<apresentação>` a cada 300 apresentações
(`0xENDEREÇO contagem`, acumulado). Está documentado em `RUNTIME.md` e `FRONTEIRA.md`, e o `tools/hw_frame.py` lê esse formato (chamadas por quadro, modos `hw|sce|tudo`).
**Não falta instrumentação: falta só o procedimento e o mapeamento.**

Indício do ganho, com arquivos velhos que ainda estão em `wotm-recomp-win\discroot\cov.*` (de 10-09/10-10, de execuções misturadas, só para ordem de grandeza):
`cov.4200` (uma execução que chegou à fase) tem 1380 funções (498 que hoje são asm). A união com `cov.2700`/`cov.6000` dá 1441 funções, 514 hoje asm,
e **199 funções asm (~122 KB) que não estão no `boot_coverage_asm.csv`**: AiReflex 19, VehicleNavigator 14, AiSeek 13, HeliVehicleNav 13, MonsterStates 13, Hud 10, Monster 9, Vehicle 7...

### Limites do tap (para ler o resultado)
- Cobertura por **entrada de função** (alvo de despacho), não por bloco. Os retornos caem dentro do chamador, que já está marcado, e o `covmap` mapeia por intervalo.
- Função trocada inteira por gancho do nativo (`snd_*`, `timerGetCurTics`, `uiMain` no `WOTM_MATCH`, filmes, `hierFlushObjQ`...) continua contada, mas os chamados dela somem.
  As chamadas diretas em C++ que não passam pelo despacho são só SDK/syscall (`EIntr`, `kprintf`, `Exit`...).
- O sufixo do arquivo conta **apresentações**: num monitor de 180 Hz são 3 por quadro do jogo. Para o `hw_frame.py` dividir certo, rode com `WOTM_HZ=60`.
- `WOTM_MATCH` pula o menu e o `AIVSAI` não exercita a entrada humana (`playerUpdateInputs`, `PadFlags::interpretInputs` do jogador). Por isso uma passada com `run_live -Auto` (roteiro de pad) e uma luta jogada também entram.

### Procedimento proposto
1. **Ferramenta no repo**: `tools/cov_merge.py` (o `covmap.py` com caminho relativo, aceitando o formato com contagem e o antigo, unindo N arquivos). Saídas:
   `config/coverage_asm.csv` com `tu,function,address,size,calls,origem` (`boot|menu|luta`), ordenado por TU/endereço, e um resumo por TU.
   O `m2c_batch.sh` passa a ler esse csv, ou o `boot_coverage_asm.csv` é regenerado com as mesmas 4 colunas, para não quebrar quem lê.
2. **Captura** (cada execução numa pasta própria, `WOTM_HZ=60`):
   - boot + menu: `run_live.bat -Auto` com `PS2X_COVERAGE=<pasta>\boot\cov`;
   - lutas: `tools\lote.ps1` com uma opção nova `-Coverage` (poucas linhas: antes de cada luta, `PS2X_COVERAGE="$disc\cov"` e apagar `cov.*`; depois, mover o último `cov.*`
     para `$out\$tag.cov`). Um lote curto que cubra fases e monstros: `-Monsters 1,2,3,4,5,7,8,9,10,11 -Levels 1,2,4,5,7,8,9,10,13,14,15,16 -AIs 1 -Secs 120`, mais algumas
     com `-AIs 3` (pickups, veículos, helicópteros) e com `WOTM_CHEATS=vencer=25` (fim de rodada, vitória, `updateDeathSequence`);
   - uma luta jogada no teclado (entrada humana, agarrão, arremesso, especial). Precisa do dono na máquina: avisar antes, sem injetar teclas.
3. `python tools/cov_merge.py <pasta>\**\*.cov <pasta>\boot\cov.*` → csv novo. Conferir a diferença contra o `boot_coverage_asm.csv` (esperado: +200 asm ou mais) e commitar (só `config/` e `tools/`, sem gate).
4. Apagar os `cov.*` e a pasta do lote depois de gerado o csv (regra dos dumps descartáveis).

Esforço: script ~2 h; opção no `lote.ps1` ~1 h; captura ~1 h de máquina (lote sem vigia humano) + ~10 min de luta jogada.
Não abri o runtime nesta investigação.
