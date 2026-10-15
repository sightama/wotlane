#include "common.h"
#include "game/game.h"
#include "game/monster_state.h"
#include "game/pad_flags.h"
#include "game/pickup.h"
#include "game/hit_event.h"
#include "game/ai.h"
#include "hd.h"
#include "vecmath.h"
#include "game/sound.h"
#include "game/anim_blend.h"
#include "game/hit_history.h"

/* Movement states: a state's tunables are blocks inside the object ("configs"); jumping and flying pick the heavy block while holding a monster or a two-handed pickup. */
/* Common base of jumping and flying: tracks the height the monster left from and the highest point reached. */
class JumpFlyBase : public MonsterState {
public:
    enum SubState { SUB_0, SUB_1, SUB_2, SUB_3, SUB_4, SUB_5, SUB_6, SUB_7, SUB_8, SUB_9 };
    enum FootPref { FOOT_0, FOOT_1, FOOT_ANY };

    int heavy;      /* 0x14: holding a monster or a two-handed pickup (heavy config in use) */
    int subState;   /* 0x18 */
    int footPref;   /* 0x1C: 2 while heavy and after the state */
    int f20;
    float startZ;   /* 0x24: height when the state began */
    float apexZ;    /* 0x28: highest point reached */

    void transitionInto(void);
    void update(void);
};
/* Flight (Monster+0xF0B0). Two configs (0x38 bytes each): light at 0x44, heavy at 0x7C. */
class StateFly : public JumpFlyBase {
public:
    char pad2C[0x44 - 0x2C];
    char lightConfig[0x7C - 0x44]; /* 0x44 */
    char heavyConfig[0xB4 - 0x7C]; /* 0x7C; +0x10: stamina needed to flap */
    char padB4[0x100 - 0xB4];
    char *config;                  /* 0x100: lightConfig or heavyConfig */
    char pad104[0x108 - 0x104];
    AnimBlend blend108;            /* 0x108 */
    AnimBlend blend170;            /* 0x170 */
    AnimBlend blendWings[3];       /* 0x1D8: animations 0x24, 0x25, 0x26 */
    AnimBlend *glide;              /* 0x310: additive glide layer (animation 0x6F) from AnimBlendPool */
    char pad314[0x31C - 0x314];
    int savedMonster130;           /* 0x31C: Monster+0x130 saved on entry, restored on exit */
    char pad320[0x340 - 0x320];

    void init(Monster *m);
    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void enterSubState(SubState s);
    void handleCollis(_hdResult &r);
    void handlePreemption(MonsterState *next);
    char *getRelevantConfig(void);
    float getJumpHeightGain(void);
    float getFlapHeightGain(void);
    float getFlapStaminaDrain(void);
};
typedef char _size_StateFly[sizeof(StateFly) == 0x340 ? 1 : -1];
/* Jump (Monster+0x7AD4). Two configs (0x30 bytes each): light at 0x38, heavy at 0x68. */
class StateJump : public JumpFlyBase {
public:
    char pad2C[0x38 - 0x2C];
    char lightConfig[0x68 - 0x38]; /* 0x38 */
    char heavyConfig[0x98 - 0x68]; /* 0x68 */
    char pad98[0x9C - 0x98];
    AnimBlend blend9C;             /* 0x9C */
    AnimBlend blend104;            /* 0x104 */
    AnimBlend blendWings[3];       /* 0x16C: animations 0x24, 0x25, 0x26 */
    char pad2A4[0x2A8 - 0x2A4];
    char *config;                  /* 0x2A8: lightConfig or heavyConfig */
    char pad2AC[0x2CC - 0x2AC];

    void init(Monster *m);
    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void enterSubState(SubState s);
    void handleCollis(_hdResult &r);
    void handlePreemption(MonsterState *next);
    char *getRelevantConfig(void);
    float getJumpHeightGain(void);
};
typedef char _size_StateJump[sizeof(StateJump) == 0x2CC ? 1 : -1];
/* Ram attack (Monster+0xFD40): a charge on all fours. */
class StateRamAttack : public MonsterState {
public:
    /* charge (animation 0x10A, or 0x109 kept when missing), coasting, impact (0x10B), done */
    enum SubState { SUB_CHARGE, SUB_COAST, SUB_IMPACT, SUB_DONE };

    int unk14, unk18;      /* AttackState */
    float blendTime;       /* 0x1C */
    float f20;
    float accel;           /* 0x24 */
    float footF28;         /* 0x28 */
    float airDeccel;       /* 0x2C */
    float maxSpeedMPH;     /* 0x30 */
    float maxSpeed;        /* 0x34: charge speed (75% of it puts out a fire) */
    float duration;        /* 0x38: frames of the charge */
    float impactBlend;     /* 0x3C */
    float f40;
    int f44;
    float staminaCost;     /* 0x48 */
    float f4C, f50, f54, f58;
    int subState;          /* 0x5C */
    int anim;              /* 0x60: MonsterAnim playing */
    char hits[0xB0 - 0x64]; /* 0x64: HitHistory of the monsters already rammed */
    int fx[3];             /* 0xB0: particle effects of the charge (dust trail, two side sprays) */
    char padBC[0xC0 - 0xBC];
    FVECTOR trailPos;      /* 0xC0: under the monster's centre */
    FVECTOR sprayPos;      /* 0xD0: between the two locators at Monster+0x3DE0/+0x3DF0, 20 up */

    int transitionOK(void);
    void transitionInto(void);
    void update(void);
    void enterSubState(SubState s);
    void updateFxPos(void)
    {
        Monster *m = owner;
        float *a, *b;

        trailPos.x = m->m_cs->trans.x;
        trailPos.y = m->m_cs->trans.y;
        trailPos.z = m->m_cs->trans.z + m->m_heightAboveCOG * 0.5f;
        a = (float *)((char *)owner + 0x3DE0);
        b = (float *)((char *)owner + 0x3DF0);
        sprayPos.x = a[0] * 0.5f + b[0] * 0.5f;
        sprayPos.y = a[1] * 0.5f + b[1] * 0.5f;
        sprayPos.z = 20.0f + a[2] * 0.5f + b[2] * 0.5f;
    }
    int acceptHit(HitEvent &e);
    void setMaxSpeedMPH(float mph);
    void handlePreemption(MonsterState *next);
};
typedef char _size_StateRamAttack[sizeof(StateRamAttack) == 0xE0 ? 1 : -1];
/* miles per hour to game units per second (feet) */
#define MPH(x) ((x) * (5280.0f / 3600.0f))
/* One block of run tuning (StateRun::init sets them per monster type). */
struct RunConfig {
    float speed;     /* 0x00: run speed; the run animation speed is the ground speed over it */
    float walkSpeed; /* 0x04: the same for the walk animation */
    float f8, fC, f10, f14, f18, f1C;
};
/* Running (Monster+0x79AC). Two configs: normal at 0x14 and heavy at 0x34. The run animation (runAnim) is chosen on
 * entry: walk 1 (with 7) when slow, run 2 (with 8), a landing run 6 (with 9) after a fall with momentum, or the heavy
 * run 0xAD (with 0xB0). */
class StateRun : public MonsterState {
public:
    enum SubState { SUB_0, SUB_1, SUB_2, SUB_3, SUB_4 };

    RunConfig normal;               /* 0x14 */
    RunConfig heavy;                /* 0x34: holding something heavy */
    float f54;
    float walkSpeed;                /* 0x58: below this speed the monster walks */
    float f5C;
    int anim;                       /* 0x60: MonsterAnim playing */
    int runAnim;                    /* 0x64 */
    int altAnim;                    /* 0x68 */
    AnimBlend blend;                /* 0x6C: running blend layer, ramped in on entry at the old percent */
    int fD4;
    RunConfig *config;              /* 0xD8: normal or heavy */
    float landMomentum;             /* 0xDC: set when landing from a jump; >= 350 starts the landing run */
    int fE0;

    void init(Monster *m);
    int transitionOK(void);
    void transitionInto(void);
    void enterSubState(SubState s);
    void handlePreemption(MonsterState *next);
    float getAnimSpeed(float speed);
};
typedef char _size_StateRun[sizeof(StateRun) == 0xE4 ? 1 : -1];
/* Dash (Monster+0x7A90): a burst of speed while the dash button is held. Sub-states: 0 start (animation 0x108),
 * 1 dashing (0x109, drains stamina every frame), 2 slowing down back to idle. */
class StateDash : public MonsterState {
public:
    enum SubState { SUB_START = 0, SUB_DASH = 1, SUB_STOP = 2 };

