#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/monster_state.h"
int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);

/* Attacks on the victim held overhead (StateGrappleLift, GrappleThrow.cpp). StateGrappleOHAttack hits it once with
 * animation 0xB1, or 0xBA (the special: mapped action 5, or the regular attack once the victim lost enough health),
 * which knocks it out of the hold. StateMonkeyOHAttack (Congar) alternates 0xB1 and 0xB2 and can chain a second hit
 * with a fresh press during the window [chainPct, endPct] of the animation. The regular hit puts the victim in
 * substate 2 of StateGrappled; the special releases it into StateGrappleBreak (Monster+0xE990). */

#define STATE_AT(m, off) ((MonsterState *)((char *)(m) + (off)))
#define ST_IDLE 0x7984
#define ST_GRAPPLE 0xDCE0
#define ST_GRAPPLED 0xDDCC
#define ST_GRAPPLE_BREAK 0xE990
#define MI(p, o) (*(int *)((char *)(p) + (o)))
/* MonsterDynamics (Monster+0x100) */
#define MONSTER_DYN(m) ((MonsterDynamics *)((char *)(m) + 0x100))
#define MONSTER_SOUND(m) ((MonsterSound *)(m)->m_sound)
/* the reaction states (flags & 0x10) keep the monster that hit last at 0x524 */
#define LAST_ATTACKER(st) (*(Monster **)((char *)(st) + 0x524))
/* virtual call through the retail vtable (entries are {this delta, 0, function}) */
#define VCALL_INT(st, slot) \
    (((int (*)(void *))*(void **)((char *)(st)->vptr + (slot) + 4))((char *)(st) + *(short *)((char *)(st)->vptr + (slot))))
#define VT_TRANSITION_FEASIBLE 0x28
#define ANIM_OH_HIT 0xB1
#define ANIM_OH_HIT2 0xB2
#define ANIM_OH_SPECIAL 0xBA
#define ACTION_SPECIAL 5 /* PadFlags::curMappedAction of the special attack */

struct ActuatorData {
    int w[8];
};
void inputSetActuator(int pad, ActuatorData *a);
void animationSetDirection(_animHandle h, bool b);
/* engine.h declares it int; retail tests the result as a bool (xori 1) */
bool animIsRunning(_animHandle h) __asm__("animationIsRunning__FG11_animHandle");

class MonsterDynamics {
public:
    void updateMove(bool fromPad);
    void updateTurn(bool fromPad);
};
class StateGrapple : public MonsterState {
public:
    void release(bool thrown);
};
class StateGrappled : public MonsterState {
public:
    enum SubState { SUB_0, SUB_1, SUB_2 };

    char pad14[0x50 - 0x14];
    float struggleDisable; /* 0x50: fields left before struggling counts */

    int healthDeltaAchieved(float delta);
    void enterSubState(SubState s);
    int isOverhead(void);
};
#define GRAPPLED(m) ((StateGrappled *)STATE_AT(m, ST_GRAPPLED))
/* Monster::getAnim, inlined */
#define MONSTER_ANIM(m, a) ((_animHandle *)((char *)(m) + ((int)(a) * 16 + 0x1CF0)))

class AttackState : public MonsterState {
public:
    int unk14; /* 0x14 */
    int unk18; /* 0x18 */

    void creditStaminaForAttack(float f);
    int inWindUp(void);
    int inWindDown(void);
};

/* Monster+0xDE68 (id 9). Two sets of tweaks: the regular hit (0xB1) and the special (0xBA). */
class StateGrappleOHAttack : public AttackState {
public:
    float blendTime;         /* 0x1C: 100 */
    float runFrames;         /* 0x20: 180 */
    float hitPct;            /* 0x24: 0.5, point of the animation where the hit lands */
    float damage;            /* 0x28: 6, also the victim health lost that turns the regular hit into the special */
    float cost;              /* 0x2C: 3, stamina */
    float specialBlendTime;  /* 0x30: 100 (50 when zero) */
    float specialRunFrames;  /* 0x34: 180 (40 when zero) */
    float specialHitPct;     /* 0x38: 0.5 */
    float specialDamage;     /* 0x3C: 6 (10 when zero) */
    float specialCost;       /* 0x40: 3 (10 when zero) */
    float struggleDisable;   /* 0x44: 30, fields the victim cannot struggle after the regular hit */
    int hit;                 /* 0x48: the hit landed */
    unsigned char special;   /* 0x4C */
    int anim;                /* 0x50: MonsterAnim playing */

    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void update(void);
};
typedef char _size_StateGrappleOHAttack[sizeof(StateGrappleOHAttack) == 0x54 ? 1 : -1];

