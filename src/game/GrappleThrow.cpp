#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/monster_state.h"
#include "vecmath.h"

/* After a grab lands (Grapple.cpp) the grabber holds the victim overhead in StateGrappleLift (Monster+0xDD54); a fresh
 * press of the action button queues a throw, mapped action 0xA into StateGrappleThrow (Monster+0xDD74, thrown at the
 * reticle target) or 0xB into StateGrappleThrow1 (Monster+0xDDA0, thrown straight ahead). The victim leaves through
 * the state at Monster+0xE3E0 with the grabber as its last attacker. */

#define STATE_AT(m, off) ((MonsterState *)((char *)(m) + (off)))
#define ST_IDLE 0x7984
#define ST_GRAPPLE 0xDCE0
#define ST_GRAPPLE_THROW 0xDD74
#define ST_GRAPPLE_THROW1 0xDDA0
#define ST_GRAPPLED 0xDDCC
#define ST_THROWN 0xE3E0
#define MI(p, o) (*(int *)((char *)(p) + (o)))
/* MonsterDynamics (Monster+0x100): velocity at +0xE0 */
#define MONSTER_DYN(m) ((MonsterDynamics *)((char *)(m) + 0x100))
#define MONSTER_VEL(m) ((_fvector *)((char *)(m) + 0x1E0))
#define MONSTER_SOUND(m) ((MonsterSound *)(m)->m_sound)
/* the reaction states (flags & 0x10) keep the monster that hit last at 0x524 */
#define LAST_ATTACKER(st) (*(Monster **)((char *)(st) + 0x524))
#define STATE_GRAPPLED 0x12 /* id of the state at Monster+0xDDCC */

class MonsterDynamics {
public:
    void updateMove(bool fromPad);
    void updateTurn(bool fromPad);
};
class StateGrapple : public MonsterState {
public:
    void detach(void);
};
class StateGrappled : public MonsterState {
public:
    void detach(void);
};
class AttackState : public MonsterState {
public:
    int unk14; /* 0x14 */
    int unk18; /* 0x18 */
};

/* Holding the victim overhead (animation 0xAB). */
class StateGrappleLift : public MonsterState {
public:
    float blendTime; /* 0x14: 100 */
    float speed;     /* 0x18: 1.0, playback speed of the lift animation */
    int queued;      /* 0x1C: mapped action of the throw asked for (0xA, 0xB), 0xF while none */

    int transitionOK(void);
    void transitionInto(void);
    void update(void);
};
typedef char _size_StateGrappleLift[sizeof(StateGrappleLift) == 0x20 ? 1 : -1];

/* Throw at the reticle target (animation 0x37). */
class StateGrappleThrow : public AttackState {
public:
    float blendTime;  /* 0x1C: 96 */
    float releasePct; /* 0x20: 0.38, point of the animation where the victim leaves */
    float maxSpeed;   /* 0x24: 30, the throw velocity is clamped to it */
    float speed;      /* 0x28: 1000 */

    int transitionOK(void);
    void transitionInto(void);
    void update(void);
    void throwToTarget(_fvector &target);
    void handlePreemption(MonsterState *next);
};
typedef char _size_StateGrappleThrow[sizeof(StateGrappleThrow) == 0x2C ? 1 : -1];

/* Throw straight ahead (animation 0x38): the victim gets our velocity plus fwd along our facing and up. */
class StateGrappleThrow1 : public AttackState {
public:
    float blendTime;  /* 0x1C: 100 */
    float releasePct; /* 0x20: 0.38 */
    float up;         /* 0x24: 30 */
    float fwd;        /* 0x28: 1000 */

    int transitionOK(void);
    void transitionInto(void);
    void update(void);
};
typedef char _size_StateGrappleThrow1[sizeof(StateGrappleThrow1) == 0x2C ? 1 : -1];

/* *dst += dir * s (xyz) */
static inline void vecPush(_fvector *dst, _fvector *dir, float s)
{
    _fvector t;

    vecScale(&t, dir, s);
    vecAdd(dst, dst, &t);
}

INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", __16StateGrappleLift);
/* Attacks on, the lift animation present, holding someone, and a fresh press of the action button. */
int StateGrappleLift::transitionOK(void)
{
    Monster *m = owner;

    if (m->m_attacksEnabled == 0)
        return 0;
    if (m->m_anims[0xAB].ci == 0)
        return 0;
    if (m->m_target == 0)
        return 0;
    if (owner->m_padFlags[0]->action != 0 && owner->m_padFlags[1]->action == 0)
        return 1;
    return 0;
}
/* Lift animation, sounds, the victim into StateGrappled if it is not there yet, and the camera: with two monsters
 * the close-combat camera frames both when both are players (playerNum 1); otherwise the camera of player 2 follows
 * the grabber (leaving the unified view first). */
