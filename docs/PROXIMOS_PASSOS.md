# Próximos passos (atualizado em 2026-10-11; detalhes em docs/passos/)

## Direção atual (decidida com o usuário em 2026-10-09; em validação)
O objetivo continua sendo um port nativo de PC fácil de modar, mas o **meio** mudou: a base passa a ser o jogo **recompilado** (PS2Recomp, fora do repo) e a decompilação vira a
camada legível que substitui, por *hook*, só as funções que importam para modar. Consequências práticas:
- **Byte match deixou de ser requisito por função.** `equivalent` é suficiente para o que vamos modar. `matched` continua sendo bem-vindo quando sai barato, e o `gate.sh`
  continua obrigatório (a ROM do build normal tem que bater o SHA1).
- **Fila de prioridade**: `config/boot_coverage_asm.csv` (601 funções ainda em asm que o jogo chama do boot até a interface; é um piso, não cobre gameplay). Priorizar o que é
  engine/formatos/lógica de jogo (`common/animation`, `AnimCurve`, `mathf`, `hier`, `dbs`, `texm`, `view`; `game/Monster*`, `Shell`, IA) e deixar para o fim memory card,
  `input`, `vi`, `ps`, `sce/*`, `lib989snd`.
- **Verificação**: para equivalentes com VU0 ou `min/max/madd` (62 de 329) use `tools/recomp_oracle` (retail × nossa, ambos recompilados); para as demais, `tools/difftest.py`.
  O oracle já achou dois erros reais (`sqrtf` com `sqrt.s` mal codificado e `sb` virando `sw`; ver "Armadilhas").
- **Renderização**: a meta é cortar na fronteira da engine (`config/hw_boundary.csv`) e desenhar nativo; o VU1/GS emulado do runtime do PS2Recomp é só ponte para ver imagem.
  Som, FMVs e entrada completa ficam fora do primeiro port (só um mapeamento mínimo de teclado para o pad).
- **Pendente**: (1) testar o build do runtime no Windows/MSVC e ver se aparece imagem; (2) PR no PS2Recomp com a correção de `SQRT.S`/`RSQRT.S` (o upstream usa `fs` onde o R5900 usa
  `ft`; 108 + 76 funções do jogo afetadas); (3) oracle para funções com chamadas (hoje só folhas: 24 de 60 candidatas rodaram); (4) cobertura com gameplay.
- O que foi medido e como reproduzir está em `tools/recomp_oracle/README.md` e na seção "Recompilação estática" de `docs/ANALYSIS.md`.

## Onde estamos
- `sh tools/wsl/gate.sh` diz `ROM OK` (build + SHA1). Estado em 2026-10-15 (`python3 tools/progress.py`, bloco de progresso do README): `game` 1719 de 3181 funções
  decompiladas (idênticas + equivalentes), 407 KB de 909 KB; `common` 836 de 1144. Convenções em `docs/ANALYSIS.md` ("Convenções de status das funções").
- Ferramentas do dia a dia: `tools/verify_tu.py` (todas as verificações de uma TU numa tabela), `tools/m2c_batch.sh` (rascunhos da fila), `tools/vu1cmp.ps1` (checagem VU1 x nativo e imagens a partir de instantâneos, sem jogo; desde 2026-10-16 imprime o **inventário**: o que só a referência ou só o nativo desenha, por dono, em qualquer tela, com gatilho `mem:currScreen` e tecla F8; `passos/VU1CHECK.md`), `tools/corpus.ps1` (corpus de regressão: `-Capture` grava menus e lutas em `<runtime>\corpus`, `-Check -Baseline <csv>` diz o que piorou depois de uma mudança em `native/`). O código do runtime mora em `native/runtime/`.
- **Direção decidida em 2026-10-16:** port de PC primeiro; decomp só na superfície de mod (`Shell::Load*`, `MonsterParse`, câmera); runtime cobaia parado. Ordem: (1) inventário automático do que falta (feito o lado numérico; falta o diff por pixel com atribuição e a imagem de referência nos menus), (2) widescreen como primeiro mod, (3) HUD, fades, iluminação e bugs gráficos na ordem do inventário.
- Objetivo do port: precisa de ~1000 funções / ~310 KB (lógica de jogo alcançável pelo laço de atualização), fora hardware. Lista em `config/callgraph.csv`.

## Estratégia (decidida com o usuário em 2026-10-07)
1. **Escopo por alcance**, não por tamanho: `python3 tools/callgraph.py Update__7TheGame,InitBeforeDbLoad__7TheGame,InitAfterDbLoad__7TheGame,ResetLevel__7TheGame --no-libs`
   grava `config/callgraph.csv` (função, TU, tamanho, profundidade). Só segue `jal`; chamadas virtuais (`jalr`) não entram, então é um piso.
   Trabalhar de cima para baixo nessa lista. Hardware (`config/hw_funcs.txt`) fica de fora; `game/Sound` e `game/StreamingSoundManager` NÃO são hardware
   (decisões de jogo) mas o port pode começar com áudio mudo, então são baixa prioridade.
