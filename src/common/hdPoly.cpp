#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "vecmath.h"

/* Swept sphere against the polygons of a level collision object: a quantized bbox tree picks the candidate polygons,
   then each triangle is tested as a face (hdSphereVsPoly) and, failing that, by its edges (hdSphereVsEdge). */

extern int s_checkBackface;
extern float hdEdgeContactW __asm__("D_006F8750");
extern float hdFaceContactW __asm__("D_006F8754");

int hdSphereVsEdge(_fvector *c, _fvector *dir, float r, _fvector *bmin, _fvector *bmax, float tmax, _fvector *e0, _fvector *e1, ContactInfo *ci);
int hdSphereVsPoly(_fvector *c, _fvector *c2, _fvector *dir, float r, _fvector *bmin, _fvector *bmax, _fvector *tri, ContactInfo *ci, float tmax);

#ifdef NON_MATCHING
static inline float dot3(_fvector *a, _fvector *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}
static inline void vecMadd(_fvector *dst, _fvector *a, _fvector *b, float s)
{
    dst->x = a->x + b->x * s;
    dst->y = a->y + b->y * s;
    dst->z = a->z + b->z * s;
}
/* p inside triangle tri (the cross products of the vertex vectors around p all point the same way) */
static inline int insideTri(_fvector *p, _fvector *tri)
{
    FVECTOR a, b, c, c0, c1, c2;

    vecSub(&a, p, &tri[0]);
    vecSub(&b, p, &tri[1]);
    vecSub(&c, p, &tri[2]);
    vecCross(&c0, &a, &b);
    vecCross(&c1, &b, &c);
    vecCross(&c2, &c, &a);
    return 0.0f < dot3(&c0, &c1) && 0.0f < dot3(&c0, &c2);
}
#endif

