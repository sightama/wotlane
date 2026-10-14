# Checagem VU1 x nativo: estado e próximos passos (2026-10-14)

(Índice em `docs/PROXIMOS_PASSOS.md`. Como a checagem funciona, as armadilhas de método e as medições estão em `NATIVO.md`, seção "Runtime: checagem VU1 x nativo".)

## Onde paramos
- **Implementado:** `WOTM_VU1CHECK=<s>` (runtime, `wotm_vu1check.inc`). A cada `<s>` segundos, num quadro da luta, o VU1 executa a cadeia daquele quadro e cada triângulo do XGKICK é casado com o triângulo nativo do mesmo objeto (`polyPkt`), com tolerância de 1 px.
  - Commit 4066e26.
  - No `tools/lote.ps1` fica ligado por padrão com `-Check 10`; o `resumo.csv` ganha `checagens`, `casados_pior` e `objetos_divergentes`.
  - `WOTM_VU1CHECK_DUMP=<prefixo>` grava os triângulos de cada checagem num CSV para análise offline.
- **Medido:** em central, 1 contra 1, de 98,2% a 99,9% dos triângulos do VU1 casam por quadro, contando os monstros.

## Pistas encontradas (nenhuma confirmada como bug; nada corrigido)
1. **Objetos que o nativo desenha e o jogo não** (`vu 0` na saída): estão inteiros na tela e de frente.
   - Exemplos em central: tex 276 (até 545 triângulos), 234, 247 e 307.
   - Suspeita: escolhas de visibilidade (CONTROL/prédio fantasma, DESTRUCTIBLE, LOD). É a pista mais ligada aos bugs já reportados.
2. **Monstros com 20% a 28% a mais de triângulos no VU1, só com 3 IAs** (`WOTM_MATCH=1,3,2,2,1`).
   - Os vértices são os mesmos (0,04 px), mas ligados de outro jeito, e a diferença cresce no fim de cada sub-pacote.
   - No 1 contra 1 os monstros casam inteiros.
   - Causa possível: a regra de ADC do nativo ou dado velho na memória do VU1 emulado. Precisa de referência do PCSX2 para decidir.
3. **Objetos com tex 66 e várias instâncias** às vezes não casam.

## O que a checagem não cobre hoje
Ela compara só geometria (posição dos triângulos na tela, por objeto). Textura, cor e alfa ficam de fora.
- **Textura errada num objeto:** não pega. Um triângulo no lugar certo com a textura trocada conta como casado.
- **Partículas e pedestres:** o VU1 desenha, mas esses triângulos não vêm da hierarquia de objetos. Caem no contador "sem objeto", que só dá um total por quadro (programas alimentados pela tabela em 0x5B8Axx). Se o nativo não desenhar pedestres, isso não aparece como diferença.
- **HUD:** muito provavelmente não passa pelo VU1. O jogo manda o texto direto ao GS, como as fontes (`fontPacket`); o resto do HUD não foi conferido. A checagem não vê nada disso.
- **Mistura e alfa** (por exemplo, a explosão opaca): fora do alcance.

Resumo: pega objeto que falta, sobra ou está fora do lugar. Não pega o que o nativo nem tenta desenhar quando o jogo desenha fora da hierarquia de objetos.

## Próximos passos (do mais barato ao mais caro)
1. **Textura por triângulo:** o pacote do XGKICK traz o TEX0 de cada seção (e o seletor de slot). Comparar com a textura que o nativo escolheu para o mesmo triângulo (`texId`/slot do `Vtx`) e listar os objetos com textura trocada.
2. **Partículas por sistema:** atribuir os triângulos "sem objeto" ao sistema de `fx[]` que os gerou, pela origem dos trechos da cadeia, e comparar contagem e posição com o que `Scene::fx` e os sprites nativos desenham de cada sistema. Pedestres são o tipo visual 14.
3. **HUD pelo caminho direto ao GS:** pôr um tap também nos pacotes GIF que vão direto ao GS (PATH3/PATH2, fora do VU1) e comparar com o que o nativo desenha por cima da cena (texto, ícones). Antes, confirmar por onde o HUD sai.
4. **Investigar as pistas 1 a 3 acima** (pedir ao usuário qual atacar e não corrigir sem pedir).

## Problema antigo encontrado no caminho
O caminho emulado inteiro (sem `PS2X_NO_VU1`) está quebrado. A luta trava no primeiro quadro (laço do `particleDraw`) e, contornado isso, cai em `GS::consumeLocalToHostBytes` (leitura de VRAM do `sceGsExecStoreImage`, chamado a cada quadro de 0x2153FC). A checagem não depende desse caminho. O método para localizar um `[CRASH] exe+0x…` está em `NATIVO.md`.
