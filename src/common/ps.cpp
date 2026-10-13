#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

/* Projected shadows ("ps"): up to 10 shadow blockers (casters), each rendered from the light into a texture by view 4.
   The packet builders and the VU0/VU1 parts are hardware code (config/hw_funcs.txt); this keeps the blocker bookkeeping. */
struct _pshadowblockerlist {
    FVECTOR litPos;         // 0x0 light-side position this frame (psFindActiveBlockers)
    _cs *cs;                // 0x10 the caster
    _cs *shadowCs;          // 0x14
    _cs *secondCs;          // 0x18
    _fvector *wsOrigin;     // 0x1C &cs->trans unless set
    float *radius;          // 0x20 the bounding radius of the caster's model
    float litDist;          // 0x24 how far toward the light (in radii) the shadow camera sits
    float lodRangeSq;       // 0x28
    short pad2C;            // 0x2C
    unsigned short active;  // 0x2E
};
typedef char _size__pshadowblockerlist[sizeof(_pshadowblockerlist) == 0x30 ? 1 : -1];

enum _viewports { VIEWPORT_SHADOW = 6 };
struct sceGsDrawEnv1;
struct sceGsClear;

extern _pshadowblockerlist psBlkLst[10];
extern _cs *psViewCs;
extern int psBufNum;
extern FVECTOR psLitDirWs;
extern float psZoomT;
extern float psShadowIntens;
extern float psShadowAlpha __asm__("D_006F8D70");
extern void *psFilterPkt __asm__("D_006F8D74");
extern void *psBorderPkt __asm__("D_006F8D78");
extern int psShadowOn __asm__("D_006F8D7C");
extern unsigned long psDrawEnv[];
extern unsigned long psClearDraw[];

extern "C" int printf(const char *fmt, ...);
extern "C" int sceGsSetDefDrawEnv(sceGsDrawEnv1 *dp, short psm, short w, short h, short ztest, short zpsm);
extern "C" void sceGsSetDefClear(sceGsClear *cp, short ztest, short x, short y, short w, short h, unsigned char r, unsigned char g,
                                 unsigned char b, unsigned char a, unsigned z);
void viewCreate(_viewports vp, int view);
_cs *viewGetViewCs(int view);
void viewSetFov(int view, float fov, float aspect);
void viewShadowUpdate(int view);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
void *psInitFilterTexPkt(void);
void *psInitBorderPkt(void);
void psVarInit(void);
void psSetLightDir(_fvector *rph);

/* bit i set: blocker i casts a shadow this frame (written to the scratchpad by psFindActiveBlockers) */
#define PS_ACTIVE_MASK (*(unsigned *)0x70000208)

__asm__("#SNFIX_SMALL psZoomT");
__asm__("#SNFIX_SMALL psShadowIntens");
__asm__("#SNFIX_SMALL psBufNum");
__asm__("#SNFIX_SMALL D_006F8D74");
__asm__("#SNFIX_SMALL D_006F8D78");
__asm__("#SNFIX_SMALL D_006F8D7C");
typedef unsigned int u128 __attribute__((mode(TI)));

