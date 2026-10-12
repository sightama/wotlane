#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

extern "C" float sqrtf(float);

class PointMass {
public:
    _fvector *pos;
    char pad[12];
    FVECTOR vel;
    FVECTOR force;

    void init(float mass, _fvector *p);
    void applyImpulse(_fvector &impulse);
    void applyDampeningForce(float d);
    void clampForce(float maxForce);
    void resolveForcesEuler(float dt);
};

class Spring {
public:
    float restLength;
    float stiffness;
    float damping;
    PointMass *a;
    PointMass *b;

    void init(float rest, float k, float damp);
    void applyForces(void);
};

#ifdef NON_MATCHING
/* equivalent (untuned): 2/14 words, size 0x30 vs 0x38 */
/* 2/14 words: retail zeroes vel/force through derived pointers in a different order */
void PointMass::init(float mass, _fvector *p)
{
    pos = p;
    vel.x = 0;
    vel.y = 0;
    vel.z = 0;
    force.x = 0;
    force.y = 0;
    force.z = 0;
    force.w = 1.0f / mass;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointMass", init__9PointMassfP8_fvector);
#endif
void PointMass::applyImpulse(_fvector &impulse)
{
    _fvector tmp;

    vecScale(&tmp, &impulse, force.w);
    vecAdd(&vel, &vel, &tmp);
}
void PointMass::applyDampeningForce(float d)
{
    _fvector tmp;

    vecScale(&tmp, &vel, -d);
    vecAdd(&force, &force, &tmp);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/38 words, size 0x98 vs 0x98 */
void PointMass::clampForce(float maxForce)
{
    float l2 = force.x * force.x + force.y * force.y + force.z * force.z;

    if (maxForce * maxForce < l2)
        vecScale(&force, &force, maxForce / sqrtf(l2));
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointMass", clampForce__9PointMassf);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 18/26 words, size 0x68 vs 0x68 */
/* 18/26 words: pointer temporaries for &force/&vel get swapped registers */
void PointMass::resolveForcesEuler(float dt)
{
    _fvector tmp;

    vecScale(&tmp, &force, dt * force.w);
    vecAdd(&vel, &vel, &tmp);
    vecScale(&tmp, &vel, dt);
    vecAdd(pos, pos, &tmp);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointMass", resolveForcesEuler__9PointMassf);
#endif
void Spring::init(float rest, float k, float damp)
{
    restLength = rest;
    stiffness = k;
    damping = damp;
    b = 0;
    a = 0;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/65 words, size 0x104 vs 0xf4 */
/* Hooke spring with damping along the axis: pushes a and b apart/together by stiffness * stretch + damping * closing speed. */
void Spring::applyForces(void)
{
    FVECTOR d, dv;
    float len, inv, f;

    vecSub(&d, a->pos, b->pos);
    len = eeSqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    f = stiffness * (len - restLength);
    vecSub(&dv, &a->vel, &b->vel);
    inv = 1.0f / len;
    f = (f + damping * (dv.x * d.x + dv.y * d.y + dv.z * d.z) * inv) * inv;
    vecScale(&d, &d, f);
    vecAdd(&b->force, &b->force, &d);
    vecSub(&a->force, &a->force, &d);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointMass", applyForces__6Spring);
#endif
