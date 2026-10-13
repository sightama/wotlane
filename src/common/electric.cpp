#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

/* Electric arcs: particle effects of visual type 7 (a lightning chain, links in pos[], drawn by electricArcPacketBuild)
   and 17 (a moving arc, updated by arcMove in common/particle). The pool is the particle system's (fx/fxList). */
struct _ilnklist {
    int used;
    int next;
    int prev;
};

extern ParticleType fx[100];
extern _ilnklist fxList[100];
extern int fxActiveTop;
extern int fxInactiveTop;
extern unsigned localSeed;
extern unsigned defaultCLUT[];
extern unsigned crowdControlCLUT[] __asm__("D_00257290");
extern _fvector arcBBoxMin __asm__("D_00735710");
extern _fvector arcBBoxMax __asm__("D_00735720");

extern "C" float cosf(float);
extern "C" float sinf(float);
void electricArcPacketInit(ParticleType *p);
void electricArcPacketBuild(ParticleType *p);
void mathfMatrixFromNormal(float (*m)[4], _fvector *n);
void mathfRandVector(_fvector *v, float scale);
void mathfDecomposeVector(_fvector *v, _fvector *axis, _fvector *perp, _fvector *par);
ParticleType *particleGetParticle(int h);
int electricArcChainInit(float size, float jag, float segInitU, float segDeltaU, float clutOffset, unsigned *clut, unsigned seed, int numColors, int duration);
void electricArcChainAddLink(ParticleType *p, _fvector *pos, bool first, int level, _fvector *bbMin, _fvector *bbMax);

#ifdef NON_MATCHING
/* Takes the top of the inactive list and pushes it on the active list (as in common/particle). */
static inline int fxAlloc(void)
{
    int h = fxInactiveTop;
    _ilnklist *e;

    if (h == -1)
        return -1;
    e = &fxList[h];
    fxInactiveTop = e->prev;
    if (fxInactiveTop != -1)
        fxList[fxInactiveTop].next = -1;
    if (fxActiveTop != -1)
        fxList[fxActiveTop].next = h;
    e->prev = fxActiveTop;
    e->next = -1;
    e->used = 1;
    fxActiveTop = h;
    return h;
}
#define WI(v) (((int *)(v))[3])
static inline void vecMadd(_fvector *dst, _fvector *a, _fvector *b, float s)
{
    dst->x = a->x + b->x * s;
    dst->y = a->y + b->y * s;
    dst->z = a->z + b->z * s;
}
/* The 7 points of a half ring around c in the plane of a (pole) and c1, bent by b: c + a r, four in between, c + c1 r,
   c - a r. k = the ring's (cos, sin, bend) times its radius r = k.w. */
