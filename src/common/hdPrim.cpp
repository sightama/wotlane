#include "common.h"
#include "hieri_types.h"
#include "hd.h"
#include "vecmath.h"

/* Primitive collision tests: boxes, spheres, convex hulls (planes), capsules. */

extern float hdNoHitMin __asm__("D_006F8758"); // -FLT_MAX
extern float hdNoHitMax __asm__("D_006F875C"); // FLT_MAX
extern float hdHullFar __asm__("D_006F8760");
extern float hdHullInside __asm__("D_006F8764");

void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void mathfMulTransVecOld(float (*m)[4], _fvector *v, _fvector *out);
int hdSphereVsAABB(_fvector *c, float r, _fvector *a, _fvector *b, ContactInfo *ci);
int hdSphereVsAABB(float *p, float rsq, float *min, float *max);
int hdSphereVsAABB(float *p, float r, float *min, float *max, ContactInfo *ci);
int hdSphereVsHull(_fvector *c, float r, _plane *planes, int n);
int hdSphereVsHull(_fvector *c, float r, _plane *planes, int n, ContactInfo *ci);
int hdSphereVsHull(_fvector *a, _fvector *b, float r, _plane *planes, int n, ContactInfo *ci, float *t0, float *t1);

#ifdef NON_MATCHING
static inline float dot3(_fvector *a, _fvector *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}
/* dst = a + b * s (xyz) */
static inline void vecAddScaled(_fvector *dst, _fvector *a, _fvector *b, float s)
{
    register int t __asm__("$2");

    __asm__ volatile(".set push\n\t.set noreorder\n\t"
                     "lqc2 $vf11, 0x0(%2)\n\t"
                     "lqc2 $vf12, 0x0(%3)\n\t"
                     "mfc1 %1, %4\n\t"
                     "qmtc2.ni %1, $vf13\n\t"
                     "vaddax.xyz $ACC, $vf11, $vf0x\n\t"
                     "vmaddx.xyz $vf11, $vf12, $vf13x\n\t"
                     "sqc2 $vf11, %0\n\t"
                     ".set pop"
                     : "=m"(*dst), "=r"(t) : "r"(a), "r"(b), "f"(s));
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 3/40 words, size 0xa0 vs 0x9c */
int hdAABBVsAABB(float *amin, float *amax, float *bmin, float *bmax)
{
    if (bmax[0] < amin[0] || amax[0] < bmin[0] || bmax[1] < amin[1] || amax[1] < bmin[1] || bmax[2] < amin[2] || amax[2] < bmin[2])
        return 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdAABBVsAABB__FPfN30);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 27/60 words, size 0xec vs 0xf0 */
/* Sphere against a box with orientation m: the center goes into the box frame, and the contact back out. */
int hdSphereVsOBB(_fvector *c, float r, _fvector *pos, float (*m)[4], _fvector *ext, ContactInfo *ci)
{
    FVECTOR t;

    mathfMulVec(m, c, &t);
    vecSub(&t, &t, pos);
    if (ci) {
        if (!hdSphereVsAABB(&t, r, pos, ext, ci))
            return 0;
        mathfMulTransVecOld(m, &ci->point, &t);
        vecAdd(&ci->point, &t, pos);
        mathfMulTransVecOld(m, &ci->normal, &t);
        ci->normal = t;
        return 1;
    }
    return hdSphereVsAABB(&t, r, pos, ext, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsOBB__FP8_fvectorfT0PA3_fT0P11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 16/22 words, size 0x58 vs 0x58 */
int hdSphereVsAABB(_fvector *c, float r, _fvector *a, _fvector *b, ContactInfo *ci)
{
    FVECTOR lo, hi;

    vecAdd(&lo, a, b);
    vecSub(&hi, a, b);
    if (!ci)
        return hdSphereVsAABB((float *)c, r * r, (float *)&lo, (float *)&hi);
    return hdSphereVsAABB((float *)c, r, (float *)&lo, (float *)&hi, ci);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsAABB__FP8_fvectorfT0T0P11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 21/28 words, size 0x70 vs 0x70 */
/* Squared distance from p to the box against rsq. */
int hdSphereVsAABB(float *p, float rsq, float *min, float *max)
{
    float d2 = 0.0f;
    float d;
    int i;

    for (i = 0; i < 3; i++) {
        if (p[i] < min[i]) {
            d = p[i] - min[i];
            d2 += d * d;
        } else if (max[i] < p[i]) {
            d = p[i] - max[i];
            d2 += d * d;
        }
    }
    if (rsq < d2)
        return 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsAABB__FPffT0T0);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 1/87 words, size 0x15c vs 0x144 */
int hdSphereVsAABB(float *p, float r, float *min, float *max, ContactInfo *ci)
{
    FVECTOR d = {0};
    float l2, len;
    int i;

    ci->point = *(FVECTOR *)p;
    for (i = 0; i < 3; i++) {
        if (p[i] < min[i])
            (&d.x)[i] += p[i] - min[i];
        else if (max[i] < p[i])
            (&d.x)[i] += p[i] - max[i];
    }
    l2 = vecLenSq(&d);
    if (r * r < l2)
        return 0;
    if (l2 < 1e-8f)
        return 0;
    len = eeSqrtf(l2);
    ci->depth = r - len;
    vecAdd(&ci->point, &ci->point, &d);
    vecScale(&ci->normal, &d, 1.0f / len);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsAABB__FPffT0T0P11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 18/96 words, size 0x180 vs 0x15c */
/* Segment a-b (radius r) against the box: clips [tmin, tmax] to the slab interval of each axis. */
int hdSphereVsAABB(float *a, float *b, float r, float &tmin, float &tmax, float *min, float *max)
{
    float lo = hdNoHitMin;
    float hi = hdNoHitMax;
    int i;

    for (i = 2; i >= 0; i--) {
        float d = b[i] - a[i];

        if (__builtin_fabsf(d) > 0.0001f) {
            float inv = 1.0f / d;
            float t0 = (min[i] - a[i] - r) * inv;
            float t1 = (max[i] - a[i] + r) * inv;

            if (t0 < t1) {
                lo = lo > t0 ? lo : t0;
                hi = hi < t1 ? hi : t1;
            } else {
                lo = lo > t1 ? lo : t1;
                hi = hi < t0 ? hi : t0;
            }
            if (hi < lo || hi < tmin || tmax < lo)
                return 0;
        } else {
            if (a[i] < min[i] - r || max[i] + r < a[i])
                return 0;
        }
    }
    if (tmin < lo)
        tmin = lo;
    if (hi < tmax)
        tmax = hi;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsAABB__FPfT0fRfT3T0T0);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 14/20 words, size 0x50 vs 0x50 */
int hdSphereVsSphere(_fvector *a, float ra, _fvector *b, float rb)
{
    FVECTOR d;

    vecSub(&d, a, b);
    return vecLenSq(&d) < (ra + rb) * (ra + rb);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsSphere__FP8_fvectorfT0f);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 12/44 words, size 0xac vs 0xb0 */
int hdSphereVsSphere(_fvector *a, float ra, _fvector *b, float rb, ContactInfo *ci)
{
    FVECTOR d;
    float l2, r, len;

    vecSub(&d, a, b);
    l2 = vecLenSq(&d);
    r = ra + rb;
    if (r * r <= l2)
        return 0;
    len = eeSqrtf(l2);
    ci->depth = r - len;
    vecScale(&ci->normal, &d, 1.0f / len);
    vecAddScaled(&ci->point, b, &ci->normal, rb);
    ci->point.w = -1.0f;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsSphere__FP8_fvectorfT0fP11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 33/41 words, size 0xa4 vs 0xa4 */
/* Sphere against a hull with orientation m. */
int hdSphereVsHull(_fvector *c, float r, float (*m)[4], _plane *planes, int n, ContactInfo *ci)
{
    FVECTOR t;

    mathfMulVec(m, c, &t);
    vecSub(&t, &t, c);
    if (!ci)
        return hdSphereVsHull(&t, r, planes, n);
    return hdSphereVsHull(&t, r, planes, n, ci);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsHull__FP8_fvectorfPA3_fP6_planeiP11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 3/26 words, size 0x68 vs 0x60 */
/* Inside every plane (pushed out by r). */
int hdSphereVsHull(_fvector *c, float r, _plane *planes, int n)
{
    for (; n != 0; n--, planes++) {
        if (planes->d <= dot3(c, (_fvector *)&planes->norm) - r)
            return 0;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsHull__FP8_fvectorfP6_planei);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 6/50 words, size 0xc8 vs 0xb8 */
/* Inside every plane; the contact is on the plane of least penetration. */
int hdSphereVsHull(_fvector *c, float r, _plane *planes, int n, ContactInfo *ci)
{
    float best = hdHullFar;
    _plane *bp = 0;

    for (; n != 0; n--, planes++) {
        float dist = planes->d + r - dot3(c, (_fvector *)&planes->norm);

        if (dist <= 0.0f)
            return 0;
        if (dist < best) {
            best = dist;
            bp = planes;
        }
    }
    vecScale(&ci->point, (_fvector *)bp, r - best);
    vecSub(&ci->point, c, &ci->point);
    ci->normal = *(FVECTOR *)bp;
    ci->depth = best;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsHull__FP8_fvectorfP6_planeiP11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 8/189 words, size 0x2f4 vs 0x2ac */
/* Sphere of radius r swept from a to b against a hull: [*t0, *t1] (default [0, 1]) is clipped to the part of the path inside every plane.
   The contact is where the path enters the hull (point.w = the time), or, if a already starts inside, on the plane of least penetration. */
int hdSphereVsHull(_fvector *a, _fvector *b, float r, _plane *planes, int n, ContactInfo *ci, float *t0, float *t1)
{
    float best = hdHullInside;
    _plane *inPlane = 0;
    _plane *enterPlane = 0;
    _plane *exitPlane = 0;
    float tEnter = 0.0f;
    float tExit;
    float enterDepth;

    if (t0)
        tEnter = *t0;
    tExit = 1.0f;
    if (t1)
        tExit = *t1;
    enterDepth = best;
    for (; n != 0; n--, planes++) {
        float da = dot3(a, (_fvector *)&planes->norm);
        float db = dot3(b, (_fvector *)&planes->norm);
        float base = planes->d + r;
        float ea = base - da;
        float eb = base - db;

        if ((0.0f <= ea) != (0.0f <= eb)) {
            float t = ea / (db - da);

            if (0.0f <= ea) {
                if (t < tExit) {
                    tExit = t;
                    if (t < tEnter)
                        return 0;
                    exitPlane = planes;
                }
            } else if (tEnter < t) {
                tEnter = t;
                if (tExit < t)
                    return 0;
                enterDepth = eb;
                enterPlane = planes;
            }
        } else {
            if (!(0.0f <= ea))
                return 0;
            if (ea < best) {
                best = ea;
                inPlane = planes;
            }
        }
    }
    if (t0)
        *t0 = tEnter;
    if (t1)
        *t1 = tExit;
    if (enterPlane) {
        float da = dot3(a, (_fvector *)&enterPlane->norm);
        float db = dot3(b, (_fvector *)&enterPlane->norm);
        float t = (enterPlane->d + r - da - 0.01f) / (db - da);

        t = t > 0.0f ? t : 0.0f;
        ci->normal = *(FVECTOR *)enterPlane;
        vecScale(&ci->point, b, t);
        vecAddScaled(&ci->point, &ci->point, a, 1.0f - t);
        vecAddScaled(&ci->point, &ci->point, &ci->normal, -r);
        ci->point.w = t;
        ci->depth = enterDepth;
        return 1;
    }
    if (exitPlane)
        return 0;
    best += 0.01f;
    ci->normal = *(FVECTOR *)inPlane;
    vecAddScaled(&ci->point, b, &ci->normal, best - r);
    ci->depth = best;
    ci->point.w = -1.0f;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdSphereVsHull__FP8_fvectorT0fP6_planeiP11ContactInfoPfT6);
#endif

#ifdef NON_MATCHING
/* The point of segment p + d * [0, len] closest to q (out->w: its parameter). q may be out. */
static inline void segClosest(FVECTOR *out, _fvector *p, _fvector *d, float len, _fvector *q)
{
    float t;

    vecSub(out, q, p);
    t = dot3(d, out);
    t = t < len ? t : len;
    out->w = t > 0.0f ? t : 0.0f;
    vecAddScaled(out, p, d, out->w);
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 3/254 words, size 0x3f8 vs 0x308 */
/* Capsules p + d * [0, len] of radius r: the closest points between the segments are taken from each segment's end points. */
int hdCapsuleVsCapsule(_fvector &p0, _fvector &d0, float len0, float r0, _fvector &p1, _fvector &d1, float len1, float r1, ContactInfo *ci)
{
    FVECTOR a0, b0, a1, b1, ea, eb;

    segClosest(&a0, &p0, &d0, len0, &p1);
    vecAddScaled(&b0, &p1, &d1, len1);
    segClosest(&b0, &p0, &d0, len0, &b0);
    segClosest(&a1, &p1, &d1, len1, &a0);
    segClosest(&b1, &p1, &d1, len1, &b0);
    vecSub(&ea, &a0, &a1);
    ea.w = vecLenSq(&ea);
    vecSub(&eb, &b0, &b1);
    eb.w = vecLenSq(&eb);
    if (!ci) {
        float rr = (r0 + r1) * (r0 + r1);

        if (ea.w < rr || eb.w < rr)
            return 1;
        return 0;
    }
    if (ea.w < eb.w) {
        float r = r0 + r1;

        if (!(ea.w < r * r))
            return 0;
        ea.w = eeSqrtf(ea.w);
        ci->depth = r - ea.w;
        vecScale(&ci->normal, &ea, 1.0f / ea.w);
        vecAddScaled(&ci->point, &a1, &ci->normal, r1);
        return 1;
    } else {
        float r = r0 + r1;

        if (!(eb.w < r * r))
            return 0;
        eb.w = eeSqrtf(eb.w);
        ci->depth = r - eb.w;
        vecScale(&ci->normal, &eb, 1.0f / eb.w);
        vecAddScaled(&ci->point, &b1, &ci->normal, r1);
        return 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdCapsuleVsCapsule__FR8_fvectorT0ffT0T0ffP11ContactInfo);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 13/57 words, size 0xe4 vs 0xc4 */
/* Lines p0 + d0 * t0 and p1 + d1 * t1 in the xy plane: the parameters of their intersection. */
void hdLineVsLine2D(_fvector &p0, _fvector &d0, float &t0, _fvector &p1, _fvector &d1, float &t1)
{
    FVECTOR n0, n1, v;

    n0.x = -d0.y;
    n0.y = d0.x;
    n1.x = -d1.y;
    n1.y = d1.x;
    n0.z = 0.0f;
    n1.z = 0.0f;
    vecSub(&v, &p1, &p0);
    t0 = dot3(&v, &n1) / dot3(&d0, &n1);
    t1 = -dot3(&v, &n0) / dot3(&d1, &n0);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hdPrim", hdLineVsLine2D__FR8_fvectorT0RfT0T0T2);
#endif
