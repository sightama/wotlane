#ifndef MILITARY_PICKUP_H
#define MILITARY_PICKUP_H

#include "game/pickup.h"

class MilitaryFormation;

/* Pickup that moves in formation (tanks, missile trucks derive from it). */
class MilitaryPickup : public Pickup {
public:
    enum State { STATE_0, STATE_1, STATE_2, STATE_3, STATE_4, STATE_5, STATE_6, STATE_7 };

    char padE0[0x170 - 0xE0];
    int focus;                    /* 0x170 */
    int state;                    /* 0x174 */
    MilitaryFormation *formation; /* 0x178 */
    char pad17C[4];
    _fvector formationPos;        /* 0x180 */

    void grab(int i);
    void kill(void);
    void breakFormation(void);
    int getState(void);
    int getFocus(void);
    void leadFormation(void);
    void followFormation(void);
    void resignFormation(void);
    void setFormation(MilitaryFormation *f);
    void setFormationPos(_fvector &p);
    _fvector *getFormationPos(void);
};
typedef char _size_MilitaryPickup[sizeof(MilitaryPickup) == 0x190 ? 1 : -1];

#endif
