#include "common.h"
#include "game/game.h"
#include "game/monster_state.h"
#include "game/hit_event.h"
#include "game/pickup.h"
#include "game/level_pickups.h"

/* States embedded in Monster (offsets from Monster's constructor). */
#define STATE_AT(m, off) ((MonsterState *)((char *)(m) + (off)))
#define ST_IDLE 0x7984
#define ST_COUNTER 0x7DD8
#define ST_RECOIL 0x7E30
#define ST_PUNCH 0x8470
#define ST_GRAPPLE 0xDCE0
#define ST_STUNNED 0x10714
#define STATE_ID_RECOIL 0x1C
/* virtual call through the retail vtable (entries are {this delta, 0, function}) */
#define VCALL_VOID(st, slot) \
    (((void (*)(void *))*(void **)((char *)(st)->vptr + (slot) + 4))((char *)(st) + *(short *)((char *)(st)->vptr + (slot))))
#define VT_TRANSITION_INTO 0x10

struct _hdResult;
class MonsterDynamics {
public:
    void updateTurn(bool b);
    void updateMove(bool b);
};
/* Counter attack (Monster+0x7DD8), entered from StateBlock. The counter animation is 0x40; counterSuccess (VU0, still
 * asm) marks `landed` when it connects. */
class StateCounter : public MonsterState {
public:
    char pad14[0x24 - 0x14];
    float blendTime;     /* 0x24: blend back into the block pose */
    char pad28[0x34 - 0x28];
    Monster *victim;     /* 0x34 */
    int anim;            /* 0x38: MonsterAnim playing (0x40 while countering) */
    int counterStart;    /* 0x3C */
    int counterEnd;      /* 0x40 */
    int landed;          /* 0x44: the counter hit `victim` */
    int landedHandled;   /* 0x48: the effects of the hit were applied */
    int stealsPickup;    /* 0x4C: the counter takes the victim's pickup */
    char pad50[0x58 - 0x50];

    int transitionOK(void);
    int transitionFeasible(void);
    void update(void);
    int acceptHit(HitEvent &e);
    void handleCollis(_hdResult &r);
    void handlePreemption(MonsterState *next);
};
typedef char _size_StateCounter[sizeof(StateCounter) == 0x58 ? 1 : -1];
class MonsterSound {
public:
    void playCounterAttackSound(void);
};
#define ST_COUNTERED 0x11114
#define ST_BLOCK 0x7DA0
#define VCALL_INT(st, slot) \
    (((int (*)(void *))*(void **)((char *)(st)->vptr + (slot) + 4))((char *)(st) + *(short *)((char *)(st)->vptr + (slot))))
#define VT_TRANSITION_FEASIBLE 0x28
class StatePunch : public MonsterState {
public:
    int transitionOK(void);
};
class StateGrapple : public MonsterState {
public:
    int transitionOK(void);
};
class StateStunned : public MonsterState {
public:
    void handleCollis(_hdResult &r);
};

/* Blocking (Monster+0x7DA0). The block pose depends on what the monster holds: animations 0x3C (bare), 0x3D (one-handed
 * pickup), 0x3E (two-handed pickup, or a locked target), 0x3F (pickup type 0x10), each only if the monster has it. While
 * blocking the damage taken is scaled by damageScale (damageScaleArmed when holding something or locked on). */
class StateBlock : public MonsterState {
public:
    float autoBlockRange;   /* 0x14: autoBlock looks for an attacking monster this close */
    float blendTime;        /* 0x18: animation blend into the block pose */
    float runFrames;        /* 0x1C: length of the block animation */
    float damageScale;      /* 0x20 */
    float damageScaleArmed; /* 0x24 */
    int unk28;
    unsigned raiseFrames;   /* 0x2C: frames with the button held before the block counts */
    int blocking;           /* 0x30: hits are being blocked (isBlocking) */
    int anim;               /* 0x34: MonsterAnim of the current pose */

    int transitionOK(void);
    void transitionInto(void);
    int chooseBlock(void);
    void update(void);
    void handleCollis(_hdResult &r);
    void handlePreemption(MonsterState *next);
    int autoBlock(void);
    int isBlockable(HitEvent &e);
};
typedef char _size_StateBlock[sizeof(StateBlock) == 0x38 ? 1 : -1];

INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", collisTestCloseRangeAttack__20PunchSwipeConfigBaseiP7MonsterR10HitHistoryi);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12MonsterStateQ212MonsterState2IdUi);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handleCollis__12MonsterStateR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __11AttackStateQ212MonsterState2IdUi);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", creditStaminaForAttack__11AttackStatef);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", chooseBaseIdle__9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", chooseFancyIdle__9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", startBaseAnim__9StateIdle11MonsterAnim);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__9StateIdleP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", startChain__13StateBatSwipeiiRQ213StateBatSwipe11SwipeConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handleCollis__13StateBatSwipeR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__13StateBatSwipeP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", acceptHit__13StateBatSwipeR8HitEvent);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __10StateBlock);
/* Blocking needs the bare block animation, attacks enabled, the feet on something and the block button held. */
int StateBlock::transitionOK(void)
{
    Monster *m = owner;

    if (m->m_anims[0x3C].a == 0 || m->m_attacksEnabled == 0 || m->m_freeFalling != 0)
        return 0;
    return m->m_padFlags[0]->block != 0;
}
/* Coming out of a recoil (state 0x1C) the block is up at once, and recoil animation 0x4D blends in slowly. */
#ifdef NON_MATCHING
/* untuned: 23/56 words; tools/difftest.py 200/200 */
void StateBlock::transitionInto(void)
{
    float blend;

    frames = 0;
    anim = chooseBlock();
    blocking = 0;
    blend = blendTime;
    if (owner->m_prevState[0] == STATE_ID_RECOIL) {
        if (*(int *)((char *)STATE_AT(owner, ST_RECOIL) + 0x64) != 0)
            blocking = 1;
        if (*(int *)((char *)STATE_AT(owner, ST_RECOIL) + 0x38) == 0x4D)
            blend = 140.0f;
    }
    animationSetTotalRunFrames(owner->m_anims[anim], runFrames);
    animationTransitionInto(owner->m_anims[anim], blend, 0, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__10StateBlock);
#endif
/* Picks the block pose and sets the damage scale for it. */
#ifdef NON_MATCHING
/* untuned: 8/39 words (movn vs branches); tools/difftest.py 200/200 */
int StateBlock::chooseBlock(void)
{
    Monster *m = owner;
    int pose = 0x3C;

    if (m->m_pickup != 0) {
        Pickup *p = *(Pickup **)m->m_pickup;

        if ((p->bits >> 1) & 1) {
            if (m->m_anims[0x3E].a != 0)
                pose = 0x3E;
        } else if (p->pickupType == 0x10) {
            if (m->m_anims[0x3F].a != 0)
                pose = 0x3F;
        } else if (m->m_anims[0x3D].a != 0) {
            pose = 0x3D;
        }
        m->m_damageModifier = damageScaleArmed;
    } else if (m->m_target != 0) {
        m->m_damageModifier = damageScaleArmed;
        if (m->m_anims[0x3E].a != 0)
            pose = 0x3E;
    } else {
        m->m_damageModifier = damageScale;
    }
    return pose;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", chooseBlock__10StateBlock);
#endif
/* Holding the button keeps blocking (after raiseFrames) and allows a counter; letting go punches, grapples or idles. */
#ifdef NON_MATCHING
/* untuned: 68/90 words; tools/difftest.py 200/200 */
void StateBlock::update(void)
{
    MonsterState::update();
    if (owner->m_unk1E8 < 0.0f)
        owner->m_unk1B8 = 1.5f;
    ((MonsterDynamics *)((char *)owner + 0x100))->updateTurn(true);
    ((MonsterDynamics *)((char *)owner + 0x100))->updateMove(false);
    owner->updateLock((MonsterReticleState)1);
    if (owner->m_padFlags[0]->block != 0) {
        if (frames >= raiseFrames)
            blocking = 1;
        if (((StateCounter *)STATE_AT(owner, ST_COUNTER))->transitionOK())
            owner->enterNewState(STATE_AT(owner, ST_COUNTER));
    } else if (((StatePunch *)STATE_AT(owner, ST_PUNCH))->transitionOK()) {
        owner->enterNewState(STATE_AT(owner, ST_PUNCH));
    } else if (((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->transitionOK()) {
        owner->enterNewState(STATE_AT(owner, ST_GRAPPLE));
    } else {
        owner->enterNewState(STATE_AT(owner, ST_IDLE));
    }
    /* lost both the target and the pickup while in the armed pose: start over with a new pose */
    if (owner->m_target == 0 && owner->m_pickup == 0 && anim == 0x3E)
        VCALL_VOID(this, VT_TRANSITION_INTO);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__10StateBlock);
#endif
void StateBlock::handleCollis(_hdResult &r)
{
    ((StateStunned *)STATE_AT(owner, ST_STUNNED))->handleCollis(r);
}
void StateBlock::handlePreemption(MonsterState *next)
{
    owner->m_unk1B8 = 1.0f;
}
/* Used by the AI: block if the closest monster in range is in one of the attack states (3, 0x1A, 0x28, 0x29). */
int StateBlock::autoBlock(void)
{
    Monster *m = owner->getClosestMonster(autoBlockRange);

    if (m == 0)
        return 0;
    switch (m->m_state[0]) {
    case 3:
    case 0x1A:
    case 0x28:
    case 0x29:
        return 1;
    }
    return 0;
}
/* Sources 6 and 0x14 can't be blocked, nor source 5 with detail 0x40. */
#ifdef NON_MATCHING
/* untuned: 8/17 words; tools/difftest.py 200/200 */
int StateBlock::isBlockable(HitEvent &e)
{
    if (e.source == 6 || e.source == 0x14)
        return 0;
    if (e.source != 5)
        return 1;
    if (e.sourceArg == 0x40)
        return 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", isBlockable__10StateBlockR8HitEvent);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__10StateCatchP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12StateCounter);
/* A counter needs the counter button and stamina that is not exhausted. */
int StateCounter::transitionOK(void)
{
    int ok;

    if (owner->m_stamina.exhausted != 0)
        return 0;
    ok = 0;
    if (owner->m_padFlags[0]->counter != 0)
        ok = VCALL_INT(this, VT_TRANSITION_FEASIBLE) != 0;
    return ok;
}
/* Not while locked on a target, only with the counter animation, and not holding a two-handed pickup. */
#ifdef NON_MATCHING
/* untuned: 3/21 words (retail has three hazard nops after the first branch); tools/difftest.py 200/200 */
int StateCounter::transitionFeasible(void)
{
    Monster *m = owner;

    if (m->m_target != 0 || m->m_anims[0x40].a == 0)
        return 0;
    if (m->m_pickup == 0)
        return 1;
    return !(((*(Pickup **)m->m_pickup)->bits >> 1) & 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__12StateCounter);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12StateCounter);
/* Once the counter lands: the victim is countered (and loses its pickup to us if stealsPickup and our hands are free),
 * HUD message 6 and the counter sound. When the counter animation ends without landing, back to the block pose (or
 * animation 0x5A); after a landed counter or any other animation, back to Block or Idle by the block button. */
#ifdef NON_MATCHING
/* untuned: 57/153 words; tools/difftest.py 200/200 */
void StateCounter::update(void)
{
    MonsterState::update();
    if (owner->m_unk1E8 < 0.0f)
        owner->m_unk1B8 = 1.5f;
    if (landed != 0 && landedHandled == 0 && victim != 0) {
        if (stealsPickup != 0) {
            int held = victim->m_pickup;

            if (held != 0) {
                if (owner->m_pickup != 0) {
                    victim->dropPickup();
                } else {
                    victim->dropPickup();
                    owner->m_pickup = held;
                    LevelPickups::grabPickup(*(PickupIter *)&held, owner->m_id);
                }
            }
        }
        victim->enterNewState(STATE_AT(victim, ST_COUNTERED));
        game->m_huds[owner->m_cameraView].addMessage(6, 0);
        ((MonsterSound *)((char *)owner + 0x1A7C))->playCounterAttackSound();
        landedHandled = 1;
    }
    if (anim == 0x40) {
        if (!animationIsRunning(owner->m_anims[0x40])) {
            if (landed == 0) {
                int pose = 0x5A;

                if (owner->m_padFlags[0]->block != 0)
                    pose = ((StateBlock *)STATE_AT(owner, ST_BLOCK))->chooseBlock();
                anim = pose;
                animationTransitionInto(owner->m_anims[pose], blendTime, 1, 1);
            } else {
                owner->enterNewState(STATE_AT(owner, owner->m_padFlags[0]->block != 0 ? ST_BLOCK : ST_IDLE));
            }
        }
    } else if (!animationIsTransitioning(owner->m_anims[anim])) {
        owner->enterNewState(STATE_AT(owner, owner->m_padFlags[0]->block != 0 ? ST_BLOCK : ST_IDLE));
    }
    ((MonsterDynamics *)((char *)owner + 0x100))->updateTurn(true);
    ((MonsterDynamics *)((char *)owner + 0x100))->updateMove(false);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12StateCounter);
#endif
/* A counter can't be refused. */
int StateCounter::acceptHit(HitEvent &e)
{
    return 1;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", counterSuccess__12StateCounterR7MonsterR8_fvector);
void StateCounter::handleCollis(_hdResult &r)
{
    ((StateStunned *)STATE_AT(owner, ST_STUNNED))->handleCollis(r);
}
void StateCounter::handlePreemption(MonsterState *next)
{
    owner->m_unk1B8 = 1.0f;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __14StateCountered);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__14StateCountered);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__14StateCountered);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__14StateCountered);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__14StateCounteredP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __10StateDeath);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__10StateDeath);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__10StateDeath);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__10StateDeath);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", findVictor__10StateDeath);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__10StateDeathP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", setKiller__10StateDeathP7Monster);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __14StateGunAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__14StateGunAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__14StateGunAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__14StateGunAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __16StateGetupAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__16StateGetupAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__16StateGetupAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__16StateGetupAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", enterSubState__16StateGetupAttackQ216StateGetupAttack8SubState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", acceptHit__16StateGetupAttackR8HitEvent);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12StateImpaled);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__12StateImpaled);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12StateImpaled);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12StateImpaled);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handleCollis__12StateImpaledR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__12StateImpaledP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12StateJavelin);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__12StateJavelin);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12StateJavelin);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12StateJavelin);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", startChain__10StatePunchiiRQ210StatePunch11PunchConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", playPunchEffect__10StatePunchR8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handleCollis__10StatePunchR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__10StatePunchP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", acceptHit__10StatePunchR8HitEvent);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getFistSize__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getAttachRegion__11StatePickUpR8_fvectorT1);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__11StatePickUpP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", init__11StateRecoilP7Monster);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getRecoilAnim__11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", acceptHit__11StateRecoilR8HitEvent);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__11StateRecoilP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12StateStunned);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__12StateStunned);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12StateStunned);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12StateStunned);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__12StateStunnedP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handleCollis__12StateStunnedR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", setStarsCs__12StateStunnedP3_cs);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", updateStars__12StateStunned);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __10StateTaunt);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__10StateTaunt);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__10StateTaunt);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__10StateTaunt);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__10StateThrowP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", cancelOverride__10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __19StateTwoHandedThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__19StateTwoHandedThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__19StateTwoHandedThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__19StateTwoHandedThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__19StateTwoHandedThrow);
