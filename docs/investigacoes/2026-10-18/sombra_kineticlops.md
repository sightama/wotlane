# Sombra do corpo inteiro do Kineticlops (bug 3 dos "Bugs vistos jogando")

Foto `2026-10-18_005814`/001 (CENTRAL, Kineticlops, `g_frame` 3885). No retail só o olho projeta sombra; o nativo projeta a do
corpo translúcido inteiro, que ainda escurece o próprio corpo.

## Teoria (confiança alta, ~95%)

O nativo não respeita o bit **`dontCastShadow`** do `HierObject` (bit 29 da palavra de bits em `+0xC`, ou seja, `0x2000` do
u16 em `+0xE`). No retail, o objeto com esse bit é pulado na montagem da silhueta do bloqueador; no Kineticlops todas as
malhas do corpo têm o bit e só as do olho não.

## Evidência

1. **Struct:** `include/hieri_types.h:421`, `dontCastShadow : 1` em `0x3.5` dos `bits` (`+0xC`), ao lado de `dontShadowMe`
   (`:413`, `0x200`, o que o nativo já usa para o receptor).
2. **Retail, quem entra na silhueta:** `hierShadow` (`src/common/hier.cpp:843-874`) liga `tracingShadow` e percorre o `cs` de
   cada bloqueador com o `hierTraverseAsm`. Com `tracingShadow` ligado, todo OBJECT visível vai para o `psBldBlkRObjs`
   (`asm/nonmatchings/common/texm/hierTraverseAsm.s`, a chamada em 0x2075C8, que pula o desenho normal) em vez do
   `psObjectCull`/`pktAddVu1ObjAsm`. A primeira coisa do `psBldBlkRObjs` (`asm/nonmatchings/common/ps/psBldBlkRObjs.s`,
   0x208D5C..0x208D70) é ler a palavra dupla em `+8` do nó e sair sem emitir nada se o bit 61 estiver ligado, isto é, o bit
   29 da palavra em `+0xC`: exatamente o `dontCastShadow`. Não há outro critério de material/alfa: só esse bit (mais o
   `cs->drawMe`, o bit da vista e o teste de FOV que o nativo já imita).
3. **Dados do disco** (script `C:\Users\TwistZero\.claude\jobs\ab2fc446\tmp\ag3\scan_obj.py`, varre os OBJECT do `.NGP`
   com `tools/ngp.py`):
   - `MON/ENERGY0.NGP` (Kineticlops): 70 OBJECTs, 66 com `dontCastShadow`. Os 4 **sem** o bit são as malhas do olho
     (`tex 5`, 42 a 152 vértices, raio ~20, flags 0x0211/0x0311). Todas as malhas do corpo (`tex 6`, 426 a 1.856 vértices,
     raio ~78, flags 0x8203/0x8A01 = OT de alfa) **têm** o bit, assim como os efeitos.
   - `MON/MONKEY0.NGP` (Congar), controle: as 4 malhas do corpo (`tex 3`, flags 0x2310) **não** têm o bit (projetam), e os
     efeitos (`tex 6`, alphaSort) têm. Mesmo padrão em `ROBOT0`/`LIZARD0`. Ou seja, o bit é usado de propósito pelos artistas.
4. **Nativo:** `native/wotm_native.hpp:1097-1122` (`Scene::collectShadows`) põe em `b.casters` todos os itens que o
   `drawCs` devolve para o `cs` (e o `cs2`), sem olhar `+0xC`; o `shadowTexture` (`:1244-1268`) rasteriza todos com
   cobertura 1. O corpo inteiro vira silhueta.
5. **"Escurece o próprio corpo":** o corpo não recebe a sombra como receptor (está em `casterNodes` e tem `dontShadowMe`,
   `:1161-1164`), mas a sombra no chão sai depois da OT de alfa (`:2170-2171`) e o corpo, translúcido e na OT de alfa, não
   tapa o chão no Z: a mancha escura do chão aparece por cima do corpo. Some junto com a correção (no retail a sombra do
   olho é pequena e fica sob o olho).

## Próximo passo concreto

Em `collectShadows`, depois de `b.casters.swap(items)` (`native/wotm_native.hpp:1116`), descartar os itens com
`ram.u32(it.node + 0xC) & 0x20000000` (`dontCastShadow`) antes de montar `casterNodes`. Opcional: montar `casterNodes` com
todos os itens do `cs` (antes do filtro) para manter a exclusão do próprio monstro como receptor; na prática todas as
malhas de monstro têm `dontShadowMe` (0x200), que já os exclui. Pôr um nome em vez do número (por exemplo
`kObjDontCastShadow`), comentário citando `psBldBlkRObjs`, e atualizar a linha "Sombras projetadas" do `NATIVO.md`.
Conferir: replay da foto (`tools\vu1cmp.ps1` na sessão `2026-10-18_005814`) sem a sombra do corpo, e `corpus.ps1 -Check`
para ver que as sombras dos outros monstros (Congar etc.) não mudam. Fechar o item 3 no `VU1CHECK.md` depois que o dono
conferir ao vivo.

Não rodei o `vu1cmp` nesta investigação: a prova vem do código do retail e das flags no disco.
