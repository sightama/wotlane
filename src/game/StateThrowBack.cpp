#include "common.h"
#include "game/game.h"
#include "game/monster_state.h"
#include "game/hit_event.h"
#include "game/hit_history.h"
#include "game/anim_blend.h"
#include "game/enemy_info.h"
#include "game/hud.h"
#include "rigid_body.h"
#include "task_manager.h"
#include "hd.h"
#include "vecmath.h"

/* Thrown, knocked back or slammed (Monster+0x7EA0 KnockBack, +0xDE30 BeingSlammed, +0xE3E0 BeingThrown, +0xE990 GrappleBreak):
 * the monster's cs becomes a RigidBody that flails through the air (SUB_FLAIL, the six additive AnimBlends bend the upper and lower
 * body with the spin), lands and slides (SUB_LANDED), lies on the ground (SUB_LYING: mashing the face buttons gets up sooner),
 * gets up (SUB_GETUP, or the getup attack) or counters while flailing (SUB_COUNTER), and leaves the state at SUB_DONE.
 * Damage comes from what the body hits while flailing (handleDamage). The derived states only differ in their transitionInto. */
class StateThrowBack : public MonsterState {
public:
    enum SubState { SUB_FLAIL, SUB_1, SUB_LANDED, SUB_LYING, SUB_GETUP, SUB_COUNTER, SUB_DONE };

    float f14;                  /* 0x14: 250 */
    float f18;                  /* 0x18: 20 */
    float lieBlendTime;         /* 0x1C: 96, blend into the lying animation */
    float wallDamageBase;       /* 0x20: first hit of something that is not the ground: base + speed * scale */
    float wallDamageScale;      /* 0x24 */
    float frictionScale;        /* 0x28: 0.3, friction of a contact is |local normal y| * this */
    float settleSpeed;          /* 0x2C: 200, below this sliding speed the body settles on the ground */
    float restitution;          /* 0x30 */
    int f34;                    /* 0x34 */
    int counterable;            /* 0x38: a counter can be started while flailing */
    float counterSpeed;         /* 0x3C: 2, speed of the counter animation */
    float counterRecover;       /* 0x40: 15, fields from the end of the counter animation to SUB_DONE */
    float f44;                  /* 0x44: 96 */
    float lieTime;              /* 0x48: 60, fields lying on the ground before getting up */
    float f4C;                  /* 0x4C: 64 */
    float groundDamageBase;     /* 0x50: first hit of the ground: base + speed * scale (+ fallBonus) */
    float groundDamageScale;    /* 0x54 */
    float mashStamina;          /* 0x58: 2, stamina credited per mash while lying */
    float mashSpeedup;          /* 0x5C: 0.1, fraction of lieTime one mash takes off */
    float getupFrames;          /* 0x60: 60, length of the getup animation */
    float f64;                  /* 0x64: 64 */
    float invulnFields;         /* 0x68: 60, invulnerability after getting up */
    float monsterDamageBase;    /* 0x6C: hitting another monster: base + speed * scale */
    float monsterDamageScale;   /* 0x70 */
    float minKnockLift;         /* 0x74: 250, lower bound of the lift given to a monster we fly into */
    float minKnockSpeed;        /* 0x78: 350, lower bound of its speed */
    float objectDamage;         /* 0x7C: 10, damage dealt to an object hit in the air (0 = 10) */
    float objectDamageGround;   /* 0x80: 10, same when the contact is ground-like */
    float fallBonus;            /* 0x84: extra damage of the first impact after a big fall */
    float bigFallHeight;        /* 0x88: a fall is big when maxHeight - z exceeds this */
    char pad8C[0x90 - 0x8C];
    RigidBody body;             /* 0x90: on the monster's cs */
    int subState;               /* 0x260 */
    int anim;                   /* 0x264: MonsterAnim playing */
    char pad268[0x270 - 0x268];
    FVECTOR orient;             /* 0x270: quaternion of the cs while flailing */
    FVECTOR groundNormal;       /* 0x280: normal of the last ground contact */
    FVECTOR wallNormal;         /* 0x290: normal of the last other contact */
    float collisions;           /* 0x2A0: body contacts so far (gravity is 1.5x once there is one) */
    int counterAttempts;        /* 0x2A4 */
    int wallHits;               /* 0x2A8 */
    int groundHits;             /* 0x2AC */
    float maxHeight;            /* 0x2B0: highest cs z since transitionInto */
    AnimBlend adds[6];          /* 0x2B4: upper body x/y/z (0..2), lower body x/y/z (3..5) */
    Monster *throwSource;       /* 0x524 */
    int mashing;                /* 0x528: a face button was held last frame */
    int counterEnd;             /* 0x52C: field count at which the counter is over */
    int counterReady;           /* 0x530: the monster has turned upside down enough to counter */
    int lieStart;               /* 0x534: field count when lying started (mashing moves it back) */
    float f538;                 /* 0x538: 33 */
    float pauseDuration;        /* 0x53C */
    char pad540[0x558 - 0x540];
    int getupAttack;            /* 0x558: an attack button was held while mashing: get up with StateGetupAttack */
    HitHistory hits;            /* 0x55C */

    StateThrowBack(int id, unsigned flags);
    void init(Monster *m);
    int transitionOK(void);
    void transitionInto(void);
    int takeStomp(void);
    void update(void);
    void enterSubState(SubState s);
    void updateFlail(void);
    void rampInAdds(float ms);
    void rampOutAdds(float ms);
    void updateDynamics(float dt);
    void updateUpperAdds(_fvector &v, float rate);
    void updateLowerAdds(_fvector &v, float rate);
    void rotAroundBaseSphere(_fvector &q);
    void handleCollis(_hdResult &r);
    void handleDamage(_hdResult &r);
    int acceptHit(HitEvent &e);
    void handlePreemption(MonsterState *next);
    int isCounterAvailable(void);
};
#define THROWBACK_AT(f, off) typedef char _throwback_at_##f[(unsigned)&((StateThrowBack *)0)->f == (off) ? 1 : -1]
THROWBACK_AT(body, 0x90);
THROWBACK_AT(subState, 0x260);
THROWBACK_AT(orient, 0x270);
THROWBACK_AT(collisions, 0x2A0);
THROWBACK_AT(adds, 0x2B4);
THROWBACK_AT(throwSource, 0x524);
THROWBACK_AT(getupAttack, 0x558);
THROWBACK_AT(hits, 0x55C);

