#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/hit_event.h"
#include "game/anim_blend.h"
#include "game/streaming_sound.h"
#include "game/weapons.h"
#include "cs_pool.h"
#include "engine.h"
#include "vecmath.h"

/* The plant boss (level 7, the greenhouse): one global `plantBoss` with three heads, ten roots and the base (the
 * body that hides and emerges). The pieces are DbInteractives (vptr at 0x10; takeHit and getPinTrans in the vtable);
 * the vtables, __tf and the thunks stay asm, so the classes are declared without virtuals. */

class DbInteractive {
public:
    int m_kind;               /* 0x00 */
    unsigned short m_flags;   /* 0x04: 8 = can be hit */
    char pad6[6];
    _cs *m_cs;                /* 0x0C: the cs hits are tested against */
    void *m_vptr;             /* 0x10 */
};

class Interactives {
public:
    static int addInteractive(DbInteractive *p);
    static DbInteractive *getInteractive(int i);
};

class AnimContact {
public:
    char pad[0x1C];
    float m_ch[2][6];         /* 0x1C: two frames of contact channels; [m_cur][5] is the one the plant head listens to */
    int m_cur;                /* 0x4C */
    void init(_animHandle &h);
    void update(void);
};

void hierSetSwitch(_hierswitch *sw, int which);

float mathfManhatDist(_fvector *a, _fvector *b);
float mathfManhatDist2D(_fvector *a, _fvector *b);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rot);
float hdHatTest(_fvector *pos, _fvector *normal, float up, bool option, float down);
int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);
struct ActuatorData {
    int w[8];
};
void inputSetActuator(int pad, ActuatorData *a);

extern HitEvent s_takeHitInfo __asm__("_8HitEvent$s_takeHitInfo");

/* Monster's own vtable sits at +0x10; slot 0x20 is getVel */
#define MONSTER_VEL(m) \
    ((float *)((void *(*)(void *))*(void **)(*(char **)((char *)(m) + 0x10) + 0x24))((char *)(m) + *(short *)(*(char **)((char *)(m) + 0x10) + 0x20)))
/* locator `id` of a monster (0x800 + n) */
#define MONSTER_LOCATOR(m, id) ((_fvector *)((char *)(m) + (id) * 16 - 0x4260))
/* rumble presets in TheGame */
#define GAME_ACTUATOR(off) (*(ActuatorData *)((char *)game + (off)))

class PlantBossHead : public DbInteractive {
public:
    int m_awake;              /* 0x14: 1 from wakeUp */
    int m_state;              /* 0x18: enterState, also the animation played (13 and 99 are final: no more changes) */
    float m_health;           /* 0x1C: 20 at initBefore */
    float m_hurt;             /* 0x20: damage taken since state 1; 10 makes it recoil (state 17) */
    int m_24;                 /* 0x24 */
    int m_holding;            /* 0x28: player 1 is in its mouth (chomp hit); no damage taken meanwhile, health stops at 1 */
    int m_recoiled;           /* 0x2C: set with state 17, cleared by state 1 */
    int m_sporeLaunched;      /* 0x30: state 18 fired its spore (cleared on entry) */
    char pad34[0x38 - 0x34];
    _cs *m_headCs;            /* 0x38: the head (hit target, m_cs) */
    _hierhead *m_headEp;      /* 0x3C */
    _cs *m_stemCs;            /* 0x40: the stem; its ep node takes the head's interactive id */
    _hierhead *m_stemEp;      /* 0x44 */
    _hierswitch *m_switch;    /* 0x48: shown at wakeUp */
    _animHandle m_anims[19];  /* 0x4C: hier 0x271B + head, animations 0..18 */
    FVECTOR m_pos;            /* 0x180: where the head sits (its cs trans at wakeUp) */
    FVECTOR m_prevPin;        /* 0x190: m_pinTrans of the previous field (the spit throws along the difference) */
    FVECTOR m_pinTrans;       /* 0x1A0: aim point (getPinTrans; HomingBug aims at it) */
    FMATRIX_16 m_pinMat;      /* 0x1B0: the stem cs matrix; row 1 is the spit direction */
    PlantBossHeadSound m_sound; /* 0x1F0 */
    AnimContact m_contact;    /* 0x1F4 */
    char pad244[0x250 - 0x244];

    void initBefore(void);
    void initAfter(int idx);
    void wakeUp(int idx);
    void enterState(int s);
    void update(void);
    void takeHit(_fvector *dir, float dmg, int attacker);
};

class PlantBossRoot : public DbInteractive {
public:
    int m_state;              /* 0x14: enterState (0..4) */
    int m_18;                 /* 0x18 */
    _hierhead *m_ep;          /* 0x1C */
    _cs *m_rootCs;            /* 0x20 */
    _animHandle m_anims[5];   /* 0x24: hier 0x271E + root, animations 0..4 */
    char pad74[0x80 - 0x74];
    FVECTOR m_tip;            /* 0x80: the point that hurts player 1 (state 3) and where the loop sound plays */
    char pad90[0xA0 - 0x90];
    FVECTOR m_push;           /* 0xA0: recoil it gives player 1 */
    char padB0[0xD0 - 0xB0];
    PlantBossRootSound m_sound; /* 0xD0 */
    char padD4[0xE0 - 0xD4];

    void initBefore(void);
    void initAfter(int idx);
    void wakeUp(int idx);
    void enterState(int s);
    void update(void);
    void takeHit(_fvector *dir, float dmg, int attacker);
};

class PlantBossBase : public DbInteractive {
public:
    int m_state;              /* 0x14: enterState (0..9; 4 emerges, 5 hides, 7/0 take hits) */
    int m_18;                 /* 0x18 */
    _hierhead *m_ep;          /* 0x1C */
    _cs *m_baseCs;            /* 0x20 */
    _animHandle m_anims[10];  /* 0x24: hier 0x2728, animations 0..9 */
    char padC4[0xD0 - 0xC4];
    PlantBossSound m_sound;   /* 0xD0 */
    char padDC[0xE0 - 0xDC];

    void initBefore(void);
    void initAfter(void);
    void wakeUp(void);
    void enterState(int s);
    void update(void);
    void takeHit(_fvector *dir, float dmg, int attacker);
};

