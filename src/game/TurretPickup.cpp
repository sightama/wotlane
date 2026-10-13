#include "common.h"
#include "engine.h"
#include "game/pickup.h"
#include "game/pickup_sound.h"
#include "game/pickup_fx.h"
#include "cs_pool.h"

class TurretPickup : public Pickup {
public:
    enum State { STATE_0, STATE_1, STATE_2, STATE_3, STATE_4 };

    char padE0[0x100 - 0xE0];
    PickupSound sound; /* 0x100 */
    char pad110[0x1B8 - 0x110];
    int state;         /* 0x1B8 */

    int getState(void);
    void takeHit(_fvector *pos, float dmg, int x);
    void drop(void);
    void grab(int i);
    void enterState(State s);
    bool update(void);
    void kill(void);
    void updateBehavior(void);
};

INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", __12TurretPickup);
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", initAfterDbLoad__12TurretPickup);
void TurretPickup::takeHit(_fvector *pos, float dmg, int x)
{
    health -= dmg;
}
void TurretPickup::drop(void)
{
    heldState = 0;
    Pickup::setVisualState(0);
    Pickup::hatCheck();
    hdReparentCsGrid(cs);
}
void TurretPickup::kill(void)
{
    cs->drawMe = 0;
    cs->testCollision = 0;
    particleCreateFx(&cs->trans, 0xB, 8.0f, 0, 0.0f);
    PICKUP_SOUNDS()->playDestructibleSound(0x65, &cs->trans);
    sound.terminateTurretSound();
    flags &= 0xFFFD;
    if ((bits & 1) == 0) {
        CsPool::csDeactivate(cs);
        cs = 0;
    }
}
bool TurretPickup::update(void)
{
    updateBehavior();
    return health > 0.0f;
}
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", fire__12TurretPickupR8_fvector);
void TurretPickup::grab(int i)
{
    Pickup::grab(i);
    sound.terminateTurretSound();
    enterState(STATE_4);
}
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", updateBehavior__12TurretPickup);
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", enterState__12TurretPickupQ212TurretPickup5State);
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", _vt$12TurretPickup);
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", func_001D0FE0);
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", func_001D0FF0);
INCLUDE_ASM("asm/nonmatchings/game/TurretPickup", __tf12TurretPickup);
int TurretPickup::getState(void)
{
    return state;
}
