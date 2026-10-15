#include "common.h"
#include "game/game.h"
#include "game/monster_dynamics.h"
#include "hd.h"
#include "vecmath.h"

/* The first 8 bytes of an _hdResult (hit, surface, ids) as one 64-bit word */
struct HdResultIds {
    unsigned long hit : 8;
    unsigned long pad : 8;
    unsigned long surface : 16;
    unsigned long id2 : 11;
    unsigned long id1 : 14;
};

/* An interactive object (common/Interactives); vptr at 0x10, entries {short delta, short, function} */
class DbInteractive {
public:
    int m_kind;                 /* 0x00: 5 pickup */
    char pad4[0xC];
    void *m_vptr;               /* 0x10: slot 0x20 returns its velocity */
    char pad14[0x50 - 0x14];
    unsigned long m_unk50;      /* 0x50: bit 2: a rigid body pickup (RigidPickup computes the contact velocity itself) */
};

class Interactives {
public:
    static DbInteractive *getInteractive(int i);
};

class RigidPickup {
public:
    void getContactVel(_fvector &out, _hdResult &res);
};

extern "C" float sqrtf(float);
extern int gUseUnifiedView;
__asm__("#SNFIX_SMALL gUseUnifiedView");
extern float s_aiTurnFactor __asm__("s_aiTurnFactor.2457"); /* static local of updateTurnMotion, 2 */
__asm__("#SNFIX_SMALL s_aiTurnFactor.2457");
extern float s_pushMag __asm__("s_pushMag.2461"); /* static local of updatePosition, 2 */
__asm__("#SNFIX_SMALL s_pushMag.2461");
__asm__("#SNFIX_SMALL _15MonsterDynamics$s_turnModZeroAtSpeed");
__asm__("#SNFIX_SMALL _15MonsterDynamics$s_turnModMin");
extern float UNIT_MATRIX[4][4];

float smoothEasyInTC(float cur, float target, float rate, float minStep);
int timerGetUpdateRate(void);
int timerGetFieldsLastFrame(void);
void boundEulerAngles(_fvector *v);
void mathfRotMatrixRPH(float (*m)[4], _fvector *v);

