#include "common.h"
#include "hieri_types.h"
#include "cs_pool.h"
#include "task_manager.h"
#include "vecmath.h"

/* Debug drawing. In the retail build only drawSphere still draws (a scaled sphere model for one field); the other calls
   compute their geometry into locals and drop it (the line drawing was compiled out). */
class DebugDraw {
public:
    static _hierhead *s_ep[33];

    static void init(void);
    static void registerVisual(_hierhead *h);
    static _cs *drawSphere(void);
    static _cs *drawSphere(_fvector &pos, float r);
    static void drawCS(_fvector &o, float (&m)[4][4], float len);
    static void drawCollisionLines(_fvector &p, unsigned c0, unsigned c1);
    static void drawCollisionLines(_fvector &p, float len, int n, unsigned c0, unsigned c1);
    static void drawNormal(_fvector &o, _fvector &d, float len, unsigned color);
    static void drawBox(_fvector &a, _fvector &b, unsigned color);
    static void drawBox(float (&m)[4][4], _fvector &pos, _fvector &a, _fvector &b, unsigned color);
    static void drawCone(_fvector &apex, _fvector &dir, float angle, float len, unsigned color);
    static unsigned destroyCS(void *cs);
};

extern "C" float cosf(float);
extern "C" float sinf(float);
extern "C" float tanf(float);
void hierSetCsEpNode(_cs *cs, _hierhead *ep);
void mathfRandVector(_fvector *v, float scale);
void mathfVectorCrossUp(_fvector *dst, _fvector *v);
extern float UNIT_MATRIX[4][4];

void DebugDraw::init(void)
{
    int i;

    for (i = 31; i >= 0; i--)
        s_ep[i] = 0;
}
void DebugDraw::registerVisual(_hierhead *h)
{
    unsigned idx = h->id1 - 0x3E80;

    if (idx < 0x21)
        s_ep[idx] = h;
}
/* A cs showing the debug sphere model for one field (a task deactivates it). */
_cs *DebugDraw::drawSphere(void)
{
    _cs *cs = CsPool::csActivate();

    if (cs) {
        cs->drawMe = 1;
        hierSetCsEpNode(cs, s_ep[0]);
        TaskManager::global.add(destroyCS, cs, 1);
    }
    return cs;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 8/23 words, size 0x58 vs 0x5c */
_cs *DebugDraw::drawSphere(_fvector &pos, float r)
{
    _cs *cs = drawSphere();

    if (cs) {
        cs->scaleMe = 1;
        cs->scale.x = r;
        cs->scale.z = r;
        cs->scale.y = r;
        cs->trans = pos;
    }
    return cs;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/DebugDraw", drawSphere__9DebugDrawR8_fvectorf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/32 words, size 0x40 vs 0x80 */
void DebugDraw::drawCS(_fvector &o, float (&m)[4][4], float len)
{
    FVECTOR t;
    int i;

    for (i = 0; i < 3; i++) {
        vecScale(&t, (_fvector *)m[i], len);
        vecAdd(&t, &t, &o);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/DebugDraw", drawCS__9DebugDrawR8_fvectorRA3_A3_ff);
#endif
void DebugDraw::drawCollisionLines(_fvector &p, unsigned c0, unsigned c1)
{
    FVECTOR a, b, r;
    int i;

    drawCollisionLines(p, 5.0f, 1, c0, c1);
    for (i = 0; i < 10; i++) {
        mathfRandVector(&r, 1.0f);
        vecScale(&r, &r, 5.0f);
        vecAdd(&a, &p, &r);
        vecSub(&b, &p, &r);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 31/39 words, size 0x9c vs 0x9c */
void DebugDraw::drawCollisionLines(_fvector &p, float len, int n, unsigned c0, unsigned c1)
{
    FVECTOR a, b, r;

    for (; n > 0; n--) {
        mathfRandVector(&r, 1.0f);
        vecScale(&r, &r, len);
        vecAdd(&a, &p, &r);
        vecSub(&b, &p, &r);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/DebugDraw", drawCollisionLines__9DebugDrawR8_fvectorfiUiUi);
#endif
void DebugDraw::drawNormal(_fvector &o, _fvector &d, float len, unsigned color)
{
    FVECTOR a, b;

    a = o;
    vecScale(&b, &d, len);
    vecAdd(&b, &b, &a);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/31 words, size 0x7c vs 0x64 */
/* The 8 corners of the box a-b. */
static inline void boxCorners(FVECTOR *c, _fvector &a, _fvector &b)
{
    c[4] = a;
    c[7] = b;
    c[1] = a;
    c[2] = b;
    c[3] = a;
    c[5] = b;
    c[1].x = b.x;
    c[2].z = a.z;
    c[3].y = b.y;
    c[4].z = b.z;
    c[5].y = a.y;
    c[7].x = a.x;
    c[0] = a;
    c[6] = b;
}
void DebugDraw::drawBox(_fvector &a, _fvector &b, unsigned color)
{
    FVECTOR c[8];

    boxCorners(c, a, b);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/DebugDraw", drawBox__9DebugDrawR8_fvectorT1Ui);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/49 words, size 0xc4 vs 0xac */
void DebugDraw::drawBox(float (&m)[4][4], _fvector &pos, _fvector &a, _fvector &b, unsigned color)
{
    FVECTOR c[8];
    int i;

    boxCorners(c, a, b);
    for (i = 0; i < 8; i++) {
        vecApplyRows(&c[i], m, &c[i]);
        vecAdd(&c[i], &c[i], &pos);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/DebugDraw", drawBox__9DebugDrawRA3_A3_fR8_fvectorN22Ui);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 6/120 words, size 0x1e0 vs 0x1d8 */
/* A cone of half-angle angle/2 and length len along dir, as 16 points of its rim. */
void DebugDraw::drawCone(_fvector &apex, _fvector &dir, float angle, float len, unsigned color)
{
    FVECTOR axes[3];
    FVECTOR v, p;
    float r = tanf(angle * 0.5f) * len;
    float a, k;
    int i;

    axes[1] = dir;
    mathfVectorCrossUp(&axes[0], &axes[1]);
    if (axes[0].x * axes[0].x + axes[0].y * axes[0].y + axes[0].z * axes[0].z < 1e-10f)
        vecCross(&axes[0], &axes[1], (_fvector *)UNIT_MATRIX[1]);
    k = 1.0f / eeSqrtf(axes[0].x * axes[0].x + axes[0].y * axes[0].y + axes[0].z * axes[0].z);
    axes[0].z *= k;
    axes[0].x *= k;
    axes[0].y *= k;
    vecCross(&axes[2], &axes[0], &axes[1]);
    vecScale(&axes[0], &axes[0], r);
    vecScale(&axes[1], &axes[1], len);
    vecScale(&axes[2], &axes[2], r);
    v.y = 1.0f;
    a = 0.0f;
    for (i = 0; i < 16; i++) {
        v.x = cosf(a);
        v.z = sinf(a);
        vecApplyRows(&p, (float (*)[4])axes, &v);
        vecAdd(&p, &p, &apex);
        a += 0.3926991f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/DebugDraw", drawCone__9DebugDrawR8_fvectorT1ffUi);
#endif
unsigned DebugDraw::destroyCS(void *cs)
{
    CsPool::csDeactivate((_cs *)cs);
    return 0;
}
