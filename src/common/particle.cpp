#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

/* Effect pool: fx[100] with the free/used intrusive list fxList (handle 0 is reserved: fxList[1].prev is cut). */
struct _ilnklist {
    int used;
    int next;
    int prev;
};
struct sceGsStoreImage {
    unsigned char data[0x70];
};
struct SweepTrailHandle {
    int handle;
    unsigned id;
};

extern ParticleType fx[100];
extern _ilnklist fxList[100];
extern int fxActiveTop;
extern int fxInactiveTop;
extern unsigned long seed;
extern int numActiveFires;
extern _fvector activeFires[] __asm__("D_00793340");
extern int texIndex;
extern unsigned TEX0[][2];
extern unsigned short texId[];
extern TextureInfoType TextureInfo[128];
extern ParticleInfoType ParticleInfo[151];
extern FieldInfoType FieldInfo[151];
extern EmitterInfoType EmitterInfo[151];
extern VisualInfoType VisualInfo[151];
extern int nextLensFlareId __asm__("D_006F8DAC");
extern ParticleType *lensFlarePacket[2];
extern int lensFlareIndex[2];
extern HierLensFlareList *lensFlareList[2];
extern int firstTimeAround;
extern int count_006F88E0;
extern int numAutoEmitters __asm__("D_006F8DA8");
extern HierParticleEmitter *autoEmitters[] __asm__("D_0078FD20");
extern sceGsStoreImage storeImages[74] __asm__("D_007912E0");
extern SweepTrailHandle sweepTrailHandle[4];
extern int monArcParticle[5];
extern _cs *monArcCs[5] __asm__("D_0078F4C0");
extern _fvector monArcVec0[5] __asm__("D_0078F4E0");
extern _fvector monArcVec1[5] __asm__("D_0078F530");
extern int monArcFlag[5] __asm__("D_0078F580");
extern unsigned roboArcCLUT[8];
extern unsigned monArcCLUT[8];
extern QwData dustPacket[] __asm__("D_0078F6E0");
extern QwData *dustPktEnd __asm__("D_006F8D90");
extern unsigned *dustPktA __asm__("D_006F8D94");
extern unsigned *dustPktB __asm__("D_006F8D98");
extern float *dustPktNormals __asm__("D_006F8D9C");
extern float *dustPktColors __asm__("D_006F8DA0");
extern float *dustPktParams __asm__("D_006F8DA4");
extern CompositedDustLighting dustLighting;
extern float dustTexLitNormalMat[3][4];
extern _fvector Gusts;
extern unsigned colorWords[] __asm__("D_00256C10");
extern unsigned char attractors[8];

