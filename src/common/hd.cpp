#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "vecmath.h"

/* Collision against the level: the cs grid (cells of ColGridCsList), hull nodes (opcode 10) and polygon collision
   groups (opcode 42). */

/* Hull node (opcode 10): a convex hull as numPlanes planes. */
struct HdHullNode {
    _hierhead head;         // 0x0
    short numPlanes;        // 0x4
    short pad6;             // 0x6
    unsigned short surface; // 0x8
    unsigned short padA;    // 0xA
    unsigned padC[5];       // 0xC
    _plane planes[1];       // 0x20
};
/* Polygon collision group (opcode 42). */
struct HdPolyGroup {
    _hierhead head;                         // 0x0
    unsigned numObjs;                       // 0x4
    _hierpolycollisionobject *objs[1];      // 0x8
};
/* cses whose collision is switched off for a while (hdIgnore / hdClearIgnore) */
struct HdIgnoreList {
    _cs *cs[32];
    int count;
};

extern _HierCollisionGrid *colGrid;
extern _hiergroup hdNullEpNode __asm__("D_00764788");
extern HdIgnoreList hdIgnored __asm__("D_00764700");
extern int hdTraceId;
extern int csEpNodeNull;
__asm__("#SNFIX_SMALL colGrid");
__asm__("#SNFIX_SMALL hdTraceId");
__asm__("#SNFIX_SMALL csEpNodeNull");

void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
int hdSphereVsHull(_fvector *a, _fvector *b, float r, _plane *planes, int n, ContactInfo *ci, float *t0, float *t1);
int hdSphereVsPolyCollis(_fvector *a, _fvector *b, float r, _fvector *min, _fvector *max, _hierpolycollisionobject *o, _hdResult *res, int max_);
void hdGetSphereGridBox(float x, float y, float r, int *minRow, int *minCol, int *maxRow, int *maxCol);
void hdReparentCsGrid(_cs *cs, _fvector *pos, float r);