    float blendTime;       /* 0x14 */
    float startFrames;     /* 0x18: frames of the start before dashing */
    float f1C;
    float staminaPerFrame; /* 0x20: drained each dashing frame; the dash stops when the stamina is not above it */
    float staminaCost;     /* 0x24: paid on entry */
    float dashSpeed;       /* 0x28: foot motion speed while dashing (75% of it puts out a fire) */
    float footAccel;       /* 0x2C */
    float footF30;         /* 0x30 */
    float airAccel;        /* 0x34: air control while slowing down off the ground */
    float f38;
    int subState;          /* 0x3C */
    int wantRam;           /* 0x40: the ram attack became possible during the dash */

    int transitionOK(void);
    void transitionInto(void);
    void update(void);
    void enterSubState(SubState s);
    void handleCollis(_hdResult &r);
    void handlePreemption(MonsterState *next);
};
typedef char _size_StateDash[sizeof(StateDash) == 0x44 ? 1 : -1];
/* Climbing a wall (Monster+0xEFE0). checkWallContact probes six spheres around the monster and sets contactFlags:
 * 2 and 4 the two hand probes touch a climbable wall, 8 the feet are on the ground, 0x10 and 0x20 the two upper probes
 * touch (both = the wall goes on above, the monster can climb rather than mount the ledge). */
class StateClimb : public MonsterState {
public:
    enum SubState { SUB_HANG = 0, SUB_CLIMB = 1, SUB_MOUNT = 3, SUB_DISMOUNT = 4 };

    float blendHang;       /* 0x14: animation 0x6A */
    float blendClimb;      /* 0x18: animation 0x64 */
    float f1C, f20;
    float blendMount;      /* 0x24: animation 0x69 */
    float speedHang;       /* 0x28 */
    float speedClimb;      /* 0x2C */
    float f30, f34;
    float speedMount;      /* 0x38 */
    float probeLow;        /* 0x3C, 0x40: their sum is the height of the upper probes */
    float probeHigh;
    int f44;
    float f48;
    char pad4C[0x50 - 0x4C];
    float halfWidth;       /* 0x50: init: half the monster's width */
    float reach;           /* 0x54 */
    float f58;             /* 0x58: init: Monster+0x2D8 */
    char pad5C[0x60 - 0x5C];
    FVECTOR huckVelocity;  /* 0x60 */
    int f70;
    int reverse;           /* 0x74: the climb animation runs backwards */
    int f78;
    int subState;          /* 0x7C */
    int anim;              /* 0x80: MonsterAnim playing */
    char pad84[0x90 - 0x84];
    FVECTOR motion;        /* 0x90: climb motion quaternion */
    int contactFlags;      /* 0xA0 */
    int contactSurface;    /* 0xA4, 0xA8: bytes 2 and 3 of the wall contact */
    int contactKind;
    char padAC[0xB0 - 0xAC];
    FVECTOR contactNormal; /* 0xB0 */
    int contactInteractive; /* 0xC0 */
    float contactDist;     /* 0xC4: distance to the wall */

    void init(Monster *m);
    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void handlePreemption(MonsterState *next);
    void enterSubState(SubState s);
    void checkWallContact(void);
    _hdResult *getMinContact(_hdResult *r, int n, bool facing);
};
typedef char _size_StateClimb[sizeof(StateClimb) == 0xD0 ? 1 : -1];
#define MSOUND(m) ((MonsterSound *)(m)->m_sound)
void particleKillFx(int &handle);
class Interactives {
public:
    static int *getInteractive(int i); /* first word: kind (1 monster, 3 ...) */
};
class StateGrappled : public MonsterState {
public:
    void detach(void);
};
class StateGrapple : public MonsterState {
public:
    void detach(void);
};
class StateBlock : public MonsterState {
public:
    int transitionOK(void);
};
class StateCounter : public MonsterState {
public:
    int transitionOK(void);
};
#define ST_BLOCK 0x7DA0
#define ST_COUNTER 0x7DD8
#define ST_GRAPPLE 0xDCE0
#define ST_GRAPPLED 0xDDCC
#define ST_BEING_SLAMMED 0xDE30
#define ST_RAM 0xFD40
#define STATE_AT(m, off) ((MonsterState *)((char *)(m) + (off)))
/* virtual int call on a state (vtable entries {this delta, function}); slot 0x20 is transitionOK */
#define VCALL_INT(st, slot) \
    (((int (*)(void *))*(void **)((char *)(st)->vptr + (slot) + 4))((char *)(st) + *(short *)((char *)(st)->vptr + (slot))))
#define VT_TRANSITION_OK 0x20
#define ST_CLIMB 0xEFE0
#define STATE_ID_CLIMB 0x1F             /* MonsterState::id of StateClimb */
#define MONSTER_TYPE_ROBOT 0x40         /* Monster::m_typeBits of Robo-47 */
#define MONSTER_TYPE_ASSBOSS 0x1A0
/* holding a two-handed pickup (Pickup::bits bit 1) */
#define TWO_HANDED(m) ((int)((*(Pickup **)(m)->m_pickup)->bits >> 1) & 1)
extern float s_impactVel __asm__("s_impactVel.2590"); /* StateFly::handleCollis: 350 */
__asm__("#SNFIX_SMALL s_impactVel.2590");
/* the two spray angles of StateRamAttack (rodata of enterSubState) */
struct Mat44 {
    float m[4][4];
};
extern const Mat44 ramSprayLeft __asm__("D_006ED4B0");
extern const Mat44 ramSprayRight __asm__("D_006ED4F0");
/* Virtual transitionFeasible(): vtable slot {delta @0x28, function @0x2C}. */
#define VFEASIBLE(st) (((int (*)(void *))*(void **)((char *)(st)->vptr + 0x2C))((char *)(st) + *(short *)((char *)(st)->vptr + 0x28)))
#define PAD(m, n) ((char *)(*(PadFlags *)((char *)(m) + 0x5040))[n])
#define SCFG(p, o) (*(float *)((char *)(p) + (o)))

class MonsterDynamics {
public:
    void updateTurn(bool b);
    void updateMove(bool b);
    void allStop(void);
    void setMotionVec(_fvector &v);
    float *getConfig(void); /* [3]: the base foot speed */
    float getSpeedModifier(void);
    void updateFootMotion(float speed, float accel, float f);
    void updateAirMotion(float speed, float accel);
};
void animationSetToEnd(_animHandle h, bool update);
void animationSetDirection(_animHandle h, bool forward);
extern "C" float fabsf(float);
extern float UNIT_MATRIX[4][4];
/* function-static tunables of StateClimb (sdata) */
extern float clingSlack __asm__("clingSlack.2496");     /* transitionFeasible: 30 */
__asm__("#SNFIX_SMALL clingSlack.2496");
extern float FACE_ANGLE_TOL __asm__("FACE_ANGLE_TOL.2517"); /* getMinContact: 0.7071 */
__asm__("#SNFIX_SMALL FACE_ANGLE_TOL.2517");
extern float TURN_ANGLE_TOL __asm__("TURN_ANGLE_TOL.2518"); /* getMinContact: -0.1 */
__asm__("#SNFIX_SMALL TURN_ANGLE_TOL.2518");
#define DYN(m) ((MonsterDynamics *)((char *)(m) + 0x100))
/* MonsterDynamics velocity (Monster+0x1E0) */
#define MVEL(m) ((_fvector *)((char *)(m) + 0x1E0))
/* Monster's own vtable sits at +0x10 like the states'; slot 0x20 is getVel */
#define MONSTER_VEL(m) \
    ((float *)((void *(*)(void *))*(void **)(*(char **)((char *)(m) + 0x10) + 0x24))((char *)(m) + *(short *)(*(char **)((char *)(m) + 0x10) + 0x20)))
#define ST_IDLE 0x7984
#define ANIM_BUTTSLAM 0x105      /* the jump into the slam */
#define ANIM_BUTTSLAM_FALL 0x106 /* the drop */
#define POV_BUTTSLAM 6           /* camera POV a butt slam switches to when the camera looks at the monster's front */
#define PLAYER_HUMAN 1           /* Monster::m_playerNum */
#define PLAYER_AI 2

/* Cameras::m_cameras: one block per view. */
struct CameraView {
    char pad0[0xCF0];
    _cs *cs;          /* 0xCF0 */
    Monster *follows; /* 0xCF4 */
    int pov;          /* 0xCF8 */
    char padCFC[0xEB0 - 0xCFC];
};
typedef char _size_CameraView[sizeof(CameraView) == 0xEB0 ? 1 : -1];
extern CameraView cameraViews[] __asm__("_7Cameras$m_cameras");

/* Butt slam (Monster+0xFA1C): from a jump or a flight, the monster drops onto what is below. Animation 0x105 is the jump
 * into the slam (velocity cancelled, then a downward push at boostPercent), 0x106 the drop itself; handleCollis lands it. */
