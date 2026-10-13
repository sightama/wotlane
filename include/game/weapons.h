#ifndef WEAPONS_H
#define WEAPONS_H

#include "hieri_types.h"
#include "game/level_pickups.h"

/* Projectile spawner embedded in TheGame at 0x112490, see gameWeapons(). */
class Weapons {
public:
    void UpdateWeapons(void);
    void InitWeaponsAfter(void);
    /* weapon slot that carries a thrown non-rigid pickup; ids are the thrower's and the target's interactive ids (0 if none) */
    int CreateStaticWeapon(PickupIter it, int byId, int targetId, _fvector *rot);
    void CreateLavaBall(_fvector *pos, _fvector *dir, _fvector *target, int a, int b);
};

#endif
