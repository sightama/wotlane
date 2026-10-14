#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/hit_history.h"
#include "game/level_pickups.h"
#include "task_manager.h"
#include "cs_pool.h"
#include "hd.h"
#include "vecmath.h"

/* Flying chunks of a broken level object. A destructible's DEBRIS action (ActDebris) takes one of 32 pool slots: each chunk
 * of the action gets a cs from CsPool, is placed by the destructible's matrix, thrown along the action's direction (force /
 * mass) and spun; update (a task on TaskManager::debris) moves them under gravity, bounces them off the ground, hurts the
 * monsters they hit (ChunkHD) and, when they settle or the slot's life runs out, turns pickup chunks into real pickups and
 * frees the cs. CullView (from rt) hides the chunks of slots outside the view. */

extern "C" int printf(const char *, ...);
extern "C" float fabsf(float);

/* one chunk of a DEBRIS action (level data) */
struct ActDebrisChunk {
    _hierhead *hier;                    /* 0x00: TRANSLATE/ROTATE node: its child is used */
    float mass;                         /* 0x04: raised to 15000 by Activate (written back into the level data) */
    float spin;                         /* 0x08: random spin, 0 = none */
    unsigned short life : 16;       /* 0x0C: seconds the slot lives (only chunk 0's is read) */
    unsigned short kind : 3;        /* 0x0E: 1 lands at the action's height, 2 dies under the default height, 4 thrown pickup, else hat test */
    unsigned short : 2;
    unsigned short pickup : 5;      /* ePickupType the chunk turns into when it settles (0 none) */
    unsigned short damage : 6;      /* damage to monsters it touches (times m_damageMod) */
    float bounce;                       /* 0x10: vertical restitution; below 0.5 it settles after 2 bounces, else after 4 */
    float radius;                       /* 0x14: of the damage sphere */
    int dir;                            /* 0x18: 4 signed bytes, direction (/127) */
    int rot;                            /* 0x1C: 4 signed bytes, start rotation quaternion (/127) */
    FVECTOR offset;                     /* 0x20: from the destructible, in its space */
};
typedef char _size_ActDebrisChunk[sizeof(ActDebrisChunk) == 0x30 ? 1 : -1];

struct ActDebris {
    unsigned short type;                /* 0x0 */
    unsigned short pad2;
    float delay;                        /* 0x4 */
    int numChunks;                      /* 0x8 */
    float force;                        /* 0xC */
    ActDebrisChunk chunks[1];           /* 0x10 */
};

/* the first 8 bytes of an _hdResult (hit, surface, ids) as one 64-bit word */
struct HdResultIds {
    unsigned long hit : 8;
    unsigned long pad : 8;
    unsigned long surface : 16;
    unsigned long id2 : 11;
    unsigned long id1 : 14;
};

class DbInteractive {
public:
    int m_kind;               /* 0x00: 1 monster */
    char pad4[8];
    _cs *m_cs;                /* 0x0C */
};

class Interactives {
public:
    static DbInteractive *getInteractive(int i);
};

class RigidPickup {
public:
    void huck(_fvector &dir, DbInteractive *target);
};

PickupIter levelPickupsCreatePickup(_hierhead *h, _fvector &pos, float (*m)[4], bool b, ePickupType t)
    __asm__("createPickup__12LevelPickupsP9_hierheadR8_fvectorPA3_fb11ePickupType");

int hdSphereTest(_fvector *p, float r, _hdResult *res, unsigned skipId2, int max);
int hdSphereTest(_fvector *a, _fvector *b, float r, _hdResult *res, unsigned skipId2, int max);
_hdResult *hdGetNextCollis(_hdResult *res, int n, _hdResult *prev);
int viewGetNumViews(void);
int viewInFOV(int view, _fvector *a, _fvector *b);
int timerGetFieldCount(void);
float timerGetOverloadScale(void);
float smoothEasyInAngle(float cur, float target, float rate, float eps);
void mathfMulMatrix(float (*d)[4], float (*a)[4], float (*b)[4]);
void mathfMatrixToQuaternion(_fvector *q, float (*m)[4]);
void mathfQuaternionMulVec(_fvector *dst, _fvector *q, _fvector *v);
void mathfRandVector(_fvector *v, float scale);
void mathfNormalizeQuaternionFromW(_fvector *dst, _fvector *src);
void mathfVectorMinMaxGetMac(_fvector *min, _fvector *max);
extern float UNIT_MATRIX[4][4];
extern TaskManager debrisTasks __asm__("_11TaskManager$debris");

