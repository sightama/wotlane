#include "common.h"
#include "hieri_types.h"

/* Result of a segment walk through a BSP: the first crossing between empty space (material 0x200) and a solid. */
struct _bspresult {
    FVECTOR point;          // 0x0 crossing point
    FVECTOR3 normal;        // 0x10 the splitting plane
    unsigned pad1C;         // 0x1C
    float planeD;           // 0x20
    float distB;            // 0x24 signed distance of the segment end to the plane
    short hit;              // 0x28
    unsigned short mat2A;   // 0x2A
    unsigned short matFrom; // 0x2C material on the start side
    unsigned short matTo;   // 0x2E material on the far side
};
typedef char _size__bspresult[sizeof(_bspresult) == 0x30 ? 1 : -1];

extern _bspresult *gRet;
int bspVecTestIn(_hierbsp *b, _fvector *a, _fvector *c);
float mathfPlaneTest(_plane *pl, _fvector *pt);
int bspVecTest(_hierbsp *b, _fvector *a, _fvector *c, _bspresult *r);
int bspPointTest(_hierbsp *b, _fvector *pt);
void bspPlaneTest2Vec(_plane *pl, _fvector *a, _fvector *b, _fvector *out);

int bspVecTest(_hierbsp *b, _fvector *a, _fvector *c, _bspresult *r)
{
    gRet = r;
    return bspVecTestIn(b, a, c);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 67/182 words, size 0x2d4 vs 0x2d8 */
/* A leaf reached without a crossing: the whole (sub)segment is in material mat. */
static inline void bspLeaf(_bspresult *r, unsigned short mat)
{
    r->matFrom = mat;
    r->hit = 0;
    r->mat2A = mat;
    r->matTo = mat;
}
/* Walks segment a -> c down the tree (results in gRet); distances within 0.001 of a plane snap to the other end's. */
int bspVecTestIn(_hierbsp *b, _fvector *a, _fvector *c)
{
    _bspresult *r = gRet;
    FVECTOR d, m;
    _hierbsp *kid;
    unsigned short mat, saved;
    float t;

    bspPlaneTest2Vec(&b->plane, a, c, &d);
    if (d.x < 0.001f && -0.001f < d.x)
        d.x = d.y;
    if (d.y < 0.001f && -0.001f < d.y)
        d.y = d.x;
    if (0.0f <= d.x) {
        mat = b->matIdFront;
        kid = b->frontKid;
    } else {
        mat = b->matIdBack;
        kid = b->backKid;
    }
    if (0.0f <= d.x * d.y) {
        if (!kid) {
            bspLeaf(r, mat);
            return 0;
        }
        return bspVecTestIn(kid, a, c);
    }
    t = d.x / (d.x - d.y);
    m.x = a->x + t * (c->x - a->x);
    m.z = a->z + t * (c->z - a->z);
    m.y = a->y + t * (c->y - a->y);
    if (!kid)
        bspLeaf(r, mat);
    else
        bspVecTestIn(kid, a, &m);
    if (r->hit)
        return 1;
    if (0.0f <= d.x) {
        mat = b->matIdBack;
        kid = b->backKid;
    } else {
        mat = b->matIdFront;
        kid = b->frontKid;
    }
    saved = r->matTo;
    if (!kid)
        bspLeaf(r, mat);
    else
        bspVecTestIn(kid, &m, c);
    if ((r->matFrom == 0x200) == (saved == 0x200))
        return 0;
    r->hit = 1;
    r->point = m;
    r->normal.x = b->plane.norm.x;
    r->normal.y = b->plane.norm.y;
    r->normal.z = b->plane.norm.z;
    r->planeD = b->plane.d;
    r->distB = d.y;
    r->matFrom = saved;
    if (b->coplanarKid)
        r->matTo = bspPointTest(b->coplanarKid, &m);
    else
        r->matTo = b->matIdBack;
    if (d.x < 0.0f) {
        unsigned short to = r->matTo;

        r->matTo = saved;
        r->matFrom = to;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/bsp", bspVecTestIn__FP8_hierbspP8_fvectorT1);
#endif
int bspPointTest(_hierbsp *b, _fvector *pt)
{
    int ret = 0x200;

    while (b) {
        int neg = mathfPlaneTest(&b->plane, pt) < 0.0f;

        ret = neg ? b->matIdBack : b->matIdFront;
        b = neg ? b->backKid : b->frontKid;
    }
    return ret;
}
int bspVecTest(_hierbsp *b, _linesegment *ls, _bspresult *r)
{
    return bspVecTest(b, (_fvector *)ls, (_fvector *)((char *)ls + 0x10), r);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/29 words, size 0x74 vs 0x44 */
/* out.x / out.y: signed distances of a and b to the plane (retail leaves them in vf12 for bspInterp). */
void bspPlaneTest2Vec(_plane *pl, _fvector *a, _fvector *b, _fvector *out)
{
    out->x = pl->norm.x * a->x + pl->norm.y * a->y + pl->norm.z * a->z - pl->d;
    out->y = pl->norm.x * b->x + pl->norm.y * b->y + pl->norm.z * b->z - pl->d;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/bsp", bspPlaneTest2Vec__FP6_planeP8_fvectorN21);
#endif
INCLUDE_ASM("asm/nonmatchings/common/bsp", bspInterp__FP8_fvector);
#ifdef NON_MATCHING
/* equivalent (untuned): 14/18 words, size 0x48 vs 0x48 */
void bspTestChild(_hierbsp *b, short mat, _fvector *a, _fvector *c, _bspresult *r)
{
    if (!b) {
        r->matFrom = mat;
        r->hit = 0;
        r->mat2A = mat;
        r->matTo = mat;
    } else {
        bspVecTest(b, a, c, r);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/bsp", bspTestChild__FP8_hierbspsP8_fvectorT2P10_bspresult);
#endif