class PlantBoss {
public:
    int m_active;             /* 0x00: 1 after wakeUp */
    int m_04;                 /* 0x04: 0x474 at wakeUp */
    int m_08;                 /* 0x08: 0x3C0 at wakeUp (fields); the base ignores enterState while > 0 with m_0C == 1 */
    int m_0C;                 /* 0x0C */
    int m_10;                 /* 0x10: set to 10 by every head hit */
    int m_14;                 /* 0x14 */
    int m_18;                 /* 0x18 */
    int m_1C;                 /* 0x1C */
    PlantBossHead m_heads[3]; /* 0x20 */
    PlantBossRoot m_roots[10]; /* 0x710 */
    PlantBossBase m_base;     /* 0xFD0 */

    void initBefore(void);
    void initAfter(void);
    void wakeUp(void);
    void update(void);
    void decideChompOrSpit(void);
};

typedef char _size_PlantBossHead[sizeof(PlantBossHead) == 0x250 ? 1 : -1];
typedef char _size_PlantBossRoot[sizeof(PlantBossRoot) == 0xE0 ? 1 : -1];
typedef char _size_PlantBoss[sizeof(PlantBoss) == 0x10B0 ? 1 : -1];

extern PlantBoss plantBoss;
extern int numDeadHeads;
__asm__("#SNFIX_SMALL numDeadHeads");
extern int D_006F8C6C;
extern int headAttacks[7][2]; /* the attack sequence decideChompOrSpit sets up (pairs; -1 ends) */

void PlantBoss::initBefore(void)
{
    int i;

    m_active = 0;
    m_08 = 0;
    m_18 = 0;
    m_1C = 0;
    for (i = 0; i < 3; i++) {
        m_heads[i].initBefore();
    }
    for (i = 0; i < 10; i++) {
        m_roots[i].initBefore();
    }
    m_base.initBefore();
    numDeadHeads = 0;
}

void PlantBoss::initAfter(void)
{
    int i;

    m_0C = 0;
    for (i = 0; i < 3; i++) {
        m_heads[i].initAfter(i);
    }
    for (i = 0; i < 10; i++) {
        m_roots[i].initAfter(i);
    }
    m_base.initAfter();
    D_006F8C6C = 0;
}

void PlantBoss::wakeUp(void)
{
    m_14 = 0;
    m_active = 1;
    m_08 = 0x3C0;
    m_04 = 0x474;
    decideChompOrSpit();
    hierSetSwitch(m_heads[0].m_switch, 1);
    hierSetSwitch(m_heads[1].m_switch, 1);
    hierSetSwitch(m_heads[2].m_switch, 1);
}

#ifdef NON_MATCHING
extern int numLoops __asm__("numLoops.2562");   /* statics of update */
__asm__("#SNFIX_SMALL numLoops.2562");
extern int finalFade __asm__("finalFade.2563");
__asm__("#SNFIX_SMALL finalFade.2563");
extern int moviePlaying;
__asm__("#SNFIX_SMALL moviePlaying");
extern int movieAborted;
__asm__("#SNFIX_SMALL movieAborted");
#endif

class ActionDispatch {
public:
    static void callScriptFromId(unsigned short id1, unsigned short id2, int fadeOut);
};

void rtReturnToShell(int a, int b);
#define CAMERA_POV(view) (*(int *)((char *)&Cameras::m_cameras + (view) * 0xEB0 + 0xCF8))
#define CAMERA_D9C(view) (*(int *)((char *)&Cameras::m_cameras + (view) * 0xEB0 + 0xD9C))

/* m_08 counts the intro (heads and base wake at 0x348 left, fade at 31) or, with m_0C set, the ending after the third
 * head dies (cinema 0x2264, fade out, back to the shell). m_04 paces the attack cycle in m_14: 0 heads attack
 * (headAttacks), 1 heads hide and the base hides, 2 the ten roots strike in turn (three loops), 3 roots retract. */
