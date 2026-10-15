#include "common.h"
#include "game/game.h"
#include "hd.h"
#include "cs_pool.h"
#include "vecmath.h"
#include "game/hit_event.h"
#include "game/pickup.h"

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

#ifdef NON_MATCHING
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
        w->m_dyn.f1C = 0;
        w->m_dyn.speed = 0;
        w->m_48 = 0;
        w->m_targetPoint = 0;
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
        d->f1C = 0;
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
    m_params[13].f1C = 0.01f;
    m_params[13].speed = 700.0f;

    m_params[21].damage = 1.0f;
    m_params[21].lifetime = 8.0f;
    m_params[21].f18 = 1.0f;
    m_params[21].f1C = 0.02f;
    m_params[21].speed = 500.0f;

    m_params[22].damage = 1.0f;
    m_params[22].lifetime = 8.0f;
    m_params[22].f18 = 1.0f;
    m_params[22].f1C = 0.02f;
    m_params[22].speed = 500.0f;

    m_params[23].damage = 5.0f;
    m_params[23].lifetime = 10.0f;
    m_params[23].f18 = 1.0f;
    m_params[23].f1C = 0;
    m_params[23].speed = 800.0f;

    m_params[24].damage = 1.0f;
    m_params[24].lifetime = 3.0f;
    m_params[24].f18 = 1.0f;
    m_params[24].f1C = 0;
    m_params[24].speed = 800.0f;

    m_params[25].damage = 3.0f;
    m_params[25].lifetime = 2.0f;
    m_params[25].f18 = 1.0f;
    m_params[25].f1C = 0;
    m_params[25].speed = 1400.0f;

    m_params[26].damage = 8.0f;
    m_params[26].lifetime = 5.0f;
    m_params[26].f18 = 0.05f;
    m_params[26].f1C = 0.08f;
    m_params[26].speed = 700.0f;

    m_params[27].damage = 8.0f;
    m_params[27].lifetime = 5.0f;
    m_params[27].f18 = 0.05f;
    m_params[27].f1C = 1.5f;
    m_params[27].speed = 700.0f;
    m_params[27].f28 = 1.75f;

    m_params[28].damage = 3.0f;
    m_params[28].lifetime = 2.0f;
    m_params[28].f18 = 1.0f;
    m_params[28].f1C = 0;
    m_params[28].speed = 1400.0f;

    m_params[29].damage = 3.0f;
    m_params[29].lifetime = 2.0f;
    m_params[29].f18 = 1.0f;
    m_params[29].f1C = 0.2f;
    m_params[29].speed = 1400.0f;
    m_params[29].f28 = 0.4f;

    m_params[30].damage = 1.0f;
    m_params[30].lifetime = 2.0f;
    m_params[30].f18 = 1.0f;
    m_params[30].f1C = 0;
    m_params[30].speed = 800.0f;

    m_params[31].damage = 1.0f;
    m_params[31].lifetime = 2.0f;
    m_params[31].f18 = 1.0f;
    m_params[31].f1C = 0.2f;
    m_params[31].speed = 800.0f;
    m_params[31].f28 = 0.4f;

    m_params[32].damage = 3.0f;
    m_params[32].lifetime = 2.0f;
    m_params[32].f18 = 1.0f;
    m_params[32].f1C = 0;
    m_params[32].speed = 1400.0f;

    m_params[33].damage = 3.0f;
    m_params[33].lifetime = 2.0f;
    m_params[33].f18 = 1.0f;
    m_params[33].f1C = 0;
    m_params[33].speed = 1400.0f;

    m_params[34].damage = 3.0f;
    m_params[34].lifetime = 2.0f;
    m_params[34].f18 = 1.0f;
    m_params[34].f1C = 0;
    m_params[34].speed = 1400.0f;

    m_params[35].damage = 3.0f;
    m_params[35].lifetime = 2.0f;
    m_params[35].f18 = 1.0f;
    m_params[35].f1C = 0;
    m_params[35].speed = 1400.0f;

    m_params[36].damage = 1.0f;
    m_params[36].lifetime = 2.0f;
    m_params[36].f18 = 1.0f;
    m_params[36].f1C = 0;
    m_params[36].speed = 1000.0f;

    m_params[37].damage = 1.0f;
    m_params[37].lifetime = 2.0f;
    m_params[37].f18 = 1.0f;
    m_params[37].f1C = 0;
    m_params[37].speed = 1000.0f;

    m_params[38].damage = 1.0f;
    m_params[38].lifetime = 2.0f;
    m_params[38].f18 = 1.0f;
    m_params[38].f1C = 0.07f;
    m_params[38].speed = 1200.0f;

    m_params[42].damage = 30.0f;
    m_params[42].lifetime = 0.05f;
    m_params[42].f18 = 1.0f;
    m_params[42].f1C = 0;
    m_params[42].speed = 400.0f;

    m_params[43].damage = 10.0f;
    m_params[43].lifetime = 1.2f;
    m_params[43].f18 = 1.0f;
    m_params[43].f1C = 0;
    m_params[43].speed = 250.0f;

    /* faster in match mode */
    m_params[44].damage = (float)game->m_matchMode + 1.75f;
    m_params[44].lifetime = 4.0f;
    m_params[44].f18 = 1.0f;
    m_params[44].f1C = 0.04f;
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
INCLUDE_ASM("asm/nonmatchings/game/Weapon", ComputeReflection__6WeaponP8_fvectorT1b);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateStaticWeapon__7WeaponsQ26Weapon10WeaponTypeiiP8_fvectorT4P9_hierhead);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateStaticWeapon__7WeaponsGQ2t10LinkedList1ZP6Pickup8IteratoriiP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Weapon", CreateLavaBall__7WeaponsP8_fvectorN21ii);
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
INCLUDE_ASM("asm/nonmatchings/game/Weapon", AddWeaponEpNode__7WeaponsiP9_hierhead);
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

