#ifndef LEVEL_PICKUPS_H
#define LEVEL_PICKUPS_H

/* Per-level pickup spawning; everything is static. */
#include "game/pickup_type.h"

class Monster;
class Pickup;
class DbInteractive;
struct _fvector;
template <class T>
class LinkedList {
public:
    class Iterator {
    public:
        T cur;
    };
};
typedef LinkedList<Pickup *>::Iterator PickupIter;
class LevelPickups {
public:
    static void dropPickup(PickupIter it);
    static void throwPickup(PickupIter it, _fvector &at, DbInteractive *by, DbInteractive *target);
    static void deflectThrownPickup(PickupIter it, _fvector &dir, DbInteractive *by, DbInteractive *target);
    static void killPickup(PickupIter it, int how);
    static void impalePickup(PickupIter it);
    static bool inFlight(PickupIter it);
    static void grabPickup(PickupIter it, int i);
    static void grabThrownPickup(PickupIter it, int i);
    static void prunePickup(PickupIter it);
    struct Info {
        float health;        /* 0x00: initial health of the pickup type */
        float heldHealth;    /* 0x04: health while held, if non-zero */
        /* the rest is the thrown pickup's projectile (Weapon::InitWeapon, scaled by the thrower's m_pu*Mod) */
        float damage;        /* 0x08 */
        float radius;        /* 0x0C: collision radius of a thrown one (Weapon::WeaponHD) */
        char pad10[0x18 - 0x10];
        float useDamage;     /* 0x18: what a thrown rigid one (type 7) loses when it impales a monster (Weapon::DetonateWeapon) */
        char pad1C[0x2C - 0x1C];
        float duration;      /* 0x2C: seconds */
        float staminaGain;   /* 0x30 */
        float speed;         /* 0x34: mph */
        float gravity;       /* 0x38 */
        float maxRange;      /* 0x3C */
        float turnRate;      /* 0x40 */
        float pitchRate;     /* 0x44 */
        float headingBreak;  /* 0x48 */
        float pitchBreak;    /* 0x4C */
        char pad50[4];
    };
    typedef char _size_Info[sizeof(Info) == 0x54 ? 1 : -1];
    static Info s_info[];

    static void *getClosestThrownPickup(_fvector &p, float r);
    static void *getClosestPickup(_fvector &a, _fvector &b);
    static void throwRBPickup(PickupIter it, _fvector &dir, DbInteractive *target);
    static void turnOff2Handeds(void);
    static void turnOffAllVehicles(void);
    static void turnOnMilitary(void);
    static void initMilitaryForVegas(void);
    static void hatCheck(_fvector *p, float r2);
    static void turnOffForDelayedHat(_fvector *p, float r2);
    static void enableHighlights(int i);
    static void createPickupHighlight(struct _hierhead *ep);
    static void startCinema(void);
    static void endCinema(void);
    static void update(void);
    static void computeHighlight(Monster &m);
    static void initAfterDbLoad(void);
    static void turnOffMilitary(void);
    static void initMilitaryForCentral(void);
    static void init(void);
    static void *createPickup(struct _hierhead *ep, _fvector &pos, float (*m)[4], bool regen, ePickupType type);
};

#endif
