#include "common.h"
#include "hieri_types.h"
#include "gen_rand.h"
#include "sce_cdvd.h"
#include "vecmath.h"

/* Vector, matrix and quaternion math. Matrices are row-major float[4][4] with row vectors (translation in row 3);
 * most of the retail code is VU0 macro mode, written here as plain float math (equivalent, not matched).
 * A VU0 vector op only writes the lanes it names, so several functions keep the w of an input: noted below. */

extern "C" {
float sinf(float);
float cosf(float);
float atanf(float);
float atan2f(float, float);
float asinf(float);
int rand(void);
void srand(unsigned);
int printf(const char *, ...);
}
void boundEulerAngle(float *p);
_fvector *viewGetWeTrans(int view);
float (*viewGetWorldToScreenMat(int view))[4];

void mathfUnitMatrix(float (*m)[4]);
void mathfMulMatrix(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void mathfMulVec4x4(float (*m)[4], _fvector *v, _fvector *out);
void mathfRotAxisToMatrix(float (*m)[4], _fvector *axis, float s, float c);
void mathfMatricesToRotAxis(_fvector *axis, float (*a)[4], float (*b)[4]);
void mathfMatrixToRotAxis(_fvector *axis, float (*m)[4]);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
void mathfRotationFromVector(_fvector *rot, _fvector *v);
float mathfHeadingFromVector(_fvector *v);
float mathfPitchFromVector(_fvector *v);
void mathfMatrixFromNormal(float (*m)[4], _fvector *n);
void mathfRotAxisToQuaternion(_fvector *q, _fvector *axis, float angle);
void mathfRotAxisToQuaternion(_fvector *q, _fvector *axis);
void mathfConcatQuaternions(_fvector *dst, _fvector *a, _fvector *b);
void mathfQuaternionToMatrix4x4(float (*m)[4], _fvector *q);
float mathfApproxSin(float x);
float mathfApproxSin2(float x);
float mathfApproxASin2(float x);

#ifdef NON_MATCHING
static inline float dot3(_fvector *a, _fvector *b) { return a->x * b->x + a->y * b->y + a->z * b->z; }
/* dst.xyz = a x b, dst.w = a.w (vopmula/vopmsub) */
static inline void cross3(_fvector *dst, _fvector *a, _fvector *b)
{
    float x = a->y * b->z - b->y * a->z;
    float y = a->z * b->x - b->z * a->x;
    float z = a->x * b->y - b->x * a->y;
    float w = a->w;
    dst->x = x;
    dst->y = y;
    dst->z = z;
    dst->w = w;
}
static inline void normalize3(_fvector *v)
{
    float r = 1.0f / eeSqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    v->z *= r;
    v->x *= r;
    v->y *= r;
}
/* dst.xyz = b - a, dst.w = b.w */
static inline void sub3(_fvector *dst, _fvector *b, _fvector *a)
{
    dst->x = b->x - a->x;
    dst->y = b->y - a->y;
    dst->z = b->z - a->z;
    dst->w = b->w;
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 0/33 words, size 0x84 vs 0x44 */
/* out.xyz = m * v (rows dot v.xyz), out.w = v.w */
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out)
{
    float x = (m[0][0] * v->x + m[0][1] * v->y) + m[0][2] * v->z;
    float y = (m[1][1] * v->y + m[1][2] * v->z) + m[1][0] * v->x;
    float z = (m[2][2] * v->z + m[2][0] * v->x) + m[2][1] * v->y;
    float w = v->w;
    out->x = x;
    out->y = y;
    out->z = z;
    out->w = w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulVec__FPA3_fP8_fvectorT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/48 words, size 0xc0 vs 0x60 */
/* out = m * (v.xyz, 1), each row a dot product */
void mathfMulVec4x4(float (*m)[4], _fvector *v, _fvector *out)
{
    float x = m[0][0] * v->x + (m[0][1] * v->y + (m[0][2] * v->z + m[0][3]));
    float y = m[1][1] * v->y + (m[1][2] * v->z + (m[1][3] + m[1][0] * v->x));
    float z = m[2][2] * v->z + (m[2][3] + (m[2][0] * v->x + m[2][1] * v->y));
    float w = m[3][3] + (m[3][0] * v->x + (m[3][1] * v->y + m[3][2] * v->z));
    out->x = x;
    out->y = y;
    out->z = z;
    out->w = w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulVec4x4__FPA3_fP8_fvectorT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/33 words, size 0x84 vs 0x28 */
/* out.xyz = v.x * m[0] + v.y * m[1] + v.z * m[2], out.w = v.w */
void mathfMulTransVecOld(float (*m)[4], _fvector *v, _fvector *out)
{
    float x = m[0][0] * v->x + m[1][0] * v->y + m[2][0] * v->z;
    float y = m[0][1] * v->x + m[1][1] * v->y + m[2][1] * v->z;
    float z = m[0][2] * v->x + m[1][2] * v->y + m[2][2] * v->z;
    float w = v->w;
    out->x = x;
    out->y = y;
    out->z = z;
    out->w = w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulTransVecOld__FPA3_fP8_fvectorT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/21 words, size 0x54 vs 0x30 */
/* out = v.x * m[0] + v.y * m[1] + v.z * m[2] + m[3] (all four lanes) */
void mathfMulTransVec4x4(float (*m)[4], _fvector *v, _fvector *out)
{
    float vx = v->x, vy = v->y, vz = v->z;
    int i;
    for (i = 0; i < 4; i++)
        ((float *)out)[i] = m[0][i] * vx + m[1][i] * vy + m[2][i] * vz + m[3][i];
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulTransVec4x4__FPA3_fP8_fvectorT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/51 words, size 0xcc vs 0x78 */
/* dst = a * b (dst may alias either) */
void mathfMulMatrix(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    float r[4][4];
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r[i][j] = b[0][j] * a[i][0] + b[1][j] * a[i][1] + b[2][j] * a[i][2] + b[3][j] * a[i][3];
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = r[i][j];
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulMatrix__FPA3_fN20);
#endif
/* dst = a * transpose(b) */
void mathfMulMatrixTP(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = a[i][0] * b[j][0] + a[i][1] * b[j][1] + a[i][2] * b[j][2] + a[i][3] * b[j][3];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/65 words, size 0x104 vs 0x58 */
/* upper 3x3 of dst = a * b; dst[i].w = a[i].w and dst[3] = a[3] */
void mathfMulMatrix3x3(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    float r[4][4];
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            r[i][j] = b[0][j] * a[i][0] + b[1][j] * a[i][1] + b[2][j] * a[i][2];
        r[i][3] = a[i][3];
    }
    for (j = 0; j < 4; j++)
        r[3][j] = a[3][j];
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = r[i][j];
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulMatrix3x3__FPA3_fN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/50 words, size 0xc8 vs 0xb8 */
/* upper 3x3 of dst = a * transpose(b); column 3 and row 3 copied from a */
void mathfMulMatrixTP3x3(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    float (*row)[4];
    float *d = (float *)dst;
    int j;
    for (row = a; row < a + 3; row++, d += 4)
        for (j = 0; j < 3; j++)
            d[j] = (*row)[0] * b[j][0] + (*row)[1] * b[j][1] + (*row)[2] * b[j][2];
    for (j = 0; j < 4; j++) {
        dst[j][3] = a[j][3];
        dst[3][j] = a[3][j];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMulMatrixTP3x3__FPA3_fN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/23 words, size 0x5c vs 0x38 */
/* rows 0-2: dst.xyz = a + b, dst.w = a.w */
void mathfAddMatrix3x3(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    int i;
    for (i = 0; i < 3; i++) {
        float x = b[i][0] + a[i][0], y = b[i][1] + a[i][1], z = b[i][2] + a[i][2];
        float w = a[i][3];
        dst[i][0] = x;
        dst[i][1] = y;
        dst[i][2] = z;
        dst[i][3] = w;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfAddMatrix3x3__FPA3_fN20);
#endif
/* upper 3x3: dst = a + transpose(b) */
void mathfAddMatrixTP3x3(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            dst[i][j] = a[i][j] + b[j][i];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/23 words, size 0x5c vs 0x38 */
/* rows 0-2: dst.xyz = b - a (note the order), dst.w = a.w */
void mathfSubMatrix3x3(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    int i;
    for (i = 0; i < 3; i++) {
        float x = b[i][0] - a[i][0], y = b[i][1] - a[i][1], z = b[i][2] - a[i][2];
        float w = a[i][3];
        dst[i][0] = x;
        dst[i][1] = y;
        dst[i][2] = z;
        dst[i][3] = w;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfSubMatrix3x3__FPA3_fN20);
#endif
/* upper 3x3: dst = a - transpose(b) */
void mathfSubMatrixTP3x3(float (*dst)[4], float (*a)[4], float (*b)[4])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            dst[i][j] = a[i][j] - b[j][i];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/19 words, size 0x4c vs 0x34 */
/* rows 0-2: dst.xyz = src.xyz * s, dst.w = src.w */
void mathfScaleMatrix3x3(float (*dst)[4], float (*src)[4], float s)
{
    int i;
    for (i = 0; i < 3; i++) {
        float x = src[i][0] * s, y = src[i][1] * s, z = src[i][2] * s;
        float w = src[i][3];
        dst[i][0] = x;
        dst[i][1] = y;
        dst[i][2] = z;
        dst[i][3] = w;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfScaleMatrix3x3__FPA3_fT0f);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 4/7 words, size 0x1c vs 0x1c */
/* v x (0, 0, 1) */
void mathfVectorCrossUp(_fvector *dst, _fvector *v)
{
    dst->x = v->y;
    dst->z = 0.0f;
    dst->y = -v->x;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfVectorCrossUp__FP8_fvectorT0);
#endif
/* v x (1, 0, 0) */
void mathfVectorCrossRight(_fvector *dst, _fvector *v)
{
    dst->x = 0.0f;
    dst->y = v->z;
    dst->z = -v->y;
}
/* signed distance of p from the plane */
float mathfPlaneTest(_plane *pl, _fvector *p)
{
    return pl->norm.x * p->x + pl->norm.y * p->y + pl->norm.z * p->z - pl->d;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/37 words, size 0x90 vs 0x94 */
/* normalizes the three upper columns; a degenerate column becomes (0, 0, 1) */
void mathfNormalizeColumns(float (*m)[4])
{
    int c;
    for (c = 0; c < 3; c++) {
        float x = m[0][c], y = m[1][c], z = m[2][c];
        float len = eeSqrtf(x * x + y * y + z * z);
        if (len < 0.0000001f) {
            m[0][c] = 0.0f;
            m[1][c] = 0.0f;
            m[2][c] = 1.0f;
        } else {
            float r = 1.0f / len;
            m[2][c] = z * r;
            m[0][c] = x * r;
            m[1][c] = y * r;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfNormalizeColumns__FPA3_f);
#endif
void mathfTransposeMatrix(float (*dst)[4], float (*src)[4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = src[j][i];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 38/46 words, size 0xb4 vs 0xb8 */
/* Inverse of a rotation+scale+translation matrix, one source column per destination row. Retail quirk kept:
 * the loop only runs for rows 0 and 1, row 2 of dst is left as it was. Row 3 becomes (0, 0, 0, 1). */
void mathfInverseMatrix(float (*dst)[4], float (*src)[4])
{
    int i;
    for (i = 0; i < 2; i++) {
        float s = src[0][i] * src[0][i] + src[1][i] * src[1][i] + src[2][i] * src[2][i];
        dst[i][0] = src[0][i] / s;
        dst[i][1] = src[1][i] / s;
        dst[i][2] = src[2][i] / s;
        dst[i][3] = -(src[0][3] * src[0][i] + src[1][3] * src[1][i] + src[2][3] * src[2][i]) / s;
    }
    dst[3][0] = 0.0f;
    dst[3][3] = 1.0f;
    dst[3][2] = 0.0f;
    dst[3][1] = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfInverseMatrix__FPA3_fT0);
#endif
void mathfRotAxisToMatrix(float (*m)[4], _fvector *axis)
{
    float s = sinf(axis->w);
    mathfRotAxisToMatrix(m, axis, s, cosf(axis->w));
}
#ifdef NON_MATCHING
/* equivalent (untuned): 4/64 words, size 0x100 vs 0xfc */
/* rotation of angle (sin s, cos c) about the unit axis */
void mathfRotAxisToMatrix(float (*m)[4], _fvector *axis, float s, float c)
{
    float t = 1.0f - c;
    float tx = t * axis->x;
    float ty = t * axis->y;
    float tz = t * axis->z;
    m[0][0] = tx * axis->x + c;
    m[0][1] = tx * axis->y + axis->z * s;
    m[0][2] = tx * axis->z - axis->y * s;
    m[1][0] = ty * axis->x - axis->z * s;
    m[1][1] = ty * axis->y + c;
    m[1][2] = ty * axis->z + axis->x * s;
    m[2][0] = tz * axis->x + axis->y * s;
    m[2][1] = tz * axis->y - axis->x * s;
    m[0][3] = 0.0f;
    m[2][3] = 0.0f;
    m[1][3] = 0.0f;
    m[2][2] = tz * axis->z + c;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotAxisToMatrix__FPA3_fP8_fvectorff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/230 words, size 0x398 vs 0x364 */
/* Axis and angle (in w) of the rotation that takes matrix b to matrix a, from the two rows that moved most.
 * No motion gives axis (0, 0, 1) angle 0; a half turn gives angle pi. */
void mathfMatricesToRotAxis(_fvector *axis, float (*a)[4], float (*b)[4])
{
    _fvector d[3];
    float len[3];
    _fvector p, mid, u, v, w;
    int i, j, i0, i1;
    float lu, lv, k;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            ((float *)&d[i])[j] = a[i][j] - b[i][j];
    for (i = 0; i < 3; i++)
        len[i] = d[i].x * d[i].x + d[i].y * d[i].y + d[i].z * d[i].z;
    i0 = 0;
    i1 = 1;
    if (len[0] < len[1]) {
        i0 = 1;
        i1 = 0;
    }
    if (len[i0] < len[2]) {
        i1 = i0;
        i0 = 2;
    } else if (len[i1] < len[2]) {
        i1 = 2;
    }
    if (len[i0] < 0.000001f) {
        axis->w = 0.0f;
        axis->z = 1.0f;
        axis->x = 0.0f;
        axis->y = 0.0f;
        return;
    }
    cross3(axis, &d[i0], &d[i1]);
    normalize3(axis);
    k = axis->x * a[i0][0] + axis->y * a[i0][1] + axis->z * a[i0][2];
    p.x = axis->x * k;
    p.y = axis->y * k;
    p.z = axis->z * k;
    p.w = axis->w;
    mid.x = (a[i0][0] + b[i0][0]) * 0.5f;
    mid.y = (a[i0][1] + b[i0][1]) * 0.5f;
    mid.z = (a[i0][2] + b[i0][2]) * 0.5f;
    mid.w = a[i0][3];
    sub3(&u, &p, &mid);
    sub3(&v, (_fvector *)a[i0], &mid);
    lu = u.x * u.x + u.y * u.y + u.z * u.z;
    lv = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lu < 0.000001f) {
        axis->w = 3.1415927f;
        return;
    }
    cross3(&w, &u, &v);
    w.x += axis->x;
    w.y += axis->y;
    w.z += axis->z;
    if (w.x * w.x + w.y * w.y + w.z * w.z < 0.99999f) {
        axis->x = -axis->x;
        axis->y = -axis->y;
        axis->z = -axis->z;
    }
    axis->w = atanf(eeSqrtf(lv / lu)) * 2.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMatricesToRotAxis__FP8_fvectorPA3_fT1);
#endif
void mathfMatrixToRotAxis(_fvector *axis, float (*m)[4])
{
    FMATRIX unit;
    mathfUnitMatrix(unit);
    mathfMatricesToRotAxis(axis, unit, m);
}
void mathfCopyVectorNotAligned(_fvector *dst, _fvector *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}
void mathfScaleVectorNotAligned(_fvector *dst, _fvector *src, float s)
{
    dst->x = src->x * s;
    dst->y = src->y * s;
    dst->z = src->z * s;
}
void mathfCopyMatrixNotAligned(float (*dst)[4], float (*src)[4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            dst[i][j] = src[i][j];
}
void mathfCopyMatrixNoTrans(float (*dst)[4], float (*src)[4])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            dst[i][j] = src[i][j];
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/19 words, size 0x4c vs 0x30 */
void mathfUnitMatrix(float (*m)[4])
{
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[0][0] = 1.0f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfUnitMatrix__FPA3_f);
#endif
/* VU0 microprogram at 0x430 (hierLoadVu0Ucode) builds the rotation: hardware, stays asm */
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotMatrixRPH__FPA3_fP8_fvector);
#ifdef NON_MATCHING
/* equivalent (untuned): 9/79 words, size 0x13c vs 0x12c */
/* rotation from (pitch x, roll y, heading z) angles, no translation */
void mathfRotMatrixPRH(float (*m)[4], _fvector *a)
{
    float sz = sinf(a->z);
    float cz = cosf(a->z);
    float sx = sinf(a->x);
    float cx = cosf(a->x);
    float sy = sinf(a->y);
    float cy = cosf(a->y);
    m[0][2] = sy;
    m[2][0] = -cx * sy * cz + sx * sz;
    m[2][2] = cx * cy;
    m[1][2] = -sx * cy;
    m[1][0] = sx * sy * cz + cx * sz;
    m[1][1] = -sx * sy * sz + cx * cz;
    m[2][1] = cx * sy * sz + sx * cz;
    m[0][1] = -cy * sz;
    m[0][0] = cy * cz;
    m[0][3] = 0.0f;
    m[2][3] = 0.0f;
    m[1][3] = 0.0f;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotMatrixPRH__FPA3_fP8_fvector);
#endif
/* same VU0 microprogram as mathfRotMatrixRPH: stays asm */
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotMatrixRPHNoTrans__FPA3_fP8_fvector);
#ifdef NON_MATCHING
/* equivalent (untuned): 19/32 words, size 0x80 vs 0x80 */
/* unit vector from heading h (about z, 0 = +y) and pitch p */
void mathfHPtoVector(_fvector *v, float h, float p)
{
    float ch = cosf(h);
    float cp = cosf(p);
    float sh = sinf(h);
    v->y = cp * ch;
    v->x = cp * sh;
    v->z = sinf(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfHPtoVector__FP8_fvectorff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 7/127 words, size 0x1f8 vs 0x1fc */
/* Builds the view-to-screen matrix (screen distance scrz, aspect ax/ay, centre cx/cy, depth range zmin..zmax
 * over nearz..farz) plus two clipping matrices for the guard bands (scales gx, gy). */
void mathfViewScreenMatrix(float (*m)[4], float (*clip)[4], float (*clipGuard)[4], float scrz, float ax, float ay,
                           float cx, float cy, float zmin, float zmax, float nearz, float farz, float gx, float gy)
{
    FMATRIX proj, scale;
    float fn = farz * nearz;
    float az = fn * (zmax - zmin) / (farz - nearz);
    float cz = (-zmax * nearz + zmin * farz) / (farz - nearz);
    float span = nearz * 1024.0f / scrz;
    float n2 = nearz + nearz;
    float c22, c23, xg, yg;

    mathfUnitMatrix(proj);
    proj[0][0] = scrz;
    proj[1][1] = scrz;
    proj[3][2] = 1.0f;
    proj[2][3] = 1.0f;
    proj[2][2] = 0.0f;
    proj[3][3] = 0.0f;
    mathfUnitMatrix(scale);
    scale[0][0] = ax;
    scale[1][1] = ay;
    scale[2][2] = az;
    scale[0][3] = cx;
    scale[1][3] = cy;
    scale[2][3] = cz;
    mathfMulMatrix(m, scale, proj);

    mathfUnitMatrix(clip);
    c22 = (farz + nearz) / (farz - nearz);
    c23 = fn * -2.0f / (farz - nearz);
    clip[3][3] = 0.0f;
    clip[3][2] = 1.0f;
    clip[2][2] = c22;
    clip[2][3] = c23;
    clip[1][1] = n2 / (span + span);
    clip[0][0] = clip[1][1];
    yg = nearz * gy / scrz;
    xg = nearz * gx / scrz;

    mathfUnitMatrix(clipGuard);
    clipGuard[2][2] = c22;
    clipGuard[2][3] = c23;
    clipGuard[3][2] = 1.0f;
    clipGuard[3][3] = 0.0f;
    clipGuard[0][0] = n2 / (xg + xg);
    clipGuard[1][1] = n2 / (yg + yg);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfViewScreenMatrix__FPA3_fN20fffffffffff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/86 words, size 0xa4 vs 0x158 */
void mathfDumpMatrix(float (*m)[4])
{
    int i;
    for (i = 0; i < 4; i++)
        printf("%f %f %f %f \n", m[i][0], m[i][1], m[i][2], m[i][3]);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfDumpMatrix__FPA3_f);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/56 words, size 0xe0 vs 0xc0 */
/* Distance estimate without a square root: largest + 11/32 middle + 1/4 smallest |component| of b - a
 * (of a alone when b is null). */
float mathfManhatDist(_fvector *a, _fvector *b)
{
    float hi, mid, lo, t;
    if (b) {
        hi = b->x - a->x;
        mid = b->y - a->y;
        lo = b->z - a->z;
    } else {
        hi = a->x;
        mid = a->y;
        lo = a->z;
    }
    hi = hi < 0.0f ? -hi : hi;
    mid = mid < 0.0f ? -mid : mid;
    lo = lo < 0.0f ? -lo : lo;
    if (hi < mid) {
        t = hi;
        hi = mid;
        mid = t;
    }
    if (hi < lo) {
        t = hi;
        hi = lo;
        lo = t;
    }
    if (mid < lo) {
        t = mid;
        mid = lo;
        lo = t;
    }
    return hi + mid * 0.34375f + lo * 0.25f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfManhatDist__FP8_fvectorT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/33 words, size 0x84 vs 0x6c */
/* 2D version of mathfManhatDist: largest + 11/32 smallest of |dx|, |dy| */
float mathfManhatDist2D(_fvector *a, _fvector *b)
{
    float hi, lo, t;
    if (b) {
        lo = b->y - a->y;
        hi = b->x - a->x;
    } else {
        lo = a->y;
        hi = a->x;
    }
    lo = lo < 0.0f ? -lo : lo;
    hi = hi < 0.0f ? -hi : hi;
    if (hi < lo) {
        t = hi;
        hi = lo;
        lo = t;
    }
    return hi + lo * 0.34375f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfManhatDist2D__FP8_fvectorT0);
#endif
/* roll/pitch/heading of a matrix: pitch and heading from the forward row, roll from where row 0 ends up */
void mathfRPHFromMatrix(float (*m)[4], _fvector *rph)
{
    FMATRIX rot;
    _fvector side;
    mathfRotationFromVector(rph, (_fvector *)m[1]);
    mathfRotMatrixRPH(rot, rph);
    mathfMulVec(rot, (_fvector *)m[0], &side);
    rph->y = -(1.5707964f - atan2f(side.x, side.z));
    boundEulerAngle(&rph->y);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 9/53 words, size 0xd4 vs 0x94 */
/* inverse of mathfRotMatrixPRH; near gimbal lock (|m[0][2]| ~ 1) heading is 0 */
void mathfPRHFromMatrix(float (*m)[4], _fvector *prh)
{
    float t;
    prh->y = -asinf(m[0][2]);
    t = m[0][2];
    if ((t < 0.0f ? -t : t) < 0.99999f) {
        prh->x = atan2f(m[1][2], m[2][2]);
        prh->z = atan2f(m[0][1], m[0][0]);
    } else {
        prh->x = atan2f(m[1][0], m[2][0]);
        prh->z = 0.0f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfPRHFromMatrix__FPA3_fP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/22 words, size 0x58 vs 0x2c */
void mathfRotationFromPointToPoint(_fvector *rot, _fvector *from, _fvector *to)
{
    FVECTOR d;
    sub3(&d, to, from);
    mathfRotationFromVector(rot, &d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotationFromPointToPoint__FP8_fvectorN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/41 words, size 0xa4 vs 0x78 */
/* matrix whose row 1 points from `from` to `to`; m is left alone when the points coincide */
void mathfMatrixFromPointToPoint(float (*m)[4], _fvector *from, _fvector *to)
{
    FVECTOR d;
    float l2, r;
    sub3(&d, to, from);
    l2 = d.x * d.x + d.y * d.y + d.z * d.z;
    r = 1.0f / eeSqrtf(l2);
    d.z *= r;
    d.x *= r;
    d.y *= r;
    if (0.0f < l2)
        mathfMatrixFromNormal(m, &d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMatrixFromPointToPoint__FPA3_fP8_fvectorT1);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/26 words, size 0x68 vs 0x64 */
/* pitch (x) and heading (z) of a direction; roll (y) is 0 */
void mathfRotationFromVector(_fvector *rot, _fvector *v)
{
    rot->x = atan2f(v->z, eeSqrtf(v->x * v->x + v->y * v->y));
    rot->y = 0.0f;
    rot->z = atan2f(v->x, v->y);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotationFromVector__FP8_fvectorT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/22 words, size 0x58 vs 0x2c */
float mathfHeadingFromPointToPoint(_fvector *from, _fvector *to)
{
    FVECTOR d;
    sub3(&d, to, from);
    return mathfHeadingFromVector(&d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfHeadingFromPointToPoint__FP8_fvectorT0);
#endif
/* angle about z, 0 along +y, positive towards +x */
float mathfHeadingFromVector(_fvector *v)
{
    return atan2f(v->x, v->y);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 1/22 words, size 0x58 vs 0x2c */
float mathfPitchFromPointToPoint(_fvector *from, _fvector *to)
{
    FVECTOR d;
    sub3(&d, to, from);
    return mathfPitchFromVector(&d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfPitchFromPointToPoint__FP8_fvectorT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/14 words, size 0x38 vs 0x34 */
float mathfPitchFromVector(_fvector *v)
{
    return atan2f(v->z, eeSqrtf(v->x * v->x + v->y * v->y));
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfPitchFromVector__FP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/53 words, size 0xd0 vs 0xd4 */
/* rotation matrix with row 1 = n (unit) and no roll; a vertical n gets row 0 = (1, 0, 0) */
void mathfMatrixFromNormal(float (*m)[4], _fvector *n)
{
    float l;
    m[1][0] = n->x;
    m[1][1] = n->y;
    m[1][2] = n->z;
    l = eeSqrtf(n->x * n->x + n->y * n->y);
    if (0.000001f < l) {
        float c = n->y / l;
        float s = -n->x / l;
        m[0][0] = c;
        m[0][1] = s;
        m[2][0] = s * n->z;
        m[2][1] = -c * n->z;
    } else {
        m[0][1] = 0.0f;
        m[0][0] = 1.0f;
        m[2][1] = -n->z;
        m[2][0] = 0.0f;
    }
    m[2][2] = l;
    m[3][3] = 1.0f;
    m[0][2] = 0.0f;
    m[2][3] = 0.0f;
    m[1][3] = 0.0f;
    m[0][3] = 0.0f;
    m[3][2] = 0.0f;
    m[3][1] = 0.0f;
    m[3][0] = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfMatrixFromNormal__FPA3_fP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/35 words, size 0x8c vs 0x50 */
/* splits v into the part along the unit axis (par) and the rest (perp) */
void mathfDecomposeVector(_fvector *v, _fvector *axis, _fvector *perp, _fvector *par)
{
    float d = v->x * axis->x + v->y * axis->y + v->z * axis->z;
    par->x = axis->x * d;
    par->y = axis->y * d;
    par->z = axis->z * d;
    par->w = axis->w;
    sub3(perp, v, par);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfDecomposeVector__FP8_fvectorN30);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/43 words, size 0xac vs 0x64 */
/* time of closest approach of two moving points (positions p, q; velocities vp, vq) */
float mathfClosestApproach(_fvector &p, _fvector &vp, _fvector &q, _fvector &vq)
{
    FVECTOR d, dv;
    sub3(&d, &p, &q);
    sub3(&dv, &vp, &vq);
    return -dot3(&d, &dv) / dot3(&dv, &dv);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfClosestApproach__FR8_fvectorN30);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/69 words, size 0x114 vs 0xb0 */
/* same, with the time clamped to [0, maxT]; distSq gets the squared distance then, the time is returned */
float mathfClosestApproach(_fvector &p, _fvector &vp, _fvector &q, _fvector &vq, float maxT, float &distSq)
{
    FVECTOR d, dv;
    float t;
    sub3(&d, &p, &q);
    sub3(&dv, &vp, &vq);
    t = -dot3(&d, &dv) / dot3(&dv, &dv);
    if (maxT < t)
        t = maxT;
    if (t < 0.0f)
        t = 0.0f;
    dv.x = d.x + dv.x * t;
    dv.y = d.y + dv.y * t;
    dv.z = d.z + dv.z * t;
    distSq = dot3(&dv, &dv);
    return t;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfClosestApproach__FR8_fvectorN30fRf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/105 words, size 0x1a4 vs 0xf0 */
/* rebuilds an orthonormal basis: row0 = row1 x row2, row1 = row2 x row0, row2 = row0 x row1 */
void mathfOrthonormalize(float (*m)[4])
{
    _fvector *r = (_fvector *)m;
    cross3(&r[0], &r[1], &r[2]);
    normalize3(&r[0]);
    cross3(&r[1], &r[2], &r[0]);
    normalize3(&r[1]);
    cross3(&r[2], &r[0], &r[1]);
    normalize3(&r[2]);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfOrthonormalize__FPA3_f);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 12/148 words, size 0x250 vs 0x178 */
/* mathfOrthonormalize spread over six calls (one step each, *step cycles 0..5) */
void mathfOrthonormalizeOverTime(float (*m)[4], int *step)
{
    _fvector *r = (_fvector *)m;
    switch (*step) {
    case 0:
        cross3(&r[0], &r[1], &r[2]);
        break;
    case 1:
        normalize3(&r[0]);
        break;
    case 2:
        cross3(&r[1], &r[2], &r[0]);
        break;
    case 3:
        normalize3(&r[1]);
        break;
    case 4:
        cross3(&r[2], &r[0], &r[1]);
        break;
    case 5:
        normalize3(&r[2]);
        *step = -1;
        break;
    }
    (*step)++;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfOrthonormalizeOverTime__FPA3_fPi);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 16/40 words, size 0xa0 vs 0x9c */
/* "ND" variants draw from gRand (Mersenne Twister) instead of the C library rand() */
float mathfRandfND(float lo, float hi)
{
    if (hi <= lo)
        return lo;
    return lo + (hi - lo) * (float)(gRand.rand() & 0x3FFF) * (1.0f / 16384.0f);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRandfND__Fff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 12/24 words, size 0x60 vs 0x60 */
/* uniform integer in [lo, hi] */
int mathfRandND(int lo, int hi)
{
    if (lo < hi)
        return lo + (((hi - lo + 1) * (gRand.rand() & 0x7FFF)) >> 15);
    return lo;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRandND__Fii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 28/31 words, size 0x7c vs 0x7c */
/* uniform float in [lo, hi): 23 random mantissa bits under exponent 0 give [1, 2) */
float mathfRandf(float lo, float hi)
{
    Flint u;
    if (hi <= lo)
        return lo;
    u.i = (rand() & 0x7FFFFF) | 0x3F800000;
    return lo + (hi - lo) * (u.f - 1.0f);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRandf__Fff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 12/23 words, size 0x5c vs 0x5c */
/* uniform integer in [lo, hi] */
int mathfRand(int lo, int hi)
{
    if (lo < hi)
        return lo + (((hi - lo + 1) * (rand() & 0x7FFF)) >> 15);
    return lo;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRand__Fii);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 41/82 words, size 0x148 vs 0x148 */
/* Random direction: y uniform in [1 - 2 * scale, 1), the angle around y from a quadratic sin/cos
 * approximation over a quarter turn, quadrant picked by two bits of the first draw. w = 1. */
void mathfRandVector(_fvector *v, float scale)
{
    unsigned r1 = gRand.rand();
    unsigned r2 = gRand.rand();
    Flint a, b;
    float y, t, r, t2;

    a.u = (r1 & 0x7FFFFF) | 0x40000000;
    b.u = (r2 & 0x7FFFFF) | 0x3F800000;
    y = a.f * scale - scale * 4.0f + 1.0f;
    t = b.f - 1.0f;
    r = eeSqrtf(1.0f - y * y);
    t2 = t * 0.82842714f * t;
    v->y = y;
    v->x = r * (t * 1.8284271f - t2);
    v->z = r * ((t * -0.17157288f + 1.0f) - t2);
    if (r1 & 0x800000)
        v->x = 0.0f - v->x;
    if (r1 & 0x1000000)
        v->z = 0.0f - v->z;
    v->w = 1.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRandVector__FP8_fvectorf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 13/61 words, size 0xf4 vs 0xd4 */
/* World point to screen through view `view`: out = (x/w, y/w, w, z/w) of the projected point */
void mathfTransform3dTo2d(int view, _fvector *in, _fvector *out)
{
    FVECTOR p, s;
    _fvector *we = viewGetWeTrans(view);
    float (*m)[4] = viewGetWorldToScreenMat(view);
    float r;
    if (!in || !out) {
        printf("Null Vector Pointer in mathfTransform3dTo2d\n");
        return;
    }
    p.x = in->x + we->x;
    p.y = in->y + we->y;
    p.z = in->z + we->z;
    p.w = 1.0f;
    mathfMulVec4x4(m, &p, &s);
    r = 1.0f / s.w;
    out->z = s.w;
    out->w = s.z * r;
    out->x = s.x * r;
    out->y = s.y * r;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfTransform3dTo2d__FiP8_fvectorT1);
#endif
/* seed 0 means "from the CD clock" (second + minute * 60, raw BCD bytes) */
void mathfRandInit(int seed, int)
{
    sceCdCLOCK clock;
    if (seed == 0 && sceCdReadClock(&clock))
        seed = clock.second + clock.minute * 60;
    srand(seed);
    gRand.seed(seed);
}
void mathfRotAxisToQuaternion(_fvector *q, _fvector *axis, float angle)
{
    float h = angle * 0.5f;
    float s = sinf(h);
    q->x = s * axis->x;
    q->y = s * axis->y;
    q->z = s * axis->z;
    q->w = cosf(h);
}
/* angle in axis->w (radians) */
void mathfRotAxisToQuaternion(_fvector *q, _fvector *axis)
{
    mathfRotAxisToQuaternion(q, axis, axis->w);
}
/* angle in axis->w, in degrees */
void mathfAxisAngleToQuaternion(_fvector *q, _fvector *axis)
{
    float s = sinf(axis->w * 0.008726646f);
    q->x = s * axis->x;
    q->y = s * axis->y;
    q->z = s * axis->z;
    q->w = cosf(axis->w * 0.008726646f);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 47/70 words, size 0x118 vs 0x118 */
/* quaternion from (x, y, z) Euler angles; z is negated */
void mathfRPHToQuaternion(_fvector *q, _fvector *a)
{
    float sz = sinf(-a->z * 0.5f);
    float cz = cosf(-a->z * 0.5f);
    float sy = sinf(a->y * 0.5f);
    float cy = cosf(a->y * 0.5f);
    float sx = sinf(a->x * 0.5f);
    float cx = cosf(a->x * 0.5f);
    float czsy = cz * sy;
    float szcy = sz * cy;
    float czcy = cz * cy;
    float szsy = sz * sy;
    q->x = czcy * sx - szsy * cx;
    q->w = czcy * cx + szsy * sx;
    q->y = czsy * cx + szcy * sx;
    q->z = szcy * cx - czsy * sx;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRPHToQuaternion__FP8_fvectorT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/48 words, size 0xc0 vs 0x80 */
/* shortest-arc rotation taking unit vector a to unit vector b */
void mathfRotationArcQuaternion(_fvector *q, _fvector *a, _fvector *b)
{
    float s, r;
    cross3(q, a, b);
    s = eeSqrtf((a->x * b->x + a->y * b->y + a->z * b->z + 1.0f) * 2.0f);
    r = 1.0f / s;
    q->x *= r;
    q->y *= r;
    q->z *= r;
    q->w = s * 0.5f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfRotationArcQuaternion__FP8_fvectorN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 38/44 words, size 0xb0 vs 0xb0 */
/* dst = src / |src|, only when |src|^2 is off 1 by more than ~1e-6 (otherwise dst is not written) */
void mathfNormalizeQuaternion(_fvector *dst, _fvector *src)
{
    float l2 = src->x * src->x + src->y * src->y + src->z * src->z + src->w * src->w;
    if (1.000001f <= l2 || l2 <= 0.99999905f) {
        float r = 1.0f / eeSqrtf(l2);
        dst->x = src->x * r;
        dst->y = src->y * r;
        dst->z = src->z * r;
        dst->w = src->w * r;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfNormalizeQuaternion__FP8_fvectorT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 26/42 words, size 0xa8 vs 0xa8 */
/* keeps w and rescales xyz so the quaternion has unit length; a null xyz becomes (0, 0, sqrt(1 - w^2)) */
void mathfNormalizeQuaternionFromW(_fvector *dst, _fvector *src)
{
    float l2 = src->x * src->x + src->y * src->y + src->z * src->z;
    if (l2 < 0.0000001f) {
        dst->x = 0.0f;
        dst->y = 0.0f;
        dst->z = eeSqrtf(1.0f - src->w * src->w);
    } else {
        float s = eeSqrtf((1.0f - src->w * src->w) / l2);
        dst->x = src->x * s;
        dst->y = src->y * s;
        dst->z = src->z * s;
    }
    dst->w = src->w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfNormalizeQuaternionFromW__FP8_fvectorT0);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/41 words, size 0xa4 vs 0x34 */
/* dst = b * a (rotation a followed by b) */
void mathfConcatQuaternions(_fvector *dst, _fvector *a, _fvector *b)
{
    float x = b->y * a->z + b->x * a->w + a->x * b->w - a->y * b->z;
    float y = b->z * a->x + b->y * a->w + a->y * b->w - a->z * b->x;
    float z = b->x * a->y + b->z * a->w + a->z * b->w - a->x * b->y;
    float w = b->w * a->w - b->z * a->z - b->y * a->y - b->x * a->x;
    dst->x = x;
    dst->y = y;
    dst->z = z;
    dst->w = w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfConcatQuaternions__FP8_fvectorN20);
#endif
/* rotation taking b to a, through their matrices */
void mathfDeConcatQuaternions(_fvector *dst, _fvector *a, _fvector *b)
{
    FMATRIX ma, mb;
    FVECTOR axis;
    mathfQuaternionToMatrix4x4(ma, a);
    mathfQuaternionToMatrix4x4(mb, b);
    mathfMatricesToRotAxis(&axis, ma, mb);
    mathfRotAxisToQuaternion(dst, &axis);
}
/* row 2 of the rotation matrix */
void mathfQuaternionToUpVector(_fvector *v, _fvector *q)
{
    v->x = (q->x * q->z + q->y * q->w) * 2.0f;
    v->y = (q->y * q->z - q->x * q->w) * 2.0f;
    v->z = 1.0f - (q->x * q->x + q->y * q->y) * 2.0f;
}
/* row 1 of the rotation matrix */
void mathfQuaternionToForwardVector(_fvector *v, _fvector *q)
{
    v->x = (q->x * q->y - q->z * q->w) * 2.0f;
    v->y = 1.0f - (q->x * q->x + q->z * q->z) * 2.0f;
    v->z = (q->y * q->z + q->x * q->w) * 2.0f;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/53 words, size 0xd4 vs 0x80 */
void mathfQuaternionToMatrix4x4(float (*m)[4], _fvector *q)
{
    float x = q->x, y = q->y, z = q->z, w = q->w;
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = y * x, xz = z * x, yz = z * y;
    float wx = w * x, wy = w * y, wz = w * z;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
    m[0][0] = 1.0f - (yy + zz) * 2.0f;
    m[0][1] = (xy + wz) * 2.0f;
    m[0][2] = (xz - wy) * 2.0f;
    m[0][3] = 0.0f;
    m[1][0] = (xy - wz) * 2.0f;
    m[1][1] = 1.0f - (zz + xx) * 2.0f;
    m[1][2] = (yz + wx) * 2.0f;
    m[1][3] = 0.0f;
    m[2][0] = (xz + wy) * 2.0f;
    m[2][1] = (yz - wx) * 2.0f;
    m[2][2] = 1.0f - (xx + yy) * 2.0f;
    m[2][3] = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfQuaternionToMatrix4x4__FPA3_fP8_fvector);
#endif
void mathfMatrixToQuaternion(_fvector *q, float (*m)[4])
{
    FVECTOR axis;
    mathfMatrixToRotAxis(&axis, m);
    mathfRotAxisToQuaternion(q, &axis);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/17 words, size 0x44 vs 0x34 */
/* blend towards the identity: xyz * t, w = (1 - t) + t * w */
void mathfUnitizeQuaternion(_fvector *dst, _fvector *src, float t)
{
    float w = src->w;
    dst->x = src->x * t;
    dst->y = src->y * t;
    dst->z = src->z * t;
    dst->w = (1.0f - t) + t * w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfUnitizeQuaternion__FP8_fvectorT0f);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/26 words, size 0x68 vs 0x60 */
/* rotates v by q (sets v->w = 0 first) */
void mathfQuaternionMulVec(_fvector *dst, _fvector *q, _fvector *v)
{
    FVECTOR conj;
    v->w = 0.0f;
    conj.x = q->x * -1.0f;
    conj.y = q->y * -1.0f;
    conj.z = q->z * -1.0f;
    conj.w = q->w;
    mathfConcatQuaternions(dst, q, v);
    mathfConcatQuaternions(dst, dst, &conj);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfQuaternionMulVec__FP8_fvectorN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/29 words, size 0x74 vs 0x70 */
/* rotates v by the inverse of q (sets v->w = 0 first) */
void mathfQuaternionMulTransVec(_fvector *dst, _fvector *q, _fvector *v)
{
    FVECTOR conj;
    v->w = 0.0f;
    conj.x = q->x * -1.0f;
    conj.y = q->y * -1.0f;
    conj.z = q->z * -1.0f;
    conj.w = q->w;
    mathfConcatQuaternions(dst, &conj, v);
    mathfConcatQuaternions(dst, dst, q);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfQuaternionMulTransVec__FP8_fvectorN20);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 13/47 words, size 0xbc vs 0xbc */
/* not a true slerp: a linear blend (1 - t) a + t b along the shorter way, unnormalized */
void mathfSlerpQuaternion(_fvector *dst, _fvector *a, _fvector *b, float t)
{
    float tb = t;
    float ta;
    if (a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w < 0.0f)
        tb = -tb;
    ta = 1.0f - t;
    dst->x = ta * a->x + tb * b->x;
    dst->y = ta * a->y + tb * b->y;
    dst->z = ta * a->z + tb * b->z;
    dst->w = ta * a->w + tb * b->w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfSlerpQuaternion__FP8_fvectorN20f);
#endif
/* reads VU0 registers vf30/vf31: hardware, stays asm */
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfVectorMinMaxGetMac__FP8_fvectorT0);
/* sin(pi x) for x in [-1, 1]: 4x(1 - x) plus a cubic correction */
static inline float approxSinPos(float x)
{
    float a = x - 0.15023334f;
    float b = x - 0.8497667f;
    return (x - x * x) * 4.0f + ((a * a * b) * b - 0.014966272f) * 3.742388f;
}
float mathfApproxSin(float x)
{
    if (x < 0.0f) {
        x = -x;
        return -approxSinPos(x);
    }
    return approxSinPos(x);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 8/19 words, size 0x4c vs 0x4c */
/* rougher sin(pi x): the parabola 4x(1 - |x|) */
float mathfApproxSin2(float x)
{
    if (0.0f < x)
        return (x - x * x) * 4.0f;
    x = -x;
    return -((x - x * x) * 4.0f);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfApproxSin2__Ff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/47 words, size 0x6c vs 0xbc */
/* inverse of mathfApproxSin2 */
float mathfApproxASin2(float x)
{
    if (0.0f < x)
        return 1.0f - (eeSqrtf(1.0f - x) + 1.0f) * 0.5f;
    return -(1.0f - (eeSqrtf(x + 1.0f) + 1.0f) * 0.5f);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/mathf", mathfApproxASin2__Ff);
#endif
float mathfApproxCos(float x)
{
    x += 0.5f;
    if (1.0f < x)
        x -= 2.0f;
    return mathfApproxSin(x);
}
float mathfApproxCos2(float x)
{
    x += 0.5f;
    if (1.0f < x)
        x -= 2.0f;
    return mathfApproxSin2(x);
}
float mathfApproxACos2(float x)
{
    float a = mathfApproxASin2(x) - 0.5f;
    if (a < -1.0f)
        a += 2.0f;
    return -a;
}
INCLUDE_ASM("asm/nonmatchings/common/mathf", __static_initialization_and_destruction_0_0020F130);
INCLUDE_ASM("asm/nonmatchings/common/mathf", _GLOBAL_$I$gRand);
