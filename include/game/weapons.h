#ifndef WEAPONS_H
#define WEAPONS_H

/* game/Weapon: projectiles (thrown pickups, monster specials, military fire, lava balls) and the ground fires they leave. */

#include "hieri_types.h"
#include "game/level_pickups.h"
#include "game/sound_objects.h"

struct _hdResult;
class Monster;

class Weapon {
public:
    enum WeaponType { };
    /* Per-type tuning (Weapons::m_params, filled by InitWeaponsAfter); InitWeapon copies it to the start of the Weapon, then
       overrides it from the thrown pickup or the firing monster. */
    class Dynamics {
    public:
        float r;            /* 0x00: point light color (128 gray by default) */
        float g;            /* 0x04 */
        float b;            /* 0x08 */
        float lightRange;   /* 0x0C: 0 = no point light */
        float damage;       /* 0x10: handed to Interactives::dispatchHit */
        float lifetime;     /* 0x14: seconds in the table, fields (x60) once copied to a Weapon */
        float f18;          /* 0x18 */
        float f1C;          /* 0x1C: > 1 widens the angle limits below (InitWeapon) */
        float speed;        /* 0x20: miles per hour (x 5280 / 3600 for units per second) */
        float radius;       /* 0x24: blast radius of DispatchRangeBasedDamage */
        float f28;          /* 0x28 */
        float angle2C;      /* 0x2C: pi/4 */
        float angle30;      /* 0x30: 75 degrees */
        int pad34[3];

        Dynamics(void);
    };

    Dynamics m_dyn;         /* 0x00 */
    int m_active;           /* 0x40 */
    int m_age;              /* 0x44: fields alive; detonates or dies once past m_dyn.lifetime */
    int m_48;               /* 0x48 */
    int m_type;             /* 0x4C: WeaponType; 0x17 = a thrown pickup (its cs belongs to the pickup) */
    int m_50;
    int m_54;               /* 0x54 */
    int m_58;
    int m_5C;               /* 0x5C */
    int m_60;               /* 0x60 */
    WeaponSound m_sound;    /* 0x64 */
    float m_startZ;         /* 0x6C: ballistic arc (CalculateBallisticArc): launch height */
    float m_vz;             /* 0x70: initial vertical speed */
    float m_gravity;        /* 0x74: z = startZ + vz t + gravity t^2 */
    int m_launchTics;       /* 0x78: timerGetCurTics() at launch */
    float m_maxRange;       /* 0x7C: beyond it the gravity is scaled down */
    _fvector m_80;          /* 0x80 */
    FVECTOR m_prevPos;      /* 0x90: cs translation before this frame's move (GetVel) */
    _fvector m_A0;          /* 0xA0 */
    _fvector m_B0;          /* 0xB0 */
    char padC0[0x120 - 0xC0];
    int m_ownerId;          /* 0x120: interactive id of the thrower */
    int m_targetId;         /* 0x124: interactive id of the target (0 = none) */
    _fvector *m_targetPoint; /* 0x128: aimed-at point when there is no target */
    int pad12C;
    FVECTOR m_targetPos;    /* 0x130 */
    char pad140[0x150 - 0x140];
    _cs *m_cs;              /* 0x150 */
    PickupIter m_pickup;    /* 0x154: the thrown pickup (type 0x17) */
    int m_fx;               /* 0x158: particle fx handle */
    int m_fx2;              /* 0x15C */
    int m_pointLight;       /* 0x160: index in pointLights, -1 = none */
    int m_164;              /* 0x164 */
    int pad168[2];

    void UpdateRotation(void);
    void UpdateLockOnTarget(void);
    void UpdateLockAheadOfMonster(void);
    void UpdateLockVertical(void);
    void UpdateErraticity(void);
    void UpdateTranslation(void);
    void WeaponHD(void);
    void setCsTransform(_fvector *pos, float (&m)[4][4]);
    void ComputeReflection(_fvector *a, _fvector *b, bool c);
    void CalculateBallisticArc(void);
    void InitWeapon(void);
    void KillWeapon(void);
    void DetonateWeapon(_hdResult &res);
    void DispatchRangeBasedDamage(void);
    void GetVel(_fvector &v);
};
typedef char _size_Weapon[sizeof(Weapon) == 0x170 ? 1 : -1];
typedef char _size_WeaponDynamics[sizeof(Weapon::Dynamics) == 0x40 ? 1 : -1];

/* A ground fire (Weapons::CreateFire): burns the monsters inside its radius for 1800 fields. */
struct WeaponFlame {
    GenericSound sound;     /* 0x00 */
    int active;             /* 0x14 */
    int age;                /* 0x18: fields */
    int light;              /* 0x1C: index in pointLights, -1 = none */
    float radius;           /* 0x20 */
    int fx;                 /* 0x24: particle fx handle */
    int pad28[2];
    FVECTOR pos;            /* 0x30: follows the flame particle's height */
    int ownerId;            /* 0x40: interactive id credited with the burns */
    int pad44[3];
};
typedef char _size_WeaponFlame[sizeof(WeaponFlame) == 0x50 ? 1 : -1];

/* Projectile spawner embedded in TheGame at 0x112490, see gameWeapons(). */
class Weapons {
public:
    _hierhead *m_epNodes[45];           /* 0x0000: model of each weapon type (AddWeaponEpNode) */
    int padB4[3];
    Weapon::Dynamics m_params[45];      /* 0x00C0 */
    Weapon m_weapons[100];              /* 0x0C00 */
    WeaponFlame m_flames[10];           /* 0x9BC0 */

    void InitWeaponsBefore(void);
    void InitWeaponsAfter(void);
    void UpdateWeapons(void);
    /* nonzero when a live weapon is within r of pos; it is stored in *out (FinalBoss::updateJelly) */
    int WeaponInRadius(_fvector *pos, float r, Weapon **out);
    /* weapon slot that carries a thrown non-rigid pickup; ids are the thrower's and the target's interactive ids (0 if none) */
    int CreateStaticWeapon(PickupIter it, int byId, int targetId, _fvector *rot);
    void CreateLavaBall(_fvector *pos, _fvector *dir, _fvector *target, int a, int b);
    void DecrementWeaponCount(Monster *m, int type);
    void AddWeaponEpNode(int idx, _hierhead *h);
    void CreateFire(_fvector *pos, float r, int id);
    void UpdateFlames(void);
};
typedef char _size_Weapons[sizeof(Weapons) == 0x9EE0 ? 1 : -1];

#endif
