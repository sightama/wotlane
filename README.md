<img width="2113" height="744" alt="War of The Monsters recompiled LOGO" src="https://github.com/user-attachments/assets/6160bebd-48f0-47b5-bd28-f74d23c662fd" />

# War of the Monsters Recompiled!

Projeto de decompilação/recompilação de **War of the Monsters** (PS2, NTSC-U, SCUS-97197).

Você precisa de uma cópia própria do jogo. Nenhum arquivo do jogo vai para este repositório.

## Objetivo
Um **port nativo de PC fácil de modar**. O caminho que estamos validando (decidido em 2026-10-09, ainda em teste):
1. **Recompilação estática como base.** O [PS2Recomp](https://github.com/ran-j/PS2Recomp) traduz o executável inteiro (5.382 funções, 0 falhas de decodificação, VU0 incluso) para C++ que roda num runtime próprio. Esse C++ é literal (uma função por função MIPS, sobre uma RAM de PS2 emulada) e é código do jogo, então **nunca entra neste repositório**: é gerado localmente a partir da sua ISO.
2. **Decompilação só do que importa para modar.** Lógica de jogo, IA, monstros, fases e formatos de arquivo viram C++ legível aqui (função `matched` ou `equivalent`) e entram como *hooks* no lugar da versão recompilada. Bater byte a byte é **opcional**: serve para manter a ROM retail reproduzível, mas não é exigido de cada função.
3. **Renderização nativa.** Em vez de emular VU1/GS, cortamos na fronteira da engine (`hier`, `pkt`, `disp`, partículas; veja `config/hw_boundary.csv`) e desenhamos com a GPU do PC. Som, FMVs e entrada completa ficam para depois do primeiro port.

Arquitetura, o que vai no repo e o que se gera localmente: [docs/PLANO.md](docs/PLANO.md).

**Licença:** o código deste repositório é [GPL-3.0](LICENSE). Ela cobre apenas o que está aqui (decomp, ferramentas, camada nativa); não dá nenhum direito sobre o jogo, que continua pertencendo aos seus detentores.

Estado: o jogo recompilado inicializa e chega ao laço de interface (`userintMain`) num runtime de teste; ainda **não** validamos imagem na tela (teste em Windows/MSVC em andamento). Veja [docs/PROXIMOS_PASSOS.md](docs/PROXIMOS_PASSOS.md) e [tools/recomp_oracle/README.md](tools/recomp_oracle/README.md).

## Setup (WSL/Ubuntu)
```sh
sudo apt install binutils-mips-linux-gnu ninja-build python3-venv
python3 -m venv ~/.venvs/wotm && ~/.venvs/wotm/bin/pip install "splat64[mips]" pyelftools
```
1. Coloque a ISO em `ISO/` e extraia para `disc/` (por exemplo, `7z x -odisc ISO/*.iso`).
2. `~/.venvs/wotm/bin/python configure.py --split` gera a ROM, os símbolos, roda o splat e escreve o `build.ninja`.
3. `ninja` monta, linka e confere o SHA1 contra o executável original (`build/SCUS_971.97.rom: OK`).

### Notas para um clone novo
- `tools/cc.sh` roda o compilador (binário Win32) via `wine`, que precisa da arquitetura i386 habilitada **antes** da instalação:
  ```sh
  sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install wine wine32:i386
  ```
- Em clone novo, `configure.py --split` pode não gerar os `.s` de dados (por exemplo as vtables, como `_vt$12PointToolKit.s`) e o `ninja` quebra por arquivo inexistente (visto com splat 0.41.1 e 0.50.0). Contorno: mover `src/` para fora do repo, rodar o `--split` e restaurar `src/` em seguida. Causa ainda não investigada.
- `tools/difftest.py` precisa de `unicorn` e `pyelftools` no venv e roda no WSL.

`tools/symdiff.py` lista símbolos fora do lugar e `tools/romdiff.py` mostra as palavras que diferem quando o checksum falha.

Veja [docs/ANALYSIS.md](docs/ANALYSIS.md) para o que já se sabe sobre o executável.

<!-- PROGRESS:START -->
## Progresso da decompilação

![Progresso da decompilação](docs/progress.svg)

| Área | Feitas | Total | % | Bytes |
|---|---:|---:|---:|---|
| `game` | 932 | 3181 | 29,3% | 107,9 KB de 909,0 KB (11,9%) |
| `common` / engine | 836 | 1144 | 73,1% | 179,4 KB de 252,4 KB (71,1%) |

"Feita" = função `matched` (byte a byte igual ao original) ou `equivalent` (C++ equivalente, ainda sem bater).
Ignoradas: **1338** funções, sendo 1056 de bibliotecas/SDK (`libs`: gcc, newlib, sce, lib989snd, crt0) e 282 de código de hardware do PS2 (`hw`) que o port substitui.
Gerado por `tools/update_readme.py` a partir de `config/status.csv` (veja `tools/progress.py`); total de 5381 funções.
<!-- PROGRESS:END -->

<details>
  <summary>Ver mídia sobre o progresso</summary>

<img width="1024" height="768" alt="image" src="https://github.com/user-attachments/assets/c374d9bd-b484-45e7-88f3-e50b50bb1985" />

https://github.com/user-attachments/assets/758f6b00-cc1d-4ac8-87dd-0591663a8dcf

</details>