class StateButtSlam : public MonsterState {
public:
    int unk14, unk18;      /* AttackState */
    float maxRiseSpeed;    /* 0x1C: not while still rising faster than this */
    float maxDropFromApex; /* 0x20: not once fallen this far below the jump's apex */
    float minHeight;       /* 0x24: or, if lower than this above the ground shadow, only over a hat id in [0x20, 0x400] */
    float runFrames;       /* 0x28 */
    float blendTime;       /* 0x2C */
    float boostPercent;    /* 0x30: point of animation 0x105 where the downward push comes */
    float boostDrop;       /* 0x34: the push (subtracted from vel.z) */
    float f38;
    float launchPercent;   /* 0x3C: point of the drop animation where the monster is launched */
    float launchUp;        /* 0x40: launch velocity added upward */
    float launchBack;      /* 0x44: and backward along the heading */
    float f48;
    float staminaCost;     /* 0x4C */
    char pad50[0x64 - 0x50];
    _cs *shockwave;        /* 0x64: hidden when the slam is preempted */
    char pad68[0x30C - 0x68];
    int anim;              /* 0x30C: MonsterAnim playing */
    int boostPending;      /* 0x310 */
    int launched;          /* 0x314 */

    int transitionOK(void);
    int transitionFeasible(void);
    void transitionInto(void);
    void update(void);
    int acceptHit(HitEvent &e);
    void handlePreemption(MonsterState *next);
};

INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __13StateButtSlam);
/* Not for the assboss; needs attacks enabled and both slam animations. An AI monster does not slam while its brain sits
 * in one of two of its embedded states. Then the slam button (pad field 0x44) and transitionFeasible. */
#ifdef NON_MATCHING
/* untuned: 32/47 words; retail returns through one shared exit for the brain-state checks */
int StateButtSlam::transitionOK(void)
{
    Monster *m = owner;

    if (m->m_typeBits == MONSTER_TYPE_ASSBOSS)
        return 0;
    if (!m->m_attacksEnabled)
        return 0;
    if (m->m_anims[ANIM_BUTTSLAM].ci == 0)
        return 0;
    if (m->m_anims[ANIM_BUTTSLAM_FALL].ci == 0)
        return 0;
    if (m->m_playerNum == PLAYER_AI) {
        char *brain = (char *)((Ai *)m->m_ai)->m_gate;

        if (*(char **)brain == brain + 0x4C8)
            return 0;
        if (*(char **)brain == brain + 0x524)
            return 0;
    }
    if (owner->m_padFlags[0]->f44 == 0)
        return 0;
    if (VFEASIBLE(this) == 0)
        return 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__13StateButtSlam);
#endif
/* Only from the air state (Monster::m_stateRef), with the stamina for it, hands free of anything heavy, not rising
 * faster than maxRiseSpeed and not more than maxDropFromApex below the apex; high enough, or over a slammable hat. */
