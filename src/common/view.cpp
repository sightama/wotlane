#include "common.h"
#include "hieri_types.h"
#include "cs_pool.h"
#include "vecmath.h"

/* Views (cameras). Up to 5 views (split screen, rear view, shadow view): viewInfo[i] points at the camera CS and at
 * a viewport definition (viewDef[viewport]: screen distance, size, offsets, frustum planes in camera space). Every
 * frame viewUpdate turns the camera into the world context (_worldctx: world->eye matrix, frustum planes in world
 * space for the SIMD in-FOV test) and the world->screen / clip matrices, and blends the ambient light. viewDb[i] is
 * the GS double buffer of the view (IncognitoEntertainmentGS), whose clear color and scissor are set here. */

typedef unsigned long u_long;

enum _viewports { VIEWPORT_FULL = 0, VIEWPORT_TOP = 1, VIEWPORT_BOTTOM = 2, VIEWPORT_SHADOW = 6 };

/* viewDef[_viewports], 0xC0 bytes */
struct ViewDef {
    float fov;                 // 0x00 screen distance (the "fov" the game tweaks)
    unsigned short w, h;       // 0x04 viewport size
    short fbw, fbh;            // 0x08 frame buffer size for the double buffer
    unsigned short x, y;       // 0x0C scissor origin
    float offX, offY;          // 0x10 GS offset of the viewport centre
    int cx, cy;                // 0x18 centre in pixels
    float aspect;              // 0x20
    int pad24[3];              // 0x24
    _fvector planes[2][4];     // 0x30 frustum plane normals in camera space (transposed by viewSetFov)
    float degrees;             // 0xB0 horizontal field of view in degrees
    int padB4[3];              // 0xB4
};
typedef char _size_ViewDef[sizeof(ViewDef) == 0xC0 ? 1 : -1];

struct ViewInfo {
    _cs *cs;                   // 0x00 camera
    ViewDef *def;              // 0x04
    int viewport;              // 0x08 _viewports
    int prevViewport;          // 0x0C
    _cs *ownCs;                // 0x10 the view's own camera (cs can be borrowed, see viewGrappleConfig)
};
typedef char _size_ViewInfo[sizeof(ViewInfo) == 0x14 ? 1 : -1];

struct _todInfo14;
class IncognitoEntertainmentGS {
public:
    struct ieGsDBuffDc;
    static void ieGsInitAA(bool on);
    static void ieGsSetDefDBuffDc(ieGsDBuffDc *db, short interlace, short w, short h, short ztest, short zpsm, short clear);
};
enum { VIEW_DB_SIZE = 0x360, VIEW_DB_CLEAR0 = 0x1B0, VIEW_DB_CLEAR1 = 0x320,
       VIEW_DB_SCISSOR0 = 0xC0, VIEW_DB_SCISSOR1 = 0x140, VIEW_DB_SCISSOR2 = 0x230, VIEW_DB_SCISSOR3 = 0x2B0 };

extern ViewInfo viewInfo[5];
extern ViewDef viewDef[];
extern _worldctx worldCtx[5];
extern unsigned char viewDb[][VIEW_DB_SIZE];
extern float worldToScreenMat[][4][4];
extern float viewScreenMats[][4][4];
extern float viewClipMats[][4][4];
extern float viewFovMats[][4][4];
extern float viewClipView[][4][4];
extern float viewFovView[][4][4];
extern float viewShadWorldToScrMat[4][4];
extern float viewShadRecMat[4][4];
extern int viewCurView;
extern int viewNumViews;
extern int gUseUnifiedView;
extern int viewRearLargeActive;
extern int viewRearViewConfig;
extern int viewAmbientChanged;
extern float viewAmbientRed, viewAmbientGreen, viewAmbientBlue;
extern float viewAnimAmbientRed, viewAnimAmbientGreen, viewAnimAmbientBlue;
extern float viewTODAmbientRed, viewTODAmbientGreen, viewTODAmbientBlue;
extern float s_viewAmbientVolBlend[], s_viewAmbientVolRed[], s_viewAmbientVolGreen[], s_viewAmbientVolBlue[];
extern float viewTimeOfDay;
extern float viewDegrees;
extern float viewFovH;
extern int viewBgR, viewBgG, viewBgB, viewBgA;
extern int viewTODBgR, viewTODBgG, viewTODBgB, viewTODBgA;
extern int viewOddScan __asm__("D_006F8DF0");
/* VU1 packet data the renderer sends every frame (in the hier/vu packet area) */
extern float vuWorldToScreen[4][4] __asm__("D_0025B1A8");
extern float vuClipView[4][4] __asm__("D_0025B1F0");
extern float vuFovView[4][4] __asm__("D_0025B298");
extern float vuWorldToScreen2[4][4] __asm__("D_0025B798");
extern float vuClipView2[4][4] __asm__("D_0025B7E0");
extern float vuLightParams[] __asm__("D_0025B238");       // [12..14] blended ambient
extern float vuScreenToWorld[4][4] __asm__("D_00256B90");
extern float vuScreenHalf[2] __asm__("D_00256BE0");
extern float vuShadowAmbient[3] __asm__("D_00256BD0");

