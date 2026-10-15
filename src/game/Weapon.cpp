#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "engine.h"
#include "cs_pool.h"
#include "vecmath.h"
#include "game/level_pickups.h"

/* game/Weapon: projectiles (thrown pickups, monster specials, military fire, lava balls) and the ground fires they leave.
   Weapons is embedded in TheGame at 0x112490 (gameWeapons()). The classes stay local to this TU: FinalBoss, Pickup and Sound
   declare their own partial Weapon, and game.h pulls in game/weapons.h everywhere. */

class TheGame;
extern TheGame *game;
#define GAME_FIELD(type, off) (*(type *)((char *)game + (off)))

float mathfManhatDist2D(_fvector *a, _fvector *b);
int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);
void lightSetAmbient(int i, float r, float g, float b);

/* game/Sound's classes, as declared in game/sound.h (which needs game.h) */
class Weapon;
class WeaponSound {
public:
    unsigned m_handle;          /* 0x0: flight loop */
    float m_prevDist;           /* 0x4: for the doppler pitch */
    void initWeaponSound(void);
    void terminateWeaponSound(void);
    void updateWeaponSound(Weapon *w);
    void playWeaponLaunchSound(_fvector *pos, int type);
    void playWeaponExplosion(_fvector *pos, int type, int hit);
    void playRockSpecialImpact(_fvector *pos);
};
class GenericSound {
public:
    unsigned m_handle;          /* 0x0 */
    int m_bank;                 /* 0x4 */
    int m_sound;                /* 0x8 */
    int m_minDist;              /* 0xC */
    int m_maxDist;              /* 0x10 */
    void initGenericSound(int bank, int sound, int minDist, int maxDist);
    void terminateGenericSound(void);
    void updateGenericSound(_fvector *pos);
};

/* One of the 32 dynamic point lights (common/PointLight). */
class PointLight {
public:
    int active;             /* 0x00 */
    char pad4[0x34];
    int f38;                /* 0x38: lit this frame */
    int f3C;

    void SetPos(_fvector *p);
    void SetRange(float rng);
    void SetColor(float cr, float cg, float cb);
};
class PointLights {
public:
    PointLight lights[32];
    int Create(void);
};
extern PointLights pointLights;

