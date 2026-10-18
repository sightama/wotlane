# Espinha do jogo: cadeia de carga, menu, laço de jogo, estados de monstro

(Dividido de `docs/PROXIMOS_PASSOS.md`; índice lá.) Este arquivo guarda **o que se sabe do fluxo do jogo**, levantado ao decompilar a espinha em 2026-10-08. O que está ou não em C++ é o `config/status.csv` (`grep <função>`), não este arquivo.

## Cadeia de carga: da `main` ao monstro no mapa
Fluxo de `main` (comentário em `src/game/Shell.cpp`): boot -> intro/outro -> menus (`userintMain`) -> sessão: `InitRTState`, `BootInitGame`, `InitBeforeDbLoad`, `LoadLevelFiles`, `FinishLoadBar`, `FadeScreen`, `InitAfterDbLoad`, `InitPlayers`, `initAfter` dos 4 `Hud`, init por modo (BigShot/Crush/DodgeBall), `UpdatePadTweaks`, sons, `rtMain` em laço com `EvaluateGameStatus`.
- Carga: `Shell::LoadLevelFiles` -> `LoadLevelDB`, `LoadMonstersDB`, `LoadResTexture`, `LoadTexture` -> `dbsRelocateFileZero` (`.PTR`, TBP0) -> `dbInitDb` -> `dbsRelocateViaPtrListFile` (`mon/<nome>.ptr`) -> `dbsTraverse` (+ `dbsPush`/`dbsPop`) -> `dbProcInteractive` (tabela de ids de objeto). Formatos e tabela de ids em `docs/FORMATOS.md`.
- Monstros: `Shell::InitPlayers` -> `TheGame::MonsterParse/AddMonster/GetMonsterFromName/SetPlayerMonster/SetAIMonster` (criam o slot, registram em `Interactives`) -> `Monster::initBeforeDbLoad/playerInit/aiInit/initAfterDbLoad`. `Monster::playerInit` salta (`j`) para `aiInit` com os mesmos registradores.
- As funções `file*` e `zip*` são camada de plataforma (o port lê arquivos direto).
- Validação dessa cadeia: halfcpp no PCSX2 (`TESTE_PCSX2.md`; `idx.py` confere que ids e índices de `Interactives` batem) e o jogo rodando no runtime.

## Menu (`game/ui`, `game/screen`)
- As telas de `screen*` usam `screenGetInput(1)`: devolve `(pad << 16) | ação`; ações 1/2 = cima/baixo, 3/4 = esquerda/direita, 5 = voltar, 6 = confirmar. Cada tela mexe em `currentSelection[currScreen]` e troca `hierSetSwitch` do shell.
- Modos de jogo gravados em `shell->m_mode` (e `shell+0x2B54`, o modo "padrão" restaurado por `resetGameMode`): 2 = desafio 1 jogador contra a CPU (o que `WOTM_MATCH` preenche), 3 = free-for-all 2P (`PO_MPFreeForAll`), 6 = elimination 2P (`PO_MPElimination`), 7/8/9 = minigames das fases 25/26/27.
- Fases 10, 11, 14, 15, 25, 26 e 27 só abrem com a flag da tabela `monsterSelectMode` (`levelUnlocked`). O monstro bônus alarga a faixa selecionável de 1..9 para até 10 (`findAvailableMonster`).
- `currScreen` 0x14 = `screenWaitForStart` (Press Start): só aceita `screenGetInput(1)==0x10` (Start) quando `betweenScreens==0`. 0x10 só faz `changeScreen(0x10->0x14)`, 1 = menu, 0x63 = carregamento (`Shell::FadeScreen`, `DisplayLoadBackground`); dentro da fase `currScreen=1`.
- O port pode trocar os menus por UI própria; o inventário da checagem lista o que eles desenham (`VU1CHECK.md`).

## Laço de jogo (`game/rt`)
- `rtMain(first)` é o laço por quadro: `startFrame` → input → por view: `CullView`, double buffer GS, `viewUpdate`, HUD, `hier(view,0)`, `TheGame::Update` (só view 0), `animationRunGlobal`, `Update2` (view 1 ou única), partículas, `TaskManager(debris)`, `hier(view,1)`, DMA; no fim do quadro `updateLevelObjectSoundManager`, `updateSoundManager`, `g_frame++` e o pacing de tempo (`timerWaitForMinUpdateRate`; detalhes do `dt` em `NATIVO.md`, "Física com `dt`").
- Retorna o código dado a `rtReturnToShell` (2 = pausa/diálogo, 3 = sair, 5 = encerrar sessão, 0/1/4 = fim de fase). `rtPauseRT` (START ou controle desconectado → `rtReturnToShell(2, bit do campo GS)`).
- Os códigos vêm de `Shell::EvaluateGameStatus` e dos `Evaluate*Status` por modo (história, desafio, FFA com/sem IA, endurance, bigshot, crush, dodgeball). Sem `m_killTarget` (0), `EvaluateOnePlayerChallengeStatus` só reergue o derrubado (`reinitSlot`) e a luta segue sem o diálogo de fim.

## Estados de monstro e IA (para quando a decomp voltar a eles)
- Blocos grandes ainda só em asm: `MonsterStates`, `SpecialStates`, `MovementStates` (TU convertido; só getters feitos), `AiReflex`, `AiSeek`, `Weapon`/projéteis (`DetonateWeapon` 0x1220), `Hud` (update/print), `Cameras::Update`. `Sound`/`StreamingSoundManager`/`MonsterMc`/`McPage`/`McFile` são plataforma: só interessa o contrato.
- **Estados de monstro:** base em `include/game/monster_state.h` (`id`@0, `flags`@4, `owner`@0xC, `vptr`@0x10; vtable `{delta, 0, função}`; ctor e vtable ficam asm, métodos sem `virtual`). Cada estado vive embutido em `Monster` num offset fixo (Recoil 0x7E30, Block 0x7DA0, Stunned 0x10714, Shocked 0x10BA0, Grappled 0xDDCC, vitória 0x10E70, estado especial 0x7980). Ordem sugerida: `transitionOK/transitionFeasible/getRelevantConfig` (pequenos) -> `update` de cada estado -> `transitionInto/handleCollis`.
- **IA:** `AiReflex` e `AiSeek` dependem de `AiBrain`/`AiNavigator` (já em C++); a navegação (A*, `seek/arrive/tag`) já está. Layout do `AiNavigator`: `monster` 0x0, `turn` 0xC, `strafe` 0x14, `mode` 0x18, `status` 0x1C, `ObstacleSensor` 0x30 (0x130 bytes), `AiPathFinder` 0x160 (0x28), alvo 0x188..0x19C, `PathInfo` 0x220/0x240 (0x20 cada).
- Truques de codegen vistos nesses TUs: `(int)(bits >> n) & 1` gera o `dsll/dsra32` do retail; para testar um bit de `unsigned long long` num `if`, guardar `bits & m` num `unsigned long long` local; `lq/sq` por `asm volatile` com `$2` fixo; `fabsf` (extern "C") gera `abs.s`; `max.s`/`rsqrt.s` só por `asm`; `permute.py` precisa de N <= nº de ordens possíveis (senão trava).

## Observações de jogo ainda sem causa (halfcpp, 2026-10-08)
- A IA às vezes repete a mesma ação sem parar (pode ser comportamento do jogo original).
- Dois casos de "teleporte" depois de um golpe forte que arremessa o monstro: o último `HitEvent` era tipo 3 / subtipo 30, tratado só com dano (igual ao retail); a causa pode ser o knockback/física fora da `takeHit`. Não confirmado.