#ifdef NON_MATCHING
/* untuned: 63/80 words; retail keeps a blocked flag in s1 and reads the hat id with ld (64-bit bitfield) */
int StateButtSlam::transitionFeasible(void)
{
    int blocked = 0;

    if (owner->m_state != owner->m_stateRef) {
        blocked = 1;
    } else if (!owner->m_stamina.hasEnough(staminaCost)) {
        blocked = 1;
    } else if (owner->m_target != 0) {
        blocked = 1;
    } else if (owner->m_pickup != 0 && ((int)((*(Pickup **)owner->m_pickup)->bits >> 1) & 1)) {
        blocked = 1;
    } else if (maxRiseSpeed < MONSTER_VEL(owner)[2]) {
        blocked = 1;
    } else if (maxDropFromApex < *(float *)((char *)owner->m_stateRef + 0x28) - owner->m_cs->trans.z) {
        blocked = 1;
    }
    if (blocked)
        return 0;
    if (owner->m_cs->trans.z - owner->m_shadow->trans.z < minHeight) {
        unsigned hat = (unsigned)(*(unsigned long long *)((char *)owner + 0x1A40) >> 43) & 0x3FFF; /* foot probe _hdResult: hat id */

        if (hat < 0x20)
            return 0;
        return hat <= 0x400;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionFeasible__13StateButtSlam);
#endif
/* Starts animation 0x105 and pays the stamina. A human player's camera that looks at the monster's front switches to
 * the slam POV (not in game mode 9). */
#ifdef NON_MATCHING
/* untuned: 49/96 words; retail zeroes a whole quadword for the camera direction and copies it once */
void StateButtSlam::transitionInto(void)
{
    anim = ANIM_BUTTSLAM;
    frames = 0;
    animationSetTotalRunFrames(owner->m_anims[ANIM_BUTTSLAM], runFrames);
    animationTransitionInto(owner->m_anims[anim], blendTime, 1, 1);
    owner->m_stamina.drain(staminaCost, true, false);
    boostPending = 1;
    launched = 0;
    if (game->m_gameMode != 9 && owner->m_playerNum == PLAYER_HUMAN) {
        CameraView *cam = &cameraViews[owner->m_cameraView];

        if (cam->pov != 1) {
            FVECTOR d;

            d.x = cam->cs->trans.x - owner->m_cs->trans.x;
            d.y = cam->cs->trans.y - owner->m_cs->trans.y;
            d.z = 0.0f;
            if (vecDot(&d, (_fvector *)&owner->m_cs->mat[1]) < 0.0f)
                Cameras::SetCameraPOV(owner->m_cameraView, (Camera::CameraPOV)POV_BUTTSLAM);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionInto__13StateButtSlam);
#endif
/* Animation 0x105: the velocity is held at zero until boostPercent, where the downward push is applied once. The drop
 * (another anim) launches the monster back and up once at launchPercent and returns to idle near its end. */
#ifdef NON_MATCHING
/* untuned: 9/106 words; retail adds 0x1CF0 to the anim index before the handle copy */
void StateButtSlam::update(void)
{
    float pct;

    MonsterState::update();
    pct = animationGetCurrentPercent(owner->m_anims[anim]);
    if (anim == ANIM_BUTTSLAM) {
        if (boostPending) {
            _fvector *v = MVEL(owner);

            v->x = 0.0f;
            v->z = 0.0f;
            v->y = 0.0f;
            if (boostPercent <= pct) {
                MVEL(owner)->z -= boostDrop;
                boostPending = 0;
            }
        }
        DYN(owner)->updateTurn(true);
        DYN(owner)->updateMove(false);
        return;
    }
    if (!launched && launchPercent <= pct) {
        Monster *m = owner;
        FVECTOR push;

        vecScale(&push, (_fvector *)&m->m_cs->mat[1], -launchBack);
        push.z += launchUp;
        vecAdd(MVEL(m), MVEL(m), &push);
        launched = 1;
    } else if (0.99f <= pct) {
        owner->enterNewState((MonsterState *)((char *)owner + ST_IDLE));
    }
    DYN(owner)->updateTurn(false);
    DYN(owner)->updateMove(false);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__13StateButtSlam);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handleCollis__13StateButtSlamR9_hdResult);
/* Shocks (source 5) always get through; a hit from sources 2..4 of kind 0, 2 or 5, or a melee hit (0xB), only with
 * some strength. */
int StateButtSlam::acceptHit(HitEvent &e)
{
    int ok = 0;

    if ((unsigned)(e.source - 2) < 3 && (e.sourceKind == 0 || e.sourceKind == 2 || e.sourceKind == 5)
        && (e.strength34 != 0.0f || e.strength38 != 0.0f))
        ok = 1;
    else if (e.source == 5)
        ok = 1;
    else if (e.source == 0xB && (e.strength34 != 0.0f || e.strength38 != 0.0f))
        ok = 1;
    return ok;
}
/* The camera leaves the slam POV and the shockwave is hidden. */
#ifdef NON_MATCHING
/* untuned: 23/30 words; operand order of the camera-slot address */
void StateButtSlam::handlePreemption(MonsterState *next)
{
    if (cameraViews[owner->m_cameraView].pov == POV_BUTTSLAM) {
        Cameras::SetCameraPOV(owner->m_cameraView, (Camera::CameraPOV)0);
        Cameras::ResetPOVChange(owner->m_cameraView, 20.0f);
    }
    if (shockwave != 0)
        shockwave->drawMe = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handlePreemption__13StateButtSlamP12MonsterState);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __10StateClimb);
#ifdef NON_MATCHING
/* untuned: 2/13 words; retail loads Monster+0x2D8 before the multiply and stores through this+0x50 */
void StateClimb::init(Monster *m)
{
    owner = m;
    halfWidth = m->m_width * 0.5f;
    f58 = *(float *)((char *)m + 0x2D8);
    reach = 80.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", init__10StateClimbP7Monster);
#endif
#ifdef NON_MATCHING
/* Climbing starts on a fresh press of the climb button (pad field 0x2E set now, clear one frame ago). */
int StateClimb::transitionOK(void)
{
    if (*(unsigned short *)(PAD(owner, 0) + 0x2E) != 0) {
        if (*(unsigned short *)(PAD(owner, 1) + 0x2E) != 0)
            return 0;
        return VFEASIBLE(this) != 0;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__10StateClimb);
#endif
/* Needs the five climb animations and free hands. The wall must be within the monster's reach (Monster+0x2BC) plus
 * clingSlack and touch both hand probes; then either the wall goes on above, or the monster is in the air at least 50
 * above its ground shadow. */
#ifdef NON_MATCHING
/* untuned: 52/63 words; register choice for owner across the calls */
int StateClimb::transitionFeasible(void)
{
    Monster *m;

    contactFlags = 0;
    m = owner;
    if (m->m_anims[0x65].ci == 0 || m->m_anims[0x67].ci == 0 || m->m_anims[0x69].ci == 0 || m->m_anims[0x6A].ci == 0
        || m->m_anims[0x64].ci == 0)
        return 0;
    if (m->isHolding())
        return 0;
    checkWallContact();
    m = owner;
    if (*(float *)((char *)m + 0x2BC) + clingSlack < contactDist)
        return 0;
    if ((contactFlags & 6) != 6)
        return 0;
    if ((contactFlags & 0x30) == 0x30)
        return 1;
    if (contactFlags & 8)
        return 0;
    if (m->m_cs->trans.z - m->m_shadow->trans.z < 50.0f)
        return 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionFeasible__10StateClimb);
#endif
/* Without wall above the monster goes straight to the ledge (dismount); from the air state, off the ground, it climbs
 * (animation reversed); otherwise it hangs. The dynamics are stopped and the monster becomes a valid target. */
#ifdef NON_MATCHING
/* untuned: 18/71 words; retail clears the quaternion with sqc2 $vf0 and adds 0x1CF0 before the handle copy */
void StateClimb::transitionInto(void)
{
    frames = 0;
    if ((contactFlags & 0x30) != 0x30) {
        enterSubState(SUB_DISMOUNT);
    } else if (owner->m_state == owner->m_stateRef && !(contactFlags & 8)) {
        enterSubState(SUB_CLIMB);
        reverse = 1;
        animationSetDirection(owner->m_anims[anim], true);
    } else {
        enterSubState(SUB_HANG);
    }
    owner->m_turning = 0;
    DYN(owner)->allStop();
    owner->m_unk1B8 = 0.0f;
    DYN(owner)->setMotionVec(*(_fvector *)UNIT_MATRIX[1]);
    motion.x = 0.0f;
    motion.y = 0.0f;
    motion.z = 0.0f;
    motion.w = 1.0f;
    owner->m_unk49 = 1;
    f78 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionInto__10StateClimb);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__10StateClimb);
#ifdef NON_MATCHING
void StateClimb::handlePreemption(MonsterState *next)
{
    float *q = (float *)((char *)this + 0x90);

    *(float *)((char *)owner + 0x1B8) = 1.0f;
    *(unsigned char *)((char *)owner + 0x280) = 1; /* retail: sb (a word store would zero owner+0x281..0x283) */
    q[0] = 0.0f;
    q[1] = 0.0f;
    q[2] = 0.0f;
    q[3] = 1.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handlePreemption__10StateClimbP12MonsterState);
#endif
/* Each sub-state has its animation: hang 0x6A, climb 0x64, mount 0x69 forward, dismount 0x69 from its end backwards. */
#ifdef NON_MATCHING
/* untuned: 79/194 words; retail adds 0x1CF0 to the anim index before each handle copy */
void StateClimb::enterSubState(SubState s)
{
    switch (s) {
    case SUB_CLIMB:
        anim = 0x64;
        animationSetSpeed(owner->m_anims[0x64], speedClimb);
        animationTransitionInto(owner->m_anims[anim], blendClimb, 1, 1);
        break;
    case SUB_HANG:
        anim = 0x6A;
        animationSetSpeed(owner->m_anims[0x6A], speedHang);
        animationTransitionInto(owner->m_anims[anim], blendHang, 1, 1);
        frames = 0;
        break;
    case SUB_MOUNT:
        anim = 0x69;
        animationSetToBeginning(owner->m_anims[0x69], false);
        animationSetDirection(owner->m_anims[anim], true);
        animationSetSpeed(owner->m_anims[anim], speedMount);
        animationTransitionInto(owner->m_anims[anim], blendMount, 1, 7);
        break;
    case SUB_DISMOUNT:
        owner->m_unk1B8 = 1.0f;
        anim = 0x69;
        animationSetSpeed(owner->m_anims[0x69], speedMount);
        animationSetToEnd(owner->m_anims[anim], false);
        animationSetDirection(owner->m_anims[anim], false);
        animationTransitionInto(owner->m_anims[anim], blendMount, 1, 1);
        break;
    }
    if (subState == SUB_DISMOUNT)
        owner->m_unk1B8 = 0.0f;
    subState = s;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", enterSubState__10StateClimbQ210StateClimb8SubState);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", checkWallContact__10StateClimb);
/* The nearest (smallest point.w) of `n` probe results that hit a wall surface (kind byte 2 or 10). With `facing`, only
 * walls not too flat (|normal.z| <= FACE_ANGLE_TOL) that the monster faces (normal . heading <= TURN_ANGLE_TOL). */
#ifdef NON_MATCHING
/* untuned: 35/56 words; retail walks a pointer at r+0x20 and keeps the result pointer in t0 */
_hdResult *StateClimb::getMinContact(_hdResult *r, int n, bool facing)
{
    _hdResult *best = 0;
    float minDist = 1.1f;

    for (; n != 0; n--, r++) {
        int kind = ((unsigned char *)r)[3];
        int wall = 0;

        if (kind == 2 || kind == 10)
            wall = 1;
        if (!wall)
            continue;
        if (facing) {
            if (FACE_ANGLE_TOL < fabsf(r->normal.z))
                continue;
            if (-TURN_ANGLE_TOL < vecDot((_fvector *)&r->normal, (_fvector *)owner->m_cs->mat[1]))
                continue;
        }
        if (r->point.w < minDist) {
            minDist = r->point.w;
            best = r;
        }
    }
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getMinContact__10StateClimbP9_hdResultib);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __9StateDash);
#ifdef NON_MATCHING
/* Dashing needs the dash ability (0x2D80), the dash button held, enough stamina (this+0x24 <= owner+0x470) and nothing heavy in hand. */
int StateDash::transitionOK(void)
{
    char *m = (char *)owner;

    if (*(int *)(m + 0x2D80) == 0)
        return 0;
    if (*(short *)(PAD(m, 0) + 0x3E) == 0)
        return 0;
    if (!(*(float *)((char *)this + 0x24) <= *(float *)(m + 0x470)))
        return 0;
    if (*(int *)(m + 0x68B4) != 0)
        return 0;
    if (*(int *)(m + 0x68A4) == 0 || ((**(Pickup ***)(m + 0x68A4))->bits >> 1 & 1) == 0)
        return 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__9StateDash);
#endif
/* In game mode 8 the dash gets state flag 0x20. Pays the stamina and starts. */
void StateDash::transitionInto(void)
{
    frames = 0;
    if (game->m_gameMode == 8)
        flags |= 0x20;
    enterSubState(SUB_START);
    owner->m_stamina.drain(staminaCost, true, false);
    *(int *)((char *)owner + 0x484) = 0;
    wantRam = 0;
}
/* Start: after startFrames the dash proper. Dashing: while the dash button is held and the stamina lasts, run at
 * dashSpeed (paying staminaPerFrame each frame); then slow down until the speed is back near the normal foot speed
 * and go idle. Dashing fast enough puts out a fire. The ram attack, a block, a counter or the air state can take over;
 * a ram only after the start. */
#ifdef NON_MATCHING
/* untuned: 76/210 words; s-register allocation (this in s0, dynamics in s1) and the speed check order */
void StateDash::update(void)
{
    MonsterDynamics *dyn;
    float speed;

    MonsterState::update();
    DYN(owner)->updateTurn(true);
    owner->updateLock((MonsterReticleState)1);
    dyn = DYN(owner);
    dyn->setMotionVec(*(_fvector *)UNIT_MATRIX[1]);
    switch (subState) {
    case SUB_START:
        if (startFrames < (float)frames)
            enterSubState(SUB_DASH);
        DYN(owner)->updateMove(false);
        break;
    case SUB_DASH:
        MSOUND(owner)->updateDashSound();
        if (owner->m_padFlags[0]->f3E != 0 && staminaPerFrame < owner->m_stamina.cur) {
            owner->m_stamina.drain(staminaPerFrame, true, false);
            dyn->updateFootMotion(dashSpeed, footAccel, footF30);
            break;
        }
        enterSubState(SUB_STOP);
        break;
    case SUB_STOP:
        if (owner->m_freeFalling)
            dyn->updateAirMotion(dyn->getConfig()[3], airAccel);
        dyn->updateFootMotion(dyn->getConfig()[3], footAccel, footF30);
        speed = *(float *)((char *)dyn + 0x154);
        if (speed <= dyn->getConfig()[3] * dyn->getSpeedModifier() + 10.0f)
            owner->enterNewState(STATE_AT(owner, ST_IDLE));
        MSOUND(owner)->terminateDashSound();
        break;
    }
    if (owner->m_onFireCount > 0.0f && dashSpeed * 0.75f < owner->m_speed)
        owner->m_onFireCount = 0.0f;
    if (((StateRamAttack *)STATE_AT(owner, ST_RAM))->transitionOK())
        wantRam = 1;
    if (subState != SUB_START && wantRam)
        owner->enterNewState(STATE_AT(owner, ST_RAM));
    else if (((StateBlock *)STATE_AT(owner, ST_BLOCK))->transitionOK())
        owner->enterNewState(STATE_AT(owner, ST_BLOCK));
    else if (((StateCounter *)STATE_AT(owner, ST_COUNTER))->transitionOK())
        owner->enterNewState(STATE_AT(owner, ST_COUNTER));
    else if (!owner->m_freeFalling && VCALL_INT((MonsterState *)owner->m_stateRef, VT_TRANSITION_OK))
        owner->enterNewState((MonsterState *)owner->m_stateRef);
    if (owner->m_state != (int *)this)
        owner->m_padFlags[0]->f3E = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__9StateDash);
#endif
void StateDash::enterSubState(SubState s)
{
    switch (s) {
    case SUB_START:
        animationTransitionInto(owner->m_anims[0x108], blendTime, 1, 1);
        break;
    case SUB_DASH:
        animationTransitionInto(owner->m_anims[0x109], blendTime, 1, 1);
        break;
    case SUB_STOP:
        break;
    }
    subState = s;
}
/* While dashing, running head-on into something (normal against the heading by more than 45 degrees) that is not the
 * grappled monster: the body impact sound (the other monster's when it is one), a held monster is slammed out of the
 * grapple, and the dash stops unless the obstacle is a kind-3 interactive with +0x14 <= 10. */
#ifdef NON_MATCHING
/* untuned: 26/101 words; retail reads the hit ids with ld (64-bit bitfield) and keeps this in s1 */
void StateDash::handleCollis(_hdResult &r)
{
    Monster *m;
    Monster *held;
    int keepGoing;

    MonsterState::handleCollis(r);
    if (subState != SUB_DASH)
        return;
    m = owner;
    if (-0.7071f < vecDot((_fvector *)&r.normal, (_fvector *)m->m_cs->mat[1]))
        return;
    held = (Monster *)m->m_target;
    if (held != 0 && held->m_id == (r.ids & 0x7FF))
        return;
    keepGoing = 0;
    if (r.ids & 0x7FF) {
        int *hit = Interactives::getInteractive(r.ids & 0x7FF);

        if (*hit == 3 && *(float *)((char *)hit + 0x14) <= 10.0f)
            keepGoing = 1;
        if (*hit == 1)
            MSOUND((Monster *)hit)->playBodyImpactSound();
        else
            MSOUND(owner)->playBodyImpactSound();
    }
    held = (Monster *)owner->m_target;
    if (held != 0) {
        MonsterState *slam = STATE_AT(held, ST_BEING_SLAMMED);

        *(float *)((char *)slam + 0x48) = 40.0f;
        held->enterNewState(slam);
        ((StateGrappled *)STATE_AT(held, ST_GRAPPLED))->detach();
        ((StateGrapple *)STATE_AT(owner, ST_GRAPPLE))->detach();
    }
    if (!keepGoing)
        enterSubState(SUB_STOP);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handleCollis__9StateDashR9_hdResult);
#endif
void StateDash::handlePreemption(MonsterState *next)
{
    ((MonsterSound *)owner->m_sound)->terminateDashSound();
    *(int *)((char *)owner + 0x484) = 1;
}
#ifdef NON_MATCHING
/* untuned: 5/7 words; store scheduling */
void JumpFlyBase::transitionInto(void)
{
    float z;

    frames = 0;
    z = owner->m_cs->trans.z;
    startZ = z;
    apexZ = z;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionInto__11JumpFlyBase);
#endif
#ifdef NON_MATCHING
/* untuned: 5/18 words; retail keeps the apex with max.s, here a compare and branch */
void JumpFlyBase::update(void)
{
    float z;

    MonsterState::update();
    z = owner->m_cs->trans.z;
    apexZ = z < apexZ ? apexZ : z;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__11JumpFlyBase);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handleStateTransitions__11JumpFlyBase);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __8StateFly);