/* Congar's version (id 0x36): two alternating hits that can be chained. */
class StateMonkeyOHAttack : public AttackState {
public:
    float blendTime;         /* 0x1C: 100 */
    float runFrames;         /* 0x20: 180 */
    float hitPct;            /* 0x24: 0.5 */
    float damage;            /* 0x28: 6 */
    float cost;              /* 0x2C: 3, stamina (also of a chained hit) */
    float specialBlendTime;  /* 0x30: 100 */
    float specialRunFrames;  /* 0x34: 180 */
    float specialHitPct;     /* 0x38: 0.5 */
    float specialDamage;     /* 0x3C: 6 */
    float specialCost;       /* 0x40: 3 */
    float struggleDisable;   /* 0x44: 30 */
    float chainPct;          /* 0x48: 0.5, a chained hit can be asked for from here... */
    float endPct;            /* 0x4C: 1.0, ...to here, where the attack ends */
    int anim;                /* 0x50 */
    float pctOffset;         /* 0x54: added to the animation percent */
    int hit;                 /* 0x58 */
    int chained;             /* 0x5C */
    int special;             /* 0x60 */

    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void update(void);
    void startAttack(MonsterAnim a);
};
typedef char _size_StateMonkeyOHAttack[sizeof(StateMonkeyOHAttack) == 0x64 ? 1 : -1];

INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", _vt$19StateMonkeyOHAttack);
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", _vt$20StateGrappleOHAttack);
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", __20StateGrappleOHAttack);
/* No mapped action, or the special one; then transitionFeasible decides. */
int StateGrappleOHAttack::transitionOK(void)
{
    int a = owner->m_padFlags.curMappedAction;
    int ok = 0;

    if (a == 0 || a == ACTION_SPECIAL)
        ok = VCALL_INT(this, VT_TRANSITION_FEASIBLE) != 0;
    return ok;
}
/* Attacks on and the victim held overhead. Picks the animation (the special when asked for or once the victim lost
 * damage health) and checks the stamina; without the special animation the regular one is used. */
