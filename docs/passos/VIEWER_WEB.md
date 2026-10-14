# Visualizador de modelos 3D no navegador (ideia, avaliada em 2026-10-14)

(Índice em `docs/PROXIMOS_PASSOS.md`.) Só a avaliação de viabilidade; nada foi implementado ainda.

## O que já existe
- **Formato resolvido** (`docs/FORMATOS.md`, `NATIVO.md`): `.NGP` (zip `IE\3\4` na fase, cru no monstro; imagem pré-ligada em
  `0xA00000`; hierarquia GROUP/LOD/SWITCH/ROTATE/TRANSLATE/SCALE/CHAR_INSTANCE/DESTRUCTIBLE/CONTROL), malha do `polyPkt` (posições,
  normais, strips com ADC, cor por vértice, UV 4.12, multitextura por seção, lotes de 190 vértices), texturas CT32/24/16, T8, T4,
  T8H/4HL/4HH com paletas do `.RTX` (troca CSM1), skinning dos monstros (código de osso nos bits baixos de xyz, paleta de matrizes,
  3 pesos) e curvas de animação + mixer.
- **Código:** `tools/ngp.py` lê malhas direto do disco (Python; ainda usa o modelo antigo de ADC); `native/viewer.cpp` (raylib) é um
  visualizador com câmera livre, mas parte de **dump da RAM** (texturas por `texInfo` e matrizes de osso calculadas pelo jogo).

## O que falta para ler só do disco
1. **Texturas sem dump:** ler os pacotes de pixels do `.TEX` e aplicar o `.PTR` (`TBP0`, ids de textura), em vez de `texInfo` na RAM. Moderado.
2. **Pose dos monstros:** hoje as matrizes vêm da RAM. Opções: avaliar as curvas offline e montar as matrizes dos `ANIM_XFORM`/`SKEL_BONE`
   (o formato é conhecido; a montagem fora do jogo não) — de bônus vira um player de animações; ou aceitar um dump como atalho.
3. Cenários são quase todos rígidos: `ngp.py` já extrai (corrigir o ADC: pula só o triângulo, a tira continua).

## Navegador
- Regra 1: **nenhum arquivo do jogo no servidor.** Página estática que lê a pasta `disc/` (ou a ISO) do usuário pela File API;
  tudo processado localmente. Pode ficar hospedada (ex.: GitHub Pages) sem conter nada do jogo.
- **Caminho recomendado:** JS/TS + three.js, portando `ngp.py` + `wotm_gs.hpp`. Alternativa: Emscripten do `wotm_native.hpp`, mas ele
  depende da RAM do EE e teria de ser adaptado do mesmo jeito.

## Estimativa e ordem sugerida
| Etapa | Dificuldade |
|---|---|
| Cenários/objetos rígidos com textura | fácil a moderado (portar) |
| Monstros em pose fixa a partir de dump | fácil (o nativo já faz) |
| Monstros do disco com animação | moderado a difícil (esqueleto + curvas offline) |
| Exportar glTF/OBJ (útil para mods) | pouco extra depois do leitor |

Ordem: viewer web de fases/objetos primeiro; depois monstros com animação.
