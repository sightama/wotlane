#include "common.h"
#include "game/game.h"
#include "game/monster_state.h"
#include "game/enemy_info.h"
#include "task_manager.h"
#include "vecmath.h"

/* Grappling: StateGrapple (Monster+0xDCE0) is the grabber reaching for a victim, StateGrappled (Monster+0xDDCC) the
 * victim held and struggling. Once the grab lands the grabber moves on to StateGrappleLift (Monster+0xDD54,
 * GrappleThrow.cpp); either side leaves through detach, a release or a breakFree into StateGrappleBreak (Monster+0xE990). */

extern int gUseUnifiedView;
extern "C" float cosf(float);
extern "C" float atan2f(float y, float x);
void boundEulerAngle(float *p);
void mathfRotMatrixRPH(float (*m)[3], _fvector *rph);
float animationGetTotalRunFrames(_animHandle h);
/* engine.h declares it int; retail tests the result here as a bool (xori 1) */
bool animIsRunning(_animHandle h) __asm__("animationIsRunning__FG11_animHandle");

#define STATE_AT(m, off) ((MonsterState *)((char *)(m) + (off)))
#define ST_IDLE 0x7984
#define ST_PUNCH 0x8470
#define ST_BATSWIPE 0xAE90
#define ST_GRAPPLE 0xDCE0
#define ST_GRAPPLE_LIFT 0xDD54
#define ST_GRAPPLED 0xDDCC
#define ST_GRAPPLE_BREAK 0xE990
#define MI(p, o) (*(int *)((char *)(p) + (o)))
#define MF(p, o) (*(float *)((char *)(p) + (o)))
/* MonsterDynamics (Monster+0x100): velocity at +0xE0 */
#define MONSTER_VEL(m) ((_fvector *)((char *)(m) + 0x1E0))
/* virtual call through the retail vtable (entries are {this delta, 0, function}) */
#define VCALL_INT(st, slot) \
    (((int (*)(void *))*(void **)((char *)(st)->vptr + (slot) + 4))((char *)(st) + *(short *)((char *)(st)->vptr + (slot))))
#define VT_TRANSITION_FEASIBLE 0x28
#define VT_SPECIAL_GRAPPLE_OK 0x50 /* slot only the special states (Monster+0x7980) have; it lets a grapple in */
/* the reaction states (flags & 0x10) keep the monster that hit last at 0x524 */
#define LAST_ATTACKER(st) (*(Monster **)((char *)(st) + 0x524))

class MonsterDynamics {
public:
    void allStop(void);
};
class Interactives {
public:
    static Monster *getInteractive(int id);
};
class StatePunch {
public:
    char *getCurrentConfig(void);
};

/* Grabbing (Monster+0xDCE0), mapped action 0xA. Substate 0 plays the reach animation (0xAA); between attachStart and
 * attachEnd of it the closest monster in a cone in front of the grabber is taken (it enters StateGrappled and the
 * grabber StateGrappleLift). Past the window without a victim, substate 1 plays the miss animation (0xB9). */
class StateGrapple : public MonsterState {
public:
    enum SubState { SUB_REACH, SUB_MISS };

    float reach;        /* 0x14: added to Monster+0x3F4, how far ahead of the eye the cone starts */
    float eyeUp;        /* 0x18: eye height along the up axis of the monster */
    float fov;          /* 0x1C: cone angle in degrees */
    float eyeBack;      /* 0x20: eye offset along the facing axis (-50) */
    float feasibleDist; /* 0x24: range of transitionFeasible */
    float blendTime;    /* 0x28 */
    float runFrames;    /* 0x2C: length of the reach animation */
    float attachStart;  /* 0x30: window of the reach animation in which a victim is taken */
    float attachEnd;    /* 0x34 */
    float attachDist;   /* 0x38: range while reaching */
    float unk3C;        /* 0x3C */
    float unk40;        /* 0x40 */
    float throwFwd;     /* 0x44: velocity given to the victim by release(true), along the facing axis */
    float throwUp;      /* 0x48 */
    float dropFwd;      /* 0x4C: same for release(false) */
    float dropUp;       /* 0x50 */
    float recoil;       /* 0x54: getRecoilFields */
    float missSpeed;    /* 0x58: playback speed of the miss animation */
    Monster *victim;    /* 0x5C: candidate from transitionFeasible */
    char pad60[0x6C - 0x60];
    int subState;       /* 0x6C */
    char pad70[0x74 - 0x70];

    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void update(void);
    void handlePreemption(MonsterState *next);
    void enterSubState(SubState s);
    void detach(void);
    void release(bool thrown);
    float getRecoilFields(void);
    float getAttachWindowStart(void);
    float getAttachWindowEnd(void);
};
typedef char _size_StateGrapple[sizeof(StateGrapple) == 0x74 ? 1 : -1];