static inline void ringPoints(FVECTOR *pt, _fvector *c, _fvector *k, _fvector *a, _fvector *c1, _fvector *b)
{
    float r = k->w;
    float kx = k->x * r, ky = k->y * r, kz = k->z * r;
    int i;

    for (i = 0; i < 3; i++) {
        float cc = ((float *)c)[i], av = ((float *)a)[i], cv = ((float *)c1)[i], bv = ((float *)b)[i];

        ((float *)&pt[0])[i] = cc + av * r;
        ((float *)&pt[1])[i] = cc + (cv * kx + av * ky + bv * kz);
        ((float *)&pt[2])[i] = cc + (cv * ky + av * kx + bv * kz);
        ((float *)&pt[3])[i] = cc + cv * r;
        ((float *)&pt[4])[i] = cc + (cv * kx - av * ky - bv * kz);
        ((float *)&pt[5])[i] = cc + (cv * ky - av * kx - bv * kz);
        ((float *)&pt[6])[i] = cc - av * r;
    }
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 16/333 words, size 0x534 vs 0x4b4 */
/* A one-shot bolt from `from` to `to`: segs links jittered in a cone of `spread` around the line, colors c0/c1 (the
   two-entry CLUT lives on the stack, the packet is built here). Returns the effect handle or -1. */
int electricArc(_fvector *from, _fvector *to, unsigned c0, unsigned c1, float jitter, float size, float spread, float scale,
                int segs, unsigned seed, unsigned dur)
{
    unsigned clut[2];
    int h = fxAlloc();
    ParticleType *p;
    FVECTOR d, rv, dir;
    FMATRIX m;
    float len, k;
    int levels, n, pairs;
    _fvector *P;

    if (h == -1)
        return -1;
    p = &fx[h];
    p->vis.attribs = 7;
    p->vis.texInfoIndex = 10;
    p->info.attribs = 0;
    p->info.minSize = size;
    p->info.segmentDeltaU = scale + scale;
    p->info.segmentInitU = (seed & 0xFF) * 0.00390625f;
    p->emit.info.duration = dur;
    p->emit.info.normalScale = scale;
    p->emit.info.randomSeed = seed;
    p->emit.info.attribs = 3;
    p->emit.fxId = 0x600DC0DE;
    p->vis.alphaContext = 0x48;
    p->vis.colorIndex = (unsigned)clut;
    p->vis.numColors = 2;
    clut[0] = c0;
    clut[1] = c1;
    p->info.expansionRate = 0.0f;
    p->emit.info.CLUToffset = 0.0f;
    p->field.attribs = 0;
    p->vis.lightColor = 0;
    p->prevViewMat = 0;
    p->master = 0;
    p->slave = 0;
    p->backPointer = 0;
    vecSub(&d, to, from);
    len = eeSqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    k = 1.0f / len;
    d.z *= k;
    d.x *= k;
    d.y *= k;
    mathfMatrixFromNormal(m, &d);
    if (spread < 0.067f) {
        p->FOVsphere.x = (from->x + to->x) * 0.5f;
        p->FOVsphere.y = (from->y + to->y) * 0.5f;
        p->FOVsphere.z = (from->z + to->z) * 0.5f;
        p->FOVextent.x = (to->x - from->x < 0.0f ? from->x - to->x : to->x - from->x) * 0.5f;
        p->FOVextent.y = (to->y - from->y < 0.0f ? from->y - to->y : to->y - from->y) * 0.5f;
        p->FOVextent.z = (to->z - from->z < 0.0f ? from->z - to->z : to->z - from->z) * 0.5f;
    } else {
        p->FOVsphere.x = from->x;
        p->FOVsphere.y = from->y;
        p->FOVsphere.z = from->z;
        p->FOVextent.x = len;
        p->FOVextent.y = len;
        p->FOVextent.z = len;
    }
    /* subdivision depth of the bolt: log2(len / 4 / size), at most 6 */
    n = (int)(len * 0.25f / size);
    if (n < 0)
        n = 0;
    levels = -1;
    while (n) {
        n = (unsigned)n >> 1;
        levels++;
    }
    if (levels >= 7)
        levels = 6;
    electricArcPacketInit(p);
    p->numParticles = segs * 2 + 1;
    if (p->maxParticles < p->numParticles)
        p->numParticles = p->maxParticles;
    pairs = p->numParticles >> 1;
    P = p->pos;
    for (; pairs > 0; pairs--) {
        mathfRandVector(&rv, spread);
        vecApplyRows(&dir, m, &rv);
        if (0.0f < jitter)
            vecMadd(&P[0], from, &dir, jitter);
        else
            P[0] = *from;
        WI(&P[0]) = 2;
        vecMadd(&P[1], from, &dir, len);
        WI(&P[1]) = levels ? (int)(5u << ((levels - 1) & 31)) : 0;
        P += 2;
    }
    WI(P) = 0;
    electricArcPacketBuild(p);
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/electric", electricArc__FP8_fvectorT0UiUiffffiUiUi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 20/217 words, size 0x2f8 vs 0x364 */
/* An arc over a sphere of radius r around c, on a random great half circle; out gets its middle point. */
int electricArcOnSphere(_fvector *c, float r, float size, float jag, unsigned *clut, unsigned seed, int numColors, int dur,
                        int level, _fvector *out)
{
    FVECTOR k, a, b, c1, pt[7];
    ParticleType *p;
    float s, inv;
    int h, i;

    h = electricArcChainInit(size, jag, 0.0f, jag * r / (size * 4.0f * (float)(1 << level)), 0.0f, clut, seed, numColors, dur);
    if (h == -1)
        return -1;
    s = jag * jag + 1.0f;
    inv = 1.0f / eeSqrtf(s);
    k.z = jag / eeSqrtf(s);
    k.w = r;
    k.x = inv * 0.5f;
    k.y = inv * 0.8660254f;
    mathfRandVector(&a, 1.0f);
    mathfRandVector(&b, 1.0f);
    vecCross(&c1, &a, &b);
    inv = 1.0f / eeSqrtf(c1.x * c1.x + c1.y * c1.y + c1.z * c1.z);
    c1.z *= inv;
    c1.x *= inv;
    c1.y *= inv;
    vecCross(&b, &a, &c1);
    ringPoints(pt, c, &k, &a, &c1, &b);
    p = particleGetParticle(h);
    electricArcChainAddLink(p, &pt[0], true, 1, 0, 0);
    for (i = 1; i < 7; i++)
        electricArcChainAddLink(p, &pt[i], false, level, 0, 0);
    electricArcPacketBuild(p);
    if (out)
        *out = pt[3];
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/electric", electricArcOnSphere__FP8_fvectorfffPUiUiiiiT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/293 words, size 0x468 vs 0x494 */
/* The crowd-control power: 10 half rings around c (a cage) that then fly apart (vis 17, moved by arcMove). */
ParticleType *energyCrowdControl(_fvector *c)
{
    ParticleType *p;
    FVECTOR k, a, b, c1, pt[7], ctr, d, rv, perp, par;
    int i, j, n;
    _fvector *P, *V;
    float speed;

    localSeed = localSeed * 0x41C64E6D + 0x3039;
    p = particleGetParticle(electricArcChainInit(1.0f, 0.3f, -0.125f, 0.0625f, 0.0f, crowdControlCLUT, localSeed, 16, 32));
    if (!p)
        return 0;
    k.x = 0.5f;
    k.y = 0.8660254f;
    k.z = 0.0f;
    k.w = 10.0f;
    for (i = 10; i; i--) {
        float ang = (float)i * 6.2831855f / 10.0f;
        float sn, cs;

        a.x = 0.0f;
        a.z = 1.0f;
        a.y = 0.0f;
        sn = sinf(ang);
        b.x = sn;
        cs = cosf(ang);
        c1.x = cs;
        b.y = cs;
        b.z = 0.0f;
        c1.z = 0.0f;
        c1.y = -sn;
        ringPoints(pt, c, &k, &a, &c1, &b);
        electricArcChainAddLink(p, &pt[0], true, 1, 0, 0);
        for (j = 1; j < 7; j++)
            electricArcChainAddLink(p, &pt[j], false, 3, 0, 0);
    }
    p->vis.attribs = 17;
    p->emit.StaticVec = *c;
    p->info.maxSize = 1.0f;
    p->emit.cumulativeRate = 0.0f;
    p->field.lightMidInterval = 0.0f;
    p->emit.info.maxOffset = 0.3f;
    p->field.orbit1 = -0.125f;
    p->emit.StaticVec.w = 10.0f;
    p->field.orbit2 = 0.0625f;
    p->emit.info.minVel = 6.25f;
    p->emit.info.maxVel = 0.625f;
    p->info.initExpansionRate = 0.025f;
    p->field.lightInitIntensity = -0.25f;
    p->field.lightMidIntensity = 0.03125f;
    p->emit.info.rate = 0.125f;
    electricArcPacketBuild(p);
    if (p->emit.info.minVel == 0.0f)
        return p;
    ctr = p->emit.StaticVec;
    n = p->numParticles;
    speed = p->emit.info.minVel;
    P = p->pos;
    V = p->vel;
    for (; n; n--, P++, V++) {
        float inv;

        vecSub(&d, P, &ctr);
        inv = 1.0f / eeSqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        d.z *= inv;
        d.x *= inv;
        d.y *= inv;
        mathfRandVector(&rv, 1.0f);
        mathfDecomposeVector(&rv, &d, &perp, &par);
        vecAdd(&d, &d, &perp);
        vecScale(V, &d, speed);
    }
    return p;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/electric", energyCrowdControl__FP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 8/107 words, size 0x1ac vs 0x1a4 */
/* Starts an empty lightning chain (links are added with electricArcChainAddLink). seed 0 draws one. */
int electricArcChainInit(float size, float jag, float segInitU, float segDeltaU, float clutOffset, unsigned *clut, unsigned seed, int numColors, int duration)
{
    int h = fxAlloc();
    ParticleType *p;

    if (h == -1)
        return -1;
    p = &fx[h];
    p->vis.attribs = 7;
    p->vis.texInfoIndex = 10;
    p->info.minSize = size;
    p->info.segmentInitU = segInitU;
    p->info.segmentDeltaU = segDeltaU;
    p->emit.info.duration = 0 < duration ? duration : 1;
    p->emit.info.attribs = 3;
    p->emit.info.normalScale = jag;
    p->info.attribs = 0;
    p->info.expansionRate = 0.0f;
    p->field.attribs = 0;
    if (!seed) {
        localSeed = localSeed * 0x41C64E6D + 0x3039;
        seed = localSeed;
    }
    p->emit.info.randomSeed = seed;
    p->emit.info.CLUToffset = clutOffset;
    p->emit.fxId = 0xBAADC0DE;
    p->vis.alphaContext = 0x48;
    p->vis.colorIndex = (unsigned)(clut ? clut : defaultCLUT);
    p->vis.numColors = numColors;
    p->vis.lightColor = 0;
    p->prevViewMat = 0;
    p->master = 0;
    p->slave = 0;
    p->backPointer = 0;
    electricArcPacketInit(p);
    p->numParticles = 1;
    WI(&p->pos[0]) = 0;
    p->emit.parentVec = 0;
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/electric", electricArcChainInit__FfffffPUiUiii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/106 words, size 0x1a8 vs 0x134 */
/* Appends a link (at most 128). The first link of a chain counts the links that follow in its w; the others carry their
   subdivision level (5 << (level - 1)). The bounding box (FOVsphere/extent) grows in bbMin/bbMax or in a static one. */
void electricArcChainAddLink(ParticleType *p, _fvector *pos, bool first, int level, _fvector *bbMin, _fvector *bbMax)
{
    _fvector *mn, *mx, *e;
    int k;

    if (p->numParticles > 128)
        return;
    mn = bbMin ? bbMin : &arcBBoxMin;
    mx = bbMin ? bbMax : &arcBBoxMax;
    if (p->numParticles == 1) {
        *mn = *pos;
        *mx = *pos;
    } else {
        for (k = 0; k < 4; k++) {
            float v = ((float *)pos)[k];

            if (v < ((float *)mn)[k])
                ((float *)mn)[k] = v;
            if (((float *)mx)[k] < v)
                ((float *)mx)[k] = v;
        }
    }
    p->FOVsphere.x = mn->x * 0.5f + mx->x * 0.5f;
    p->FOVsphere.y = mn->y * 0.5f + mx->y * 0.5f;
    p->FOVsphere.z = mn->z * 0.5f + mx->z * 0.5f;
    p->FOVsphere.w = pos->w;
    p->FOVextent.x = mx->x * 0.5f - mn->x * 0.5f;
    p->FOVextent.y = mx->y * 0.5f - mn->y * 0.5f;
    p->FOVextent.z = mx->z * 0.5f - mn->z * 0.5f;
    e = &p->pos[p->numParticles - 1];
    *e = *pos;
    if (first) {
        p->emit.parentVec = (FVECptr)e;
        WI(e) = 1;
    } else {
        WI(p->emit.parentVec)++;
        WI(e) = level ? 5 << (level - 1) : 0;
    }
    WI(e + 1) = 0;
    p->numParticles++;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/electric", electricArcChainAddLink__FP13_ParticleTypeP8_fvectorbiT1T1);
#endif