void handlePreemption__19StateTwoHandedThrowP12MonsterState(void *self) __asm__("handlePreemption__19StateTwoHandedThrowP12MonsterState");
void handlePreemption__19StateTwoHandedThrowP12MonsterState(void *self)
{
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12StateShocked);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__12StateShocked);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12StateShocked);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12StateShocked);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", handlePreemption__12StateShockedP12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", startChain__16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", launchProjectile__16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", fade__12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __15StateBigTakeHit);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__15StateBigTakeHit);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__15StateBigTakeHit);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", update__15StateBigTakeHit);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", damageToPickup__20PunchSwipeConfigBase);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", damageCaused__20PunchSwipeConfigBase);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12MonsterState);
void init__12MonsterStateP7Monster(void *self, int v) __asm__("init__12MonsterStateP7Monster");
void init__12MonsterStateP7Monster(void *self, int v)
{
    *(int *)((char *)self + 0xC) = v;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionInto__12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionOK__12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", transitionFeasible__12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", acceptHit__12MonsterStateR8HitEvent);
void handlePreemption__12MonsterStateP12MonsterState(void *self) __asm__("handlePreemption__12MonsterStateP12MonsterState");
void handlePreemption__12MonsterStateP12MonsterState(void *self)
{
}
int getFieldsInState__12MonsterState(void *self) __asm__("getFieldsInState__12MonsterState");
int getFieldsInState__12MonsterState(void *self)
{
    return *(int *)((char *)self + 0x8);
}
int getStateId__12MonsterState(void *self) __asm__("getStateId__12MonsterState");
int getStateId__12MonsterState(void *self)
{
    return *(int *)((char *)self + 0x0);
}
int getCaps__12MonsterState(void *self) __asm__("getCaps__12MonsterState");
int getCaps__12MonsterState(void *self)
{
    return *(int *)((char *)self + 0x4);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", setCaps__12MonsterStateUs);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", clearCaps__12MonsterStateUs);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", hasCaps__12MonsterStateUi);