void StateFly::init(Monster *m)
{
    owner = m;
    blendWings[0].init(m->m_anims[0x24]);
    blendWings[1].init(owner->m_anims[0x25]);
    blendWings[2].init(owner->m_anims[0x26]);
}
#ifdef NON_MATCHING
/* Height reached by the jump launch speed (config +0xC) against gravity: v^2 / (2g), g being negative. */
float StateFly::getJumpHeightGain(void)
{
    float v = SCFG(getRelevantConfig(), 0xC);

    return -(v * v) / (2.0f * game->m_gravity);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getJumpHeightGain__8StateFly);
#endif
#ifdef NON_MATCHING
float StateFly::getFlapHeightGain(void)
{
    float v = SCFG(getRelevantConfig(), 0x14);

    return -(v * v) / (2.0f * game->m_gravity);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getFlapHeightGain__8StateFly);
#endif
float StateFly::getFlapStaminaDrain(void)
{
    return SCFG(getRelevantConfig(), 0x10);
}
#ifdef NON_MATCHING
char *StateFly::getRelevantConfig(void)
{
    char *m = (char *)owner;

    if (*(int *)(m + 0x68B4) == 0) {
        int heavy = 0;

        if (*(int *)(m + 0x68A4) != 0)
            heavy = (int)((**(Pickup ***)(m + 0x68A4))->bits >> 1) & 1;
        if (heavy)
            return (char *)this + 0x7C;
        return (char *)this + 0x44;
    }
    return (char *)this + 0x7C;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getRelevantConfig__8StateFly);
#endif
#ifdef NON_MATCHING
/* Flying needs the monster able to fly (flags at 0x1ED0, 0x2400, 0x2410) and either no fly press / not feasible but enough stamina state (0x288 >= 6). */
int StateFly::transitionOK(void)
{
    char *m = (char *)owner;

    if (*(int *)(m + 0x1ED0) == 0 || *(int *)(m + 0x2400) == 0 || *(int *)(m + 0x2410) == 0)
        return 0;
    if (*(unsigned short *)(PAD(m, 0) + 0x2A) == 0 || VFEASIBLE(this) == 0)
        return *(int *)(m + 0x288) >= 6;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__8StateFly);
#endif
#ifdef NON_MATCHING
/* Jumping/flying is only possible while the pad entry one frame back has its field 0x2A at 0. */
int StateFly::transitionFeasible(void)
{
    return *(short *)((char *)(*(PadFlags *)((char *)owner + 0x5040))[1] + 0x2A) == 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionFeasible__8StateFly);
#endif
/* Picks the config, saves Monster+0x130. Off a climb's ledge mount it goes to sub-state 2; without a fresh press of
 * the fly button to 3; a fresh press in the air (not holding something large, not in game modes 7 and 8) flaps (4)
 * when the stamina allows, else 7; on the ground it takes off (1). A glide layer (animation 0x6F, additive, priority
 * 999, held at 50%) is taken from the AnimBlendPool. */
#ifdef NON_MATCHING
/* untuned: 61/152 words; retail computes the heavy test as a value first (inline) and the climb-state address differently */
void StateFly::transitionInto(void)
{
    AnimBlend *b;

    JumpFlyBase::transitionInto();
    if (owner->m_target != 0 || (owner->m_pickup != 0 && TWO_HANDED(owner))) {
        heavy = 1;
        config = heavyConfig;
        footPref = FOOT_ANY;
    } else {
        heavy = 0;
        config = lightConfig;
    }
    *(int *)((char *)owner + 0x240) = 0;
    subState = SUB_0;
    savedMonster130 = *(int *)((char *)owner + 0x130);
    if (*owner->m_prevState == STATE_ID_CLIMB && ((StateClimb *)STATE_AT(owner, ST_CLIMB))->subState == StateClimb::SUB_MOUNT) {
        enterSubState(SUB_2);
    } else if (owner->m_padFlags[0]->jump == 0 || owner->m_padFlags[1]->jump != 0) {
        enterSubState(SUB_3);
    } else {
        int canFlap = 0;

        if (!owner->isHoldingLarge() && game->m_gameMode != 8)
            canFlap = game->m_gameMode != 7;
        if (owner->m_freeFalling && canFlap) {
            if (SCFG(config, 0x10) <= owner->m_stamina.cur)
                enterSubState(SUB_4);
            else
                enterSubState(SUB_7);
        } else {
            enterSubState(SUB_1);
        }
    }
    b = AnimBlendPool::alloc();
    glide = b;
    if (b != 0) {
        b->init(owner->m_anims[0x6F]);
        glide->m_node.blendType = BLEND_ADDITIVE;
        glide->m_node.priority = 999;
        glide->setSpeed(0.0f);
        glide->setPercent(0.5f);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionInto__8StateFly);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__8StateFly);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", enterSubState__8StateFlyQ211JumpFlyBase8SubState);
