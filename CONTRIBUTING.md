# Contribuindo

Obrigado por ajudar! O projeto é uma decompilação cujo objetivo é um port nativo de PC fácil de modar; leia o [CLAUDE.md](CLAUDE.md) (serve para humanos também) e o [README](README.md) para o setup.

## O que priorizar
- **Byte match não é exigido.** Uma função `equivalent` (C++ legível, comportamento verificado) vale tanto quanto uma `matched` para o objetivo. Se bater barato, ótimo; não gaste horas afinando.
- **Escolha pelo que importa para modar**, não pelo tamanho: lógica de jogo, IA, monstros, fases, animação e formatos de arquivo. A fila está em `config/boot_coverage_asm.csv`
  (funções ainda em asm que o jogo chama do boot até a interface). Deixe para o fim memory card (`McPage`/`McFile`), `input`, `vi`, `ps`, `sce/*` e `lib989snd`.
- **Confira o comportamento**: `tools/difftest.py` para funções sem VU0; `tools/recomp_oracle` (veja o README dele) para as que usam VU0 ou `min/max/madd`. Não use `sqrtf` em código novo: use `eeSqrtf` (`include/vecmath.h`).

## Licença
Ao contribuir, você concorda que sua contribuição seja distribuída sob a [GPL-3.0](LICENSE) do projeto.

## Antes de começar
- Você precisa da **sua própria cópia** do jogo (NTSC-U, SCUS-97197). Nada do jogo entra no repo: nem ISO, nem `asm/`, nem trechos de assembly em issues/PRs.
- Faça o setup do README e confirme que `ninja` termina com `build/SCUS_971.97.rom: OK` antes de mexer em qualquer coisa.
- Combine com o dono qual área (TU) você vai pegar, para não trabalharmos na mesma. Headers compartilhados (`include/game/game.h`, `include/engine.h`) são o ponto mais provável de conflito: avise antes de alterá-los.

## Fluxo
1. Fork do repo (ou branch, se você tem acesso de escrita) a partir da `master` atualizada: `git switch -c <tu>-<assunto>`.
2. Um PR por TU ou por pequeno grupo de funções relacionadas.
3. Antes de cada commit: `sh tools/wsl/gate.sh` tem que terminar em `ROM OK`.
4. **Não commite** `config/status.csv`: é gerado (`tools/progress.py`) e causa conflito em todo PR. O dono o regenera ao integrar o PR. Evite `git commit -a`.
5. Abra o PR contra `master` preenchendo o template. Não há CI (não dá para buildar sem a ROM), então a saída do `gate.sh` no PR é a prova. O merge na `master` é feito só pelo dono e por `LostTwilightG`.
6. Mantenha o branch atualizado com `git rebase origin/master`.

## Usando Claude Code
Ok e incentivado. O `CLAUDE.md` já carrega as regras. Lembre o seu Claude de rodar o gate antes de cada commit.