int getMon__12MonsterState(void *self) __asm__("getMon__12MonsterState");
int getMon__12MonsterState(void *self)
{
    return *(int *)((char *)self + 0xC);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", func_0015A100);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", func_0015A170);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", func_0015A178);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$15StateBigTakeHit);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12StateShocked);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$19StateTwoHandedThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$10StateTaunt);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12StateStunned);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$Q210StatePunch11PunchConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12StateJavelin);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12StateImpaled);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$14StateGunAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$16StateGetupAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$10StateDeath);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$14StateCountered);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12StateCounter);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$10StateBlock);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$Q213StateBatSwipe11SwipeConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$11AttackState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", _vt$12MonsterState);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", D_006EBB68);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", D_006EBB78);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf9StateIdle);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tfQ213StateBatSwipe11SwipeConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", damageToPickup__Q213StateBatSwipe11SwipeConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getAttackTrans__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getRecvrTransMod__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getRecoilFields__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getHitPause__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getSphereRadius__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getDamageMod__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getCurrentAnim__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getCurrentConfig__13StateBatSwipe);
int getCurrentSwipe__13StateBatSwipe(void *self) __asm__("getCurrentSwipe__13StateBatSwipe");
int getCurrentSwipe__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2D4C);
}
int getFieldsInConfig__13StateBatSwipe(void *self) __asm__("getFieldsInConfig__13StateBatSwipe");
int getFieldsInConfig__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2DAC);
}
int getCounterStartField__13StateBatSwipe(void *self) __asm__("getCounterStartField__13StateBatSwipe");
int getCounterStartField__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2DB0);
}
int getCounterEndField__13StateBatSwipe(void *self) __asm__("getCounterEndField__13StateBatSwipe");
int getCounterEndField__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2DB4);
}
int getTier__13StateBatSwipe(void *self) __asm__("getTier__13StateBatSwipe");
int getTier__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2D48);
}
int isWindingDown__13StateBatSwipe(void *self) __asm__("isWindingDown__13StateBatSwipe");
int isWindingDown__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2D5C);
}
int inCollisionWindow__13StateBatSwipe(void *self) __asm__("inCollisionWindow__13StateBatSwipe");
int inCollisionWindow__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2DC0);
}
int inCounterableWindow__13StateBatSwipe(void *self) __asm__("inCounterableWindow__13StateBatSwipe");
int inCounterableWindow__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2DC4);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", inWindUp__13StateBatSwipe);
int inWindDown__13StateBatSwipe(void *self) __asm__("inWindDown__13StateBatSwipe");
int inWindDown__13StateBatSwipe(void *self)
{
    return *(int *)((char *)self + 0x2D5C);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", isStunHit__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", isKnockbackHit__13StateBatSwipe);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf10StateBlock);