/* Ignores interactives without flag 8 and other monsters. Landing on a floor (kind 7, or a normal steeper than 45
 * degrees up that is not kind 10/11) while not rising: a dive (7) lands (8) only above s_impactVel into the surface;
 * 3 and 6 land, 8 goes on to 9. A dive into a wall faster than s_impactVel bounces off (6). */
#ifdef NON_MATCHING
/* untuned: 8/117 words; retail reads the hit ids with ld (64-bit bitfield) and keeps r in s0 */
void StateFly::handleCollis(_hdResult &r)
{
    int kind;
    int floor;

    MonsterState::handleCollis(r);
    if (r.ids & 0x7FF) {
        int *hit = Interactives::getInteractive(r.ids & 0x7FF);

        if (!(*(unsigned short *)(hit + 1) & 8))
            return;
        if (*hit == 1)
            return;
    }
    kind = ((unsigned char *)&r)[3];
    floor = 0;
    if (kind == 7 || (0.7071f < r.normal.z && kind != 10 && kind != 11))
        floor = 1;
    if (floor) {
        _fvector *v = MVEL(owner);

        if (0.0f < v->z)
            return;
        if (subState == SUB_7) {
            if (s_impactVel < -vecDot(v, (_fvector *)&r.normal))
                enterSubState(SUB_8);
        } else if (subState == SUB_3 || subState == SUB_6) {
            enterSubState(SUB_8);
        } else if (subState == SUB_8) {
            enterSubState(SUB_9);
        }
    } else if (subState == SUB_7) {
        if (s_impactVel < -vecDot(MVEL(owner), (_fvector *)&r.normal))
            enterSubState(SUB_6);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handleCollis__8StateFlyR9_hdResult);
#endif
/* All layers ramp out (the glide one goes back to the pool), Monster+0x130 is restored and the glide sound stops. */
void StateFly::handlePreemption(MonsterState *next)
{
    subState = SUB_0;
    footPref = FOOT_ANY;
    blend108.rampOut(100.0f);
    blend170.rampOut(100.0f);
    blendWings[0].rampOut(100.0f);
    blendWings[1].rampOut(100.0f);
    blendWings[2].rampOut(100.0f);
    if (glide != 0) {
        glide->rampOutAndDealloc(100.0f);
        glide = 0;
    }
    owner->m_unk1B8 = 1.0f;
    *(int *)((char *)owner + 0x130) = savedMonster130;
    MSOUND(owner)->terminateFlightGlideSound();
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __9StateJump);
void StateJump::init(Monster *m)
{
    owner = m;
    blendWings[0].init(m->m_anims[0x24]);
    blendWings[1].init(owner->m_anims[0x25]);
    blendWings[2].init(owner->m_anims[0x26]);
}
#ifdef NON_MATCHING
/* Height reached by the jump launch speed (config +0xC) against gravity: v^2 / (2g), g being negative. */
float StateJump::getJumpHeightGain(void)
{
    float v = SCFG(getRelevantConfig(), 0xC);

    return -(v * v) / (2.0f * game->m_gravity);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getJumpHeightGain__9StateJump);
#endif
#ifdef NON_MATCHING
char *StateJump::getRelevantConfig(void)
{
    char *m = (char *)owner;

    if (*(int *)(m + 0x68B4) == 0) {
        int heavy = 0;

        if (*(int *)(m + 0x68A4) != 0)
            heavy = (int)((**(Pickup ***)(m + 0x68A4))->bits >> 1) & 1;
        if (heavy)
            return (char *)this + 0x68;
        return (char *)this + 0x38;
    }
    return (char *)this + 0x68;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getRelevantConfig__9StateJump);
#endif
#ifdef NON_MATCHING
int StateJump::transitionOK(void)
{
    char *m = (char *)owner;

    if (*(int *)(m + 0x14) == 0x1A0 && *(int *)(m + 0x68B4) != 0)
        return 0;
    if (*(int *)(m + 0x1ED0) == 0)
        return 0;
    if (*(unsigned short *)(PAD(m, 0) + 0x2A) == 0 || VFEASIBLE(this) == 0)
        return *(int *)(m + 0x288) >= 6;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__9StateJump);
#endif
#ifdef NON_MATCHING
/* Jumping/flying is only possible while the pad entry one frame back has its field 0x2A at 0. */
int StateJump::transitionFeasible(void)
{
    return *(short *)((char *)(*(PadFlags *)((char *)owner + 0x5040))[1] + 0x2A) == 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionFeasible__9StateJump);
#endif
/* Picks the config. With the jump button held right after leaving the ground: from a climb sub-state 2, else the
 * take-off (1). Otherwise 2 after a climb's ledge mount, else falling (3). Robo-47 plays its jet. */
#ifdef NON_MATCHING
/* untuned: 74/78 words; retail computes the heavy test as a value first (inline) */
void StateJump::transitionInto(void)
{
    JumpFlyBase::transitionInto();
    if (owner->m_target != 0 || (owner->m_pickup != 0 && TWO_HANDED(owner))) {
        heavy = 1;
        config = heavyConfig;
        footPref = FOOT_ANY;
    } else {
        heavy = 0;
        config = lightConfig;
    }
    if (owner->m_padFlags[0]->jump != 0 && owner->m_fallTime < 2) {
        if (*owner->m_prevState == STATE_ID_CLIMB)
            enterSubState(SUB_2);
        else
            enterSubState(SUB_1);
    } else if (*owner->m_prevState == STATE_ID_CLIMB && ((StateClimb *)STATE_AT(owner, ST_CLIMB))->subState == StateClimb::SUB_MOUNT) {
        enterSubState(SUB_2);
    } else {
        enterSubState(SUB_3);
    }
    if (owner->m_typeBits == MONSTER_TYPE_ROBOT)
        MSOUND(owner)->playRoboJetOneShot();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionInto__9StateJump);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__9StateJump);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", enterSubState__9StateJumpQ211JumpFlyBase8SubState);
/* Only while not rising. Ignores interactives without flag 8 and other monsters. On a floor (kind 7, or a normal
 * steeper than 45 degrees up that is not kind 10/11), sub-states 3 and 6 land (8), and 8 goes on to 9. */
#ifdef NON_MATCHING
/* untuned: 13/71 words; retail reads the hit ids with ld (64-bit bitfield) */
void StateJump::handleCollis(_hdResult &r)
{
    int kind;
    int floor;

    MonsterState::handleCollis(r);
    if (0.0f < MVEL(owner)->z)
        return;
    if (r.ids & 0x7FF) {
        int *hit = Interactives::getInteractive(r.ids & 0x7FF);

        if (!(*(unsigned short *)(hit + 1) & 8))
            return;
        if (*hit == 1)
            return;
    }
    kind = ((unsigned char *)&r)[3];
    floor = 0;
    if (kind == 7 || (0.7071f < r.normal.z && kind != 10 && kind != 11))
        floor = 1;
    if (!floor)
        return;
    if (subState == SUB_3 || subState == SUB_6)
        enterSubState(SUB_8);
    if (subState == SUB_8)
        enterSubState(SUB_9);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handleCollis__9StateJumpR9_hdResult);
#endif
void StateJump::handlePreemption(MonsterState *next)
{
    footPref = FOOT_ANY;
    blend9C.rampOut(100.0f);
    blend104.rampOut(100.0f);
    blendWings[0].rampOut(100.0f);
    blendWings[1].rampOut(100.0f);
    blendWings[2].rampOut(100.0f);
    owner->m_unk1B8 = 1.0f;
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __14StateRamAttack);
void StateRamAttack::setMaxSpeedMPH(float mph)
{
    SCFG(this, 0x30) = mph;
    owner->recomputeDynamics();
}
/* Needs attacks enabled, the ram animation (0x10B), the ram button, the feet on the ground (Monster::m_freeFalling
 * clear), the stamina and no two-handed pickup. */
