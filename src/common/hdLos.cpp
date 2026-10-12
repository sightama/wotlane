#include "common.h"
#include "hieri_types.h"
#include "hd.h"

/* Line of sight against the level hulls (optionally against one cs only). */
extern int hdLosFilterCs __asm__("D_006F8D48");
extern int hdLosCsId __asm__("D_006F8D4C");
extern float hdLosFar __asm__("D_006F8748");

int hdTestNode(_hierhead *node, _fvector *p0, _fvector *p1, float r, float (*m)[4][4], _hdResult *res, int maxRes);
int hdLosTest(_fvector *from, _fvector *to, _hdResult *res, bool option);

int hdLosCSTest(_fvector *from, _fvector *to, _hdResult *res, int csId)
{
    int r;

    hdLosCsId = csId;
    hdLosFilterCs = 1;
    r = hdLosTest(from, to, res, false);
    hdLosFilterCs = 0;
    return r;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 9/112 words, size 0x194 vs 0x1c0 */
/* Nonzero when nothing blocks from -> to. With res, the nearest hit is kept there (and the walk stops past it). */
int hdLosTest(_fvector *from, _fvector *to, _hdResult *res, bool option)
{
    _hdResult tmp[16];
    HdItGridLine it;
    float best = hdLosFar;
    int n, i;

    if (res)
        res->hit = 0;
    if (hdLosFilterCs) {
        it.filterCs = 1;
        it.csId = hdLosCsId;
    }
    it.option = option;
    it.reset(*from, *to, 0.0f);
    while (!it.isDone()) {
        n = hdTestNode(it.hull, from, to, 0.0f, (float (*)[4][4])it.mat, tmp, 16);
        if (n && !res)
            return 0;
        for (i = 0; i < n; i++) {
            if (!(tmp[i].point.w < best))
                continue;
            *res = tmp[i];
            if (!(res->ids & (0x3FFF << 11)))
                HD_RESULT_SET_IDS(res, it.hatId1, it.hatId2);
            best = res->point.w;
            it.bestT = best;
        }
        it.next();
    }
    if (!res)
        return 1;
    return !res->hit;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdLos", hdLosTest__FP8_fvectorT0P9_hdResultb);
#endif
