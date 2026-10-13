#ifndef LEVEL_PICKUPS_H
#define LEVEL_PICKUPS_H

/* Per-level pickup spawning; everything is static. */
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
    static void throwPickup(PickupIter it, _fvector &dir, DbInteractive *by, DbInteractive *target);
    static void killPickup(PickupIter it, int how);
    static void impalePickup(PickupIter it);
    static bool inFlight(PickupIter it);
    static void grabPickup(PickupIter it, int i);
    static void grabThrownPickup(PickupIter it, int i);
    static void prunePickup(PickupIter it);
    struct Info {
        float health;        /* 0x00: initial health of the pickup type */
        char pad4[0x2C];
        float staminaGain;   /* 0x30 */
        char pad34[0x20];
    };
    static Info s_info[];

    static void *getClosestThrownPickup(_fvector &p, float r);
    static void *getClosestPickup(_fvector &a, _fvector &b);
    static void throwRBPickup(PickupIter it, _fvector &dir, DbInteractive *target);
    static void turnOff2Handeds(void);
    static void turnOffAllVehicles(void);
    static void turnOnMilitary(void);
    static void initMilitaryForVegas(void);
    static void createPickupHighlight(struct _hierhead *ep);
    static void startCinema(void);
    static void endCinema(void);
    static void update(void);
    static void computeHighlight(Monster &m);
    static void initAfterDbLoad(void);
    static void turnOffMilitary(void);
    static void initMilitaryForCentral(void);
};

#endif