#ifdef NON_MATCHING
/* equivalent (untuned): 18/28 words, size 0x70 vs 0x70 */
/* A dummy group used as the ep node of a cs that has none while hdCsTest runs. */
void hdInit(void)
{
    hdNullEpNode.head.opcode = 1;
    hdNullEpNode.head.id1 = 1;
    hdNullEpNode.head.id2 = 0;
    hdNullEpNode.sphereRadius = 10.0f;
    hdNullEpNode.sphereRadiusSqrd = 100.0f;
    hdNullEpNode.ctx = 0;
    hdNullEpNode.numKids = 0;
    hdNullEpNode.flags = 0;
    hdNullEpNode.sphereCenter.x = 0.0f;
    hdNullEpNode.sphereCenter.y = 0.0f;
    hdNullEpNode.sphereCenter.z = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdInit__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 24/225 words, size 0x378 vs 0x384 */
/* Sweeps a sphere of radius r from p0 to p1 (in the node's parent space, m is the node's matrix) against a hull or a
   polygon group; fills up to maxRes results and returns how many, with point and normal back in m's space. */
int hdTestNode(_hierhead *node, _fvector *p0, _fvector *p1, float r, float (*m)[4][4], _hdResult *res, int maxRes)
{
    FVECTOR a, b, t;
    FVECTOR box[2];
    ContactInfo ci;
    int n = 0;
    int i;

    mathfMulVec(*m, p0, &a);
    vecAdd(&a, &a, (_fvector *)(*m)[3]);
    mathfMulVec(*m, p1, &b);
    vecAdd(&b, &b, (_fvector *)(*m)[3]);
    switch (node->opcode) {
    case 10: {
        HdHullNode *h = (HdHullNode *)node;

        if (hdSphereVsHull(&a, &b, r, h->planes, h->numPlanes, &ci, 0, 0)) {
            res->hit = 1;
            n = 1;
            res->surface = h->surface;
            res->depth = ci.depth;
            res->normal = ci.normal;
            res->point = ci.point;
            HD_RESULT_SET_IDS(res, node->id1, node->id2);
        }
        break;
    }
    case 42: {
        HdPolyGroup *g = (HdPolyGroup *)node;
        unsigned k;

        box[0].x = (a.x < b.x ? a.x : b.x) - r;
        box[0].y = (a.y < b.y ? a.y : b.y) - r;
        box[0].z = (a.z < b.z ? a.z : b.z) - r;
        box[1].x = (a.x > b.x ? a.x : b.x) + r;
        box[1].y = (a.y > b.y ? a.y : b.y) + r;
        box[1].z = (a.z > b.z ? a.z : b.z) + r;
        for (k = 0; k < g->numObjs; k++)
            n += hdSphereVsPolyCollis(&a, &b, r, &box[0], &box[1], g->objs[k], &res[n], maxRes - n);
        for (i = 0; i < n; i++)
            HD_RESULT_SET_IDS(&res[i], node->id1, node->id2);
        break;
    }
    }
    for (i = 0; i < n; i++) {
        float w = res[i].point.w;

        vecSub(&t, &res[i].point, (_fvector *)(*m)[3]);
        vecApplyRows(&res[i].point, *m, &t);
        res[i].point.w = w;
        w = res[i].normal.w;
        t = res[i].normal;
        vecApplyRows(&res[i].normal, *m, &t);
        res[i].normal.w = w;
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdTestNode__FP9_hierheadP8_fvectorT1fPA3_A3_fP9_hdResulti);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/277 words, size 0x3c0 vs 0x454 */
/* Tests the points of a cs, swept from (m, pos) to its current placement, against the hulls of the grid cells around it.
   res[i] is the deepest-first contact of point i (a shallower later hit does not replace it). */
void hdCsTest(_hdCsTestPts *pts, _fvector *pos, float (*m)[4][4], _hdResult *res)
{
    _cs *cs = pts->cs;
    HdItGridBox it;
    float r;
    int i;

    if (!cs->epNode) {
        csEpNodeNull = 1;
        cs->epNode = &hdNullEpNode.head;
    }
    cs = pts->cs;
    for (i = 0; i < pts->numPts; i++)
        res[i].hit = 0;
    r = 300.0f;
    if (cs->epNode && (cs->epNode->opcode == 2 || cs->epNode->opcode == 1))
        r = ((_hiergroup *)cs->epNode)->sphereRadius;
    it.cell.withCs = 1;
    it.cell.skipId2 = pts->cs->epNode->id2;
    it.reset(*pos, cs->trans, r);
    while (it.row < it.maxRow) {
        for (i = 0; i < pts->numPts; i++) {
            float (*M)[4] = it.state.mat;
            HdHullNode *h;
            FVECTOR p, a, b;
            ContactInfo ci;
            _hdResult *rr;

            if (pts->done[i])
                continue;
            vecApplyRows(&p, *m, &pts->pts[i]);
            vecAdd(&p, pos, &p);
            mathfMulVec(M, &p, &a);
            vecAdd(&a, &a, (_fvector *)M[3]);
            vecApplyRows(&p, cs->mat, &pts->pts[i]);
            vecAdd(&p, &cs->trans, &p);
            mathfMulVec(M, &p, &b);
            vecAdd(&b, &b, (_fvector *)M[3]);
            h = (HdHullNode *)it.state.hull;
            if (!hdSphereVsHull(&a, &b, 0.0f, h->planes, h->numPlanes, &ci, 0, 0))
                continue;
            rr = &res[i];
            if (rr->hit && rr->depth < ci.depth)
                continue;
            vecApplyRows(&rr->normal, M, &ci.normal);
            vecSub(&p, &ci.point, (_fvector *)M[3]);
            vecApplyRows(&rr->point, M, &p);
            rr->depth = ci.depth;
            if (h->head.id1)
                HD_RESULT_SET_IDS(rr, h->head.id1, h->head.id2);
            else
                HD_RESULT_SET_IDS(rr, it.state.hatId1, it.state.hatId2);
            rr->hit = 1;
            rr->surface = h->surface;
        }
        it.next();
    }
    if (csEpNodeNull == 1) {
        csEpNodeNull = 0;
        pts->cs->epNode = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdCsTest__FP12_hdCsTestPtsP8_fvectorPA3_A3_fP9_hdResult);
#endif
/* Sets the grid used by the cs functions; the first grid set wins until it is cleared with 0. */
void hdSaveColGrid(_HierCollisionGrid *g)
{
    if (!g)
        colGrid = 0;
    else if (!colGrid)
        colGrid = g;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 5/50 words, size 0xc8 vs 0xc4 */
/* The cells (clamped to the grid) covered by the square of half-side r around (x, y). */
void hdGetSphereGridBox(float x, float y, float r, int *minRow, int *minCol, int *maxRow, int *maxCol)
{
    float inv = 1.0f / colGrid->cellSize;
    int v;

    v = (int)((x - r - colGrid->minX) * inv);
    if (v <= 0)
        v = 0;
    *minCol = v;
    v = (int)((y - r - colGrid->minY) * inv);
    if (v <= 0)
        v = 0;
    *minRow = v;
    v = (int)((x + r - colGrid->minX) * inv);
    *maxCol = v < colGrid->nCols - 1 ? v : colGrid->nCols - 1;
    v = (int)((y + r - colGrid->minY) * inv);
    *maxRow = v < colGrid->nRows - 1 ? v : colGrid->nRows - 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdGetSphereGridBox__FfffPiN33);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 116/122 words, size 0x1e8 vs 0x1e8 */
static inline ColGridCsList *gridCell(int row, int col)
{
    return &colGrid->csListArr[row * colGrid->nCols + col];
}
/* Moves cs to the cells around pos: leaves the cells of its old box that are not in the new one, joins the new ones. */
void hdReparentCsGrid(_cs *cs, _fvector *pos, float r)
{
    int minRow, minCol, maxRow, maxCol;
    int row, col;

    hdGetSphereGridBox(pos->x, pos->y, r, &minRow, &minCol, &maxRow, &maxCol);
    for (row = cs->minCellRow; row <= cs->maxCellRow; row++)
        for (col = cs->minCellCol; col <= cs->maxCellCol; col++)
            if (row < minRow || maxRow < row || col < minCol || maxCol < col)
                gridCell(row, col)->removeCs(cs);
    for (row = minRow; row <= maxRow; row++)
        for (col = minCol; col <= maxCol; col++)
            if (row < cs->minCellRow || cs->maxCellRow < row || col < cs->minCellCol || cs->maxCellCol < col)
                gridCell(row, col)->pushCsFront(cs);
    cs->minCellRow = minRow;
    cs->maxCellCol = maxCol;
    cs->maxCellRow = maxRow;
    cs->minCellCol = minCol;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdReparentCsGrid__FP3_csP8_fvectorf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/19 words, size 0x44 vs 0x4c */
void hdReparentCsGrid(_cs *cs)
{
    _hierhead *ep = cs->epNode;
    float r;

    switch (ep->opcode) {
    case 1:
    case 2:
        r = ((_hiergroup *)ep)->sphereRadius;
        break;
    default:
        r = 30.0f;
        break;
    }
    hdReparentCsGrid(cs, &cs->trans, r);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdReparentCsGrid__FP3_cs);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 39/49 words, size 0xc4 vs 0xc4 */
void hdRemoveCsFromGrid(_cs *cs)
{
    int row, col;

    for (row = cs->minCellRow; row <= cs->maxCellRow; row++)
        for (col = cs->minCellCol; col <= cs->maxCellCol; col++)
            gridCell(row, col)->removeCs(cs);
    cs->maxCellRow = 0x80000001;
    cs->minCellRow = 0x7FFFFFFF;
    cs->minCellCol = 0x7FFFFFFF;
    cs->maxCellCol = 0x80000001;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdRemoveCsFromGrid__FP3_cs);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 62/111 words, size 0x1bc vs 0x1b4 */
/* Collects up to max cses (each once per call, via traceId) whose origin is within r of pos and whose hat id1 is in
   [idMin, idMax]; hdCsCollect2D measures the distance in x/y only. */
int hdCsCollect(_fvector *pos, float r, unsigned idMin, unsigned idMax, _cs **out, int max)
{
    int minRow, minCol, maxRow, maxCol;
    int n = 0;
    float r2;

    hdTraceId++;
    hdGetSphereGridBox(pos->x, pos->y, r, &minRow, &minCol, &maxRow, &maxCol);
    r2 = r * r;
    for (; minRow <= maxRow; minRow++) {
        ColGridCsList *l = gridCell(minRow, minCol);
        int k;

        for (k = maxCol - minCol; k >= 0; k--, l++) {
            ColGridCsListNode *nd;

            for (nd = l->head; nd; nd = nd->next) {
                _cs *cs = nd->cs;
                unsigned id;
                FVECTOR d;

                if (!cs || !cs->epNode || cs->traceId == (unsigned)hdTraceId)
                    continue;
                id = cs->epNode->id1;
                if (id < idMin || idMax < id)
                    continue;
                vecSub(&d, pos, &cs->trans);
                if (!(d.x * d.x + d.y * d.y + d.z * d.z < r2))
                    continue;
                cs->traceId = hdTraceId;
                *out++ = cs;
                if (++n >= max)
                    return n;
            }
        }
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdCsCollect__FP8_fvectorfUiUiPP3_csi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 63/108 words, size 0x1b0 vs 0x1ac */
int hdCsCollect2D(_fvector *pos, float r, unsigned idMin, unsigned idMax, _cs **out, int max)
{
    int minRow, minCol, maxRow, maxCol;
    int n = 0;
    float r2;

    hdTraceId++;
    hdGetSphereGridBox(pos->x, pos->y, r, &minRow, &minCol, &maxRow, &maxCol);
    r2 = r * r;
    for (; minRow <= maxRow; minRow++) {
        ColGridCsList *l = gridCell(minRow, minCol);
        int k;

        for (k = maxCol - minCol; k >= 0; k--, l++) {
            ColGridCsListNode *nd;

            for (nd = l->head; nd; nd = nd->next) {
                _cs *cs = nd->cs;
                unsigned id;
                FVECTOR d;

                if (!cs || !cs->epNode || cs->traceId == (unsigned)hdTraceId)
                    continue;
                id = cs->epNode->id1;
                if (id < idMin || idMax < id)
                    continue;
                vecSub(&d, pos, &cs->trans);
                if (!(d.x * d.x + d.y * d.y < r2))
                    continue;
                cs->traceId = hdTraceId;
                *out++ = cs;
                if (++n >= max)
                    return n;
            }
        }
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdCsCollect2D__FP8_fvectorfUiUiPP3_csi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 40/43 words, size 0xac vs 0xac */
/* The next contact after prev in (distance, address) order over the results of all tests; idx gets its test. */
_hdResult *hdGetNextCollis(int &idx, _hdSphereBundleTest *tests, int n, _hdResult *prev)
{
    _hdResult *best = 0;
    int i;

    for (i = 0; i < n; i++, tests++) {
        _hdResult *r = tests->results;
        int k;

        for (k = tests->numResults; k; k--, r++) {
            if (best && !(r->point.w < best->point.w))
                continue;
            if (!prev || prev->point.w < r->point.w || (prev->point.w == r->point.w && prev < r)) {
                idx = i;
                best = r;
            }
        }
    }
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdGetNextCollis__FRiP19_hdSphereBundleTestiP9_hdResult);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 27/30 words, size 0x78 vs 0x78 */
_hdResult *hdGetNextCollis(_hdResult *r, int n, _hdResult *prev)
{
    _hdResult *best = 0;

    for (; n; n--, r++) {
        if (best && !(r->point.w < best->point.w))
            continue;
        if (!prev || prev->point.w < r->point.w || (prev->point.w == r->point.w && prev < r))
            best = r;
    }
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdGetNextCollis__FP9_hdResultiT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 6/14 words, size 0x38 vs 0x38 */
void hdIgnore(_cs *cs)
{
    if (cs->testCollision) {
        hdIgnored.cs[hdIgnored.count++] = cs;
        cs->testCollision = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hd", hdIgnore__FP3_cs);
#endif
void hdClearIgnore(void)
{
    int n;

    for (n = hdIgnored.count; n; n--)
        hdIgnored.cs[--hdIgnored.count]->testCollision = 1;
}
INCLUDE_ASM("asm/nonmatchings/common/hd", __static_initialization_and_destruction_0_001FE940);
INCLUDE_ASM("asm/nonmatchings/common/hd", _GLOBAL_$I$colGrid);
