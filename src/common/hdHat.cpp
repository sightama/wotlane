#include "common.h"
#include "hieri_types.h"
#include "hd.h"

/* Ground ("hat") height under a point: a vertical segment through the hulls of the grid. */
extern float hdDefaultHeight;
__asm__("#SNFIX_SMALL hdDefaultHeight");

int hdTestNode(_hierhead *node, _fvector *p0, _fvector *p1, float r, float (*m)[4][4], _hdResult *res, int maxRes);

void hdInitHatTimer__Fv(void *self) __asm__("hdInitHatTimer__Fv");
void hdInitHatTimer__Fv(void *self)
{
}
void hdSetDefaultHeight(float h)
{
    hdDefaultHeight = h;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 3/109 words, size 0x1b4 vs 0x1a0 */
/* Casts from `up` above pos down to `down` below it; returns how far below pos the highest surface is (normal gets its
   normal), or 0 with an up normal when nothing was hit. */
float hdHatTest(_fvector *pos, _fvector *normal, float up, bool option, float down)
{
    FVECTOR top, bottom;
    _hdResult res[16];
    HdItGridBox it;
    _hdResult *best;
    int n = 0;
    int i;

    bottom = *pos;
    top = *pos;
    bottom.z -= down;
    top.z += up;
    it.option = option;
    it.reset(top, bottom, 0.0f);
    while (it.row < it.maxRow) {
        n += hdTestNode(it.state.hull, &top, &bottom, 0.0f, (float (*)[4][4])it.state.mat, &res[n], 16 - n);
        it.next();
        if (!(it.row < it.maxRow) || n >= 16)
            break;
    }
    if (!n) {
        normal->x = 0.0f;
        normal->z = 1.0f;
        normal->y = 0.0f;
        normal->w = 0.0f;
        return 0.0f;
    }
    best = &res[0];
    for (i = 1; i < n; i++)
        if (res[i].point.w < best->point.w)
            best = &res[i];
    *normal = best->normal;
    return pos->z - best->point.z;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdHat", hdHatTest__FP8_fvectorT0fbf);
#endif