#ifdef NON_MATCHING
/* equivalent: 83/146 words (register/delay-slot scheduling of the unified-view camera branch); tools/difftest.py 57/57 */
void StateGrappleLift::transitionInto(void)
{
    Monster *v;

    frames = 0;
    MI(owner, 0x130) = 1;
    animationSetSpeed(owner->m_anims[0xAB], speed);
    animationTransitionInto(owner->m_anims[0xAB], blendTime, 1, 7);
    queued = 0xF;
    v = (Monster *)owner->m_target;
    MONSTER_SOUND(v)->playSignatureSound();
    MONSTER_SOUND(owner)->playGrapplePickupSound();
    if (*v->m_state != STATE_GRAPPLED)
        v->enterNewState(STATE_AT(v, ST_GRAPPLED));
    if (!gUseUnifiedView) {
        if (game->m_numMonsters == 2) {
            if (owner->m_playerNum == 1 && v->m_playerNum == 1) {
                Cameras::InitCCCamera(owner, v);
                Cameras::SetCameraPOV(2, Camera::POV_0);
                Cameras::m_cameras.m_unk2AF8 = 1;
                owner->m_padFlags.f16E8 = 0;
                v->m_padFlags.f16E8 = 0;
            } else if (owner->m_playerNum == 2) {
                Cameras::SetCameraMonster(v->m_monsterNum, owner);
            }
        } else if (owner->m_playerNum == 1 || v->m_playerNum == 1) {
            Cameras::InitCCCamera(owner, v);
            Cameras::SetCameraPOV(2, Camera::POV_0);
            owner->m_padFlags.f16E8 = 0;
            v->m_padFlags.f16E8 = 0;
        }
    } else {
        if (game->m_numMonsters == 2) {
            if (owner->m_playerNum == 1 && v->m_playerNum == 1) {
                Cameras::InitCCMonsters(owner, v);
                if (Cameras::m_cameras.m_state != 0) {
                    Cameras::SetCameraPOV(2, Camera::POV_0);
                    owner->m_padFlags.f16E8 = 0;
                    v->m_padFlags.f16E8 = 0;
                }
            } else if (owner->m_playerNum == 1 || v->m_playerNum == 1) {
                Cameras::LeaveUnifiedView(3);
                if (owner->m_playerNum == 2)
                    Cameras::SetCameraMonster(v->m_monsterNum, owner);
            }
        } else if (owner->m_playerNum == 1 || v->m_playerNum == 1) {
            Cameras::InitCCMonsters(owner, v);
            Cameras::SetCameraPOV(2, Camera::POV_0);
            owner->m_padFlags.f16E8 = 0;
            v->m_padFlags.f16E8 = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", transitionInto__16StateGrappleLift);
#endif
/* Lost the victim (or it died): back to Idle. At the end of the lift, the queued throw (or Idle). A player can queue
 * the throw with mapped action 0xA or 0xB while lifting. */
void StateGrappleLift::update(void)
{
    float pct;
    Monster *v;

    MonsterState::update();
    MONSTER_DYN(owner)->updateMove(false);
    MONSTER_DYN(owner)->updateTurn(false);
    owner->updateLock((MonsterReticleState)1);
    pct = animationGetCurrentPercent(owner->m_anims[0xAB]);
    v = (Monster *)owner->m_target;
    if (v == 0 || v->m_dead) {
        owner->m_padFlags.f16E8 = 0;
        MI(owner, 0x130) = 0;
        owner->enterNewState(STATE_AT(owner, ST_IDLE));
    } else if (0.99f <= pct) {
        owner->m_padFlags.f16E8 = 0;
        ((Monster *)owner->m_target)->m_padFlags.f16E8 = 0;
        if (queued == 0xA)
            owner->enterNewState(STATE_AT(owner, ST_GRAPPLE_THROW));
        else if (queued == 0xB)
            owner->enterNewState(STATE_AT(owner, ST_GRAPPLE_THROW1));
        else
            owner->enterNewState(STATE_AT(owner, ST_IDLE));
    }
    if (owner->m_playerNum == 1 && queued == 0xF) {
        int a = owner->m_padFlags.curMappedAction;

        if (a == 0xA)
            queued = a;
        else if (a == 0xB)
            queued = a;
    }
}
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", __17StateGrappleThrow);
int StateGrappleThrow::transitionOK(void)
{
    Monster *m = owner;

    if (m->m_attacksEnabled == 0 || m->m_anims[0x37].ci == 0)
        return 0;
    if (m->m_target != 0 && m->m_padFlags.curMappedAction == 0xA)
        return 1;
    return 0;
}
void StateGrappleThrow::transitionInto(void)
{
    frames = 0;
    animationTransitionInto(owner->m_anims[0x37], blendTime, 1, 1);
    owner->m_unk49 = 1;
    MONSTER_SOUND(owner)->playGrappleThrowSound();
}
/* At releasePct of the throw animation the victim flies at the reticle target; Idle at the end if nobody is held. */
void StateGrappleThrow::update(void)
{
    float pct;

    MonsterState::update();
    MONSTER_DYN(owner)->updateMove(false);
    MONSTER_DYN(owner)->updateTurn(true);
    owner->updateLock((MonsterReticleState)1);
    pct = animationGetCurrentPercent(owner->m_anims[0x37]);
    if (owner->m_target != 0) {
        if (releasePct <= pct) {
            _fvector target;

            owner->getTarget(target, true);
            throwToTarget(target);
        }
    } else if (0.99f <= pct) {
        owner->enterNewState(STATE_AT(owner, ST_IDLE));
    }
}
/* Ballistic throw from the victim's point at Monster+0x3E30 to `target`: horizontal speed `speed`, flight time
 * s_tMod * dist / speed, the vertical speed that lands it under game gravity; the velocity is clamped to maxSpeed
 * (its w keeps the squared length). Both let go and the victim enters the thrown state (Monster+0xE3E0). */
#ifdef NON_MATCHING
extern float s_tMod __asm__("s_tMod.2460");
__asm__("#SNFIX_SMALL s_tMod.2460");
/* sqrt.s $f1, $f1 in the R5900 encoding (see eeSqrtf) */
static inline float eeSqrtf1(float v)
{
    register float x __asm__("$f1") = v;
    __asm__(".word 0x46010044" : "+f"(x));
    return x;
}
/* equivalent: 67/83 words, same size (FPR allocation of the two squared lengths); VU0, so no tools/difftest.py */
void StateGrappleThrow::throwToTarget(_fvector &target)
{
    Monster *v = (Monster *)owner->m_target;
    MonsterState *st;
    _fvector d;
    _fvector *vel;
    float dist;
    float t;
    float lenSq;

    vecSub(&d, &target, (_fvector *)((char *)v + 0x3E30));
    dist = eeSqrtf1(vecDot(&d, &d));
    vel = MONSTER_VEL(v);
    t = s_tMod * dist / speed;
    vecScale(vel, &d, speed / dist);
    vel->z = (d.z - game->m_gravity * 0.5f * t * t) / t;
    lenSq = vecDot(vel, vel);
    vel->w = lenSq;
    if (maxSpeed * maxSpeed < lenSq)
        vecScale(vel, vel, eeRsqrtf(maxSpeed, lenSq));
    ((StateGrappled *)STATE_AT(v, ST_GRAPPLED))->detach();
    ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->detach();
    st = STATE_AT(v, ST_THROWN);
    v->enterNewState(st);
    LAST_ATTACKER(st) = owner;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", throwToTarget__17StateGrappleThrowR8_fvector);
#endif
void StateGrappleThrow::handlePreemption(MonsterState *next)
{
    MONSTER_SOUND(owner)->terminateGrappleSound();
}
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", __18StateGrappleThrow1);
int StateGrappleThrow1::transitionOK(void)
{
    Monster *m = owner;

    if (m->m_attacksEnabled == 0 || m->m_anims[0x38].ci == 0)
        return 0;
    if (m->m_target != 0 && m->m_padFlags.curMappedAction == 0xB)
        return 1;
    return 0;
}
void StateGrappleThrow1::transitionInto(void)
{
    frames = 0;
    animationTransitionInto(owner->m_anims[0x38], blendTime, 1, 1);
    owner->m_unk49 = 1;
    MONSTER_SOUND(owner)->playGrappleThrowSound();
}
/* At releasePct the victim takes our velocity plus fwd along our facing and up, both let go and it enters the thrown
 * state; two players in the unified view get the throw camera. Idle at the end if nobody is held. */
void StateGrappleThrow1::update(void)
{
    float pct;
    Monster *v;
    MonsterState *st;

    MonsterState::update();
    pct = animationGetCurrentPercent(owner->m_anims[0x38]);
    if (owner->m_target != 0) {
        if (releasePct <= pct) {
            v = (Monster *)owner->m_target;
            vecCopy(MONSTER_VEL(v), MONSTER_VEL(owner));
            vecPush(MONSTER_VEL(v), (_fvector *)owner->m_cs->mat[1], fwd);
            vecPush(MONSTER_VEL(v), (_fvector *)owner->m_cs->mat[2], up);
            v = (Monster *)owner->m_target;
            st = STATE_AT(v, ST_THROWN);
            v->enterNewState(st);
            LAST_ATTACKER(st) = owner;
            ((StateGrappled *)STATE_AT(v, ST_GRAPPLED))->detach();
            ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->detach();
            if (gUseUnifiedView && game->m_numMonsters == 2 && owner->m_playerNum == 1 && v->m_playerNum == 1)
                Cameras::InitCCCameraFromThrow(owner, v);
        }
    } else if (0.99f <= pct) {
        owner->enterNewState(STATE_AT(owner, ST_IDLE));
    }
    MONSTER_DYN(owner)->updateMove(false);
    MONSTER_DYN(owner)->updateTurn(true);
}
/* two `return 0` functions nothing references (unnamed in the ELF) */
extern "C" int func_0013E7D8(void)
{
    return 0;
}
extern "C" int func_0013E7E0(void)
{
    return 0;
}
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", _vt$18StateGrappleThrow1);
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", _vt$17StateGrappleThrow);
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", _vt$16StateGrappleLift);
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", __tf16StateGrappleLift);
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", __tf17StateGrappleThrow);
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", __tf18StateGrappleThrow1);
INCLUDE_ASM("asm/nonmatchings/game/GrappleThrow", func_0013E8D8);
