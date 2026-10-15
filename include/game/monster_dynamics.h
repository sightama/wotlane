#ifndef MONSTER_DYNAMICS_H
#define MONSTER_DYNAMICS_H

#include "hieri_types.h"

class Monster;
struct _hdResult;

/* A point mass integrated with explicit Euler (common/PointMass). force.w holds 1 / mass. */
class PointMass {
public:
    _fvector *pos;          /* 0x0 the position it moves (the monster's cs trans) */
    char pad4[12];
    FVECTOR vel;            /* 0x10 */
    FVECTOR force;          /* 0x20 accumulated this frame; w = 1 / mass */

    PointMass() : pos(0) {}
    void init(float mass, _fvector *p);
    void resolveForcesEuler(float dt);
};

/* The movement physics of one monster, embedded in Monster at 0x100: the body is a PointMass on the cs trans (shifted up to
   the centre of gravity while it integrates), the heading turns through an angular velocity on the cs Euler angles, and the
   stick drives a target speed along m_motionVec. Four parameter sets (m_configs) select how it walks, the active one is
   m_configIdx (3 skips the collision response). */
class MonsterDynamics {
public:
    struct Config {
        float unk0;             /* 0x0: 0.01 (0 in sets 2 and 3), never read here */
        float gripAlong;        /* 0x4: ground friction along the motion direction */
        float gripAcross;       /* 0x8: ground friction across it */
        float runSpeed;         /* 0xC: forward top speed (stick y); also the planar speed cap against walls */
        float strafeSpeed;      /* 0x10: sideways top speed (stick x) */
        float airAccel;         /* 0x14: steering force while airborne */
        float footAccel;        /* 0x18 */
        float footDecel;        /* 0x1C: below 10 the speed decays by speed * decel, otherwise it snaps to the target */

        void set(float u0, float along, float across, float run, float strafe, float air, float accel, float decel)
        {
            unk0 = u0;
            gripAlong = along;
            gripAcross = across;
            runSpeed = run;
            strafeSpeed = strafe;
            airAccel = air;
            footAccel = accel;
            footDecel = decel;
        }
    };

    Monster *m_owner;           /* 0x0 */
    char pad4[0x20 - 0x4];
    FVECTOR m_cogOffset;        /* 0x20: centre of gravity in cs space (0, 0, 15) */
    int m_configIdx;            /* 0x30 */
    Config m_configs[4];        /* 0x34 */
    float m_moveScale;          /* 0xB4: scales this frame's displacement (z only by half of what it takes off); reset to 1 */
    float m_gravityScale;       /* 0xB8: Monster+0x1B8; StateBlock raises it to 1.5 while falling */
    char padBC[0xD0 - 0xBC];
    PointMass m_body;           /* 0xD0: vel is Monster+0x1E0 */
    FVECTOR m_angVel;           /* 0x100: rate of the cs Euler angles (per second) */
    FVECTOR m_angAccel;         /* 0x110: cleared after each integration */
    FVECTOR m_motionVec;        /* 0x120: wanted direction in cs space (unit, from the stick); w = turn input */
    FVECTOR m_worldMotion;      /* 0x130: m_motionVec in world space, updated by updatePosition */
    float m_speed;              /* 0x140: current target speed */
    float m_speedAccel;         /* 0x144: added to m_speed (per second) by updatePosition, then cleared */
    char pad148[0x14C - 0x148];
    int m_enabled;              /* 0x14C: 0 freezes updatePosition */
    float m_groundSpeed;        /* 0x150: |m_velocity| */
    float m_fwdSpeed;           /* 0x154: m_velocity along the cs y axis */
    float m_sideSpeed;          /* 0x158: m_velocity along the cs x axis */
    char pad15C[0x160 - 0x15C];
    FVECTOR m_velocity;         /* 0x160: measured velocity (per second) */
    FVECTOR m_frameDelta;       /* 0x170: cs trans - previous trans */

    static float s_turnModZeroAtSpeed;
    static float s_turnModMin;

    MonsterDynamics(Monster &m);
    void init(void);
    void allStop(void);
    void smoothToUpright(float rate);
    void setMotionVec(_fvector &v);
    void setMotionVecFromPad(void);
    void setMotionRotFromPad(void);
    float computeTargetSpeed(float fwd, float strafe);
    void updateMove(bool fromPad);
    void updateAirMotion(float speed, float accel);
    void updateFootMotion(float speed, float accel, float decel);
    void updateTurn(bool fromPad);
    void updateTurnMotion(float a, float b, float c);
    void updatePosition(void);
    void handleHeadCollis(_hdResult &res, float depth);
    void handleBodyCollis(_hdResult &res, float depth);
    void handleGroundCollis(_hdResult &res, float depth);
    void getContactVel(_fvector &out, _hdResult &res);
    void computeAuxiliaries(void);
    float getSpeedModifier(void);
    Config *getConfig(void);
};
typedef char _size_MonsterDynamics[sizeof(MonsterDynamics) == 0x180 ? 1 : -1];

#endif