#ifdef NON_MATCHING
/* untuned: 26/44 words; retail keeps the free-hands test in a flag */
int StateRamAttack::transitionOK(void)
{
    if (!owner->m_attacksEnabled)
        return 0;
    if (owner->m_anims[0x10B].ci == 0)
        return 0;
    if (owner->m_padFlags[0]->f40 == 0)
        return 0;
    if (owner->m_freeFalling)
        return 0;
    if (!owner->m_stamina.hasEnough(staminaCost))
        return 0;
    if (owner->m_pickup == 0 || !TWO_HANDED(owner))
        return 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__14StateRamAttack);
#endif
void StateRamAttack::transitionInto(void)
{
    frames = 0;
    enterSubState(SUB_CHARGE);
    if (owner->m_typeBits == MONSTER_TYPE_ASSBOSS)
        flags &= ~0x20;
}
/* Charge: at maxSpeed for `duration` frames, as long as the button is held or the monster is not falling for 6
 * frames; the effects follow. Coasting: while face button 2 is held keep the base foot speed (air control off the
 * ground), else just move (falling ends the ram); done once back near the normal speed. Impact: done near the end of
 * the animation. A fast ram puts out a fire. */
#ifdef NON_MATCHING
/* untuned: 4/206 words; the spray position is computed with VU0 macro ops in retail (vadda/vmaddw), here in C */
void StateRamAttack::update(void)
{
    MonsterDynamics *dyn;
    float pct;
    float speed;

    MonsterState::update();
    dyn = DYN(owner);
    pct = animationGetCurrentPercent(owner->m_anims[anim]);
    switch (subState) {
    case SUB_CHARGE:
        if (duration <= (float)frames || (owner->m_padFlags[0]->f2C == 0 && owner->m_fallTime >= 6)) {
            enterSubState(SUB_COAST);
        } else {
            dyn->updateFootMotion(maxSpeed, accel, footF28);
            updateFxPos();
        }
        break;
    case SUB_COAST:
        if (owner->m_padFlags[0]->face[2] != 0) {
            if (owner->m_freeFalling)
                dyn->updateAirMotion(dyn->getConfig()[3], airDeccel);
            dyn->updateFootMotion(dyn->getConfig()[3], accel, footF28);
        } else {
            if (owner->m_freeFalling) {
                enterSubState(SUB_DONE);
                break;
            }
            DYN(owner)->updateMove(false);
        }
        speed = *(float *)((char *)dyn + 0x154);
        if (speed <= dyn->getConfig()[3] * dyn->getSpeedModifier() + 10.0f)
            enterSubState(SUB_DONE);
        break;
    case SUB_IMPACT:
        if (0.99f <= pct)
            enterSubState(SUB_DONE);
        DYN(owner)->updateMove(false);
        break;
    }
    if (owner->m_onFireCount > 0.0f && maxSpeed * 0.75f < owner->m_speed)
        owner->m_onFireCount = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__14StateRamAttack);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", handleCollis__14StateRamAttackR9_hdResult);
/* Charge: animation, sound, stamina, the three effects (a dust trail 0x31 under the monster, two sprays 0x13 between
 * the locators, angled by ramSprayLeft/ramSprayRight) and a fresh hit history. Coasting stops the effects. Impact:
 * animation 0x10B with a camera shake (once), effects stopped, the velocity cut to a quarter. Done: back to idle and
 * the dash button released. */