class Debris {
public:
    /* one flying chunk (0xA0) */
    struct ChunkData {
        _cs *cs;                        /* 0x00: 0 when free */
        int bounces;                    /* 0x04 */
        ActDebrisChunk *data;           /* 0x08 */
        HitHistory hits;                /* 0x0C: monsters already hurt */
        char pad14[0x60 - 0x14];
        FVECTOR vel;                    /* 0x60: per field */
        FVECTOR quat;                   /* 0x70: orientation */
        FVECTOR spin;                   /* 0x80: rotation per frame (quaternion) */
        FVECTOR lastPos;                /* 0x90: where the last hat test found it */
    };

    int m_active;                       /* 0x00 */
    int m_time;                         /* 0x04: fields since Activate */
    int m_life;                         /* 0x08: fields */
    int m_numChunks;                    /* 0x0C */
    float m_groundZ;                    /* 0x10: height of the destructible (kind 1 lands there) */
    int m_slot;                         /* 0x14: index % 4: the frame (of 4) its hat tests run */
    char pad18[8];
    FVECTOR m_center;                   /* 0x20: bounding box of the chunks, for CullView */
    FVECTOR m_extent;                   /* 0x30: half size */
    ChunkData m_chunks[32];             /* 0x40 */

    static float m_damageMod;
    static float m_damageThreshold;

    static void InitBefore(void);
    static void InitAfter(void);
    static void Activate(ActDebris *act, float (*m)[4], _fvector *pos, float (*mat)[4][4], float speed, _fvector *rot);
    static void CullView(int view);
    unsigned update(void);
    void ChunkHD(int i);
    float ChunkHat(ChunkData *c);
    static unsigned update(void *p);
};
typedef char _size_ChunkData[sizeof(Debris::ChunkData) == 0xA0 ? 1 : -1];
typedef char _size_Debris[sizeof(Debris) == 0x1440 ? 1 : -1];
/* Debris::m_debrisPool, declared once the class is complete (inside it gcc takes the array as unaligned: swl/swr) */
extern Debris debrisPool[32] __asm__("_6Debris$m_debrisPool");
#define m_debrisPool debrisPool

#define DEBRIS_SOUNDS() ((LevelObjectSoundManager *)((char *)game + 0x121570))

void Debris::InitBefore(void)
{
    int i;

    for (i = 0; i < 32; i++) {
        m_debrisPool[i].m_active = 0;
        m_debrisPool[i].m_slot = i % 4;
    }
    m_damageMod = 0.25f;
    m_damageThreshold = 5.0f;
}