LightEnv *lightGetEnv(int i);
float fogGetFarClipRange(void);
int todActive(void);
void todInit(void);
void todGetTOD(int *, float *, _todInfo14 *);
void todSetTOD(int, float, void *, float);
void todSetSkyEntry(HierHead *node);
float todGetCurrentTOD(void);
void todGetAmbient(float *r, float *g, float *b);
void todUpdate(int view);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
void mathfMulMatrix(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfTransposeMatrix(float (*dst)[4], float (*src)[4]);
void mathfViewScreenMatrix(float (*m)[4], float (*clip)[4], float (*clipGuard)[4], float scrz, float ax, float ay,
                           float cx, float cy, float zmin, float zmax, float nearz, float farz, float gx, float gy);
void vu0MulMatrixTP3x3(float (*dst)[4], float (*a)[4], float (*b)[4]);
extern "C" float atanf(float);
void viewStoreNorms1InVu0(void);
void viewStoreNorms2InVu0(void);
_fvector *viewGetTrans(int view);
void viewApplySwap(float (*dst)[4], float (*src)[4]);
void viewSetVUPacketMat(float (*a)[4], float (*b)[4], float (*c)[4], float (*d)[4], int view);
void viewSetBgColor(unsigned long r, unsigned long g, unsigned long b, unsigned long a);
void viewGetBgColor(int &r, int &g, int &b, int &a);
int viewInFOV(int view, _fvector *center, _fvector *extent);
ViewDef *viewGetDef(int view);
void viewSetFov(int view, float fov, float aspect);
void viewComputeNormal(_fvector *n, _fvector *o, _fvector *a, _fvector *b);
void viewCreate(_viewports vp, int view);

#ifdef NON_MATCHING
/* GS SCISSOR value of the viewport */
static inline u_long viewScissor(ViewDef *d)
{
    return d->x | ((u_long)(d->x + d->w - 1) << 16) | ((u_long)d->y << 32) | ((u_long)(d->y + d->h - 1) << 48);
}
static inline void setScissors(unsigned char *db, u_long s)
{
    *(u_long *)(db + VIEW_DB_SCISSOR0) = s;
    *(u_long *)(db + VIEW_DB_SCISSOR3) = s;
    *(u_long *)(db + VIEW_DB_SCISSOR2) = s;
    *(u_long *)(db + VIEW_DB_SCISSOR1) = s;
}
/* clear color as an RGBAQ register (Q = 1.0) */
static inline u_long rgbaq(unsigned long r, unsigned long g, unsigned long b, unsigned long a)
{
    return r | (g << 8) | (b << 16) | (a << 24) | ((u_long)0x3F800000 << 32);
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 4/141 words, size 0x178 vs 0x234 */
/* sets view `view` to viewport vp: camera CS, double buffer, scissor and projection */
void viewCreate(_viewports vp, int view)
{
    ViewInfo *vi = &viewInfo[view];
    ViewDef *d;
    unsigned char *db;
    int r, g, b, a;

    vi->prevViewport = vi->viewport;
    if (!vi->cs)
        vi->cs = CsPool::csActivate();
    vi->viewport = vp;
    vi->ownCs = vi->cs;
    d = &viewDef[vp];
    vi->def = d;
    worldCtx[view].lightEnv = lightGetEnv(0);
    db = viewDb[view];
    IncognitoEntertainmentGS::ieGsInitAA(false);
    viewGetBgColor(r, g, b, a);
    IncognitoEntertainmentGS::ieGsSetDefDBuffDc((IncognitoEntertainmentGS::ieGsDBuffDc *)db, vp != 4, d->fbw, d->fbh, 2,
                                                0x31, 1);
    viewSetBgColor(r, g, b, a);
    setScissors(db, viewScissor(d));
    viewSetFov(view, d->fov, d->aspect);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewCreate__F10_viewportsi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 15/352 words, size 0x4e8 vs 0x580 */
/* Per frame: world->eye matrix, world-space frustum planes (+ signs for the box test), world->screen/clip
 * matrices, the VU1 packet copies and the ambient light (blended towards the ambient volume the camera is in). */
void viewUpdate(int view)
{
    ViewInfo *vi = &viewInfo[view];
    _worldctx *w = &worldCtx[view];
    _cs *cs = vi->cs;
    _fvector *t;
    float blend, inv;
    int i, j, k;

    viewCurView = view;
    viewApplySwap(w->weMat, cs->mat);
    w->eo.x = -cs->trans.x;
    w->eo.y = -cs->trans.y;
    w->eo.z = -cs->trans.z;
    vu0MulMatrixTP3x3(w->fovNorms[0], (float (*)[4])vi->def->planes[0], cs->mat);
    vu0MulMatrixTP3x3(w->fovNorms[1], (float (*)[4])vi->def->planes[1], cs->mat);
    mathfTransposeMatrix(w->fovNormsTP[0], w->fovNorms[0]);
    mathfTransposeMatrix(w->fovNormsTP[1], w->fovNorms[1]);
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            for (k = 0; k < 3; k++)
                w->fovSigns[i][j][k] = w->fovNormsTP[i][j][k] < 0.0f ? -1.0f : 1.0f;
    /* plane distances: the camera position against each plane (row 3 of the transposed sets) */
    for (i = 0; i < 2; i++)
        for (k = 0; k < 3; k++)
            w->fovNormsTP[i][3][k] = cs->trans.x * w->fovNorms[i][0][k] + cs->trans.y * w->fovNorms[i][1][k] +
                                     cs->trans.z * w->fovNorms[i][2][k];
    w->fovNormsTP[1][3][2] -= fogGetFarClipRange();
    mathfMulMatrix(worldToScreenMat[view], viewScreenMats[view], w->weMat);
    mathfMulMatrix(viewClipView[view], viewClipMats[view], w->weMat);
    mathfMulMatrix(viewFovView[view], viewFovMats[view], w->weMat);
    mathfTransposeMatrix(vuScreenToWorld, worldToScreenMat[view]);
    t = viewGetTrans(view);
    for (k = 0; k < 4; k++)
        vuScreenToWorld[3][k] -= vuScreenToWorld[0][k] * t->x + vuScreenToWorld[1][k] * t->y + vuScreenToWorld[2][k] * t->z;
    vuScreenHalf[0] = vi->def->fov * 0.5f;
    vuScreenHalf[1] = vi->def->fov * 0.5f * vi->def->aspect;
    viewSetVUPacketMat(worldToScreenMat[view], viewClipView[view], viewFovView[view], w->weMat, view);

    blend = s_viewAmbientVolBlend[view];
    inv = 1.0f - blend;
    vuLightParams[12] = viewAmbientRed * inv + blend * s_viewAmbientVolRed[view];
    vuLightParams[13] = viewAmbientGreen * inv + blend * s_viewAmbientVolGreen[view];
    vuLightParams[14] = viewAmbientBlue * inv + blend * s_viewAmbientVolBlue[view];
    vuShadowAmbient[0] = viewAmbientRed * 0.3f;
    vuShadowAmbient[1] = viewAmbientGreen * 0.3f;
    vuShadowAmbient[2] = viewAmbientBlue * 0.3f;
    viewTimeOfDay = todGetCurrentTOD();
    todGetAmbient(&viewTODAmbientRed, &viewTODAmbientGreen, &viewTODAmbientBlue);
    blend = s_viewAmbientVolBlend[view];
    viewTODAmbientRed = viewTODAmbientRed * inv + blend * s_viewAmbientVolRed[view];
    viewTODAmbientGreen = viewTODAmbientGreen * inv + blend * s_viewAmbientVolGreen[view];
    viewTODAmbientBlue = viewTODAmbientBlue * inv + blend * s_viewAmbientVolBlue[view];
    s_viewAmbientVolBlend[view] = 0.0f;  /* the volume must set it again every frame */
    todUpdate(view);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewUpdate__Fi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 4/52 words, size 0xd0 vs 0xc0 */
/* box (centre, half extents) against the 6 frustum planes: 0 as soon as it is fully outside one */
int viewInFOV(int view, _fvector *center, _fvector *extent)
{
    _worldctx *w = &worldCtx[view];
    int i, j;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            float *n = w->fovNormsTP[i][j];
            float *s = w->fovSigns[i][j];
            float px = center->x + extent->x * s[0];
            float py = center->y + extent->y * s[1];
            float pz = center->z + extent->z * s[2];
            if (n[0] * px + n[1] * py + n[2] * pz < w->fovNormsTP[i][3][j])
                return 0;
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewInFOV__FiP8_fvectorT1);
#endif
/* same, with the far plane pulled in to `dist` */
int viewInFOV(int view, _fvector *center, _fvector *extent, float dist)
{
    _worldctx *w = &worldCtx[view];
    float saved = w->fovNormsTP[1][3][2];
    int r;
    w->fovNormsTP[1][3][2] += fogGetFarClipRange() - dist;
    r = viewInFOV(view, center, extent);
    w->fovNormsTP[1][3][2] = saved;
    return r;
}
/* the shadow view: planes go straight to VU0 memory */
void viewShadowUpdate(int view)
{
    ViewInfo *vi = &viewInfo[view];
    _worldctx *w = &worldCtx[view];
    float (*m)[4] = vi->cs->mat;
    viewApplySwap(w->weMat, m);
    vu0MulMatrixTP3x3(w->fovNorms[0], (float (*)[4])vi->def->planes[0], m);
    viewStoreNorms1InVu0();
    vu0MulMatrixTP3x3(w->fovNorms[1], (float (*)[4])vi->def->planes[1], m);
    viewStoreNorms2InVu0();
    mathfMulMatrix(worldToScreenMat[view], viewScreenMats[view], w->weMat);
    mathfMulMatrix(viewShadWorldToScrMat, viewShadRecMat, w->weMat);
}
/* VU0 data memory stores (vsqi): hardware, stay asm */
INCLUDE_ASM("asm/nonmatchings/common/view", viewStoreNorms1InVu0__Fv);
INCLUDE_ASM("asm/nonmatchings/common/view", viewStoreNorms2InVu0__Fv);
float (*viewGetWorld2ScreenMat(int view))[4]
{
    return worldToScreenMat[view];
}
float (*viewGetShadWorldScreenMat(void))[4]
{
    return viewShadWorldToScrMat;
}
void viewGetTOD(int *a, float *b, _todInfo14 *c)
{
    todGetTOD(a, b, c);
}
void viewSetTOD(int a, float b, void *c, float d)
{
    todSetTOD(a, b, c, d);
}
_fvector *viewGetRot(int view)
{
    return &viewInfo[view].cs->rot;
}
float (*viewGetMat(int view))[4]
{
    return viewInfo[view].cs->mat;
}
_fvector *viewGetTrans(int view)
{
    return &viewInfo[view].cs->trans;
}
void viewSetRot(_fvector *rot, int view)
{
    _cs *cs = viewInfo[view].cs;
    cs->rot.x = rot->x;
    cs->rot.y = rot->y;
    cs->rot.z = rot->z;
    mathfRotMatrixRPH(cs->mat, &cs->rot);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 4/26 words, size 0x68 vs 0x40 */
void viewSetRot(float (*m)[4][4], int view)
{
    _cs *cs = viewInfo[view].cs;
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            cs->mat[i][j] = (*m)[i][j];
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewSetRot__FPA3_A3_fi);
#endif
void viewSetTrans(_fvector *t, int view)
{
    viewInfo[view].cs->trans.x = t->x;
    viewInfo[view].cs->trans.y = t->y;
    viewInfo[view].cs->trans.z = t->z;
}
int viewGetCurView(void)
{
    return viewCurView;
}
_cs *viewGetViewCs(int view)
{
    return viewInfo[view].cs;
}
_worldctx *viewGetWorldCtx(int view)
{
    return &worldCtx[view];
}
void viewSetWorldEpNode(_hierhead *node)
{
    int i;
    for (i = 4; i >= 0; i--)
        worldCtx[i].ep = node;
}
/* ambient of the light volume the camera of `view` is in (blend 0 = none); reset by viewUpdate every frame */
void viewSetAmbientVol(int view, float blend, float r, float g, float b)
{
    s_viewAmbientVolBlend[view] = blend;
    s_viewAmbientVolRed[view] = r;
    s_viewAmbientVolGreen[view] = g;
    s_viewAmbientVolBlue[view] = b;
}
_fvector *viewGetWeTrans(int view)
{
    return &worldCtx[view].eo;
}
float (*viewGetWeMat(int view))[4]
{
    return worldCtx[view].weMat;
}
/* sky node by its id2: 0 sky dome (second sky cleared), 4 clouds; view 0 also feeds the time of day */
void viewSetSkyEntry(_hierhead *node, int view)
{
    _worldctx *w = &worldCtx[view];
    if (!w->skyCs)
        w->skyCs = CsPool::csActivate();
    if (!w->skyCs2)
        w->skyCs2 = CsPool::csActivate();
    if (!w->skyClouds)
        w->skyClouds = CsPool::csActivate();
    if (node->id2 == 0) {
        w->skyCs->epNode = node;
        w->skyCs2->epNode = 0;
    }
    if (node->id2 == 4)
        w->skyClouds->epNode = node;
    if (view == 0)
        todSetSkyEntry(node);
}
_cs *viewGetSky(int view)
{
    return worldCtx[view].skyCs;
}
void viewSetSkyTrans(_fvector *t, int view)
{
    _worldctx *w = &worldCtx[view];
    if (w->skyCs) {
        w->skyCs->trans.x = t->x;
        w->skyCs->trans.y = t->y;
        w->skyCs->trans.z = t->z;
    }
    if (w->skyCs2) {
        w->skyCs2->trans.x = t->x;
        w->skyCs2->trans.y = t->y;
        w->skyCs2->trans.z = t->z;
    }
    if (w->skyClouds) {
        w->skyClouds->trans.x = t->x;
        w->skyClouds->trans.y = t->y;
        w->skyClouds->trans.z = t->z;
    }
}
/* which: 0 sky, 1 second sky, 2 clouds; for every view */
void viewSetSkyNode(_hierhead *node, int which)
{
    int i;
    if (which == 0) {
        for (i = 4; i >= 0; i--)
            if (worldCtx[4 - i].skyCs)
                worldCtx[4 - i].skyCs->epNode = node;
    } else if (which == 1) {
        for (i = 4; i >= 0; i--)
            if (worldCtx[4 - i].skyCs2)
                worldCtx[4 - i].skyCs2->epNode = node;
    } else if (which == 2) {
        for (i = 4; i >= 0; i--)
            if (worldCtx[4 - i].skyClouds)
                worldCtx[4 - i].skyClouds->epNode = node;
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/171 words, size 0xe0 vs 0x2ac */
/* transposed copies of the matrices into the VU1 packet data (d and view unused) */
void viewSetVUPacketMat(float (*a)[4], float (*b)[4], float (*c)[4], float (*)[4], int)
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            vuWorldToScreen[i][j] = a[j][i];
            vuClipView[i][j] = b[j][i];
            vuFovView[i][j] = c[j][i];
            vuWorldToScreen2[i][j] = a[j][i];
            vuClipView2[i][j] = b[j][i];
        }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewSetVUPacketMat__FPA3_fN30i);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/37 words, size 0x44 vs 0x94 */
/* camera matrix (y forward, z up) to the eye convention: rows 1 and 2 swapped, the new row 1 negated */
void viewApplySwap(float (*dst)[4], float (*src)[4])
{
    int j;
    for (j = 0; j < 4; j++) {
        dst[0][j] = src[0][j];
        dst[1][j] = -src[2][j];
        dst[2][j] = src[1][j];
        dst[3][j] = src[3][j];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewApplySwap__FPA3_fT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 44/56 words, size 0xe0 vs 0xe0 */
/* clear color of every view (only while the time of day does not drive it) */
void viewSetBgColor(unsigned long r, unsigned long g, unsigned long b, unsigned long a)
{
    if (!todActive() && viewNumViews > 0) {
        u_long c = rgbaq(r, g, b, a);
        unsigned char *db = viewDb[0];
        int n;
        for (n = viewNumViews; n; n--, db += VIEW_DB_SIZE) {
            *(u_long *)(db + VIEW_DB_CLEAR0) = c;
            *(u_long *)(db + VIEW_DB_CLEAR1) = c;
        }
    }
    viewBgR = r;
    viewBgG = g;
    viewBgB = b;
    viewBgA = a;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewSetBgColor__FUlUlUlUl);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 44/56 words, size 0xe0 vs 0xe0 */
void viewSetTODBgColor(unsigned long r, unsigned long g, unsigned long b, unsigned long a)
{
    if (todActive() && viewNumViews > 0) {
        u_long c = rgbaq(r, g, b, a);
        unsigned char *db = viewDb[0];
        int n;
        for (n = viewNumViews; n; n--, db += VIEW_DB_SIZE) {
            *(u_long *)(db + VIEW_DB_CLEAR0) = c;
            *(u_long *)(db + VIEW_DB_CLEAR1) = c;
        }
    }
    viewTODBgR = r;
    viewTODBgG = g;
    viewTODBgB = b;
    viewTODBgA = a;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewSetTODBgColor__FUlUlUlUl);
#endif
void viewGetBgColor(int &r, int &g, int &b, int &a)
{
    r = viewDb[0][VIEW_DB_CLEAR0];
    g = viewDb[0][VIEW_DB_CLEAR0 + 1];
    b = viewDb[0][VIEW_DB_CLEAR0 + 2];
    a = viewDb[0][VIEW_DB_CLEAR0 + 3];
}
unsigned char *viewGetDb(int view)
{
    return viewDb[view];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/7 words, size 0x1c vs 0x18 */
int viewGetNumViews(void)
{
    return gUseUnifiedView ? 1 : viewNumViews;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewGetNumViews__Fv);
#endif
void viewSetNumViews(int n)
{
    viewNumViews = n;
}
void viewGetCenter(int view, int *x, int *y)
{
    *x = viewInfo[view].def->cx;
    *y = viewInfo[view].def->cy;
}
void viewGetWH(int view, int *w, int *h)
{
    *w = viewInfo[view].def->w;
    *h = viewInfo[view].def->h;
}
float (*viewGetWorldToScreenMat(int view))[4]
{
    return worldToScreenMat[view];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 3/10 words, size 0x28 vs 0x24 */
void viewGetZBuffParams(int view, float *scale, float *offset)
{
    *scale = viewScreenMats[view][2][2];
    *offset = viewScreenMats[view][2][3];
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewGetZBuffParams__FiPfT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/27 words, size 0x6c vs 0x64 */
void viewInit(void)
{
    ViewInfo *vi;
    for (vi = viewInfo; vi < viewInfo + 5; vi++) {
        vi->cs = 0;
        vi->def = 0;
        vi->viewport = 0;
        vi->prevViewport = 0;
    }
    todInit();
    viewRearLargeActive = 0;
    viewRearViewConfig = 1;
    viewAmbientChanged = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewInit__Fv);
#endif
void viewTweakInit(void)
{
}
void viewTweakSetFov(void)
{
    int i;
    for (i = 0; i < viewNumViews; i++)
        viewSetFov(i, viewFovH, viewGetDef(i)->aspect);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 14/38 words, size 0x90 vs 0x98 */
/* scene ambient light (0..1 per channel); the animated copy is in 1/32 units */
void viewSetAmbient(float r, float g, float b)
{
    vuLightParams[14] = b;
    viewAmbientRed = r;
    viewAmbientGreen = g;
    viewAnimAmbientBlue = b * 0.03125f;
    viewAnimAmbientRed = r * 0.03125f;
    viewAnimAmbientGreen = g * 0.03125f;
    viewAmbientBlue = b;
    vuLightParams[12] = r;
    vuLightParams[13] = g;
    viewAmbientChanged = (0.0f < r || 0.0f < g || 0.0f < b) ? 1 : 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewSetAmbient__Ffff);
#endif
void viewGetAmbient(float *r, float *g, float *b)
{
    *r = viewAmbientRed;
    *g = viewAmbientGreen;
    *b = viewAmbientBlue;
}
int viewGetAnimAmbient(float *r, float *g, float *b)
{
    *r = viewAnimAmbientRed;
    *g = viewAnimAmbientGreen;
    *b = viewAnimAmbientBlue;
    return viewAmbientChanged;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 7/16 words, size 0x40 vs 0x40 */
void viewGetFov(int view, float *fov, float *aspect)
{
    float f = viewInfo[view].def->fov;
    *fov = f;
    *aspect = viewScreenMats[view][1][1] / f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewGetFov__FiPfT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 12/289 words, size 0x484 vs 0x3d4 */
/* Screen distance and aspect of a view: rebuilds the camera-space frustum planes (near, left, right, top, bottom,
 * far), the screen/clip matrices and, for the shadow view, the shadow texture matrix. */
void viewSetFov(int view, float fov, float aspect)
{
    ViewInfo *vi = &viewInfo[view];
    ViewDef *d = vi->def;
    FVECTOR o, p, q;
    FMATRIX tmp;
    float dist;
    float halfW;

    o.x = o.y = o.z = o.w = 0.0f;
    p.x = p.y = p.w = 0.0f;
    p.z = 200.0f;
    d->fov = fov;
    dist = fov;
    if (vi->viewport == VIEWPORT_SHADOW)
        dist = 1024.0f;
    d->planes[0][0].x = 0.0f;
    d->planes[0][0].y = 1.0f;
    d->planes[0][0].z = 0.0f;
    d->planes[0][0].w = 0.0f;
    halfW = (float)(d->w >> 1);
    q.x = halfW;
    q.y = dist;
    q.z = 0.0f;
    viewComputeNormal(&d->planes[0][1], &o, &p, &q);
    q.y = dist;
    q.z = 0.0f;
    q.x = (float)-(int)(d->w >> 1);
    viewComputeNormal(&d->planes[0][2], &o, &q, &p);
    p.y = 0.0f;
    p.z = 0.0f;
    q.x = 0.0f;
    p.x = (float)-(int)(d->w >> 1);
    q.y = dist;
    q.z = (float)d->h;
    viewComputeNormal(&d->planes[1][0], &o, &p, &q);
    p.y = 0.0f;
    p.z = 0.0f;
    q.x = 0.0f;
    q.y = dist;
    p.x = (float)-(int)(d->w >> 1);
    q.z = (float)-(int)d->h;
    viewComputeNormal(&d->planes[1][1], &o, &q, &p);
    d->planes[1][2].x = 0.0f;
    d->planes[1][2].y = -1.0f;
    d->planes[1][2].z = 0.0f;
    d->planes[1][2].w = 0.0f;
    if (vi->viewport != VIEWPORT_SHADOW) {
        viewDegrees = atanf((float)(d->w >> 1) / dist) * 57.29578f * 2.0f;
        d->degrees = viewDegrees;
        viewFovH = dist;
    }
    mathfViewScreenMatrix(viewScreenMats[view], viewClipMats[view], viewFovMats[view], d->fov, 1.0f, aspect, d->offX, d->offY,
                          0.1f, 16777210.0f, 1.0f, 65536.0f, (float)(d->w >> 1), (float)(d->h + 16));
    *(FMATRIX *)tmp = *(FMATRIX *)d->planes[0];
    mathfTransposeMatrix((float (*)[4])d->planes[0], tmp);
    *(FMATRIX *)tmp = *(FMATRIX *)d->planes[1];
    mathfTransposeMatrix((float (*)[4])d->planes[1], tmp);
    if (vi->viewport == VIEWPORT_SHADOW) {
        float s = d->fov * 0.0078125f;
        int i, j;
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                viewShadRecMat[i][j] = 0.0f;
        viewShadRecMat[0][0] = s;
        viewShadRecMat[0][2] = 0.5f;
        viewShadRecMat[1][1] = d->fov * 0.0078125f;
        viewShadRecMat[1][2] = 0.5f;
        viewShadRecMat[2][2] = 1.0f;
        viewShadRecMat[3][2] = 1.0f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewSetFov__Fiff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/40 words, size 0xa0 vs 0x88 */
/* unit normal of the plane through o, a, b: (a - o) x (b - o); n.w is kept */
void viewComputeNormal(_fvector *n, _fvector *o, _fvector *a, _fvector *b)
{
    float ux = a->x - o->x, uy = a->y - o->y, uz = a->z - o->z;
    float vx = b->x - o->x, vy = b->y - o->y, vz = b->z - o->z;
    float cx = uy * vz - vy * uz;
    float cy = uz * vx - vz * ux;
    float cz = ux * vy - vx * uy;
    float r = 1.0f / eeSqrtf(cx * cx + cy * cy + cz * cz);
    n->z = cz * r;
    n->x = cx * r;
    n->y = cy * r;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewComputeNormal__FP8_fvectorN30);
#endif
/* 1 view <-> 2 views (top/bottom); only while there are fewer than 3 views */
void viewToggleSplitScreen(bool, bool)
{
    if (viewNumViews >= 3)
        return;
    if (viewNumViews == 1) {
        viewNumViews = 2;
        viewCreate((_viewports)VIEWPORT_TOP, 0);
        viewCreate((_viewports)VIEWPORT_BOTTOM, 1);
    } else {
        viewNumViews = 1;
        viewCreate((_viewports)VIEWPORT_FULL, 0);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 2/17 words, size 0x28 vs 0x44 */
/* is view 0 a split-screen half */
int viewGetScreenDisplay(void)
{
    switch (viewInfo[0].viewport) {
    case VIEWPORT_TOP:
    case VIEWPORT_BOTTOM:
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewGetScreenDisplay__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 75/103 words, size 0x19c vs 0x19c */
/* grapple camera: with the unified view, view 0 borrows view 2's camera and viewport 5 */
void viewGrappleConfig(void)
{
    ViewDef *d;
    if (gUseUnifiedView) {
        viewInfo[0].cs = viewInfo[2].cs;
        d = &viewDef[5];
        viewInfo[0].def = d;
    } else {
        viewInfo[0].cs = viewInfo[0].ownCs;
        d = &viewDef[viewInfo[0].viewport];
        viewInfo[0].def = d;
    }
    viewSetFov(0, d->fov, d->aspect);
    *(u_long *)(viewDb[0] + VIEW_DB_SCISSOR0) = viewScissor(d);
    *(u_long *)(viewDb[0] + VIEW_DB_SCISSOR2) = viewScissor(d);
    *(u_long *)(viewDb[0] + VIEW_DB_SCISSOR1) = viewScissor(d);
    *(u_long *)(viewDb[0] + VIEW_DB_SCISSOR3) = viewScissor(d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/view", viewGrappleConfig__Fv);
#endif
ViewDef *viewGetDef(int view)
{
    return viewInfo[view].def;
}
ViewDef *viewGetDef(_viewports vp)
{
    return &viewDef[vp];
}
void viewSetOddEven(int odd)
{
    viewOddScan = odd != 0;
}
int viewIsOddScan(void)
{
    return viewOddScan;
}
