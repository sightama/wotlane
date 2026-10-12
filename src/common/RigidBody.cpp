#include "common.h"
#include "rigid_body.h"
#include "vecmath.h"

void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void mathfMulMatrix3x3(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfMulMatrixTP3x3(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfScaleMatrix3x3(float (*dst)[4], float (*a)[4], float s);
void mathfAddMatrixTP3x3(float (*dst)[4], float (*a)[4], float (*b)[4]);
void mathfOrthonormalize(float (*m)[4]);

/* a.b over xyz (retail spells it out; the ee-gcc fuses it into mula.s/madda.s/madd.s) */
#define dot3(a, b) ((a)->x * (b)->x + (a)->y * (b)->y + (a)->z * (b)->z)

#ifdef NON_MATCHING
/* equivalent (untuned): 1/10 words, size 0x24 vs 0x28 */
RigidBody::RigidBody()
{
    invMass = 0.0f;
    cs = 0;
    cm.x = 0.0f;
    cm.y = 0.0f;
    cm.z = 0.0f;
    x = cm;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", __9RigidBody);
#endif
void RigidBody::setCenterOfMass(_fvector &c)
{
    setCenterOfMass(c.x, c.y, c.z);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 6/10 words, size 0x28 vs 0x28 */
void RigidBody::setCenterOfMass(float cx, float cy, float cz)
{
    cm.x = cx;
    cm.z = cz;
    cm.y = cy;
    computeX();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", setCenterOfMass__9RigidBodyfff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/10 words, size 0x28 vs 0x28 */
void RigidBody::setTensor(float ix, float iy, float iz)
{
    invInertia.x = 1.0f / ix;
    invInertia.z = 1.0f / iz;
    invInertia.y = 1.0f / iy;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", setTensor__9RigidBodyfff);
#endif
void RigidBody::updateFromCS(void)
{
    computeX();
}
#ifdef NON_MATCHING
/* equivalent (untuned): 9/18 words, size 0x48 vs 0x48 */
void RigidBody::setTrans(_fvector &t)
{
    FVECTOR d;

    vecSub(&d, &x, &cs->trans);
    cs->trans = t;
    vecAdd(&x, &cs->trans, &d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", setTrans__9RigidBodyR8_fvector);
#endif
void RigidBody::deltaTrans(_fvector &d)
{
    vecAdd(&cs->trans, &cs->trans, &d);
    vecAdd(&x, &x, &d);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 13/28 words, size 0x70 vs 0x70 */
void RigidBody::setMatrix(float (&m)[4][4])
{
    FVECTOR t;

    matCopy(cs->mat, m);
    vecApplyRows(&t, cs->mat, &cm);
    vecSub(&cs->trans, &x, &t);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", setMatrix__9RigidBodyRA3_A3_f);
#endif
void RigidBody::initBlock(float sx, float sy, float sz, float mass)
{
    float x2 = sx * sx;
    float y2 = sy * sy;
    float z2 = sz * sz;
    float k = mass / 12.0f;

    setTensor((y2 + z2) * k, (x2 + z2) * k, (x2 + y2) * k);
    invMass = 1.0f / mass;
}
void RigidBody::initSphere(float r, float mass)
{
    float i = (mass + mass) * r * r / 5.0f;

    setTensor(i, i, i);
    invMass = 1.0f / mass;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 53/57 words, size 0xe4 vs 0xe4 */
/* A cylinder of radius r and height h along axis 0/1/2. */
void RigidBody::initCylindrical(float r, float h, int axis, float mass)
{
    float perp = mass * (3.0f * r * r + h * h) / 12.0f;
    float along = mass * r * r * 0.5f;

    switch (axis) {
    case 0:
        setTensor(along, perp, perp);
        break;
    case 1:
        setTensor(perp, along, perp);
        break;
    case 2:
        setTensor(perp, perp, along);
        break;
    }
    invMass = 1.0f / mass;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", initCylindrical__9RigidBodyffif);
#endif
void RigidBody::initEllipsoid(float sx, float sy, float sz, float mass)
{
    float a = sx * 0.5f;
    float b = sy * 0.5f;
    float c = sz * 0.5f;
    float k;

    a = a * a;
    b = b * b;
    k = mass / 5.0f;
    c = c * c;
    setTensor(k * (b + c), k * (a + c), k * (a + b));
    invMass = 1.0f / mass;
}
void RigidBody::reset(void)
{
    vecStoreUnitW(&P);
    vecStoreUnitW(&L);
    clearForces();
    computeAuxiliarys();
    computeX();
}
void RigidBody::clearForces(void)
{
    vecStoreUnitW(&force);
    vecStoreUnitW(&torque);
}
void RigidBody::applyGravForce(float g)
{
    force.z += g / invMass;
}
void RigidBody::applyWorldForce(_fvector &f)
{
    vecAdd(&force, &force, &f);
}
void RigidBody::applyWorldForce(_fvector &f, _fvector &at)
{
    FVECTOR r, t;

    vecAdd(&force, &force, &f);
    vecSub(&r, &at, &x);
    vecCross(&t, &r, &f);
    vecAdd(&torque, &torque, &t);
}
void RigidBody::applyLocalForce(_fvector &f)
{
    FVECTOR w;

    vecApplyRows(&w, cs->mat, &f);
    applyWorldForce(w);
}
void RigidBody::applyLocalForce(_fvector &f, _fvector &at)
{
    FVECTOR w, p;

    vecApplyRows(&w, cs->mat, &f);
    vecApplyRows(&p, cs->mat, &at);
    vecAdd(&p, &p, &cs->trans);
    applyWorldForce(w, p);
}
void RigidBody::applyLocalTorque(_fvector &f, _fvector &at)
{
    FVECTOR w, p;

    vecApplyRows(&w, cs->mat, &f);
    vecApplyRows(&p, cs->mat, &at);
    vecAdd(&p, &p, &cs->trans);
    applyWorldTorque(w, p);
}
void RigidBody::applyWorldTorque(_fvector &f, _fvector &at)
{
    FVECTOR r, t;

    vecSub(&r, &at, &x);
    vecCross(&t, &r, &f);
    vecAdd(&torque, &torque, &t);
}
void RigidBody::applyWorldImpulse(_fvector &j)
{
    vecAdd(&P, &P, &j);
    vecScale(&v, &P, invMass);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 28/34 words, size 0x88 vs 0x88 */
void RigidBody::applyWorldImpulse(_fvector &j, _fvector &at)
{
    FVECTOR t, r;

    applyWorldImpulse(j);
    vecSub(&r, &at, &x);
    vecCross(&t, &r, &j);
    vecAdd(&L, &L, &t);
    mathfMulVec(Iinv, &L, &w);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", applyWorldImpulse__9RigidBodyR8_fvectorT1);
#endif
void RigidBody::applyLocalImpulse(_fvector &j)
{
    FVECTOR w;

    vecApplyRows(&w, cs->mat, &j);
    applyWorldImpulse(w);
}
void RigidBody::applyLocalImpulse(_fvector &j, _fvector &at)
{
    FVECTOR w, p;

    vecApplyRows(&w, cs->mat, &j);
    vecApplyRows(&p, cs->mat, &at);
    vecAdd(&p, &p, &cs->trans);
    applyWorldImpulse(w, p);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 26/176 words, size 0x2c0 vs 0x2b0 */
/* Impulse between two bodies at contact point `at` with normal n (from o toward this) and restitution e. Only approaching
   contacts (relative normal speed <= -0.001) are resolved; returns 1 if an impulse was applied. mu1/mu2 are not used. */
int RigidBody::resolveCollision(RigidBody &o, _fvector &at, _fvector &n, float e, float mu1, float mu2)
{
    FVECTOR rA, vA, rB, vB, t, u;
    float vn, num, den;

    vecSub(&rA, &at, &x);
    vecCross(&vA, &w, &rA);
    vecAdd(&vA, &vA, &v);
    vecSub(&rB, &at, &o.x);
    vecCross(&vB, &o.w, &rB);
    vecAdd(&vB, &vB, &o.v);
    vecSub(&t, &vA, &vB);
    vn = dot3(&n, &t);
    if (0.001f < vn)
        return 0;
    if (-0.001f < vn)
        return 0;
    num = -(e + 1.0f) * vn;
    den = invMass + o.invMass;
    vecCross(&t, &rA, &n);
    mathfMulVec(Iinv, &t, &u);
    vecCross(&t, &u, &rA);
    den += dot3(&n, &t);
    vecCross(&t, &rB, &n);
    mathfMulVec(o.Iinv, &t, &u);
    vecCross(&t, &u, &rB);
    den += dot3(&n, &t);
    vecScale(&t, &n, num / den);
    applyWorldImpulse(t, at);
    vecScale(&t, &t, -1.0f);
    o.applyWorldImpulse(t, at);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", resolveCollision__9RigidBodyR9RigidBodyR8_fvectorT2fff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 34/119 words, size 0x1dc vs 0x1d0 */
/* Against the static world: friction first (no impulse at all if it reports a separating contact). */
int RigidBody::resolveCollision(_fvector &at, _fvector &n, float e, float mu1, float mu2)
{
    FVECTOR r, vp, t, u;
    float vn, num, den;

    if (!resolveFriction(at, n, mu1, mu2))
        return 0;
    vecSub(&r, &at, &x);
    vecCross(&vp, &w, &r);
    vecAdd(&vp, &vp, &v);
    vn = dot3(&n, &vp);
    if (0.001f < vn)
        return 0;
    if (-0.001f < vn)
        return 0;
    num = -(e + 1.0f) * vn;
    den = invMass;
    vecCross(&t, &r, &n);
    mathfMulVec(Iinv, &t, &u);
    vecCross(&t, &u, &r);
    den += dot3(&n, &t);
    vecScale(&t, &n, num / den);
    applyWorldImpulse(t, at);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", resolveCollision__9RigidBodyR8_fvectorT1fff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 28/114 words, size 0x1c8 vs 0x1c0 */
/* Against something moving with velocity vOther (no friction pass). */
int RigidBody::resolveCollision(_fvector &vOther, _fvector &at, _fvector &n, float e, float mu1, float mu2)
{
    FVECTOR r, vp, t, u;
    float vn, num, den;

    vecSub(&r, &at, &x);
    vecCross(&vp, &w, &r);
    vecAdd(&vp, &vp, &v);
    vecSub(&t, &vp, &vOther);
    vn = dot3(&n, &t);
    if (0.001f < vn)
        return 0;
    if (-0.001f < vn)
        return 0;
    num = -(e + 1.0f) * vn;
    den = invMass;
    vecCross(&t, &r, &n);
    mathfMulVec(Iinv, &t, &u);
    vecCross(&t, &u, &r);
    den += dot3(&n, &t);
    vecScale(&t, &n, num / den);
    applyWorldImpulse(t, at);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", resolveCollision__9RigidBodyR8_fvectorN21fff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 35/152 words, size 0x260 vs 0x260 */
/* Friction impulse against the tangential velocity at `at`: coefficient mu1, or mu2 above 6.5 units per field.
   Returns 0 for a separating contact. */
int RigidBody::resolveFriction(_fvector &at, _fvector &n, float mu1, float mu2)
{
    FVECTOR r, vp, t, c, u;
    float vn, len, k, num, den;

    vecSub(&r, &at, &x);
    vecCross(&vp, &w, &r);
    vecAdd(&vp, &vp, &v);
    vn = dot3(&n, &vp);
    if (0.001f < vn)
        return 0;
    if (-0.001f < vn)
        return 0;
    vecScale(&t, &n, dot3(&n, &vp));
    vecSub(&t, &vp, &t);
    len = eeSqrtf(t.x * t.x + t.y * t.y + t.z * t.z);
    k = 1.0f / len;
    t.z *= k;
    t.x *= k;
    t.y *= k;
    vecScale(&t, &t, -1.0f);
    if (6.5f < len)
        mu1 = mu2;
    num = -(mu1 + -1.0f + 1.0f) * len;
    den = invMass;
    vecCross(&c, &r, &t);
    mathfMulVec(Iinv, &c, &u);
    vecCross(&c, &u, &r);
    den += dot3(&t, &c);
    vecScale(&c, &t, -(num / den));
    applyWorldImpulse(c, at);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", resolveFriction__9RigidBodyR8_fvectorT1ff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/100 words, size 0x188 vs 0x190 */
/* Euler step of dt: position, orientation (R += dt * skew(w) R, re-orthonormalized), then momenta from the accumulated
   force and torque. */
void RigidBody::resolveForces(float dt)
{
    float (*R)[4] = cs->mat;
    FVECTOR dx, t;
    FMATRIX dR;
    float skew[4][4];

    dx = v;
    createSkewSymmetric(skew, w);
    mathfMulMatrixTP3x3(dR, skew, R);
    ((FVECTOR *)skew)[0] = force;
    ((FVECTOR *)skew)[1] = torque;
    vecScale(&dx, &dx, dt);
    vecAdd(&x, &x, &dx);
    mathfScaleMatrix3x3(dR, dR, dt);
    mathfAddMatrixTP3x3(R, R, dR);
    mathfOrthonormalize(R);
    R[2][3] = 0.0f;
    vecScale((_fvector *)skew[0], (_fvector *)skew[0], dt);
    vecAdd(&P, &P, (_fvector *)skew[0]);
    vecScale((_fvector *)skew[1], (_fvector *)skew[1], dt);
    vecAdd(&L, &L, (_fvector *)skew[1]);
    computeAuxiliarys();
    vecApplyRows(&t, cs->mat, &cm);
    vecSub(&cs->trans, &x, &t);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", resolveForces__9RigidBodyf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 17/51 words, size 0xcc vs 0xcc */
/* v = P / m; Iinv = R^T-weighted body inverse inertia (via mathfMulMatrix3x3); w = Iinv L. */
void RigidBody::computeAuxiliarys(void)
{
    float (*R)[4] = cs->mat;
    float m[3][4];
    int i, j;

    vecScale(&v, &P, invMass);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            m[i][j] = ((float *)&invInertia)[i] * R[j][i];
    mathfMulMatrix3x3(Iinv, m, R);
    mathfMulVec(Iinv, &L, &w);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", computeAuxiliarys__9RigidBody);
#endif
void RigidBody::computeX(void)
{
    vecApplyRows(&x, cs->mat, &cm);
    vecAdd(&x, &x, &cs->trans);
}
void RigidBody::getPointVelocity(_fvector &out, _fvector &at)
{
    FVECTOR r;

    vecSub(&r, &at, &x);
    vecCross(&out, &w, &r);
    vecAdd(&out, &out, &v);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 2/14 words, size 0x34 vs 0x38 */
void RigidBody::setLinearVelocity(_fvector &vel)
{
    v = vel;
    vecScale(&P, &vel, 1.0f / invMass);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", setLinearVelocity__9RigidBodyR8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/21 words, size 0x4c vs 0x54 */
void RigidBody::createSkewSymmetric(float (*m)[4], _fvector &a)
{
    m[0][0] = 0.0f;
    m[2][2] = 0.0f;
    m[1][1] = 0.0f;
    m[0][1] = -a.z;
    m[1][0] = a.z;
    m[0][2] = a.y;
    m[2][0] = -a.y;
    m[1][2] = -a.x;
    m[2][1] = a.x;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", createSkewSymmetric__9RigidBodyPA3_fR8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/57 words, size 0xd8 vs 0xe4 */
void RigidBody::checkpoint(void)
{
    int i, j;

    savedTrans = cs->trans;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            savedMat[i][j] = cs->mat[i][j];
    savedX = x;
    savedP = P;
    savedL = L;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            savedIinv[i][j] = Iinv[i][j];
    savedV = v;
    savedW = w;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", checkpoint__9RigidBody);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/60 words, size 0xd8 vs 0xf0 */
void RigidBody::rollback(void)
{
    int i, j;

    cs->trans = savedTrans;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            cs->mat[i][j] = savedMat[i][j];
    x = savedX;
    P = savedP;
    L = savedL;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            Iinv[i][j] = savedIinv[i][j];
    v = savedV;
    w = savedW;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/RigidBody", rollback__9RigidBody);
#endif
