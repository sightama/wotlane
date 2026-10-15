#include "common.h"
#include "game/game.h"
#include "hd.h"
#include "cs_pool.h"
#include "vecmath.h"
#include "game/hit_event.h"
#include "game/pickup.h"
#include "task_manager.h"

/* game/Weapon: the classes are in game/weapons.h. Weapons is embedded in TheGame at 0x112490 (gameWeapons()). */

/* TheGame fields game.h does not name yet */
#define GAME_FIELD(type, off) (*(type *)((char *)game + (off)))

float mathfManhatDist(_fvector *a, _fvector *b);
float mathfManhatDist2D(_fvector *a, _fvector *b);
int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);
int particleCreateFadingFlame(_fvector *pos, int fields, _fvector *vel, float scale);
void *particleGetParticle(int h);
void lightSetAmbient(int i, float r, float g, float b);
int timerGetCurTics(void);

extern "C" int printf(const char *, ...);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
/* "Weapons::CreateStaticWeapon - Unable to activate CS for weapon.  Will NOT be added.\n", kept in the asm rodata */
extern char noCsMsg[] __asm__("D_006F28F8");
float smoothEasyInAngle(float cur, float target, float rate, float eps);
void boundEulerAngle(float *a);
void mathfMatrixFromNormal(float (*m)[4], _fvector *n);
/* a particle's position and matrix (common/particle) */
#define PARTICLE_POS(p) ((_fvector *)((char *)(p) + 0x2D00))
#define PARTICLE_MAT(p) ((float (*)[4])((char *)(p) + 0x2D20))
void mathfRandVector(_fvector *v, float r);
extern "C" float atan2f(float y, float x);
extern "C" float fabsf(float);
float hdHatTest(_fvector *pos, _fvector *out, float up, bool b, float down);
float mathfPitchFromPointToPoint(_fvector *from, _fvector *to);
/* One of the 32 dynamic point lights (common/PointLight). */
class PointLight {
public:
    int active;             /* 0x00 */
    char pad4[0x34];
    int f38;                /* 0x38: lit this frame */
    int f3C;

    void SetPos(_fvector *p);
    void SetPos(float px, float py, float pz);
    void SetRange(float rng);
    void SetColor(float cr, float cg, float cb);
};
class PointLights {
public:
    PointLight lights[32];
    int Create(void);
};
extern PointLights pointLights;

class DbInteractive {
public:
    int m_kind;               /* 0x00: 1 monster */
    unsigned short m_flags;   /* 0x04 */
    char pad6[6];
    _cs *m_cs;                /* 0x0C */
    void *m_vptr;             /* 0x10 */
};
class Interactives {
public:
    static int s_numInteractives;
    static DbInteractive *getInteractive(int i);
    static void dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId);
};
/* virtual getPinTrans() of an interactive (vtable entry {delta, function} at vptr + 0x10) */
#define PIN_TRANS(t) \
    (((_fvector *(*)(void *))*(void **)(*(char **)((char *)(t) + 0x10) + 0x14))((char *)(t) + *(short *)(*(char **)((char *)(t) + 0x10) + 0x10)))
/* the Pickup a LevelPickups list iterator stands on (the node's data word) */
#define ITER_PICKUP(it) (*(Pickup **)(it).cur)

/* vecSub in noreorder mode: GNU as would otherwise move the sqc2 into the delay slot of the call that follows */
static inline void vecSubNR(_fvector *dst, _fvector *a, _fvector *b)
{
    __asm__ volatile(".set push\n\t.set noreorder\n\t"
                     "lqc2 $vf11, 0x0(%1)\n\t"
                     "lqc2 $vf12, 0x0(%2)\n\t"
                     "vsub.xyz $vf11, $vf11, $vf12\n\t"
                     "sqc2 $vf11, %0\n\t"
                     ".set pop"
                     : "=m"(*dst) : "r"(a), "r"(b));
}

/* v /= |v| with one rsqrt.s, in the registers retail's inline uses */
#ifdef NON_MATCHING
static inline void vecNormalize(_fvector *v)
{
    register float x __asm__("$f3") = v->x;
    register float y __asm__("$f4") = v->y;
    register float z __asm__("$f2") = v->z;
    register float s __asm__("$f5");
    register float one __asm__("$f1");
    register float inv __asm__("$f0");

    __asm__("mula.s %1, %1\n\t"
            "madda.s %2, %2\n\t"
            "madd.s %0, %3, %3"
            : "=f"(s) : "f"(x), "f"(y), "f"(z));
    __asm__ volatile(".set noat\n\t"
                     "lui $1, 0x3F80\n\t"
                     "mtc1 $1, %4\n\t"
                     ".set at\n\t"
                     "rsqrt.s %3, %4, %5\n\t"
                     "mul.s %6, %6, %3\n\t"
                     "mul.s %7, %7, %3\n\t"
                     "mul.s %8, %8, %3\n\t"
                     "swc1 %6, %2\n\t"
                     "swc1 %7, %0\n\t"
                     "swc1 %8, %1"
                     : "=m"(v->x), "=m"(v->y), "=m"(v->z), "=f"(inv), "=f"(one)
                     : "f"(s), "f"(z), "f"(x), "f"(y));
}
/* a . b loaded from memory, in the registers retail's inline uses */
static inline float vecDotMem(_fvector *a, _fvector *b)
{
    register float ax __asm__("$f1");
    register float bx __asm__("$f5");
    register float ay __asm__("$f4");
    register float by __asm__("$f3");
    register float az __asm__("$f2");
    register float bz __asm__("$f0");

    __asm__ volatile("lwc1 %0, 0(%6)\n\t"
                     "lwc1 %1, 0(%7)\n\t"
                     "lwc1 %2, 4(%6)\n\t"
                     "lwc1 %3, 4(%7)\n\t"
                     "lwc1 %4, 8(%6)\n\t"
                     "lwc1 %5, 8(%7)\n\t"
                     "mula.s %0, %1\n\t"
                     "madda.s %2, %3\n\t"
                     "madd.s %0, %4, %5"
                     : "=&f"(ax), "=&f"(bx), "=&f"(ay), "=&f"(by), "=&f"(az), "=&f"(bz)
                     : "r"(a), "r"(b) : "memory");
    return ax;
}
/* x^2 + y^2 + z^2 on the FPU accumulator */
static inline float sumSq3(float x, float y, float z)
{
    float r;

    __asm__ volatile("mula.s %1, %1\n\t"
                     "madda.s %2, %2\n\t"
                     "madd.s %0, %3, %3"
                     : "=f"(r) : "f"(x), "f"(y), "f"(z));
    return r;
}
/* |v|^2 on the FPU accumulator like vecLenSq, registers left to the compiler */
static inline float vecLenSqR(_fvector *v)
{
    float *f = (float *)v;
    float x, y, z, r;

    __asm__ volatile("lwc1 %0, %4\n\t"
                     "lwc1 %1, %5\n\t"
                     "lwc1 %2, %6\n\t"
                     "mula.s %0, %0\n\t"
                     "madda.s %1, %1\n\t"
                     "madd.s %3, %2, %2"
                     : "=&f"(x), "=&f"(y), "=&f"(z), "=f"(r) : "m"(f[0]), "m"(f[1]), "m"(f[2]));
    return r;
}
/* the same with x, y, z in $f2, $f3, $f0 and the result over x */
static inline float vecLenSqX(_fvector *v)
{
    float *f = (float *)v;
    register float x __asm__("$f2");
    register float y __asm__("$f3");
    register float z __asm__("$f0");

    __asm__ volatile("lwc1 %0, %3\n\t"
                     "lwc1 %1, %4\n\t"
                     "lwc1 %2, %5\n\t"
                     "mula.s %0, %0\n\t"
                     "madda.s %1, %1\n\t"
                     "madd.s %0, %2, %2"
                     : "=f"(x), "=f"(y), "=f"(z) : "m"(f[0]), "m"(f[1]), "m"(f[2]));
    return x;
}
#endif

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
        w->m_dyn.damage = 0;
        w->m_dyn.lifetime = 0;
        w->m_dyn.f18 = 0;
        w->m_dyn.turnRate = 0;
        w->m_dyn.speed = 0;
        w->m_homing = 0;
        w->m_targetPoint = 0;
        w->m_type = 0;
        w->m_wobble.x = 0;
        w->m_wobble.y = 0;
        w->m_wobble.z = 0;
        w->m_wobbleTime = 0;
        w->m_5C = 0;
        w->m_cs = 0;
        w->m_vel.x = 0;
        w->m_vel.y = 0;
        w->m_vel.z = 0;
        _fvector *v = &w->m_spin;
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
        f->active = 0;
        f->age = 0;
        f->radius = 0;
        f->fx = 0;
        f->light = -1;
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
        d->damage = 1.0f;
        d->lifetime = 3.0f;
        d->f18 = 1.0f;
        d->turnRate = 0;
        d->speed = 800.0f;
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

    m_params[13].damage = 15.0f;
    m_params[13].lifetime = 10.0f;
    m_params[13].f18 = 1.0f;
    m_params[13].turnRate = 0.01f;
    m_params[13].speed = 700.0f;

    m_params[21].damage = 1.0f;
    m_params[21].lifetime = 8.0f;
    m_params[21].f18 = 1.0f;
    m_params[21].turnRate = 0.02f;
    m_params[21].speed = 500.0f;

    m_params[22].damage = 1.0f;
    m_params[22].lifetime = 8.0f;
    m_params[22].f18 = 1.0f;
    m_params[22].turnRate = 0.02f;
    m_params[22].speed = 500.0f;

    m_params[23].damage = 5.0f;
    m_params[23].lifetime = 10.0f;
    m_params[23].f18 = 1.0f;
    m_params[23].turnRate = 0;
    m_params[23].speed = 800.0f;

    m_params[24].damage = 1.0f;
    m_params[24].lifetime = 3.0f;
    m_params[24].f18 = 1.0f;
    m_params[24].turnRate = 0;
    m_params[24].speed = 800.0f;

    m_params[25].damage = 3.0f;
    m_params[25].lifetime = 2.0f;
    m_params[25].f18 = 1.0f;
    m_params[25].turnRate = 0;
    m_params[25].speed = 1400.0f;

    m_params[26].damage = 8.0f;
    m_params[26].lifetime = 5.0f;
    m_params[26].f18 = 0.05f;
    m_params[26].turnRate = 0.08f;
    m_params[26].speed = 700.0f;

    m_params[27].damage = 8.0f;
    m_params[27].lifetime = 5.0f;
    m_params[27].f18 = 0.05f;
    m_params[27].turnRate = 1.5f;
    m_params[27].speed = 700.0f;
    m_params[27].pitchRate = 1.75f;

    m_params[28].damage = 3.0f;
    m_params[28].lifetime = 2.0f;
    m_params[28].f18 = 1.0f;
    m_params[28].turnRate = 0;
    m_params[28].speed = 1400.0f;

    m_params[29].damage = 3.0f;
    m_params[29].lifetime = 2.0f;
    m_params[29].f18 = 1.0f;
    m_params[29].turnRate = 0.2f;
    m_params[29].speed = 1400.0f;
    m_params[29].pitchRate = 0.4f;

    m_params[30].damage = 1.0f;
    m_params[30].lifetime = 2.0f;
    m_params[30].f18 = 1.0f;
    m_params[30].turnRate = 0;
    m_params[30].speed = 800.0f;

    m_params[31].damage = 1.0f;
    m_params[31].lifetime = 2.0f;
    m_params[31].f18 = 1.0f;
    m_params[31].turnRate = 0.2f;
    m_params[31].speed = 800.0f;
    m_params[31].pitchRate = 0.4f;

    m_params[32].damage = 3.0f;
    m_params[32].lifetime = 2.0f;
    m_params[32].f18 = 1.0f;
    m_params[32].turnRate = 0;
    m_params[32].speed = 1400.0f;

    m_params[33].damage = 3.0f;
    m_params[33].lifetime = 2.0f;
    m_params[33].f18 = 1.0f;
    m_params[33].turnRate = 0;
    m_params[33].speed = 1400.0f;

    m_params[34].damage = 3.0f;
    m_params[34].lifetime = 2.0f;
    m_params[34].f18 = 1.0f;
    m_params[34].turnRate = 0;
    m_params[34].speed = 1400.0f;

    m_params[35].damage = 3.0f;
    m_params[35].lifetime = 2.0f;
    m_params[35].f18 = 1.0f;
    m_params[35].turnRate = 0;
    m_params[35].speed = 1400.0f;

    m_params[36].damage = 1.0f;
    m_params[36].lifetime = 2.0f;
    m_params[36].f18 = 1.0f;
    m_params[36].turnRate = 0;
    m_params[36].speed = 1000.0f;

    m_params[37].damage = 1.0f;
    m_params[37].lifetime = 2.0f;
    m_params[37].f18 = 1.0f;
    m_params[37].turnRate = 0;
    m_params[37].speed = 1000.0f;

    m_params[38].damage = 1.0f;
    m_params[38].lifetime = 2.0f;
    m_params[38].f18 = 1.0f;
    m_params[38].turnRate = 0.07f;
    m_params[38].speed = 1200.0f;

    m_params[42].damage = 30.0f;
    m_params[42].lifetime = 0.05f;
    m_params[42].f18 = 1.0f;
    m_params[42].turnRate = 0;
    m_params[42].speed = 400.0f;

    m_params[43].damage = 10.0f;
    m_params[43].lifetime = 1.2f;
    m_params[43].f18 = 1.0f;
    m_params[43].turnRate = 0;
    m_params[43].speed = 250.0f;

    /* faster in match mode */
    m_params[44].damage = (float)game->m_matchMode + 1.75f;
    m_params[44].lifetime = 4.0f;
    m_params[44].f18 = 1.0f;
    m_params[44].turnRate = 0.04f;
    m_params[44].speed = 800.0f;
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
/* Homing (m_homing: 1 lock on the target, 2 lead the monster, 3 vertical) or, unguided, the spin in m_spin easing out in pitch and
   heading. A cloaked target monster breaks the lock. */