/* Held by a grabber (Monster+0xDDCC). Pressing any of the four face buttons struggles: in substate 0 every fresh press
 * drains the grabber's stamina and plays a struggle animation (0x118..0x11C); in substate 1 (a hold with a meter) every
 * press lowers the meter, which refills by meterRate per field, and at 0 the hold ends. The victim breaks free once the
 * grabber's stamina drops to minGrapplerStamina or it took breakDamage; it is let go once it took releaseDamage. */
class StateGrappled : public MonsterState {
public:
    enum SubState { SUB_0, SUB_1, SUB_2 };

    float blendTime;          /* 0x14 */
    float minGrapplerStamina; /* 0x18 */
    float breakDamage;        /* 0x1C: damage taken by the grabber that frees us */
    float releaseDamage;      /* 0x20: damage taken by us that makes the grabber let go */
    float struggleCost;       /* 0x24: grabber stamina drained per struggle */
    float breakBack;          /* 0x28: breakFree pushes the grabber back... */
    float breakUp;            /* 0x2C: ...and up */
    float pushFwd;            /* 0x30: and us along the grabber's facing... */
    float pushUp;             /* 0x34: ...and up */
    float grapplerHealth;     /* 0x38: grabber health when the grab landed */
    float health;             /* 0x3C: our health then */
    int lastPressed;          /* 0x40: a face button was down last frame */
    int anim;                 /* 0x44: MonsterAnim playing */
    float speed;              /* 0x48: playback speed of animation 0xD2 (substate 2) */
    int subState;             /* 0x4C */
    float struggleDisable;    /* 0x50: fields left before struggling counts */
    float meter;              /* 0x54: substate 1 */
    float meterCost;          /* 0x58 */
    float meterRate;          /* 0x5C */

    int transitionOK(int attackerId);
    void transitionInto(void);
    void update(void);
    int healthDeltaAchieved(float delta);
    void enterSubState(SubState s);
    void detach(void);
    void breakFree(void);
    int isOverhead(void);
    void handlePreemption(MonsterState *next);
    void setStruggleDisable(int fields);
};
typedef char _at_StateGrappled_meterRate[(unsigned)&((StateGrappled *)0)->meterRate == 0x5C ? 1 : -1];

#ifdef NON_MATCHING
/* dst = a + b * s (xyz) */
static inline void vecMadd(_fvector *dst, _fvector *a, _fvector *b, float s)
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
/* min.s, as retail clamps */
static inline float eeMinf(float a, float b)
{
    float r;

    __asm__ volatile("min.s %0, %1, %2" : "=f"(r) : "f"(a), "f"(b) : "memory");
    return r;
}
#endif
/* Monster::getAnim, inlined */
static inline _animHandle *monsterAnim(Monster *m, int a)
{
    return &m->m_anims[a];
}
#ifdef NON_MATCHING
/* *dst += dir * s (xyz) */
static inline void vecPush(_fvector *dst, _fvector *dir, float s)
{
    _fvector t;

    vecScale(&t, dir, s);
    vecAdd(dst, dst, &t);
}
#endif