/* *v = (0, 0, 0), w kept */
static inline void vecZero(_fvector *v)
{
    v->x = 0.0f;
    v->y = 0.0f;
    v->z = 0.0f;
}
/* *v = (x, y, z), w kept */
static inline void vecSet(_fvector *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}
/* a . b (xyz) on the FPU accumulator, operands handed over pairwise (vecDot in vecmath.h takes all of a first) */
static inline float vecDotI(_fvector *a, _fvector *b)
{
    float *p = (float *)a, *q = (float *)b;
    float r;

    __asm__ volatile("mula.s %1, %2\n\t"
                     "madda.s %3, %4\n\t"
                     "madd.s %0, %5, %6"
                     : "=f"(r) : "f"(p[0]), "f"(q[0]), "f"(p[1]), "f"(q[1]), "f"(p[2]), "f"(q[2]));
    return r;
}
/* |a|^2 (xyz) on the FPU accumulator, registers left to the compiler (vecLenSq in vecmath.h pins them) */
static inline float vecLenSqF(_fvector *a)
{
    float *p = (float *)a;
    float r;

    __asm__ volatile("mula.s %1, %1\n\t"
                     "madda.s %2, %2\n\t"
                     "madd.s %0, %3, %3"
                     : "=f"(r) : "f"(p[0]), "f"(p[1]), "f"(p[2]));
    return r;
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
#ifdef NON_MATCHING
/* x^2 + y^2 on the FPU accumulator */
static inline float vecLenSqXY(_fvector *a)
{
    float *p = (float *)a;
    float r;

    __asm__("mula.s %1, %1\n\t"
            "madd.s %0, %2, %2"
            : "=f"(r) : "f"(p[0]), "f"(p[1]));
    return r;
}
/* dst = a - b * s (xyz) */
static inline void vecSubScaled(_fvector *dst, _fvector *a, _fvector *b, float s)
{
    register int t __asm__("$2");

    __asm__ volatile(".set push\n\t.set noreorder\n\t"
                     "lqc2 $vf11, 0x0(%2)\n\t"
                     "lqc2 $vf12, 0x0(%3)\n\t"
                     "mfc1 %1, %4\n\t"
                     "qmtc2.ni %1, $vf13\n\t"
                     "vaddax.xyz $ACC, $vf11, $vf0x\n\t"
                     "vmsubx.xyz $vf11, $vf12, $vf13x\n\t"
                     "sqc2 $vf11, %0\n\t"
                     ".set pop"
                     : "=m"(*dst), "=r"(t) : "r"(a), "r"(b), "f"(s));
}
/* abs.s as a scheduling barrier (retail does not interleave the two |x| * k of computeTargetSpeed) */
static inline float fabsV(float x)
{
    float r;

    __asm__ volatile("abs.s %0, %1" : "=f"(r) : "f"(x));
    return r;
}
#endif

MonsterDynamics::MonsterDynamics(Monster &m)
{
    m_owner = &m;
    m_configs[0].set(0.01f, 1.0f, 1.0f, 366.66669f, 352.0f, 750.0f, 1000.0f, 40.0f);
    m_configs[1].set(0.01f, 1.0f, 1.0f, 146.66667f, 117.333336f, 500.0f, 500.0f, 40.0f);
    m_configs[2].set(0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    m_configs[3].set(0.0f, 0.01f, 0.01f, 366.66669f, 352.0f, 750.0f, 1000.0f, 40.0f);
    m_configIdx = 0;
}
#ifdef NON_MATCHING
/* equivalent: 44/50 words, size 0xc8 vs 0xc8; the scheduler hoists the m_sideSpeed store above the others */
void MonsterDynamics::init(void)
{
    m_body.init(1.0f, &m_owner->m_cs->trans);
    vecSet(&m_cogOffset, 0.0f, 0.0f, 15.0f);
    vecZero(&m_angVel);
    vecZero(&m_angAccel);
    vecSet(&m_motionVec, 0.0f, 1.0f, 0.0f);
    vecZero(&m_velocity);
    m_moveScale = 1.0f;
    m_gravityScale = 1.0f;
    m_enabled = 1;
    m_speed = 0.0f;
    m_speedAccel = 0.0f;
    m_configIdx = 0;
    m_groundSpeed = 0.0f;
    m_fwdSpeed = 0.0f;
    m_sideSpeed = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", init__15MonsterDynamics);
#endif
void MonsterDynamics::allStop(void)
{
    vecZero(&m_angVel);
    vecZero(&m_angAccel);
    vecZero(&m_body.force);
    m_speedAccel = m_speed = 0.0f;
}
/* Eases the pitch and roll back to 0 and stops their rates. */
void MonsterDynamics::smoothToUpright(float rate)
{
    _fvector &rot = m_owner->m_cs->rot;

    rot.x = smoothEasyInTC(rot.x, 0.0f, rate, 0.01f);
    rot.y = smoothEasyInTC(rot.y, 0.0f, rate, 0.01f);
    vecSet(&m_angVel, 0.0f, 0.0f, m_angVel.z);
    vecSet(&m_angAccel, 0.0f, 0.0f, m_angAccel.z);
}
/* Turning away from the current direction loses speed: none straight ahead, all of it when reversing. */
void MonsterDynamics::setMotionVec(_fvector &v)
{
    m_speed *= vecDotI(&v, &m_motionVec) * 0.5f + 0.5f;
    if (m_speed < 0.0f)
        m_speed = 0.0f;
    vecCopy(&m_motionVec, &v);
}
#ifdef NON_MATCHING
/* equivalent: 41/43 words, size 0xac vs 0xac; the rsqrt result lands in $f0 instead of $f1 */
void MonsterDynamics::setMotionVecFromPad(void)
{
    FVECTOR v;
    float l2;

    m_owner->m_padFlags.computeMotionVec(v);
    l2 = vecLenSqF(&v);
    if (1e-10f < l2) {
        vecScale(&v, &v, 1.0f / sqrtf(l2));
        setMotionVec(v);
    } else {
        setMotionVec(*(_fvector *)UNIT_MATRIX[1]);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", setMotionVecFromPad__15MonsterDynamics);
#endif
void MonsterDynamics::setMotionRotFromPad(void)
{
    m_motionVec.w = m_owner->m_padFlags.computeMotionRot();
}
/* The speed the stick asks for: forward and strafe top speeds weighted by how far it is pushed on each axis. */
#ifdef NON_MATCHING
/* equivalent: 29/49 words, size 0xc4 vs 0xc4; FPR allocation of the two |x| * k and of the length */
float MonsterDynamics::computeTargetSpeed(float fwd, float strafe)
{
    FVECTOR v;
    _fvector *mv = &m_motionVec;

    m_owner->m_padFlags.computeMotionVec(v);
    strafe *= fabsV(v.x);
    fwd *= fabsV(v.y);
    if (strafe + fwd <= 1e-10f)
        return 0.0f;
    v.x = mv->x * strafe;
    v.y = mv->y * fwd;
    v.z = 0.0f;
    return sqrtf(vecLenSqF(&v));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", computeTargetSpeed__15MonsterDynamicsff);
#endif
void MonsterDynamics::updateMove(bool fromPad)
{
    float speed = 0.0f;
    Config *cfg = &m_configs[m_configIdx];

    if (fromPad) {
        setMotionVecFromPad();
        speed = computeTargetSpeed(cfg->runSpeed, cfg->strafeSpeed);
    }
    if (m_owner->m_freeFalling)
        updateAirMotion(speed, cfg->airAccel);
    else
        updateFootMotion(speed, cfg->footAccel, cfg->footDecel);
}
/* In the air the stick only steers: a force (accel, doubled for an AI) pulls the planar velocity toward the wanted one. */
void MonsterDynamics::updateAirMotion(float speed, float accel)
{
    Monster *m = m_owner;
    FVECTOR v;
    float l2, mod;

    if (m->m_playerNum == 2)
        accel += accel;
    if (m->m_unkF5)
        mod = m->m_rageSpeedMod;
    else if (m->m_boostFull)
        mod = m->m_boostSpeedMod;
    else
        goto noMod;
    if (0.0f < mod)
        speed *= mod;
noMod:
    vecApplyRows(&v, m_owner->m_cs->mat, &m_motionVec);
    v.x = v.x * speed - m_body.vel.x;
    v.y = v.y * speed - m_body.vel.y;
    v.z = 0.0f;
    l2 = vecLenSqF(&v);
    if (1e-10f < l2) {
        vecScale(&v, &v, accel / sqrtf(l2));
        vecAdd(&m_body.force, &m_body.force, &v);
    }
}
/* On foot m_speed chases the target speed (never below 35% of the run speed, 45% holding something large, once moving). */
#ifdef NON_MATCHING
/* equivalent: 77/86 words, size 0x158 vs 0x158; retail takes the max into a new FPR and loads m_speed before the test */
void MonsterDynamics::updateFootMotion(float speed, float accel, float decel)
{
    Monster *m = m_owner;
    float mod, cur;

    if (m->m_unkF5)
        mod = m->m_rageSpeedMod;
    else if (m->m_boostFull)
        mod = m->m_boostSpeedMod;
    else
        goto noMod;
    if (0.0f < mod)
        speed *= mod;
noMod:
    if (0.0f < speed) {
        float run = getConfig()->runSpeed;
        float min = m_owner->isHoldingLarge() ? run * 0.45f : run * 0.35f;

        speed = speed < min ? min : speed;
    }
    cur = m_speed;
    if (0.0f < speed && 0.0f < speed - cur) {
        m_speedAccel += accel - cur * accel / speed;
    } else if (decel < 10.0f) {
        m_speedAccel += cur * -decel;
    } else {
        m_speed = speed;
        m_speedAccel = 0.0f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", updateFootMotion__15MonsterDynamicsfff);
#endif
void MonsterDynamics::updateTurn(bool fromPad)
{
    Monster *m;

    if (fromPad)
        setMotionRotFromPad();
    else
        m_motionVec.w = 0.0f;
    m = m_owner;
    updateTurnMotion(m->m_turnMaxRate, m->m_turnAccel, m->m_turnDrag);
}
/* Heading rate: with input it accelerates toward maxRate * input (the input loses weight with speed, down to s_turnModMin at
   s_turnModZeroAtSpeed), without input it is damped by drag. */
#ifdef NON_MATCHING
/* equivalent: 22/82 words, size 0x13c vs 0x148; retail reaches m_angVel/m_angAccel through derived pointers and loads the gp floats in another order */
void MonsterDynamics::updateTurnMotion(float maxRate, float accel, float drag)
{
    float turn;

    if (m_configIdx == 3) {
        turn = m_motionVec.w;
    } else {
        turn = 1.0f - m_groundSpeed / s_turnModZeroAtSpeed;
        turn = (turn < s_turnModMin ? s_turnModMin : turn) * m_motionVec.w;
    }
    if (m_owner->m_playerNum == 2 && gUseUnifiedView) {
        maxRate *= s_aiTurnFactor;
        accel *= s_aiTurnFactor / timerGetFieldsLastFrame();
        drag *= s_aiTurnFactor / timerGetFieldsLastFrame();
    }
    if (1e-10f < __builtin_fabsf(turn)) {
        accel *= turn;
        m_angAccel.z += accel - m_angVel.z * (__builtin_fabsf(accel) / maxRate);
    } else {
        m_angAccel.z -= m_angVel.z * drag;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", updateTurnMotion__15MonsterDynamicsfff);
#endif
/* One physics step: push off a ledge the foot probe hangs over, integrate the body at the centre of gravity, the speed and the
   Euler angles, then scale the frame's displacement by m_moveScale. */
#ifdef NON_MATCHING
/* equivalent: 45/249 words, size 0x3d8 vs 0x3e4; retail keeps this in $s2 and &m_body.vel/&m_angAccel in $s0 across calls */
void MonsterDynamics::updatePosition(void)
{
    Monster *m;
    FVECTOR d;
    FVECTOR off;
    float dt;

    if (!m_enabled) {
        m_moveScale = 1.0f;
        return;
    }
    m = m_owner;
    if (*m->m_state != 0x1F && *(signed char *)((char *)m + 0x3B0)) {
        vecSub(&d, &((_hdResult *)m->getFootHDResult())->point, &m_owner->m_shadow->trans);
        if (5.0f < d.z) {
            register float len __asm__("$f3") = vecLenSqXY(&d);

            __asm__(".word 0x460300C4" : "+f"(len)); /* sqrt.s $f3, $f3 (R5900 form, see eeSqrtf) */
            d.w = len;
            if (0.01f < len && 1.0f < d.z / len) {
                d.z = 0.0f;
                vecAddScaled(&m_owner->m_cs->trans, &m_owner->m_cs->trans, &d, -s_pushMag / len);
            }
        }
    }
    m = m_owner;
    if (m->m_freeFalling && !(m->m_state[1] & 2)) {
        if ((m->m_state[1] & 0x10) && 0.0f < m_body.vel.z)
            m_body.vel.z = smoothEasyInTC(m_body.vel.z, 0.0f, 0.08f, 0.001f);
    }
    vecApplyRows(&off, m_owner->m_cs->mat, &m_cogOffset);
    vecAdd(&m_owner->m_cs->trans, &m_owner->m_cs->trans, &off);
    dt = 1.0f / timerGetUpdateRate();
    m_speed += m_speedAccel * dt;
    m_speedAccel = 0.0f;
    vecAddScaled(&m_angVel, &m_angVel, &m_angAccel, dt);
    vecAddScaled(&m_owner->m_cs->rot, &m_owner->m_cs->rot, &m_angVel, dt);
    boundEulerAngles(&m_owner->m_cs->rot);
    mathfRotMatrixRPH(m_owner->m_cs->mat, &m_owner->m_cs->rot);
    m_angAccel.x = 0.0f;
    m_angAccel.y = 0.0f;
    m_angAccel.z = 0.0f;
    m_body.force.z += game->m_gravity * m_gravityScale / m_body.force.w;
    m_body.resolveForcesEuler(dt);
    m_body.force.x = 0.0f;
    m_body.force.y = 0.0f;
    m_body.force.z = 0.0f;
    vecApplyRows(&d, m_owner->m_cs->mat, &m_cogOffset);
    vecSub(&m_owner->m_cs->trans, &m_owner->m_cs->trans, &d);
    vecSub(&d, &m_owner->m_cs->trans, &m_owner->m_prevTrans);
    d.x *= m_moveScale;
    d.y *= m_moveScale;
    d.z *= m_moveScale + (1.0f - m_moveScale) * 0.5f;
    vecAdd(&m_owner->m_cs->trans, &m_owner->m_prevTrans, &d);
    m_moveScale = 1.0f;
    vecApplyRows(&m_worldMotion, m_owner->m_cs->mat, &m_motionVec);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", updatePosition__15MonsterDynamics);
#endif
void MonsterDynamics::handleHeadCollis(_hdResult &res, float depth)
{
    vecAddScaled(&m_body.vel, &m_body.vel, &res.normal, -depth);
}
/* Against a wall: drop the velocity into the (horizontal) wall normal and cap the planar speed at the run speed. */
#ifdef NON_MATCHING
/* equivalent: 3/65 words, size 0xf0 vs 0x104; FPR allocation and order of the normalization */
void MonsterDynamics::handleBodyCollis(_hdResult &res, float depth)
{
    FVECTOR n;
    float inv, d, l2, max;

    if (m_configIdx == 3)
        return;
    n = res.normal;
    n.z = 0.0f;
    inv = 1.0f / sqrtf(vecLenSqF(&n));
    n.x *= inv;
    n.y *= inv;
    n.z *= inv;
    d = vecDotI(&n, &m_body.vel);
    if (d < 0.0f)
        vecAddScaled(&m_body.vel, &m_body.vel, &n, -d);
    l2 = vecLenSqXY(&m_body.vel);
    max = m_configs[m_configIdx].runSpeed;
    if (max * max < l2) {
        float k = max / sqrtf(l2);

        m_body.vel.x *= k;
        m_body.vel.y *= k;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", handleBodyCollis__15MonsterDynamicsR9_hdResultf);
#endif
/* On the ground: move with what we stand on, push out of the ground and apply the grip along and across the motion direction. */
#ifdef NON_MATCHING
/* equivalent: 79/160 words, size 0x27c vs 0x280; stack slots and register order of the dot products */
void MonsterDynamics::handleGroundCollis(_hdResult &res, float depth)
{
    FVECTOR cv, t, r, c;
    float inv;

    if (m_configIdx != 3 && *m_owner->m_state != 0x1F) {
        m_angVel.x = 0.0f;
        m_angVel.y = 0.0f;
        m_owner->m_cs->rot.x = 0.0f;
        m_owner->m_cs->rot.y = 0.0f;
    }
    getContactVel(cv, res);
    vecAddScaled(&cv, &cv, &res.normal, -vecDotI(&cv, &res.normal));
    vecSub(&m_body.vel, &m_body.vel, &cv);
    vecAddScaled(&m_body.vel, &m_body.vel, &res.normal, -depth);
    vecAddScaled(&t, &m_worldMotion, &res.normal,
                 -vecDotI(&m_worldMotion, &res.normal));
    inv = 1.0f / sqrtf(vecLenSqF(&t));
    t.z *= inv;
    t.x *= inv;
    t.y *= inv;
    vecSubScaled(&r, &m_body.vel, &t, m_speed);
    vecSubScaled(&m_body.vel, &m_body.vel, &t, vecDotI(&r, &t) * m_configs[m_configIdx].gripAlong);
    vecCross(&c, &t, &res.normal);
    vecSubScaled(&m_body.vel, &m_body.vel, &c, vecDotI(&r, &c) * m_configs[m_configIdx].gripAcross);
    vecAdd(&m_body.vel, &m_body.vel, &cv);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", handleGroundCollis__15MonsterDynamicsR9_hdResultf);
#endif
/* Velocity of what res touched: a rigid pickup works it out at the contact point, other interactives give their own. */
#ifdef NON_MATCHING
/* equivalent: 8/51 words, size 0xc8 vs 0xcc; retail extracts the ids with dsra32/lwu (signed 64-bit fields) and copies the vtable result through $v1 */
void MonsterDynamics::getContactVel(_fvector &out, _hdResult &res)
{
    HdResultIds &ids = (HdResultIds &)res;

    if (ids.id2) {
        DbInteractive *it = Interactives::getInteractive(ids.id2);

        if (it->m_kind == 5 && (it->m_unk50 >> 2 & 1)) {
            ((RigidPickup *)Interactives::getInteractive(res.ids & 0x7FF))->getContactVel(out, res);
        } else {
            char *vt = (char *)it->m_vptr;

            out = *((FVECTOR * (*)(void *))*(void **)(vt + 0x24))((char *)it + *(short *)(vt + 0x20));
        }
    } else {
        vecStoreUnitW(&out);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", getContactVel__15MonsterDynamicsR8_fvectorR9_hdResult);
#endif
#ifdef NON_MATCHING
/* equivalent: 15/61 words, size 0xf0 vs 0xf4; retail reuses m_velocity.x from the length for both dot products and stores m_groundSpeed first */
void MonsterDynamics::computeAuxiliaries(void)
{
    vecSub(&m_frameDelta, &m_owner->m_cs->trans, &m_owner->m_prevTrans);
    vecScale(&m_velocity, &m_frameDelta, (float)timerGetUpdateRate());
    {
        register float l __asm__("$f1") = vecLenSqF(&m_velocity);

        __asm__(".word 0x46010044" : "+f"(l)); /* sqrt.s $f1, $f1 (R5900 form, see eeSqrtf) */
        m_groundSpeed = l;
    }
    m_fwdSpeed = vecDotI(&m_velocity, (_fvector *)m_owner->m_cs->mat[1]);
    m_sideSpeed = vecDotI(&m_velocity, (_fvector *)m_owner->m_cs->mat[0]);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterDynamics", computeAuxiliaries__15MonsterDynamics);
#endif
float MonsterDynamics::getSpeedModifier(void)
{
    Monster *m = m_owner;

    if (m->m_boostFull && 0.0f < m->m_boostSpeedMod)
        return m->m_boostSpeedMod;
    return 1.0f;
}
MonsterDynamics::Config *MonsterDynamics::getConfig(void)
{
    return &m_configs[m_configIdx];
}
