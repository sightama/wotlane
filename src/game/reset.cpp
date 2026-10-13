#include "common.h"
#include "game/game.h"

/* The game's tweak file reader (common/resetcom) with the level keys it adds. Declared without virtuals here: the
   vtable, the constructor and __tf stay in asm. A key is {int id; int; value...}. */
class resetcom {
public:
    int readKey(char *key);
};
class reset : public resetcom {
public:
    int readKey(char *key);
};
struct ParticleColoring {
    int c[4];
};
extern TheGame *game;
__asm__("#SNFIX_SMALL game");
extern CompositedDustLighting dustLighting;
extern ParticleColoring trailColoring;
extern ParticleColoring fragColoring;
void hdSetDefaultHeight(float h);

INCLUDE_ASM("asm/nonmatchings/game/reset", _vt$5reset);
INCLUDE_ASM("asm/nonmatchings/game/reset", __5reset11CREATOR_KEY);
INCLUDE_ASM("asm/nonmatchings/game/reset", writeKeys__5resetPc);
/* returns the size of the key read, or -1 */
int reset::readKey(char *key)
{
    int size = -1;
    float h;

    switch (*(unsigned *)key) {
    case 0xBBA: /* hat default height */
        h = *(float *)(key + 8);
        game->m_defaultHeight = h;
        hdSetDefaultHeight(h);
        break;
    case 0xBC2:
        dustLighting = *(CompositedDustLighting *)(key + 8);
        size = 0x28;
        break;
    case 0xBC3:
        trailColoring = *(ParticleColoring *)(key + 8);
        size = 0x18;
        break;
    case 0xBC4:
        fragColoring = *(ParticleColoring *)(key + 8);
        size = 0x18;
        break;
    default:
        size = resetcom::readKey(key);
        break;
    }
    return size;
}
INCLUDE_ASM("asm/nonmatchings/game/reset", oldStyleRead__5resetPc);
INCLUDE_ASM("asm/nonmatchings/game/reset", __tf5reset);
