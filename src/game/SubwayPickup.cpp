#include "common.h"
#include "engine.h"
#include "game/pickup.h"
#include "game/pickup_sound.h"

class SubwayPickup : public Pickup {
public:
    char padE0[0x100 - 0xE0];
    PickupSound sound;  /* 0x100 */
    char pad110[0x230 - 0x110];
    _fvector vel;       /* 0x230 */
    char pad240[4];
    int f244;

    _fvector *getVel(void);
    void takeHit(_fvector *pos, float dmg, int x);
    void kill(void);
    void drop(void);
};

INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", _vt$12SubwayPickup);
INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", __12SubwayPickup);
INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", initAfterDbLoad__12SubwayPickup);
void SubwayPickup::takeHit(_fvector *pos, float dmg, int x)
{
    if (dmg > 0.25f)
        health -= dmg;
}
void SubwayPickup::kill(void)
{
    cs->drawMe = 0;
    cs->testCollision = 0;
    Pickup::kill();
    sound.terminatePickupSound();
}
void SubwayPickup::drop(void)
{
    heldState = 0;
    Pickup::setVisualState(0);
    Pickup::hatCheck();
    hdReparentCsGrid(cs);
    f244 = 0;
}
INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", update__12SubwayPickup);
INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", func_001CF870);
INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", func_001CF880);
INCLUDE_ASM("asm/nonmatchings/game/SubwayPickup", __tf12SubwayPickup);
_fvector *SubwayPickup::getVel(void)
{
    return (_fvector *)((char *)this + 0x230);
}