class StateBeingSlammed : public StateThrowBack {
public:
    StateBeingSlammed(void);
    void transitionInto(void);
};
class StateBeingThrown : public StateThrowBack {
public:
    StateBeingThrown(void);
    void transitionInto(void);
};
class StateKnockBack : public StateThrowBack {
public:
    StateKnockBack(void);
    void transitionInto(void);
};
class StateGrappleBreak : public StateThrowBack {
public:
    StateGrappleBreak(void);
    void transitionInto(void);
};

class MonsterDynamics {
public:
    void allStop(void);
    void computeAuxiliaries(void);
    void smoothToUpright(float rate);
};
class MonsterSound {
public:
    void playKnockBackSound(void);
    void playBodyImpactSound(void);
};
class StateThrow {
public:
    void cancelOverride(void);
};
class StateGrapple {
public:
    void detach(void);
};
class StateGrappled {
public:
    void detach(void);
};
class StateGetupAttack : public MonsterState {
public:
    int transitionOK(void);
};
class FireBreath {
public:
    void ApplyMint(void);
};
class Interactives {
public:
    static Monster *getInteractive(int id);
    static void dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId);
};
struct ActuatorData {
    int w[8];
};
void inputSetActuator(int pad, ActuatorData *a);
int timerGetFieldCount(void);
void animationResume(_animHandle h);
extern "C" float cosf(float);
extern "C" float fabsf(float);
float mathfApproxCos2(float x);
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void mathfRPHFromMatrix(float (*m)[4], _fvector *rph);
void mathfMatrixToQuaternion(_fvector *q, float (*m)[4]);
void mathfMatrixFromNormal(float (*m)[4], _fvector *n);
void mathfRotationArcQuaternion(_fvector *q, _fvector *from, _fvector *to);
void mathfUnitizeQuaternion(_fvector *dst, _fvector *src, float maxAngle);
void mathfQuaternionMulTransVec(_fvector *dst, _fvector *q, _fvector *v);
int infoInFov(EnemyInfo::Info *info, float cosAngle) __asm__("inFov__9EnemyInfof");
int infoInLos(EnemyInfo::Info *info) __asm__("inLos__9EnemyInfo");
extern float UNIT_MATRIX[4][4];
extern float D_006F83B4; /* -FLT_MAX */

/* the Monster fields this TU touches that game.h does not name */
#define OWNER_DYN(m) ((MonsterDynamics *)((char *)(m) + 0x100))
#define OWNER_SOUND(m) ((MonsterSound *)(m)->m_sound)
#define OWNER_VEL(m) (*(FVECTOR *)((char *)(m) + 0x1E0))      /* MonsterDynamics + 0xE0 */
#define OWNER_I(m, o) (*(int *)((char *)(m) + (o)))
#define OWNER_F(m, o) (*(float *)((char *)(m) + (o)))
#define OWNER_V(m, o) (*(FVECTOR *)((char *)(m) + (o)))
#define STATE_OF(m, off) ((char *)(m) + (off))
#define ST_IDLE 0x7984
#define ST_GRAPPLE 0xDCE0
#define ST_GRAPPLED 0xDDCC
#define ST_THROW 0xEF40
#define ST_GETUP_ATTACK 0x10C40
/* Monster virtual getVel (vtable slot 0x20, entries {this delta, 0, function}) */
#define MONSTER_GET_VEL(m) \
    ((_fvector *)((void *(*)(void *))*(void **)(*(char **)((char *)(m) + 0x10) + 0x24))( \
        (char *)(m) + *(short *)(*(char **)((char *)(m) + 0x10) + 0x20)))
/* Cameras::m_cameras: one 0xEB0 block per view (camera cs at 0xCF0, POV at 0xCF8, matrix at 0xE70); the unified camera cs at
   0x2A50, the two close-combat monsters at 0x2B6C/0x2B70 and their matrix at 0x2BD0 */
struct CameraView {
    char pad0[0xCF0];
    _cs *cs;                    /* 0xCF0 */
    int padCF4;
    int pov;                    /* 0xCF8 */
    char padCFC[0xD98 - 0xCFC];
    int fD98;                   /* 0xD98 */
    int fD9C;                   /* 0xD9C */
    char padDA0[0xE70 - 0xDA0];
    float mat[4][4];            /* 0xE70 */
};
typedef char _size_CameraView[sizeof(CameraView) == 0xEB0 ? 1 : -1];
#define CAMS ((char *)&Cameras::m_cameras)
#define CAM_VIEW(v) (&((CameraView *)&Cameras::m_cameras)[v])
#define CAM_POV(v) (CAM_VIEW(v)->pov)
#define CAM_CC_A (*(Monster **)(CAMS + 0x2B6C))
#define CAM_CC_B (*(Monster **)(CAMS + 0x2B70))
/* dst = a + b * s (xyz) in VU0, the way retail inlines it */
#define VEC_MADD(dst, a, b, s)                                                                                    \
    __asm__ volatile("lqc2 $vf11, 0x0(%1)\n\tlqc2 $vf12, 0x0(%2)\n\tmfc1 $2, %3\n\tqmtc2.ni $2, $vf13\n\t"      \
                     "vaddax.xyz $ACC, $vf11, $vf0x\n\tvmaddx.xyz $vf11, $vf12, $vf13x\n\tsqc2 $vf11, %0"       \
                     : "=m"(*(dst)) : "r"(a), "r"(b), "f"(s) : "$2")
#define VEC_MSUB(dst, a, b, s)                                                                                    \
    __asm__ volatile("lqc2 $vf11, 0x0(%1)\n\tlqc2 $vf12, 0x0(%2)\n\tmfc1 $2, %3\n\tqmtc2.ni $2, $vf13\n\t"      \
                     "vaddax.xyz $ACC, $vf11, $vf0x\n\tvmsubx.xyz $vf11, $vf12, $vf13x\n\tsqc2 $vf11, %0"       \
                     : "=m"(*(dst)) : "r"(a), "r"(b), "f"(s) : "$2")
#define VEC_UNIT_W(dst) __asm__ volatile("sqc2 $vf0, %0" : "=m"(*(dst)))
/* a contact that counts as ground: surface type 7, or a normal pointing up more than 45 degrees that is not surface 10 or 11 */
#define HD_IS_GROUND(r) \
    (((r).surface >> 8) == 7 || ((r).normal.z > 0.70710677f && ((r).surface >> 8) != 10 && ((r).surface >> 8) != 11))

