#include "common.h"
#include "hieri_types.h"
#include "cs_pool.h"

struct Mat4 {
    float m[4][4];
} __attribute__ ((aligned (16)));

extern _hierSkelBone **skelStack;
extern float (*skelMatStack)[4][4];
extern int g_cameraLosPointsPerView[5];
extern _fvector g_cameraLosPoints[5][2];
extern "C" float sqrtf(float);
extern unsigned *lastGsCtx;
typedef void (*TravCb)(_cs *, unsigned, unsigned, float (&)[4][4], _fvector *);
extern TravCb gTraversalCallback;
extern unsigned FxToKill[32];
void particleKillFx(int &);
void mathfMulMatrix3x3(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfNormalizeColumns(float (*m)[4]);
void mathfScaleMatrix3x3(float (*dst)[4], float (*m)[4], float s);
void vu0LoadEoAsm(QwData *);
struct LensFlareEnt {
    float pos[3];
    unsigned id;
    union {
        int w10;
        unsigned char b10;
    };
    float f14;
    float f18;
    float f1c;
    float r;
    float g;
    float b;
    float intensity;
};
extern int lensFlareListIndex;
extern LensFlareEnt *lensFlareListPtr;
extern float hierClipRange;
void ctxSetUpGSCtx(void);
int particleCreateModeledFx(_fvector *, HierParticleEmitter *);
void hierRegisterLocator(unsigned id, int fx);
void hierPush(_hierhead **, int, unsigned, unsigned, unsigned *, float, _animCharInstance *, int, int);
extern int eoCnt;
extern int lastEoCnt;
extern float shadowLodRangeSq;
void psInitBlockerDMA(void);
_cs *psIsBlockerCsActive(int);
float psGetBlockerLodRangeSq(int);
void psSetBlkLitPos(_fvector *, int);
void psBldBlockerPkt(_fvector *, int, int);
_cs *psBlockerHas2ndCS(int);
void psEndBlkObjs(int);
void psEndBlkDma(void);
extern _worldctx worldCtx[5];
extern int g_doFOVGroups;
extern int g_dontTraverseObjects;
extern int tracingShadow;
extern char pointLights[0x8A0];
void *psGetLightDir(void);
float fogGetFarClipRange(void);
extern "C" void hierCacheForAsm(_fvector *trans, void *lightDir, int pl, float farClip, float one);
extern "C" void hierCacheWorldMatsAsm(_worldctx *wc, float (*light)[4]);
extern "C" void hierTraverseAsm(void *root, void *ctx, int arg);
struct HierStackEnt {
    float eo[3];
    _hierhead **pp;
    short i10;
    short i12;
    unsigned w14;
    unsigned *p18;
    float f1c;
    _animCharInstance *anim;
    short s24;
    short s26;
    int pad28[2];
};
#define SPAD_HIERSTACK (*(HierStackEnt **)0x70000030)
#define SPAD_HIERDEPTH (*(int *)0x70000034)
void mathfUnitMatrix(float (*m)[4]);
void viewSetRot(float (*m)[4][4], int view);
void viewSetTrans(_fvector *v, int view);
void viewSetSkyTrans(_fvector *v, int view);
extern int hierCamDirty __asm__("dirty.476");
extern float hierCamFixAxes[4][4] __asm__("fixAxes.477");
extern float D_00777F80[4][4];
extern _fvector D_00777FC0;
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void vu0MulMatrix3x3(float (*dst)[4], float (*a)[4], float (*b)[4]);
void vu0MulMatrix3x3_1(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfMulMatrix(float (*dst)[4], float (*a)[4], float (*b)[4]);
_hierSkelBone *skelGetRoot(int);
float (*skelGetSkelMat(int))[4][4];
extern "C" void sceGsSyncPath(int, int);
extern "C" void FlushCache(int);
extern char D_0025B890[];
extern char D_00257C10[];
void vu0GetEoAsm(QwData *);
_fvector *viewGetTrans(int);
_lightenv *lightGetEnv(int);
void mathfUnitMatrix(float (*m)[4]);
extern _worldctx *world;
extern _cs *gCs;
extern float (*matStack)[4];
extern float (*fovNorms)[4];
extern float (*fovNorms1)[4];
extern float (*lightDir)[4];
extern "C" int hierCsUpdateAsm(_cs *cs, _fvector *eo);
extern "C" void hierTraverseAsm(void *root, void *ctx, int arg);
void vu0UnitMatrix(float (*m)[4]);
void vu0CopyMatrix(float (*dst)[4], float (*src)[4]);
float (*plightGetParaLight(void))[4];

unsigned *hierGetLastGSCtx(void)
{
    return lastGsCtx;
}
void addSkeleton(_hierSkelBone *bone, int idx)
{
    skelStack[idx] = bone;
}
void addSkelMat(float (*mat)[4][4], int idx)
{
    char *dst = (char *)(idx * 64) + (unsigned)skelMatStack;

    __asm__ volatile("lq $8, 0x0(%0)
	"
                     "lq $9, 0x10(%0)
	"
                     "lq $10, 0x20(%0)
	"
                     "lq $11, 0x30(%0)
	"
                     "sq $8, 0x0(%1)
	"
                     "sq $9, 0x10(%1)
	"
                     "sq $10, 0x20(%1)
	"
                     "sq $11, 0x30(%1)"
                     : : "r"(mat), "r"(dst) : "$8", "$9", "$10", "$11", "memory");
}
#ifdef NON_MATCHING
/* register allocation differs: base pointer lands in $v1 instead of $v0 */
_hierSkelBone *skelGetRoot(int idx)
{
    return (unsigned)idx < 5 ? skelStack[idx] : 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", skelGetRoot__Fi);
#endif
float (*skelGetSkelMat(int idx))[4][4]
{
    return (unsigned)idx < 0xA0 ? &skelMatStack[idx] : 0;
}
INCLUDE_ASM("asm/nonmatchings/common/hier", hier__Fii);
void hierTraceSky(int arg)
{
    _fvector eo;
    _cs *sky = world->skyCs;

    if (sky && sky->epNode) {
        hierCsUpdateAsm(sky, &eo);
        hierTraverseAsm(world->skyCs, &eo, arg);
    }
}
void hierTraceWorld(int arg)
{
    if (world->ep) {
        vu0UnitMatrix(matStack);
        vu0CopyMatrix(fovNorms, (float (*)[4])world->fovNorms);
        vu0CopyMatrix(fovNorms1, (float (*)[4])world->fovNorms[1]);
        vu0CopyMatrix(lightDir, plightGetParaLight());
        hierTraverseAsm(world, &world->eo, arg);
    }
}
#ifdef NON_MATCHING
/* 49/57 words: $s2/$s3 swapped (list end vs ~(1 << mask)) */
void hierTraceCsList(int mask)
{
    _fvector eo;
    CsNode *n;
    CsNode *const end = &CsPool::m_activeList;

    for (n = end->next; n != end; n = n->next) {
        if (!n->cs->drawMe || !n->cs->epNode)
            continue;
        gCs = n->cs;
        gCs->inFov &= ~(1 << mask);
        if (gCs->scaleMe)
            *(float *)0x700003A0 = 1.0f / gCs->scale.x;
        hierCsUpdateAsm(gCs, &eo);
        hierTraverseAsm(n->cs, &eo, mask);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierTraceCsList__Fi);
#endif
#ifdef NON_MATCHING
/* 49/57 words: $s2/$s3 swapped (list end vs ~(1 << mask)) */
void hierTraceHPCsList(int mask)
{
    _fvector eo;
    CsNode *n;
    CsNode *const end = &CsPool::m_HPActiveList;

    for (n = end->next; n != end; n = n->next) {
        if (!n->cs->drawMe || !n->cs->epNode)
            continue;
        gCs = n->cs;
        gCs->inFov &= ~(1 << mask);
        if (gCs->scaleMe)
            *(float *)0x700003A0 = 1.0f / gCs->scale.x;
        hierCsUpdateAsm(gCs, &eo);
        hierTraverseAsm(n->cs, &eo, mask);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierTraceHPCsList__Fi);
#endif
void hierLoadVu1Ucode(void)
{
    FlushCache(0);
    sceGsSyncPath(0, 0);
    *(volatile unsigned *)0x10009020 = 0;
    *(volatile unsigned *)0x10009030 = (unsigned)D_00257C10 & 0xFFFFFFF;
    *(volatile unsigned *)0x1000E010 = 2;
    *(volatile unsigned *)0x10009000 = 0x145;
}
void hierLoadVu0Ucode(void)
{
    sceGsSyncPath(0, 0);
    *(volatile unsigned *)0x10008020 = 0;
    *(volatile unsigned *)0x10008030 = (unsigned)D_0025B890 & 0xFFFFFFF;
    *(volatile unsigned *)0x1000E010 = 1;
    *(volatile unsigned *)0x10008000 = 0x145;
}
void hierCsUpdate(_cs *cs, _worldctx *wc, _lightenv *le, _fvector *out)
{
    _fvector v;

    v.x = wc->eo.x + cs->trans.x;
    v.y = wc->eo.y + cs->trans.y;
    v.z = wc->eo.z + cs->trans.z;
    mathfMulVec((float (*)[4])&cs->mat, &v, out);
    vu0CopyMatrix(matStack, (float (*)[4])&cs->mat);
    vu0MulMatrix3x3(fovNorms, (float (*)[4])&cs->mat, (float (*)[4])wc->fovNorms[0]);
    vu0MulMatrix3x3(fovNorms1, (float (*)[4])&cs->mat, (float (*)[4])wc->fovNorms[1]);
    vu0MulMatrix3x3_1(lightDir, (float (*)[4])&cs->mat, plightGetParaLight());
}
#ifdef NON_MATCHING
/* rows 0-2: dst[r] = a[r].x * b[0] + a[r].y * b[1] + a[r].z * b[2] (all 4 lanes); row 3 = (0, 0, 0, 1) */
void vu0MulMatrix3x3_1(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    float r[3][4];
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            r[i][j] = b[0][j] * a[i][0] + b[1][j] * a[i][1] + b[2][j] * a[i][2];
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = r[i][j];
    dst[3][0] = 0.0f;
    dst[3][1] = 0.0f;
    dst[3][2] = 0.0f;
    dst[3][3] = 1.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", vu0MulMatrix3x3_1__FPA3_fN20);
#endif
#ifdef NON_MATCHING
/* 45/59 words: $s register order and store order differ */
void hierPush(_hierhead **pp, int a, unsigned b, unsigned c, unsigned *d, float f, _animCharInstance *anim, int e, int g)
{
    HierStackEnt *ent = SPAD_HIERSTACK;

    vu0GetEoAsm((QwData *)ent);
    ent->s24 = e;
    ent->anim = anim;
    ent->f1c = f;
    ent->p18 = d;
    ent->w14 = b;
    ent->i12 = c;
    ent->i10 = a;
    ent->pp = pp;
    ent->s26 = g;
    if (SPAD_HIERDEPTH < 0x96) {
        SPAD_HIERDEPTH++;
        SPAD_HIERSTACK++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierPush__FPP9_hierheadiUiUiPUifP17_animCharInstanceii);
#endif
extern "C" void hierTranslateSkel(short *depth, short *matIdx, _animCharInstance *anim, _fvector *trans)
{
    Mat4 a;
    Mat4 b;
    _hierSkelBone *root = skelGetRoot(*depth);

    mathfUnitMatrix(a.m);
    __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                     "lqc2 $vf12, 0x0(%2)
	"
                     "vadd.xyz $vf11, $vf11, $vf12
	"
                     "sqc2 $vf11, %0"
                     : "=m"(a.m[3]) : "r"(&a.m[3]), "r"(trans));
    float (*sm)[4][4] = skelGetSkelMat(*matIdx);
    mathfMulMatrix(b.m, a.m, (float (*)[4])sm);
    addSkelMat((float (*)[4][4])&b, ++*matIdx);
    if (root->skelOutputMatIdx >= 0)
        mathfMulMatrix((float (*)[4])anim->animMatrixPtr[root->skelOutputMatIdx], (float (*)[4])&root->restPoseInv, b.m);
    --*depth;
}
extern "C" void hierRotateSkel(short *depth, short *matIdx, _animCharInstance *anim, _hierSkelBone *node)
{
    Mat4 m;
    _hierSkelBone *root = skelGetRoot(*depth);
    float (*sm)[4][4] = skelGetSkelMat(*matIdx);

    mathfMulMatrix(m.m, (float (*)[4])&node->restPoseInv, (float (*)[4])sm);
    addSkelMat((float (*)[4][4])&m, ++*matIdx);
    if (root->skelOutputMatIdx >= 0)
        mathfMulMatrix((float (*)[4])anim->animMatrixPtr[root->skelOutputMatIdx], (float (*)[4])&root->restPoseInv, m.m);
    --*depth;
}
extern "C" void hierStartRotMatPRH(FVECTOR4 *rot);
extern "C" void hierGetRotMatPRH(float (*m)[4]);
#ifdef NON_MATCHING
/* Are any of the node's channels dirty: (dirty bits from firstIdx) & dirty.animating (hand-written helper in texm's
 * assembly, func_0020803C). Its unaligned read is an lwr: only the bytes up to the word boundary are loaded, the
 * upper bytes keep the value firstIdx had in the register. */
static inline int xfChannelsDirty(AnimationOutputBlock *out, _animtransform *xf)
{
    int first = xf->firstIdx;
    unsigned char *p;
    unsigned w;
    int b, n;
    if (first < 0)
        return 0;
    p = out->dirty + ((unsigned)first >> 3);
    w = first;
    n = 4 - ((unsigned)p & 3);
    for (b = 0; b < n; b++)
        w = (w & ~(0xFFu << (8 * b))) | ((unsigned)p[b] << (8 * b));
    return (w >> (first & 7)) & (signed char)xf->dirty.animating;
}
/* Animated transform node: takes its rotate/translate channels when they changed, builds the node matrix (VU0
 * rotation from the angles, optional pre/post rotation, translation) in the instance's matrix slot and, inside a
 * skeleton, the bone matrix. Returns the matrix (null and *visible = 0 when hidden). */
extern "C" float (*hierAnimTransNode(_animtransform *xf, _hierstack *st, int *visible))[4]
{
    _animCharInstance *ci = st->animCharInst;
    AnimationOutputBlock *out = &ci->animOutput;
    float (*m)[4];
    if (xf->visidx >= 0 && ((out->dirty[xf->visidx / 8] >> (xf->visidx - xf->visidx / 8 * 8)) & 1))
        xf->visible = out->val[xf->visidx];
    if (xf->visible == 0.0f || !out) {
        *visible = 0;
        return 0;
    }
    m = (float (*)[4])((char *)ci->animOutput.atMat + xf->matrixIdx * 64);
    if (xfChannelsDirty(out, xf)) {
        float *val = out->val;
        if (xf->rXidx >= 0)
            xf->rotate.x = -val[xf->rXidx];
        if (xf->rYidx >= 0)
            xf->rotate.y = -val[xf->rYidx];
        if (xf->rZidx >= 0)
            xf->rotate.z = -val[xf->rZidx];
        hierStartRotMatPRH(&xf->rotate);
        if (xf->tXidx >= 0)
            xf->translate.x = val[xf->tXidx];
        if (xf->tYidx >= 0)
            xf->translate.y = val[xf->tYidx];
        if (xf->tZidx >= 0)
            xf->translate.z = val[xf->tZidx];
        hierGetRotMatPRH(m);
        if (xf->bits.applyPreRot) {
            if (xf->bits.applyPostRot) {
                Mat4 tmp;
                mathfMulMatrix(tmp.m, (float (*)[4])xf->preRot, m);
                mathfMulMatrix(m, tmp.m, (float (*)[4])xf->postRot);
            } else {
                mathfMulMatrix(m, (float (*)[4])xf->preRot, m);
            }
        } else if (xf->bits.applyPostRot) {
            mathfMulMatrix(m, m, (float (*)[4])xf->postRot);
        }
        m[3][0] += xf->translate.x;
        m[3][1] += xf->translate.y;
        m[3][2] += xf->translate.z;
        m[3][3] = 1.0f;
    }
    if (st->skelIndex >= 0) {
        float (*dst)[4] = (float (*)[4])skelMatStack[st->skelMatIndex + 1];
        _hierSkelBone *root = skelGetRoot(st->skelIndex);
        mathfMulMatrix(dst, m, (float (*)[4])skelGetSkelMat(st->skelMatIndex));
        st->skelMatIndex++;
        if (root->skelOutputMatIdx >= 0)
            mathfMulMatrix((float (*)[4])ci->animMatrixPtr[root->skelOutputMatIdx], (float (*)[4])&root->restPoseInv, dst);
        st->skelIndex--;
    }
    *visible = 1;
    return m;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierAnimTransNode);
#endif
/* start/collect a rotation by the VU0 microprogram at 0x4D0: hardware, stay asm */
INCLUDE_ASM("asm/nonmatchings/common/hier", hierStartRotMatPRH);
INCLUDE_ASM("asm/nonmatchings/common/hier", hierGetRotMatPRH);
__asm__("#SNFIX_SMALL gTraversalCallback");
extern "C" void hierAttachPtNode(_hierhead *node, int view, _cs *cs, int idx)
{
    QwData eo;
    _fvector out;
    float (*m)[4];

    vu0GetEoAsm(&eo);
    m = (float (*)[4])((char *)(idx * 64) + (unsigned)matStack);
    __asm__ volatile("lqc2 $vf11, %1
	"
                     "lqc2 $vf12, 0x0(%2)
	"
                     "lqc2 $vf13, 0x10(%2)
	"
                     "lqc2 $vf14, 0x20(%2)
	"
                     "vmulax.xyz $ACC, $vf12, $vf11x
	"
                     "vmadday.xyz $ACC, $vf13, $vf11y
	"
                     "vmaddz.xyz $vf11, $vf14, $vf11z
	"
                     "sqc2 $vf11, %0"
                     : "=m"(out) : "m"(eo), "r"(m));
    _fvector *t = viewGetTrans(view);
    __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                     "lqc2 $vf12, 0x0(%2)
	"
                     "vadd.xyz $vf11, $vf11, $vf12
	"
                     "sqc2 $vf11, %0"
                     : "=m"(out) : "r"(&out), "r"(t));
    gTraversalCallback(cs, node->id1, node->id2, *(float (*)[4][4])((char *)matStack + idx * 64), &out);
}
#ifdef NON_MATCHING
/* 127/144 words: temp registers of the three 4xqword copies differ ($v1 vs $a0) */
extern "C" void hierScaleNode(_hierscale *sc, int idx)
{
    Mat4 m;
    QwData eo;
    char *src;
    char *dst;

    if (sc->numKids) {
        mathfUnitMatrix(m.m);
        m.m[0][0] = sc->scale.x;
        m.m[1][1] = sc->scale.y;
        m.m[2][2] = sc->scale.z;
        vu0GetEoAsm(&eo);
        eo.fVec[0] = (eo.fVec[0] + sc->trans.x) / sc->scale.x;
        eo.fVec[1] = (eo.fVec[1] + sc->trans.y) / sc->scale.y;
        eo.fVec[2] = (eo.fVec[2] + sc->trans.z) / sc->scale.z;
        vu0LoadEoAsm(&eo);
        mathfMulMatrix3x3((float (*)[4])((char *)matStack + (idx + 1) * 64), m.m, (float (*)[4])((char *)matStack + idx * 64));
        if (sc->uniform) {
            dst = (char *)((idx + 1) * 64) + (unsigned)fovNorms;
            src = (char *)(idx * 64) + (unsigned)fovNorms;
            __asm__ volatile("lq $8, 0x0(%0)
	"
                             "lq $9, 0x10(%0)
	"
                             "lq $10, 0x20(%0)
	"
                             "lq $11, 0x30(%0)
	"
                             "sq $8, 0x0(%1)
	"
                             "sq $9, 0x10(%1)
	"
                             "sq $10, 0x20(%1)
	"
                             "sq $11, 0x30(%1)"
                             : : "r"(src), "r"(dst) : "$8", "$9", "$10", "$11", "memory");
            dst = (char *)((idx + 1) * 64) + (unsigned)fovNorms1;
            src = (char *)(idx * 64) + (unsigned)fovNorms1;
            __asm__ volatile("lq $8, 0x0(%0)
	"
                             "lq $9, 0x10(%0)
	"
                             "lq $10, 0x20(%0)
	"
                             "lq $11, 0x30(%0)
	"
                             "sq $8, 0x0(%1)
	"
                             "sq $9, 0x10(%1)
	"
                             "sq $10, 0x20(%1)
	"
                             "sq $11, 0x30(%1)"
                             : : "r"(src), "r"(dst) : "$8", "$9", "$10", "$11", "memory");
            dst = (char *)((idx + 1) * 64) + (unsigned)lightDir;
            src = (char *)(idx * 64) + (unsigned)lightDir;
            __asm__ volatile("lq $8, 0x0(%0)
	"
                             "lq $9, 0x10(%0)
	"
                             "lq $10, 0x20(%0)
	"
                             "lq $11, 0x30(%0)
	"
                             "sq $8, 0x0(%1)
	"
                             "sq $9, 0x10(%1)
	"
                             "sq $10, 0x20(%1)
	"
                             "sq $11, 0x30(%1)"
                             : : "r"(src), "r"(dst) : "$8", "$9", "$10", "$11", "memory");
        } else {
            mathfMulMatrix3x3((float (*)[4])((char *)fovNorms + (idx + 1) * 64), m.m, (float (*)[4])((char *)fovNorms + idx * 64));
            mathfMulMatrix3x3((float (*)[4])((char *)fovNorms1 + (idx + 1) * 64), m.m, (float (*)[4])((char *)fovNorms1 + idx * 64));
            m.m[0][0] = 1.0f / sc->scale.x;
            m.m[1][1] = 1.0f / sc->scale.y;
            m.m[2][2] = 1.0f / sc->scale.z;
            mathfMulMatrix3x3((float (*)[4])((char *)lightDir + (idx + 1) * 64), m.m, (float (*)[4])((char *)lightDir + idx * 64));
            mathfNormalizeColumns((float (*)[4])((char *)fovNorms + (idx + 1) * 64));
            mathfNormalizeColumns((float (*)[4])((char *)fovNorms1 + (idx + 1) * 64));
            mathfNormalizeColumns((float (*)[4])((char *)lightDir + (idx + 1) * 64));
            mathfScaleMatrix3x3((float (*)[4])((char *)lightDir + (idx + 1) * 64), (float (*)[4])((char *)lightDir + (idx + 1) * 64), 32.0f);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierScaleNode);
#endif
#ifdef NON_MATCHING
/* Animated scale node: scale/translate channels, eye offset moved into the scaled space, and the next depth of the
 * matrix, frustum normal and light direction stacks (renormalized unless the scale is uniform). 0 when hidden. */
extern "C" int hierAnimScaleNode(_animscale *s, int depth, _animCharInstance *ci)
{
    AnimationOutputBlock *out = ci ? &ci->animOutput : 0;
    float vis = s->vidx == -1 ? 1.0f : out->val[s->vidx];
    Mat4 m;
    QwData eo;
    float *e = (float *)&eo;
    if (vis == 0.0f || !out || !s->numKids)
        return 0;
    if (s->sXidx >= 0)
        s->scale.x = out->val[s->sXidx];
    if (s->sYidx >= 0)
        s->scale.y = out->val[s->sYidx];
    if (s->sZidx >= 0)
        s->scale.z = out->val[s->sZidx];
    if (s->tXidx >= 0)
        s->translate.x = out->val[s->tXidx];
    if (s->tYidx >= 0)
        s->translate.y = out->val[s->tYidx];
    if (s->tZidx >= 0)
        s->translate.z = out->val[s->tZidx];
    mathfUnitMatrix(m.m);
    m.m[0][0] = s->scale.x;
    m.m[1][1] = s->scale.y;
    m.m[2][2] = s->scale.z;
    vu0GetEoAsm(&eo);
    e[0] = (e[0] + (s->translate.x + s->postScale.x)) / s->scale.x + s->preScale.x;
    e[1] = (e[1] + (s->translate.y + s->postScale.y)) / s->scale.y + s->preScale.y;
    e[2] = (e[2] + (s->translate.z + s->postScale.z)) / s->scale.z + s->preScale.z;
    vu0LoadEoAsm(&eo);
    mathfMulMatrix3x3(matStack + (depth + 1) * 4, m.m, matStack + depth * 4);
    {
        float dxy = s->scale.x - s->scale.y;
        float dxz = s->scale.x - s->scale.z;
        if ((dxy < 0.0f ? -dxy : dxy) < 0.001f && (dxz < 0.0f ? -dxz : dxz) < 0.001f) {
            *(Mat4 *)(fovNorms + (depth + 1) * 4) = *(Mat4 *)(fovNorms + depth * 4);
            *(Mat4 *)(fovNorms1 + (depth + 1) * 4) = *(Mat4 *)(fovNorms1 + depth * 4);
            *(Mat4 *)(lightDir + (depth + 1) * 4) = *(Mat4 *)(lightDir + depth * 4);
            return 1;
        }
    }
    mathfMulMatrix3x3(fovNorms + (depth + 1) * 4, m.m, fovNorms + depth * 4);
    mathfMulMatrix3x3(fovNorms1 + (depth + 1) * 4, m.m, fovNorms1 + depth * 4);
    m.m[2][2] = 1.0f / s->scale.z;
    m.m[0][0] = 1.0f / s->scale.x;
    m.m[1][1] = 1.0f / s->scale.y;
    mathfMulMatrix3x3(lightDir + (depth + 1) * 4, m.m, lightDir + depth * 4);
    mathfNormalizeColumns(fovNorms + (depth + 1) * 4);
    mathfNormalizeColumns(fovNorms1 + (depth + 1) * 4);
    mathfNormalizeColumns(lightDir + (depth + 1) * 4);
    mathfScaleMatrix3x3(lightDir + (depth + 1) * 4, lightDir + (depth + 1) * 4, 32.0f);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierAnimScaleNode);
#endif
extern "C" void hierSkelBoneNode(_hierSkelBone *bone, unsigned short *boneIdx, short *matIdx)
{
    Mat4 m;

    addSkeleton(bone, (short)++*boneIdx);
    if (bone->isRootNode) {
        ++*matIdx;
        mathfUnitMatrix(m.m);
        addSkelMat((float (*)[4][4])&m, *matIdx);
    }
}
#ifdef NON_MATCHING
/* 53/141 words: 1.0f/len is built via $at and divided into $f4; retail goes through $v0 and divides into $f1 */
extern "C" void hierLightNode(HierLight *l, int view, int idx)
{
    QwData eo;
    _fvector out;
    _fvector nrm;
    _fvector out2;
    _fvector *t;
    char *m;

    if (l->flag.isOn && l->flag.hasLensFlare && lensFlareListIndex < 0x4A && lensFlareListPtr) {
        vu0GetEoAsm(&eo);
        __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "vadd.xyz $vf11, $vf11, $vf12
	"
                         "sqc2 $vf11, %0"
                         : "=m"(eo) : "r"(&eo), "r"(&l->position));
        m = (char *)(idx * 64) + (unsigned)matStack;
        __asm__ volatile("lqc2 $vf11, %1
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "lqc2 $vf13, 0x10(%2)
	"
                         "lqc2 $vf14, 0x20(%2)
	"
                         "vmulax.xyz $ACC, $vf12, $vf11x
	"
                         "vmadday.xyz $ACC, $vf13, $vf11y
	"
                         "vmaddz.xyz $vf11, $vf14, $vf11z
	"
                         "sqc2 $vf11, %0"
                         : "=m"(out) : "m"(eo), "r"(m));
        t = viewGetTrans(view);
        __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "vadd.xyz $vf11, $vf11, $vf12
	"
                         "sqc2 $vf11, 0x0(%3)"
                         : "=m"(*lensFlareListPtr) : "r"(&out), "r"(t), "r"(lensFlareListPtr));
        register float x __asm__("$f2");
        register float y __asm__("$f3");
        register float z __asm__("$f0");
        register float len __asm__("$f4");
        __asm__ volatile("lwc1 %0, %4
	"
                         "lwc1 %1, %5
	"
                         "lwc1 %2, %6
	"
                         "mula.s %0, %0
	"
                         "madda.s %1, %1
	"
                         "madd.s %3, %2, %2
	"
                         ".word 0x46040104"
                         : "=f"(x), "=f"(y), "=f"(z), "=f"(len) : "m"(out.x), "m"(out.y), "m"(out.z));
        {
            float inv = 1.0f / len;

            nrm.z = z * inv;
            nrm.x = x * inv;
            nrm.y = y * inv;
        }
        if (!l->flag.lightAtInfinity) {
            if (l->flag.isDirectional) {
                register float nx __asm__("$f1");
                register float ox __asm__("$f5");
                register float ny __asm__("$f4");
                register float oy __asm__("$f3");
                register float nz __asm__("$f2");
                register float oz __asm__("$f0");

                m = (char *)(idx * 64) + (unsigned)matStack;
                __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                                 "lqc2 $vf12, 0x0(%2)
	"
                                 "lqc2 $vf13, 0x10(%2)
	"
                                 "lqc2 $vf14, 0x20(%2)
	"
                                 "vmulax.xyz $ACC, $vf12, $vf11x
	"
                                 "vmadday.xyz $ACC, $vf13, $vf11y
	"
                                 "vmaddz.xyz $vf11, $vf14, $vf11z
	"
                                 "sqc2 $vf11, %0"
                                 : "=m"(out2) : "r"(&l->beamNormal), "r"(m));
                __asm__ volatile("lwc1 %0, %1" : "=f"(nx) : "m"(nrm.x));
                __asm__ volatile("lwc1 %0, %1" : "=f"(ox) : "m"(out2.x));
                __asm__ volatile("lwc1 %0, %1" : "=f"(ny) : "m"(nrm.y));
                __asm__ volatile("lwc1 %0, %1" : "=f"(oy) : "m"(out2.y));
                __asm__ volatile("lwc1 %0, %1" : "=f"(nz) : "m"(nrm.z));
                __asm__ volatile("lwc1 %0, %1" : "=f"(oz) : "m"(out2.z));
                __asm__ volatile("mula.s %1, %2
	"
                                 "madda.s %3, %4
	"
                                 "madd.s %0, %5, %6"
                                 : "=f"(nx) : "0"(nx), "f"(ox), "f"(ny), "f"(oy), "f"(nz), "f"(oz));
                lensFlareListPtr->f18 = -nx;
                lensFlareListPtr->f1c = l->beamWidth;
            } else {
                lensFlareListPtr->f1c = 0;
                lensFlareListPtr->f18 = 1.0f;
            }
        }
        lensFlareListPtr->w10 = 0;
        lensFlareListPtr->r = l->lightRed;
        lensFlareListPtr->g = l->lightGreen;
        lensFlareListPtr->b = l->lightBlue;
        lensFlareListPtr->id = l->head.id2;
        lensFlareListPtr->intensity = l->intensity;
        lensFlareListPtr->b10 = (unsigned char)l->flags;
        lensFlareListPtr++;
        lensFlareListIndex++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierLightNode);
#endif
extern "C" void hierParticleNode(HierParticleEmitter *pe, int view, int idx)
{
    QwData eo;
    _fvector out;
    _fvector *t;

    if (pe->flag.isActivated) {
        if (pe->flag.isMoving) {
            char *m = (char *)(idx * 64) + (unsigned)matStack;
            QwData *dst = (QwData *)&pe->cachedOrientation;

            __asm__ volatile("lq $8, 0x0(%4)
	"
                             "lq $9, 0x10(%4)
	"
                             "lq $10, 0x20(%4)
	"
                             "lq $11, 0x30(%4)
	"
                             "sq $8, %0
	"
                             "sq $9, %1
	"
                             "sq $10, %2
	"
                             "sq $11, %3"
                             : "=m"(dst[0]), "=m"(dst[1]), "=m"(dst[2]), "=m"(dst[3]) : "r"(m) : "$8", "$9", "$10", "$11");
            vu0GetEoAsm(&eo);
            idx = idx * 64 + (unsigned)matStack;
            m = (char *)idx;
            __asm__ volatile("lqc2 $vf11, %1
	"
                             "lqc2 $vf12, 0x0(%2)
	"
                             "lqc2 $vf13, 0x10(%2)
	"
                             "lqc2 $vf14, 0x20(%2)
	"
                             "vmulax.xyz $ACC, $vf12, $vf11x
	"
                             "vmadday.xyz $ACC, $vf13, $vf11y
	"
                             "vmaddz.xyz $vf11, $vf14, $vf11z
	"
                             "sqc2 $vf11, %0"
                             : "=m"(out) : "m"(eo), "r"(m));
            t = viewGetTrans(view);
            __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                             "lqc2 $vf12, 0x0(%2)
	"
                             "vadd.xyz $vf11, $vf11, $vf12
	"
                             "sqc2 $vf11, 0xA0(%3)"
                             : "=m"(pe->cachedPosition) : "r"(&out), "r"(t), "r"(pe));
            pe->cachedPosition.w = 1.0f;
        }
    } else {
        vu0GetEoAsm(&eo);
        __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "vadd.xyz $vf11, $vf11, $vf12
	"
                         "sqc2 $vf11, %0"
                         : "=m"(eo) : "r"(&eo), "r"(&pe->position));
        char *m = (char *)(idx * 64) + (unsigned)matStack;
        __asm__ volatile("lqc2 $vf11, %1
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "lqc2 $vf13, 0x10(%2)
	"
                         "lqc2 $vf14, 0x20(%2)
	"
                         "vmulax.xyz $ACC, $vf12, $vf11x
	"
                         "vmadday.xyz $ACC, $vf13, $vf11y
	"
                         "vmaddz.xyz $vf11, $vf14, $vf11z
	"
                         "sqc2 $vf11, %0"
                         : "=m"(out) : "m"(eo), "r"(m));
        t = viewGetTrans(view);
        __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "vadd.xyz $vf11, $vf11, $vf12
	"
                         "sqc2 $vf11, %0"
                         : "=m"(out) : "r"(&out), "r"(t));
        int fx = particleCreateModeledFx(&out, pe);
        hierRegisterLocator(pe->head.id1, fx);
        pe->flag.isActivated = 1;
    }
}
void hierShadow(int ctx, int arg)
{
    _fvector eo;
    _cs *cs;
    int i;

    tracingShadow = 1;
    eoCnt = 0;
    lastGsCtx = 0;
    world = &worldCtx[ctx];
    gCs = 0;
    psInitBlockerDMA();
    for (i = 0; i < 10; i++) {
        cs = psIsBlockerCsActive(i);
        if (cs && cs->drawMe && cs->epNode) {
            lastEoCnt = 999999;
            shadowLodRangeSq = psGetBlockerLodRangeSq(i);
            gCs = cs;
            psSetBlkLitPos(&world->eo, i);
            psBldBlockerPkt(&world->eo, ctx, arg);
            hierCsUpdate(cs, world, cs->lightEnv, &eo);
            hierTraverseAsm(cs, &eo, ctx);
            cs = psBlockerHas2ndCS(i);
            if (cs) {
                hierCsUpdate(cs, world, cs->lightEnv, &eo);
                hierTraverseAsm(cs, &eo, ctx);
            }
            psEndBlkObjs(i);
        }
    }
    psEndBlkDma();
}
INCLUDE_ASM("asm/nonmatchings/common/hier", hierCheckVifComplete__Fv);
INCLUDE_ASM("asm/nonmatchings/common/hier", hierWaitForVif1__Fv);
INCLUDE_ASM("asm/nonmatchings/common/hier", vu0HierRotateAsm__FPA3_A3_fi);
INCLUDE_ASM("asm/nonmatchings/common/hier", hierGetRotatesAsm__Fi);
INCLUDE_ASM("asm/nonmatchings/common/hier", vu0FovTestResAsm__Fv);
INCLUDE_ASM("asm/nonmatchings/common/hier", vu0MulEoForObj__FPA3_f);
#ifdef NON_MATCHING
/* 42/98 words: gcc cross-jumps the two hierPush call tails, retail keeps them separate (0x17c vs 0x188) */
int hierLod(_hierlod *lod, float dist, _hierhead **out, _hierstack *stk)
{
    int i;
    float t;

    for (i = lod->numLods - 1; i >= 0; i--) {
        if (dist < lod->lods[i].switchOutDis) {
            *out = lod->lods[i].child;
            if (lod->ctx)
                stk->gsCtx = lod->ctx;
            if (lod->lods[i].fade > 0.0f) {
                if (lod->lods[i].fade < dist) {
                    t = 1.0f - (dist - lod->lods[i].fade) / (lod->lods[i].switchOutDis - lod->lods[i].fade);
                    if (i - 1 >= 0) {
                        if (t > 0.5f) {
                            *out = lod->lods[i - 1].child;
                            stk->fade = (1.0f - t) * 2.0f;
                            hierPush(&lod->lods[i].child, stk->matIdx, 1, stk->eoCnt, stk->gsCtx, 1.0f, stk->animCharInst, stk->skelIndex, stk->skelMatIndex);
                            return 1;
                        }
                        *out = lod->lods[i - 1].child;
                        stk->fade = 1.0f;
                        hierPush(&lod->lods[i].child, stk->matIdx, 1, stk->eoCnt, stk->gsCtx, t * 2.0f, stk->animCharInst, stk->skelIndex, stk->skelMatIndex);
                        return 1;
                    }
                    stk->fade = t;
                }
            }
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierLod__FP8_hierlodfPP9_hierheadP10_hierstack);
#endif
#ifdef NON_MATCHING
struct _bspresult;
int bspVecTest(_hierbsp *b, _fvector *a, _fvector *c, _bspresult *r);
int bspPointTest(_hierbsp *b, _fvector *pt);
extern _fvector *g_CameraLos;    // this view's camera line-of-sight segments (g_cameraLosPoints[view])
extern int g_CameraLosPerView;
/* CONTROL node: child1 while the camera is clear of the node's volumes, child2 when it is inside one (volType 0) or
 * when a line of sight from the camera (to the monsters) crosses one (volType != 0): buildings that would hide the
 * fight swap to their cut-away version. */
extern "C" void hierCtrlNode(_hierctrl *c, _hierhead **out, int matIdx)
{
    QwData eoq;
    _fvector *eo = (_fvector *)&eoq;
    FVECTOR cam, toCenter, d;
    char res[0x30];
    int clear = 1;
    int i;
    unsigned k;
    HierHead *pick;

    vu0GetEoAsm(&eoq);
    cam.x = -eo->x;
    cam.y = -eo->y;
    cam.z = -eo->z;
    if (c->volType) {
        float r2 = c->sphere.w * c->sphere.w;
        toCenter.x = eo->x + c->sphere.x;
        toCenter.y = eo->y + c->sphere.y;
        toCenter.z = eo->z + c->sphere.z;
        for (i = 0; i < g_CameraLosPerView && clear; i++) {
            float dot, cc;
            mathfMulVec(matStack + matIdx * 4, &g_CameraLos[i], &d);
            dot = toCenter.x * d.x + toCenter.y * d.y + toCenter.z * d.z;
            cc = toCenter.x * toCenter.x + toCenter.y * toCenter.y + toCenter.z * toCenter.z;
            if (g_CameraLos[i].w < dot - c->sphere.w)
                continue;  /* sphere beyond the end of the segment */
            if (r2 < cc - dot * dot)
                continue;  /* the line misses the sphere */
            if (dot < 0.0f && r2 < cc)
                continue;  /* sphere behind the camera */
            d.x = cam.x + d.x * g_CameraLos[i].w;
            d.y = cam.y + d.y * g_CameraLos[i].w;
            d.z = cam.z + d.z * g_CameraLos[i].w;
            for (k = 0; k < c->numVolumes; k = (unsigned char)(k + 1)) {
                bspVecTest(c->bspNodes[k], &cam, &d, (_bspresult *)res);
                if (!(*(unsigned short *)(res + 0x28) == 0 && *(unsigned short *)(res + 0x2C) == 2)) {
                    clear = 0;
                    break;
                }
            }
        }
    } else {
        for (k = 0; k < c->numVolumes; k = (unsigned char)(k + 1)) {
            if (bspPointTest(c->bspNodes[k], &cam) != 0x200) {
                clear = 0;
                break;
            }
        }
    }
    pick = clear ? c->child1 : c->child2;
    if (pick)
        *out = pick;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierCtrlNode);
#endif
#ifdef NON_MATCHING
/* 35/124 words: retail keeps the scratchpad base 0x70000000 in $s0 across calls; constant folding here does not */
void hierInit(void)
{
    _worldctx *w;
    int i;
    unsigned *spad = (unsigned *)0x70000000;

    hierClipRange = 5.0f;
    for (i = 0; i < 5; i++) {
        w = &worldCtx[i];
        w->skyCs = 0;
        w->skyCs2 = 0;
        w->skyClouds = 0;
        w->ep = 0;
    }
    CsPool::init();
    ctxSetUpGSCtx();
    FlushCache(0);
    sceGsSyncPath(0, 0);
    spad[0x3] = 0x1000;
    spad[0x6] = 0x50505050;
    spad[0x0] = 0x30000000;
    spad[0xB] = 0x800000;
    spad[0x5] = 0x20000000;
    spad[0x7] = 0x31000000;
    spad[0x8] = 0x800000;
    spad[0x9] = 0x800000;
    spad[0xA] = 0x800000;
    spad[0x1] = 0;
    spad[0x2] = 0;
    spad[0x4] = 0;
    *(unsigned *)0x10009020 = 3;
    *(unsigned *)0x10009010 = 0xF0000000;
    *(unsigned *)0x1000E010 = 2;
    *(unsigned *)0x10009000 = 0x101;
    sceGsSyncPath(0, 0);
    spad[0x7] = 0x31000000;
    spad[0x0] = 0x30000000;
    *(unsigned *)0x10008010 = 0xF0000000;
    spad[0x5] = 0x20000000;
    *(unsigned *)0x10008020 = 3;
    *(unsigned *)0x1000E010 = 1;
    *(unsigned *)0x10008000 = 0x101;
    sceGsSyncPath(0, 0);
    *(unsigned *)0x10008020 = 0;
    *(unsigned *)0x10008030 = (unsigned)D_0025B890 & 0xFFFFFFF;
    *(unsigned *)0x1000E010 = 1;
    *(unsigned *)0x10008000 = 0x145;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierInit__Fv);
#endif
void hierDisableFov(void)
{
}
void hierSetCsEpNode(_cs *cs, _hierhead *ep)
{
    cs->epNode = ep;
}
void hierSetCsDrawMe(_cs *cs, unsigned char drawMe)
{
    if (cs)
        cs->drawMe = drawMe;
}
void hierSetTraversalCallback(TravCb cb)
{
    gTraversalCallback = cb;
}
#ifdef NON_MATCHING
/* 30/51 words: store scheduling and constant register choice differ */
void hierInitCs(_cs *cs)
{
    cs->epNode = 0;
    cs->lightEnv = lightGetEnv(0);
    cs->drawMe = 0;
    cs->testCollision = 0;
    cs->inFov = 0;
    cs->scaleMe = 0;
    cs->next = 0;
    cs->rot.x = 0;
    cs->rot.y = 0;
    cs->rot.z = 0;
    cs->color.chan.a = 0x80;
    cs->color.chan.r = 0x80;
    cs->color.chan.g = 0x80;
    cs->color.chan.b = 0x80;
    mathfUnitMatrix((float (*)[4])&cs->mat);
    cs->cloakWeight = 16.0f;
    cs->minCellRow = 0x7FFFFFFF;
    cs->maxCellRow = 0x80000001;
    cs->colorQuad.fVec[3] = 1.0f;
    cs->lightMe = 1;
    cs->trans.x = 0;
    cs->trans.y = 0;
    cs->trans.z = 0;
    cs->minCellCol = 0x7FFFFFFF;
    cs->maxCellCol = 0x80000001;
    cs->colorQuad.fVec[0] = 1.0f;
    cs->colorQuad.fVec[1] = 1.0f;
    cs->colorQuad.fVec[2] = 1.0f;
    cs->ptLightMe = 0;
    cs->cloakMe = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierInitCs__FP3_cs);
#endif
void hierSetSwitch(_hierswitch *sw, int which)
{
    if (sw && sw->head.opcode == 6 && which < sw->numKids)
        sw->whichChild = which;
}
#ifdef NON_MATCHING
/* 27/40 words: 1.0f is materialized via $v0 (not $at) and store scheduling differs */
void hierSetCameraLosPoint(_fvector *pos, int view, int idx)
{
    _fvector *dst = &g_cameraLosPoints[view][idx];
    _fvector *t = viewGetTrans(view);
    register float x __asm__("$f2");
    register float y __asm__("$f3");
    register float z __asm__("$f0");
    register float len __asm__("$f4");
    float inv;

    __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                     "lqc2 $vf12, 0x0(%2)
	"
                     "vsub.xyz $vf11, $vf11, $vf12
	"
                     "sqc2 $vf11, 0x0(%0)"
                     : : "r"(dst), "r"(pos), "r"(t) : "memory");
    __asm__ volatile("lwc1 %0, 0x0(%4)
	"
                     "lwc1 %1, 0x4(%4)
	"
                     "lwc1 %2, 0x8(%4)
	"
                     "mula.s %0, %0
	"
                     "madda.s %1, %1
	"
                     "madd.s %3, %2, %2
	"
                     ".word 0x46040104"
                     : "=f"(x), "=f"(y), "=f"(z), "=f"(len) : "r"(dst) : "memory");
    inv = 1.0f / len;
    dst->z = z * inv;
    dst->x = x * inv;
    dst->y = y * inv;
    dst->w = len;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hier", hierSetCameraLosPoint__FP8_fvectorii);
#endif
void hierSetLosPointsPerView(int idx, int points)
{
    g_cameraLosPointsPerView[idx] = points;
}
__asm__("#SNFIX_SMALL dirty.476");
extern "C" void hierSetCamera(float (*mat)[4][4], _fvector *vec, int view)
{
    if (mat) {
        hierCamDirty = 1;
        __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "lqc2 $vf13, 0x10(%2)
	"
                         "lqc2 $vf14, 0x20(%2)
	"
                         "vmulax.xyz $ACC, $vf12, $vf11x
	"
                         "vmadday.xyz $ACC, $vf13, $vf11y
	"
                         "vmaddz.xyz $vf11, $vf14, $vf11z
	"
                         "sqc2 $vf11, %0"
                         : "=m"(D_00777FC0) : "r"(vec), "r"(mat));
        _fvector *t = viewGetTrans(view);
        __asm__ volatile("lqc2 $vf11, 0x0(%1)
	"
                         "lqc2 $vf12, 0x0(%2)
	"
                         "vadd.xyz $vf11, $vf11, $vf12
	"
                         "sqc2 $vf11, %0"
                         : "=m"(D_00777FC0) : "r"(&D_00777FC0), "r"(t));
        mathfMulMatrix(D_00777F80, hierCamFixAxes, (float (*)[4])mat);
    } else if (hierCamDirty) {
        hierCamDirty = 0;
        viewSetRot((float (*)[4][4])D_00777F80, view);
        viewSetTrans(&D_00777FC0, view);
        viewSetSkyTrans(&D_00777FC0, view);
    }
}
extern "C" void hierKillLocator(int idx, _hierhead *node)
{
    unsigned *slot = &FxToKill[idx];

    particleKillFx((int &)*slot);
    *slot = (unsigned)node;
    node->id1 = idx;
}
void hierRegisterLocator(unsigned id, int fx)
{
    unsigned idx;

    if (fx != -1) {
        idx = id - 0x1D4C;
        if (idx < 32)
            FxToKill[idx] = fx;
    }
}
void hierClearLocatorListForReplay(void)
{
    int i;

    for (i = 0; i < 32; i++) {
        unsigned fx = FxToKill[i];

        if (fx >= 100) {
            if (fx != 0xFFFFFFFF) {
                _hierhead *node = (_hierhead *)fx;

                node->id1 = node->id1 + 0x1D4C;
            }
        }
        FxToKill[i] = 0xFFFFFFFF;
    }
}
__asm__("#SNFIX_SMALL g_doFOVGroups");
__asm__("#SNFIX_SMALL g_dontTraverseObjects");
void hierTraverseCSForLocators(_cs *cs, bool flag)
{
    _fvector eo;

    if (cs->epNode) {
        matStack = (float (*)[4])0x70000400;
        fovNorms = (float (*)[4])0x70000900;
        g_doFOVGroups = flag;
        fovNorms1 = (float (*)[4])0x70000E00;
        lightDir = (float (*)[4])0x70001300;
        world = worldCtx;
        gCs = cs;
        g_dontTraverseObjects = 1;
        tracingShadow = 0;
        _fvector *t = viewGetTrans(0);
        void *ld = psGetLightDir();
        int pl = *(int *)(pointLights + 0x820);
        hierCacheForAsm(t, ld, pl, fogGetFarClipRange(), 1.0f);
        hierCacheWorldMatsAsm(world, plightGetParaLight());
        __asm__ volatile("sync
	sync.p");
        hierCsUpdate(cs, world, world->lightEnv, &eo);
        hierTraverseAsm(cs, &eo, 0);
        g_doFOVGroups = 1;
        g_dontTraverseObjects = 0;
    }
}
INCLUDE_ASM("asm/nonmatchings/common/hier", vu0LoadEoAsm__FP6QwData);
INCLUDE_ASM("asm/nonmatchings/common/hier", vu0GetEoAsm__FP6QwData);
