# Visualizador de modelos 3D no navegador

(Índice dos docs no `CLAUDE.md`.) O viewer é um projeto à parte: https://yanm1103.github.io/wotm-model-museum/. A avaliação de
viabilidade que o originou está em `bbed7d9:docs/passos/VIEWER_WEB.md` (ler com `git show`).

## Regras
- **Nenhum arquivo do jogo no servidor.** Página estática que lê a pasta `disc/` (ou a ISO) do usuário pela File API; tudo é
  processado localmente. A hospedagem (GitHub Pages) não contém nada do jogo.

## O que ele usa deste repo
- **Formato** (`docs/FORMATOS.md`, `NATIVO.md`): `.NGP` (zip `IE\3\4` na fase, cru no monstro; imagem pré-ligada em
  `0xA00000`; hierarquia GROUP/LOD/SWITCH/ROTATE/TRANSLATE/SCALE/CHAR_INSTANCE/DESTRUCTIBLE/CONTROL), malha do `polyPkt` (posições,
  normais, strips com ADC, cor por vértice, UV 4.12, multitextura por seção, lotes de 190 vértices), texturas CT32/24/16, T8, T4,
  T8H/4HL/4HH com paletas do `.RTX` (troca CSM1), skinning dos monstros (código de osso nos bits baixos de xyz, paleta de matrizes,
  3 pesos) e curvas de animação + mixer.
- **Monstros do disco** (pose e texturas sem dump da RAM): `passos/MODELOS_3D.md`, `tools/pose.py`, `native/wotm_model.hpp`.
- **Cenários e objetos:** `tools/ngp.py` (`ngp.decode_mesh`) e `tools/gltf.py` (glTF binário, `FERRAMENTAS.md`).
- `native/viewer.cpp` (raylib) é o visualizador local com câmera livre; ele parte de um **dump da RAM** (texturas por `texInfo` e
  matrizes de osso calculadas pelo jogo).