#ifdef NON_MATCHING
/* equivalent, 18/570 words untuned; retail walks the heads by pointer and schedules the colour flash differently */
void PlantBoss::update(void)
{
    PlantBossBase *base;
    PlantBossHead *h;
    int i;

    if (moviePlaying != 0 && movieAborted != 0 && m_08 >= 0x20) {
        m_08 = 0x1F;
    }
    base = &m_base;
    if (m_08 > 0) {
        m_08--;
        if (m_0C == 0) {
            if (m_08 <= 0) {
                hierSetSwitch(m_heads[0].m_switch, 1);
                hierSetSwitch(m_heads[1].m_switch, 0);
                hierSetSwitch(m_heads[2].m_switch, 0);
            }
            if (m_08 < 0x1F && game->f120454 == 0) {
                game->fadeOutAndIn(4);
            }
            if (m_08 == 0x348) {
                m_heads[0].wakeUp(0);
                m_heads[1].wakeUp(1);
                m_heads[2].wakeUp(2);
                base->wakeUp();
            }
            game->m_lists.players[0]->cleanUpForMovie();
        } else if (numDeadHeads >= 2) {
            m_04 = 0x5DC;
            if (m_08 < 0x349 && D_006F8C6C == 0) {
                for (h = m_heads; h < m_heads + 3; h++) {
                    h->m_headCs->rot.z = 0.0f;
                    mathfRotMatrixRPH(h->m_headCs->mat, &h->m_headCs->rot);
                }
                D_006F8C6C = 1;
                finalFade = 0;
                ActionDispatch::callScriptFromId(0x2264, 0, 0);
                STREAMING_SOUND()->startCinemaMusicFromId(0x2264);
            }
            if (m_08 < 0x1F && finalFade == 0) {
                game->fadeOut(4);
                finalFade = 1;
            }
            if (m_08 <= 0) {
                rtReturnToShell(0, 0);
            }
            game->m_lists.players[0]->cleanUpForMovie();
        }
    }
    if (game->m_lists.players[0]->m_dead != 0) {
        m_04 = 0x12C;
        if (m_14 == 0) {
            m_14 = 1;
            m_04 = 1;
            m_18 = 0;
            if (CAMERA_POV(0) == 3) {
                if (game->m_lists.players[0]->m_state[1] & 0x10) {
                    Cameras::SetCameraPOV(0, (Camera::CameraPOV)5);
                } else {
                    Cameras::SetCameraPOV(0, (Camera::CameraPOV)0);
                }
                CAMERA_D9C(0) = 1;
            }
        }
    }
    if (m_04 > 0 && numDeadHeads < 3 && --m_04 == 0) {
        switch (m_14) {
        case 0: {
            int head = headAttacks[m_18][0];
            int kind = headAttacks[m_18][1];

            if (head != -1 && kind != 0) {
                if (kind == 1) {
                    if (m_heads[head].m_awake != 0) {
                        m_heads[head].enterState(1);
                        m_04 = 0x1F4;
                    } else {
                        m_14 = kind;
                        m_04 = kind;
                        m_18 = 0;
                    }
                } else if (kind == 2) {
                    m_heads[head].enterState(0x12);
                    m_04 = 0x12C;
                }
                m_18++;
            } else {
                m_18 = 0;
                m_14 = 1;
                m_04 = 1;
            }
            break;
        }
        case 1:
            m_04 = 0x12C;
            m_14 = 2;
            m_1C = 0;
            numLoops = 0;
            for (i = 0; i < 3; i++) {
                if (m_heads[i].m_awake != 0) {
                    m_heads[i].enterState(0xE);
                }
            }
            base->enterState(5);
            break;
        case 2:
            if (m_roots[m_1C].m_rootCs->drawMe != 0) {
                m_04 = 2;
            } else {
                m_roots[m_1C].wakeUp(m_1C);
                m_04 = mathfRand(0x14, 0x28);
            }
            if (++m_1C >= 10) {
                m_1C = 0;
                if (++numLoops >= 3) {
                    m_14 = 3;
                    decideChompOrSpit();
                    for (i = 0; i < 3; i++) {
                        if (m_heads[i].m_awake != 0) {
                            m_heads[i].enterState(0xF);
                        }
                    }
                    base->enterState(4);
                }
            }
            break;
        case 3:
            m_roots[m_1C++].enterState(4);
            m_04 = mathfRand(0xF, 0x1E);
            if (m_1C >= 10) {
                m_1C = 0;
                m_04 = 0x3C;
                m_14 = 0;
            }
            break;
        }
    }
    for (i = 0; i < 3; i++) {
        if (m_heads[i].m_awake != 0) {
            m_heads[i].update();
        }
    }
    for (i = 0; i < 10; i++) {
        m_roots[i].update();
    }
    base->update();
    if (--m_10 > 0) {
        for (h = m_heads; h < m_heads + 3; h++) {
            if (h->m_awake != 0) {
                h->m_headCs->color.chan.r = 0xFF;
                h->m_headCs->color.chan.g = 0;
                h->m_headCs->color.chan.b = 0;
            }
        }
        base->m_baseCs->color.chan.r = 0xFF;
        base->m_baseCs->color.chan.g = 0;
        base->m_baseCs->color.chan.b = 0;
    } else {
        for (h = m_heads; h < m_heads + 3; h++) {
            if (h->m_awake != 0) {
                h->m_headCs->color.chan.r = (int)smoothEasyIn(h->m_headCs->color.chan.r, 128.0f, 0.15f, 0.001f);
                h->m_headCs->color.chan.g = (int)smoothEasyIn(h->m_headCs->color.chan.g, 128.0f, 0.15f, 0.001f);
                h->m_headCs->color.chan.b = (int)smoothEasyIn(h->m_headCs->color.chan.b, 128.0f, 0.15f, 0.001f);
            }
        }
        base->m_baseCs->color.chan.r = (int)smoothEasyIn(base->m_baseCs->color.chan.r, 128.0f, 0.15f, 0.001f);
        base->m_baseCs->color.chan.g = (int)smoothEasyIn(base->m_baseCs->color.chan.g, 128.0f, 0.15f, 0.001f);
        base->m_baseCs->color.chan.b = (int)smoothEasyIn(base->m_baseCs->color.chan.b, 128.0f, 0.15f, 0.001f);
    }
    for (h = m_heads; h < m_heads + 3; h++) {
        h->m_headCs->colorQuad.fVec[0] = h->m_headCs->color.chan.r * 0.0078125f;
        h->m_headCs->colorQuad.fVec[1] = h->m_headCs->color.chan.g * 0.0078125f;
        h->m_headCs->colorQuad.fVec[2] = h->m_headCs->color.chan.b * 0.0078125f;
    }
    base->m_baseCs->colorQuad.fVec[0] = base->m_baseCs->color.chan.r * 0.0078125f;
    base->m_baseCs->colorQuad.fVec[1] = base->m_baseCs->color.chan.g * 0.0078125f;
    base->m_baseCs->colorQuad.fVec[2] = base->m_baseCs->color.chan.b * 0.0078125f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", update__9PlantBoss);
#endif
/* Picks the attack sequence for the current phase (one live head per phase: numDeadHeads 0, 1, 2) and shows only
 * that head; the other two lose their hit flags (2 and 4). */
#ifdef NON_MATCHING
/* equivalent, 13/107 words; retail keeps the -7 mask in a register and orders the headAttacks stores differently */
void PlantBoss::decideChompOrSpit(void)
{
    PlantBossHead *h0, *h1, *h2;
    int i;

    for (i = 0; i < 7; i++) {
        headAttacks[i][0] = -1;
        headAttacks[i][1] = 0;
    }
    h0 = &m_heads[0];
    h1 = &m_heads[1];
    h2 = &m_heads[2];
    h0->m_flags &= ~6;
    h1->m_flags &= ~6;
    h2->m_flags &= ~6;
    if (numDeadHeads == 0) {
        headAttacks[0][0] = 1;
        headAttacks[0][1] = 2;
        headAttacks[1][0] = 2;
        headAttacks[1][1] = 2;
        headAttacks[2][0] = 0;
        headAttacks[2][1] = 1;
        h0->m_flags |= 6;
        hierSetSwitch(m_heads[0].m_switch, 1);
        hierSetSwitch(m_heads[1].m_switch, 0);
        hierSetSwitch(m_heads[2].m_switch, 0);
    } else if (numDeadHeads == 1) {
        headAttacks[0][0] = 2;
        headAttacks[0][1] = 2;
        headAttacks[1][0] = 1;
        headAttacks[1][1] = 2;
        headAttacks[2][0] = 2;
        headAttacks[2][1] = 2;
        headAttacks[3][0] = 1;
        headAttacks[3][1] = 1;
        h1->m_flags |= 6;
        hierSetSwitch(m_heads[0].m_switch, 0);
        hierSetSwitch(m_heads[1].m_switch, 1);
        hierSetSwitch(m_heads[2].m_switch, 0);
    } else if (numDeadHeads == 2) {
        headAttacks[0][0] = 2;
        headAttacks[0][1] = 2;
        headAttacks[1][0] = 2;
        headAttacks[1][1] = 2;
        headAttacks[2][0] = 2;
        headAttacks[2][1] = 2;
        headAttacks[3][0] = 2;
        headAttacks[3][1] = 2;
        headAttacks[4][0] = 2;
        headAttacks[4][1] = 2;
        headAttacks[5][0] = 2;
        headAttacks[5][1] = 1;
        h2->m_flags |= 6;
        hierSetSwitch(m_heads[0].m_switch, 0);
        hierSetSwitch(m_heads[1].m_switch, 0);
        hierSetSwitch(m_heads[2].m_switch, 1);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", decideChompOrSpit__9PlantBoss);
#endif

void PlantBossHead::initBefore(void)
{
    m_awake = 0;
    m_state = 0;
    m_health = 20.0f;
    m_24 = 0;
    m_headCs = 0;
    m_headEp = 0;
    m_stemCs = 0;
    m_stemEp = 0;
}

void PlantBossHead::initAfter(int idx)
{
    int i;

    for (i = 0; i < 19; i++) {
        animationGetHandle(&m_anims[i], 0x271B + idx, 0, i);
    }
    switch (game->m_matchMode) {
    case 0:
        animationSetSpeed(m_anims[1], 1.75f);
        break;
    case 1:
        animationSetSpeed(m_anims[1], 2.5f);
        break;
    case 2:
        animationSetSpeed(m_anims[1], 3.0f);
        break;
    }
    m_headCs = CsPool::csActivate();
    hierSetCsEpNode(m_headCs, m_headEp);
    m_headCs->drawMe = 0;
    m_headCs->epNode->id2 = Interactives::addInteractive(this);
    m_headCs->testCollision = 0;
    m_cs = m_headCs;
    m_stemCs = CsPool::csActivate();
    hierSetCsEpNode(m_stemCs, m_stemEp);
    m_stemCs->drawMe = 0;
    m_stemCs->epNode->id2 = m_headCs->epNode->id2;
    m_stemCs->testCollision = 0;
    hierSetSwitch(m_switch, 1);
    m_contact.init(m_anims[0]);
}

#ifdef NON_MATCHING
/* equivalent, 29/51 words; retail stores x in each branch and y/z through a pointer to m_pos */
void PlantBossHead::wakeUp(int idx)
{
    m_awake = 1;
    enterState(0xC);
    m_headCs->drawMe = 1;
    m_stemCs->drawMe = 1;
    m_stemCs->testCollision = 1;
    m_flags |= 8;
    {
        float x, y;
        if (idx == 0) {
            x = 0.0f;
            y = 33.884f;
        } else {
            x = idx == 1 ? -29.344f : 29.344f;
            y = -16.942f;
        }
        m_pos.x = x;
        m_pos.y = y;
        m_pos.z = 0.0f;
        vecCopy(&m_headCs->trans, &m_pos);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", wakeUp__13PlantBossHeadi);
#endif

void PlantBossHead::enterState(int s)
{
    if (m_state == 0xD || m_state == 0x63) {
        return;
    }
    m_state = s;
    switch (s) {
    case 0:
        animationTransitionInto(m_anims[m_state], 500.0f, 1, 1);
        m_stemCs->testCollision = 1;
        break;
    case 1:
        animationTransitionInto(m_anims[m_state], 500.0f, 1, 1);
        m_stemCs->testCollision = 1;
        m_recoiled = 0;
        m_hurt = 0;
        break;
    case 9:
    case 10:
    case 11:
        animationStart(m_anims[m_state], true);
        m_stemCs->testCollision = 1;
        break;
    case 12:
    case 15:
        animationStart(m_anims[m_state], true);
        m_flags |= 8;
        m_stemCs->testCollision = 1;
        break;
    case 2:
    case 3:
    case 4:
        animationStart(m_anims[m_state], true);
        m_flags &= ~8;
        m_stemCs->testCollision = 1;
        break;
    case 8:
        animationStart(m_anims[m_state], true);
        m_flags &= ~8;
        m_stemCs->testCollision = 0;
        break;
    case 5:
    case 6:
    case 7:
        animationStart(m_anims[m_state], true);
        break;
    case 13:
    case 14:
        animationTransitionInto(m_anims[m_state], 400.0f, 1, 1);
        m_flags = (m_flags | 8) & ~6;
        m_stemCs->testCollision = 1;
        break;
    case 16:
        m_stemCs->testCollision = 1;
        break;
    case 17:
        animationTransitionInto(m_anims[m_state], 400.0f, 1, 1);
        m_stemCs->testCollision = 1;
        break;
    case 18:
        m_sporeLaunched = 0;
        animationTransitionInto(m_anims[18], 500.0f, 1, 1);
        m_stemCs->testCollision = 1;
        break;
    }
}
#ifdef NON_MATCHING
extern float junk __asm__("junk.2582"); /* static of update: a chomp only bites while the animation is under it */
__asm__("#SNFIX_SMALL junk.2582");
float smoothEasyInAngle(float cur, float target, float rate, float eps);
void mathfRotationFromPointToPoint(_fvector *rot, _fvector *from, _fvector *to);
#define MONSTER_VEL_FIELD(m) ((_fvector *)((char *)(m) + 0x1E0))     /* MonsterDynamics velocity */
#define MONSTER_24C(m) (*(int *)((char *)(m) + 0x24C))
/* this->takeHit through the vtable (slot 0x8) */
#define HEAD_TAKEHIT(h, dir, dmg, id) \
    ((void (*)(void *, _fvector *, float, int))*(void **)((char *)(h)->m_vptr + 0xC))((char *)(h) + *(short *)((char *)(h)->m_vptr + 0x8), dir, dmg, id)

/* Turns the head towards player 1 at `rate` (yaw only). */
#define HEAD_FACE(rate)                                                                                             \
    do {                                                                                                            \
        float heading_ = mathfHeadingFromPointToPoint(&m_headCs->trans, &m->m_cs->trans);                            \
        m_headCs->rot.x = 0.0f;                                                                                     \
        m_headCs->rot.y = 0.0f;                                                                                     \
        m_headCs->rot.z = smoothEasyInAngle(m_headCs->rot.z, heading_, rate, 0.001f);                               \
        mathfRotMatrixRPH(m_headCs->mat, &m_headCs->rot);                                                           \
    } while (0)

/* Keeps player 1 in the mouth: moved by how far the pin is from its locator 0x809. */
#define HEAD_HOLD()                                                                                                 \
    do {                                                                                                            \
        FVECTOR p_;                                                                                                 \
        vecSub(&p_, &m_pinTrans, MONSTER_LOCATOR(m, 0x809));                                                       \
        vecAdd(&p_, &m->m_cs->trans, &p_);                                                                          \
        m->setTrans(p_);                                                                                            \
    } while (0)

/* 0 idle (hiss/growl now and then), 1 rises (then 2, 3 or 4 by distance: chomp near/mid/far, or recoil 17),
 * 2..4 chomp: bites player 1 within 100 of the pin before `junk` of the animation (a blocking player hurts the head
 * instead); 5..7 chew while holding, 8 spit (throws the player along the pin's motion), 9..12 retract, 13 dies,
 * 14/15 face the player, 17 recoils, 18 launches a spore at 80%. */
void PlantBossHead::update(void)
{
    Monster *m = game->m_lists.players[0];
    float pct, d;

    switch (m_state) {
    case 0:
        if (mathfManhatDist(&m_headCs->trans, &m->m_cs->trans) > 450.0f) {
            HEAD_FACE(0.05f);
        }
        if (m_24 > 0 && --m_24 == 0) {
            m_flags |= 8;
            m_stemCs->testCollision = 1;
        }
        switch (mathfRand(0, 0x118)) {
        case 0:
            m_sound.playPlantBossHissSound(&m_pinTrans);
            break;
        case 1:
            m_sound.playPlantBossGrowlSound(&m_pinTrans);
            break;
        }
        break;
    case 1:
        pct = animationGetCurrentPercent(m_anims[m_state]);
        {
            float heading = mathfHeadingFromPointToPoint(&m_headCs->trans, &m->m_cs->trans);

            m_holding = 0;
            m_headCs->rot.x = 0.0f;
            m_headCs->rot.y = 0.0f;
            m_headCs->rot.z = smoothEasyInAngle(m_headCs->rot.z, heading, 0.05f, 0.001f);
            mathfRotMatrixRPH(m_headCs->mat, &m_headCs->rot);
        }
        if (animationGetCurrentFrame(m_anims[m_state]) == 144.0f) {
            m_sound.playPlantBossHissSound(&m_pinTrans);
        }
        if (pct >= 0.99f) {
            if (m_recoiled != 0) {
                plantBoss.m_04 = 0x12C;
                enterState(0x11);
            } else {
                d = mathfManhatDist(&m_headCs->trans, &m->m_cs->trans);
                if (d < 450.0f) {
                    enterState(0);
                }
                if (d < 650.0f) {
                    enterState(2);
                } else if (d < 850.0f) {
                    enterState(3);
                } else {
                    enterState(4);
                }
            }
        }
        break;
    case 2:
    case 3:
    case 4: {
        int play;
        float heading = mathfHeadingFromPointToPoint(&m_headCs->trans, &m->m_cs->trans);

        pct = animationGetCurrentPercent(m_anims[m_state]);
        m_headCs->rot.x = 0.0f;
        m_headCs->rot.y = 0.0f;
        m_headCs->rot.z = smoothEasyInAngle(m_headCs->rot.z, heading, 0.1f, 0.001f);
        mathfRotMatrixRPH(m_headCs->mat, &m_headCs->rot);
        play = 0;
        if (animationGetCurrentFrame(m_anims[m_state]) == 20.0f) {
            play = m_holding == 0;
        }
        if (play) {
            m_sound.playPlantBossMissChompSound(&m_pinTrans);
        }
        play = 0;
        if (animationGetCurrentFrame(m_anims[m_state]) == 20.0f) {
            play = m_holding != 0;
        }
        if (play) {
            m_sound.playPlantBossChompSound(&m_pinTrans);
        }
        if (pct >= 1.0f) {
            if (m_holding != 0) {
                switch (m_state) {
                case 2:
                    enterState(5);
                    break;
                case 3:
                    enterState(6);
                    break;
                case 4:
                    enterState(7);
                    break;
                }
            } else {
                switch (m_state) {
                case 2:
                    enterState(9);
                    break;
                case 3:
                    enterState(10);
                    break;
                case 4:
                    enterState(11);
                    break;
                }
                switch (mathfRand(0, 2)) {
                case 0:
                    m_sound.playPlantBossHissSound(&m_pinTrans);
                    break;
                case 1:
                    m_sound.playPlantBossGrowlSound(&m_pinTrans);
                    break;
                }
            }
        }
        d = mathfManhatDist(&m_pinTrans, MONSTER_LOCATOR(m, 0x809));
        if (d < 150.0f) {
            m_stemCs->testCollision = 0;
        }
        if (d < 100.0f && pct < junk && m_holding == 0) {
            {
                ActuatorData a = GAME_ACTUATOR(0x1227E0);

                inputSetActuator(0, &a);
            }
            if (*m->m_state != 3) {
                m->takeDamage(m->m_health > 25.0f ? 25.0f : m->m_health - 1.0f, false, 0);
                Cameras::InitPlantBossCamera();
                if (m->m_dead == 0) {
                    m_holding = 1;
                    MONSTER_24C(m) = 0;
                    m->m_cs->rot.y = mathfRand(0, 1) != 0 ? 1.5707964f : -1.5707964f;
                    mathfRotMatrixRPH(m->m_cs->mat, &m->m_cs->rot);
                }
                Cameras::StartShake(50.0f);
            } else {
                HitEvent *e = HitEvent::create();
                FVECTOR dir;

                vecSub(&dir, &m_pinTrans, &m->m_cs->trans);
                e->type = 2;
                e->pos = &m->m_cs->trans;
                HEAD_TAKEHIT(this, &dir, 1.0f, m->m_id);
                switch (m_state) {
                case 2:
                    enterState(9);
                    break;
                case 3:
                    enterState(10);
                    break;
                case 4:
                    enterState(11);
                    break;
                }
            }
        }
        if (m_holding != 0) {
            HEAD_HOLD();
            m->m_attacksEnabled = 0;
        }
        break;
    }
    case 5:
    case 6:
    case 7:
        pct = animationGetCurrentPercent(m_anims[m_state]);
        m_sound.updateChompSound(&m_pinTrans);
        if (pct >= 1.0f) {
            if (m_holding != 0) {
                enterState(8);
            } else {
                enterState(0);
                m_sound.terminateChompSound();
            }
        }
        m->m_unk1E8 = 0.0f;
        HEAD_HOLD();
        m->m_attacksEnabled = 0;
        m_24 = 0;
        break;
    case 8:
        pct = animationGetCurrentPercent(m_anims[8]);
        if (animationGetCurrentFrame(m_anims[8]) == 100.0f) {
            m_sound.playPlantBossSpitSound(&m_pinTrans);
            m_sound.terminateChompSound();
        }
        if (pct < 0.714f) {
            HEAD_HOLD();
        } else if (m_24 == 0) {
            m_24 = 0x78;
            MONSTER_24C(m) = 1;
            vecSub(MONSTER_VEL_FIELD(m), &m_pinTrans, &m_prevPin);
            m->m_attacksEnabled = 1;
            m->knockBack(*(_fvector *)m_pinMat[1], -200.0f, 800.0f);
            if (CAMERA_POV(0) == 3) {
                if (m->m_state[1] & 0x10) {
                    Cameras::SetCameraPOV(0, (Camera::CameraPOV)5);
                } else {
                    Cameras::SetCameraPOV(0, (Camera::CameraPOV)0);
                }
                CAMERA_D9C(0) = 1;
            }
        } else if (pct >= 1.0f) {
            m_stemCs->testCollision = 0;
            enterState(0);
            plantBoss.m_04 = 0x3C;
        }
        break;
    case 9:
    case 10:
    case 11:
    case 12:
        if (animationGetCurrentPercent(m_anims[m_state]) >= 1.0f) {
            enterState(0);
            plantBoss.m_04 = 0x78;
        }
        m_stemCs->testCollision = 1;
        break;
    case 13:
        if (animationGetCurrentPercent(m_anims[m_state]) >= 1.0f) {
            m_state = 0x63;
            m_awake = 0;
            if (++numDeadHeads < 3) {
                plantBoss.m_04 = 1;
            }
        }
        break;
    case 17:
        if (animationGetCurrentPercent(m_anims[m_state]) >= 0.99f) {
            plantBoss.m_04 = 1;
        }
        break;
    case 14:
        HEAD_FACE(0.05f);
        break;
    case 15:
        pct = animationGetCurrentPercent(m_anims[15]);
        HEAD_FACE(0.05f);
        if (pct >= 1.0f) {
            enterState(0);
        }
        switch (mathfRand(0, 0xB4)) {
        case 0:
            m_sound.playPlantBossHissSound(&m_pinTrans);
            break;
        case 1:
            m_sound.playPlantBossGrowlSound(&m_pinTrans);
            break;
        }
        break;
    case 18:
        pct = animationGetCurrentPercent(m_anims[18]);
        d = mathfManhatDist(&m_headCs->trans, &m->m_cs->trans);
        if (animationGetCurrentFrame(m_anims[18]) == 124.0f) {
            m_sound.playPlantBossSporeLaunchSound(&m_pinTrans);
        }
        if (d > 450.0f) {
            HEAD_FACE(0.05f);
        }
        if (pct >= 0.8f && m_sporeLaunched == 0) {
            FVECTOR rot;

            mathfRotationFromPointToPoint(&rot, &m_pinTrans, MONSTER_LOCATOR(m, 0x809));
            m_sporeLaunched = 1;
            game->getWeapons()->CreateStaticWeapon((Weapon::WeaponType)0x2A, m_stemCs->epNode->id2, m->m_id, &m_pinTrans,
                                                   &rot, 0);
        }
        if (pct >= 1.0f) {
            enterState(0);
            if (numDeadHeads != 0) {
                plantBoss.m_04 = numDeadHeads == 1 ? 0x1E : 2;
            }
        }
        break;
    }
    m_prevPin = m_pinTrans;
    vecApplyRows(&m_headCs->trans, m_headCs->mat, &m_pos);
    m_stemCs->trans = m_pinTrans;
    matCopy(m_stemCs->mat, m_pinMat);
    hdReparentCsGrid(m_stemCs);
    m_contact.update();
    if (m_contact.m_ch[m_contact.m_cur][5] > 0.5f && m_contact.m_ch[m_contact.m_cur == 0][5] < 0.5f) {
        m_sound.playPlantBossHeadDeadSound(&m_pinTrans);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", update__13PlantBossHead);
#endif
/* Hurts only while exposed (state 1 past half its animation, or 2..4); hits under 0.11 count only as shocks, and a
 * monster with 0xEC set deals 1000. Dying (state 13) on the third head ends the fight (fade, plantBoss.m_08 = 885);
 * 10 damage makes it recoil (state 17). Every hit flashes animation 16 as an additive layer. */
#ifdef NON_MATCHING
/* equivalent, 3/169 words (0x280 vs 0x2a4) untuned; retail reaches D_006F8C6C gp-relative here only */
void PlantBossHead::takeHit(_fvector *dir, float dmg, int attacker)
{
    Monster *m;
    int exposed;

    if (m_holding != 0) {
        return;
    }
    if (dmg < 0.11f && s_takeHitInfo.source != 5) {
        return;
    }
    m = 0;
    if (attacker != 0 && Interactives::getInteractive(attacker)->m_kind == 1) {
        m = (Monster *)Interactives::getInteractive(attacker);
    }
    if (m != 0 && (signed char)m->m_unkEC != 0) {
        dmg = 1000.0f;
    }
    exposed = 0;
    if (m_state == 1 && animationGetCurrentPercent(m_anims[1]) > 0.5f) {
        exposed = 1;
    } else if (m_state == 2 || m_state == 3 || m_state == 4) {
        exposed = 1;
    }
    if (!exposed) {
        return;
    }
    m_health -= dmg;
    plantBoss.m_10 = 10;
    if (m_health <= 0.0f) {
        if (m_holding != 0) {
            m_health = 1.0f;
        } else {
            m_health = 0.0f;
            if (numDeadHeads >= 2) {
                plantBoss.m_08 = 0x375;
                plantBoss.m_0C = 1;
                D_006F8C6C = 0;
                game->fadeOutAndIn(4);
            }
            enterState(0xD);
            m_sound.playPlantBossDeathSound(&m_pinTrans);
        }
    } else {
        m_hurt += dmg;
        if (m_hurt >= 10.0f) {
            m_recoiled = 1;
            plantBoss.m_04 = 0x12C;
            enterState(0x11);
        }
    }
    {
        AnimBlend *b = AnimBlendPool::alloc();

        if (b != 0) {
            b->init(m_anims[16]);
            b->m_node.blendType = BLEND_ADDITIVE;
            b->m_node.priority = 999;
            b->runAndDealloc(0.0f, 0.0f);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", takeHit__13PlantBossHeadP8_fvectorfi);
#endif

void PlantBossRoot::initBefore(void)
{
    m_state = 0;
    m_18 = 0;
}

void PlantBossRoot::initAfter(int idx)
{
    int i;

    for (i = 0; i < 5; i++) {
        animationGetHandle(&m_anims[i], 0x271E + idx, 0, i);
        animationSetSpeed(m_anims[i], mathfRandf(1.02f, 1.38f));
    }
    m_rootCs = CsPool::csActivate();
    hierSetCsEpNode(m_rootCs, m_ep);
    m_rootCs->drawMe = 0;
    m_rootCs->epNode->id2 = Interactives::addInteractive(this);
    m_rootCs->testCollision = 0;
    m_flags |= 8;
    m_cs = m_rootCs;
    m_sound.initPlantBossRootSound();
}

#ifdef NON_MATCHING
/* Pops up next to player 1 (one of ten offsets plus the player's velocity), pushed out to 500..1075 from the centre,
 * dropped onto the ground and turned randomly. */
void PlantBossRoot::wakeUp(int idx)
{
    FVECTOR zero = {0.0f, 0.0f, 0.0f, 0.0f};
    FVECTOR offsets[10] = {
        {0.0f, 0.0f, 0.0f, 0.0f},     {0.0f, 150.0f, 0.0f, 0.0f},   {0.0f, -150.0f, 0.0f, 0.0f},
        {-106.0f, 106.0f, 0.0f, 0.0f}, {106.0f, -106.0f, 0.0f, 0.0f}, {106.0f, 106.0f, 0.0f, 0.0f},
        {-106.0f, -106.0f, 0.0f, 0.0f}, {150.0f, 0.0f, 0.0f, 0.0f},  {-150.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    };
    FVECTOR dir;
    Monster *m = game->m_lists.players[0];
    _fvector *t;
    float d, inv;

    enterState(1);
    m_18 = 0;
    m_rootCs->drawMe = 1;
    m_rootCs->testCollision = 1;
    m_rootCs->trans.x = m->m_cs->trans.x + offsets[idx].x + MONSTER_VEL(m)[0];
    m_rootCs->trans.y = m->m_cs->trans.y + offsets[idx].y + MONSTER_VEL(m)[1];
    d = mathfManhatDist2D(&m_rootCs->trans, &zero);
    if (d < 500.0f) {
        t = &m_rootCs->trans;
        inv = 1.0f / eeSqrtf(t->x * t->x + t->y * t->y + t->z * t->z);
        dir.x = t->x * inv;
        dir.y = t->y * inv;
        dir.z = t->z * inv;
        vecScale(t, &dir, 500.0f);
    }
    if (1075.0f < d) {
        t = &m_rootCs->trans;
        inv = 1.0f / eeSqrtf(t->x * t->x + t->y * t->y + t->z * t->z);
        dir.x = t->x * inv;
        dir.y = t->y * inv;
        dir.z = t->z * inv;
        vecScale(t, &dir, 1075.0f);
    }
    m_rootCs->trans.z = 100.0f;
    m_rootCs->trans.z -= hdHatTest(&m_rootCs->trans, &dir, 25.0f, false, 4096.0f);
    m_rootCs->rot.z = mathfRandf(0.0f, 6.28f);
    mathfRotMatrixRPH(m_rootCs->mat, &m_rootCs->rot);
    hdReparentCsGrid(m_rootCs);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", wakeUp__13PlantBossRooti);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", D_006EDCB0);
#endif

void PlantBossRoot::enterState(int s)
{
    if (m_state == s) {
        return;
    }
    m_state = s;
    switch (s) {
    case 0:
        animationTransitionInto(m_anims[0], 100.0f, 1, 1);
        break;
    case 1:
        animationTransitionInto(m_anims[1], 100.0f, 1, 1);
        m_flags |= 8;
        break;
    case 2:
        animationTransitionInto(m_anims[2], 100.0f, 1, 1);
        break;
    case 3:
        animationTransitionInto(m_anims[3], 100.0f, 1, 1);
        m_flags |= 8;
        break;
    case 4:
        animationTransitionInto(m_anims[4], 100.0f, 1, 1);
        m_flags &= ~8;
        break;
    }
}
/* retail pads the jump table of enterState to four words */
__asm__(".section .rodata\n\t.word 0\n\t.word 0\n\t.word 0\n\t.text");

/* 1, 2: grows, then strikes (3); 3: hurts player 1 within 75 of the tip (three hits retract it, as does the player
 * getting more than 500 away); 4: retracts and hides. */
#ifdef NON_MATCHING
/* equivalent, 4/204 words untuned */
void PlantBossRoot::update(void)
{
    Monster *m = game->m_lists.players[0];

    switch (m_state) {
    case 1:
        if (animationGetCurrentPercent(m_anims[1]) >= 0.99f) {
            enterState(3);
        }
        m_sound.updatePlantBossRootSound(&m_tip);
        break;
    case 2:
        if (animationGetCurrentPercent(m_anims[2]) >= 0.99f) {
            enterState(3);
        }
        m_sound.updatePlantBossRootSound(&m_tip);
        break;
    case 3:
        if (mathfManhatDist(&m_tip, &m->m_cs->trans) < 75.0f) {
            m->takeDamage(game->m_matchMode * 0.5f + 1.0f, false, 0);
            m->takeAdditiveRecoil(m_push, 1.0f);
            particleCreateFx(MONSTER_LOCATOR(m, 0x809), 0x4E, 1.0f, 0, 0.0f);
            particleCreateFx(MONSTER_LOCATOR(m, 0x809), 0x4D, 1.0f, 0, 0.0f);
            particleCreateFx(MONSTER_LOCATOR(m, 0x809), 0x4F, 1.0f, 0, 0.0f);
            if (++m_18 >= 3) {
                enterState(4);
            }
            {
                ActuatorData a = GAME_ACTUATOR(0x1227A0);

                inputSetActuator(0, &a);
            }
        }
        if (mathfManhatDist(&m_rootCs->trans, &m->m_cs->trans) > 500.0f) {
            enterState(4);
        }
        m_sound.updatePlantBossRootSound(&m_tip);
        break;
    case 4:
        if (animationGetCurrentPercent(m_anims[4]) >= 0.99f) {
            m_rootCs->drawMe = 0;
            m_rootCs->testCollision = 0;
            m_sound.terminatePlantBossRootSound();
        } else {
            m_sound.updatePlantBossRootSound(&m_tip);
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", update__13PlantBossRoot);
#endif

void PlantBossRoot::takeHit(_fvector *dir, float dmg, int attacker)
{
    if (m_state == 3 && s_takeHitInfo.type == 3) {
        enterState(4);
    }
}

void PlantBossBase::initBefore(void)
{
    m_state = 0;
    m_18 = 0;
}

void PlantBossBase::initAfter(void)
{
    int i;

    for (i = 0; i < 10; i++) {
        animationGetHandle(&m_anims[i], 0x2728, 0, i);
    }
    m_baseCs = CsPool::csActivate();
    hierSetCsEpNode(m_baseCs, m_ep);
    m_baseCs->drawMe = 0;
    m_baseCs->epNode->id2 = Interactives::addInteractive(this);
    m_baseCs->testCollision = 0;
    m_cs = m_baseCs;
    m_sound.initPlantBossSound();
}

void PlantBossBase::wakeUp(void)
{
    enterState(1);
    m_baseCs->drawMe = 1;
    m_baseCs->testCollision = 1;
    m_flags |= 8;
    {
        _fvector *t = &m_baseCs->trans;
        t->x = 0.0f;
        t->y = 0.0f;
        t->z = 0.0f;
    }
    hdReparentCsGrid(m_baseCs);
}

void PlantBossBase::enterState(int s)
{
    if (plantBoss.m_0C == 1 && plantBoss.m_08 > 0) {
        return;
    }
    m_state = s;
    switch (s) {
    case 0:
        animationTransitionInto(m_anims[0], 500.0f, 1, 1);
        break;
    case 1:
        animationTransitionInto(m_anims[1], 500.0f, 1, 1);
        break;
    case 2:
        animationTransitionInto(m_anims[2], 250.0f, 1, 1);
        break;
    case 3:
        animationTransitionInto(m_anims[3], 500.0f, 1, 1);
        break;
    case 4:
        animationTransitionInto(m_anims[4], 500.0f, 1, 1);
        m_sound.playPlantBossEmergeSound();
        break;
    case 5:
        animationTransitionInto(m_anims[5], 500.0f, 1, 1);
        m_sound.playPlantBossHideSound();
        break;
    case 6:
        animationTransitionInto(m_anims[6], 100.0f, 1, 1);
        break;
    case 7:
        animationTransitionInto(m_anims[7], 500.0f, 1, 1);
        break;
    case 8:
        animationTransitionInto(m_anims[8], 100.0f, 1, 1);
        break;
    case 9:
        animationTransitionInto(m_anims[9], 100.0f, 1, 1);
        break;
    }
}
/* retail pads the jump table of enterState to four words */
__asm__(".section .rodata\n\t.word 0\n\t.word 0\n\t.text");

#ifdef NON_MATCHING
extern int playerThrown __asm__("playerThrown.2619"); /* static of update: the smack of states 8/9 lands once */
__asm__("#SNFIX_SMALL playerThrown.2619");
#endif

/* 1, 4: back to idle (0) when the animation ends; 3: to 7 (exposed); 5: hidden (3), plantBoss.m_04 = 1;
 * 6: to 7, or 0 when plantBoss.m_14 is clear; 8, 9: the smack that throws player 1 (500 back, 800 up, 10 damage). */
#ifdef NON_MATCHING
/* equivalent, 7/321 words untuned; retail shares the tail that clears playerThrown */
void PlantBossBase::update(void)
{
    Monster *m = game->m_lists.players[0];
    float pct;

    switch (m_state) {
    case 1:
        if (animationGetCurrentPercent(m_anims[1]) >= 0.99f) {
            enterState(0);
        }
        break;
    case 3:
        if (animationGetCurrentPercent(m_anims[3]) >= 0.99f) {
            enterState(7);
        }
        break;
    case 4:
        if (animationGetCurrentPercent(m_anims[4]) >= 0.99f) {
            enterState(0);
        }
        break;
    case 5:
        if (animationGetCurrentPercent(m_anims[5]) >= 0.99f) {
            enterState(3);
            plantBoss.m_04 = 1;
        }
        break;
    case 6:
        if (animationGetCurrentPercent(m_anims[6]) >= 0.99f) {
            if (plantBoss.m_14 != 0) {
                enterState(7);
            } else {
                enterState(0);
            }
        }
        break;
    case 8:
        pct = animationGetCurrentPercent(m_anims[8]);
        if (pct >= 0.2f && !playerThrown) {
            FVECTOR dir;
            _fvector *t = &m->m_cs->trans;
            float inv;

            playerThrown = 1;
            m_sound.playPlantBossCabbageSmack();
            inv = 1.0f / eeSqrtf(t->x * t->x + t->y * t->y + t->z * t->z);
            dir.x = t->x * inv;
            dir.y = t->y * inv;
            dir.z = t->z * inv;
            m->knockBack(dir, 500.0f, 800.0f);
            m->takeDamage(10.0f, false, 0);
            {
                ActuatorData a = GAME_ACTUATOR(0x1227E0);

                inputSetActuator(0, &a);
            }
        }
        if (pct >= 0.99f) {
            enterState(7);
            playerThrown = 0;
        }
        break;
    case 9:
        pct = animationGetCurrentPercent(m_anims[9]);
        if (pct >= 0.5f && !playerThrown) {
            FVECTOR dir;
            _fvector *t = &m->m_cs->trans;
            float inv;

            playerThrown = 1;
            m_sound.playPlantBossCabbageSmack();
            inv = 1.0f / eeSqrtf(t->x * t->x + t->y * t->y + t->z * t->z);
            dir.x = t->x * inv;
            dir.y = t->y * inv;
            dir.z = t->z * inv;
            m->knockBack(dir, 500.0f, 800.0f);
            m->takeDamage(10.0f, false, 0);
            {
                ActuatorData a = GAME_ACTUATOR(0x1227E0);

                inputSetActuator(0, &a);
            }
        }
        if (pct >= 0.99f) {
            enterState(0);
            playerThrown = 0;
        }
        break;
    case 0:
    case 7:
        playerThrown = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", update__13PlantBossBase);
#endif

void PlantBossBase::takeHit(_fvector *dir, float dmg, int attacker)
{
    if (s_takeHitInfo.type == 0) {
        if (m_state == 7) {
            enterState(8);
        } else if (m_state == 0) {
            enterState(9);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", func_00186928);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", func_00186938);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", func_00186960);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", _vt$13PlantBossBase);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", _vt$13PlantBossRoot);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", _vt$13PlantBossHead);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", __tf13PlantBossHead);

FVECTOR *getPinTrans__13PlantBossHead(PlantBossHead *self) __asm__("getPinTrans__13PlantBossHead");
FVECTOR *getPinTrans__13PlantBossHead(PlantBossHead *self)
{
    return &self->m_pinTrans;
}

INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", __tf13PlantBossRoot);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", __tf13PlantBossBase);
INCLUDE_ASM("asm/nonmatchings/game/PlantBoss", func_00186A68);