int isBlocking__10StateBlock(void *self) __asm__("isBlocking__10StateBlock");
int isBlocking__10StateBlock(void *self)
{
    return *(int *)((char *)self + 0x30);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf10StateCatch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12StateCounter);
int getCounterStartField__12StateCounter(void *self) __asm__("getCounterStartField__12StateCounter");
int getCounterStartField__12StateCounter(void *self)
{
    return *(int *)((char *)self + 0x3C);
}
int getCounterEndField__12StateCounter(void *self) __asm__("getCounterEndField__12StateCounter");
int getCounterEndField__12StateCounter(void *self)
{
    return *(int *)((char *)self + 0x40);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf14StateCountered);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf10StateDeath);
int getKiller__10StateDeath(void *self) __asm__("getKiller__10StateDeath");
int getKiller__10StateDeath(void *self)
{
    return *(int *)((char *)self + 0x1C);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf16StateGetupAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf14StateGunAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12StateImpaled);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12StateJavelin);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf11StatePickUp);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tfQ210StatePunch11PunchConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", damageCaused__Q210StatePunch11PunchConfig);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getAttackTrans__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getRecvrTransMod__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getRecoilFields__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getHitPause__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getSphereRadius__10StatePunch);
int getTier__10StatePunch(void *self) __asm__("getTier__10StatePunch");
int getTier__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x2988);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getCurrentAnim__10StatePunch);
int getCurrentPunch__10StatePunch(void *self) __asm__("getCurrentPunch__10StatePunch");
int getCurrentPunch__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x298C);
}
int getFieldsInConfig__10StatePunch(void *self) __asm__("getFieldsInConfig__10StatePunch");
int getFieldsInConfig__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x29EC);
}
int getCounterStartField__10StatePunch(void *self) __asm__("getCounterStartField__10StatePunch");
int getCounterStartField__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x29F0);
}
int getCounterEndField__10StatePunch(void *self) __asm__("getCounterEndField__10StatePunch");
int getCounterEndField__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x29F4);
}
void setPunchEffect__10StatePunch4FxId(void *self, int v) __asm__("setPunchEffect__10StatePunch4FxId");
void setPunchEffect__10StatePunch4FxId(void *self, int v)
{
    *(int *)((char *)self + 0x2960) = v;
}
int getPunchEffect__10StatePunch(void *self) __asm__("getPunchEffect__10StatePunch");
int getPunchEffect__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x2960);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", isStunHit__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", isKnockbackHit__10StatePunch);
int isWindingDown__10StatePunch(void *self) __asm__("isWindingDown__10StatePunch");
int isWindingDown__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x299C);
}
int inCollisionWindow__10StatePunch(void *self) __asm__("inCollisionWindow__10StatePunch");
int inCollisionWindow__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x2A00);
}
int inCounterableWindow__10StatePunch(void *self) __asm__("inCounterableWindow__10StatePunch");
int inCounterableWindow__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x2A04);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", inWindUp__10StatePunch);
int inWindDown__10StatePunch(void *self) __asm__("inWindDown__10StatePunch");
int inWindDown__10StatePunch(void *self)
{
    return *(int *)((char *)self + 0x299C);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getCurrentConfig__10StatePunch);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", setTransDirection__11StateRecoilR8_fvector);