extern float s_vFactor;
__asm__("#SNFIX_SMALL s_vFactor");
extern float s_counterUpright __asm__("s_counterUpright.2448");
__asm__("#SNFIX_SMALL s_counterUpright.2448");
extern int s_maxCounterAttempts __asm__("s_maxCounterAttempts.2449");
__asm__("#SNFIX_SMALL s_maxCounterAttempts.2449");
extern float s_snapAlignThresh __asm__("s_snapAlignThresh.2477");
__asm__("#SNFIX_SMALL s_snapAlignThresh.2477");
extern float s_snapW __asm__("s_snapW.2478");
__asm__("#SNFIX_SMALL s_snapW.2478");
extern float s_collisX __asm__("s_collisX.2479");
__asm__("#SNFIX_SMALL s_collisX.2479");
extern float s_collisY __asm__("s_collisY.2480");
__asm__("#SNFIX_SMALL s_collisY.2480");
extern float s_collisZ __asm__("s_collisZ.2481");
__asm__("#SNFIX_SMALL s_collisZ.2481");
extern float s_collisForce __asm__("s_collisForce.2482");
__asm__("#SNFIX_SMALL s_collisForce.2482");
extern float s_pushMagX __asm__("s_pushMagX.2483");
__asm__("#SNFIX_SMALL s_pushMagX.2483");
extern float s_pushMagY __asm__("s_pushMagY.2484");
__asm__("#SNFIX_SMALL s_pushMagY.2484");

INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __14StateThrowBackQ212MonsterState2IdUi);
/* Big monsters (0x1E0) get a taller body. */
#ifdef NON_MATCHING
/* 14/73 words, untuned: register allocation (retail keeps &body in s1) */
void StateThrowBack::init(Monster *m)
{
    owner = m;
    body.cs = m->m_cs;
    if (m->m_typeBits == 0x1E0) {
        body.initCylindrical(45.0f, 150.0f, 2, 1.0f);
        body.setCenterOfMass(0.0f, 0.0f, 75.0f);
    } else {
        body.initCylindrical(45.0f, 100.0f, 2, 1.0f);
        body.setCenterOfMass(0.0f, 0.0f, 50.0f);
    }
    adds[0].init(owner->m_anims[0xFC]);
    adds[1].init(owner->m_anims[0xFB]);
    adds[2].init(owner->m_anims[0xFD]);
    adds[3].init(owner->m_anims[0xFF]);
    adds[4].init(owner->m_anims[0xFE]);
    adds[5].init(owner->m_anims[0x100]);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", init__14StateThrowBackP7Monster);
#endif
/* Needs both lying animations (0x57 and 0x58). */
int StateThrowBack::transitionOK(void)
{
    if (owner->m_anims[0x57].ci && owner->m_anims[0x58].ci)
        return 1;
    return 0;
}
#ifdef NON_MATCHING
/* 72/105 words, untuned: block order of the unified-view checks */
void StateThrowBack::transitionInto(void)
{
    maxHeight = D_006F83B4;
    frames = 0;
    lieStart = 0;
    throwSource = 0;
    collisions = 0;
    counterAttempts = 0;
    counterReady = 0;
    wallHits = 0;
    groundHits = 0;
    OWNER_I(owner, 0x130) = 2;
    OWNER_DYN(owner)->allStop();
    OWNER_I(owner, 0x24C) = 0;
    owner->m_attacksEnabled = 0;
    body.reset();
    hits.reset();
    ((StateThrow *)STATE_OF(owner, ST_THROW))->cancelOverride();
    owner->dropPickupImpaler();
    owner->dropPickup();
    if (id == 0x1D && gUseUnifiedView && game->m_numMonsters == 2 && game->m_gameMode != 8 && Cameras::m_cameras.m_state == 0 &&
        owner->m_playerNum == 1)
        Cameras::LeaveUnifiedView(0x10);
    if (owner->m_target) {
        ((StateGrappled *)STATE_OF(owner->m_target, ST_GRAPPLED))->detach();
        ((Monster *)owner->m_target)->enterNewState((MonsterState *)STATE_OF(owner->m_target, ST_IDLE));
        ((StateGrapple *)STATE_OF(owner, ST_GRAPPLE))->detach();
    }
    if (owner->m_grappler) {
        ((StateGrapple *)STATE_OF(owner->m_grappler, ST_GRAPPLE))->detach();
        owner->m_grappler->enterNewState((MonsterState *)STATE_OF(owner->m_grappler, ST_IDLE));
        ((StateGrappled *)STATE_OF(owner, ST_GRAPPLED))->detach();
    }
    if (*(int *)owner->m_fireBreath)
        ((FireBreath *)owner->m_fireBreath)->ApplyMint();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", transitionInto__14StateThrowBack);
#endif
/* A stomp only lands on a monster lying on the ground. */
int StateThrowBack::takeStomp(void)
{
    if ((unsigned)(subState - SUB_LANDED) < 2) {
        enterSubState(SUB_LYING);
        return 1;
    }
    return 0;
}
#ifdef NON_MATCHING
/* 4/512 words, untuned: switch layout and register allocation differ */
void StateThrowBack::update(void)
{
    MonsterState::update();
    /* the thrower lost sight of us: leave the unified view */
    if (gUseUnifiedView && Cameras::m_cameras.m_state != 1 && frames > 0x3C && throwSource && game->m_gameMode != 9 &&
        game->m_gameMode != 8) {
        if (game->m_numMonsters == 2 && owner->m_playerNum == 1 && throwSource->m_playerNum == 1) {
            if (!(EnemyInfo::getInfo(*throwSource, *owner)->f18 > 0.70710677f))
                Cameras::LeaveUnifiedView(6);
        }
    }
    switch (subState) {
    case SUB_FLAIL:
        updateFlail();
        if (counterReady) {
            if (owner->m_padFlags[0]->block && !owner->m_padFlags[1]->block) {
                if (counterAttempts < s_maxCounterAttempts && owner->m_cs->mat[2][2] > s_counterUpright)
                    enterSubState(SUB_COUNTER);
                counterAttempts++;
            }
        } else if (counterable && owner->m_cs->mat[2][2] <= s_counterUpright) {
            counterReady = 1;
        }
        if (anim != 0xFA && !animationIsRunning(owner->m_anims[anim])) {
            anim = 0xFA;
            animationTransitionInto(owner->m_anims[0xFA], 100.0f, 1, 1);
        }
        break;
    case SUB_LANDED:
    case SUB_LYING: {
        int mash = 0;

        updateDynamics(1.0f / timerGetUpdateRate());
        if (owner->m_padFlags[0]->face[0] || owner->m_padFlags[0]->face[3] || owner->m_padFlags[0]->face[2] ||
            owner->m_padFlags[0]->face[1])
            mash = 1;
        if (mash && !mashing) {
            int attack = 0;

            owner->creditStamina(mashStamina, true);
            lieStart -= (int)(lieTime * mashSpeedup);
            if (*(unsigned short *)((char *)owner->m_padFlags[0] + 0x2C) || *(unsigned short *)((char *)owner->m_padFlags[0] + 0x30))
                attack = 1;
            getupAttack = attack;
        }
        mashing = mash;
        if ((float)(timerGetFieldCount() - lieStart) >= lieTime || (owner->m_dead && timerGetFieldCount() - lieStart >= 10))
            enterSubState(SUB_GETUP);
        if (!gUseUnifiedView)
            break;
        if (CAM_CC_A != owner && CAM_CC_B != owner)
            break;
        if (Cameras::ccMonsNoLongerInRange(0.0f))
            break;
        if (Cameras::m_cameras.m_state == 1)
            break;
        if (CAM_CC_A->m_playerNum == 2)
            Cameras::InitCCMonsters(CAM_CC_B, CAM_CC_A);
        if (game->m_numMonsters == 2) {
            Cameras::InitCCCamera(CAM_CC_A, CAM_CC_B);
        } else if (owner->m_playerNum == 1) {
            /* one player against AIs: stay unified only while an AI that is free to act sees us */
            int i;
            Monster *seen = 0;
            float c = cosf(OWNER_F(owner, 0x6E24));

            for (i = 0; i < game->m_numAIs; i++) {
                Monster *m = game->m_lists.ais[i];
                EnemyInfo::Info *info = EnemyInfo::getInfo(*owner, *m);
                int visible = 0;

                if (infoInFov(info, c))
                    visible = infoInLos(info) != 0;
                if (visible && !m->m_dead && !m->m_freeFalling && !m->m_target) {
                    int st = *m->m_state;

                    if (st != 0x26 && st != 0x27 && st != 0x28)
                        seen = m;
                }
            }
            if (!seen)
                Cameras::LeaveUnifiedView(0x12);
        } else if (!(((MonsterState *)CAM_CC_A->m_state)->flags & 0x10)) {
            Cameras::InitCCCamera(CAM_CC_A, CAM_CC_B);
        }
        break;
    }
    case SUB_GETUP:
        updateDynamics(1.0f / timerGetUpdateRate());
        if (animationIsRunning(owner->m_anims[anim])) {
            FVECTOR q;

            mathfRotationArcQuaternion(&q, (_fvector *)owner->m_cs->mat[2], (_fvector *)UNIT_MATRIX[2]);
            mathfUnitizeQuaternion(&q, &q, 0.05f);
            rotAroundBaseSphere(q);
        } else {
            enterSubState(SUB_DONE);
        }
        break;
    case SUB_COUNTER: {
        Monster *m = owner;
        MonsterDynamics *dyn = OWNER_DYN(m);
        _animHandle *h = &m->m_anims[anim];

        if (animationIsRunning(*h)) {
            float p = animationGetCurrentPercent(*h);
            float t = 0.0f;
            float s;

            if (p > 1.0f)
                p = 1.0f;
            if (p < 0.0f)
                p = 0.0f;
            if (!(p < 0.5f))
                t = (p - 0.5f) + (p - 0.5f);
            s = (mathfApproxCos2(t) + 1.0f) * 0.5f;
            vecScale(&OWNER_VEL(m), &OWNER_VEL(m), s);
            counterEnd = timerGetFieldCount() + (int)counterRecover;
        } else {
            VEC_UNIT_W(&OWNER_VEL(m));
            if (counterEnd < timerGetFieldCount())
                enterSubState(SUB_DONE);
        }
        dyn->smoothToUpright(0.2f);
        break;
    }
    }
    if (owner->m_cs->trans.z > maxHeight)
        maxHeight = owner->m_cs->trans.z;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", update__14StateThrowBack);
#endif
#ifdef NON_MATCHING
/* 19/749 words, untuned: register allocation and block order */
void StateThrowBack::enterSubState(SubState s)
{
    switch (s) {
    case SUB_FLAIL: {
        FVECTOR spin;
        FVECTOR L;

        flags |= 0x20;
        spin.x = mathfRandf(0.2f, 1.0f);
        spin.y = mathfRandf(-0.4f, 0.4f);
        spin.z = mathfRandf(-0.6f, 0.6f);
        vecScale(&spin, &spin, 8000.0f);
        vecApplyRows(&L, owner->m_cs->mat, &spin);
        body.P = OWNER_VEL(owner);
        body.L = L;
        if (owner->m_playerNum == 1 && game->m_gameMode != 9) {
            int pov;

            if (gUseUnifiedView) {
                _cs *from = *(_cs **)(CAMS + 0x2A50);
                _cs *to = CAM_VIEW(owner->m_cameraView)->cs;

                to->trans = from->trans;
                to->rot = from->rot;
                matCopy(to->mat, from->mat);
            }
            pov = *((signed char *)owner + 0xF8) ? 8 : 5;
            if (CAM_POV(gUseUnifiedView ? 2 : owner->m_cameraView) != 3 &&
                CAM_POV(gUseUnifiedView ? 2 : owner->m_cameraView) != pov) {
                Cameras::SetCameraPOV(owner->m_cameraView, (Camera::CameraPOV)pov);
                if (gUseUnifiedView && game->m_numMonsters == 1)
                    Cameras::SetCameraPOV(2, (Camera::CameraPOV)pov);
            }
        }
        VEC_UNIT_W(&OWNER_VEL(owner));
        anim = 0xFA;
        animationTransitionInto(owner->m_anims[0xFA], 100.0f, 1, 1);
        rampInAdds(100.0f);
        break;
    }
    case SUB_LANDED: {
        FVECTOR up;
        FVECTOR q;
        Monster *m;

        flags &= ~0x20;
        VEC_UNIT_W(&body.v);
        VEC_UNIT_W(&body.P);
        VEC_UNIT_W(&body.w);
        VEC_UNIT_W(&body.L);
        VEC_UNIT_W(&OWNER_VEL(owner));
        /* settle on the side we landed on: turn the cs y axis (or its opposite) onto the ground normal */
        mathfMatrixToQuaternion(&orient, owner->m_cs->mat);
        vecScale(&up, (_fvector *)owner->m_cs->mat[1], owner->m_cs->mat[1][2] <= 0.0f ? -1.0f : 1.0f);
        mathfRotationArcQuaternion(&q, &up, &groundNormal);
        rotAroundBaseSphere(q);
        lieStart = timerGetFieldCount();
        mashing = 0;
        getupAttack = 0;
        anim = owner->m_cs->mat[1][2] <= 0.0f ? 0x55 : 0x56;
        animationSetTotalRunFrames(owner->m_anims[anim], 60.0f);
        animationTransitionInto(owner->m_anims[anim], 48.0f, 1, 1);
        rampOutAdds(100.0f);
        m = owner;
        if (m->m_playerNum == 1 && game->m_gameMode != 9 && game->m_gameMode != 8) {
            /* point this player's camera along the body, seen from the side it lies on */
            int numMonsters = game->m_numMonsters;
            int view = m->m_cameraView;
            FVECTOR dir;
            FVECTOR q2;
            float inv;
            char *cam;
            int pov;

            vecScale(&dir, (_fvector *)m->m_cs->mat[1], m->m_cs->mat[1][2] <= 0.0f ? -1.0f : 1.0f);
            mathfRotationArcQuaternion(&q2, &dir, &groundNormal);
            mathfQuaternionMulTransVec(&dir, &q2, (_fvector *)owner->m_cs->mat[2]);
            dir.z = 0.0f;
            inv = eeRsqrtf(1.0f, dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            dir.x *= inv;
            dir.y *= inv;
            dir.z *= inv;
            vecScale(&dir, &dir, owner->m_cs->mat[1][2] <= 0.0f ? 1.0f : -1.0f);
            cam = (char *)CAM_VIEW(view)->mat;
            mathfMatrixFromNormal((float (*)[4])cam, &dir);
            if (numMonsters == 1)
                matCopy(CAMS + 0x2BD0, cam);
            pov = CAM_POV(gUseUnifiedView ? 2 : view);
            if (pov != 4 && pov != 3) {
                Cameras::SetCameraPOV(view, (Camera::CameraPOV)4);
                if (numMonsters == 1)
                    Cameras::SetCameraPOV(2, (Camera::CameraPOV)4);
            }
        }
        if (gUseUnifiedView && Cameras::m_cameras.m_state == 1 && game->m_gameMode != 9 && game->m_gameMode != 8 &&
            Cameras::ccMonsNoLongerInRange(1250.0f))
            Cameras::LeaveUnifiedView(8);
        OWNER_SOUND(owner)->playBodyImpactSound();
        break;
    }
    case SUB_LYING:
        flags &= ~0x20;
        if (subState == SUB_LYING) {
            /* stomped while lying: freeze the pose */
            animationPause(owner->m_anims[anim]);
        } else {
            lieStart = timerGetFieldCount();
            mashing = 0;
            getupAttack = 0;
            anim = anim == 0x56 ? 0x50 : 0x51;
        }
        animationTransitionInto(owner->m_anims[anim], lieBlendTime, 1, 1);
        break;
    case SUB_GETUP: {
        FVECTOR q;
        _cs *cs;

        flags &= ~0x20;
        anim = owner->m_cs->mat[1][2] <= 0.0f ? 0x57 : 0x58;
        animationSetTotalRunFrames(owner->m_anims[anim], getupFrames);
        animationStart(owner->m_anims[anim], true);
        cs = owner->m_cs;
        VEC_MADD(&cs->trans, &cs->trans, cs->mat[2], 50.0f);
        mathfRotationArcQuaternion(&q, (_fvector *)owner->m_cs->mat[2], &groundNormal);
        mathfConcatQuaternions(&orient, &orient, &q);
        mathfNormalizeQuaternion(&orient, &orient);
        mathfQuaternionToMatrix4x4(owner->m_cs->mat, &orient);
        cs = owner->m_cs;
        VEC_MSUB(&cs->trans, &cs->trans, &groundNormal, OWNER_F(owner, 0x37C));
        /* rebuild the monster's own frame (0x50 side, 0x60 forward, 0x70 up, 0x90 position) from the cs */
        OWNER_V(owner, 0x90) = owner->m_cs->trans;
        OWNER_V(owner, 0x70) = *(FVECTOR *)UNIT_MATRIX[2];
        OWNER_V(owner, 0x60) = *(FVECTOR *)owner->m_cs->mat[1];
        OWNER_F(owner, 0x68) = 0.0f;
        {
            _fvector *f = &OWNER_V(owner, 0x60);
            float inv = eeRsqrtf(1.0f, f->x * f->x + f->y * f->y + f->z * f->z);

            f->x *= inv;
            f->z *= inv;
            f->y *= inv;
        }
        vecCross(&OWNER_V(owner, 0x50), &OWNER_V(owner, 0x60), (_fvector *)UNIT_MATRIX[2]);
        if (getupAttack && ((StateGetupAttack *)STATE_OF(owner, ST_GETUP_ATTACK))->transitionOK()) {
            owner->enterNewState((MonsterState *)STATE_OF(owner, ST_GETUP_ATTACK));
        } else {
            owner->m_unk49 = 0;
            TaskManager::global.add((unsigned (*)(void *))&Monster::restoreVulnerability, owner, (int)invulnFields);
        }
        break;
    }
    case SUB_COUNTER: {
        Monster *m;

        flags &= ~0x20;
        gameHud(owner->m_cameraView)->addMessage(0x20, 0);
        anim = 0x52;
        m = owner;
        rampOutAdds(100.0f);
        animationSetSpeed(m->m_anims[0x52], counterSpeed);
        animationTransitionInto(m->m_anims[0x52], 100.0f, 1, 7);
        OWNER_I(owner, 0x24C) = 1;
        OWNER_I(owner, 0x130) = 0;
        OWNER_VEL(owner) = body.v;
        mathfMatrixToQuaternion(&orient, owner->m_cs->mat);
        break;
    }
    case SUB_DONE:
        VEC_UNIT_W(&OWNER_VEL(owner));
        if (owner->m_freeFalling)
            owner->enterNewState((MonsterState *)owner->m_stateRef);
        else
            owner->enterNewState((MonsterState *)STATE_OF(owner, ST_IDLE));
        if (gUseUnifiedView && game->m_gameMode != 9 && game->m_gameMode != 8 && (CAM_CC_A == owner || CAM_CC_B == owner)) {
            if (Cameras::ccMonsNoLongerInRange(0.0f)) {
                if (game->m_numMonsters == 2 || owner->m_playerNum == 1)
                    Cameras::LeaveUnifiedView(7);
            } else if (Cameras::m_cameras.m_state != 1) {
                if (CAM_CC_A->m_playerNum == 2)
                    Cameras::InitCCMonsters(CAM_CC_B, CAM_CC_A);
                if (game->m_numMonsters == 2)
                    Cameras::InitCCCamera(CAM_CC_A, CAM_CC_B);
                else if (CAM_CC_A == owner)
                    Cameras::InitCCCamera(CAM_CC_A, CAM_CC_B);
            }
        }
        break;
    }
    subState = s;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", enterSubState__14StateThrowBackQ214StateThrowBack8SubState);
#endif
/* The body spins freely; its point velocities at the chest and at the base, in the cs frame, bend the additive layers. */
#ifdef NON_MATCHING
/* 1/111 words, untuned: stack layout of the locals and register allocation */
void StateThrowBack::updateFlail(void)
{
    FVECTOR p;
    FVECTOR v;
    FVECTOR local;
    _animHandle *h;

    updateDynamics(1.0f / timerGetUpdateRate());
    OWNER_VEL(owner) = body.v;
    OWNER_DYN(owner)->computeAuxiliaries();
    if (owner->m_fallTime < 6)
        return;
    VEC_MADD(&p, &owner->m_cs->trans, owner->m_cs->mat[2], 100.0f);
    body.getPointVelocity(v, p);
    mathfMulVec(owner->m_cs->mat, &v, &local);
    updateUpperAdds(local, 0.5f);
    body.getPointVelocity(v, owner->m_cs->trans);
    mathfMulVec(owner->m_cs->mat, &v, &local);
    updateLowerAdds(local, 0.5f);
    h = &owner->m_anims[anim];
    if (!animationIsRunning(*h))
        animationResume(*h);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", updateFlail__14StateThrowBack);
#endif
void StateThrowBack::rampInAdds(float ms)
{
    adds[0].rampIn(ms);
    adds[0].setSpeed(0.0f);
    adds[0].setPercent(0.5f);
    adds[1].rampIn(ms);
    adds[1].setSpeed(0.0f);
    adds[1].setPercent(0.5f);
    adds[2].rampIn(ms);
    adds[2].setSpeed(0.0f);
    adds[2].setPercent(0.5f);
    adds[3].rampIn(ms);
    adds[3].setSpeed(0.0f);
    adds[3].setPercent(0.5f);
    adds[4].rampIn(ms);
    adds[4].setSpeed(0.0f);
    adds[4].setPercent(0.5f);
    adds[5].rampIn(ms);
    adds[5].setSpeed(0.0f);
    adds[5].setPercent(0.5f);
}
void StateThrowBack::rampOutAdds(float ms)
{
    adds[0].rampOut(ms);
    adds[1].rampOut(ms);
    adds[2].rampOut(ms);
    adds[3].rampOut(ms);
    adds[4].rampOut(ms);
    adds[5].rampOut(ms);
}
/* Gravity is 1.5x once the body has hit something (or, except for state 0x17, while falling). */
#ifdef NON_MATCHING
/* 22/54 words, untuned: shape of the gravity condition */
void StateThrowBack::updateDynamics(float dt)
{
    float g;

    body.updateFromCS();
    g = game->m_gravity;
    if (id == 0x17 ? collisions > 0.0f : (body.v.z < 0.0f || collisions > 0.0f))
        g *= 1.5f;
    body.applyGravForce(g);
    body.resolveForces(dt);
    body.clearForces();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", updateDynamics__14StateThrowBackf);
#endif
/* The additive layers are at 0.5 when still; v (a local velocity) scaled by -s_vFactor and clamped to length 0.5 moves them. */
#ifdef NON_MATCHING
/* 7/91 words, untuned: retail computes the length into $f1 and stores it in v.w; register allocation */
void StateThrowBack::updateUpperAdds(_fvector &vel, float rate)
{
    FVECTOR v;
    float max = 0.5f;

    vecScale(&v, &vel, -s_vFactor);
    v.w = v.x * v.x + v.y * v.y + v.z * v.z;
    if (v.w > max * max)
        vecScale(&v, &v, eeRsqrtf(max, v.w));
    v.x = smoothEasyInTC(adds[0].getPercent(), -v.x + max, rate, 0.01f);
    adds[0].setPercent(v.x);
    v.y = smoothEasyInTC(adds[1].getPercent(), v.y + max, rate, 0.01f);
    adds[1].setPercent(v.y);
    v.z = smoothEasyInTC(adds[2].getPercent(), v.z + max, rate, 0.01f);
    adds[2].setPercent(v.z);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", updateUpperAdds__14StateThrowBackR8_fvectorf);
#endif
#ifdef NON_MATCHING
/* 7/90 words, untuned: same as updateUpperAdds */
void StateThrowBack::updateLowerAdds(_fvector &vel, float rate)
{
    FVECTOR v;
    float max = 0.5f;

    vecScale(&v, &vel, -s_vFactor);
    v.w = v.x * v.x + v.y * v.y + v.z * v.z;
    if (v.w > max * max)
        vecScale(&v, &v, eeRsqrtf(max, v.w));
    v.x = smoothEasyInTC(adds[3].getPercent(), v.x + max, rate, 0.01f);
    adds[3].setPercent(v.x);
    v.y = smoothEasyInTC(adds[4].getPercent(), v.y + max, rate, 0.01f);
    adds[4].setPercent(v.y);
    v.z = smoothEasyInTC(adds[5].getPercent(), v.z + max, rate, 0.01f);
    adds[5].setPercent(v.z);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", updateLowerAdds__14StateThrowBackR8_fvectorf);
#endif
/* Rotates the cs by q around the point owner+0x2BC above the origin along the cs z axis. */
#ifdef NON_MATCHING
/* 35/49 words: only the registers of the inline VU0 multiply-add differ */
void StateThrowBack::rotAroundBaseSphere(_fvector &q)
{
    _cs *cs = owner->m_cs;
    float h = OWNER_F(owner, 0x2BC);

    VEC_MADD(&cs->trans, &cs->trans, cs->mat[2], h);
    mathfConcatQuaternions(&orient, &orient, &q);
    mathfNormalizeQuaternion(&orient, &orient);
    mathfQuaternionToMatrix4x4(owner->m_cs->mat, &orient);
    cs = owner->m_cs;
    VEC_MADD(&cs->trans, &cs->trans, cs->mat[2], -h);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", rotAroundBaseSphere__14StateThrowBackR8_fvector);
#endif
#ifdef NON_MATCHING
/* 5/456 words, untuned: register allocation and block order */
void StateThrowBack::handleCollis(_hdResult &r)
{
    MonsterState::handleCollis(r);
    if (((r.ids >> 11) & 0x3FFF) == 0x2744)
        return;
    if ((r.ids & 0x7FF) && !(Interactives::getInteractive(r.ids & 0x7FF)->m_flags & 8))
        return;
    if (subState == SUB_FLAIL) {
        handleDamage(r);
        animationPause(owner->m_anims[anim]);
        if ((unsigned)(((r.ids >> 11) & 0x3FFF) - 0x20) <= 0x3E0) {
            /* flew into another monster: knock it back along our ground velocity */
            Monster *m;
            FVECTOR v;
            float speed;
            float lift;

            if (id != 0x17)
                return;
            m = Interactives::getInteractive(r.ids & 0x7FF);
            if (!(((MonsterState *)m->m_state)->flags & 0x10) && !(throwSource && throwSource == m)) {
                v = *MONSTER_GET_VEL(owner);
                v.w = eeSqrtf(v.x * v.x + v.y * v.y);
                speed = v.w > minKnockSpeed ? v.w : minKnockSpeed;
                lift = MONSTER_GET_VEL(owner)->z;
                if (lift < minKnockLift)
                    lift = minKnockLift;
                vecScale(&v, &v, 1.0f / v.w);
                v.z = 0.0f;
                m->knockBack(v, lift, speed);
                if (m->m_flags & 0x10)
                    *(Monster **)((char *)m->m_state + 0x524) = throwSource;
            }
        } else {
            FVECTOR n;
            FVECTOR base;
            FVECTOR p;
            FVECTOR d;
            FVECTOR ax;
            FVECTOR ay;
            FVECTOR az;
            float k;
            float mu;
            int hit = 0;

            /* push the contact back through three corners of a box around the body (two when the contact is near x = 0) */
            mathfMulVec(owner->m_cs->mat, &r.normal, &n);
            VEC_MADD(&base, &owner->m_cs->trans, owner->m_cs->mat[2], 50.0f);
            vecSub(&p, &base, &r.point);
            mathfMulVec(owner->m_cs->mat, &p, &d);
            k = fabsf(n.y) * (2.0f - fabsf(n.y));
            vecScale(&ax, (_fvector *)owner->m_cs->mat[0], (d.x >= 0.0f ? -s_collisX : s_collisX) * k);
            vecScale(&ay, (_fvector *)owner->m_cs->mat[1], d.y >= 0.0f ? s_collisY : -s_collisY);
            vecScale(&az, (_fvector *)owner->m_cs->mat[2], d.z >= 0.0f ? -s_collisZ : s_collisZ);
            mu = fabsf(n.y) * frictionScale;
            vecAdd(&p, &base, &ax);
            vecAdd(&p, &p, &ay);
            vecAdd(&p, &p, &az);
            hit |= body.resolveCollision(p, r.normal, restitution, mu, mu) != 0;
            if (fabsf(d.x) < 0.1f) {
                vecSub(&p, &base, &ax);
                vecAdd(&p, &p, &ay);
                vecAdd(&p, &p, &az);
                hit = (hit != 0) | (body.resolveCollision(p, r.normal, restitution, mu, mu) != 0);
            }
            if (hit)
                collisions += 1.0f;
            d.x = -s_pushMagX * n.x;
            d.y = -s_pushMagY * fabsf(n.z) * (n.y < 0.0f ? -1.0f : 1.0f);
            d.z = 0.0f;
            updateUpperAdds(d, 0.1f);
            d.x = -d.x;
            updateLowerAdds(d, 0.1f);
            if (HD_IS_GROUND(r)) {
                owner->m_freeFalling = 0;
                groundNormal = r.normal;
                if (fabsf(n.y) > s_snapAlignThresh) {
                    /* lying flat and nearly still: settle */
                    float dn = r.normal.x * body.v.x + r.normal.y * body.v.y + r.normal.z * body.v.z;
                    FVECTOR t;

                    VEC_MADD(&t, &body.v, &r.normal, -dn);
                    if (t.x * t.x + t.y * t.y + t.z * t.z < settleSpeed * settleSpeed &&
                        body.w.x * body.w.x + body.w.y * body.w.y + body.w.z * body.w.z < s_snapW * s_snapW)
                        enterSubState(SUB_LANDED);
                }
                return;
            }
            wallNormal = r.normal;
            if (hit) {
                FVECTOR f;

                vecScale(&f, &wallNormal, s_collisForce * fabsf(n.y));
                body.applyWorldForce(f);
            }
        }
        OWNER_SOUND(owner)->playBodyImpactSound();
        return;
    }
    VEC_UNIT_W(&body.v);
    VEC_UNIT_W(&body.P);
    VEC_UNIT_W(&body.w);
    VEC_UNIT_W(&body.L);
    if (HD_IS_GROUND(r))
        owner->m_freeFalling = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", handleCollis__14StateThrowBackR9_hdResult);
#endif
/* The first impact on the ground and the first on anything else hurt (more after a big fall), as does every new monster hit;
   whatever we hit takes damage too. */
#ifdef NON_MATCHING
/* 13/347 words, untuned: register allocation and block order */
void StateThrowBack::handleDamage(_hdResult &r)
{
    FVECTOR pv;
    float speed;
    int bigFall;
    float dmg;
    HitEvent *e;
    Monster *it;
    int attacker;

    if (throwSource && throwSource->m_id == (int)(r.ids & 0x7FF))
        return;
    body.getPointVelocity(pv, r.point);
    speed = r.normal.x * pv.x + r.normal.y * pv.y + r.normal.z * pv.z;
    bigFall = bigFallHeight < maxHeight - owner->m_cs->trans.z;
    dmg = 0.0f;
    if ((r.ids & 0x7FF) && hits.newHit(r.ids & 0x7FF, false) && *(int *)Interactives::getInteractive(r.ids & 0x7FF) == 1) {
        dmg = monsterDamageBase + speed * monsterDamageScale;
    } else if (HD_IS_GROUND(r)) {
        if (++groundHits == 1 && wallHits == 0) {
            dmg = groundDamageBase + speed * groundDamageScale;
            dmg += bigFall ? fallBonus : 0.0f;
        }
    } else {
        if (++wallHits == 1 && groundHits == 0) {
            dmg = wallDamageBase + speed * wallDamageScale;
            dmg += bigFall ? fallBonus : 0.0f;
        }
    }
    if (!(dmg > 0.0f))
        return;
    if (bigFall)
        Cameras::StartShake(owner->m_cs->trans, 20.0f);
    if (owner->m_playerNum == 1) {
        ActuatorData a = *(ActuatorData *)((char *)game + 0x1227E0);

        inputSetActuator(owner->m_index, &a);
    }
    if (game->m_numMonsters == 2) {
        int other = owner->m_index == 0;

        if (EnemyInfo::getInfo(*game->m_monsters[other], *owner)->f18 > 0.70710677f || gUseUnifiedView) {
            ActuatorData a = *(ActuatorData *)((char *)game + 0x1227E0);

            inputSetActuator(other, &a);
        }
    }
    e = HitEvent::create();
    e->pos = &r.point;
    e->source = 0xD;
    e->result = &r;
    e->type = 0;
    Interactives::dispatchHit(&r.normal, dmg, throwSource ? throwSource->m_id : (int)(r.ids & 0x7FF), owner->m_id);
    if (!(r.ids & 0x7FF))
        return;
    it = Interactives::getInteractive(r.ids & 0x7FF);
    e->source = 8;
    vecScale(&r.normal, &r.normal, -1.0f);
    attacker = owner->m_id;
    if (*(int *)it == 3) {
        float d = HD_IS_GROUND(r) ? objectDamageGround : objectDamage;

        dmg = d == 0.0f ? 10.0f : d;
    } else if (*(int *)it == 1 && throwSource) {
        attacker = throwSource->m_id;
    }
    Interactives::dispatchHit(&r.normal, dmg, attacker, r.ids & 0x7FF);
    vecScale(&r.normal, &r.normal, -1.0f);
    if (throwSource && *(int *)Interactives::getInteractive(r.ids & 0x7FF) == 1)
        throwSource->registerComboHit(Interactives::getInteractive(r.ids & 0x7FF));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", handleDamage__14StateThrowBackR9_hdResult);
#endif
/* Flailing: hits of types 1, 2, 5, 6, 8, 9, 11 and 13. Lying: type 1, and the shock (5) of source arg 0x140. Getting up: 1, 5, 7, 8,
   14 and 17. */
#ifdef NON_MATCHING
/* 27/53 words, untuned: retail tests the sub-state with range compares before the jump tables */
int StateThrowBack::acceptHit(HitEvent &e)
{
    switch (subState) {
    case SUB_FLAIL:
        switch (e.source) {
        case 1:
        case 2:
        case 5:
        case 6:
        case 8:
        case 9:
        case 11:
        case 13:
            return 1;
        }
        return 0;
    case SUB_LANDED:
    case SUB_LYING:
        if (e.source == 1)
            return 1;
        if (e.source == 5)
            return e.sourceArg == 0x140;
        return 0;
    case SUB_GETUP:
        switch (e.source) {
        case 1:
        case 5:
        case 7:
        case 8:
        case 14:
        case 17:
            return 1;
        }
        return 0;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", acceptHit__14StateThrowBackR8HitEvent);
#endif
void StateThrowBack::handlePreemption(MonsterState *next)
{
    OWNER_I(owner, 0x130) = 0;
    OWNER_I(owner, 0x24C) = 1;
    owner->m_attacksEnabled = 1;
    mathfRPHFromMatrix(owner->m_cs->mat, &owner->m_cs->rot);
    rampOutAdds(100.0f);
    if (game->m_gameMode != 9) {
        Cameras::SetCameraPOV(owner->m_cameraView, (Camera::CameraPOV)0);
        CAM_VIEW(owner->m_cameraView)->fD9C = 0;
        CAM_VIEW(owner->m_cameraView)->fD98 = 0;
    }
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __17StateBeingSlammed);
void StateBeingSlammed::transitionInto(void)
{
    StateThrowBack::transitionInto();
    enterSubState(SUB_FLAIL);
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __16StateBeingThrown);
void StateBeingThrown::transitionInto(void)
{
    StateThrowBack::transitionInto();
    enterSubState(SUB_FLAIL);
    OWNER_SOUND(owner)->playKnockBackSound();
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __14StateKnockBack);
void StateKnockBack::transitionInto(void)
{
    StateThrowBack::transitionInto();
    enterSubState(SUB_FLAIL);
    OWNER_SOUND(owner)->playKnockBackSound();
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __17StateGrappleBreak);
void StateGrappleBreak::transitionInto(void)
{
    StateThrowBack::transitionInto();
    enterSubState(SUB_FLAIL);
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __tf14StateThrowBack);
void setThrowSource__14StateThrowBackP7Monster(void *self, int v) __asm__("setThrowSource__14StateThrowBackP7Monster");
void setThrowSource__14StateThrowBackP7Monster(void *self, int v)
{
    *(int *)((char *)self + 0x524) = v;
}
void setPauseDuration__14StateThrowBackf(void *self, float v) __asm__("setPauseDuration__14StateThrowBackf");
void setPauseDuration__14StateThrowBackf(void *self, float v)
{
    *(float *)((char *)self + 0x53C) = v;
}
void setCounterable__14StateThrowBackb(void *self, int v) __asm__("setCounterable__14StateThrowBackb");
void setCounterable__14StateThrowBackb(void *self, int v)
{
    *(int *)((char *)self + 0x38) = v;
}
int getSubState__14StateThrowBack(void *self) __asm__("getSubState__14StateThrowBack");
int getSubState__14StateThrowBack(void *self)
{
    return *(int *)((char *)self + 0x260);
}
int getThrowSource__14StateThrowBack(void *self) __asm__("getThrowSource__14StateThrowBack");
int getThrowSource__14StateThrowBack(void *self)
{
    return *(int *)((char *)self + 0x524);
}
/* Once per throw, after turning upside down. */
int StateThrowBack::isCounterAvailable(void)
{
    return counterable && counterReady && counterAttempts == 0;
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", _vt$14StateKnockBack);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", _vt$17StateGrappleBreak);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", _vt$16StateBeingThrown);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", _vt$17StateBeingSlammed);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", _vt$14StateThrowBack);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", D_006F1A08);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __tf17StateBeingSlammed);
void setDownTime__17StateBeingSlammedf(void *self, float v) __asm__("setDownTime__17StateBeingSlammedf");
void setDownTime__17StateBeingSlammedf(void *self, float v)
{
    *(float *)((char *)self + 0x48) = v;
}
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __tf16StateBeingThrown);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __tf17StateGrappleBreak);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", __tf14StateKnockBack);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", func_001D6570);
INCLUDE_ASM("asm/nonmatchings/game/StateThrowBack", func_001D6590);