int StateGrappleOHAttack::transitionFeasible(void)
{
    float need;
    Monster *m = owner;

    if (m->m_attacksEnabled == 0)
        return 0;
    if (m->m_target == 0 || !GRAPPLED(m->m_target)->isOverhead())
        return 0;
    if (owner->m_padFlags.curMappedAction == ACTION_SPECIAL) {
        anim = ANIM_OH_SPECIAL;
        special = 1;
        need = specialCost;
    } else {
        special = 0;
        anim = ANIM_OH_HIT;
        need = cost;
    }
    if (!special && GRAPPLED(owner->m_target)->healthDeltaAchieved(damage))
        anim = ANIM_OH_SPECIAL;
    if (anim == 0)
        return 0;
    if (need <= owner->m_stamina.cur) {
        if (MONSTER_ANIM(owner, anim)->ci == 0)
            anim = ANIM_OH_HIT;
        return 1;
    }
    return 0;
}
#ifdef NON_MATCHING
/* equivalent: 55/105 words (retail keeps the anim handle address unfolded and copies it before the ternary); tools/difftest.py 73/73 */
void StateGrappleOHAttack::transitionInto(void)
{
    float c;

    frames = 0;
    if (specialBlendTime == 0.0f)
        specialBlendTime = 50.0f;
    if (specialRunFrames == 0.0f)
        specialRunFrames = 40.0f;
    if (specialHitPct == 0.0f)
        specialHitPct = 0.5f;
    if (specialDamage == 0.0f)
        specialDamage = 10.0f;
    if (specialCost == 0.0f)
        specialCost = 10.0f;
    animationSetTotalRunFrames(*MONSTER_ANIM(owner, anim), anim != ANIM_OH_HIT ? specialRunFrames : runFrames);
    animationTransitionInto(*MONSTER_ANIM(owner, anim), anim == ANIM_OH_HIT ? blendTime : specialBlendTime, 1, 7);
    c = !special ? cost : specialCost;
    owner->m_stamina.drain(c, true, false);
    owner->m_unk49 = 1;
    MONSTER_SOUND(owner)->playGrappleAttackSound();
    hit = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", transitionInto__20StateGrappleOHAttack);
#endif
/* At the hit point the victim takes the damage: the special releases it (StateGrappleBreak with us as the attacker),
 * the regular hit makes it stop struggling for a while. Idle at the end of the animation. */
#ifdef NON_MATCHING
/* equivalent: 3/175 words (one more saved register in retail shifts the frame; anim/rumble addresses unfolded); tools/difftest.py 68/68 */
void StateGrappleOHAttack::update(void)
{
    float pct;

    MonsterState::update();
    MONSTER_DYN(owner)->updateMove(false);
    MONSTER_DYN(owner)->updateTurn(false);
    pct = animationGetCurrentPercent(*MONSTER_ANIM(owner, anim));
    if (owner->m_target != 0 && !hit
        && (anim == ANIM_OH_HIT ? hitPct <= pct : specialHitPct <= pct)) {
        Monster *v = (Monster *)owner->m_target;
        MonsterSound *vs;

        if (special == 1 || anim == ANIM_OH_SPECIAL) {
            ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->release(true);
            vs = MONSTER_SOUND(v);
            LAST_ATTACKER(STATE_AT(v, ST_GRAPPLE_BREAK)) = owner;
            vs->playTakeBatHitSound();
            MONSTER_SOUND(owner)->terminateGrappleSound();
        } else {
            GRAPPLED(v)->struggleDisable = (int)struggleDisable;
            vs = MONSTER_SOUND(v);
            GRAPPLED(v)->enterSubState(StateGrappled::SUB_2);
        }
        owner->registerComboHit(v);
        particleCreateFx((_fvector *)((char *)owner + 0x3DD0), MI(owner, 0xADD0), 1.0f, 0, 0.0f);
        creditStaminaForAttack(0.0f);
        hit = 1;
        vs->playTakeHit(owner);
        v->takeDamage(!special ? damage : specialDamage, false, owner);
        if (v->m_playerNum == 1) {
            ActuatorData a = *(ActuatorData *)((char *)game + 0x1227C0);
            inputSetActuator(v->m_index, &a);
        }
    } else if (1.0f <= pct) {
        owner->enterNewState(STATE_AT(owner, ST_IDLE));
        if (owner->m_target != 0)
            GRAPPLED(owner->m_target)->enterSubState(StateGrappled::SUB_0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", update__20StateGrappleOHAttack);
#endif
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", __19StateMonkeyOHAttack);
int StateMonkeyOHAttack::transitionOK(void)
{
    int a = owner->m_padFlags.curMappedAction;
    int ok = 0;

    if (a == ACTION_SPECIAL || a == 0)
        ok = VCALL_INT(this, VT_TRANSITION_FEASIBLE) != 0;
    return ok;
}
/* As StateGrappleOHAttack::transitionFeasible; the regular hit is 0xB1 or 0xB2 at random. */
#ifdef NON_MATCHING
/* equivalent: 62/65 words (layout of the regular-cost return); tools/difftest.py 154/154 */
int StateMonkeyOHAttack::transitionFeasible(void)
{
    Monster *m = owner;

    if (m->m_attacksEnabled == 0)
        return 0;
    if (m->m_target == 0 || !GRAPPLED(m->m_target)->isOverhead())
        return 0;
    if (owner->m_padFlags.curMappedAction == 0) {
        anim = mathfRand(0, 1) == 0 ? ANIM_OH_HIT : ANIM_OH_HIT2;
        special = 0;
        if (GRAPPLED(owner->m_target)->healthDeltaAchieved(damage))
            anim = ANIM_OH_SPECIAL;
    } else {
        anim = ANIM_OH_SPECIAL;
        special = 1;
    }
    if (anim == 0)
        return 0;
    {
        float cur = owner->m_stamina.cur;

        if (special == 0) {
            if (cost <= cur)
                return 1;
        } else if (specialCost <= cur) {
            return 1;
        }
        return 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", transitionFeasible__19StateMonkeyOHAttack);
#endif
void StateMonkeyOHAttack::transitionInto(void)
{
    float c;

    frames = 0;
    c = !special ? cost : specialCost;
    owner->m_stamina.drain(c, true, false);
    owner->m_unk49 = 1;
    startAttack((MonsterAnim)anim);
}
/* Regular hit: the victim stops struggling; a fresh press of the attack button between chainPct and endPct queues
 * the other hit (0xB1 <-> 0xB2), started once the animation ends. 0xBA, or the special, releases the victim.
 * Idle when the victim is gone or lost enough health, or at the end. */
#ifdef NON_MATCHING
/* equivalent: 12/322 words, size 0x4fc vs 0x508 (anim/rumble addresses unfolded, game read gp-relative in one rumble); tools/difftest.py 107/107 */
void StateMonkeyOHAttack::update(void)
{
    float pct;

    MonsterState::update();
    pct = animationGetCurrentPercent(*MONSTER_ANIM(owner, anim)) + pctOffset;
    switch (special) {
    case 0:
        if (hit) {
            if (chained) {
                if (!animIsRunning(*MONSTER_ANIM(owner, anim))) {
                    startAttack((MonsterAnim)(anim != ANIM_OH_HIT ? ANIM_OH_HIT : ANIM_OH_HIT2));
                    return;
                }
            } else if (endPct <= pct || !animIsRunning(*MONSTER_ANIM(owner, anim))) {
                owner->enterNewState(STATE_AT(owner, ST_IDLE));
                return;
            }
        } else if (owner->m_target != 0
                   && (anim == ANIM_OH_SPECIAL ? specialHitPct <= pct : hitPct <= pct)) {
            Monster *v = (Monster *)owner->m_target;

            if (anim == ANIM_OH_SPECIAL) {
                ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->release(true);
                LAST_ATTACKER(STATE_AT(v, ST_GRAPPLE_BREAK)) = owner;
                MONSTER_SOUND(v)->playTakeBatHitSound();
            } else {
                GRAPPLED(v)->struggleDisable = (int)struggleDisable;
                GRAPPLED(v)->enterSubState(StateGrappled::SUB_2);
                owner->registerComboHit((Monster *)owner->m_target);
                creditStaminaForAttack(0.0f);
                hit = 1;
                MONSTER_SOUND(v)->playTakeHit(owner);
                v->takeDamage(damage, false, owner);
                if (v->m_playerNum == 1) {
                    ActuatorData a = *(ActuatorData *)((char *)game + 0x1227C0);
                    inputSetActuator(v->m_index, &a);
                }
            }
        }
        if (owner->m_target == 0 || GRAPPLED(owner->m_target)->healthDeltaAchieved(0.0f)) {
            owner->enterNewState(STATE_AT(owner, ST_IDLE));
        } else if (!chained && chainPct <= pct && pct <= endPct && cost <= owner->m_stamina.cur
                   && owner->m_padFlags[0]->f2C != 0 && owner->m_padFlags[1]->f2C == 0) {
            chained = 1;
            owner->m_stamina.drain(cost, true, false);
        }
        break;
    case 1: {
        Monster *v = (Monster *)owner->m_target;

        if (v != 0 && !hit && specialHitPct <= pct) {
            ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->release(true);
            LAST_ATTACKER(STATE_AT(v, ST_GRAPPLE_BREAK)) = owner;
            owner->registerComboHit(v);
            creditStaminaForAttack(0.0f);
            hit = special;
            v->takeDamage(specialDamage, false, owner);
            if (v->m_playerNum == 1) {
                ActuatorData a = *(ActuatorData *)((char *)game + 0x1227C0);
                inputSetActuator(v->m_index, &a);
            }
            MONSTER_SOUND(v)->playTakeBatHitSound();
        } else if (1.0f <= pct) {
            owner->enterNewState(STATE_AT(owner, ST_IDLE));
            if (owner->m_target != 0)
                GRAPPLED(owner->m_target)->enterSubState(StateGrappled::SUB_0);
        }
        break;
    }
    }
    MONSTER_DYN(owner)->updateMove(false);
    MONSTER_DYN(owner)->updateTurn(true);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", update__19StateMonkeyOHAttack);
#endif
/* Plays a (0xB1 when the monster lacks it) from the start, forward and not looping. */
void StateMonkeyOHAttack::startAttack(MonsterAnim a)
{
    if (MONSTER_ANIM(owner, a)->ci == 0)
        a = (MonsterAnim)ANIM_OH_HIT;
    hit = 0;
    chained = 0;
    anim = a;
    pctOffset = 0;
    _animHandle &h = owner->m_anims[anim];
    animationLoop(h, false);
    animationSetDirection(h, true);
    animationSetTotalRunFrames(h, !special ? runFrames : specialRunFrames);
    animationTransitionInto(h, !special ? blendTime : specialBlendTime, 1, 7);
}
int AttackState::inWindUp(void)
{
    return 0;
}
int AttackState::inWindDown(void)
{
    return 0;
}
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", __tf20StateGrappleOHAttack);
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", __tf19StateMonkeyOHAttack);
INCLUDE_ASM("asm/nonmatchings/game/GrappleOHAttack", __tf11AttackState);