2. **Equivalente natural, sem afinar**: escrever C++ natural, pontuar, embrulhar com `nm_wrap.py` se não bater. Só afinar quando é barato (ver armadilhas).
3. **m2c como rascunho** para funções com mais de ~30 instruções: `sh tools/m2c.sh <tu> <função>`. Conferir contra o assembly, trocar `unkNNN` por campos
   nomeados dos headers, ajustar tipos. Funções curtas: escrever direto. Nunca confiar no m2c sem ler o asm (os argumentos que ele mostra são ruído de registradores).
4. **Comparar comportamento com o PCSX2** assim que houver algo rodando (ainda sem testes; decisão do usuário).
5. **Unificar tipos conforme os usos se repetem**: promover campo a header só quando há evidência (mesmo offset, mesmo uso) em 2+ lugares ou o m2c/asm deixa claro.

## Como retomar
1. `wsl -d Ubuntu` (a distro padrão é a `docker-desktop`, que não serve). Venv: `~/.venvs/wotm/bin/python`. Raiz: `/mnt/c/Users/TwistZero/WoTM`.
2. **Gate de commit**: `sh tools/wsl/gate.sh && git commit ...` (de preferência o `git commit` rodando no Windows; o git do WSL não tem identidade).
   `check.sh; git commit` ou `check.sh | tail && git commit` commitam builds quebrados.
3. Converter TUs novos: `sh tools/wsl/convert_tus.sh TuA TuB ...` (um de cada vez, só mantém os que não quebram a ROM, posiciona vtables com `place_data.py`).
   Depois de um TU removido/revertido, regenerar com `~/.venvs/wotm/bin/python configure.py --split` (senão o `build.ninja` aponta para arquivo inexistente).
4. Por função: ler `asm/nonmatchings/<tu>/<func>.s` (ou rodar o m2c), escrever o C++ no lugar da linha `INCLUDE_ASM`,
   `sh tools/wsl/scoreall.sh <TU...>` (pontua com `-DNON_MATCHING`), `nm_wrap.py <arquivo> <simbolo> "<nota>" [Classe::metodo]` se não bater.
   Rascunhos de toda a fila de uma vez: `sh tools/m2c_batch.sh` (`build/m2c/`). No fim da TU: `~/.venvs/wotm/bin/python tools/verify_tu.py game/<TU>`
   (tabela por função com pontuação, difftest, chamadas, layout e retornos; `--no-nm` para a versão rápida). Detalhes em `passos/FERRAMENTAS.md`.
5. Antes de mexer em header compartilhado: `sh tools/wsl/scoreall.sh <todos os TUs de src/game> > ~/base.txt`; depois comparar com `diff`. Rodar `gate.sh`.

## Índice dos detalhes (`docs/passos/`)
- [RUNTIME.md](passos/RUNTIME.md): jogo recompilado no Windows, variáveis `PS2X_*`, roteiro de pad, build rápido sem LTCG, teclado, próximos passos do runtime.
- [NATIVO.md](passos/NATIVO.md): `native/` (raylib), texturas/paletas, desenho ao vivo, monstros skinned, LOD, partículas/efeitos, sombras projetadas, física com `dt` no runtime, hooks de host (`hostLeaf`) e lições de método.
- [FERRAMENTAS.md](passos/FERRAMENTAS.md): `tools/`, headers compartilhados (`include/`), trabalho em TheGame, difftest.
- [ARMADILHAS.md](passos/ARMADILHAS.md): bugs/pegadinhas já vistos (gcc 2.95, gp, rodata, `sqrtf`, `__asm__("sym")`, build NM).
- [TESTE_PCSX2.md](passos/TESTE_PCSX2.md): ELF halfcpp, layout `gen_nm_ld.py`, heap, bugs de comportamento achados jogando.
- [ESPINHA.md](passos/ESPINHA.md): cadeia `main` -> monstro no mapa, menu, `rtMain`, roteiro de blocos pendentes.
- [IA.md](passos/IA.md): estrutura de AiAction/AiSeek e onde parei.
- [VIEWER_WEB.md](passos/VIEWER_WEB.md): ideia de visualizador de modelos 3D no navegador (2026-10-14): o que já se sabe, o que falta (texturas e pose dos monstros sem dump), regra de não hospedar arquivos do jogo, estimativa.
- [PLANO_PROCESSO.md](passos/PLANO_PROCESSO.md): plano de 2026-10-15 para otimizar o processo de decomp e de port (comparação offline VU1 x nativo, código do port no repo, endereços por nome, verify_tu, runtime cobaia 1.1) com o estado de cada item.
- [VU1CHECK.md](passos/VU1CHECK.md): checagem VU1 x nativo (2026-10-14): objetos, textura e partículas por sistema; fonte única do estado, do que foi corrigido e das pistas abertas (partículas restantes, itens de alfa, tex 156/193), o que não cobre (cor, alfa, HUD) e próximos passos.