#ifdef NON_MATCHING
/* untuned: 4/316 words; retail copies the spray matrices with lq/sq and computes the spray position with VU0 */
void StateRamAttack::enterSubState(SubState s)
{
    switch (s) {
    case SUB_CHARGE:
        if (owner->m_anims[0x10A].ci != 0) {
            anim = 0x10A;
            animationTransitionInto(owner->m_anims[0x10A], blendTime, 1, 1);
        } else {
            anim = 0x109;
        }
        MSOUND(owner)->playRamSound();
        owner->m_stamina.drain(staminaCost, true, false);
        *(int *)((char *)owner + 0x484) = 0;
        DYN(owner)->setMotionVec(*(_fvector *)UNIT_MATRIX[1]);
        *(int *)((char *)owner + 0x22C) = 0;
        updateFxPos();
        {
            float trailMat[4][4] = {{1.0f}, {0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
            Mat44 left = ramSprayLeft;
            Mat44 right = ramSprayRight;

            fx[0] = particleCreateFx(&trailPos, owner->m_cs->mat, 0x31, 1.0f, 0, trailMat, false, 0.0f);
            fx[1] = particleCreateFx(&sprayPos, owner->m_cs->mat, 0x13, 1.0f, 0, left.m, false, 0.0f);
            fx[2] = particleCreateFx(&sprayPos, owner->m_cs->mat, 0x13, 1.0f, 0, right.m, false, 0.0f);
        }
        ((HitHistory *)hits)->reset();
        break;
    case SUB_COAST:
        particleKillFx(fx[0]);
        particleKillFx(fx[1]);
        particleKillFx(fx[2]);
        break;
    case SUB_IMPACT:
        anim = 0x10B;
        if (!animationIsRunning(owner->m_anims[0x10B])) {
            animationTransitionInto(owner->m_anims[anim], impactBlend, 1, 1);
            Cameras::StartShake(*(_fvector *)&owner->m_cs->trans, 50.0f);
        }
        particleKillFx(fx[0]);
        particleKillFx(fx[1]);
        particleKillFx(fx[2]);
        vecScale(MVEL(owner), MVEL(owner), 0.25f);
        break;
    case SUB_DONE:
        *(int *)((char *)owner + 0x240) = 0;
        owner->enterNewState(STATE_AT(owner, ST_IDLE));
        owner->m_padFlags[0]->f3E = 0;
        break;
    }
    subState = s;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", enterSubState__14StateRamAttackQ214StateRamAttack8SubState);
#endif
/* Another state takes over: the ram's three effects stop. */
void StateRamAttack::handlePreemption(MonsterState *next)
{
    *(int *)((char *)owner + 0x484) = 1;
    particleKillFx(*(int *)((char *)this + 0xB0));
    particleKillFx(*(int *)((char *)this + 0xB4));
    particleKillFx(*(int *)((char *)this + 0xB8));
}
/* The assboss's charge (sub-states 1 and 2) takes every hit. Otherwise like StateButtSlam::acceptHit, except that a
 * hit from sources 2..4 counts only when blockable (type 2). */
int StateRamAttack::acceptHit(HitEvent &e)
{
    if ((unsigned)(subState - 1) < 2 && owner->m_typeBits == MONSTER_TYPE_ASSBOSS)
        return 1;
    if ((unsigned)(e.source - 2) < 3 && (e.sourceKind == 0 || e.sourceKind == 2 || e.sourceKind == 5) && e.type == 2)
        return 1;
    if (e.source == 5)
        return 1;
    if (e.source == 0xB && (e.strength34 != 0.0f || e.strength38 != 0.0f))
        return 1;
    return 0;
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __8StateRun);
/* The run tuning of each monster type (speeds in miles per hour, see MPH). */
#ifdef NON_MATCHING
/* untuned: 63/205 words; same constants, retail reuses the MPH(270) register and orders the stores differently */
void StateRun::init(Monster *m)
{
    owner = m;
    blend.init(m->m_anims[5]);
    walkSpeed = 250.0f;
    f5C = 240.0f;
    normal.walkSpeed = 150.0f;
    heavy.speed = MPH(170);
    heavy.f10 = 0.5f;
    heavy.f1C = 0.62f;
    anim = 0;
    normal.speed = MPH(270);
    normal.f10 = 0.5f;
    normal.f14 = 0.0f;
    normal.f18 = 0.15f;
    normal.f1C = 0.62f;
    heavy.f14 = 0.0f;
    heavy.f18 = 0.15f;
    switch (m->m_typeBits) {
    case 0x20:
        heavy.f14 = 0.8f;
        normal.speed = MPH(270);
        normal.f18 = 0.2f;
        normal.f1C = 0.7f;
        heavy.f10 = 0.35f;
        normal.f10 = 0.35f;
        normal.f14 = 0.8f;
        break;
    case 0x40:
    case 0xE0:
    case 0x120:
        normal.speed = MPH(270);
        break;
    case 0x60:
        normal.speed = MPH(265);
        break;
    case 0x80:
        heavy.f14 = 0.8f;
        normal.speed = MPH(275);
        normal.walkSpeed = 235.0f;
        normal.f18 = 0.15f;
        normal.f1C = 0.65f;
        heavy.f10 = 0.3f;
        normal.f10 = 0.3f;
        normal.f14 = 0.8f;
        break;
    case 0xA0:
        normal.speed = MPH(289);
        normal.walkSpeed = 200.0f;
        break;
    case 0x100:
        normal.speed = MPH(279);
        break;
    case 0x140:
        heavy.f14 = 0.99f;
        normal.speed = MPH(264);
        normal.f18 = 0.2f;
        normal.f1C = 0.7f;
        heavy.f10 = 0.35f;
        normal.f10 = 0.35f;
        normal.f14 = 0.99f;
        break;
    case 0x160:
        normal.speed = MPH(264);
        break;
    case MONSTER_TYPE_ASSBOSS:
        normal.speed = MPH(400);
        normal.f14 = 0.75f;
        normal.walkSpeed = 142.0f;
        normal.f10 = 0.3f;
        break;
    case 0x1C0:
        normal.speed = MPH(100);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", init__8StateRunP7Monster);
#endif
/* Needs the run animation (2), and 0xAD when holding something large; starts on any movement input. */
#ifdef NON_MATCHING
/* untuned: 12/63 words; the last input test returns through a branch in retail */
int StateRun::transitionOK(void)
{
    if (owner->m_anims[2].ci == 0)
        return 0;
    if (owner->isHoldingLarge() && owner->m_anims[0xAD].ci == 0)
        return 0;
    if (owner->m_padFlags[0]->f12 != 0)
        return 1;
    if (owner->m_padFlags[0]->f14 != 0)
        return 1;
    if (owner->m_padFlags[0]->f16 != 0)
        return 1;
    if (owner->m_padFlags[0]->f18 != 0)
        return 1;
    if (owner->m_padFlags[0]->f1A != 0)
        return 1;
    if (owner->m_padFlags[0]->f1C != 0)
        return 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionOK__8StateRun);
#endif
/* Chooses the config and the run animation (see the class), ramps the blend layer in at the percent it had (pulled
 * towards 50% by how much it still weighed) and enters the sub-state of the input held. */
#ifdef NON_MATCHING
/* untuned: 35/142 words; retail computes the heavy test as a value first (inline) */
void StateRun::transitionInto(void)
{
    AnimBlend *b;
    float pct;

    frames = 0;
    fD4 = 0;
    fE0 = 0;
    if (owner->m_target != 0 || (owner->m_pickup != 0 && TWO_HANDED(owner))) {
        config = &heavy;
        runAnim = 0xAD;
        altAnim = 0xB0;
        landMomentum = 0.0f;
    } else {
        config = &normal;
        if (landMomentum < 350.0f) {
            if (owner->m_speed < walkSpeed && owner->m_anims[1].ci != 0 && owner->m_anims[7].ci != 0) {
                runAnim = 1;
                altAnim = 7;
            } else {
                runAnim = 2;
                altAnim = 8;
            }
            landMomentum = 0.0f;
        } else {
            runAnim = 6;
            altAnim = 9;
        }
    }
    b = &blend;
    pct = 0.5f;
    if (b->m_node.active)
        pct = (b->getPercent() - pct) * b->getWeight() + pct;
    b->rampIn(0.0f);
    b->setSpeed(0.0f);
    b->setPercent(pct);
    if (owner->m_padFlags[0]->f12 != 0)
        enterSubState(SUB_1);
    else if (owner->m_padFlags[0]->f14 != 0)
        enterSubState(SUB_2);
    else if (owner->m_padFlags[0]->f1A != 0)
        enterSubState(SUB_3);
    else if (owner->m_padFlags[0]->f1C != 0)
        enterSubState(SUB_4);
    else
        enterSubState(SUB_1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", transitionInto__8StateRun);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", update__8StateRun);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", enterSubState__8StateRunQ28StateRun8SubState);
/* The blend layer ramps out slowly into the air state, quickly otherwise. */
void StateRun::handlePreemption(MonsterState *next)
{
    if (next == (MonsterState *)owner->m_stateRef)
        blend.rampOut(900.0f);
    else
        blend.rampOut(200.0f);
    landMomentum = 0.0f;
}
/* Animation speed for a ground speed: the speed (at least a share of the base foot speed: 30%, 43% heavy) over the
 * config's reference speed ([1] for the walk animation, [0] otherwise). */
#ifdef NON_MATCHING
/* untuned: 12/55 words; retail clamps with max.s */
float StateRun::getAnimSpeed(float speed)
{
    float k;
    float s;

    if (runAnim == 1) {
        s = DYN(owner)->getConfig()[3] * 0.3f;
        s = speed > s ? speed : s;
        return s / config->walkSpeed;
    }
    k = 0.3f;
    if (config == &heavy)
        k = 0.43f;
    s = DYN(owner)->getConfig()[3] * k;
    s = speed > s ? speed : s;
    return s / config->speed;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getAnimSpeed__8StateRunf);
#endif
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", func_001769B8);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", func_001769C0);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", func_001769C8);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", D_006ED4B0);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", D_006ED4F0);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$8StateRun);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$14StateRamAttack);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$9StateJump);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$8StateFly);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$11JumpFlyBase);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$9StateDash);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$10StateClimb);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", _vt$13StateButtSlam);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf13StateButtSlam);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf10StateClimb);
int getSubState__10StateClimb(void *self) __asm__("getSubState__10StateClimb");
int getSubState__10StateClimb(void *self)
{
    return *(int *)((char *)self + 0x7C);
}
void * getClimbMotion__10StateClimb(void *self) __asm__("getClimbMotion__10StateClimb");
void * getClimbMotion__10StateClimb(void *self)
{
    return (char *)self + 0x90;
}
void * getHuckVelocity__10StateClimb(void *self) __asm__("getHuckVelocity__10StateClimb");
void * getHuckVelocity__10StateClimb(void *self)
{
    return (char *)self + 0x60;
}
void * getContactNormal__10StateClimb(void *self) __asm__("getContactNormal__10StateClimb");
void * getContactNormal__10StateClimb(void *self)
{
    return (char *)self + 0xB0;
}
int getContactInteractive__10StateClimb(void *self) __asm__("getContactInteractive__10StateClimb");
int getContactInteractive__10StateClimb(void *self)
{
    return *(int *)((char *)self + 0xC0);
}
int getContactFlags__10StateClimb(void *self) __asm__("getContactFlags__10StateClimb");
int getContactFlags__10StateClimb(void *self)
{
    return *(int *)((char *)self + 0xA0);
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf9StateDash);
unsigned char getSubState__9StateDash(void *self) __asm__("getSubState__9StateDash");
unsigned char getSubState__9StateDash(void *self)
{
    return *(unsigned char *)((char *)self + 0x3C);
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf11JumpFlyBase);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __11JumpFlyBaseQ212MonsterState2IdQ212MonsterState4Caps);
void setFootPref__11JumpFlyBaseQ211JumpFlyBase8FootPref(void *self, int v) __asm__("setFootPref__11JumpFlyBaseQ211JumpFlyBase8FootPref");
void setFootPref__11JumpFlyBaseQ211JumpFlyBase8FootPref(void *self, int v)
{
    *(int *)((char *)self + 0x1C) = v;
}
unsigned char getSubState__11JumpFlyBase(void *self) __asm__("getSubState__11JumpFlyBase");
unsigned char getSubState__11JumpFlyBase(void *self)
{
    return *(unsigned char *)((char *)self + 0x18);
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getApexHeight__11JumpFlyBase);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", getHeightGain__11JumpFlyBase);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", D_006ED800);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf8StateFly);
unsigned char getSubState__8StateFly(void *self) __asm__("getSubState__8StateFly");
unsigned char getSubState__8StateFly(void *self)
{
    return *(unsigned char *)((char *)self + 0x18);
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf9StateJump);
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf14StateRamAttack);
void setRamDuration__14StateRamAttackf(void *self, float v) __asm__("setRamDuration__14StateRamAttackf");
void setRamDuration__14StateRamAttackf(void *self, float v)
{
    *(float *)((char *)self + 0x38) = v;
}
void setAirDeccel__14StateRamAttackf(void *self, float v) __asm__("setAirDeccel__14StateRamAttackf");
void setAirDeccel__14StateRamAttackf(void *self, float v)
{
    *(float *)((char *)self + 0x2C) = v;
}
void setAcceleration__14StateRamAttackf(void *self, float v) __asm__("setAcceleration__14StateRamAttackf");
void setAcceleration__14StateRamAttackf(void *self, float v)
{
    *(float *)((char *)self + 0x24) = v;
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", __tf8StateRun);
void setLandMomentum__8StateRunf(void *self, float v) __asm__("setLandMomentum__8StateRunf");
void setLandMomentum__8StateRunf(void *self, float v)
{
    *(float *)((char *)self + 0xDC) = v;
}
int getAnimIndex__8StateRun(void *self) __asm__("getAnimIndex__8StateRun");
int getAnimIndex__8StateRun(void *self)
{
    return *(int *)((char *)self + 0x60);
}
INCLUDE_ASM("asm/nonmatchings/game/MovementStates", func_00176DA0);
