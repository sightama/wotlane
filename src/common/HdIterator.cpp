#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "vecmath.h"

/* Iterators over the collision grid: HdItCell hands out the objects of one cell, HdItGridBox walks the cells under a box,
   HdItGridLine the cells a segment crosses. At each object the hull walk (common/hdTraverse) finds the hull nodes. */

extern unsigned g_traceId;
__asm__("#SNFIX_SMALL g_traceId");
extern _HierCollisionGrid *colGrid;
extern float hdItNoMin __asm__("D_006F8740"); // -FLT_MAX
extern float hdItNoMax __asm__("D_006F8744"); // FLT_MAX

extern "C" float ceilf(float);
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
int hdAABBVsAABB(float *amin, float *amax, float *bmin, float *bmax);
int hdSphereVsAABB(float *a, float *b, float r, float &tmin, float &tmax, float *min, float *max);
int hdTraversePath(_HierCollisionPath *path, hdTraverseState *st, unsigned short option);
int hdTraverseHier(hdTraverseState *st, unsigned short option);
void hdInitTraverseHier(float (*m)[4][4], _hierhead **ep);

HdItCell::HdItCell()
{
    traceId = g_traceId++;
    path = 0;
    csNode = 0;
    skipId2 = 0;
    withCs = 0;
    triggers = 0;
}

