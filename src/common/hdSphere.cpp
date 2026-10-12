#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "memory_stack.h"

/* Sphere (moving or not) against the level hulls, alone or in bundles that share one grid walk. */
extern FVECTOR hdEmptyBox[2] __asm__("D_006F3E30");

int hdTestNode(_hierhead *node, _fvector *p0, _fvector *p1, float r, float (*m)[4][4], _hdResult *res, int maxRes);
int hdAABBVsAABB(float *amin, float *amax, float *bmin, float *bmax);
int hdSphereTest(_fvector *a, _fvector *b, float r, _hdResult *res, unsigned skipId2, int max);

#ifdef NON_MATCHING
/* Results of a hull without ids of its own get the ids of the cs the grid walk is in. */
static inline void setHatIds(_hdResult *res, int n, HdItGridBox &it)
{
    int j;

    if (res[0].ids & (0x3FFF << 11))
        return;
    for (j = 0; j < n; j++)
        HD_RESULT_SET_IDS(&res[j], it.hatId1, it.hatId2);
}
#endif

INCLUDE_ASM("asm/nonmatchings/common/hdSphere", D_006F3E30);
int hdSphereTest(_fvector *p, float r, _hdResult *res, unsigned skipId2, int max)
{
    return hdSphereTest(p, p, r, res, skipId2, max);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 2/114 words, size 0x188 vs 0x1c8 */
/* Sphere of radius r swept a -> b; skips the hulls of the cs whose ep id2 is skipId2. Returns the number of contacts. */
int hdSphereTest(_fvector *a, _fvector *b, float r, _hdResult *res, unsigned skipId2, int max)
{
    int n = 0;

    if (!max)
        return 0;
    /* the iterator is constructed only past the early return, as in retail */
    {
        HdItGridBox it;

        it.skipSelf = 1;
        it.skipId2 = skipId2;
        it.reset(*a, *b, r);
        while (it.cur < it.end) {
            _hdResult *r0 = &res[n];
            int k = hdTestNode(it.hull, a, b, r, (float (*)[4][4])it.mat, r0, max - n);

            if (k)
                setHatIds(r0, k, it);
            n += k;
            if (!(n < max))
                break;
            it.next();
        }
    }
    if (!n)
        res->hit = 0;
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdSphere", hdSphereTest__FP8_fvectorT0fP9_hdResultUii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/237 words, size 0x350 vs 0x3b4 */
struct HdBundleBox {
    float min[4];
    float max[4];
};
struct HdBundleTest {
    _fvector *a;
    _fvector *b;
    float r;
    int maxRes;
    _hdResult *results;
    int numResults;
};
/* n sphere tests in one grid walk over the union of their swept boxes; returns the total number of contacts. */
int hdBundledSphereTest(_hdSphereBundleTest *tests, int n, int skipId2)
{
    HdBundleTest *t = (HdBundleTest *)tests;
    HdBundleBox *boxes;
    FVECTOR all[2];
    HdItGridBox it;
    int total = 0;
    int i, k;

    MemoryStack::global.pushMark();
    all[0] = hdEmptyBox[0];
    all[1] = hdEmptyBox[1];
    boxes = (HdBundleBox *)MemoryStack::global.alloc16((unsigned short)(n * 32));
    for (i = 0; i < n; i++) {
        for (k = 0; k < 3; k++) {
            float av = ((float *)t[i].a)[k];
            float bv = ((float *)t[i].b)[k];
            float *lo = &((float *)&all[0])[k];
            float *hi = &((float *)&all[1])[k];

            if (bv < av) {
                boxes[i].min[k] = bv - t[i].r;
                boxes[i].max[k] = av + t[i].r;
            } else {
                boxes[i].max[k] = bv + t[i].r;
                boxes[i].min[k] = av - t[i].r;
            }
            *lo = *lo < boxes[i].min[k] ? *lo : boxes[i].min[k];
            *hi = *hi > boxes[i].max[k] ? *hi : boxes[i].max[k];
        }
        t[i].numResults = 0;
    }
    it.skipId2 = skipId2;
    it.skipSelf = 1;
    it.reset(all[0], all[1], 0.0f);
    while (it.cur < it.end) {
        for (i = 0; i < n; i++) {
            float *hb;
            _hdResult *r0;
            int got;

            if (t[i].numResults == t[i].maxRes)
                continue;
            hb = it.cellObj ? (float *)it.cellObj[1] : 0;
            if (hb && !hdAABBVsAABB(boxes[i].min, boxes[i].max, hb, hb + 3))
                continue;
            r0 = &t[i].results[t[i].numResults];
            got = hdTestNode(it.hull, t[i].a, t[i].b, t[i].r, (float (*)[4][4])it.mat, r0, t[i].maxRes - t[i].numResults);
            if (got)
                setHatIds(r0, got, it);
            total += got;
            t[i].numResults += got;
        }
        it.next();
    }
    MemoryStack::global.popMark();
    return total;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdSphere", hdBundledSphereTest__FP19_hdSphereBundleTestii);
#endif