extern "C" int rand(void);
extern "C" float cosf(float);
extern "C" float sinf(float);
extern "C" int sceGsSetDefStoreImage(sceGsStoreImage *sp, short sbp, short sbw, short spsm, short x, short y, short w, short h);
int timerGetFieldsLastFrame(void);
void defaultContextInit(void);
void texDrawInitDustTextureInfo(int *idx, int vram, int w);
void texDrawDustPacketBuild(QwData *pkt, int vram, int w, unsigned **a, unsigned **b, float **params, float **normals, float **colors, QwData **end);
void ilnkListFill(_ilnklist *list, int n, int *head, bool used);
void lensFlarePacketInit(ParticleType *p, int buf);
void spritePacketInit(ParticleType *p);
void texSpritePacketInit(ParticleType *p);
void linePacketInit(ParticleType *p);
void dustTrailPacketInit(ParticleType *p);
void fragPacketInit(ParticleType *p);
void stripePacketInit(ParticleType *p);
void stritePacketInit(ParticleType *p);
void sprinePacketInit(ParticleType *p);
void kitePacketInit(ParticleType *p);
void wakePacketInit(ParticleType *p);
void sweepTrailPacketInit(ParticleType *p);
int electricArcChainInit(float a, float b, float c, float d, float e, unsigned *clut, unsigned r, int n, int kind);
void electricArcChainAddLink(ParticleType *p, _fvector *pos, bool first, int kind, _fvector *a, _fvector *b);
void mathfRandVector(_fvector *v, float scale);
void mathfMulMatrix(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfMulMatrixTP(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void mathfMatrixFromNormal(float (*m)[4], _fvector *n);
void mathfVectorMinMaxGetMac(_fvector *min, _fvector *max);
void mathfMulTransVecOld(float (*m)[4], _fvector *v, _fvector *out);
void hierRegisterLocator(unsigned id, int fx);
float hdHatTest(_fvector *pos, _fvector *out, float up, bool b, float down);
void electricArcPacketUpdate(ParticleType *p, float a, float b, float c, float d, float e, unsigned *clut, unsigned seed);
void spritePacketBuild(ParticleType *p);
void texSpritePacketBuild(ParticleType *p);
void linePacketBuild(ParticleType *p);
void dustTrailPacketBuild(ParticleType *p);
void fragPacketBuild(ParticleType *p);
void stritePacketBuild(ParticleType *p);
void sprinePacketBuild(ParticleType *p);
void kitePacketBuild(ParticleType *p);
void stripePacketBuild(ParticleType *p);
void sweepTrailPacketBuild(ParticleType *p);
void wakePacketBuild(ParticleType *p);
extern FVECTOR NoOffset;
extern FVECTOR FireOffset;
extern float Up[4][4];
ParticleType *particleGetParticle(int h);
void particleKillAllFx(void);
void particleDestroyEffect(ParticleType *p);
void particleInitAfter(void);
int particleInitFx(int type, float scale);
int particleCreateModeledFx(_fvector *pos, HierParticleEmitter *e);
void particleFOVcalcMaxRadius(ParticleInfoType *info, unsigned type);
int sweepTrailGetHandleFromId(unsigned id);
int sweepTrailGetHandleFromNewId(unsigned id);
void interpolateCurve(_fvector *p0, _fvector *p1, _fvector *p2, _fvector *p3, int n, int w);
void utilUpdateEmitterMatrices(EmitterType *e, _fvector *v);
void wakeEmit(ParticleType *p, _fvector *pos, _fvector *vel, bool preAge, bool sizeRange, float minFrac, int i);

#ifdef NON_MATCHING
#define DEG2RAD 0.017453292f

/* The retail LCG (seed is 64-bit): a float in [1, 2) from 23 bits of the new seed, or its raw bits. */
static inline unsigned long seedNext(void)
{
    seed = seed * 0x41C64E6D + 0x3039;
    return seed;
}
static inline int randBits12(void)
{
    return (int)(((seedNext() >> 9) & 0x7FFFFF) | 0x3F800000);
}
static inline float bitsToFloat(int i)
{
    union {
        int i;
        float f;
    } u;
    u.i = i;
    return u.f;
}
static inline int floatToBits(float f)
{
    union {
        int i;
        float f;
    } u;
    u.f = f;
    return u.i;
}
static inline float rand12(void)
{
    return bitsToFloat(randBits12());
}
static inline int fxIndex(ParticleType *p)
{
    return p - fx;
}
/* Takes the top of the inactive list and pushes it on the active list (inlined everywhere in retail). */
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
#endif

int particleActiveFires(_fvector **out)
{
    *out = activeFires;
    return numActiveFires;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 4/14 words, size 0x38 vs 0x38 */
bool verifyParticleBelongsToHandleAndViceVersa(int *h)
{
    if ((unsigned)*h >= 100)
        return false;
    return fx[*h].backPointer == h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", verifyParticleBelongsToHandleAndViceVersa__FPi);
#endif
void disconnectBackPointer(ParticleType *p)
{
    if (p && p->backPointer) {
        if (*p->backPointer == p - fx)
            *p->backPointer = -1;
        p->backPointer = 0;
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 6/65 words, size 0x104 vs 0xec */
void particleParseTexture(int idx, _hierhead *node)
{
    _hierobject *obj = (_hierobject *)node;
    unsigned numMips = node->id2;
    unsigned *tex0;

    if (node->opcode == 1)
        obj = (_hierobject *)((_hiergroup *)node)->child[0];
    if (idx < 199) {
        tex0 = (unsigned *)&obj->gsTexCtx[0].tex0;
        TEX0[texIndex][0] = tex0[0];
        TEX0[texIndex][1] = tex0[1];
        texId[texIndex] = obj->texId[0];
        if (numMips == 0) {
            TextureInfo[idx].numTextures = 0;
            TextureInfo[idx].texIndex = texIndex;
            TextureInfo[idx].du = 16 << ((TEX0[texIndex][0] >> 26) & 0xF);
            TextureInfo[idx].dv = 16 << ((TEX0[texIndex][0] >> 30) | ((tex0[1] << 2) & 0xC));
        }
        texIndex++;
        TextureInfo[idx].numTextures++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleParseTexture__FiP9_hierhead);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 28/107 words, size 0x1ac vs 0x168 */
void particleFOVcalcMaxRadius(ParticleInfoType *info, unsigned type)
{
    float r, m;

    switch (type) {
    case 1:
    case 2:
        info->maxRadius = 0.01f;
        break;
    case 5:
    case 9:
        info->maxRadius = info->minSize + info->expansionRate * info->lifetime;
        break;
    case 13:
        info->maxRadius = info->maxSize;
        break;
    case 8:
    case 15:
        m = info->minSize > info->maxSize ? info->minSize : info->maxSize;
        r = m + info->expansionRate * info->lifetime;
        info->maxRadius = r;
        if (info->expansionRate < 0.0f) {
            r = r < 0.0f ? -r : r;
            info->maxRadius = r > m ? r : m;
        }
        break;
    case 16:
        if (info->expansionRateTransition > 0) {
            float half = info->expansionRateTransition >> 1;
            float a = info->maxSize + info->initExpansionRate * half;
            float b = a - half * info->expansionRate + info->expansionRate * info->lifetime;
            m = info->maxSize > a ? info->maxSize : a;
            info->maxRadius = m > b ? m : b;
        } else if (info->expansionRate >= 0.0f) {
            info->maxRadius = info->maxSize + info->expansionRate * info->lifetime;
        } else {
            info->maxRadius = info->maxSize;
        }
        break;
    default:
        info->maxRadius = 0.0f;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleFOVcalcMaxRadius__FP16ParticleInfoTypeUi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/30 words, size 0x78 vs 0x70 */
void particleLensFlareAddToDeBounceList(HierLight *l)
{
    unsigned flags = l->flags; /* a separate load: ee-gcc merges the two bitfield tests into one 64-bit mask snfix can't expand */

    if ((flags & 0x80) && l->head.id2 == 0)
        l->head.id2 = nextLensFlareId++;
    else
        l->head.id2 = 0x7FF;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleLensFlareAddToDeBounceList__FP9HierLight);
#endif
void particleGetLensFlareInfo(HierLensFlareList **list, int *num, int buf)
{
    ParticleType *p = lensFlarePacket[buf & 1];

    if (p) {
        *list = (HierLensFlareList *)p->pos;
        lensFlareIndex[buf & 1] = 0;
        *num = 0;
    } else {
        *list = 0;
        *num = -1;
    }
}
void particleSetLensFlareNumEntries(int n, int buf)
{
    lensFlareIndex[buf & 1] = n;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/102 words, size 0x18c vs 0x198 */
void particleAddLensFlareToList(ParticleType *p, int buf)
{
    int n = lensFlareIndex[buf & 1];
    HierLensFlareList *e;
    unsigned c;

    if (n < 74) {
        e = &lensFlareList[buf & 1][n];
        *(_fvector *)e = (p->vis.lightColor & 0x8000000) ? p->emit.ParticleToEmitVec : p->FOVsphere;
        c = p->vis.lightColor;
        e->ID = 0;
        e->Zfudge = p->info.Zfudge;
        e->rgb.x = (c & 0xFF) * 0.0039215684f;
        e->rgb.y = ((c >> 8) & 0xFF) * 0.0039215684f;
        e->rgb.z = ((c >> 16) & 0xFF) * 0.0039215684f;
        e->flags = 3;
        e->intensity = p->info.lightIntensity * p->info.lightIntensity;
        lensFlareIndex[buf & 1]++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleAddLensFlareToList__FP13_ParticleTypei);
#endif
INCLUDE_ASM("asm/nonmatchings/common/particle", particleLensFlareTestZbuffer__Fi);
INCLUDE_ASM("asm/nonmatchings/common/particle", particleLensFlareRender__FP6QwDataiPPUi);
#ifdef NON_MATCHING
/* equivalent (untuned): 62/203 words, size 0x32c vs 0x32c */
void particleInit(void)
{
    int i;
    sceGsStoreImage *si;

    defaultContextInit();
    if (firstTimeAround) {
        for (i = 0; i < 151; i++) {
            FieldInfoType *f = &FieldInfo[i];
            EmitterInfoType *e = &EmitterInfo[i];
            ParticleInfoType *pi = &ParticleInfo[i];

            if (f->dragPhaseIn > 0)
                f->dragPhaseInFactor = 1.0f / f->dragPhaseIn;
            else
                f->dragPhaseInFactor = 0.0f;
            if (VisualInfo[i].attribs == 8)
                e->attribs &= ~0x50;
            if (!(e->attribs & 4))
                e->spread = (1.0f - cosf(e->spread * 0.5f * DEG2RAD)) * 0.5f;
            pi->recipLifetime = 1.0f / pi->lifetime;
            particleFOVcalcMaxRadius(pi, VisualInfo[i].attribs);
            pi->colorsPerFrame = ((VisualInfo[i].numColors - 1) << 12) / (unsigned)pi->lifetime;
        }
    } else {
        particleKillAllFx();
    }
    nextLensFlareId = 1;
    firstTimeAround = 0;
    count_006F88E0 = 0;
    texIndex = 0;
    lensFlarePacket[0] = 0;
    lensFlarePacket[1] = 0;
    numAutoEmitters = 0;
    texDrawInitDustTextureInfo(&texIndex, 0xFD800, 0x40);
    ilnkListFill(fxList, 100, &fxInactiveTop, false);
    fxList[1].prev = -1;
    fxActiveTop = -1;
    for (i = 0; i < 128; i++) {
        TextureInfo[i].numTextures = 0;
        texId[i] = 0xFFFF;
    }
    for (si = storeImages; si < storeImages + 74; si++)
        sceGsSetDefStoreImage(si, 0, 10, 0x30, 0, 0, 4, 4);
    for (i = 0; i < 4; i++) {
        sweepTrailHandle[i].handle = -1;
        sweepTrailHandle[i].id = 0;
    }
    for (i = 0; i < 151; i++) {
        VisualInfo[i].texMask = 1;
        ParticleInfo[i].mapsPerFrame = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleInit__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 14/369 words, size 0x588 vs 0x5c4 */
/* A lens flare packet per frame buffer: a pool entry drawn as type 11. */
static inline void initLensFlarePacket(int buf)
{
    int h = fxAlloc();
    ParticleType *p;

    if (h == -1) {
        lensFlarePacket[buf] = 0;
        return;
    }
    p = &fx[h];
    p->numParticles = 3;
    p->vis.attribs = 11;
    p->field.attribs = 0x1000;
    lensFlarePacket[buf] = p;
    p->prevViewMat = 0;
    p->master = 0;
    p->slave = 0;
    lensFlarePacketInit(p, buf);
}
void particleInitAfter(void)
{
    int i, h;

    initLensFlarePacket(0);
    initLensFlarePacket(1);
    for (i = 0; i < 5; i++) {
        monArcParticle[i] = -1;
        monArcCs[i] = 0;
        monArcFlag[i] = 1;
    }
    for (i = 0; i < 151; i++) {
        VisualInfoType *v = &VisualInfo[i];
        ParticleInfoType *pi = &ParticleInfo[i];

        v->texMask = TextureInfo[v->texInfoIndex].numTextures;
        pi->mapsPerFrame = (v->texMask << 12) / (unsigned)((pi->attribs & 4) ? pi->texAnimLifetime : pi->lifetime);
    }
    for (h = fxActiveTop; h != -1; h = fxList[h].prev) {
        ParticleType *p = &fx[h];

        if (p->vis.attribs == 4) {
            unsigned attribs = p->info.attribs;
            int n = TextureInfo[p->vis.texInfoIndex].numTextures;

            p->info.attribs = attribs & ~0x400;
            p->vis.texMask = n;
            p->info.mapsPerFrame = (n << 12) / ((attribs & 4) ? p->info.texAnimLifetime : p->info.lifetime);
            texSpritePacketInit(p);
        }
    }
    texDrawDustPacketBuild(dustPacket, 0xFD800, 0x40, &dustPktA, &dustPktB, &dustPktParams, &dustPktNormals, &dustPktColors, &dustPktEnd);
    ((int *)dustPktParams)[-4] = 0x118;
    dustPktParams[0] = 4.0f;
    dustPktParams[1] = 4.0f;
    dustPktParams[2] = 6.0f;
    dustPktParams[4] = 106.0f;
    dustPktParams[5] = 1.2075472f;
    dustPktParams[6] = -1000.0f;
    dustPktParams[7] = 72.0f;
    for (i = 0; i < 4; i++) {
        float c = cosf(dustLighting.pitch[i] * DEG2RAD);

        dustTexLitNormalMat[0][i] = sinf(dustLighting.heading[i] * DEG2RAD) * c;
        dustTexLitNormalMat[1][i] = cosf(dustLighting.heading[i] * DEG2RAD) * c;
        dustTexLitNormalMat[2][i] = sinf(dustLighting.pitch[i] * DEG2RAD);
    }
    for (i = 0; i < 12; i++)
        dustPktNormals[i] = ((float *)dustTexLitNormalMat)[i];
    for (i = 0; i < 20; i++)
        dustPktColors[i] = ((unsigned char *)&dustLighting)[i];
    numActiveFires = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleInitAfter__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 98/207 words, size 0x33c vs 0x318 */
/* One electric arc per monster (5 slots keyed by cs); node >= 30 means a robot (thinner arc, nodes 0 and 5 not jittered). */
void particleMonsterArcSetNodePos(_cs *cs, unsigned node, unsigned unused, _fvector *pos)
{
    int robo = 0;
    int i;

    if (node >= 30) {
        robo = 1;
        node -= 30;
    }
    for (i = 0; i < 5; i++) {
        ParticleType *p;
        FVECTOR v, d;
        float scale;

        if (monArcCs[i] == 0)
            monArcCs[i] = cs;
        if (monArcCs[i] != cs)
            continue;
        p = particleGetParticle(monArcParticle[i]);
        if (!p) {
            if (robo) {
                unsigned *clut = &roboArcCLUT[(rand() >> 8) & 7];
                monArcParticle[i] = electricArcChainInit(1.0f, 0.5f, 0.0f, 0.4f, 0.0f, clut, rand(), 8, 3);
            } else {
                unsigned *clut = &monArcCLUT[rand() & 7];
                monArcParticle[i] = electricArcChainInit(8.0f, 0.4f, 0.0f, 0.5f, 0.0f, clut, rand(), 8, 2);
            }
            if (monArcParticle[i] != -1) {
                p = particleGetParticle(monArcParticle[i]);
                if (!p)
                    return;
                p->backPointer = &monArcParticle[i];
            }
            if (!p)
                return;
        }
        if (p->numParticles < 9) {
            scale = 8.0f;
            if (robo) {
                scale = 3.0f;
                if (node == 0 || node == 5) {
                    v = *pos;
                    goto add;
                }
            }
            mathfRandVector(&v, 1.0f);
            v.x = pos->x + v.x * scale;
            v.y = pos->y + v.y * scale;
            v.z = pos->z + v.z * scale;
        add:
            if (p->numParticles == 1) {
                v.w = 0.0f;
                electricArcChainAddLink(p, &v, true, 0, &monArcVec0[i], &monArcVec1[i]);
                p->info.maxSize = 0.0f;
            } else {
                vecSub(&d, &v, &p->pos[p->numParticles - 2]);
                v.w = eeSqrtf(vecLenSq(&d));
                electricArcChainAddLink(p, &v, false, 2, &monArcVec0[i], &monArcVec1[i]);
                p->info.maxSize += v.w;
            }
        }
        return;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleMonsterArcSetNodePos__FP3_csUiUiP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/61 words, size 0xe0 vs 0xf4 */
int sweepTrailGetHandleFromNewId(unsigned id)
{
    int i, h;

    for (i = 0; i < 4; i++) {
        if (sweepTrailHandle[i].handle == -1) {
            h = fxAlloc();
            if (h == -1)
                return -1;
            sweepTrailHandle[i].id = id;
            sweepTrailHandle[i].handle = h;
            fx[h].backPointer = &sweepTrailHandle[i].handle;
            return h;
        }
    }
    return -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", sweepTrailGetHandleFromNewId__FUi);
#endif
int sweepTrailGetHandleFromId(unsigned id)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (sweepTrailHandle[i].id == id) {
            if (particleGetParticle(sweepTrailHandle[i].handle)->numParticles == 128)
                return -1;
            return sweepTrailHandle[i].handle;
        }
    }
    return -1;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 6/81 words, size 0x144 vs 0x144 */
/* Catmull-Rom between p1 and p2: writes n - 1 points every other vector after p1 (trail vertices come in pairs), w = flags. */
void interpolateCurve(_fvector *p0, _fvector *p1, _fvector *p2, _fvector *p3, int n, int w)
{
    float dt = 1.0f / n;
    float t = dt;
    _fvector *out = p1 + 2;
    int i;

    for (i = 1; i < n; i++) {
        float t2 = t * t;
        float t3 = t2 * t;
        float c0 = t2 - (t + t3) * 0.5f;
        float c1 = t3 * 1.5f - t2 * 2.5f + 1.0f;
        float c2 = t3 * -1.5f + (t2 + t2) + t * 0.5f;
        float c3 = (t3 - t2) * 0.5f;

        out->x = p0->x * c0 + p1->x * c1 + p2->x * c2 + p3->x * c3;
        out->y = p0->y * c0 + p1->y * c1 + p2->y * c2 + p3->y * c3;
        out->z = p0->z * c0 + p1->z * c1 + p2->z * c2 + p3->z * c3;
        WI(out) = w;
        out += 2;
        t += dt;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", interpolateCurve__FP8_fvectorN30ii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 54/245 words, size 0x3d0 vs 0x3d4 */
static inline void extrapolate(_fvector *dst, _fvector *a, _fvector *b)
{
    vecScale(dst, a, 2.0f);
    vecSub(dst, dst, b);
}
/* A sword-style trail: each call appends the segment a-b and smooths the previous two with 3 interpolated pairs. */
void particleSweepTrail(_fvector *a, _fvector *b, unsigned id)
{
    int h = sweepTrailGetHandleFromId(id);
    ParticleType *p;
    _fvector *P;
    int base, j, k;
    FVECTOR t0, t1;

    if (h != -1) {
        p = &fx[h];
    } else {
        h = sweepTrailGetHandleFromNewId(id);
        if (h == -1)
            return;
        p = &fx[h];
        p->emit.fxId = id;
        p->numParticles = 0;
        p->prevViewMat = 0;
        p->master = 0;
        p->slave = 0;
        p->info.attribs = 0;
        p->maxParticles = 128;
        p->vis.attribs = 2;
        p->vis.colorIndex = 0x36FFFFFF;
        p->emit.info.onsetDelay = 1;
        p->field.attribs = 0x1000;
        p->info.lifetime = 64;
        p->info.recipLifetime = 0.015625f;
        p->info.mapsPerFrame = 4;
        sweepTrailPacketInit(p);
    }
    base = p->numParticles ? (p->numParticles + 3) * 2 : 0;
    P = p->pos;
    P[base] = *a;
    P[base + 1] = *b;
    if (p->numParticles > 0) {
        for (j = 0; j < 2; j++) {
            if (p->numParticles >= 2)
                t0 = P[base - 16 + j];
            else
                extrapolate(&t0, &P[base - 8 + j], &P[base + j]);
            extrapolate(&t1, &P[base + j], &P[base - 8 + j]);
            interpolateCurve(&t0, &P[base - 8 + j], &P[base + j], &t1, 4, 2);
        }
        for (k = 1; k < 4; k++)
            p->age[p->numParticles + 3 - k] = k;
        if (p->numParticles >= 2) {
            for (j = 0; j < 2; j++) {
                if (p->numParticles >= 6)
                    t0 = P[base - 24 + j];
                else
                    extrapolate(&t0, &P[base - 16 + j], &P[base - 8 + j]);
                interpolateCurve(&t0, &P[base - 16 + j], &P[base - 8 + j], &P[base + j], 4, 2);
            }
        }
    }
    if (p->numParticles > 0) {
        WI(&P[base - 7]) |= 2;
        WI(&P[base]) = 2;
        WI(&P[base + 1]) = 0;
        p->numParticles += 4;
    } else {
        WI(&P[base + 1]) = 0x8000;
        WI(&P[base]) = 0x8002;
        p->numParticles = 1;
    }
    p->age[p->numParticles - 1] = 0;
    p->info.attribs &= ~0x400;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleSweepTrail__FP8_fvectorT0Ui);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/118 words, size 0x1d8 vs 0x180 */
void utilUpdateEmitterMatrices(EmitterType *e, _fvector *v)
{
    float (*parentMat)[4] = (float (*)[4])e->parentMat;
    float (*psMat)[4] = (float (*)[4])e->particleSpaceMat;
    float (*mat)[4] = (float (*)[4])e->info.Mat;
    FVECTOR t, d;
    FMATRIX m;

    if (parentMat) {
        _fvector *iv = e->info.Vec;

        t.x = parentMat[0][0] * iv->x + parentMat[1][0] * iv->y + parentMat[2][0] * iv->z;
        t.y = parentMat[0][1] * iv->x + parentMat[1][1] * iv->y + parentMat[2][1] * iv->z;
        t.z = parentMat[0][2] * iv->x + parentMat[1][2] * iv->y + parentMat[2][2] * iv->z;
        if (!psMat) {
            mathfMulMatrix(e->ParticleToEmitMat, mat, parentMat);
            vecAdd(&e->ParticleToEmitVec, &t, e->parentVec);
        } else {
            mathfMulMatrix(m, mat, parentMat);
            mathfMulMatrixTP(e->ParticleToEmitMat, m, (float (*)[4])e->particleSpaceMat);
            vecAdd(&t, &t, e->parentVec);
            vecSub(&d, &t, e->particleSpaceVec);
            mathfMulVec((float (*)[4])e->particleSpaceMat, &d, &e->ParticleToEmitVec);
        }
    } else if (!psMat) {
        matCopy(e->ParticleToEmitMat, mat);
        e->ParticleToEmitVec = *e->info.Vec;
    } else {
        mathfMulMatrixTP(e->ParticleToEmitMat, mat, psMat);
        vecSub(&d, e->particleSpaceVec, e->info.Vec);
        mathfMulVec((float (*)[4])e->particleSpaceMat, &d, &e->ParticleToEmitVec);
    }
    if (e->particleSpaceMat)
        mathfMulVec((float (*)[4])e->particleSpaceMat, v, v);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", utilUpdateEmitterMatrices__FP11EmitterTypeP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 15/51 words, size 0xcc vs 0xc4 */
void pedSplat(ParticleType *p, _fvector *v)
{
    int i = p->numParticles + p->newParticles;
    _fvector *d;

    if (i < p->maxParticles) {
        p->newParticles++;
        p->vel[i].x = v->x;
        p->vel[i].y = v->y;
        p->vel[i].z = v->z + 2.0f;
        d = &p->vel[i];
        WI(d) = ((seedNext() >> 9) & 7) | 0xFF8;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", pedSplat__FP13_ParticleTypeP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/42 words, size 0xa8 vs 0x84 */
void pedSplatEmit(ParticleType *p)
{
    int i;

    if (p->newParticles) {
        for (i = p->numParticles; i < p->numParticles + p->newParticles; i++) {
            p->pos[i] = p->vel[i];
            p->vel[i].w = p->info.recipLifetime;
            ((int *)&p->vel[i])[0] = 0;
            ((int *)&p->vel[i])[1] = 0;
            ((int *)&p->vel[i])[2] = 0;
        }
        p->numParticles += p->newParticles;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", pedSplatEmit__FP13_ParticleType);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/149 words, size 0x254 vs 0x1d0 */
/* Emits from the boundary circle of StaticMat (rows 0 and 2) at StaticVec, alternating quadrants with the random bits. */
void particleEmitBoundaryFx(ParticleType *p)
{
    float cum = p->emit.cumulativeRate + p->emit.info.rate;
    int n = (int)cum;
    int r;
    float x, y, q;
    _fvector *row0, *row2, *vel;

    p->emit.cumulativeRate = cum;
    p->newParticles = n;
    if ((short)n != 0) {
        p->emit.cumulativeRate = 0.0f;
        p->emit.info.rate = 0.0f;
        if (p->numParticles < p->maxParticles) {
            p->pos[p->numParticles] = p->emit.StaticVec;
            r = randBits12();
            x = bitsToFloat(r);
            x = (x + x) - x * x;
            y = 1.0f - x * x;
            q = eeSqrtf(y);
            row0 = (_fvector *)p->emit.StaticMat[0];
            row2 = (_fvector *)p->emit.StaticMat[2];
            {
                float s0 = (r & 1) ? -1.0f : 1.0f;
                float s2 = (r & 2) ? -1.0f : 1.0f;
                float v = p->emit.info.minVel;

                WI(&p->pos[p->numParticles]) = ((seedNext() >> 9) & 7) | 0xFF8;
                vel = &p->vel[p->numParticles];
                vel->x = row0->x * s0 * v * x + row2->x * s2 * v * q;
                vel->y = row0->y * s0 * v * x + row2->y * s2 * v * q;
                vel->z = row0->z * s0 * v * x + row2->z * s2 * v * q;
                vel->w = p->info.recipLifetime;
            }
            p->numParticles++;
        }
        p->emit.info.duration = p->info.lifetime;
        p->emit.info.attribs |= 1;
    } else if (--p->emit.info.duration <= 0) {
        p->emit.info.attribs = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleEmitBoundaryFx__FP13_ParticleType);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 14/108 words, size 0x1b0 vs 0x1b0 */
void wakeEmit(ParticleType *p, _fvector *pos, _fvector *vel, bool preAge, bool sizeRange, float minFrac, int i)
{
    int r = randBits12();
    float one = 1.0f;
    float s;

    if (preAge) {
        one = 1.0f - (2.0f - bitsToFloat(r)) * p->emit.info.preAgeFactor;
        vel->w = p->info.recipLifetime / one;
    } else {
        vel->w = p->info.recipLifetime;
    }
    if (sizeRange)
        s = (minFrac + (1.0f - minFrac) * (bitsToFloat(r) - 1.0f)) * 4088.0f;
    else
        s = one * 4088.0f;
    WI(pos) = (int)s;
    if (WI(pos) < 8)
        WI(pos) = 8;
    WI(pos) = (WI(pos) & ~7) | ((seedNext() >> 9) & 7);
    if (!i)
        vu0BBoxInit(pos);
    else
        vu0BBoxAdd(pos);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", wakeEmit__FP13_ParticleTypeP8_fvectorT1bT3fi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 6/1039 words, size 0x103c vs 0xe44 */
/* Spawns this frame's particles. Each new one gets a direction (random cone, or the rotating StaticRotAxis when emit attrib 4),
   a speed and an offset in [min, max), and a spawn point spread along the emitter's motion since the last frame. Retail keeps all of
   this in VU0 registers (vf17..vf29); the bounding box of the new positions goes to bbMin/bbMax through vf30/vf31. */
void particleEmit(ParticleType *p, _fvector *bbMin, _fvector *bbMax)
{
    int fields = timerGetFieldsLastFrame();
    float fF = fields;
    EmitterType *e = &p->emit;
    FVECTOR wind, disp, inh, rel, axis, sv, rv, dir, v13, o;
    float preAgeW, recipN, minFrac, colorRate, nc, alphaMin;
    float offScale, velScale, speed, offset, speedRange, offRange, speedBase, offBase;
    float frac, remain, dFrac, dRemain, z25, relW;
    int n, start, i, mask, va;
    int velRange, offRangeOn, sizeRange, preAge, flag27, rot;
    int is8, is13, is3, is6, is15, is9, is16;
    unsigned colorWord;
    _fvector *pos, *vel;
    float *R0, *R1, *R2;

    if ((e->info.attribs & 2) && e->info.onsetDelay == 0) {
        e->info.duration -= fields;
        if (e->info.duration < 0) {
            e->info.attribs = 0;
            return;
        }
    }
    if (e->info.attribs & 0x800000) {
        if (e->parentVec->w == 1.0f) {
            e->parentVec->w = 0.0f;
            e->info.attribs |= 0x4000000;
        } else {
            e->info.onsetDelay = 1;
            e->info.attribs &= ~0x4000000;
            return;
        }
    }
    if (p->field.attribs & 0x400) {
        wind.x = p->field.wind.x + Gusts.x * p->field.wind.w;
        wind.y = p->field.wind.y + Gusts.y * p->field.wind.w;
        wind.z = p->field.wind.z + Gusts.z * p->field.wind.w;
    } else {
        wind.z = p->field.wind.z;
        wind.x = 0.0f;
        wind.y = 0.0f;
    }
    if (e->info.attribs & 0x300) {
        if (!(e->info.attribs & 0x400))
            e->prevParticleToEmitVec = e->ParticleToEmitVec;
        utilUpdateEmitterMatrices(e, &wind);
        e->info.attribs &= ~0x100;
    }
    if (e->info.onsetDelay != 0) {
        e->info.onsetDelay = 0;
        return;
    }
    preAgeW = p->info.lifetime * e->info.preAgeFactor;
    if (e->fxId == 0x92 || e->fxId == 20)
        disp = e->parentVec[1];
    else
        vecSub(&disp, &e->ParticleToEmitVec, &e->prevParticleToEmitVec);
    vecScale(&inh, &disp, e->info.velocityInherit / fF);
    if (e->info.attribs & 0x10000000)
        inh.z = inh.z > 0.0f ? inh.z * 0.125f : inh.z * -0.1875f;
    vecSub(&rel, &inh, &wind);

    if (p->vis.attribs == 8) {
        n = fields;
    } else {
        if ((e->info.attribs & 0x2000000) && (unsigned)e->info.duration % e->info.emissionPulse != 0)
            return;
        if (e->info.attribs & 0x1000000) {
            if (e->fxId == 20) {
                float a = eeSqrtf(inh.x * inh.x + inh.y * inh.y) * 1.5f / ParticleInfo[20].maxSize;
                float b = inh.z * 0.625f;
                float s = a + b;

                e->info.rate = 0.0f;
                if (0.25f < s) {
                    e->info.rate = a + b * 8.0f;
                    e->cumulativeRate += e->info.rate;
                }
                if (s < 1.0f)
                    s = 1.0f;
                p->info.minSize = ParticleInfo[20].minSize * s;
                p->info.maxSize = ParticleInfo[20].maxSize * s;
                e->info.minVel = EmitterInfo[20].minVel * s;
                e->info.maxVel = EmitterInfo[20].maxVel * s;
            } else {
                float r = eeSqrtf(vecLenSq(&disp)) * 4.0f / p->info.maxSize;

                e->cumulativeRate += e->info.rate > r ? e->info.rate : r;
            }
        } else {
            e->cumulativeRate += e->info.rate * fF;
        }
        n = (int)e->cumulativeRate;
        e->cumulativeRate -= (float)n;
        if (p->maxParticles < p->numParticles + n)
            n = p->maxParticles - p->numParticles;
    }
    if (n == 0)
        goto done;

    start = p->numParticles;
    p->numParticles = start + n;
    p->newParticles = n;
    recipN = 1.0f / n;
    mask = 3;
    if ((p->info.attribs & 0x2000) && p->vis.texInfoIndex != 0x22)
        mask = 15;
    if (p->info.attribs & 2)
        mask = (unsigned char)((*(unsigned char *)&p->vis.texMask << 2) | 3);
    if (p->info.attribs & 0x20)
        mask &= 0xFE;
    velRange = e->info.minVel < e->info.maxVel;
    offRangeOn = e->info.minOffset < e->info.maxOffset;
    va = p->vis.attribs;
    is8 = va == 8;
    is13 = va == 13;
    is3 = va == 3;
    is6 = va == 6;
    is15 = va == 15;
    is9 = va == 9;
    is16 = va == 16;
    rot = (e->info.attribs >> 2) & 1;
    sizeRange = 0;
    if (p->info.minSize < p->info.maxSize && !is13 && !is8)
        sizeRange = !is9;
    preAge = 0;
    if (0.0f < e->info.preAgeFactor)
        preAge = va != 8;
    flag27 = (e->info.attribs >> 27) & 1;
    nc = (float)(p->vis.numColors - 1);
    colorWord = colorWords[p->vis.colorIndex];
    minFrac = p->info.minSize / p->info.maxSize;
    colorRate = nc / p->info.lifetime;
    if (rot) {
        if (0.0f < e->info.spread) {
            float d = p->info.Zfudge * DEG2RAD;
            float spr = e->info.spread;

            e->StaticVel.x = cosf(d) * spr;
            e->StaticVel.y = spr;
            e->StaticVel.z = sinf(d) * e->info.spread;
            e->StaticRotAxis.x = 1.0f;
            e->StaticRotAxis.y = e->info.normalScale;
            e->StaticRotAxis.z = 0.0f;
            e->StaticRotAxis.w = p->info.lifetime;
            e->info.spread = -1.0f;
            e->StaticVel.w = p->info.lifetime;
        }
        axis = e->StaticRotAxis;
        sv = e->StaticVel;
    }
    if (sizeRange && is15)
        alphaMin = p->info.minSize * 255.0f / p->info.maxSize;
    else
        alphaMin = 0.0f;
    pos = &p->pos[start];
    vel = &p->vel[start];

    offScale = e->info.normOffsScale;
    velScale = e->info.normalScale;
    speed = e->info.minVel;
    offset = e->info.minOffset;
    speedRange = (velRange ? e->info.maxVel : e->info.minVel) - speed;
    offRange = (offRangeOn ? e->info.maxOffset : e->info.minOffset) - offset;
    speedBase = speed - speedRange;
    offBase = offset - offRange;
    frac = 0.0f;
    remain = fF;
    if (e->fxId == 0x92)
        z25 = e->parentVec->w;
    if (e->fxId == 20)
        z25 = e->parentVec->w - 5.0f;
    relW = preAge ? preAgeW : 0.0f;
    dFrac = recipN;
    dRemain = -remain * recipN;
    R0 = e->ParticleToEmitMat[0];
    R1 = e->ParticleToEmitMat[1];
    R2 = e->ParticleToEmitMat[2];

    for (i = start; i < p->numParticles; i++, pos++, vel++) {
        int k;

        frac += dFrac;
        remain += dRemain;
        if (rot) {
            float ax = axis.x, az = axis.z;

            dir = axis;
            relW = sv.w - axis.w;
            e->info.maxVel *= e->StaticVel.y;
            e->info.minVel *= e->StaticVel.y;
            p->info.maxSize *= e->StaticVel.y;
            p->info.minSize *= e->StaticVel.y;
            axis.y *= sv.y;
            axis.w *= sv.y;
            axis.x = -sv.z * az + sv.x * ax;
            axis.z = sv.x * az + sv.z * ax;
        } else {
            mathfRandVector(&rv, e->info.spread);
            dir = rv;
        }
        if (velRange || offRangeOn) {
            float r = rand12();

            speed = speedBase + speedRange * r;
            offset = offBase + offRange * r;
        }
        for (k = 0; k < 4; k++) {
            float base = R0[k] * dir.x + R2[k] * dir.z;

            ((float *)&v13)[k] = base + R1[k] * velScale * dir.y;
            ((float *)&o)[k] = base + R1[k] * offScale * dir.y;
        }
        vel->x = rel.x + v13.x * speed;
        vel->y = rel.y + v13.y * speed;
        vel->z = rel.z + v13.z * speed;
        vel->w = relW + v13.w * speed;
        pos->x = e->prevParticleToEmitVec.x + o.x * offset + disp.x * frac;
        pos->y = e->prevParticleToEmitVec.y + o.y * offset + disp.y * frac;
        pos->z = e->prevParticleToEmitVec.z + o.z * offset + disp.z * frac;
        pos->w = o.w;
        if (!rot) {
            pos->x += v13.x * remain * speed;
            pos->y += v13.y * remain * speed;
            pos->z += v13.z * remain * speed;
        }
        if (flag27) {
            pos->z = z25;
            if (is16)
                vel->z = 0.0f;
        }

        if (is15) {
            unsigned char *a = (unsigned char *)&((unsigned *)p->age)[i];

            ((unsigned *)p->age)[i] = colorWord;
            a[2] = ((seedNext() >> 9) & 3) | ((colorWord >> 16) & 0xFC);
            if (sizeRange)
                a[3] = (int)((255.0f - alphaMin) * (rand12() - 1.0f) + alphaMin);
            else
                a[3] = 0xFF;
        } else if (is16) {
            wakeEmit(p, pos, vel, preAge, sizeRange, minFrac, i);
            continue;
        } else if (sizeRange) {
            pos->w = (p->info.maxSize - p->info.minSize) * (rand12() - 1.0f) + p->info.minSize;
        } else if (is8) {
            WI(pos) = randBits12();
        } else if (is13) {
            int b = floatToBits(bitsToFloat((int)(((seedNext() >> 9) & 0x7FFFFF) | 0x40800000)) - 6.0f) & ~0xF;

            WI(pos) = b | ((seedNext() >> 16) & 0xF);
        } else if (is9) {
            WI(pos) = (seedNext() >> 9) & 1;
        } else {
            pos->w = p->info.minSize;
        }
        if (i == 0)
            vu0BBoxInit(pos);
        else
            vu0BBoxAdd(pos);
        if (is15 || is9) {
            if (preAge) {
                vel->w = p->info.recipLifetime / (1.0f - (rand12() - 1.0f) * e->info.preAgeFactor);
                if (0.5f < vel->w)
                    vel->w = 0.5f;
            } else {
                vel->w = p->info.recipLifetime;
            }
        } else {
            if (preAge)
                vel->w = vel->w * (rand12() - 1.0f);
            p->age[i] = (int)vel->w;
            if (is6)
                vel->w = vel->w * colorRate;
        }
        if (is3) {
            int w = WI(pos);

            WI(pos) = ((int)(seedNext() >> 16) & mask) | (w & ~mask);
        } else {
            p->uvI[i] = mask & (seedNext() >> 16);
        }
        if (p->field.attribs & 0x2000) {
            p->uvI[i] = attractors[e->info.sequence & 7];
            e->info.sequence++;
        }
    }
    if (rot)
        e->StaticRotAxis = axis;
    mathfVectorMinMaxGetMac(bbMin, bbMax);
done:
    e->info.onsetDelay = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleEmit__FP13_ParticleTypeP8_fvectorT1);
#endif
/* Scales flame effect 44 (rate, size, speed, offsets) by s, capping the rate at 256 live particles. */
void utilScaleFlame(ParticleType *p, float s)
{
    float a = s * 0.2f + 0.8f;
    float b = s * 0.6f + 0.4f;

    p->emit.info.minVel = EmitterInfo[44].minVel * a;
    p->emit.info.rate = EmitterInfo[44].rate * b;
    p->info.minSize = ParticleInfo[44].minSize * b;
    p->emit.info.minOffset = EmitterInfo[44].minOffset * b;
    p->emit.info.maxOffset = EmitterInfo[44].maxOffset * b;
    if (256.0f < p->emit.info.rate * p->info.lifetime)
        p->emit.info.rate = p->info.recipLifetime * 256.0f;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 17/223 words, size 0x350 vs 0x37c */
/* Moves the emitter of a free-flying effect (StaticVec/StaticVel), scaling flames 44 and 18 as they burn out; slaves die with their master. */
void emitterMove(ParticleType *p)
{
    EmitterType *e = &p->emit;
    int fields, k;
    FVECTOR sum, t, out;
    float s, h;

    if (e->info.onsetDelay >= 2)
        return;
    fields = timerGetFieldsLastFrame();
    if (e->fxId == 44) {
        s = eeSqrtf(1.0f - e->cumulativeTime);
        utilScaleFlame(p, s * p->field.flameSize);
    } else if (e->fxId == 18) {
        s = eeSqrtf(1.0f - e->cumulativeTime) * p->field.flameSize * 0.7f + 0.3f;
        e->info.minVel = EmitterInfo[18].minVel * s;
        e->info.rate = EmitterInfo[18].rate * s;
        e->StaticVec = *p->master->emit.info.Vec;
        e->StaticVec.z -= 2.0f;
    }
    if (p->master) {
        if (p->master->emit.info.duration > 0)
            return;
        e->info.duration = 0;
        e->info.attribs = (e->info.attribs | 2) & ~0x1000;
        return;
    }
    for (k = 0; k < fields; k++) {
        if (e->info.attribs & 0x8000) {
            float l2 = e->StaticVel.x * e->StaticVel.x + e->StaticVel.y * e->StaticVel.y + e->StaticVel.z * e->StaticVel.z;
            float f = 1.0f / (l2 * 0.01f + 1.0f);

            e->StaticVel.w = l2;
            if (f < 0.875f)
                f = 0.875f;
            e->StaticVel.x *= f;
            e->StaticVel.y *= f;
            e->StaticVel.z *= f;
        }
        if (e->info.attribs & 0x4000)
            e->StaticVel.z -= 0.03f;
        if (k)
            vecAdd(&sum, &sum, &e->StaticVel);
        else
            sum = e->StaticVel;
    }
    if (e->info.attribs & 0x4000) {
        e->StaticVec.x += sum.x;
        e->StaticVec.y += sum.y;
        e->StaticVec.z += sum.z;
    }
    if (e->info.attribs & 0x30000) {
        vecSub(&t, &e->StaticVec, &FireOffset);
        t.z += 5.0f;
        if ((count_006F88E0 & 7) == (fxIndex(p) & 7)) {
            h = hdHatTest(&t, &out, 50.0f, false, 4096.0f) - 5.0f;
            if (h <= 0.0f) {
                e->StaticVel.z = 0.0f;
                e->StaticVel.x = 0.0f;
                e->StaticVel.y = 0.0f;
                e->StaticVec.z -= h;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", emitterMove__FP13_ParticleType);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/89 words, size 0x164 vs 0x134 */
/* An electric arc (vis 17): emit attribs reuse the offset/segment fields as the arc parameters that drift each frame. */
void arcMove(ParticleType *p)
{
    float cum = p->emit.cumulativeRate + p->emit.info.rate;
    int n = (int)cum;
    float size = p->info.minSize;
    float grow = p->emit.info.maxVel;
    int i;

    p->emit.cumulativeRate = cum;
    p->newParticles = n;
    if ((short)n != 0) {
        p->emit.info.CLUToffset = 0.0f;
        p->emit.info.minOffset = p->emit.info.maxOffset;
        p->info.segmentInitU = p->field.orbit1;
        p->info.segmentDeltaU = p->field.orbit2;
        p->emit.info.randomSeed = (unsigned)seedNext();
        p->emit.cumulativeRate = 0.0f;
    }
    if (p->emit.info.minVel != 0.0f) {
        for (i = 0; i < p->numParticles; i++)
            vecAdd(&p->pos[i], &p->pos[i], &p->vel[i]);
    }
    p->info.minSize = size + grow;
    p->emit.info.minOffset += p->info.initExpansionRate;
    p->info.segmentInitU += p->field.lightInitIntensity;
    p->info.segmentDeltaU += p->field.lightMidIntensity;
    p->emit.info.CLUToffset += p->field.lightMidInterval;
    electricArcPacketUpdate(p, p->info.minSize, p->emit.info.minOffset, p->info.segmentInitU, p->info.segmentDeltaU,
                            p->emit.info.CLUToffset, 0, p->emit.info.randomSeed);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", arcMove__FP13_ParticleType);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/145 words, size 0x244 vs 0x19c */
/* Dust (vis 15): integrate, drop the ones whose w (age, 0..1) reached 1 by compacting the arrays, then drag
   v *= 1 - k + k / min(1 + air * (|v| or |v|^2), 2) with k = min(age / dragPhaseIn, 1). age holds the 32-bit colors here. */
void dustMove(ParticleType *p, _fvector *bbMin, _fvector *bbMax, _fvector *accel, float dt)
{
    int sqrtDrag = (p->field.attribs >> 5) & 1;
    int first = 1;
    _fvector *src = p->pos, *sv = p->vel, *dst = p->pos, *dv = p->vel;
    unsigned *sa = (unsigned *)p->age, *da = (unsigned *)p->age;
    float phase = p->field.dragPhaseIn ? p->field.dragPhaseIn * p->info.recipLifetime : p->info.recipLifetime;
    float rPhase = 1.0f / phase;
    float air = p->field.airResistance * dt;
    int n;

    for (n = p->numParticles; n > 0; n--, src++, sv++, sa++) {
        float l2 = sv->x * sv->x + sv->y * sv->y + sv->z * sv->z;
        float ramp, den, f;
        FVECTOR np;

        np.x = accel->x + src->x + sv->x * dt;
        np.y = accel->y + src->y + sv->y * dt;
        np.z = accel->z + src->z + sv->z * dt;
        np.w = src->w + sv->w * dt;
        *dst = np;
        if (1.0f <= dst->w) {
            p->numParticles--;
            continue;
        }
        if (src != dst)
            *da = *sa;
        ramp = np.w * rPhase;
        den = 1.0f + air * (sqrtDrag ? eeSqrtf(l2) : l2);
        if (ramp > 1.0f)
            ramp = 1.0f;
        if (den > 2.0f)
            den = 2.0f;
        f = 1.0f - ramp + ramp / den;
        if (first) {
            vu0BBoxInit(&np);
            first = 0;
        } else {
            vu0BBoxAdd(&np);
        }
        dv->x = sv->x * f;
        dv->y = sv->y * f;
        dv->z = sv->z * f;
        dv->w = sv->w;
        dst++;
        da++;
        dv++;
    }
    mathfVectorMinMaxGetMac(bbMin, bbMax);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", dustMove__FP13_ParticleTypeP8_fvectorN21f);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/130 words, size 0x208 vs 0x194 */
/* Wakes and kites (vis 16 / 9): like dustMove but a linear drag max(1 - age * air / phase, 1 - air), and the low `mask` bits of w
   (flags) survive the integration. Emit attrib 0x8000000 pins z to the parent's w (water level). */
void wakeAndKiteMove(ParticleType *p, _fvector *bbMin, _fvector *bbMax, _fvector *accel, float dt, unsigned mask)
{
    int first = 1;
    int pinZ = 0;
    _fvector *src = p->pos, *sv = p->vel, *dst = p->pos, *dv = p->vel;
    float phase, zLevel, air, lo, k;
    int n;

    if ((p->emit.info.attribs & 0x8000000) && p->emit.parentVec->w != 0.0f)
        pinZ = 1;
    phase = p->field.dragPhaseIn ? p->field.dragPhaseIn * p->info.recipLifetime : p->info.recipLifetime;
    air = p->field.airResistance;
    lo = 1.0f - air;
    k = air * (1.0f / phase);
    if (pinZ)
        zLevel = p->emit.parentVec->w;
    for (n = p->numParticles; n > 0; n--, src++, sv++) {
        unsigned keep = WI(src) & mask;
        float f;
        FVECTOR np;

        np.x = accel->x + src->x + sv->x * dt;
        np.y = accel->y + src->y + sv->y * dt;
        np.z = accel->z + src->z + sv->z * dt;
        np.w = src->w + sv->w * dt;
        if (pinZ)
            np.z = zLevel;
        f = 1.0f - np.w * k;
        *dst = np;
        if (1.0f <= dst->w) {
            p->numParticles--;
            continue;
        }
        if (f < lo)
            f = lo;
        WI(dst) = (WI(dst) & ~mask) | keep;
        dv->x = sv->x * f;
        dv->y = sv->y * f;
        dv->z = sv->z * f;
        dv->w = sv->w;
        if (first) {
            vu0BBoxInit(&np);
            first = 0;
        } else {
            vu0BBoxAdd(&np);
        }
        dst++;
        dv++;
    }
    mathfVectorMinMaxGetMac(bbMin, bbMax);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", wakeAndKiteMove__FP13_ParticleTypeP8_fvectorN21fUi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/1023 words, size 0xffc vs 0xcf0 */
/* Per-effect update flags of particleMove (retail packs them in one register). */
#define MV_ZPARENT 0x1    /* info attrib 0x800 + parentVec: falling particles age faster near the parent's w */
#define MV_COLORS 0x2     /* vis 1/6: vel.w advances the color index */
#define MV_DRAG 0x4       /* field.airResistance > 0 */
#define MV_DRAGSQRT 0x8   /* field attrib 0x20: drag on |v| instead of |v|^2 */
#define MV_BOX 0x10       /* field attrib 0x200: pulled toward the center, clamped per axis */
#define MV_ORBIT 0x20     /* field attrib 0x80: orbits the vertical axis through the center */
#define MV_EXPAND 0x40    /* expansion rates set (not vis 8/9) */
#define MV_RANDSIZE 0x80  /* info attrib 0x10 */
#define MV_FIRST 0x100    /* bounding box not started */
#define MV_KEEPLOW 0x200  /* vis 3: the low 4 bits of w are flags */
#define MV_ATTRACT 0x400  /* field attrib 0x2000: each particle is pulled by two neighbours (uvI nibbles) */

void particleMove(void)
{
    int next = fxActiveTop;
    int fields = timerGetFieldsLastFrame();
    float fF = fields;
    FVECTOR bbMin, bbMax;

    while (next != -1) {
        ParticleType *p = &fx[next];
        FieldInfoType *f = &p->field;
        int n, i, dIdx;
        unsigned flags;
        float colorStep;
        FVECTOR accel, center, v25, v24;
        float lifeF, zParent, airF, dragF, gravF;
        float e0, e1, e3;
        float falloff;

        if (next != -1)
            next = fxList[next].prev;
        if (p->vis.attribs == 12) {
            _fvector *q = p->pos;

            vu0BBoxInit(q);
            for (i = 1; i < p->numParticles; i++)
                vu0BBoxAdd(++q);
            mathfVectorMinMaxGetMac(&bbMin, &bbMax);
            p->FOVsphere.x = bbMin.x * 0.5f + bbMax.x * 0.5f;
            p->FOVsphere.y = bbMin.y * 0.5f + bbMax.y * 0.5f;
            p->FOVsphere.z = bbMin.z * 0.5f + bbMax.z * 0.5f;
            p->FOVextent.x = bbMax.x * 0.5f - bbMin.x * 0.5f;
            p->FOVextent.y = bbMax.y * 0.5f - bbMin.y * 0.5f;
            p->FOVextent.z = bbMax.z * 0.5f - bbMin.z * 0.5f;
            continue;
        }
        if (f->attribs & 0x1000)
            continue;
        if (p->vis.attribs == 7 || p->vis.attribs == 17) {
            if (p->emit.info.duration <= 0) {
                particleDestroyEffect(p);
                continue;
            }
            p->emit.info.duration -= fields;
            if (p->vis.attribs == 17)
                arcMove(p);
            continue;
        }
        if (p->emit.info.onsetDelay >= 2) {
            p->emit.info.onsetDelay--;
            continue;
        }
        p->emit.cumulativeTime += p->emit.deltaTime * fF;
        if (p->emit.info.attribs & 0x1000)
            emitterMove(p);
        if (p->emit.fxId == 0x91)
            pedSplatEmit(p);
        n = p->numParticles;
        p->newParticles = 0;
        if (n == 0)
            goto emit;

        flags = MV_FIRST;
        if ((p->info.attribs & 0x800) && p->emit.parentVec)
            flags |= MV_ZPARENT;
        if (p->vis.attribs == 1 || p->vis.attribs == 6)
            flags |= MV_COLORS;
        if (0.0f < f->airResistance)
            flags |= MV_DRAG;
        if ((f->attribs >> 5) & 1)
            flags |= MV_DRAGSQRT;
        if ((f->attribs >> 9) & 1)
            flags |= MV_BOX;
        if ((f->attribs >> 7) & 1)
            flags |= MV_ORBIT;
        if ((p->info.initExpansionRate != 0.0f || p->info.expansionRate != 0.0f) && !(p->vis.attribs - 8 < 2))
            flags |= MV_EXPAND;
        if ((p->info.attribs >> 4) & 1)
            flags |= MV_RANDSIZE;
        if (p->vis.attribs == 3)
            flags |= MV_KEEPLOW;
        if ((f->attribs >> 13) & 1)
            flags |= MV_ATTRACT;
        colorStep = fF;
        if (p->vis.attribs == 6)
            colorStep = fF * (float)(p->vis.numColors - 1) * p->info.recipLifetime;

        accel.x = 0.0f;
        accel.y = 0.0f;
        accel.z = 0.0f;
        accel.w = 0.0f;
        if (f->attribs & 0x400) {
            accel.x = f->wind.x + Gusts.x * f->wind.w;
            accel.y = f->wind.y + Gusts.y * f->wind.w;
            accel.z = f->wind.z + Gusts.z * f->wind.w;
        } else {
            accel.z = f->wind.z;
        }
        vecScale(&accel, &accel, fF);
        if (p->vis.attribs == 15) {
            dustMove(p, &bbMin, &bbMax, &accel, fF);
            goto emit;
        }
        if (p->vis.attribs == 9) {
            wakeAndKiteMove(p, &bbMin, &bbMax, &accel, fF, 1);
            goto emit;
        }
        if (p->vis.attribs == 16) {
            wakeAndKiteMove(p, &bbMin, &bbMax, &accel, fF, 0xFFF);
            goto emit;
        }

        lifeF = p->info.lifetime;
        if (flags & MV_ZPARENT)
            zParent = p->emit.parentVec->w;
        airF = f->airResistance * fF;
        dragF = f->dragPhaseInFactor;
        gravF = f->gravity * fF;
        if (flags & (MV_ATTRACT | MV_ORBIT | MV_BOX)) {
            if (!(flags & MV_ORBIT) && p->emit.parentVec)
                center = *p->emit.parentVec;
            else
                center = p->FOVsphere;
            falloff = f->ageFallOff;
            if (flags & MV_ATTRACT) {
                float m2 = p->info.minSize * p->info.minSize;

                v25.y = f->orbit1 * fF;
                v25.w = m2;
                v25.z = -f->orbit2 * fF;
                v25.x = v25.y / m2;
            } else {
                v25.z = f->orbit2 * fF;
                v25.x = f->orbit1 * fF;
                v25.y = v25.x;
                v25.w = f->orbit2 * 25.0f * fF;
                v24.x = -v25.x;
                v24.y = -v25.y;
                v24.z = -v25.z;
            }
        }
        if (flags & MV_EXPAND) {
            e0 = p->info.initExpansionRate * fF;
            e3 = (p->info.expansionRate - p->info.initExpansionRate) * fF;
            e1 = 1.0f / p->info.expansionRateTransition;
        } else if (flags & MV_RANDSIZE) {
            e3 = p->info.maxSize - p->info.minSize;
            e0 = p->info.minSize + p->info.minSize - p->info.maxSize;
        }

        dIdx = 0;
        for (i = 0; i < n; i++) {
            _fvector *P = &p->pos[i];
            _fvector *V = &p->vel[i];
            FVECTOR np, nv;
            float age = p->age[i];
            float l2 = V->x * V->x + V->y * V->y + V->z * V->z;
            float vlen, d;
            int w58 = WI(P);
            int iage;

            nv = *V;
            if ((flags & MV_ZPARENT) && V->z < 0.0f) {
                float x = (P->w - P->z + zParent) * (lifeF / P->w) - (age + fF);

                if (x < 0.0f)
                    x = 0.0f;
                if (x > 8.0f)
                    x = 8.0f;
                age = age + fF + x;
            } else {
                age = age + fF;
                if (flags & MV_COLORS)
                    nv.w += colorStep;
            }
            vlen = eeSqrtf(l2);
            iage = (int)age;
            d = age - lifeF;
            if (floatToBits(d) >= 0) {
                p->numParticles--;
                dIdx--;
                continue;
            }
            p->age[i + dIdx] = iage;
            if (flags & MV_DRAG) {
                float den = 1.0f + ((flags & MV_DRAGSQRT) ? airF * vlen : l2 * airF);
                float ramp = age * dragF;

                if (den > 2.0f)
                    den = 2.0f;
                if (iage < f->dragPhaseIn) {
                    float s = 1.0f - ramp + ramp / den;

                    nv.x *= s;
                    nv.y *= s;
                    nv.z *= s;
                } else {
                    float s = 1.0f / den;

                    nv.x *= s;
                    nv.y *= s;
                    nv.z *= s;
                }
            }
            np.x = accel.x + P->x + nv.x * fF;
            np.y = accel.y + P->y + nv.y * fF;
            np.z = accel.z + P->z + nv.z * fF;
            np.w = P->w;
            if (flags & (MV_ATTRACT | MV_ORBIT | MV_BOX)) {
                if (flags & MV_ATTRACT) {
                    unsigned b = p->uvI[i];
                    int off = (b & 0xF) - 8;
                    int k;

                    for (k = 1; k >= 0; k--) {
                        int j;

                        if (off < 0)
                            off += dIdx;
                        j = i + off;
                        if (off != 0 && j < n && j >= 0) {
                            FVECTOR dv;
                            float dl2, s;

                            vecSub(&dv, &P[off], &np);
                            dl2 = dv.x * dv.x + dv.y * dv.y + dv.z * dv.z;
                            s = (dl2 - v25.w) * v25.x;
                            if (s > v25.y)
                                s = v25.y;
                            if (s < v25.z)
                                s = v25.z;
                            s = s / eeSqrtf(dl2);
                            nv.x += dv.x * s;
                            nv.y += dv.y * s;
                            nv.z += dv.z * s;
                        }
                        off = (int)(b >> 4) - 8;
                    }
                } else if (flags & MV_BOX) {
                    FVECTOR dv;

                    vecSub(&dv, &center, &np);
                    nv.x += dv.x < v25.x ? (dv.x > v24.x ? dv.x : v24.x) : (v25.x > v24.x ? v25.x : v24.x);
                    nv.y += dv.y < v25.y ? (dv.y > v24.y ? dv.y : v24.y) : (v25.y > v24.y ? v25.y : v24.y);
                    nv.z += dv.z < v25.z ? (dv.z > v24.z ? dv.z : v24.z) : (v25.z > v24.z ? v25.z : v24.z);
                } else {
                    FVECTOR dv;
                    float rxy, s = 0.0f;

                    vecSub(&dv, &np, &center);
                    rxy = eeSqrtf(dv.x * dv.x + dv.y * dv.y);
                    if (0.0f < rxy)
                        s = dv.z * (v25.x / rxy);
                    nv.x += dv.x * s;
                    nv.y += dv.y * s;
                    nv.z += v25.w - v25.z * rxy;
                }
                v24.x *= falloff;
                v24.y *= falloff;
                v24.z *= falloff;
                v25.x *= falloff;
                v25.y *= falloff;
                v25.z *= falloff;
            }
            nv.z += gravF;
            if (flags & MV_EXPAND) {
                float t = age * e1;

                if (t > 1.0f)
                    t = 1.0f;
                np.w = np.w + e0 + e3 * t;
            } else if (flags & MV_RANDSIZE) {
                np.w = e0 + e3 * rand12();
            }
            if (flags & MV_FIRST) {
                vu0BBoxInit(&np);
                flags &= 0x6FF;
            } else {
                vu0BBoxAdd(&np);
            }
            p->pos[i + dIdx] = np;
            p->uvI[i + dIdx] = p->uvI[i];
            if (flags & MV_KEEPLOW)
                WI(&p->pos[i + dIdx]) = (WI(&p->pos[i + dIdx]) & ~0xF) | (w58 & 0xF);
            p->vel[i + dIdx] = nv;
        }
        mathfVectorMinMaxGetMac(&bbMin, &bbMax);
    emit:
        if (p->emit.fxId == 0x93)
            particleEmitBoundaryFx(p);
        else if (p->emit.fxId != 0x91 && (p->emit.info.attribs & 1) && !(p->emit.info.attribs & 0x80000))
            particleEmit(p, &bbMin, &bbMax);
        if (p->numParticles > 0) {
            if (p->info.maxRadius != 0.0f)
                bbMax.w = p->info.maxRadius;
            p->FOVsphere.x = bbMin.x * 0.5f + bbMax.x * 0.5f;
            p->FOVsphere.y = bbMin.y * 0.5f + bbMax.y * 0.5f;
            p->FOVsphere.z = bbMin.z * 0.5f + bbMax.z * 0.5f;
            p->FOVextent.x = bbMax.w + bbMax.x * 0.5f - bbMin.x * 0.5f;
            p->FOVextent.y = bbMax.w + bbMax.y * 0.5f - bbMin.y * 0.5f;
            p->FOVextent.z = bbMax.w + bbMax.z * 0.5f - bbMin.z * 0.5f;
        }
        if (p->vis.lightColor & 0x3000000) {
            unsigned lc = p->vis.lightColor;

            if ((lc & 0x10000000) && (p->emit.info.attribs & 0x4800000) == 0x800000) {
                p->info.lightIntensity = 0.0f;
            } else if (lc & 0x4000000) {
                if (f->lightMidInterval < p->emit.cumulativeTime)
                    p->vis.lightColor = lc & ~0x4000000;
                if ((rand() & 0xFF) < 0x80 && !(p->info.lightIntensity < f->lightInitIntensity)) {
                    if (f->lightInitIntensity + f->randWalkStepSize < p->info.lightIntensity)
                        p->info.lightIntensity = p->info.lightIntensity - f->randWalkStepSize;
                } else {
                    p->info.lightIntensity += f->randWalkStepSize;
                    if (f->lightMidIntensity < p->info.lightIntensity)
                        p->info.lightIntensity = f->lightMidIntensity;
                }
            } else {
                float t = p->emit.cumulativeTime;

                if (t <= f->lightMidInterval)
                    p->info.lightIntensity = f->lightInitIntensity + f->lightSlope1 * t;
                else
                    p->info.lightIntensity = f->lightMidIntensity + f->lightSlope2 * (t - f->lightMidInterval);
            }
            if (!(p->emit.info.attribs & 0x800000) && p->info.lightIntensity <= 0.0f) {
                p->vis.lightColor = 0;
                if (p->pLight >= 0)
                    p->pLight = -1;
            }
        }
        if (p->numParticles == 0 && !(p->emit.info.attribs & 1)) {
            p->vis.lightColor = 0;
            if (p->pLight != -1)
                p->pLight = -1;
            if (p->slave)
                p->slave->emit.info.attribs &= ~1;
            particleDestroyEffect(p);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleMove__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 60/426 words, size 0x6a8 vs 0x6a8 */
/* Instantiates effect `type` from the tables; scale != 1 grows it (lifetimes and speeds by 1 + (scale - 1) / 5, sizes by scale). */
int particleInitFx(int type, float scale)
{
    int h = fxAlloc();
    ParticleType *p;

    if (h == -1)
        return -1;
    p = &fx[h];
    p->emit.info = EmitterInfo[type];
    p->field = FieldInfo[type];
    p->info = ParticleInfo[type];
    p->vis = VisualInfo[type];
    if (scale != 1.0f) {
        float g = (scale - 1.0f) * 0.2f + 1.0f;
        int life, trans;

        p->field.dragPhaseIn = (int)(p->field.dragPhaseIn * g);
        if (p->vis.lightColor & 0x3000000) {
            float q = eeSqrtf(scale);

            p->info.lightIntensity *= q;
            p->field.lightMidIntensity *= q;
        }
        if (type == 1) {
            p->emit.info.minVel *= scale;
            p->emit.info.maxVel *= scale;
        } else {
            p->emit.info.minVel *= g;
            p->emit.info.maxVel *= g;
        }
        if (p->emit.info.attribs & 0x20)
            p->emit.info.duration = (int)(p->emit.info.duration * g);
        if (p->emit.info.attribs & 0x40)
            p->emit.info.rate /= g;
        else if (p->emit.info.attribs & 0x10)
            p->emit.info.rate *= g;
        life = (int)(p->info.lifetime * g);
        trans = (int)(p->info.expansionRateTransition * g);
        p->emit.info.minOffset *= scale;
        p->emit.info.maxOffset *= scale;
        p->info.minSize *= scale;
        p->info.maxSize *= scale;
        p->info.initExpansionRate *= g;
        p->info.expansionRate *= g;
        p->info.lifetime = life;
        p->info.expansionRateTransition = trans;
        p->info.recipLifetime = 1.0f / (short)life;
        particleFOVcalcMaxRadius(&p->info, p->vis.attribs);
        p->info.colorsPerFrame = ((p->vis.numColors - 1) << 12) / (unsigned)p->info.lifetime;
        if (p->info.attribs & 4)
            p->info.mapsPerFrame = (TextureInfo[p->vis.texInfoIndex].numTextures << 12) / p->info.texAnimLifetime;
        else
            p->info.mapsPerFrame = (TextureInfo[p->vis.texInfoIndex].numTextures << 12) / p->info.lifetime;
    }
    if (p->field.dragPhaseIn > 0)
        p->field.dragPhaseInFactor = 1.0f / p->field.dragPhaseIn;
    p->texLoad[0] = 0;
    p->TEX0ptr = 0;
    p->numParticles = 0;
    p->emit.parentVec = 0;
    p->emit.parentMat = 0;
    p->emit.particleSpaceVec = 0;
    p->emit.particleSpaceMat = 0;
    p->emit.cumulativeTime = 0.0f;
    if (p->emit.info.duration != 0)
        p->emit.deltaTime = 1.0f / (p->emit.info.duration + p->info.lifetime * p->emit.info.lifetimeToLightDuration);
    else
        p->emit.deltaTime = p->emit.cumulativeTime;
    p->emit.cumulativeRate = p->emit.info.rate < 1.0f ? 1.0f : 0.0f;
    p->emit.fxId = type;
    p->prevViewMat = 0;
    p->master = 0;
    p->slave = 0;
    p->backPointer = 0;
    p->pLight = 0;
    if (p->vis.lightColor & 0x3000000) {
        if (p->vis.lightColor & 0x4000000)
            p->info.lightIntensity = 1.0f;
        else
            p->field.lightSlope1 = (p->field.lightMidIntensity - p->field.lightInitIntensity) / p->field.lightMidInterval;
        p->field.lightSlope2 = -p->field.lightMidIntensity / p->field.lightEndInterval;
        if (p->vis.lightColor & 0x2000000)
            p->pLight = 0;
    }
    if (p->emit.info.duration < 2) {
        if (p->emit.info.duration <= 0)
            p->emit.info.attribs &= ~2;
        else
            p->emit.info.duration = 2;
    }
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleInitFx__Fif);
#endif
/* Builds the draw packet of the effect's visual type (inlined in particlePostInitFx and particleScrapeSparks). */
static inline void packetInit(ParticleType *p)
{
    switch (p->vis.attribs) {
    case 3:
        spritePacketInit(p);
        break;
    case 4:
        texSpritePacketInit(p);
        break;
    case 1:
        linePacketInit(p);
        break;
    case 15:
        dustTrailPacketInit(p);
        break;
    case 13:
        fragPacketInit(p);
        break;
    case 5:
        stripePacketInit(p);
        break;
    case 6:
        stritePacketInit(p);
        break;
    case 8:
        sprinePacketInit(p);
        break;
    case 9:
        kitePacketInit(p);
        break;
    case 16:
        wakePacketInit(p);
        break;
    }
}
void particlePostInitFx(_fvector *pos, ParticleType *p, float scale)
{
    p->master = 0;
    if (p->emit.fxId == 44)
        utilScaleFlame(p, scale);
    packetInit(p);
}
void initSelfAnimated(ParticleType *p, float spread, float (*m)[4], float speed)
{
    mathfRandVector(&p->emit.StaticVel, spread);
    if (m)
        mathfMulTransVecOld(m, &p->emit.StaticVel, &p->emit.StaticVel);
    vecScale(&p->emit.StaticVel, &p->emit.StaticVel, speed);
    p->emit.info.attribs |= 0x200;
}
void initRandomDirection(ParticleType *p, float spread, float (*m)[4])
{
    FVECTOR n;

    mathfRandVector(&n, spread);
    if (m)
        mathfMulTransVecOld(m, &n, &n);
    mathfMatrixFromNormal(p->emit.StaticMat, &n);
    p->emit.info.Mat = (FMATptr)p->emit.StaticMat;
    p->emit.info.attribs |= 0x100;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 45/97 words, size 0x184 vs 0x180 */
/* A free-standing effect at pos (+ the table offset); m orients it. zFudge != 0 overrides the draw depth bias. */
int particleCreateFx(_fvector *pos, int type, float scale, float (*m)[4], float zFudge)
{
    int h = particleInitFx(type, scale);
    ParticleType *p;

    if (h == -1)
        return -1;
    p = &fx[h];
    vecAdd(&p->emit.StaticVec, pos, EmitterInfo[type].Vec);
    p->emit.ParticleToEmitVec = p->emit.StaticVec;
    p->emit.prevParticleToEmitVec = p->emit.StaticVec;
    p->emit.info.Vec = &p->emit.StaticVec;
    if (p->emit.info.attribs & 0x1000) {
        initSelfAnimated(p, 0.1464f, m, scale * 3.0f);
    } else if (p->emit.info.attribs & 0x800) {
        initRandomDirection(p, 0.5f, m);
    } else if (m) {
        matCopy(p->emit.StaticMat, m);
        p->emit.info.Mat = (FMATptr)p->emit.StaticMat;
        p->emit.info.attribs |= 0x100;
    }
    if (zFudge != 0.0f)
        p->info.Zfudge = zFudge;
    particlePostInitFx(pos, p, scale);
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleCreateFx__FP8_fvectorifPA3_ff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 36/106 words, size 0x1a4 vs 0x1a8 */
/* An effect attached to a parent (pos/m are followed every frame). With `shared` the offset and orientation are
   referenced instead of copied. */
int particleCreateFx(_fvector *pos, float (*m)[4], int type, float scale, _fvector *offs, float (*om)[4], bool shared, float zFudge)
{
    int h = particleInitFx(type, scale);
    ParticleType *p;

    if (h == -1)
        return -1;
    p = &fx[h];
    p->emit.parentMat = (FMATptr)m;
    p->emit.info.Vec = &p->emit.StaticVec;
    p->emit.info.attribs |= 0x200;
    p->emit.parentVec = (FVECptr)pos;
    if (offs) {
        if (shared)
            p->emit.info.Vec = (FVECptr)offs;
        else
            p->emit.StaticVec = *offs;
    } else {
        p->emit.StaticVec = *EmitterInfo[type].Vec;
    }
    if (om) {
        if (shared) {
            p->emit.info.Mat = (FMATptr)om;
        } else {
            if (p->emit.info.attribs & 0x800) {
                FVECTOR n;

                mathfRandVector(&n, 0.5f);
                mathfMatrixFromNormal(p->emit.StaticMat, &n);
            } else {
                matCopy(p->emit.StaticMat, om);
            }
            p->emit.info.Mat = (FMATptr)p->emit.StaticMat;
        }
    }
    if (zFudge != 0.0f)
        p->info.Zfudge = zFudge;
    particlePostInitFx(pos, p, scale);
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleCreateFx__FP8_fvectorPA3_fifT0T1bf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 60/362 words, size 0x598 vs 0x5a8 */
/* An emitter placed in a level model: its fields override the table entry; auto-start emitters are remembered for replays. */
int particleCreateModeledFx(_fvector *pos, HierParticleEmitter *e)
{
    float scale = 1.0f;
    int h;
    ParticleType *p;
    FVECTOR n;
    float c;

    if (e->size != 0.0f)
        scale = e->size;
    h = particleInitFx(e->particleEnum, scale);
    if (h == -1)
        return -1;
    if (numAutoEmitters < 128)
        autoEmitters[numAutoEmitters++] = e;
    p = &fx[h];
    p->emit.info.duration = e->emissionDuration;
    if (0.0f < e->emitterStretch)
        p->emit.info.normalScale = e->emitterStretch;
    if (e->particleLifetime && (unsigned short)p->info.lifetime != e->particleLifetime) {
        short life = e->particleLifetime;

        p->info.lifetime = life;
        p->info.recipLifetime = 1.0f / life;
        p->info.colorsPerFrame = ((p->vis.numColors - 1) << 12) / (unsigned)life;
        if (p->info.attribs & 4)
            p->info.mapsPerFrame = (TextureInfo[p->vis.texInfoIndex].numTextures << 12) / p->info.texAnimLifetime;
        else
            p->info.mapsPerFrame = (TextureInfo[p->vis.texInfoIndex].numTextures << 12) / life;
    }
    if (e->gravity != 0.0f) {
        p->field.attribs |= 0x100;
        p->field.gravity = e->gravity * -0.008888889f;
    }
    if (0.0f < e->airResistance)
        p->field.airResistance = e->airResistance;
    if (e->airResistanceTransition) {
        p->field.dragPhaseIn = e->airResistanceTransition;
        if ((int)e->airResistanceTransition > 0)
            p->field.dragPhaseInFactor = 1.0f / (int)e->airResistanceTransition;
        else
            p->field.dragPhaseInFactor = 0.0f;
    }
    if (0.0f < e->initialExpansionRate)
        p->info.initExpansionRate = e->initialExpansionRate;
    if (e->expansionRate != 0.0f)
        p->info.expansionRate = e->expansionRate;
    if (e->expansionRateTransition)
        p->info.expansionRateTransition = e->expansionRateTransition;
    if (e->onsetDelay)
        p->emit.info.onsetDelay = e->onsetDelay;
    if (p->emit.info.duration > 0) {
        p->emit.info.attribs |= 2;
        p->emit.deltaTime = 1.0f / (p->emit.info.duration + p->info.lifetime * p->emit.info.lifetimeToLightDuration);
    } else {
        p->emit.deltaTime = 0.0f;
        p->emit.info.attribs &= ~2;
    }
    if (0.0f < e->velocity) {
        p->emit.info.minVel = e->velocity * 0.5f;
        p->emit.info.maxVel = e->velocity;
    }
    if (0.0f < e->emissionRate && p->vis.attribs != 8 && p->vis.attribs != 5) {
        p->emit.info.rate = e->emissionRate;
        p->emit.cumulativeRate = e->emissionRate < 1.0f ? 1.0f : 0.0f;
    }
    if (e->flags & 1) { /* isMoving */
        p->emit.parentVec = &e->cachedPosition;
        p->emit.parentMat = (FMATptr)e->cachedOrientation;
        p->emit.StaticVec = e->position;
        p->emit.info.attribs |= 0x800200;
    } else {
        p->emit.StaticVec = *pos;
        p->emit.info.attribs |= 0x100;
    }
    p->emit.ParticleToEmitVec = p->emit.StaticVec;
    p->emit.prevParticleToEmitVec = p->emit.StaticVec;
    p->emit.info.Vec = &p->emit.StaticVec;
    c = cosf(e->emissionPitch);
    n.x = c * sinf(e->emissionHeading);
    n.y = c * cosf(e->emissionHeading);
    n.z = sinf(e->emissionPitch);
    mathfMatrixFromNormal(p->emit.StaticMat, &n);
    p->emit.info.Mat = (FMATptr)p->emit.StaticMat;
    if (p->emit.info.attribs & 0x1000) {
        float a = (1.0f - cosf(e->spread * DEG2RAD)) * 0.5f;

        initSelfAnimated(p, a, p->emit.StaticMat, e->size != 0.0f ? e->size * 3.0f : 3.0f);
    } else if (p->emit.info.attribs & 0x800) {
        initRandomDirection(p, (1.0f - cosf(e->spread * DEG2RAD)) * 0.5f, p->emit.StaticMat);
    } else if (0.0f < e->spread) {
        p->emit.info.spread = (1.0f - cosf(e->spread * DEG2RAD)) * 0.5f;
    }
    particlePostInitFx(pos, p, e->size != 0.0f ? e->size : 1.0f);
    if (e->particleEnum == 13 || e->particleEnum == 43 || e->particleEnum == 44)
        p->info.attribs |= 0x8000;
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleCreateModeledFx__FP8_fvectorP19HierParticleEmitter);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 32/202 words, size 0x328 vs 0x318 */
/* Sparks where something scrapes along a surface: a is the motion, at is the contact point, n the surface normal. */
int particleScrapeSparks(_fvector *a, _fvector *at, _fvector *n)
{
    float d = a->x * n->x + a->y * n->y + a->z * n->z;
    FVECTOR proj, tang, dir, t;
    float len, l, f, k;
    int h;
    ParticleType *p;

    vecScale(&proj, n, d);
    vecSub(&tang, &proj, a);
    t.x = tang.x - proj.x * 0.5f;
    t.y = tang.y - proj.y * 0.5f;
    t.z = tang.z - proj.z * 0.5f;
    len = eeSqrtf(t.x * t.x + t.y * t.y + t.z * t.z);
    k = 1.0f / len;
    dir.z = t.z * k;
    dir.x = t.x * k;
    dir.y = t.y * k;
    if (len < 0.1f || !(0.0f < d))
        return -1;
    h = particleInitFx(0x16, 1.0f);
    if (h == -1)
        return -1;
    p = &fx[h];
    mathfMatrixFromNormal(p->emit.StaticMat, &dir);
    p->emit.ParticleToEmitVec = *at;
    vecSub(&p->emit.StaticVec, at, &tang);
    p->emit.info.Vec = &p->emit.StaticVec;
    p->emit.info.Mat = (FMATptr)p->emit.StaticMat;
    l = eeSqrtf(tang.x * tang.x + tang.y * tang.y + tang.z * tang.z);
    f = -d * l;
    p->emit.info.minOffset = len * 0.125f;
    p->emit.info.maxOffset = len;
    p->emit.info.duration = 2;
    p->emit.parentVec = &NoOffset;
    p->emit.info.rate = f * 4.0f + 2.0f;
    p->info.lifetime = (int)(f * 3.0f) + 3;
    p->emit.parentMat = (FMATptr)Up;
    p->emit.info.attribs |= 0x200;
    p->emit.info.minVel = len * 0.125f;
    p->emit.info.maxVel = len;
    p->emit.info.onsetDelay = 0;
    packetInit(p);
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleScrapeSparks__FP8_fvectorN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/9 words, size 0x24 vs 0x24 */
ParticleType *particleGetParticle(int h)
{
    if (h > 0)
        return &fx[h];
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleGetParticle__Fi);
#endif
/* Stops emitting (the effect dies when its particles do) and invalidates the handle. */
void particleKillFx(int &h)
{
    if (h > 0) {
        ParticleType *p = &fx[h];

        p->emit.info.attribs |= 2;
        p->emit.info.duration = 0;
        if (p->pLight)
            p->pLight = 0;
    }
    h = -1;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 44/50 words, size 0xc8 vs 0xc0 */
void particleKillAllFx(void)
{
    int h = fxActiveTop;

    while (h != -1) {
        ParticleType *p = &fx[h];

        if (p->pLight)
            p->pLight = 0;
        if (h != -1)
            h = fxList[h].prev;
        particleDestroyEffect(p);
    }
    count_006F88E0 = 0;
    texIndex = 0;
    lensFlarePacket[0] = 0;
    lensFlarePacket[1] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleKillAllFx__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 11/74 words, size 0x108 vs 0x128 */
/* Moves the pool entry from the active to the inactive list and clears the owner's handle. */
void particleDestroyEffect(ParticleType *p)
{
    int idx = fxIndex(p);

    if ((unsigned)idx >= 100)
        return;
    if (idx != -1 && fxList[idx].used) {
        _ilnklist *e = &fxList[idx];

        if (e->next != -1)
            fxList[e->next].prev = e->prev;
        else
            fxActiveTop = e->prev;
        if (e->prev != -1)
            fxList[e->prev].next = e->next;
        if (fxInactiveTop != -1)
            fxList[fxInactiveTop].next = idx;
        e->prev = fxInactiveTop;
        e->next = -1;
        e->used = 0;
        fxInactiveTop = idx;
    }
    disconnectBackPointer(p);
    p->vis.attribs = 0;
    p->info.attribs = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleDestroyEffect__FP13_ParticleType);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 31/94 words, size 0x164 vs 0x178 */
/* Retail starts at the entry after the top (the top itself is not destroyed here; the list is rebuilt below anyway). */
void particleInitForReplay(void)
{
    int h = fxActiveTop;
    int n;
    HierParticleEmitter **pe;

    while (h != -1) {
        h = fxList[h].prev;
        particleDestroyEffect(&fx[h]);
    }
    ilnkListFill(fxList, 100, &fxInactiveTop, false);
    fxList[1].prev = -1;
    fxActiveTop = -1;
    particleInitAfter();
    n = numAutoEmitters;
    numAutoEmitters = 0;
    for (pe = autoEmitters; n > 0; n--, pe++) {
        HierParticleEmitter *e = *pe;

        if (!e)
            continue;
        if (e->flags & 8) { /* isAutoStart; word ops, not bitfields (a 64-bit mask snfix cannot expand) */
            hierRegisterLocator(e->head.id1, particleCreateModeledFx(&e->position, e));
            e->flags |= 0x10;
        } else {
            e->flags &= ~0x10;
            hierRegisterLocator(e->head.id1, -1);
            *pe = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleInitForReplay__Fv);
#endif
void particleUpdatePickupFX(_cs *cs, int i)
{
}
#ifdef NON_MATCHING
/* equivalent (untuned): 57/61 words, size 0xf4 vs 0xf4 */
/* count fragments of effect `type` flying off in random directions (biased upward: y and z of the random vector swapped). */
void particleCreateFlyingFrags(_fvector *pos, float speed, int count, int type)
{
    int i, h;

    for (i = 0; i < count; i++) {
        ParticleType *p;
        FVECTOR v;
        float t;

        h = particleCreateFx(pos, type, 1.0f, 0, 0.0f);
        if (h == -1)
            break;
        p = particleGetParticle(h);
        mathfRandVector(&v, 0.4f);
        t = v.y;
        v.y = v.z;
        v.z = t;
        mathfMatrixFromNormal(p->emit.StaticMat, &v);
        vecScale(&p->emit.StaticVel, &v, speed);
        p->emit.info.attribs |= 0x200;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleCreateFlyingFrags__FP8_fvectorfii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 39/74 words, size 0x120 vs 0x128 */
/* A flame (effect 44) that burns out over `duration` fields; without a surface normal it falls and sticks to the ground. */
int particleCreateFadingFlame(_fvector *pos, int duration, _fvector *normal, float size)
{
    FMATRIX m;
    int h;
    ParticleType *p;

    if (normal) {
        mathfMatrixFromNormal(m, normal);
        h = particleCreateFx(pos, 44, size, m, 0.0f);
    } else {
        h = particleCreateFx(pos, 44, size, 0, 0.0f);
    }
    if (h == -1)
        return -1;
    p = particleGetParticle(h);
    if (!normal)
        p->emit.info.attribs |= 0x14000;
    p->emit.info.duration = duration;
    if ((short)duration)
        p->emit.deltaTime = 1.0f / ((short)duration + p->info.lifetime * p->emit.info.lifetimeToLightDuration);
    else
        p->emit.deltaTime = 0.0f;
    ((int *)&p->emit.StaticVel)[0] = 0;
    ((int *)&p->emit.StaticVel)[2] = 0;
    ((int *)&p->emit.StaticVel)[1] = 0;
    p->field.flameSize = size;
    return h;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleCreateFadingFlame__FP8_fvectoriT0f);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/107 words, size 0x1ac vs 0x19c */
void particlePrepDraw(void)
{
    int h;

    count_006F88E0++;
    numActiveFires = 0;
    particleMove();
    h = fxActiveTop;
    while (h != -1) {
        ParticleType *p = &fx[h];

        if (h != -1)
            h = fxList[h].prev;
        if (p->numParticles) {
            switch (p->vis.attribs) {
            case 3:
                spritePacketBuild(p);
                break;
            case 4:
                texSpritePacketBuild(p);
                break;
            case 1:
                linePacketBuild(p);
                break;
            case 15:
                dustTrailPacketBuild(p);
                break;
            case 13:
                fragPacketBuild(p);
                break;
            case 6:
                stritePacketBuild(p);
                break;
            case 8:
                sprinePacketBuild(p);
                break;
            case 9:
                kitePacketBuild(p);
                break;
            case 5:
                stripePacketBuild(p);
                break;
            case 2:
                sweepTrailPacketBuild(p);
                break;
            case 16:
                wakePacketBuild(p);
                break;
            }
        }
        if ((p->info.attribs & 0x8000) && p->visibility)
            activeFires[numActiveFires++] = p->emit.ParticleToEmitVec;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particlePrepDraw__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/88 words, size 0x13c vs 0x160 */
/* Wind gusts: a slow product of sines over the frame counter (x and y only). */
void particleClearBuffers(void)
{
    float t = count_006F88E0 & 0x7FFF;
    const float k = 0.0001917476f;

    Gusts.x = sinf(t * 13.0f * k) * cosf(t * 15.0f * k) * sinf(t * 19.0f * k) * 0.2f + 0.05f;
    Gusts.y = cosf(t * 14.0f * k) * cosf(t * 17.0f * k) * sinf(t * 23.0f * k) * 0.2f + 0.05f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/particle", particleClearBuffers__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/common/particle", particleDraw__Fi);
