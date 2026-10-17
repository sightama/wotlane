# Próximos passos (atualizado em 2026-10-16; detalhes em docs/passos/)

## Direção (decidida em 2026-10-09, confirmada em 2026-10-16): port primeiro
O objetivo continua sendo um port nativo de PC fácil de modar. A base é o jogo **recompilado** (PS2Recomp, fora do repo) mais a camada nativa deste repo (`native/`), e a decompilação é a
camada legível que substitui, por *hook*, só as funções que importam para modar. Consequências práticas:
- **O port vem antes da decomp.** Próximo: iluminação e bugs gráficos **na ordem do inventário** da checagem (`passos/VU1CHECK.md`, "Inventário do port"),
  com `tools\corpus.ps1 -Check` antes e depois de cada mudança em `native/` (fluxo no `CLAUDE.md`, "Fluxo do port"). O runtime cobaia (equivalentes dentro do runtime) está parado, incompleto
  (`passos/NATIVO.md`, "Runtime cobaia").
- **A fazer (anotado em 2026-10-17): ordem LIFO da OT de alfa no passe translúcido.** O jogo (`pktLinkAlphaObjAsm`) põe cada objeto translúcido no
  começo da lista da sua faixa de profundidade (`int(8z)+1`, z = vf30.x do programa 0x0 do VU0), então no empate o último visitado é desenhado primeiro;
  o nativo desenha na ordem da travessia. Confirmado pela ordem do XGKICK no menu (`g` 469: logo `ea3300` antes da tela `ea2800`). A 1a tentativa sumiu
  com o logo do menu, mas as duas pendências que a travavam caíram em `b33d854` (corte por esfera; o logo forte vinha do filme do telão, `texId 999`).
  Fazer: trocar o desempate no `drawList` (e a profundidade pela esfera do objeto contra o plano da câmera), conferir o logo no menu ao vivo e no
  `menus.vu1snap`, e ver se muda as rachaduras do Kineticlops na cor 3. Contexto em `passos/VU1CHECK.md` ("Kineticlops na cor 3", item (2)).
- **Decomp só na superfície de mod:** `Shell::Load*`, `MonsterParse`, câmera. **Byte match não é requisito por função.** `equivalent` é suficiente; `matched` quando sai barato; o `gate.sh`
  continua obrigatório (a ROM do build normal tem que bater o SHA1). Quando voltar a decompilar fora dessa superfície, a fila é `config/boot_coverage_asm.csv` (engine/formatos/lógica primeiro;
  memory card, `input`, `vi`, `ps`, `sce/*`, `lib989snd` por último).
- **Verificação das equivalentes:** `tools/verify_tu.py game/<TU>` (pontuação, difftest, chamadas, layout, retornos); para funções com VU0 ou `min/max/madd` use `tools/recomp_oracle`
  (`tools/recomp_oracle/README.md`; hoje só folhas, 24 de 60 candidatas rodaram).
- **Pendências fora do caminho crítico:** PR #277 no PS2Recomp (`SQRT.S`/`RSQRT.S`; 108 + 76 funções do jogo afetadas); oracle para funções com chamadas; cobertura com gameplay para realimentar `boot_coverage_asm.csv`.

## Onde estamos
- **Port:** o jogo roda no Windows a 60 fps com desenho nativo (fases, monstros, partículas, sombras, neblina, texto do menu), luta por teclado, som e filmes. Geometria casada com o VU1 em mais de 99%
  dos triângulos; textura, UV, cor e neblina conferidos; HUD do `hier` (retrato, barras de vida e energia, *letterbox*) e widescreen Hor+ (janela em 16:9, `WOTM_ASPECT`) e fades desde 2026-10-16. Falta o resto do inventário (iluminação e bugs gráficos; resíduos em aberto em `passos/VU1CHECK.md`). Código em `native/` e `native/runtime/`; estado por área em `passos/NATIVO.md`.
- **Decomp:** `sh tools/wsl/gate.sh` diz `ROM OK` (build + SHA1). Bloco de progresso do README (`python3 tools/progress.py`): `game` 1723 de 3181 funções decompiladas (idênticas + equivalentes),
  417 KB de 909 KB; `common` 836 de 1144. Convenções em `docs/ANALYSIS.md` ("Convenções de status das funções").