void hdPolyCheckBackface(bool on)
{
    s_checkBackface = on;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 5/126 words, size 0x1f8 vs 0x1b0 */
/* Static contact of sphere p (radius r) with the segment from a along dir (len = |dir|^2, t = (p - a).dir): fails
   past the far end; before the near end the contact is with a itself (d = p - a). */
int edgePunt(_fvector &a, _fvector &dir, _fvector &d, float len, float t, _fvector &p, float r, ContactInfo *ci)
{
    float r2, l2, l, k;
    FVECTOR q;

    if (len <= t)
        return 0;
    r2 = r * r;
    if (t < 0.0f) {
        l2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (r2 < l2)
            return 0;
        l = eeSqrtf(l2);
        k = 1.0f / l;
        ci->normal.x = d.x * k;
        ci->normal.z = d.z * k;
        ci->normal.y = d.y * k;
        ci->depth = r - l + 0.01f;
        vecMadd(&ci->point, &a, &ci->normal, 0.01f);
    } else {
        vecMadd(&q, &a, &dir, t / len);
        vecSub(&ci->normal, &p, &q);
        l2 = ci->normal.x * ci->normal.x + ci->normal.y * ci->normal.y + ci->normal.z * ci->normal.z;
        if (r2 < l2)
            return 0;
        l = eeSqrtf(l2);
        k = 1.0f / l;
        ci->normal.x *= k;
        ci->normal.z *= k;
        ci->normal.y *= k;
        ci->depth = r - l + 0.01f;
        vecMadd(&ci->point, &q, &ci->normal, 0.01f);
    }
    ci->point.w = hdEdgeContactW;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPoly", edgePunt__FR8_fvectorN20ffT0fP11ContactInfo);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 22/320 words, size 0x500 vs 0x3f4 */
/* Smallest root of a t^2 + b t + c = 0 when real, else -1 (the callers then fail). */
static inline int sweptRoot(float a, float b, float c, float *t)
{
    float disc = b * b - a * 4.0f * c;
    float s, t1, t2;

    if (disc <= 0.0f)
        return 0;
    s = eeSqrtf(disc);
    t1 = (-b - s) / (a + a);
    t2 = (-b + s) / (a + a);
    *t = t2 < t1 ? t2 : t1;
    return 1;
}
/* Sphere c moving by dir (0 <= t <= tmax) against segment e0-e1 (bmin/bmax: the swept box). A resting contact is
   handled by edgePunt; otherwise the first time the sphere touches the segment's line inside the segment, or the
   vertex e0. The moving case only fills ci->point (w = t). */
int hdSphereVsEdge(_fvector *c, _fvector *dir, float r, _fvector *bmin, _fvector *bmax, float tmax, _fvector *e0, _fvector *e1, ContactInfo *ci)
{
    FVECTOR rel, edge, q;
    float re, ee, ev, rv, rr, vv, r2, a, b, cc, t, u;
    int k;

    for (k = 0; k < 3; k++) {
        float p0 = ((float *)e0)[k];
        float p1 = ((float *)e1)[k];
        float mx = ((float *)bmax)[k];

        if (p0 < p1) {
            if (mx < p0 || p1 < ((float *)bmin)[k])
                return 0;
        } else {
            if (mx < p1 || p0 < ((float *)bmin)[k])
                return 0;
        }
    }
    vecSub(&rel, c, e0);
    vecSub(&edge, e1, e0);
    re = dot3(&rel, &edge);
    ee = dot3(&edge, &edge);
    if (edgePunt(*e0, edge, rel, ee, re, *c, r, ci))
        return 1;
    ev = dot3(&edge, dir);
    rv = dot3(&rel, dir);
    rr = dot3(&rel, &rel);
    vv = dot3(dir, dir);
    r2 = r * r;
    a = ev * ev - ee * vv;
    b = (re * ev - rv * ee) * 2.0f;
    cc = re * re + r2 * ee - rr * ee;
    if (0.0001f < (a < 0.0f ? -a : a)) {
        if (!sweptRoot(a, b, cc, &t))
            return 0;
        if (t < 0.0f || tmax < t)
            return 0;
        vecMadd(&q, c, dir, t);
        vecSub(&q, &q, e0);
        u = dot3(&q, &edge) / ee;
        if (0.0f <= u && u <= 1.0f) {
            vecMadd(&ci->point, e0, &edge, u);
            ci->point.w = t;
            return 1;
        }
    }
    cc = rr - r2;
    if ((vv < 0.0f ? -vv : vv) < 0.0001f)
        return 0;
    if (!sweptRoot(vv, rv + rv, cc, &t))
        return 0;
    if (t < 0.0f || tmax < t)
        return 0;
    ci->point = *e0;
    ci->point.w = t;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPoly", hdSphereVsEdge__FP8_fvectorT0fT0T0fT0T0P11ContactInfo);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/485 words, size 0x794 vs 0x62c */
/* Sphere c (radius r) moving to c2 (dir = c2 - c) against triangle tri. Back faces are skipped unless s_checkBackface
   (then the triangle is flipped in place). Face contact first, then the three edges; ci->point.w is the time. */
int hdSphereVsPoly(_fvector *c, _fvector *c2, _fvector *dir, float r, _fvector *bmin, _fvector *bmax, _fvector *tri, ContactInfo *ci, float tmax)
{
    FVECTOR n, e0, e1, q;
    ContactInfo edges[3];
    ContactInfo *best;
    float k, dist, d0, d1, t, eps;
    int flipped = 0;
    int i;

    vecSub(&e1, &tri[2], &tri[1]);
    vecSub(&e0, &tri[0], &tri[1]);
    vecCross(&n, &e1, &e0);
    k = 1.0f / eeSqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    n.z *= k;
    n.x *= k;
    n.y *= k;
    n.w = dot3(&n, &tri[1]);
    dist = dot3(c, &n) - n.w;
    if (dist < 0.0f) {
        if (!s_checkBackface)
            return 0;
        q = tri[0];
        tri[0] = tri[2];
        tri[2] = q;
        n.x = -n.x;
        n.y = -n.y;
        n.z = -n.z;
        n.w = 1.0f - n.w; /* retail negates with vf0, whose w is 1 */
        dist = -dist;
        flipped = 1;
    }
    d0 = dist - r;
    if (d0 <= 0.0f) {
        ci->depth = -d0 + 0.01f;
        vecMadd(&ci->point, c, &n, ci->depth - r);
        if (insideTri(&ci->point, tri)) {
            ci->normal = n;
            ci->point.w = hdFaceContactW;
            return 1;
        }
    } else {
        d1 = dot3(c2, &n) - n.w - r;
        if (0.0f < d1)
            return 0;
        t = d0 / (d0 - d1);
        eps = 0.01f / (d0 - d1);
        if (t < 0.0f || 1.0f < t)
            return 0;
        if (tmax < t - eps)
            return 0;
        d0 = t - eps;
        vecMadd(&q, c, &n, -r);
        vecMadd(&ci->point, &q, dir, t);
        if (insideTri(&ci->point, tri)) {
            if (flipped)
                vecScale(&ci->normal, &n, -1.0f);
            else
                ci->normal = n;
            ci->depth = -d1 + 0.01f;
            t = d0 > 0.0f ? d0 : 0.0f;
            vecMadd(&ci->point, &q, dir, t);
            ci->point.w = t;
            return 1;
        }
    }
    if (r == 0.0f)
        return 0;
    best = 0;
    for (i = 0; i < 3; i++) {
        ContactInfo *e = &edges[i];

        if (!hdSphereVsEdge(c, dir, r, bmin, bmax, tmax, &tri[i], &tri[(i + 1) % 3], e))
            continue;
        if (best && !(e->point.w < best->point.w))
            continue;
        best = e;
        tmax = best->point.w > 0.0f ? best->point.w : 0.0f;
    }
    if (!best)
        return 0;
    if (0.0f <= best->point.w) {
        vecMadd(&q, c, dir, best->point.w);
        vecSub(&ci->normal, &q, &best->point);
        vecScale(&ci->normal, &ci->normal, 1.0f / r);
        ci->point = best->point;
        return 1;
    }
    ci->point = best->point;
    ci->normal = best->normal;
    *(FVECTOR *)&ci->depth = *(FVECTOR *)&best->depth;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPoly", hdSphereVsPoly__FP8_fvectorN20fT0N20P11ContactInfof);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/398 words, size 0x638 vs 0x5a0 */
static inline int clampShort(int v)
{
    if (0x7FFF < v)
        v = 0x7FFF;
    if (!(-0x8001 < v))
        v = -0x8000;
    return v;
}
/* Sphere a -> b of radius r against object o (bmin/bmax: the swept box). Keeps the earliest contacts (several when they
   all start inside), at most maxRes; returns how many (maxRes if there were more). */
int hdSphereVsPolyCollis(_fvector *a, _fvector *b, float r, _fvector *bmin, _fvector *bmax, _hierpolycollisionobject *o, _hdResult *res, int maxRes)
{
    unsigned short list[0x200];
    unsigned short stack[64];
    unsigned short *sp;
    short qmin[3], qmax[3];
    int nb = o->numBBoxes;
    int numPolys, depth, i, n;
    _hiercollisionpoly *polys;
    FVECTOR3 *verts;
    FVECTOR dir;
    float tmax;

    if (nb) {
        float s = o->bboxScale;
        int count = 0;

        qmin[0] = clampShort((int)((bmin->x - o->bboxOrigin.x) * s) - 1);
        qmin[1] = clampShort((int)((bmin->y - o->bboxOrigin.y) * s) - 1);
        qmin[2] = clampShort((int)((bmin->z - o->bboxOrigin.z) * s) - 1);
        qmax[0] = clampShort((int)((bmax->x - o->bboxOrigin.x) * s) + 1);
        qmax[1] = clampShort((int)((bmax->y - o->bboxOrigin.y) * s) + 1);
        qmax[2] = clampShort((int)((bmax->z - o->bboxOrigin.z) * s) + 1);
        /* depth-first: a node's two children are consecutive; the first is pushed, the second visited at once */
        sp = stack;
        sp[0] = 0;
        depth = 1;
        while (depth) {
            int idx;

            depth--;
            idx = *sp;
            for (;;) {
                _hdpolybboxnode *nd = &o->bbox[idx];
                unsigned bits;

                if (qmax[0] < nd->minBox[0] || qmax[1] < nd->minBox[1] || qmax[2] < nd->minBox[2] ||
                    nd->maxBox[0] < qmin[0] || nd->maxBox[1] < qmin[1] || nd->maxBox[2] < qmin[2])
                    break;
                bits = *(unsigned short *)((char *)nd + 0xC);
                if (bits & 1) {
                    int first = (bits >> 5) & 0x1FF;
                    int k;

                    for (k = (bits >> 1) & 0xF; k; k--)
                        list[count++] = first++;
                    break;
                }
                *sp++ = (bits >> 5) & 0x1FF;
                idx = ((bits >> 5) & 0x1FF) + 1;
                depth++;
            }
            if (depth)
                sp--;
        }
        numPolys = count;
    } else {
        numPolys = o->numPolys;
    }
    if (!numPolys)
        return 0;
    n = 0;
    polys = (_hiercollisionpoly *)((char *)o + 0x18 + nb * 14);
    verts = o->vertexPool;
    vecSub(&dir, b, a);
    tmax = 1.0f;
    for (i = 0; i < numPolys; i++) {
        _hiercollisionpoly *p = nb ? &polys[list[i]] : &polys[i];
        unsigned bb = *(unsigned short *)((char *)p + 6);
        FVECTOR tri[3];
        ContactInfo ci;
        _hdResult *rr;
        float t;

        if (bmax->x < verts[p->idx[bb & 3]].x || verts[p->idx[(bb >> 6) & 3]].x < bmin->x ||
            bmax->y < verts[p->idx[(bb >> 2) & 3]].y || verts[p->idx[(bb >> 8) & 3]].y < bmin->y ||
            bmax->z < verts[p->idx[(bb >> 4) & 3]].z || verts[p->idx[(bb >> 10) & 3]].z < bmin->z)
            continue;
        tri[0].x = verts[p->idx[0]].x;
        tri[0].y = verts[p->idx[0]].y;
        tri[0].z = verts[p->idx[0]].z;
        tri[1].x = verts[p->idx[1]].x;
        tri[1].y = verts[p->idx[1]].y;
        tri[1].z = verts[p->idx[1]].z;
        tri[2].x = verts[p->idx[2]].x;
        tri[2].y = verts[p->idx[2]].y;
        tri[2].z = verts[p->idx[2]].z;
        if (!hdSphereVsPoly(a, b, &dir, r, bmin, bmax, tri, &ci, tmax))
            continue;
        t = ci.point.w;
        if (t <= 0.0f && tmax <= 0.0f)
            n++;
        else if (t < tmax)
            n = 1;
        else
            continue;
        if (maxRes < n)
            return maxRes;
        rr = &res[n - 1];
        rr->hit = 1;
        rr->surface = p->matId;
        rr->depth = ci.depth;
        rr->normal = ci.normal;
        rr->point = ci.point;
        tmax = t > 0.0f ? t : 0.0f;
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPoly", hdSphereVsPolyCollis__FP8_fvectorT0fT0T0P24_hierpolycollisionobjectP9_hdResulti);
#endif
