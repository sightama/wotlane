#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

extern _ParticleType fx[];
extern int splats;
void particleDestroyEffect(_ParticleType *p);
void pedSplat(_ParticleType *p, _fvector *pos);

__asm__("#SNFIX_SMALL splats");
struct _ilnklist {
    int used;
    int next;
    int prev;
};
extern _ilnklist fxList[100];
extern int fxActiveTop;
extern int fxInactiveTop;
extern "C" float sqrtf(float);
void stampPacketInit(_ParticleType *p);
int particleCreateFx(_fvector *pos, int type, float scale, float (*m)[4], float zFudge);
void mathfVectorMinMaxGetMac(_fvector *min, _fvector *max);

void pedsTurnOff(_fvector *pos, bool b)
{
    if (b && splats != -1)
        pedSplat(&fx[splats], pos);
    *(int *)&pos->w = -1;
}
INCLUDE_ASM("asm/nonmatchings/common/peds", stampPacketInit__FP13_ParticleType);
#ifdef NON_MATCHING
/* equivalent (untuned): 11/129 words, size 0x200 vs 0x204 */
/* Takes the top of the inactive particle list and pushes it on the active list (as in common/particle). */
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
/* A crowd of n pedestrians drawn as stamped sprites (vis 14); *out gets their positions (w < 0: gone). The shared blood
   splat effect (0x91) is created with the first crowd. */
_ParticleType *pedsCreate(int n, float sx, float sy, unsigned clut, _fvector **out, int numColors, int frames)
{
    int h = fxAlloc();
    _ParticleType *p;

    if (h == -1)
        return 0;
    p = &fx[h];
    p->vis.attribs = 14;
    p->newParticles = n;
    p->vis.texInfoIndex = 30;
    p->field.attribs = 0x1000;
    p->numParticles = n;
    p->emit.info.rate = 1.0f;
    sqrtf(sx * sx + sy * sy); /* computed and dropped in retail too */
    p->info.minSize = sx;
    p->vis.texMask = frames - 1;
    p->info.maxSize = sy;
    p->vis.colorIndex = clut;
    p->vis.numColors = numColors;
    p->info.lifetime = 0;
    p->info.attribs = 0;
    p->vis.alphaContext = 0;
    p->vis.lightColor = 0;
    p->prevViewMat = 0;
    p->master = 0;
    p->slave = 0;
    stampPacketInit(p);
    *out = p->pos;
    if (splats == -1) {
        splats = particleCreateFx(p->pos, 0x91, 1.0f, 0, 0.0f);
        if (splats != -1)
            fx[splats].backPointer = &splats;
    }
    return p;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/peds", pedsCreate__FiffUiPP8_fvectorii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/69 words, size 0x114 vs 0xfc */
/* Bounding box of the live pedestrians, grown by the sprite size, as the effect's FOV box. */
void pedsUpdateFOVbox(_ParticleType *p)
{
    FVECTOR mn, mx;
    _fvector *q;
    int n;

    if (!p)
        return;
    q = p->pos;
    for (n = p->numParticles; n; n--, q++)
        if (((int *)q)[3] >= 0)
            break;
    if (n) {
        vu0BBoxInit(q);
        for (; n; n--, q++)
            if (((int *)q)[3] >= 0)
                vu0BBoxAdd(q);
    }
    mathfVectorMinMaxGetMac(&mn, &mx);
    p->FOVsphere.x = mn.x * 0.5f + mx.x * 0.5f;
    p->FOVsphere.y = mn.y * 0.5f + mx.y * 0.5f;
    p->FOVsphere.z = mn.z * 0.5f + mx.z * 0.5f;
    p->FOVextent.x = p->info.minSize + mx.x * 0.5f - mn.x * 0.5f;
    p->FOVextent.y = p->info.minSize + mx.y * 0.5f - mn.y * 0.5f;
    p->FOVextent.z = p->info.maxSize + mx.z * 0.5f - mn.z * 0.5f;
    p->FOVextent.w = 0.5f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/peds", pedsUpdateFOVbox__FP13_ParticleType);
#endif
void pedsKill(_ParticleType **pp)
{
    particleDestroyEffect(*pp);
    *pp = 0;
    particleDestroyEffect(&fx[splats]);
}