INCLUDE_ASM("asm/nonmatchings/common/ps", psObjectCull);
INCLUDE_ASM("asm/nonmatchings/common/ps", psGetMatrix__FP6QwData);
INCLUDE_ASM("asm/nonmatchings/common/ps", psFovTestResAsm__Fv);
INCLUDE_ASM("asm/nonmatchings/common/ps", psBldBlkRObjs);
#ifdef NON_MATCHING
/* equivalent (untuned): 17/56 words, size 0xdc vs 0xe0 */
_pshadowblockerlist *psBlockerAdd(_cs *cs, float litDist)
{
    float lod = 1200.0f;
    int i;
    _pshadowblockerlist *b;

    for (i = 0; i < 10; i++) {
        b = &psBlkLst[i];
        if (b->active)
            continue;
        b->cs = cs;
        b->shadowCs = 0;
        b->secondCs = 0;
        b->lodRangeSq = lod * lod;
        b->wsOrigin = &cs->trans;
        b->active = 1;
        b->litDist = 0.0f < litDist ? litDist : 9.0f;
        if (cs->epNode && cs->epNode->opcode == 1) {
            b->radius = &((_hiergroup *)cs->epNode)->sphereRadius;
        } else {
            printf("Radius for shadow block not available \n");
            b->radius = 0;
        }
        return b;
    }
    printf("Can't allocate shadow blocker, too many! \n");
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ps", psBlockerAdd__FP3_csf);
#endif
void psBlockerSetShadowCs(_pshadowblockerlist *b, _cs *cs)
{
    b->shadowCs = cs;
}
void psBlockerSet2ndCs(_pshadowblockerlist *b, _cs *cs)
{
    b->secondCs = cs;
}
float psBlockerSetLodRange(_pshadowblockerlist *b, float range)
{
    return b->lodRangeSq = range * range;
}
void psBlockerSetWsOrigin(_pshadowblockerlist *b, _fvector *origin)
{
    b->wsOrigin = origin;
}
void psBlockerSetRadius(_pshadowblockerlist *b, float *radius)
{
    b->radius = radius;
}
/* The shadow view (4, a 128x128 render into the shadow texture), the blocker table, the filter/border packets and the light. */
void psBlockerInit(void)
{
    int i;
    FVECTOR rph;
    _pshadowblockerlist *b;

    *(u128 *)&rph = 0;
    rph.x = -1.5707964f;
    viewCreate(VIEWPORT_SHADOW, 4);
    psViewCs = viewGetViewCs(4);
    psBufNum = 0;
    sceGsSetDefDrawEnv((sceGsDrawEnv1 *)psDrawEnv, 0, 0x80, 0x80, 0, 0x31);
    psDrawEnv[0] = (psDrawEnv[0] & ~0x1FFUL) | 0x1F3;
    sceGsSetDefClear((sceGsClear *)psClearDraw, 0, 0x7C0, 0x7C0, 0x80, 0x80, 0, 0, 0, 0, 0);
    for (i = 0; i < 10; i++) {
        b = &psBlkLst[i];
        b->cs = 0;
        b->secondCs = 0;
        b->radius = 0;
        b->litDist = 9.0f;
        b->active = 0;
        b->pad2C = 30;
        b->wsOrigin = 0;
    }
    psFilterPkt = psInitFilterTexPkt();
    psBorderPkt = psInitBorderPkt();
    psVarInit();
    psSetLightDir(&rph);
}
int psBlockerRemove(_cs *cs)
{
    int i;

    for (i = 0; i < 10; i++) {
        if (psBlkLst[i].cs == cs) {
            psBlkLst[i].cs = 0;
            psBlkLst[i].active = 0;
            return 1;
        }
    }
    return 0;
}
INCLUDE_ASM("asm/nonmatchings/common/ps", psFindActiveBlockers__FP8_fvectori);
INCLUDE_ASM("asm/nonmatchings/common/ps", psInitBlockerDMA__Fv);
INCLUDE_ASM("asm/nonmatchings/common/ps", psBldBlockerPkt__FP8_fvectorii);
INCLUDE_ASM("asm/nonmatchings/common/ps", psEndBlkObjs__Fi);
INCLUDE_ASM("asm/nonmatchings/common/ps", psEndBlkDma__Fv);
INCLUDE_ASM("asm/nonmatchings/common/ps", psGetBlkPktStart__Fv);
INCLUDE_ASM("asm/nonmatchings/common/ps", psAttachDmaToAlpha__FP9Vu1DmaPkt);
INCLUDE_ASM("asm/nonmatchings/common/ps", psAttachDmaToMain__FP9Vu1DmaPkt);
INCLUDE_ASM("asm/nonmatchings/common/ps", psSetVU1ShadowMat__FPA3_fP6QwData);
/* rph orients the shadow camera (roll forced to 0); the world light direction is its -y axis. */
void psSetLightDir(_fvector *rph)
{
    FVECTOR down;

    *(u128 *)&down = 0;
    down.y = -1.0f;
    down.w = 1.0f;
    if (psViewCs) {
        rph->y = 0.0f;
        mathfRotMatrixRPH(psViewCs->mat, rph);
        vecApplyRows(&psLitDirWs, psViewCs->mat, &down);
        viewSetFov(4, psZoomT, 0.98f);
        viewShadowUpdate(4);
    }
}
_fvector *psGetLightDir(void)
{
    return &psLitDirWs;
}
_cs *psIsBlockerCsActive(int i)
{
    if ((1 << i) & PS_ACTIVE_MASK)
        return psBlkLst[i].cs;
    return 0;
}
_cs *psBlockerHas2ndCS(int i)
{
    return psBlkLst[i].secondCs;
}
float psGetBlockerLodRangeSq(int i)
{
    return psBlkLst[i].lodRangeSq;
}
void psSetShadowOnOff(int on)
{
    psShadowOn = on;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 12/35 words, size 0x84 vs 0x8c */
/* The shadow camera of blocker i: litDist radii from its origin, against the light direction. */
void psSetBlkLitPos(_fvector *out, int i)
{
    _pshadowblockerlist *b = &psBlkLst[i];
    float d = *b->radius * b->litDist;

    out->x = -(psLitDirWs.x * d + b->wsOrigin->x);
    out->y = -(psLitDirWs.y * d + b->wsOrigin->y);
    out->z = -(psLitDirWs.z * d + b->wsOrigin->z);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ps", psSetBlkLitPos__FP8_fvectori);
#endif
void psSetCurrentBuffer(int buf)
{
    psBufNum = buf;
}
int psGetCurrentBuffer(void)
{
    return psBufNum;
}
INCLUDE_ASM("asm/nonmatchings/common/ps", psInitFilterTexPkt__Fv);
INCLUDE_ASM("asm/nonmatchings/common/ps", psDrawFilterSprite__FP6QwDataiiiiiiiii);
INCLUDE_ASM("asm/nonmatchings/common/ps", psInitBorderPkt__Fv);
INCLUDE_ASM("asm/nonmatchings/common/ps", psDrawBorderSprite__FP6QwDataiiii);
void psVarInit(void)
{
    psZoomT = 620.0f;
    psShadowIntens = 80.0f;
    psShadowAlpha = 0.4f;
    psShadowOn = 1;
}
void psStoreQword(QwData *dst, QwData *src)
{
    __asm__ volatile("lq $3, 0x0(%1)\n\t"
                     "sq $3, 0x0(%0)" : : "r"(dst), "r"(src) : "$3", "memory");
}
INCLUDE_ASM("asm/nonmatchings/common/ps", psFovTestAsm__FP8_fvectorP11_hierobject);
