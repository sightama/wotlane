#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "vecmath.h"

/* Trigger volumes ("td"): BSP-shaped nodes in the collision grid that report when a point is inside them. */
struct HierCollisionTrigger {
    _hierhead head;     // 0x0
    _hierbsp *bsp;      // 0x4
};
struct _tdResult {
    HierCollisionTrigger *trigger;  // 0x0
    unsigned short hatId1;          // 0x4 the cs it belongs to
    unsigned short hatId2;          // 0x6
};

void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
int bspPointTest(_hierbsp *b, _fvector *pt);
int tdColTriggerNode(HierCollisionTrigger *t, _fvector *p, float (*m)[4][4], _tdResult *res);

#ifdef NON_MATCHING
/* equivalent (untuned): 7/38 words, size 0x98 vs 0x7c */
/* p (in m's parent space) inside trigger t? Fills res->trigger. */
int tdColTriggerNode(HierCollisionTrigger *t, _fvector *p, float (*m)[4][4], _tdResult *res)
{
    FVECTOR q;

    mathfMulVec(*m, p, &q);
    vecAdd(&q, &q, (_fvector *)(*m)[3]);
    if (bspPointTest(t->bsp, &q) == 0x200)
        return 0;
    res->trigger = t;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/td", tdColTriggerNode__FP20HierCollisionTriggerP8_fvectorPA3_A3_fP9_tdResult);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 17/51 words, size 0xc8 vs 0xcc */
/* The triggers containing p (at most max). */
int tdTest(_fvector *p, _tdResult *res, int max)
{
    HdItGridBox it;
    int n = 0;

    it.cell.triggers = 1;
    it.reset(*p, *p, 0.0f);
    while (it.row < it.maxRow) {
        if (tdColTriggerNode((HierCollisionTrigger *)it.state.hull, p, (float (*)[4][4])it.state.mat, res)) {
            n++;
            res->hatId1 = it.state.hatId1;
            res->hatId2 = it.state.hatId2;
            res++;
            if (!(n < max))
                break;
        }
        it.next();
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/td", tdTest__FP8_fvectorP9_tdResulti);
#endif