class Weapon {
public:
    enum WeaponType { };
    /* Per-type tuning (Weapons::m_params, filled by InitWeaponsAfter); InitWeapon copies it to the start of the Weapon. */
    class Dynamics {
    public:
        float r;            /* 0x00: point light color (128 gray by default) */
        float g;            /* 0x04 */
        float b;            /* 0x08 */
        float lightRange;   /* 0x0C: 0 = no point light */
        float f10;          /* 0x10 */
        float lifetime;     /* 0x14: seconds in the table, fields (x60) once copied to a Weapon */
        float f18;          /* 0x18 */
        float f1C;          /* 0x1C: > 1 widens the angle limits below (InitWeapon) */
        float f20;          /* 0x20 */
        float f24;          /* 0x24 */
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
    char pad6C[0x80 - 0x6C];
    _fvector m_80;          /* 0x80 */
    FVECTOR m_prevPos;      /* 0x90: cs translation before this frame's move (GetVel) */
    _fvector m_A0;          /* 0xA0 */
    _fvector m_B0;          /* 0xB0 */
    char padC0[0x120 - 0xC0];
    int m_ownerId;          /* 0x120: interactive id of the thrower */
    int m_targetId;         /* 0x124: interactive id of the target (0 = none) */
    int m_128;              /* 0x128 */
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

/* A ground fire (CreateFire, UpdateFlames). */
struct WeaponFlame {
    GenericSound sound;     /* 0x00 */
    int f14;                /* 0x14 */
    int f18;                /* 0x18 */
    int fx;                 /* 0x1C: particle fx handle, -1 = none */
    int f20;                /* 0x20 */
    int f24;                /* 0x24 */
    int pad28[2];
    _fvector pos;           /* 0x30 */
    int pad40[4];
};
typedef char _size_WeaponFlame[sizeof(WeaponFlame) == 0x50 ? 1 : -1];

class Monster;
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
    int WeaponInRadius(_fvector *pos, float r, Weapon **out);
    void DecrementWeaponCount(Monster *m, int type);
    void AddWeaponEpNode(int idx, _hierhead *h);
    void CreateFire(_fvector *pos, float r, int id);
    void UpdateFlames(void);
};
typedef char _size_Weapons[sizeof(Weapons) == 0x9EE0 ? 1 : -1];

/* vecSub in noreorder mode: GNU as would otherwise move the sqc2 into the delay slot of the call that follows */
static inline void vecSubNR(_fvector *dst, _fvector *a, _fvector *b)
{
    __asm__ volatile(".set push
	.set noreorder
	"
                     "lqc2 $vf11, 0x0(%1)
	"
                     "lqc2 $vf12, 0x0(%2)
	"
                     "vsub.xyz $vf11, $vf11, $vf12
	"
                     "sqc2 $vf11, %0
	"
                     ".set pop"
                     : "=m"(*dst) : "r"(a), "r"(b));
}

Weapon::Dynamics::Dynamics(void)
{
    b = g = r = 128.0f;
    angle2C = 0.7853982f;
    angle30 = 1.3089969f;
    lightRange = 0;
}
void Weapons::InitWeaponsBefore(void)
{
    int i;

    for (i = 44; i >= 0; i--)
        m_epNodes[i] = 0;
    for (i = 0; i < 100; i++) {
        Weapon *w = &m_weapons[i];
        w->m_active = 0;
        w->m_dyn.f10 = 0;
        w->m_dyn.lifetime = 0;
        w->m_dyn.f18 = 0;
        w->m_dyn.f1C = 0;
        w->m_dyn.f20 = 0;
        w->m_48 = 0;
        w->m_128 = 0;
        w->m_type = 0;
        w->m_80.x = 0;
        w->m_80.y = 0;
        w->m_80.z = 0;
        w->m_54 = 0;
        w->m_5C = 0;
        w->m_cs = 0;
        w->m_A0.x = 0;
        w->m_A0.y = 0;
        w->m_A0.z = 0;
        _fvector *v = &w->m_B0;
        v->x = 0;
        v->y = 0;
        v->z = 0;
        w->m_ownerId = 0;
        w->m_targetId = 0;
        w->m_pointLight = -1;
        w->m_164 = 0;
    }
    for (i = 0; i < 10; i++) {
        WeaponFlame *f = &m_flames[i];
        f->f14 = 0;
        f->f18 = 0;
        f->f20 = 0;
        f->f24 = 0;
        f->fx = -1;
        _fvector *v = &f->pos;
        v->x = 0;
        v->y = 0;
        v->z = 0;
    }
}
/* Per-type tuning. The defaults are set for every type first; some types then set them again with the same values. */
#ifdef NON_MATCHING
/* equivalent: 58/284 words, same size; only the scheduling of the ~130 table stores differs */
void Weapons::InitWeaponsAfter(void)
{
    int i;
    Weapon::Dynamics *red = &m_params[6];
    Weapon::Dynamics *green = &m_params[2];
    Weapon::Dynamics *none = &m_params[4];
    Weapon::Dynamics *gray = &m_params[0];
    Weapon::Dynamics *blue = &m_params[8];

    for (i = 0; i < 100; i++)
        m_weapons[i].m_sound.initWeaponSound();
    for (i = 0; i < 10; i++)
        m_flames[i].sound.initGenericSound(0x15, 5, 500, 1500);
    lightSetAmbient(2, 0, 0, 255.0f);
    for (i = 0; i < 45; i++) {
        Weapon::Dynamics *d = &m_params[i];
        d->f10 = 1.0f;
        d->lifetime = 3.0f;
        d->f18 = 1.0f;
        d->f1C = 0;
        d->f20 = 800.0f;
    }
    red->r = 96.0f;
    red->g = 32.0f;
    red->b = 32.0f;
    red->lightRange = 400.0f;
    green->r = 32.0f;
    green->g = 96.0f;
    green->b = 32.0f;
    green->lightRange = 400.0f;
    none->r = 0;
    none->g = 0;
    none->b = 0;
    none->lightRange = 0;
    gray->r = 64.0f;
    gray->g = 64.0f;
    gray->b = 64.0f;
    gray->lightRange = 400.0f;
    blue->r = 32.0f;
    blue->g = 0;
    blue->b = 96.0f;
    blue->lightRange = 400.0f;

    m_params[13].f10 = 15.0f;
    m_params[13].lifetime = 10.0f;
    m_params[13].f18 = 1.0f;
    m_params[13].f1C = 0.01f;
    m_params[13].f20 = 700.0f;

    m_params[21].f10 = 1.0f;
    m_params[21].lifetime = 8.0f;
    m_params[21].f18 = 1.0f;
    m_params[21].f1C = 0.02f;
    m_params[21].f20 = 500.0f;

    m_params[22].f10 = 1.0f;
    m_params[22].lifetime = 8.0f;
    m_params[22].f18 = 1.0f;
    m_params[22].f1C = 0.02f;
    m_params[22].f20 = 500.0f;

    m_params[23].f10 = 5.0f;
    m_params[23].lifetime = 10.0f;
    m_params[23].f18 = 1.0f;
    m_params[23].f1C = 0;
    m_params[23].f20 = 800.0f;

    m_params[24].f10 = 1.0f;
    m_params[24].lifetime = 3.0f;
    m_params[24].f18 = 1.0f;
    m_params[24].f1C = 0;
    m_params[24].f20 = 800.0f;

    m_params[25].f10 = 3.0f;
    m_params[25].lifetime = 2.0f;
    m_params[25].f18 = 1.0f;
    m_params[25].f1C = 0;
    m_params[25].f20 = 1400.0f;

    m_params[26].f10 = 8.0f;
    m_params[26].lifetime = 5.0f;
    m_params[26].f18 = 0.05f;
    m_params[26].f1C = 0.08f;
    m_params[26].f20 = 700.0f;

    m_params[27].f10 = 8.0f;
    m_params[27].lifetime = 5.0f;
    m_params[27].f18 = 0.05f;
    m_params[27].f1C = 1.5f;
    m_params[27].f20 = 700.0f;
    m_params[27].f28 = 1.75f;

    m_params[28].f10 = 3.0f;
    m_params[28].lifetime = 2.0f;
    m_params[28].f18 = 1.0f;
    m_params[28].f1C = 0;
    m_params[28].f20 = 1400.0f;

    m_params[29].f10 = 3.0f;
    m_params[29].lifetime = 2.0f;
    m_params[29].f18 = 1.0f;
    m_params[29].f1C = 0.2f;
    m_params[29].f20 = 1400.0f;
    m_params[29].f28 = 0.4f;

    m_params[30].f10 = 1.0f;
    m_params[30].lifetime = 2.0f;
    m_params[30].f18 = 1.0f;
    m_params[30].f1C = 0;
    m_params[30].f20 = 800.0f;

    m_params[31].f10 = 1.0f;
    m_params[31].lifetime = 2.0f;
    m_params[31].f18 = 1.0f;
    m_params[31].f1C = 0.2f;
    m_params[31].f20 = 800.0f;
    m_params[31].f28 = 0.4f;

    m_params[32].f10 = 3.0f;
    m_params[32].lifetime = 2.0f;
    m_params[32].f18 = 1.0f;
    m_params[32].f1C = 0;
    m_params[32].f20 = 1400.0f;

    m_params[33].f10 = 3.0f;
    m_params[33].lifetime = 2.0f;
    m_params[33].f18 = 1.0f;
    m_params[33].f1C = 0;
    m_params[33].f20 = 1400.0f;

    m_params[34].f10 = 3.0f;
    m_params[34].lifetime = 2.0f;
    m_params[34].f18 = 1.0f;
    m_params[34].f1C = 0;
    m_params[34].f20 = 1400.0f;

    m_params[35].f10 = 3.0f;
    m_params[35].lifetime = 2.0f;
    m_params[35].f18 = 1.0f;
    m_params[35].f1C = 0;
    m_params[35].f20 = 1400.0f;

    m_params[36].f10 = 1.0f;
    m_params[36].lifetime = 2.0f;
    m_params[36].f18 = 1.0f;
    m_params[36].f1C = 0;
    m_params[36].f20 = 1000.0f;

    m_params[37].f10 = 1.0f;
    m_params[37].lifetime = 2.0f;
    m_params[37].f18 = 1.0f;
    m_params[37].f1C = 0;
    m_params[37].f20 = 1000.0f;

    m_params[38].f10 = 1.0f;
    m_params[38].lifetime = 2.0f;
    m_params[38].f18 = 1.0f;
    m_params[38].f1C = 0.07f;
    m_params[38].f20 = 1200.0f;

    m_params[42].f10 = 30.0f;
    m_params[42].lifetime = 0.05f;
    m_params[42].f18 = 1.0f;
    m_params[42].f1C = 0;
    m_params[42].f20 = 400.0f;

    m_params[43].f10 = 10.0f;
    m_params[43].lifetime = 1.2f;
    m_params[43].f18 = 1.0f;
    m_params[43].f1C = 0;
    m_params[43].f20 = 250.0f;

    /* faster in match mode */
    m_params[44].f10 = (float)GAME_FIELD(int, 0x1203CC) + 1.75f;
    m_params[44].lifetime = 4.0f;
    m_params[44].f18 = 1.0f;
    m_params[44].f1C = 0.04f;
    m_params[44].f20 = 800.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", InitWeaponsAfter__7Weapons);
#endif
void Weapons::UpdateWeapons(void)
{
    int i;

    for (i = 0; i < 100; i++) {
        Weapon *w = &m_weapons[i];
        w->m_sound.updateWeaponSound(w);
        if (!w->m_active)
            continue;
        w->UpdateRotation();
        w->UpdateTranslation();
        w->WeaponHD();
        w->m_age += timerGetFieldsLastFrame();
        if (w->m_dyn.lifetime < (float)w->m_age) {
            if ((unsigned)(w->m_type - 0x2A) < 3) {
                /* these explode where they are when they run out */
                _hdResult res;
                res.hit = 0;
                /* the ids are bitfields of a 64-bit word in retail's _hdResult */
                *(unsigned long *)&res &= ~(0x3FFFUL << 43);
                *(unsigned long *)&res &= ~(0x7FFUL << 32);
                res.point.x = w->m_cs->trans.x;
                res.point.y = w->m_cs->trans.y;
                res.point.z = w->m_cs->trans.z;
                w->DetonateWeapon(res);
            } else {
                particleCreateFx(&w->m_cs->trans, 0x17, 1.0f, 0, 0.0f);
                particleCreateFx(&w->m_cs->trans, 0x20, 4.0f, 0, 0.0f);
            }
            w->KillWeapon();
        }
    }
    UpdateFlames();
}
/* First live weapon within r of pos (2D Manhattan distance); stored in *out when out is non-null. */
int Weapons::WeaponInRadius(_fvector *pos, float r, Weapon **out)
{
    int i;

    for (i = 0; i < 100; i++) {
        if (m_weapons[i].m_active && mathfManhatDist2D(pos, &m_weapons[i].m_cs->trans) < r) {
            if (out)
                *out = &m_weapons[i];
            return 1;
        }
    }
    return 0;
}
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateRotation__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateLockOnTarget__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateLockAheadOfMonster__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateLockVertical__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateErraticity__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateTranslation__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", WeaponHD__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", D_006F28F8);
void Weapon::setCsTransform(_fvector *pos, float (&m)[4][4])
{
    float (*d)[4] = m_cs->mat;

    __asm__ volatile("lq $8, 0(%4)
	"
                     "lq $9, 16(%4)
	"
                     "lq $10, 32(%4)
	"
                     "lq $11, 48(%4)
	"
                     "sq $8, %0
	"
                     "sq $9, %1
	"
                     "sq $10, %2
	"
                     "sq $11, %3"
                     : "=m"(d[0][0]), "=m"(d[1][0]), "=m"(d[2][0]), "=m"(d[3][0])
                     : "r"(m)
                     : "$8", "$9", "$10", "$11");
    vecCopy(&m_prevPos, &m_cs->trans);
    vecCopy(&m_cs->trans, pos);
}
INCLUDE_ASM("asm/nonmatchings/game/Weapon", ComputeReflection__6WeaponP8_fvectorT1b);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateStaticWeapon__7WeaponsQ26Weapon10WeaponTypeiiP8_fvectorT4P9_hierhead);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateStaticWeapon__7WeaponsGQ2t10LinkedList1ZP6Pickup8IteratoriiP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateLavaBall__7WeaponsP8_fvectorN21ii);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CalculateBallisticArc__6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", InitWeapon__6Weapon);
void Weapon::KillWeapon(void)
{
    m_active = 0;
    if (m_cs) {
        /* a thrown pickup keeps drawing its own cs */
        if (m_pickup.cur)
            m_pickup.cur = 0;
        else
            m_cs->drawMe = 0;
    }
    if (m_type != 0x17)
        CsPool::csDeactivate(m_cs);
    if (m_pointLight >= 0) {
        PointLight *pl = &pointLights.lights[m_pointLight];
        pl->active = 0;
        pl->f38 = 0;
        m_pointLight = -1;
    }
    particleKillFx(m_fx);
    particleKillFx(m_fx2);
    m_sound.terminateWeaponSound();
}
INCLUDE_ASM("asm/nonmatchings/game/Weapon", ricochetParticlesOffSurface__FiP8_fvectorN31);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", DetonateWeapon__6WeaponR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", DispatchRangeBasedDamage__6Weapon);
/* Velocity per second from the last frame's move. */
void Weapon::GetVel(_fvector &v)
{
    vecSubNR(&v, &m_cs->trans, &m_prevPos);
    vecScale(&v, &v, (float)timerGetUpdateRate());
}
void Weapons::DecrementWeaponCount(Monster *m, int type)
{
}
INCLUDE_ASM("asm/nonmatchings/game/Weapon", AddWeaponEpNode__7WeaponsiP9_hierhead);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateFire__7WeaponsP8_fvectorfi);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateFlames__7Weapons);
