#ifndef PICKUP_SOUND_H
#define PICKUP_SOUND_H

class Pickup;

/* Looping engine/turret sound owned by a pickup; embedded at 0x100 in the pickups that have one (game/Sound). */
class PickupSound {
public:
    Pickup *m_pickup;           // 0x0
    unsigned m_handle;          // 0x4: engine loop
    char pad8[4];
    unsigned m_turretHandle;    // 0xC

    void initPickupSound(Pickup *p);
    void playBatImpactSound(void);
    void updatePickupSound(void);
    void updateTurretSound(int type, float a, float b);
    void terminateTurretSound(void);
    void terminatePickupSound(void);
    void playTurretPickupFireSound(int type);
};
typedef char _size_PickupSound[sizeof(PickupSound) == 0x10 ? 1 : -1];

#endif