#ifdef NON_MATCHING
/* equivalent (untuned): 31/35 words, size 0x8c vs 0x8c */
void HdItCell::reset(int col, int row)
{
    int i;

    if (col < 0 || col >= colGrid->nCols || row < 0 || row >= colGrid->nRows) {
        csNode = 0;
        path = 0;
        return;
    }
    i = row * colGrid->nCols + col;
    path = colGrid->cells[i];
    if (withCs)
        csNode = colGrid->csListArr[i].head;
    next();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", reset__8HdItCellii);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 42/60 words, size 0xf0 vs 0xf0 */
/* Moves to the next object not handed out yet in this walk: a path of the wanted kind, then a collidable cs. */
void HdItCell::next(void)
{
    for (; path; path = path->next) {
        _HierCollisionPath *p = path->val;

        if (p->traceId == traceId)
            continue;
        if (triggers ? p->flags.triggerVol : !p->flags.triggerVol) {
            p->traceId = traceId;
            return;
        }
    }
    for (; csNode; csNode = csNode->next) {
        _cs *cs = csNode->cs;

        if (cs && cs->drawMe && cs->testCollision && cs->epNode && cs->epNode->id2 != skipId2 && cs->traceId != traceId) {
            cs->traceId = traceId;
            return;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", next__8HdItCell);
#endif

HdItGridLine::HdItGridLine() : option(0), pad28(0), tExit(0.0f), t(0.0f)
{
}

#ifdef NON_MATCHING
/* equivalent (untuned): 5/167 words, size 0x274 vs 0x29c */
void HdItGridLine::reset(_fvector &a, _fvector &b, float rad)
{
    FVECTOR d, gmin, gmax;

    from = a;
    to = b;
    r = rad;
    tExit = 1.0f;
    tEnter = 0.0f;
    gmin.x = colGrid->minX;
    gmin.y = colGrid->minY;
    gmin.z = hdItNoMin;
    gmax.x = colGrid->maxX;
    gmax.y = colGrid->maxY;
    gmax.z = hdItNoMax;
    if (!hdSphereVsAABB((float *)&from, (float *)&to, r, tEnter, tExit, (float *)&gmin, (float *)&gmax)) {
        t = tExit;
        return;
    }
    vecSub(&d, &to, &from);

    tMaxX = hdItNoMax;
    tDeltaX = 0.0f;
    stepX = 0;
    col = (int)((from.x + tEnter * d.x - colGrid->minX) / colGrid->cellSize);
    if (__builtin_fabsf(d.x) > 1e-7f) {
        tDeltaX = colGrid->cellSize / __builtin_fabsf(d.x);
        tMaxX = ((float)col * colGrid->cellSize + colGrid->minX - from.x) / d.x;
        if (0.0f < d.x)
            tMaxX += tDeltaX;
        stepX = 0.0f < d.x ? 1 : -1;
    }

    tMaxY = hdItNoMax;
    tDeltaY = 0.0f;
    stepY = 0;
    row = (int)((from.y + tEnter * d.y - colGrid->minY) / colGrid->cellSize);
    if (__builtin_fabsf(d.y) > 1e-7f) {
        tDeltaY = colGrid->cellSize / __builtin_fabsf(d.y);
        tMaxY = ((float)row * colGrid->cellSize + colGrid->minY - from.y) / d.y;
        if (0.0f < d.y)
            tMaxY += tDeltaY;
        stepY = 0.0f < d.y ? 1 : -1;
    }
    t = tEnter;
    next();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", reset__12HdItGridLineR8_fvectorT1f);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 24/96 words, size 0x174 vs 0x180 */
/* Moves to the next hull node in the current cell: the paths whose bbox the segment crosses, then the cs hierarchies
   (in the cs's space: the inverse of its matrix). Nonzero when one was found. */
int HdItGridLine::incrementPath(void)
{
    if (cell.isDone())
        cell.reset(col, row);
    else
        cell.next();
    while (!cell.isDone()) {
        if (cell.path) {
            _HierCollisionPath *p = cell.path->val;
            float t0 = tEnter;
            float t1 = tExit;

            if (hdSphereVsAABB((float *)&from, (float *)&to, r, t0, t1, (float *)&p->boundBoxMin, (float *)&p->boundBoxMax)
                && hdTraversePath(p, &state, option))
                return 1;
        } else {
            _cs *cs = cell.csNode->cs;
            QMat m;
            FVECTOR v;

            m = *(QMat *)&cs->mat;
            vecScale(&v, &cs->trans, -1.0f);
            mathfMulVec(m.m, &v, (_fvector *)m.m[3]);
            hdInitTraverseHier(&m.m, &cs->epNode);
            if (hdTraverseHier(&state, option))
                return 1;
        }
        cell.next();
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", incrementPath__12HdItGridLine);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 55/58 words, size 0xe8 vs 0xe8 */
/* Steps to the next cell along the segment until a hull node turns up or the segment ends. */
void HdItGridLine::next(void)
{
    while (t < tExit) {
        if (incrementPath())
            return;
        if (tMaxX < tMaxY) {
            t = tMaxX;
            tMaxX += tDeltaX;
            col += stepX;
        } else if (tMaxY < tMaxX) {
            t = tMaxY;
            tMaxY += tDeltaY;
            row += stepY;
        } else {
            /* both boundaries at once: tMaxX takes tMaxY's step as well (as in retail) */
            tMaxX = tMaxY + tDeltaY;
            col += stepX;
            row += stepY;
            t = tMaxY;
            tMaxY = tMaxX;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", next__12HdItGridLine);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 6/9 words, size 0x24 vs 0x24 */
int HdItGridLine::isDone(void) const
{
    return tExit <= t;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", isDone__C12HdItGridLine);
#endif

void HdItGridLine::drawGrid(void)
{
}

HdItGridBox::HdItGridBox() : pad20(0), option(0), maxRow(0), maxCol(0), row(0), col(0)
{
}

#ifdef NON_MATCHING
/* equivalent (untuned): 3/118 words, size 0x1d8 vs 0x174 */
void HdItGridBox::reset(_fvector &a, _fvector &b, float r)
{
    float inv;
    int v;

    min.x = (a.x < b.x ? a.x : b.x) - r;
    min.y = (a.y < b.y ? a.y : b.y) - r;
    min.z = (a.z < b.z ? a.z : b.z) - r;
    max.x = (a.x > b.x ? a.x : b.x) + r;
    max.y = (a.y > b.y ? a.y : b.y) + r;
    max.z = (a.z > b.z ? a.z : b.z) + r;
    inv = 1.0f / colGrid->cellSize;
    v = (int)((min.x - colGrid->minX) * inv);
    minCol = v > 0 ? v : 0;
    v = (int)((min.y - colGrid->minY) * inv);
    minRow = v > 0 ? v : 0;
    v = (int)ceilf((max.x - colGrid->minX) * inv);
    maxCol = v < colGrid->nCols ? v : colGrid->nCols;
    v = (int)ceilf((max.y - colGrid->minY) * inv);
    maxRow = v < colGrid->nRows ? v : colGrid->nRows;
    row = minRow;
    col = minCol;
    next();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", reset__11HdItGridBoxR8_fvectorT1f);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 24/89 words, size 0x158 vs 0x164 */
/* As HdItGridLine::incrementPath, with the paths picked by bbox overlap. */
int HdItGridBox::incrementPath(void)
{
    if (cell.isDone())
        cell.reset(col, row);
    else
        cell.next();
    while (!cell.isDone()) {
        if (cell.path) {
            _HierCollisionPath *p = cell.path->val;

            if (hdAABBVsAABB((float *)&min, (float *)&max, (float *)&p->boundBoxMin, (float *)&p->boundBoxMax)
                && hdTraversePath(p, &state, option))
                return 1;
        } else {
            _cs *cs = cell.csNode->cs;
            QMat m;
            FVECTOR v;

            m = *(QMat *)&cs->mat;
            vecScale(&v, &cs->trans, -1.0f);
            mathfMulVec(m.m, &v, (_fvector *)m.m[3]);
            hdInitTraverseHier(&m.m, &cs->epNode);
            if (hdTraverseHier(&state, option))
                return 1;
        }
        cell.next();
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", incrementPath__11HdItGridBox);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 23/30 words, size 0x74 vs 0x78 */
/* Walks the cells row by row until a hull node turns up. */
void HdItGridBox::next(void)
{
    while (row < maxRow) {
        if (incrementPath())
            return;
        if (++col >= maxCol) {
            col = minCol;
            row++;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/HdIterator", next__11HdItGridBox);
#endif