INCLUDE_ASM("asm/nonmatchings/game/Grapple", __12StateGrapple);
/* Not for the 0x1C0 monster type nor in game mode 9; needs mapped action 0xA and a victim in reach. */
int StateGrapple::transitionOK(void)
{
    Monster *m = owner;
    int ok;

    if (m->m_typeBits == 0x1C0 || game->m_gameMode == 9)
        return 0;
    ok = 0;
    if (m->m_padFlags.curMappedAction == 0xA)
        ok = VCALL_INT(this, VT_TRANSITION_FEASIBLE) != 0;
    return ok;
}
/* Attacks on, the reach animation present, hands free and nobody held; the closest monster in the cone that is not
 * one of the bosses (types 0x1A0, 0x1C0, 0x1E0), is in line of sight and can be grappled by us. */
#ifdef NON_MATCHING
/* equivalent: 97/101 words (Monster+0x3F4 loaded before m_cs in retail); tools/difftest.py 76/76 */
int StateGrapple::transitionFeasible(void)
{
    Monster *m = owner;
    _fvector from;
    _fvector eye;
    float dist;
    int type;

    if (m->m_attacksEnabled == 0)
        return 0;
    if (m->m_anims[0xAA].ci == 0)
        return 0;
    if (m->m_target != 0)
        return 0;
    if (m->m_pickup != 0)
        return 0;
    vecMadd(&from, &m->m_cs->trans, (_fvector *)m->m_cs->mat[2], eyeUp);
    vecMadd(&eye, &from, (_fvector *)m->m_cs->mat[1], eyeBack);
    vecMadd(&from, &from, (_fvector *)m->m_cs->mat[1], reach + MF(m, 0x3F4));
    victim = owner->getClosestMonsterInFOV(from, feasibleDist, eye, cosf(fov * 0.017453292f * 0.5f), dist);
    if (victim != 0 && (type = victim->m_typeBits) != 0x1A0 && type != 0x1C0) {
        if (type == 0x1E0)
            return 0;
        if (EnemyInfo::getInfo(*owner, *victim)->los != 0 &&
            ((StateGrappled *)STATE_AT(victim, ST_GRAPPLED))->transitionOK(owner->m_id)) {
            victim->m_grappleAttempt = owner;
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Grapple", transitionFeasible__12StateGrapple);
#endif
void StateGrapple::transitionInto(void)
{
    frames = 0;
    enterSubState(SUB_REACH);
    ((MonsterDynamics *)((char *)owner + 0x100))->allStop();
}
/* Reaching: turn towards the candidate, and inside the attach window take the closest monster in the cone. In the
 * tutorial (game mode 1, level 1, player 1) a successful grab shows text box 0x16 or 0x1D. */
#ifdef NON_MATCHING
/* equivalent: 223/228 words (retail stores m_unk1B8 through Monster+0x100, bc1f vs bc1fl); tools/difftest.py 53/53 */
void StateGrapple::update(void)
{
    float pct;
    Monster *m;
    StateGrappled *g;

    MonsterState::update();
    if (owner->m_unk1E8 < 0.0f)
        owner->m_unk1B8 = 1.5f;
    switch (subState) {
    case SUB_REACH:
        pct = animationGetCurrentPercent(owner->m_anims[0xAA]);
        if (victim != 0) {
            float heading;

            heading = atan2f(victim->m_cs->trans.x - owner->m_cs->trans.x, victim->m_cs->trans.y - owner->m_cs->trans.y);
            boundEulerAngle(&heading);
            owner->m_cs->rot.z = heading;
            mathfRotMatrixRPH((float (*)[3])owner->m_cs->mat, &owner->m_cs->rot);
        }
        if (owner->m_target != 0)
            return;
        if (!(attachStart <= pct))
            return;
        if (animationIsTransitioning(owner->m_anims[0xAA]))
            return;
        if (attachEnd <= pct && !animationIsTransitioning(owner->m_anims[0xAA])) {
            enterSubState(SUB_MISS);
            return;
        }
        {
        _fvector from;
        _fvector eye;
        float dist;

        vecMadd(&from, &owner->m_cs->trans, (_fvector *)owner->m_cs->mat[2], eyeUp);
        vecMadd(&eye, &from, (_fvector *)owner->m_cs->mat[1], eyeBack);
        vecMadd(&from, &from, (_fvector *)owner->m_cs->mat[1], reach + MF(owner, 0x3F4));
        m = owner->getClosestMonsterInFOV(from, attachDist, eye, cosf(fov * 0.017453292f * 0.5f), dist);
        }
        if (m == 0)
            return;
        g = (StateGrappled *)STATE_AT(m, ST_GRAPPLED);
        if (!g->transitionOK(0))
            return;
        if (EnemyInfo::getInfo(*owner, *m)->los == 0)
            return;
        owner->m_target = m;
        owner->setSecondaryShadowBlocker(m->m_cs);
        m->m_grappler = owner;
        m->enterNewState(g);
        g->enterSubState(StateGrappled::SUB_0);
        owner->enterNewState(STATE_AT(owner, ST_GRAPPLE_LIFT));
        if (game->m_gameMode == 1 && game->m_levelId == 1 && owner->m_playerNum == 1) {
            if (mathfRand(0, 1))
                game->m_huds[0].addTextBoxMessage(0x16);
            else
                game->m_huds[0].addTextBoxMessage(0x1D);
        }
        break;
    case SUB_MISS:
        if (animationGetCurrentPercent(owner->m_anims[0xB9]) >= 1.0f)
            owner->enterNewState(STATE_AT(owner, ST_IDLE));
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Grapple", update__12StateGrapple);
#endif
void StateGrapple::handlePreemption(MonsterState *next)
{
    owner->m_unk1B8 = 1.0f;
    if (victim != 0) {
        victim->m_grappleAttempt = 0;
        victim = 0;
    }
}
/* Reach: animation 0xAA over runFrames. Miss: animation 0xB9 with attacks off until it ends (or straight to Idle
 * without it). */
void StateGrapple::enterSubState(SubState s)
{
    switch (s) {
    case SUB_REACH:
        animationSetTotalRunFrames(owner->m_anims[0xAA], runFrames);
        animationTransitionInto(owner->m_anims[0xAA], blendTime, 1, 7);
        owner->m_unk49 = 1;
        break;
    case SUB_MISS:
        if (owner->m_anims[0xB9].ci != 0) {
            float frames;
            Monster *m;

            animationSetSpeed(owner->m_anims[0xB9], missSpeed);
            animationTransitionInto(owner->m_anims[0xB9], blendTime, 1, 1);
            frames = animationGetTotalRunFrames(owner->m_anims[0xB9]);
            /* Monster::setAttackDisableDuration, inlined */
            m = owner;
            m->m_attacksEnabled = 0;
            TaskManager::global.add(&Monster::restoreAttacksEnabled, m, (int)frames);
        } else {
            owner->enterNewState(STATE_AT(owner, ST_IDLE));
        }
        break;
    }
    subState = s;
}
void StateGrapple::detach(void)
{
    owner->m_target = 0;
    owner->setSecondaryShadowBlocker(0);
    MI(owner, 0x130) = 0;
}
/* Let go of the victim, pushing it along our facing and up (thrown or dropped strengths), into StateGrappleBreak. */
#ifdef NON_MATCHING
/* equivalent: 66/91 words, register allocation only (0x8000 base and velocity pointer swap a0/a1/a2) */
void StateGrapple::release(bool thrown)
{
    Monster *m = owner;
    Monster *v = (Monster *)m->m_target;

    detach();
    ((StateGrappled *)STATE_AT(v, ST_GRAPPLED))->detach();
    if (!thrown) {
        vecPush(MONSTER_VEL(v), (_fvector *)m->m_cs->mat[1], ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->dropFwd);
        vecPush(MONSTER_VEL(v), (_fvector *)m->m_cs->mat[2], ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->dropUp);
        v->enterNewState(STATE_AT(v, ST_GRAPPLE_BREAK));
    } else {
        vecPush(MONSTER_VEL(v), (_fvector *)m->m_cs->mat[1], ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->throwFwd);
        vecPush(MONSTER_VEL(v), (_fvector *)m->m_cs->mat[2], ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->throwUp);
        v->enterNewState(STATE_AT(v, ST_GRAPPLE_BREAK));
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Grapple", release__12StateGrappleb);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Grapple", __13StateGrappled);
/* Needs the hold (0xAE) and struggle (0x118) animations. Not against the monster that just hit us (in a reaction
 * state), nor one whose stamina is down to minGrapplerStamina; in the special state only if it allows it. A punch or
 * bat swipe that is moving or past 100% reach can be grabbed before it lands; otherwise not while held, holding,
 * busy (Monster+0x49) or in the state at Monster+0x797C. State 0x26 always can. */
#ifdef NON_MATCHING
/* equivalent: 39/262 words, size 0x3a8 vs 0x418 (branch layout of the punch/bat swipe checks, retail likely inlines helpers); tools/difftest.py 228/228 */
int StateGrappled::transitionOK(int attackerId)
{
    Monster *m = owner;
    char *p;
    int sw;

    if (m->m_anims[0xAE].ci == 0)
        return 0;
    if (m->m_anims[0x118].ci == 0)
        return 0;
    if (attackerId != 0) {
        MonsterState *st = (MonsterState *)m->m_state;

        if ((st->flags & 0x10) && LAST_ATTACKER(st) != 0 && LAST_ATTACKER(st)->m_id == attackerId)
            return 0;
    }
    if (attackerId != 0 && Interactives::getInteractive(attackerId)->m_stamina.cur <= minGrapplerStamina)
        return 0;
    m = owner;
    if (m->m_state == m->m_specialState && VCALL_INT((MonsterState *)m->m_state, VT_SPECIAL_GRAPPLE_OK) == 0)
        return 0;
    m = owner;
    if (m->m_state[0] == 3) {
        p = (char *)m + ST_PUNCH;
        sw = MI(p, 0x298C);
        if (sw == 0 || sw == 2 || sw == 5) {
            int moving = MF(((StatePunch *)p)->getCurrentConfig(), 0x68) != 0.0f ||
                         MF(((StatePunch *)p)->getCurrentConfig(), 0x64) != 0.0f;

            m = owner;
            if (moving) {
                p = (char *)m + ST_PUNCH;
                if ((float)MI(p, 0x29EC) < MF(p, 0x29F8))
                    return 1;
                if (MI(p, 0x299C) != 0)
                    return 1;
            }
        }
    }
    if (m->m_state[0] == 3) {
        p = (char *)m + ST_PUNCH;
        if (MF(p, MI(p, 0x298C) * 0xB0 + MI(p, 0x2988) * 0x420 + 0x8C) > 100.0f) {
            if ((float)MI(p, 0x29EC) < MF(p, 0x29F8))
                return 1;
            if (MI(p, 0x299C) != 0)
                return 1;
        }
    }
    if (m->m_state[0] == 0x29) {
        p = (char *)m + ST_BATSWIPE;
        sw = MI(p, 0x2D4C);
        if (sw == 0 || sw == 2 || sw == 5) {
            char *rec = p + MI(p, 0x2D48) * 0x480 + 0x20 + MI(p, 0x2D4C) * 0xC0;

            if ((MF(rec, 0x68) != 0.0f || MF(rec, 0x64) != 0.0f) && MI(p, 0x2DC0) == 0) {
                if ((float)MI(p, 0x2DAC) < MF(p, 0x2DB8))
                    return 1;
                if (MI(p, 0x2D5C) != 0)
                    return 1;
            }
        }
    }
    if (m->m_state[0] == 0x29) {
        p = (char *)m + ST_BATSWIPE;
        if (MF(p + MI(p, 0x2D48) * 0x480 + 0x20 + MI(p, 0x2D4C) * 0xC0, 0x6C) > 100.0f && MI(p, 0x2DC0) == 0) {
            if ((float)MI(p, 0x2DAC) < MF(p, 0x2DB8))
                return 1;
            if (MI(p, 0x2D5C) != 0)
                return 1;
        }
    }
    if (m->m_state[0] == 0x26)
        return 1;
    if (m->m_grappler != 0)
        return 0;
    if (m->m_target != 0)
        return 0;
    if (m->m_unk49 == 0)
        return 0;
    return m->m_state != (int *)MI(m, 0x797C);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Grapple", transitionOK__13StateGrappledi);
#endif
/* Stop, drop what we hold, hide the shadow and remember both healths; the tutorial shows text box 0x1A. */
void StateGrappled::transitionInto(void)
{
    frames = 0;
    ((MonsterDynamics *)((char *)owner + 0x100))->allStop();
    owner->m_flags &= ~2;
    owner->dropPickupImpaler();
    owner->dropPickup();
    owner->m_grappleAttempt = 0;
    owner->setShadowOnOff(false);
    grapplerHealth = owner->m_grappler->m_health;
    health = owner->m_health;
    enterSubState(SUB_0);
    if (game->m_gameMode == 1 && game->m_levelId == 1 && owner->m_playerNum == 1)
        game->m_huds[0].addTextBoxMessage(0x1A);
}
#ifdef NON_MATCHING
/* equivalent: 252/257 words (FPR allocation around the min.s clamp); tools/difftest.py 13/13 */
void StateGrappled::update(void)
{
    int pressed;

    MonsterState::update();
    if (struggleDisable != 0.0f)
        struggleDisable = struggleDisable - 1.0f;
    if (subState == SUB_1) {
        meter = meter + (float)timerGetFieldsLastFrame() * meterRate;
        meter = eeMinf(100.0f, meter);
    }
    if (subState == SUB_2) {
        if (animationGetCurrentPercent(*monsterAnim(owner, anim)) >= 1.0f)
            enterSubState(SUB_0);
        return;
    }
    pressed = 0;
    if (owner->m_padFlags[0]->face[2] != 0 || owner->m_padFlags[0]->face[3] != 0 || owner->m_padFlags[0]->face[0] != 0 ||
        owner->m_padFlags[0]->face[1] != 0)
        pressed = 1;
    if (pressed && struggleDisable <= 0.0f) {
        if (lastPressed == 0) {
            if (subState == SUB_1) {
                meter = meter - meterCost;
                if (meter <= 0.0f)
                    enterSubState(SUB_0);
            } else {
                owner->m_grappler->m_stamina.drain(struggleCost, false, false);
                if (anim == 0xAE || !animIsRunning(*monsterAnim(owner, anim))) {
                    anim = mathfRand(0, 4) + 0x118;
                    animationTransitionInto(*monsterAnim(owner, anim), 83.333351f, 1, 1);
                }
            }
        } else if (anim != 0xAE && !animIsRunning(*monsterAnim(owner, anim))) {
            anim = 0xAE;
            animationTransitionInto(*monsterAnim(owner, anim), blendTime, 0, 1);
        }
    } else if (subState != SUB_1 && anim != 0xAE) {
        anim = 0xAE;
        animationTransitionInto(*monsterAnim(owner, anim), blendTime, 0, 1);
    }
    lastPressed = pressed;
    if (animationIsRunning(owner->m_grappler->m_anims[0xBA]))
        return;
    if (owner->m_grappler->m_stamina.cur <= minGrapplerStamina || breakDamage <= grapplerHealth - owner->m_grappler->m_health)
        breakFree();
    else if (healthDeltaAchieved(0.0f))
        ((StateGrapple *)STATE_AT(owner->m_grappler, ST_GRAPPLE))->release(false);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Grapple", update__13StateGrappled);
#endif
/* We took at least releaseDamage since the grab (counting `delta` more). */
int StateGrappled::healthDeltaAchieved(float delta)
{
    return releaseDamage <= health - (owner->m_health - delta);
}
/* 0: hold animation 0xAE. 1: animation 0xD1 looping with the meter full. 2: animation 0xD2 looping at `speed`. */
void StateGrappled::enterSubState(SubState s)
{
    switch (s) {
    case SUB_0:
        anim = 0xAE;
        animationTransitionInto(owner->m_anims[anim], blendTime, 1, 1);
        break;
    case SUB_1:
        anim = 0xD1;
        animationLoop(owner->m_anims[anim], true);
        animationTransitionInto(owner->m_anims[anim], blendTime, 1, 1);
        meter = 100.0f;
        break;
    case SUB_2:
        anim = 0xD2;
        if (speed != 0.0f)
            animationSetSpeed(owner->m_anims[anim], speed);
        animationLoop(*monsterAnim(owner, anim), true);
        animationTransitionInto(owner->m_anims[anim], blendTime, 1, 1);
        break;
    }
    subState = s;
}
/* Player 1 held by player 2: the camera goes back to us (in the unified view it re-frames both monsters, and unless
 * the camera is in state 1 it detaches when we are dead or else switches to POV 1). */
void StateGrappled::detach(void)
{
    int player = owner->m_playerNum;

    if (player == 1 && owner->m_grappler != 0 && owner->m_grappler->m_playerNum == 2) {
        Cameras::SetCameraMonster(owner->m_monsterNum, owner);
        if (gUseUnifiedView) {
            Cameras::InitCCMonsters(owner, owner->m_grappler);
            if (Cameras::m_cameras.m_state != player) {
                if (owner->m_dead)
                    Cameras::DetachCamera(2, 0);
                else
                    Cameras::SetCameraPOV(2, Camera::POV_1);
            }
        } else {
            owner->m_padFlags.f16E8 = 0;
        }
    }
    owner->m_grappler = 0;
    owner->m_flags |= 2;
    owner->setShadowOnOff(true);
}
/* Both let go: the grabber is pushed back (breakBack) and up into StateGrappleBreak, we are pushed along its facing and
 * up and fall (Monster+0x7978). */
#ifdef NON_MATCHING
/* equivalent: 92/96 words (grabber velocity address computed after the m_cs load) */
void StateGrappled::breakFree(void)
{
    Monster *m = owner;
    Monster *g = m->m_grappler;

    if (g != 0)
        ((StateGrapple *)STATE_AT(g, ST_GRAPPLE))->detach();
    detach();
    vecPush(MONSTER_VEL(g), (_fvector *)g->m_cs->mat[1], -breakBack);
    vecPush(MONSTER_VEL(g), (_fvector *)g->m_cs->mat[2], breakUp);
    g->enterNewState(STATE_AT(g, ST_GRAPPLE_BREAK));
    if (g->m_playerNum == 1 && gUseUnifiedView && Cameras::m_cameras.m_state != 5)
        Cameras::SetCameraPOV(2, (Camera::CameraPOV)5);
    vecPush(MONSTER_VEL(m), (_fvector *)g->m_cs->mat[1], pushFwd);
    vecPush(MONSTER_VEL(m), (_fvector *)g->m_cs->mat[2], pushUp);
    m->m_freeFalling = 1;
    m->m_fallTime = frames;
    m->enterNewState((MonsterState *)m->m_stateRef);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Grapple", breakFree__13StateGrappled);
#endif
int StateGrappled::isOverhead(void)
{
    return (unsigned)subState < 3;
}
void StateGrappled::handlePreemption(MonsterState *next)
{
    Monster *g = owner->m_grappler;

    if (g != 0) {
        ((StateGrapple *)STATE_AT(g, ST_GRAPPLE))->detach();
        g->enterNewState(STATE_AT(g, ST_IDLE));
    }
    detach();
}
INCLUDE_ASM("asm/nonmatchings/game/Grapple", _vt$13StateGrappled);
INCLUDE_ASM("asm/nonmatchings/game/Grapple", _vt$12StateGrapple);
INCLUDE_ASM("asm/nonmatchings/game/Grapple", __tf12StateGrapple);
unsigned char getSubState__12StateGrapple(void *self) __asm__("getSubState__12StateGrapple");
unsigned char getSubState__12StateGrapple(void *self)
{
    return *(unsigned char *)((char *)self + 0x6C);
}
float StateGrapple::getRecoilFields(void)
{
    return recoil;
}
float StateGrapple::getAttachWindowStart(void)
{
    return attachStart;
}
float StateGrapple::getAttachWindowEnd(void)
{
    return attachEnd;
}
INCLUDE_ASM("asm/nonmatchings/game/Grapple", __tf13StateGrappled);
void StateGrappled::setStruggleDisable(int fields)
{
    struggleDisable = fields;
}
unsigned char getSubState__13StateGrappled(void *self) __asm__("getSubState__13StateGrappled");
unsigned char getSubState__13StateGrappled(void *self)
{
    return *(unsigned char *)((char *)self + 0x4C);
}