void Weapon::UpdateRotation(void)
{
    if (m_targetId > 0) {
        DbInteractive *t = Interactives::getInteractive(m_targetId);
        if (t->m_kind == 1 && ((Monster *)t)->m_cloaked)
            m_homing = 0;
    }
    switch (m_homing) {
    case 0:
        if (m_spin.x != 0.0f || m_spin.y != 0.0f || m_spin.z != 0.0f) {
            m_spin.x = smoothEasyInAngle(m_spin.x, 0.0f, 0.1f, 0.01f);
            m_spin.z = smoothEasyInAngle(m_spin.z, 0.0f, 0.1f, 0.01f);
            vecAdd(&m_cs->rot, &m_cs->rot, &m_spin);
            boundEulerAngle(&m_cs->rot.x);
            boundEulerAngle(&m_cs->rot.y);
            boundEulerAngle(&m_cs->rot.z);
            mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
        }
        break;
    case 1:
        UpdateLockOnTarget();
        break;
    case 2:
        UpdateLockAheadOfMonster();
        break;
    case 3:
        UpdateLockVertical();
        break;
    }
}
/* Homing mode 1: turn toward the target's pin point, heading at m_dyn.turnRate and pitch at m_dyn.pitchRate (turnRate when 0). An untargetable
   target drops the lock; so does one more than angle2C (heading) or angle30 (pitch) off, unless the weapon wobbles (m_wobble.x).
   Before the first field, or without a target, it only spins by m_spin. */