void setTransDelta__11StateRecoilf(void *self, float v) __asm__("setTransDelta__11StateRecoilf");
void setTransDelta__11StateRecoilf(void *self, float v)
{
    *(float *)((char *)self + 0x4C) = v;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", setHitDirection__11StateRecoilR8_fvector);
void setRecoilFields__11StateRecoilf(void *self, float v) __asm__("setRecoilFields__11StateRecoilf");
void setRecoilFields__11StateRecoilf(void *self, float v)
{
    *(float *)((char *)self + 0x1C) = v;
}
void setHitPause__11StateRecoilf(void *self, float v) __asm__("setHitPause__11StateRecoilf");
void setHitPause__11StateRecoilf(void *self, float v)
{
    *(float *)((char *)self + 0x28) = v;
}
void setHitSource__11StateRecoili(void *self, int v) __asm__("setHitSource__11StateRecoili");
void setHitSource__11StateRecoili(void *self, int v)
{
    *(int *)((char *)self + 0x30) = v;
}
void setHitType__11StateRecoili(void *self, int v) __asm__("setHitType__11StateRecoili");
void setHitType__11StateRecoili(void *self, int v)
{
    *(int *)((char *)self + 0x34) = v;
}
int isBlocking__11StateRecoil(void *self) __asm__("isBlocking__11StateRecoil");
int isBlocking__11StateRecoil(void *self)
{
    return *(int *)((char *)self + 0x64);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", isStaggering__11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", getBlendTime__11StateRecoil);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12StateStunned);
int getStarsCs__12StateStunned(void *self) __asm__("getStarsCs__12StateStunned");
int getStarsCs__12StateStunned(void *self)
{
    return *(int *)((char *)self + 0x30);
}
void setRecoilFirst__12StateStunnedb(void *self, int v) __asm__("setRecoilFirst__12StateStunnedb");
void setRecoilFirst__12StateStunnedb(void *self, int v)
{
    *(int *)((char *)self + 0x24) = v;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf10StateTaunt);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf10StateThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf19StateTwoHandedThrow);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12StateShocked);
void setDamage__12StateShockedf(void *self, float v) __asm__("setDamage__12StateShockedf");
void setDamage__12StateShockedf(void *self, float v)
{
    *(float *)((char *)self + 0x40) = v;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf16StateStompAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf16StateTazerAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", launchProjectile__16StateTazerAttackPv);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf17StateShieldAttack);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf15StateBigTakeHit);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf12StateVictory);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", acceptHit__12StateVictoryR8HitEvent);
int getVictoryAnim__12StateVictory(void *self) __asm__("getVictoryAnim__12StateVictory");
int getVictoryAnim__12StateVictory(void *self)
{
    return *(int *)((char *)self + 0x14);
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", fade__12StateVictoryPv);
INCLUDE_ASM("asm/nonmatchings/game/MonsterStates", __tf20PunchSwipeConfigBase);