#ifdef NON_MATCHING
/* 17/22 words, untuned: outer pointer/next-slot registers allocated in another order */
void Debris::InitAfter(void)
{
    Debris *d = m_debrisPool;
    int i, j;

    for (i = 0; i < 32; i++, d++)
        for (j = 31; j >= 0; j--)
            d->m_chunks[j].data = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Debris", InitAfter__6Debris);
#endif

#ifdef NON_MATCHING
/* 4 signed bytes -> float vector (xyzw) */
static inline void vecFromS8(_fvector *dst, int packed)
{
    int t;

    __asm__ volatile("pextlb %1, %2, $0\n\t"
                     "pextlh %1, %1, $0\n\t"
                     "psraw %1, %1, 24\n\t"
                     "qmtc2.ni %1, $vf11\n\t"
                     "vitof0.xyzw $vf11, $vf11\n\t"
                     "sqc2 $vf11, %0"
                     : "=m"(*dst), "=&r"(t) : "r"(packed));
}
/* vecScale on all four components */
static inline void vecScale4(_fvector *dst, _fvector *a, float s)
{
    register int t __asm__("$2");

    __asm__ volatile(".set push\n\t.set noreorder\n\t"
                     "lqc2 $vf11, 0x0(%2)\n\t"
                     "mfc1 %1, %3\n\t"
                     "qmtc2.ni %1, $vf12\n\t"
                     "vmulx.xyzw $vf11, $vf11, $vf12x\n\t"
                     "sqc2 $vf11, %0\n\t"
                     ".set pop"
                     : "=m"(*dst), "=r"(t) : "r"(a), "f"(s));
}
#endif

/* Takes a free slot for the action's chunks (at most 32). Under load (overload scale > 0.75) every odd chunk without a pickup
 * is skipped. speed and rot (the breaking monster's) are not used. */
#ifdef NON_MATCHING
/* 5/359 words, untuned: retail keeps the slot pointer and the scratch vector addresses in stack slots, different stack layout */
void Debris::Activate(ActDebris *act, float (*m)[4], _fvector *pos, float (*mat)[4][4], float speed, _fvector *rot)
{
    FMATRIX world;
    FMATRIX local;
    FVECTOR v;
    FVECTOR offset;
    FVECTOR q;
    FVECTOR start;
    FVECTOR dir;
    FVECTOR hvel;
    FMATRIX hmat;
    Debris *d = m_debrisPool;
    ChunkData *c;
    int overloaded;
    int i;

    for (i = 0; i < 32 && d->m_active; i++)
        d++;
    if (i == 32) {
        printf("Warning: Unable to activate debris system.  All available systems are active.\n");
        return;
    }
    d->m_time = 0;
    d->m_active = 1;
    d->m_groundZ = pos->z;
    d->m_life = act->chunks[0].life * 60;
    d->m_numChunks = act->numChunks;
    if (act->numChunks > 32) {
        printf("Warning: Too many chunks (%d) in debris system.  Using first %d chunks.\n", act->numChunks, 32);
        d->m_numChunks = 32;
    }
    mathfMulMatrix(world, *mat, m);
    c = d->m_chunks;
    overloaded = 1;
    if (!(timerGetOverloadScale() > 0.75f))
        overloaded = 0;
    for (i = 0; i < d->m_numChunks; i++, c++) {
        ActDebrisChunk *desc = &act->chunks[i];
        _cs *cs;
        _hierhead *h;

        c->data = desc;
        if (overloaded && (i & 1) && !desc->pickup)
            continue;
        cs = CsPool::csActivate();
        c->cs = cs;
        if (cs == 0) {
            printf("No more CSs available for debris chunks\n");
            d->m_numChunks = i;
            continue;
        }
        cs->testCollision = 0;
        cs->drawMe = 1;
        h = desc->hier;
        if (h->opcode == TRANSLATE_NODE)
            h = ((HierTranslate *)h)->child[0];
        else if (h->opcode == ROTATE_NODE)
            h = ((HierRotate *)h)->child[0];
        hierSetCsEpNode(cs, h);
        matCopy(local, world);
        h = cs->epNode;
        if (h->opcode == GROUP_NODE) {
            HierHead *k = ((HierGroup *)h)->child[0];
            if (k->opcode == SWITCH_NODE)
                ((HierSwitch *)k)->whichChild = 1;
        }
        vecApplyRows(&offset, *mat, &desc->offset);
        vecAdd(&cs->trans, &offset, pos);
        vecCopy(&c->lastPos, &cs->trans);
        mathfMatrixToQuaternion(&q, local);
        vecFromS8(&start, desc->rot);
        vecScale4(&start, &start, 1.0f / 127.0f);
        mathfConcatQuaternions(&c->quat, &start, &q);
        mathfQuaternionToMatrix4x4(cs->mat, &c->quat);
        vecFromS8(&dir, desc->dir);
        vecScale(&dir, &dir, 1.0f / 127.0f);
        if (desc->mass < 15000.0f)
            desc->mass = 15000.0f;
        vecScale(&v, &dir, act->force / desc->mass / (float)timerGetUpdateRate());
        mathfQuaternionMulVec(&c->vel, &q, &v);
        if (desc->spin != 0.0f) {
            mathfRandVector(&c->spin, 1.0f);
            c->spin.w = 1.0f - mathfRandf(0.0f, desc->spin * 0.5f) / 180.0f * 60.0f;
            mathfNormalizeQuaternionFromW(&c->spin, &c->spin);
        } else {
            vecStoreUnitW(&c->spin);
        }
        c->bounces = 0;
        if (desc->kind == 4) {
            PickupIter it;

            mathfQuaternionToMatrix4x4(hmat, &c->quat);
            vecScale(&v, &dir, act->force / desc->mass);
            mathfQuaternionMulVec(&hvel, &c->quat, &v);
            it = levelPickupsCreatePickup(cs->epNode, cs->trans, hmat, false, (ePickupType)desc->pickup);
            ((RigidPickup *)*(void **)it.cur)->huck(hvel, 0);
            LevelPickups::dropPickup(it);
            CsPool::csDeactivate(cs);
            c->cs = 0;
        }
    }
    debrisTasks.add(update, d, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Debris", Activate__6DebrisP9ActDebrisPA3_fP8_fvectorPA3_A3_ffT3);
#endif

/* Visible slots draw all their chunks (the split-screen test always ends visible too); the others hide them. */
#ifdef NON_MATCHING
/* 0/76 words: retail keeps a never-set flag (a3 = 0) and the numViews test in the loop; gcc folds them, drawMe = 1 is the same */
void Debris::CullView(int view)
{
    int multi = viewGetNumViews() >= 2;
    int never = 0;
    Debris *d = m_debrisPool;
    int i, j;

    for (i = 0; i < 32; i++, d++) {
        ChunkData *c;

        if (!d->m_active)
            continue;
        c = d->m_chunks;
        if (viewInFOV(view, &d->m_center, &d->m_extent)) {
            for (j = 0; j < d->m_numChunks; j++, c++) {
                if (c->cs)
                    c->cs->drawMe = !multi || c->data->pickup || !never;
            }
        } else {
            for (j = d->m_numChunks; j != 0; j--, c++) {
                if (c->cs)
                    c->cs->drawMe = 0;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Debris", CullView__6Debrisi);
#endif

/* Moves the chunks one frame; returns 0 (task done) when the slot's life is over. */
#ifdef NON_MATCHING
/* 20/349 words, untuned: register allocation (s3/s4 swapped, the die flag spilled) and the first-chunk search loop shape */
unsigned Debris::update(void)
{
    ChunkData *c = m_chunks;
    int i;

    m_time += timerGetFieldsLastFrame();
    if (m_time < m_life) {
        for (i = 0; i < m_numChunks; i++, c++) {
            if (c->cs == 0)
                continue;
            vu0BBoxInit(&c->cs->trans);
            for (; i < m_numChunks; i++, c++) {
                _cs *cs = c->cs;
                int die = 0;
                float hat;

                if (cs == 0)
                    continue;
                vecAdd(&cs->trans, &cs->trans, &c->vel);
                c->vel.z -= (float)timerGetFieldsLastFrame() * 0.067f;
                mathfConcatQuaternions(&c->quat, &c->quat, &c->spin);
                mathfQuaternionToMatrix4x4(cs->mat, &c->quat);
                vu0BBoxAdd(&cs->trans);
                hat = 0.0f;
                switch (c->data->kind) {
                case 1:
                    hat = cs->trans.z - m_groundZ;
                    break;
                case 2:
                    hat = 1000.0f;
                    if (cs->trans.z < game->m_defaultHeight)
                        die = 1;
                    break;
                default:
                    if (m_slot == timerGetFieldCount() % 4)
                        hat = ChunkHat(c);
                    break;
                }
                if (c->bounces) {
                    vecStoreUnitW(&c->spin);
                    c->quat.w = smoothEasyInAngle(c->quat.w, 1.0f, 0.1f, 0.001f);
                    mathfNormalizeQuaternionFromW(&c->quat, &c->quat);
                    mathfQuaternionToMatrix4x4(cs->mat, &c->quat);
                }
                if (hat < 0.0f) {
                    int lively = c->data->bounce < 0.5f ? c->bounces < 2 : c->bounces < 4;

                    c->vel.x *= lively ? 0.65f : 0.5f;
                    c->vel.y *= lively ? 0.65f : 0.5f;
                    if (c->vel.z < 0.0f) {
                        if (lively) {
                            c->vel.z *= -c->data->bounce;
                            cs->trans.z -= hat;
                            DEBRIS_SOUNDS()->playDebrisImpactSound(&cs->trans, c->data->pickup);
                        } else {
                            c->vel.z = 0.0f;
                            cs->trans.z = smoothEasyIn(cs->trans.z, cs->trans.z - hat, 0.1f, 0.01f);
                            if (-5.0f < hat && c->data->pickup)
                                die = 1;
                        }
                        c->bounces++;
                        vecScale(&c->spin, &c->spin, 0.5f);
                        c->lastPos.z = cs->trans.z;
                    }
                }
                if (c->data->damage && c->bounces == 0)
                    ChunkHD(i);
                if (die) {
                    if (c->data->pickup)
                        levelPickupsCreatePickup(cs->epNode, cs->trans, UNIT_MATRIX, false, (ePickupType)c->data->pickup);
                    CsPool::csDeactivate(cs);
                    c->cs = 0;
                    c->data = 0;
                }
            }
            break;
        }
        mathfVectorMinMaxGetMac(&m_center, &m_extent);
        {
            /* min/max -> center and half size grown by 20 */
            FVECTOR grow = { 20.0f, 20.0f, 20.0f, 0.5f };

            __asm__ volatile("lqc2 $vf12, %2\n\t"
                             "lqc2 $vf31, 0x0(%3)\n\t"
                             "vmulaw.xyz $ACC, $vf30, $vf12w\n\t"
                             "vmaddw.xyz $vf11, $vf31, $vf12w\n\t"
                             "vadda.xyz $ACC, $vf12, $vf0\n\t"
                             "vmaddaw.xyz $ACC, $vf31, $vf12w\n\t"
                             "vmsubw.xyz $vf12, $vf30, $vf12w\n\t"
                             "sqc2 $vf11, %0\n\t"
                             "sqc2 $vf12, %1"
                             : "=m"(m_center), "=m"(m_extent) : "m"(grow), "r"(&m_extent));
        }
        return 1;
    }
    m_active = 0;
    for (i = m_numChunks; i != 0; i--, c++) {
        if (c->cs) {
            if (c->data->pickup)
                levelPickupsCreatePickup(c->cs->epNode, c->cs->trans, UNIT_MATRIX, false, (ePickupType)c->data->pickup);
            CsPool::csDeactivate(c->cs);
            c->cs = 0;
            c->data = 0;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Debris", update__6Debris);
#endif

/* The chunk hurts the monsters its sphere touches, each once, when the damage is above the threshold. */
#ifdef NON_MATCHING
/* 80/88 words, untuned: the andi of the hit id is scheduled after the call's other args */
void Debris::ChunkHD(int i)
{
    ChunkData *c = &m_chunks[i];
    FVECTOR p;
    _hdResult res[100];
    _hdResult *r;
    int n, j;

    vecCopy(&p, &c->cs->trans);
    n = hdSphereTest(&p, c->data->radius, res, 0, 100);
    if (n <= 0)
        return;
    for (j = 0, r = res; j < n; j++, r++) {
        if (c->hits.newHit(((HdResultIds *)r)->id2, false) && m_damageThreshold < c->data->damage * m_damageMod &&
            Interactives::getInteractive(((HdResultIds *)r)->id2)->m_kind == 1)
            ((Monster *)Interactives::getInteractive(((HdResultIds *)r)->id2))->takeDamage(c->data->damage * m_damageMod, false, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Debris", ChunkHD__6Debrisi);
#endif

/* Height of the chunk above the first hat (ground) under it, along its fall since the last test; hats of id1 10052 and of
 * interactives that have a cs are ignored. 0 when nothing was found; a nonzero height is lowered by 5. */
#ifdef NON_MATCHING
/* 36/103 words, untuned: fall-distance math scheduled differently; retail extracts id2 from the 64-bit word it already loaded */
float Debris::ChunkHat(ChunkData *c)
{
    _cs *cs = c->cs;
    FVECTOR a;
    FVECTOR b;
    _hdResult res[16];
    float d;
    int n;
    _hdResult *r;

    vecCopy(&a, &cs->trans);
    b = a;
    d = c->lastPos.z - cs->trans.z + fabsf(c->vel.z);
    b.z -= d;
    a.z += d;
    n = hdSphereTest(&a, &b, 0.0f, res, 0, 16);
    for (r = hdGetNextCollis(res, n, 0); r != 0; r = hdGetNextCollis(res, n, r)) {
        HdResultIds *h = (HdResultIds *)r;

        if (h->id1 == 10052)
            continue;
        if (h->id2 != 0 && Interactives::getInteractive(h->id2)->m_cs != 0)
            continue;
        {
            float z = c->cs->trans.z - r->point.z;

            if (z != 0.0f)
                z -= 5.0f;
            vecCopy(&c->lastPos, &c->cs->trans);
            return z;
        }
    }
    return 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Debris", ChunkHat__6DebrisPQ26Debris9ChunkData);
#endif

INCLUDE_ASM("asm/nonmatchings/game/Debris", __static_initialization_and_destruction_0_001301B0);

unsigned Debris::update(void *p)
{
    return ((Debris *)p)->update();
}

INCLUDE_ASM("asm/nonmatchings/game/Debris", _GLOBAL_$I$InitBefore__6Debris);