#ifdef NON_MATCHING
/* equivalent: 45/191 words, 0x2f4 vs 0x2fc; the 0.0f constant is kept in $f20/$f21 from the start and the eased angle lands in $f1 */
void Weapon::UpdateLockOnTarget(void)
{
    float d[2];
    float a;
    float r;
    DbInteractive *t;

    if (m_targetId && m_age > 0) {
        t = Interactives::getInteractive(m_targetId);
        if (!t || !(t->m_flags & 2)) {
            m_targetId = 0;
            return;
        }
        a = mathfHeadingFromPointToPoint(&m_cs->trans, PIN_TRANS(t));
        d[0] = a - m_cs->rot.z;
        boundEulerAngle(&d[0]);
        if (m_wobble.x == 0.0f && fabsf(d[0]) > m_dyn.angle2C)
            goto lost;
        r = smoothEasyInAngle(m_cs->rot.z, a, m_dyn.turnRate, 0.0001f);
        m_spin.z = r - m_cs->rot.z;
        m_cs->rot.z = r;
        a = mathfPitchFromPointToPoint(&m_cs->trans, PIN_TRANS(t));
        d[1] = a - m_cs->rot.x;
        boundEulerAngle(&d[1]);
        if (m_wobble.x == 0.0f && fabsf(d[1]) > m_dyn.angle30) {
        lost:
            m_hugGround = 0;
            m_targetId = 0;
            m_homing = 0;
            return;
        }
        r = smoothEasyInAngle(m_cs->rot.x, a, m_dyn.pitchRate != 0.0f ? m_dyn.pitchRate : m_dyn.turnRate, 0.0001f);
        m_spin.x = r - m_cs->rot.x;
        m_cs->rot.x = r;
        if (m_wobble.x != 0.0f)
            UpdateErraticity();
        vecAdd(&m_cs->rot, &m_cs->rot, &m_spin);
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
    } else if (m_spin.x != 0.0f || m_spin.y != 0.0f || m_spin.z != 0.0f) {
        m_cs->rot.x += m_spin.x;
        m_cs->rot.y += m_spin.y;
        m_cs->rot.z += m_spin.z;
        boundEulerAngle(&m_cs->rot.x);
        boundEulerAngle(&m_cs->rot.y);
        boundEulerAngle(&m_cs->rot.z);
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateLockOnTarget__6Weapon);
#endif
/* Homing mode 2: aim at the target monster's locator 0x809 led by a third of the way it runs (m_speed per second along its
   forward axis) while the weapon covers the distance, plus m_aimOffset in the monster's frame when m_5C is set; heading
   then pitch at m_dyn.turnRate. More than 90 degrees off either way breaks the lock. Before the first field, or without
   a target, it only spins by m_spin. */
#ifdef NON_MATCHING
void Weapon::UpdateLockAheadOfMonster(void)
{
    float d[2];
    float a;
    float dx, dy, dz, dist, lead;
    FVECTOR aim, off;
    Monster *t;

    if (m_targetId && m_age > 0) {
        t = (Monster *)Interactives::getInteractive(m_targetId);
        if (((DbInteractive *)t)->m_kind != 1)
            return;
        dist = mathfManhatDist(&m_cs->trans, &t->m_cs->trans);
        lead = t->m_speed / 60.0f * (dist / (m_dyn.speed * (5280.0f / 3600.0f / 60.0f)) * 0.33f);
        vecScale(&aim, (_fvector *)t->m_cs->mat[1], lead);
        vecAdd(&aim, &aim, (_fvector *)((char *)t + 0x3E30)); /* locator 0x809 */
        if (m_5C) {
            vecApplyRows(&off, t->m_cs->mat, &m_aimOffset);
            vecAdd(&aim, &aim, &off);
        }
        dy = aim.y - m_cs->trans.y;
        dx = aim.x - m_cs->trans.x;
        if (dy == 0.0f)
            dy = 0.01f;
        a = atan2f(dx, dy);
        d[0] = a - m_cs->rot.z;
        boundEulerAngle(&d[0]);
        if (fabsf(d[0]) > 1.5707964f)
            goto lost;
        m_cs->rot.z = smoothEasyInAngle(m_cs->rot.z, a, m_dyn.turnRate, 0.0001f);
        dz = aim.z - m_cs->trans.z;
        dist = mathfManhatDist(&aim, &m_cs->trans);
        if (dist == 0.0f)
            dist = 0.01f;
        a = atan2f(dz, dist);
        d[1] = a - m_cs->rot.x;
        boundEulerAngle(&d[1]);
        if (fabsf(d[1]) > 1.5707964f) {
        lost:
            m_homing = 0;
            m_targetId = 0;
            return;
        }
        m_cs->rot.x = smoothEasyInAngle(m_cs->rot.x, a, m_dyn.turnRate, 0.0001f);
        vecAdd(&m_cs->rot, &m_cs->rot, &m_spin);
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
    } else if (m_spin.x != 0.0f || m_spin.y != 0.0f || m_spin.z != 0.0f) {
        vecAdd(&m_cs->rot, &m_cs->rot, &m_spin);
        boundEulerAngle(&m_cs->rot.x);
        boundEulerAngle(&m_cs->rot.y);
        boundEulerAngle(&m_cs->rot.z);
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateLockAheadOfMonster__6Weapon);
#endif
/* Homing mode 3: turn heading then pitch toward m_targetPos at the thrower's LavaBlast turn rates; a target more than 90
   degrees off either way breaks the lock. Before the first field the weapon only spins by m_spin. */
#ifdef NON_MATCHING
/* equivalent: 17/170 words, 0x29c vs 0x2a8; scheduling of the pi/2 and 0 constants (f22/f23 swapped) and of the bail-out branches */
void Weapon::UpdateLockVertical(void)
{
    char *owner = (char *)Interactives::getInteractive(m_ownerId);
    float d[2];
    float a;
    float dx, dy, dz, dist;

    if (m_age > 0) {
        dy = m_targetPos.y - m_cs->trans.y;
        dx = m_targetPos.x - m_cs->trans.x;
        if (dy == 0.0f)
            dy = 0.01f;
        a = atan2f(dx, dy);
        d[0] = a - m_cs->rot.z;
        boundEulerAngle(&d[0]);
        if (fabsf(d[0]) > 1.5707964f) {
            m_homing = 0;
            m_targetId = 0;
            return;
        }
        owner += 0x10930; /* the thrower's StateLavaBlast */
        m_cs->rot.z = smoothEasyInAngle(m_cs->rot.z, a, *(float *)(owner + 0x194), 0.0001f);
        dz = m_targetPos.z - m_cs->trans.z;
        dist = mathfManhatDist(&m_targetPos, &m_cs->trans);
        if (dist == 0.0f)
            dist = 0.01f;
        a = atan2f(dz, dist);
        d[1] = a - m_cs->rot.x;
        boundEulerAngle(&d[1]);
        if (fabsf(d[1]) > 1.5707964f) {
            m_spin.x = d[1];
            m_targetId = 0;
            m_homing = 0;
            m_hugGround = 0;
            return;
        }
        m_cs->rot.x = smoothEasyInAngle(m_cs->rot.x, a, *(float *)(owner + 0x198), 0.0001f);
        vecAdd(&m_cs->rot, &m_cs->rot, &m_spin);
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
    } else if (m_spin.x != 0.0f || m_spin.y != 0.0f || m_spin.z != 0.0f) {
        m_cs->rot.x += m_spin.x;
        m_cs->rot.y += m_spin.y;
        m_cs->rot.z += m_spin.z;
        boundEulerAngle(&m_cs->rot.x);
        boundEulerAngle(&m_cs->rot.y);
        boundEulerAngle(&m_cs->rot.z);
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateLockVertical__6Weapon);
#endif
/* Wobble of an unsteady projectile: every m_wobblePeriod fields (15..60) a new random spin target, up to m_wobble.x degrees in heading
   (at least 5) and in pitch, either sign; the spin follows it for the first half of the period and settles in the second.
   Within 300 of the target the wobble (and m_dyn.turnRate) eases out. */
#ifdef NON_MATCHING
/* equivalent: 67/158 words, 0x26c vs 0x278; the target pointer is picked before the m_cs load and the distance compare uses other registers */
void Weapon::UpdateErraticity(void)
{
    _fvector *t;

    if (m_wobblePeriod < ++m_wobbleTime) {
        m_wobbleTime = 0;
        m_wobblePeriod = mathfRand(15, 60);
        if (mathfRand(0, 1))
            m_wobble.y = mathfRandf(0.08726646f, m_wobble.x * 0.017453292f);
        else
            m_wobble.y = -mathfRandf(0.08726646f, m_wobble.x * 0.017453292f);
        if (mathfRand(0, 1))
            m_wobble.z = mathfRandf(0.0f, m_wobble.x * 0.017453292f);
        else
            m_wobble.z = -mathfRandf(0.0f, m_wobble.x * 0.017453292f);
    }
    if (m_targetId > 0)
        t = &Interactives::getInteractive(m_targetId)->m_cs->trans;
    else
        t = m_targetPoint;
    if (mathfManhatDist(&m_cs->trans, t) < 300.0f) {
        m_wobbleTime = m_wobblePeriod;
        m_wobble.x = (int)smoothEasyIn(m_wobble.x, 0.0f, 0.08f, 0.001f);
        m_dyn.turnRate = smoothEasyIn(m_dyn.turnRate, 0.1f, 0.08f, 0.001f);
    }
    if (m_wobbleTime < m_wobblePeriod >> 1) {
        m_spin.x = smoothEasyInAngle(m_spin.x, m_wobble.z, 0.1f, 0.0001f);
        m_spin.z = smoothEasyInAngle(m_spin.z, m_wobble.y, 0.1f, 0.0001f);
    } else {
        m_spin.x = smoothEasyInAngle(m_spin.x, 0.0f, 0.07f, 0.0001f);
        m_spin.z = smoothEasyInAngle(m_spin.z, 0.0f, 0.07f, 0.0001f);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateErraticity__6Weapon);
#endif
/* Moves the weapon: the forward speed m_vel.y eases toward m_dyn.speed (mph to units per field; slowing down is near
   instant), m_vel is applied in the cs frame, a ballistic arc overrides z, and with m_hugGround set the weapon hugs the ground 5 above
   the hat. A lava ball that starts falling becomes a homing shot (750 mph). */
#ifdef NON_MATCHING
/* equivalent: 65/204 words, 0x330 vs 0x328; the 0.0f constant is hoisted to $f20 and the ground-hug branches are laid out differently */
void Weapon::UpdateTranslation(void)
{
    FVECTOR world;
    FVECTOR local;
    float target;
    float t;
    float h;
    float rate;

    if (m_vel.y != 0.0f)
        vecCopy(&m_prevPos, &m_cs->trans);
    target = m_dyn.speed * 0.024444444f;
    m_vel.y = smoothEasyIn(m_vel.y, target, target < m_vel.y ? 0.0001f : m_dyn.f18, 0.0001f);
    vecScale(&local, &m_vel, (float)timerGetFieldsLastFrame());
    vecApplyRows(&world, m_cs->mat, &local);
    vecAdd(&m_cs->trans, &m_cs->trans, &world);
    if (m_vz != 0.0f) {
        t = (float)(timerGetCurTics() - m_launchTics) / 576.0f * 0.001f;
        m_cs->trans.z = m_startZ + m_vz * t + m_gravity * (t * t);
    }
    if (m_hugGround) {
        h = hdHatTest(&m_cs->trans, &world, 0.0f, false, 4096.0f);
        if (m_targetId || m_targetPoint) {
            if (!(0.0f < h && h < 5.0f))
                goto light;
            rate = 0.2f;
        } else if (5.0f < h) {
            rate = 0.01f;
        } else {
            rate = 0.2f;
        }
        m_cs->trans.z = smoothEasyIn(m_cs->trans.z, m_cs->trans.z - h + 5.0f, rate, 0.001f);
    }
light:
    if (m_pointLight >= 0)
        pointLights.lights[m_pointLight].SetPos(&m_cs->trans);
    if ((unsigned)(m_type - 0x27) < 3 && m_lavaHoming && m_cs->trans.z < m_prevPos.z) {
        m_homing = 1;
        m_dyn.turnRate = 0.1f;
        m_dyn.speed = 750.0f;
        m_lavaHoming = 0;
        m_vz = 0;
        m_gravity = 0;
        m_maxRange = 0;
    }
    if (m_homing == 1 && m_vz != 0.0f && m_cs->trans.z < m_prevPos.z)
        m_vz = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateTranslation__6Weapon);
#endif
/* Collision of this field's move (m_prevPos -> cs): a swept sphere whose radius depends on the type (a thrown pickup uses its
   collision sphere or its LevelPickups radius and credits the thrower's "Pickup Use" token), squishing pedestrians on the way.
   Hits on the thrower, on hats 0xB/0xC and, for some types, on certain level objects are ignored; a monster blocking with a
   shield (held pickup type 0x10) facing the shot reflects it; anything else detonates the weapon. */
#ifdef NON_MATCHING
/* equivalent: 6/408 words, 0x61c vs 0x660 untuned; gcc merges the repeated token credit and the id checks */
int hdSphereTest(_fvector *a, _fvector *b, float r, _hdResult *res, unsigned skipId2, int max);
void hdPolyCheckBackface(bool on);
void mathfRotationFromVector(_fvector *rot, _fvector *v);
class PedGroup {
public:
    static void squishGlobal(_fvector &pos, float r, Monster *by);
};
/* the 64-bit first word of an _hdResult: id2 in bits 32..42, id1 in bits 43..56 */
#define HD_ID2(r) ((int)((r).ids & 0x7FF))
#define HD_ID1(r) ((int)(((r).ids >> 11) & 0x3FFF))

void Weapon::WeaponHD(void)
{
    _hdResult res;
    Monster *owner;
    _fvector *from;
    int bounced;

    res.hit = 0;
    owner = 0;
    if (m_ownerId && Interactives::getInteractive(m_ownerId)->m_kind == 1)
        owner = (Monster *)Interactives::getInteractive(m_ownerId);
    switch (m_type) {
    default:
        hdSphereTest(&m_prevPos, &m_cs->trans, 0.0f, &res, m_ownerId, 1);
        from = &m_prevPos;
        if (!res.hit)
            return;
        PedGroup::squishGlobal(m_cs->trans, 15.0f, owner);
        goto levelObject;
    case 9:
    case 0x15:
    case 0x16:
        hdSphereTest(&m_prevPos, &m_cs->trans, 15.0f, &res, m_ownerId, 1);
        from = &m_prevPos;
        if (!res.hit)
            return;
        PedGroup::squishGlobal(m_cs->trans, 30.0f, owner);
        if (!res.hit)
            return;
        PedGroup::squishGlobal(m_cs->trans, 15.0f, owner);
    levelObject:
        /* kind 5 objects with a positive +0xD8 are not hit in the first 5 fields */
        if (HD_ID2(res) != 0) {
            DbInteractive *o = Interactives::getInteractive(HD_ID2(res));

            if (o->m_kind == 5 && m_age < 5 && *(int *)((char *)o + 0xD8) > 0)
                res.hit = 0;
        }
        break;
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1C:
    case 0x1E:
    case 0x20:
    case 0x21:
    case 0x24:
    case 0x26: {
        int id1;

        hdSphereTest(&m_prevPos, &m_cs->trans, 0.0f, &res, m_ownerId, 1);
        from = &m_prevPos;
        if (!res.hit)
            return;
        id1 = HD_ID1(res);
        if (id1 == 0x139F || id1 == 0x13A1 || (id1 >= 0x1391 && id1 < 0x1395))
            res.hit = 0;
        break;
    }
    case 0x27:
    case 0x28:
    case 0x29:
        hdSphereTest(&m_prevPos, &m_cs->trans, 15.0f, &res, 0, 1);
        from = &m_prevPos;
        if (m_age < 60 && HD_ID2(res) == 0)
            res.hit = 0;
        if (!res.hit)
            return;
        PedGroup::squishGlobal(m_cs->trans, 15.0f, owner);
        break;
    case 0x17: {
        Pickup *p;

        hdPolyCheckBackface(true);
        p = ITER_PICKUP(m_pickup);
        if (p->pickupType == 7) {
            FVECTOR a, b;

            vecApplyRows(&a, m_cs->mat, p->getCollisSphereTrans(0));
            vecAdd(&b, &a, &m_cs->trans);
            vecAdd(&a, &a, &m_prevPos);
            from = &m_prevPos;
            hdSphereTest(&a, &b, ITER_PICKUP(m_pickup)->getCollisSphereRadius(0), &res, m_cs->epNode->id2, 1);
            if (res.hit) {
                PedGroup::squishGlobal(m_cs->trans, 15.0f, owner);
                if (owner && owner->m_playerNum == 1)
                    game->m_tokens[owner->m_index].credit("Pickup Use", 10);
            }
        } else {
            float r = LevelPickups::s_info[p->pickupType].radius;

            from = &m_prevPos;
            hdSphereTest(&m_prevPos, &m_cs->trans, r, &res, m_ownerId, 1);
            if (res.hit) {
                PedGroup::squishGlobal(m_cs->trans, r + r, owner);
                if (owner && owner->m_playerNum == 1)
                    game->m_tokens[owner->m_index].credit("Pickup Use", 10);
            }
        }
        hdPolyCheckBackface(false);
        break;
    }
    }
    if (!res.hit)
        return;
    if (HD_ID1(res) == 0xB || HD_ID1(res) == 0xC)
        res.hit = 0;
    if (HD_ID2(res) == m_ownerId)
        res.hit = 0;
    if (!res.hit)
        return;
    bounced = 0;
    if ((unsigned)(m_type - 0xC) >= 11 && HD_ID2(res) != 0) {
        int kind = Interactives::getInteractive(HD_ID2(res))->m_kind;

        if (kind == 1) {
            Monster *t = (Monster *)Interactives::getInteractive(HD_ID2(res));
            int shield = 0;

            if (t->isBlocking() && t->m_pickup != 0)
                shield = (*(Pickup **)t->m_pickup)->pickupType == 0x10 ? kind : 0;
            if (shield) {
                FVECTOR d;
                float k;

                vecSub(&d, &m_cs->trans, from);
                k = eeRsqrtf(1.0f, d.x * d.x + d.y * d.y + d.z * d.z);
                d.z *= k;
                d.x *= k;
                d.y *= k;
                if (d.x * t->m_cs->mat[1][0] + d.y * t->m_cs->mat[1][1] + d.z * t->m_cs->mat[1][2] < 0.0f) {
                    ComputeReflection((_fvector *)t->m_cs->mat[1], &d, false);
                    bounced = 1;
                    mathfRotationFromVector(&m_cs->rot, &d);
                    mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
                } else {
                    bounced = 0;
                }
            }
        }
    }
    if (!bounced) {
        DetonateWeapon(res);
        KillWeapon();
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", WeaponHD__6Weapon);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Weapon", D_006F28F8);
void Weapon::setCsTransform(_fvector *pos, float (&m)[4][4])
{
    float (*d)[4] = m_cs->mat;

    __asm__ volatile("lq $8, 0(%4)\n\t"
                     "lq $9, 16(%4)\n\t"
                     "lq $10, 32(%4)\n\t"
                     "lq $11, 48(%4)\n\t"
                     "sq $8, %0\n\t"
                     "sq $9, %1\n\t"
                     "sq $10, %2\n\t"
                     "sq $11, %3"
                     : "=m"(d[0][0]), "=m"(d[1][0]), "=m"(d[2][0]), "=m"(d[3][0])
                     : "r"(m)
                     : "$8", "$9", "$10", "$11");
    vecCopy(&m_prevPos, &m_cs->trans);
    vecCopy(&m_cs->trans, pos);
}
/* Bounce direction off a surface of normal n, from the last frame's move: the mirrored unit direction when reflect is set,
   otherwise a random direction around n (radius 0.25). A weapon that did not move counts as moving along +y. */
#ifdef NON_MATCHING
/* equivalent: 39/110 words, same size; the zero test of the move does not steal the normalize loads into its branch delay slots */
void Weapon::ComputeReflection(_fvector *n, _fvector *out, bool reflect)
{
    FVECTOR v;
    FVECTOR t;
    float m[4][4];
    FVECTOR r;
    float k;

    vecSub(&v, &m_cs->trans, &m_prevPos);
    if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
        v.y = 0.001f;
    else
        vecNormalize(&v);
    k = -vecDotMem(&v, n);
    if (reflect) {
        vecScale(&t, n, k + k);
        vecAdd(out, &v, &t);
        vecNormalize(out);
    } else {
        mathfMatrixFromNormal(m, n);
        mathfRandVector(&r, 0.25f);
        vecApplyRows(out, m, &r);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", ComputeReflection__6WeaponP8_fvectorT1b);
#endif
/* A free weapon slot for a projectile of the given type at pos (rotation rot); returns its index, -1 if none is free or no
   cs is left. byId/targetId are interactive ids (0 = none); h is not used. */
#ifdef NON_MATCHING
/* equivalent: 89/108 words, same size; register allocation only (pos, rot, i and the constants 1/-1 in other $s registers) */
int Weapons::CreateStaticWeapon(Weapon::WeaponType type, int byId, int targetId, _fvector *pos, _fvector *rot, _hierhead *h)
{
    int i;
    Weapon *w = 0;

    for (i = 0; i < 100; i++) {
        if (!m_weapons[i].m_active) {
            _fvector *a, *b, *c;

            w = &m_weapons[i];
            w->m_active = 1;
            w->m_age = 0;
            w->m_lavaHoming = 0;
            w->m_wobble.x = 0;
            w->m_wobble.y = 0;
            w->m_wobble.z = 0;
            w->m_wobbleTime = 0;
            w->m_hugGround = 0;
            w->m_ownerId = byId;
            w->m_homing = targetId != 0;
            w->m_type = type;
            w->m_targetId = targetId;
            w->m_cs = CsPool::csActivate();
            if (!w->m_cs) {
                printf(noCsMsg);
                w->m_active = 0;
                return -1;
            }
            w->m_cs->drawMe = 1;
            w->m_cs->testCollision = 0;
            vecCopy(&w->m_cs->trans, pos);
            vecCopy(&w->m_prevPos, pos);
            vecCopy(&w->m_cs->rot, rot);
            mathfRotMatrixRPH(w->m_cs->mat, &w->m_cs->rot);
            a = &w->m_vel;
            a->x = 0;
            a->y = 0;
            a->z = 0;
            b = &w->m_spin;
            b->x = 0;
            b->y = 0;
            b->z = 0;
            c = &w->m_C0;
            c->x = 0;
            c->y = 0;
            c->z = 0;
            w->m_fx2 = -1;
            w->m_fx = -1;
            w->m_vz = 0;
            w->m_sound.playWeaponLaunchSound(&w->m_cs->trans, w->m_type);
            break;
        }
    }
    if (i == 100)
        return -1;
    w->InitWeapon();
    if (w->m_dyn.turnRate == 0.0f)
        w->m_homing = 0;
    return i;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateStaticWeapon__7WeaponsQ26Weapon10WeaponTypeiiP8_fvectorT4P9_hierhead);
#endif
/* A weapon slot that flies a thrown pickup (type 0x17, the pickup's own cs); returns its index or -1. Rigid pickups
   (pickupType 7) fly straight, the others on a ballistic arc. */
#ifdef NON_MATCHING
/* equivalent: 76/102 words, same size; register allocation of the arguments and the order of the first stores */
int Weapons::CreateStaticWeapon(PickupIter it, int byId, int targetId, _fvector *rot)
{
    int i;
    Weapon *w = 0;

    for (i = 0; i < 100; i++) {
        if (!m_weapons[i].m_active) {
            _fvector *a, *b, *c;

            w = &m_weapons[i];
            w->m_pickup = it;
            w->m_active = 1;
            w->m_ownerId = byId;
            w->m_homing = targetId != 0;
            w->m_type = 0x17;
            w->m_fx2 = -1;
            w->m_cs = ITER_PICKUP(it)->cs;
            w->m_targetId = targetId;
            w->m_fx = -1;
            w->m_age = 0;
            w->m_lavaHoming = 0;
            w->m_wobble.x = 0;
            w->m_wobble.y = 0;
            w->m_wobble.z = 0;
            w->m_wobbleTime = 0;
            w->m_hugGround = 0;
            w->m_vz = 0;
            vecCopy(&w->m_cs->rot, rot);
            mathfRotMatrixRPH(w->m_cs->mat, rot);
            vecCopy(&w->m_prevPos, &w->m_cs->trans);
            a = &w->m_vel;
            a->x = 0;
            a->y = 0;
            a->z = 0;
            b = &w->m_spin;
            b->x = 0;
            b->y = 0;
            b->z = 0;
            c = &w->m_C0;
            c->x = 0;
            c->y = 0;
            c->z = 0;
            w->m_sound.playWeaponLaunchSound(&w->m_cs->trans, w->m_type);
            w->m_targetPoint = 0;
            break;
        }
    }
    if (i == 100)
        return -1;
    w->InitWeapon();
    if (ITER_PICKUP(it)->pickupType != 7)
        w->CalculateBallisticArc();
    if (w->m_dyn.turnRate == 0.0f)
        w->m_homing = 0;
    return i;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateStaticWeapon__7WeaponsGQ2t10LinkedList1ZP6Pickup8IteratoriiP8_fvector);
#endif
/* A lava ball (types 0x27..0x29, one at random) lobbed from pos at the target point; it turns into a homing shot once it
   starts falling (UpdateTranslation). If no slot is free the ballistic arc is still computed, on a null weapon (retail bug). */
void Weapons::CreateLavaBall(_fvector *pos, _fvector *rot, _fvector *target, int byId, int targetId)
{
    int i;
    Weapon *w = 0;
    Weapons *ws;

    for (i = 0; i < 100; i++) {
        if (!m_weapons[i].m_active) {
            _fvector *a, *b, *c;

            w = &m_weapons[i];
            w->m_ownerId = byId;
            w->m_active = 1;
            w->m_age = 0;
            w->m_targetId = targetId;
            if (targetId > 0)
                w->m_lavaHoming = 1;
            else
                w->m_lavaHoming = 0;
            w->m_homing = 0;
            w->m_wobble.x = 0;
            w->m_wobble.y = 0;
            w->m_wobble.z = 0;
            w->m_wobbleTime = 0;
            w->m_hugGround = 0;
            w->m_cs = CsPool::csActivate();
            if (!w->m_cs) {
                printf(noCsMsg);
                w->m_active = 0;
                return;
            }
            w->m_cs->drawMe = 1;
            w->m_cs->testCollision = 0;
            vecCopy(&w->m_cs->trans, pos);
            vecCopy(&w->m_prevPos, pos);
            vecCopy(&w->m_cs->rot, rot);
            mathfRotMatrixRPH(w->m_cs->mat, &w->m_cs->rot);
            w->m_targetPoint = target;
            vecCopy(&w->m_targetPos, target);
            a = &w->m_vel;
            a->x = 0;
            a->y = 0;
            a->z = 0;
            b = &w->m_spin;
            b->x = 0;
            b->y = 0;
            b->z = 0;
            c = &w->m_C0;
            c->x = 0;
            c->y = 0;
            c->z = 0;
            ws = gameWeapons();
            w->m_dyn.f18 = 0.1f;
            w->m_dyn.damage = 5.0f;
            w->m_dyn.radius = 50.0f;
            w->m_dyn.lifetime = 600.0f;
            w->m_dyn.speed = mathfRandf(500.0f, 600.0f);
            w->m_gravity = -800.0f;
            w->m_maxRange = 1000.0f;
            switch (mathfRand(0, 2)) {
            case 0:
                w->m_type = 0x27;
                hierSetCsEpNode(w->m_cs, ws->m_epNodes[39]);
                w->m_fx = particleCreateFx(&w->m_cs->trans, w->m_cs->mat, 0x2F, 8.0f, 0, 0, 0, 0.0f);
                break;
            case 1:
                w->m_type = 0x28;
                hierSetCsEpNode(w->m_cs, ws->m_epNodes[40]);
                w->m_fx = particleCreateFx(&w->m_cs->trans, w->m_cs->mat, 0x2F, 12.0f, 0, 0, 0, 0.0f);
                break;
            case 2:
                w->m_type = 0x29;
                hierSetCsEpNode(w->m_cs, ws->m_epNodes[41]);
                w->m_fx = particleCreateFx(&w->m_cs->trans, w->m_cs->mat, 0x2F, 15.0f, 0, 0, 0, 0.0f);
                break;
            }
            if (w->m_fx != -1) {
                *(int *)((char *)particleGetParticle(w->m_fx) + 0x2DA0) = 0;
                *(float *)((char *)particleGetParticle(w->m_fx) + 0x2DA4) = 1.0f;
                *(float *)((char *)particleGetParticle(w->m_fx) + 0x2D90) = 1.0f;
            }
            w->m_sound.playWeaponLaunchSound(&w->m_cs->trans, w->m_type);
            break;
        }
    }
    w->CalculateBallisticArc();
}
/* Ballistic launch toward the target's pin point (or the aimed-at point, or the thrower's reticle): z follows
   startZ + vz t + gravity t^2 over the flight time at m_dyn.speed; past m_maxRange the gravity is scaled down. */
#ifdef NON_MATCHING
/* equivalent: 15/77 words, 0x134 vs 0x130; the target pointer and the owner swap $v0/$v1 (one extra move), the rest is identical */
void Weapon::CalculateBallisticArc(void)
{
    Monster *owner = (Monster *)Interactives::getInteractive(m_ownerId);
    FVECTOR d;
    FVECTOR target;
    _fvector *t;
    _cs *cs;
    float speed;
    float time;

    if (m_targetId > 0) {
        DbInteractive *tgt = Interactives::getInteractive(m_targetId);
        t = PIN_TRANS(tgt);
    } else if (!(t = m_targetPoint))
        t = &owner->m_reticleCS->trans;
    vecCopy(&target, t);
    if (m_type == 0x17)
        cs = ITER_PICKUP(m_pickup)->cs;
    else
        cs = m_cs;
    vecSub(&d, &target, &cs->trans);
    register float dist __asm__("$f3") = vecLenSqR(&d);
    __asm__ volatile(".word 0x460300C4" : "+f"(dist)); /* sqrt.s $f3, $f3 (R5900 form, see eeSqrtf) */
    speed = m_dyn.speed * 5280.0f / 3600.0f;
    if (m_maxRange < dist)
        m_gravity *= m_maxRange / dist;
    time = dist / speed;
    m_startZ = cs->trans.z;
    m_vz = (target.z - m_startZ - m_gravity * time * time) / time;
    m_launchTics = timerGetCurTics();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CalculateBallisticArc__6Weapon);
#endif
/* Fills m_dyn from Weapons::m_params of the type, then by type: a monster's default projectile (0..11, 20) from the thrower's
   m_dp* tuning (homing at 0.08 while raging), a thrown pickup (0x17) from its LevelPickups::Info scaled by the thrower's m_pu*Mod,
   lava (0x15/0x16) from the thrower's StateLavaBlast aimed at the target or the reticle, and the level's own shots (0x2A..0x2C).
   Turn/pitch rates above 1 are clamped with wider break angles; a weapon with a light range gets a point light. */
#ifdef NON_MATCHING
/* equivalent: 67/595 words, 0x8ec vs 0x94c untuned; gcc merges the repeated hierSetCsEpNode/particleCreateFx tails. The m_targetPos copies use vecCopy
   (the same lq/sq) because difftest mis-tracks the upper half of a plain struct copy through $v1. */
int particleCreateFx(_fvector *pos, float (*m)[4], int type, float f, _fvector *pos2, float (*m2)[4], bool b, float f2);

void Weapon::InitWeapon(void)
{
    Weapons *ws = game->getWeapons();
    Dynamics *p = &ws->m_params[m_type];

    m_dyn.f18 = p->f18;
    m_dyn.damage = p->damage;
    m_dyn.lifetime = p->lifetime * 60.0f;
    m_dyn.turnRate = p->turnRate;
    m_dyn.pitchRate = p->pitchRate;
    m_dyn.speed = p->speed;
    vecCopy((_fvector *)&m_dyn, (_fvector *)p);
    switch (m_type) {
    case 0x17: {
        Monster *o;
        int t;
        float life;
        LevelPickups::Info *info;

        if (ITER_PICKUP(m_pickup)->pickupType != 7)
            m_spin.y = 0.125f;
        m_hugGround = 0;
        o = (Monster *)Interactives::getInteractive(m_ownerId);
        t = ITER_PICKUP(m_pickup)->pickupType;
        info = &LevelPickups::s_info[t];
        life = o->m_puDurationMod[t] * info->duration * 60.0f;
        m_dyn.lifetime = (int)(life > 0.0f ? life + 0.5f : life - 0.5f);
        m_dyn.damage = o->m_puLaunchDamageMod[t] * info->damage;
        m_dyn.speed = o->m_puSpeedMod[t] * info->speed;
        m_dyn.turnRate = info->turnRate;
        m_dyn.pitchRate = info->pitchRate;
        m_gravity = info->gravity;
        m_maxRange = info->maxRange;
        m_dyn.angle2C = info->headingBreak;
        m_dyn.angle30 = info->pitchBreak;
        m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 5, 1.0f, 0, 0, false, 0.0f);
        break;
    }
    case 0x1A:
    case 0x1B:
    case 0x21:
    case 0x23:
        m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 0x36, 0.25f, 0, 0, false, 0.0f);
        m_age = -5;
        /* fall through */
    case 0x18:
    case 0x19:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
    case 0x22:
    case 0x24:
    case 0x25:
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type] ? ws->m_epNodes[m_type] : ws->m_epNodes[0x18]);
        break;
    case 0x26:
        if (m_ownerId) {
            DbInteractive *o = Interactives::getInteractive(m_ownerId);

            if (o->m_kind == 5 && *(int *)((char *)o + 0x210))
                m_dyn.damage = GAME_FIELD(float, 0x1204AC);
        }
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type] ? ws->m_epNodes[m_type] : ws->m_epNodes[0x18]);
        break;
    case 9:
        m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 0x2F, 5.0f, 0, 0, false, 0.0f);
        /* fall through */
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 10:
    case 11:
    case 20: {
        Monster *o = (Monster *)Interactives::getInteractive(m_ownerId);

        m_dyn.damage = o->m_dpDamage;
        m_dyn.lifetime = o->m_dpDuration * 60.0f;
        m_dyn.speed = o->m_dpSpeed;
        m_dyn.turnRate = o->m_dpHomingFactor;
        m_dyn.pitchRate = o->m_dpVertHomingFactor;
        m_dyn.angle2C = o->m_dpHeadingBreak;
        m_dyn.angle30 = o->m_dpPitchBreak;
        if (o && (((signed char *)o)[0xED] || o->m_unkF5)) {
            m_homing = 1;
            m_dyn.turnRate = 0.08f;
        }
        if (m_type == 10) {
            m_age = 0;
            m_dyn.speed = 1000.0f;
        }
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type] ? ws->m_epNodes[m_type] : ws->m_epNodes[0]);
        break;
    }
    case 13: {
        char *st = (char *)Interactives::getInteractive(m_ownerId) + 0xFE5C;
        void *part;

        m_dyn.lifetime = *(float *)(st + 0x48) * 60.0f;
        m_dyn.speed = *(float *)(st + 0x4C);
        m_dyn.turnRate = *(float *)(st + 0x50);
        m_homing = m_targetId ? 1 : 0;
        m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 0x36, 1.0f, 0, 0, false, 0.0f);
        m_fx2 = particleCreateFx(&m_cs->trans, m_cs->mat, 0x15, 10.0f, 0, 0, false, 0.0f);
        part = particleGetParticle(m_fx2);
        if (part) {
            *(short *)((char *)part + 0x2D86) = 600;
            *(short *)((char *)part + 0x2E4A) = 60;
            *(float *)((char *)part + 0x2D90) = 0.125f;
        }
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type]);
        break;
    }
    case 0x15:
    case 0x16: {
        Monster *t = 0;
        Monster *o = (Monster *)Interactives::getInteractive(m_ownerId);
        char *lava;

        m_age = -6;
        m_wobble.x = 15.0f;
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
        if (m_targetId > 0) {
            DbInteractive *d = Interactives::getInteractive(m_targetId);

            if (d->m_kind == 1)
                t = (Monster *)d;
        }
        m_dyn.turnRate = ws->m_params[m_type].turnRate;
        m_homing = m_targetId ? 1 : 0;
        vecCopy(&m_targetPos, t ? &t->m_cs->trans : &o->m_reticleCS->trans);
        lava = (char *)o + 0x10930; /* the thrower's StateLavaBlast */
        m_dyn.damage = *(float *)(lava + 0x19C);
        m_dyn.speed = *(float *)(lava + 0x1A0);
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type]);
        m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 0x2F, 5.0f, 0, 0, false, 0.0f);
        break;
    }
    case 0x2A: {
        Monster *pl = game->m_lists.players[0];

        m_dyn.radius = 100.0f;
        m_dyn.damage = ws->m_params[m_type].damage;
        m_dyn.turnRate = 0.1f;
        m_dyn.speed = ws->m_params[m_type].speed;
        m_homing = 1;
        m_age = 0;
        if (pl)
            vecCopy(&m_targetPos, &pl->m_cs->trans);
        break;
    }
    case 0x2B:
        m_dyn.radius = 100.0f;
        m_dyn.damage = ws->m_params[m_type].damage;
        m_dyn.speed = ws->m_params[m_type].speed + mathfRandf(-50.0f, 50.0f);
        m_dyn.lifetime += mathfRandf(-10.0f, 10.0f);
        m_homing = 0;
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type]);
        break;
    case 0x2C: {
        Monster *t = game->m_lists.players[0];

        if (m_targetId > 0) {
            DbInteractive *d = Interactives::getInteractive(m_targetId);

            if (d->m_kind == 1)
                t = (Monster *)d;
        }
        m_homing = 1;
        m_dyn.turnRate = 0.03f;
        m_age = 0;
        if (t)
            vecCopy(&m_targetPos, &t->m_cs->trans);
        m_dyn.radius = 100.0f;
        m_dyn.damage = ws->m_params[m_type].damage;
        m_dyn.speed = ws->m_params[m_type].speed + mathfRandf(-50.0f, 50.0f);
        m_dyn.lifetime = mathfRandf(6.0f, 90.0f);
        hierSetCsEpNode(m_cs, ws->m_epNodes[m_type]);
        break;
    }
    }
    if (m_dyn.turnRate > 1.5f) {
        m_dyn.angle30 = 1.5707964f;
        m_dyn.turnRate = 1.0f;
        m_dyn.angle2C = 1.5707964f;
    } else if (m_dyn.turnRate > 1.0f) {
        m_dyn.turnRate = 1.0f;
        m_dyn.angle30 = 1.1780972f;
        m_dyn.angle2C = 1.1780972f;
    }
    if (m_dyn.pitchRate > 1.5f) {
        m_dyn.angle30 = 1.5707964f;
        m_dyn.pitchRate = 1.0f;
        m_dyn.angle2C = 1.5707964f;
    } else if (m_dyn.pitchRate > 1.0f) {
        m_dyn.pitchRate = 1.0f;
        m_dyn.angle30 = 1.1780972f;
        m_dyn.angle2C = 1.1780972f;
    }
    if (m_dyn.lightRange > 0.0f) {
        m_pointLight = pointLights.Create();
        if (m_pointLight >= 0) {
            pointLights.lights[m_pointLight].SetPos(&m_cs->trans);
            pointLights.lights[m_pointLight].SetColor(m_dyn.r, m_dyn.g, m_dyn.b);
            pointLights.lights[m_pointLight].SetRange(m_dyn.lightRange);
            pointLights.lights[m_pointLight].f38 = 1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", InitWeapon__6Weapon);
#endif
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
/* Points the ricochet particle fx along the reflection of the from->to direction off a surface of the given normal, at hitPos;
   the glancing angle scales its size. Returns the particle's matrix (0 when the fx is gone); no caller reads it. */
#ifdef NON_MATCHING
/* equivalent: 61/102 words, same size; register allocation (particle/normal in $s1/$s2) and the load order of the dot products */
void *ricochetParticlesOffSurface(int fx, _fvector *from, _fvector *to, _fvector *hitPos, _fvector *normal)
{
    char *p = (char *)particleGetParticle(fx);
    FVECTOR dir;
    FVECTOR push;
    FVECTOR refl;
    FVECTOR out;
    float x, y, z;
    float inv;
    float d;
    float e;

    if (!p)
        return 0;
    vecSub(&dir, from, to);
    x = dir.x;
    y = dir.y;
    z = dir.z;
    inv = eeRsqrtf(1.0f, sumSq3(x, y, z));
    dir.z = z * inv;
    dir.x = x * inv;
    dir.y = y * inv;
    d = vecDot(&dir, normal);
    vecScale(&push, normal, d + d);
    vecSub(&refl, &dir, &push);
    mathfMatrixFromNormal(PARTICLE_MAT(p), &refl);
    *(_fvector **)(p + 0x2D94) = PARTICLE_POS(p);
    vecCopy(PARTICLE_POS(p), hitPos);
    *(float (**)[4])(p + 0x2D98) = PARTICLE_MAT(p);
    vecScale(&out, normal, -d);
    e = vecDot(&refl, &out);
    *(float *)(p + 0x2D90) = e * 0.5f;
    *(float *)(p + 0x2D8C) *= e * 0.6f + 0.4f;
    return PARTICLE_MAT(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", ricochetParticlesOffSurface__FiP8_fvectorN31);
#endif
/* The explosion at res.point: per type its particles (most pointed off the surface by ricochetParticlesOffSurface), and a few
   specials: a thrown pickup impales a monster (the rigid pickup type 7) or is destroyed and credits the thrower's stamina, the
   generator pickup (0xE) starts its generator on a monster with m_unk49; type 8 an electric arc, 13 the owner's shockwave,
   0x27..0x29 a blast that sets the monster on fire, 0x2A/0x2B five more shots of the next type. A hit on an interactive gets a big
   hit (type 3, source 0xC) along the last move, unless it is a kind 3 object hit by a source-1 explosion. */
#ifdef NON_MATCHING
/* equivalent: untuned; the shockwave ring is copied by value as in retail (ShockWave::Activate takes it by value) */
struct ShockWaveConfig {
    int f0;
    float damage;
    float f8;
    float strength38;
    float strength34;
    float f14, f18;
};
template <class T>
class ExpandingRing {
public:
    int numRings;
    char pad4[0x20 - 0x4];
    T rings[20];      /* 0x20 */
    float reach[20];  /* 0x250 */
    float timeScale;  /* 0x2A0 */
};
typedef char _size_ShockRing[sizeof(ExpandingRing<ShockWaveConfig>) == 0x2A4 ? 1 : -1];
class SpecFxAnim {
public:
    static SpecFxAnim *getInstance(Monster *m, int id);
};
class ShockWave {
public:
    static void *Activate(_fvector *pos, ExpandingRing<ShockWaveConfig> cfg, HitEvent::DamageCause cause, int id, SpecFxAnim *anim);
};
void electricArc(_fvector *from, _fvector *to, unsigned colorA, unsigned colorB, float a, float b, float c, float d, int n,
                 unsigned seed, unsigned e);
extern "C" int rand(void);
void mathfRotationFromPointToPoint(_fvector *rot, _fvector *from, _fvector *to);
extern "C" unsigned pickupUpdateGenerator(void *p) __asm__("updateGenerator__6PickupPv");
void monsterAttachPickupImpaler(Monster *m, PickupIter it, _fvector *dir) __asm__("attachPickupImpaler__7MonsterGQ2t10LinkedList1ZP6Pickup8IteratorP8_fvector");
#define PU_FIELD(type, p, off) (*(type *)((char *)(p) + (off)))
#define HITEVENT_28(e) (*(int *)((char *)(e) + 0x28))

void Weapon::DetonateWeapon(_hdResult &res)
{
    HitEvent *e = 0;
    FVECTOR dir;
    int impaled = 0;
    int fx;
    float (*m)[4];

    if (m_type != 0xD && HD_ID2(res) != 0) {
        e = HitEvent::create();
        e->pos = &res.point;
        e->type = 3;
        e->source = 0xC;
        e->subtype = m_type;
        vecSub(&dir, &m_cs->trans, &m_prevPos);
    }
    switch (m_type) {
    case 0x17: {
        Pickup *p;

        if (e) {
            e->source = 0xB;
            HITEVENT_28(e) = ITER_PICKUP(m_pickup)->pickupType;
        }
        p = ITER_PICKUP(m_pickup);
        if (p->pickupType == 7 && HD_ID2(res) != 0 && p->cs != 0) {
            DbInteractive *t = Interactives::getInteractive(HD_ID2(res));

            ITER_PICKUP(m_pickup)->takeUseDamage(LevelPickups::s_info[ITER_PICKUP(m_pickup)->pickupType].useDamage);
            if (t->m_kind == 1) {
                impaled = 1;
                monsterAttachPickupImpaler((Monster *)t, m_pickup, &dir);
            }
        }
        if (!impaled)
            LevelPickups::killPickup(m_pickup, 1);
        if (HD_ID2(res) != 0) {
            Monster *o = (Monster *)Interactives::getInteractive(m_ownerId);

            o->creditStamina(LevelPickups::s_info[ITER_PICKUP(m_pickup)->pickupType].staminaGain, false);
        }
        if (ITER_PICKUP(m_pickup)->pickupType == 0xE) {
            int gen = 0;

            if (e)
                e->type = 5;
            if (HD_ID2(res) != 0) {
                DbInteractive *t = Interactives::getInteractive(HD_ID2(res));
                DbInteractive *o = Interactives::getInteractive(m_ownerId);

                if (t->m_kind == 1) {
                    PU_FIELD(DbInteractive *, ITER_PICKUP(m_pickup), 0xE0) = t;
                    PU_FIELD(DbInteractive *, ITER_PICKUP(m_pickup), 0xE4) = o;
                    if (((Monster *)t)->m_unk49)
                        gen = 1;
                    goto generator;
                }
            }
            vecCopy(&PU_FIELD(FVECTOR, ITER_PICKUP(m_pickup), 0xF0), &m_cs->trans);
            PU_FIELD(DbInteractive *, ITER_PICKUP(m_pickup), 0xE0) = 0;
        generator:
            if (gen) {
                TaskManager::global.add(pickupUpdateGenerator, ITER_PICKUP(m_pickup), 1);
                PU_FIELD(short, ITER_PICKUP(m_pickup), 0xDC) = 0;
            }
        }
        if (ITER_PICKUP(m_pickup)->pickupType == 4) {
            fx = particleCreateFx(&res.point, 0x52, 1.0f, 0, 0.0f);
            particleCreateFx(&res.point, 0x53, 1.0f, 0, 0.0f);
            particleCreateFx(&res.point, 0x1D, 2.0f, 0, 0.0f);
            ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        }
        break;
    }
    case 0x1A:
    case 0x1B:
    case 0x21:
    case 0x23:
        particleCreateFx(&res.point, 9, 0.5f, 0, 0.0f);
        fx = particleCreateFx(&res.point, 0x17, 14.0f, 0, 0.0f);
        goto ricochet;
    case 0x18:
    case 0x19:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
    case 0x22:
    case 0x24:
    case 0x25:
    case 0x26:
        if (e)
            e->source = 1;
        break;
    case 0:
        fx = particleCreateFx(&res.point, 0x4B, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x4A, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x25, 2.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1D, 2.0f, 0, 0.0f);
        goto ricochet;
    case 1:
    case 10:
        fx = particleCreateFx(&res.point, 0x4C, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1E, 1.0f, 0, 0.0f);
        goto ricochet;
    case 2:
        fx = particleCreateFx(&res.point, 0x4E, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x4D, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x4F, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x73, 1.0f, 0, 0.0f);
        goto ricochet;
    case 3:
        fx = particleCreateFx(&res.point, 0x51, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x50, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x24, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1D, 2.0f, 0, 0.0f);
        goto ricochet;
    case 4:
        fx = particleCreateFx(&res.point, 0x52, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x53, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1D, 2.0f, 0, 0.0f);
        goto ricochet;
    case 5:
        fx = particleCreateFx(&res.point, 0x54, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1D, 2.0f, 0, 0.0f);
        goto ricochet;
    case 6:
        fx = particleCreateFx(&res.point, 0x70, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1D, 2.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x74, 1.0f, 0, 0.0f);
        goto ricochet;
    case 7:
    case 11:
    case 20:
        fx = particleCreateFx(&res.point, 0x58, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x21, 1.5f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1F, 2.0f, 0, 0.0f);
        goto ricochet;
    case 0x2A:
    case 0x2B: {
        /* five more shots of the next type, spread around the line to player 1 */
        float scale = m_type == 0x2A ? 5.0f : 3.0f;
        float spread = m_type == 0x2A ? 45.0f : 60.0f;
        int next = m_type == 0x2A ? 0x2B : 0x2C;

        fx = particleCreateFx(&res.point, 0x4E, scale, 0, 0.0f);
        particleCreateFx(&res.point, 0x4D, scale, 0, 0.0f);
        particleCreateFx(&res.point, 0x4F, scale, 0, 0.0f);
        particleCreateFx(&res.point, 0x73, scale, 0, 0.0f);
        if (HD_ID1(res) == 0) {
            FVECTOR rot, r;
            int i;

            DispatchRangeBasedDamage();
            mathfRotationFromPointToPoint(&rot, &m_cs->trans, (_fvector *)((char *)game->m_lists.players[0] + 0x3E30));
            for (i = 0; i < 5; i++) {
                vecCopy(&r, &rot);
                r.x += mathfRandf(-25.0f, 25.0f) * 0.017453292f;
                r.z += mathfRandf(-spread, spread) * 0.017453292f;
                game->getWeapons()->CreateStaticWeapon((Weapon::WeaponType)next, m_ownerId, m_targetId, &m_cs->trans, &r, 0);
            }
        }
        if (res.hit)
            goto ricochet;
        break;
    }
    case 0x2C:
        fx = particleCreateFx(&res.point, 0x4E, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x4D, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x4F, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x73, 1.0f, 0, 0.0f);
        if (HD_ID1(res) == 0)
            DispatchRangeBasedDamage();
        if (!res.hit)
            break;
    ricochet:
        if (e)
            e->source = 1;
        ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        break;
    case 8: {
        FVECTOR end;

        if (e)
            e->source = 1;
        fx = particleCreateFx(&res.point, 0x17, 1.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x20, 3.0f, 0, 0.0f);
        ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        vecCopy(&end, &res.point);
        end.x = res.point.x + res.normal.x * 250.0f;
        end.y = res.point.y + res.normal.y * 250.0f;
        end.z = res.point.z + res.normal.z * 250.0f;
        electricArc(&res.point, &end, 0x80806040, 0x2800040, 0.0f, 6.0f, 0.5f, 0.5f, 8, rand(), 4);
        break;
    }
    case 9:
        if (e)
            e->source = 1;
        fx = particleCreateFx(&res.point, 0x6B, 1.0f, 0, 0.0f);
        m = (float (*)[4])ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        particleCreateFx(&res.point, 9, 5.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1B, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x1B, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x1B, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x1B, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        break;
    case 0x15:
    case 0x16:
        if (e) {
            e->source = 5;
            e->type = 2;
        }
        fx = particleCreateFx(&res.point, 0x6B, 1.0f, 0, 0.0f);
        m = (float (*)[4])ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        particleCreateFx(&res.point, 9, 5.0f, 0, 0.0f);
        particleCreateFx(&res.point, 0x1B, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x1B, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        break;
    case 13: {
        Monster *o = (Monster *)Interactives::getInteractive(m_ownerId);
        char *st = (char *)o + 0xFE5C;
        ExpandingRing<ShockWaveConfig> *ring = (ExpandingRing<ShockWaveConfig> *)(st + 0x58);
        SpecFxAnim *a = SpecFxAnim::getInstance(o, 0x11FF);
        float r;

        if (!a)
            break;
        animationSetSpeed(*(_animHandle *)((char *)a + 0x10), *(float *)(st + 0x444));
        r = ring->reach[ring->numRings - 1];
        ring->timeScale = r / (r * 240.0f * 0.001f / 240.0f * 240.0f);
        ShockWave::Activate(&m_cs->trans, *ring, (HitEvent::DamageCause)5, m_ownerId, a);
        vecCopy(&(*(_cs **)((char *)a + 0x38))->trans, &m_cs->trans);
        fx = particleCreateFx(&res.point, 0x16, 200.0f, 0, 0.0f);
        m = (float (*)[4])ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        particleCreateFx(&res.point, 0x6A, 1.0f, m, 0.0f);
        break;
    }
    case 0x27:
    case 0x28:
    case 0x29:
        DispatchRangeBasedDamage();
        if (e)
            e->source = 5;
        fx = particleCreateFx(&res.point, 0x6B, 2.0f, 0, 0.0f);
        ricochetParticlesOffSurface(fx, &m_cs->trans, &m_prevPos, &res.point, &res.normal);
        particleCreateFx(&res.point, 9, 10.0f, 0, 0.0f);
        if (HD_ID2(res) != 0) {
            Monster *t = (Monster *)Interactives::getInteractive(HD_ID2(res));

            if (((DbInteractive *)t)->m_kind == 1) {
                char *o = (char *)Interactives::getInteractive(m_ownerId);

                if (t->m_health > 0.0f) {
                    if (*(int *)(o + 0x14) == 0x100) {
                        char *lava = o + 0x10930;

                        t->lightOnFire(*(float *)(lava + 0x1A8), *(float *)(lava + 0x1A4), *(int *)(o + 0x20));
                    } else {
                        t->lightOnFire(300.0f, 1.0f, *(int *)(o + 0x20));
                    }
                }
            }
        }
        break;
    case 14:
        particleKillFx(m_fx);
        m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 0x6D, 1.0f, 0, 0, false, 0.0f);
        break;
    default:
        particleCreateFx(&res.point, 0x1A, 1.0f, 0, 0.0f);
        break;
    }
    if (m_type != 0xD && HD_ID2(res) != 0) {
        if (e->source != 1 || Interactives::getInteractive(HD_ID2(res))->m_kind != 3)
            Interactives::dispatchHit(&dir, m_dyn.damage, m_ownerId, HD_ID2(res));
    }
    m_sound.playWeaponExplosion(&m_cs->trans, m_type, impaled);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", DetonateWeapon__6WeaponR9_hdResult);
#endif
/* Blast damage: every interactive but the thrower within m_dyn.radius (Manhattan) of the weapon takes a blockable hit
   (a big one for types 0x2B and 0x2C) along the weapon's last move. */
void Weapon::DispatchRangeBasedDamage(void)
{
    int n = Interactives::s_numInteractives;
    int i;

    for (i = 1; i < n; i++) {
        DbInteractive *d = Interactives::getInteractive(i);
        _fvector *pos;

        if (m_ownerId == i)
            continue;
        if (!d->m_cs)
            continue;
        pos = &d->m_cs->trans;
        if (pos && mathfManhatDist(&m_cs->trans, pos) < m_dyn.radius) {
            HitEvent *e = HitEvent::create();
            FVECTOR dir;

            e->pos = pos;
            e->type = 2;
            e->source = 12;
            e->subtype = m_type;
            if ((unsigned)(m_type - 0x2B) < 2)
                e->type = 3;
            vecSub(&dir, &m_cs->trans, &m_prevPos);
            Interactives::dispatchHit(&dir, m_dyn.damage, m_ownerId, i);
        }
    }
}
/* Velocity per second from the last frame's move. */
void Weapon::GetVel(_fvector &v)
{
    vecSubNR(&v, &m_cs->trans, &m_prevPos);
    vecScale(&v, &v, (float)timerGetUpdateRate());
}
void Weapons::DecrementWeaponCount(Monster *m, int type)
{
}
/* Model of a weapon type, from the level db (node db ids 4000..4042; only the first instance, id2 0, is kept). */
void Weapons::AddWeaponEpNode(int idx, _hierhead *h)
{
    if (h->id2)
        return;
    switch (idx) {
    case 4000:
        m_epNodes[24] = h;
        break;
    case 4002:
        m_epNodes[23] = h;
        break;
    case 4001:
        m_epNodes[0] = h;
        break;
    case 4003:
        m_epNodes[1] = h;
        break;
    case 4008:
        m_epNodes[2] = h;
        break;
    case 4009:
        m_epNodes[3] = h;
        break;
    case 4010:
        m_epNodes[4] = h;
        break;
    case 4011:
        m_epNodes[5] = h;
        break;
    case 4012:
        m_epNodes[6] = h;
        break;
    case 4013:
        m_epNodes[7] = h;
        break;
    case 4014:
        m_epNodes[8] = h;
        break;
    case 4018:
        m_epNodes[9] = h;
        break;
    case 4019:
        m_epNodes[21] = h;
        break;
    case 4021:
        m_epNodes[22] = h;
        break;
    case 4028:
        m_epNodes[10] = h;
        break;
    case 4005:
        m_epNodes[13] = h;
        break;
    case 4015:
        m_epNodes[20] = h;
        break;
    case 4017:
        m_epNodes[11] = h;
        break;
    case 4022:
        m_epNodes[39] = h;
        break;
    case 4023:
        m_epNodes[40] = h;
        break;
    case 4024:
        m_epNodes[41] = h;
        break;
    case 4025:
        m_epNodes[25] = h;
        break;
    case 4026:
        m_epNodes[26] = h;
        break;
    case 4027:
        m_epNodes[30] = h;
        break;
    case 4029:
        m_epNodes[28] = h;
        break;
    case 4030:
        m_epNodes[32] = h;
        break;
    case 4031:
        m_epNodes[33] = h;
        break;
    case 4032:
        m_epNodes[36] = h;
        m_epNodes[38] = h;
        break;
    case 4033:
        m_epNodes[27] = h;
        break;
    case 4034:
        m_epNodes[31] = h;
        break;
    case 4035:
        m_epNodes[29] = h;
        break;
    case 4036:
        m_epNodes[34] = h;
        break;
    case 4037:
        m_epNodes[35] = h;
        break;
    case 4038:
        m_epNodes[37] = h;
        break;
    case 4040:
        m_epNodes[42] = h;
        break;
    case 4041:
        m_epNodes[43] = h;
        break;
    case 4042:
        m_epNodes[44] = h;
        break;
    }
}
/* retail pads the jump table of AddWeaponEpNode to an even word count */
__asm__(".section .rodata\n\t.word 0\n\t.text");

/* A ground fire at pos (radius r) credited to interactive id; when all ten are burning, slot 0 is taken over. */
#ifdef NON_MATCHING
/* equivalent: 81/95 words, same size; pos and id swap $s1/$s2 */
void Weapons::CreateFire(_fvector *pos, float r, int id)
{
    int i;
    WeaponFlame *f;

    for (i = 0; i < 10; i++)
        if (!m_flames[i].active)
            break;
    if (i == 10) {
        i = 0;
        if (m_flames[0].light >= 0) {
            PointLight *pl = &pointLights.lights[m_flames[0].light];
            pl->active = 0;
            pl->f38 = 0;
        }
    }
    f = &m_flames[i];
    f->active = 1;
    f->age = 0;
    f->radius = r;
    f->fx = particleCreateFadingFlame(pos, 1800, 0, r / 25.0f);
    f->ownerId = id;
    vecCopy(&f->pos, pos);
    f->light = pointLights.Create();
    if (f->light >= 0) {
        pointLights.lights[f->light].SetPos(pos->x, pos->y, pos->z + 50.0f);
        pointLights.lights[f->light].SetColor(128.0f, 35.0f, 0);
        pointLights.lights[f->light].f38 = 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateFire__7WeaponsP8_fvectorfi);
#endif
/* Burns the living monsters inside each fire, keeps its light on the flame particle and puts it out after 1800 fields. */
#ifdef NON_MATCHING
/* equivalent: 115/160 words, same size; register allocation (this in $s5, game kept in $a1 instead of reloaded) */
void Weapons::UpdateFlames(void)
{
    int i;
    int j;

    for (i = 0; i < 10; i++) {
        if (!m_flames[i].active)
            continue;
        for (j = 0; j < game->m_numSlots; j++) {
            Monster *m = &game->m_slots[j];
            FVECTOR d;

            vecSub(&d, &m_flames[i].pos, &m->m_cs->trans);
            if (vecLenSqX(&d) < m_flames[i].radius * m_flames[i].radius && 0.0f < m->m_health)
                m->lightOnFire((float)GAME_FIELD(int, 0x1204A0), 2.0f, m_flames[i].ownerId);
        }
        m_flames[i].sound.updateGenericSound(&m_flames[i].pos);
        char *p = (char *)particleGetParticle(m_flames[i].fx);
        if (p) {
            float z = *(float *)(p + 0x2D08);
            if (m_flames[i].pos.z != z)
                m_flames[i].pos.z = z;
            if (m_flames[i].light >= 0)
                pointLights.lights[m_flames[i].light].SetPos(m_flames[i].pos.x, m_flames[i].pos.y, m_flames[i].pos.z + 50.0f);
        }
        if (++m_flames[i].age > 1800) {
            m_flames[i].active = 0;
            m_flames[i].age = 0;
            particleKillFx(m_flames[i].fx);
            m_flames[i].sound.terminateGenericSound();
            if (m_flames[i].light >= 0) {
                PointLight *pl = &pointLights.lights[m_flames[i].light];
                pl->active = 0;
                pl->f38 = 0;
                m_flames[i].light = -1;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Weapon", UpdateFlames__7Weapons);
#endif