- **Ferramentas do dia a dia:** `tools/corpus.ps1` (corpus de regressão da checagem), `tools/vu1cmp.ps1` (checagem VU1 x nativo e imagens a partir de instantâneos, com inventário por dono, gatilho
  `mem:currScreen` e tecla F8), `tools/lote.ps1` (lutas em lote com vigia), `tools/verify_tu.py`, `tools/m2c_batch.sh`, `tools/wotm_env.py` (tabela de variáveis `WOTM_*`).

## Como retomar
1. `wsl -d Ubuntu` (a distro padrão é a `docker-desktop`, que não serve). Venv: `~/.venvs/wotm/bin/python`. Raiz: `/mnt/c/Users/TwistZero/WoTM`.
2. **Port:** `build_runtime.bat` (build `build_dev`, sem LTCG), `run_live.bat` para jogar (teclas em `passos/RUNTIME.md`), `tools\corpus.ps1 -Check` antes e depois. Commit sem gate.
3. **Decomp, gate de commit:** `sh tools/wsl/gate.sh && git commit ...` (de preferência o `git commit` rodando no Windows; o git do WSL não tem identidade).
   `check.sh; git commit` ou `check.sh | tail && git commit` commitam builds quebrados.
4. Converter TUs novos: `sh tools/wsl/convert_tus.sh TuA TuB ...` (um de cada vez, só mantém os que não quebram a ROM, posiciona vtables com `place_data.py`).
   Depois de um TU removido/revertido, regenerar com `~/.venvs/wotm/bin/python configure.py --split` (senão o `build.ninja` aponta para arquivo inexistente).
5. Por função: ler `asm/nonmatchings/<tu>/<func>.s` (ou rodar o m2c), escrever o C++ no lugar da linha `INCLUDE_ASM`,
   `sh tools/wsl/scoreall.sh <TU...>` (pontua com `-DNON_MATCHING`), `nm_wrap.py <arquivo> <simbolo> "<nota>" [Classe::metodo]` se não bater.
   Rascunhos de toda a fila de uma vez: `sh tools/m2c_batch.sh` (`build/m2c/`). No fim da TU: `~/.venvs/wotm/bin/python tools/verify_tu.py game/<TU>`. Detalhes em `passos/FERRAMENTAS.md`.
6. Antes de mexer em header compartilhado: `sh tools/wsl/scoreall.sh <todos os TUs de src/game> > ~/base.txt`; depois comparar com `diff`. Rodar `gate.sh`.

## Índice dos detalhes (`docs/passos/`)
- [NATIVO.md](passos/NATIVO.md): `native/` (raylib), texturas/paletas, desenho ao vivo, monstros skinned, LOD, partículas/efeitos, sombras, física com `dt` no runtime, hooks de host, vigia, partida direta (`WOTM_MATCH`), lote, runtime cobaia, tabela de variáveis `WOTM_*`.
- [VU1CHECK.md](passos/VU1CHECK.md): checagem VU1 x nativo: como funciona, **inventário do port** (o que falta/sobra, por dono), estado, resíduos em aberto, microprogramas das partículas, tudo o que já foi corrigido.
- [RUNTIME.md](passos/RUNTIME.md): runtime recompilado no Windows: build sem LTCG, variáveis `PS2X_*`, teclado, rastro do boot, endereços úteis.
- [FERRAMENTAS.md](passos/FERRAMENTAS.md): `tools/`, headers compartilhados (`include/`), estado de `TheGame`, difftest.
- [ARMADILHAS.md](passos/ARMADILHAS.md): bugs/pegadinhas já vistos (gcc 2.95, gp, rodata, `sqrtf`, `__asm__("sym")`, build NM).
- [TESTE_PCSX2.md](passos/TESTE_PCSX2.md): ELF halfcpp no PCSX2, layout `gen_nm_ld.py`, heap, bugs de comportamento achados jogando.
- [ESPINHA.md](passos/ESPINHA.md): cadeia `main` -> monstro no mapa, menu, `rtMain`, estados de monstro: o que se sabe do fluxo do jogo.
- [IA.md](passos/IA.md): estrutura de AiAction/AiSeek e onde parou.
- [VIEWER_WEB.md](passos/VIEWER_WEB.md): ideia de visualizador de modelos 3D no navegador (2026-10-14).
- [PLANO_PROCESSO.md](passos/PLANO_PROCESSO.md): plano de 2026-10-15 para otimizar o processo (com explicação para leigo) e o estado de cada item.
