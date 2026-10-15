#ifndef PICKUP_H
#define PICKUP_H

#include "hieri_types.h"
#include "game/level_pickups.h"
#include "game/pickup_sound.h"
#include "game/pickup_type.h"
#include "game/monster_anim.h"

/* Base of the pickups (cars, military, subway, tanks ...). Virtual in retail (vptr at 0x10, set by the ctor; offset 0 holds a
   constant 5): derived pickups declare their own non-virtual overrides and keep vtable/ctor/__tf as asm. sizeof == 0x170. */
class Pickup {
public:
    int kind;                 /* 0x00: the ctor sets 5 */
    unsigned short flags;     /* 0x04: bit 1 cleared on grab/kill, bit 3 cleared when health runs out */
    char pad6[6];
    _cs *cs;                  /* 0x0C */
    void *vptr;               /* 0x10 */
    PickupIter ref;           /* 0x14: node of LevelPickups' lists */
    char pad18[0x40 - 0x18];
    float maxHealth;          /* 0x40 */
    float speed;              /* 0x44 */
    float health;             /* 0x48 */
    int regenTimer;           /* 0x4C */
    unsigned long long bits;  /* 0x50: bit 0 regenable (cs is shared and must not be deactivated), 1 two-handed, 2 rigid body, 5 dragon head */
    char pad58[0x60 - 0x58];
    float handleMat[3][4];    /* 0x60: rotation of the grab handle (transposed from its hier node) */
    float handle[3];          /* 0x90: grab handle offset (negated node translation) */
    float handleRange;        /* 0x9C: radius of the handle sphere, from the node's db id */
    int pickupType;           /* 0xA0 */
    char padA4[0xB4 - 0xA4];
    float swipeFar;           /* 0xB4 */
    char padB8[0xC4 - 0xB8];
    float swipeNear;          /* 0xC4 */
    char padC8[0xD0 - 0xC8];
    int weapIdx;              /* 0xD0 */
    char padD4[4];
    int heldState;            /* 0xD8: grabber slot; zeroed on drop */
    char padDC[4];

    void kill(void);
    void grab(int i);
    void setVisualState(int s);
    void hatCheck(void);
    void drop(void);
    void regen(void);
    void initAfterDbLoad(void);
    bool isTargetClear(DbInteractive *target);
    void initHandle(_hiertranslate *t);
    void initHandle(_hierrotate *r);
    bool update(void);
    bool regenUpdate(void);
    void initConfig(void);
    _fvector *getVel(void);
    void getVel(_fvector &v);
    PickupIter getRef(void);
    void setRef(PickupIter it);
    float getHealth(void);
    float getSpeed(void);
    int getDbId(void);
    int getPickupType(void);
    int getWeapIdx(void);
    void setWeapIdx(int i);
    int getGrabber(void);
    void setGrabber(int i);
    int isRegenable(void);
    int isTwoHanded(void);
    int isRigidBody(void);
    int isADragonHead(void);
    float getSwipeRange(void);
    float getHandleRange(void);
    int getNumCollisSpheres(void);
    _fvector *getCollisSphereTrans(int i);
    float getCollisSphereRadius(int i);
    void resetHealth(void);
    void takeHit(_fvector *pos, float dmg, int x);
    void takeUseDamage(float dmg);
    int updateGenerator(void);
    int updateGenerator(void *p);
    void updateCS(_fvector *pos, float (&m)[4][4], bool flip);
    void initCollisSpheres(_fvector *p);
    void swipeTrailEffect(void);
    void initLocators(_hierhead *ep);
    int init(_hierhead *ep, _fvector &pos, float (*m)[4], bool regen, ePickupType type);
    int collisTestBatAttack(float a, unsigned byId, MonsterAnim anim, class HitHistory &hist, float b, float c, float d);
};
typedef char _size_Pickup[sizeof(Pickup) == 0xE0 ? 1 : -1];

#endif
