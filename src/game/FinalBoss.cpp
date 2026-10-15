#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/hit_event.h"
#include "game/anim_blend.h"
#include "game/ai_support.h"
#include "game/monster_state.h"
#include "game/streaming_sound.h"
#include "cs_pool.h"
#include "vecmath.h"
#include "hd.h"

/* The final boss (level 11): phase 0 is the big robot ("Mecha"), phase 1 the jelly creature, phase 2 the last monster
 * fight, phase 3 the ending (fade out, ending cinema, back to the shell). One global instance, `finalBoss`. Virtual in
 * retail (DbInteractive: vptr at 0x10, takeHit and getPinTrans in the vtable); the vtable, __tf and the static
 * initialization stay asm. */

class DbInteractive {
public:
    int m_kind;               /* 0x00 */
    unsigned short m_flags;   /* 0x04 */
    char pad6[6];
    _cs *m_cs;                /* 0x0C */
    void *m_vptr;             /* 0x10 */
};

class Interactives {
public:
    static int addInteractive(DbInteractive *p);
    static void dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId);
};

class ActionDispatch {
public:
    static void callScriptFromId(unsigned short id1, unsigned short id2, int fadeOut);
};

class AnimContact {
public:
    char pad[0x50];
    void init(_animHandle &h);
    void update(void);
};

struct _pshadowblockerlist;
_pshadowblockerlist *psBlockerAdd(_cs *cs, float litDist);
void psBlockerSetWsOrigin(_pshadowblockerlist *b, _fvector *origin);
void psBlockerSetRadius(_pshadowblockerlist *b, float *radius);
float psBlockerSetLodRange(_pshadowblockerlist *b, float range);
void animationInitModifierBlends(_animCharInstance *ci);
void animationResetCharacter(_animHandle &h);
void rtReturnToShell(int a, int b);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rot);
float mathfManhatDist2D(_fvector *a, _fvector *b);
void hierSetSwitch(_hierswitch *sw, int which);
int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);
void electricArc(_fvector *from, _fvector *to, unsigned colorA, unsigned colorB, float a, float b, float c, float d, int n,
                 unsigned seed, unsigned e);
extern "C" int rand(void);
void boundEulerAngle(float *a);
float smoothEasyInAngleTC(float cur, float target, float rate, float eps);
void mathfRotationFromVector(_fvector *rot, _fvector *v);
void mathfRotationFromPointToPoint(_fvector *rot, _fvector *from, _fvector *to);
float mathfPitchFromVector(_fvector *v);
float mathfPitchFromPointToPoint(_fvector *from, _fvector *to);
int hdLosCSTest(_fvector *from, _fvector *to, _hdResult *res, int csId);
void animationSetToPercent(_animHandle h, float p, int b);
struct ActuatorData {
    int w[8];
};
void inputSetActuator(int pad, ActuatorData *a);

class HurtHistory {
public:
    char pad[4];
    float creditHit(int id, float amount);
};

class StateUltraTazer {
public:
    int transitionOK(void);
};

/* Monster parts the boss drives that game.h does not name yet */
#define MONSTER_NAV(m) ((AiNavigator *)((char *)(m) + 0x560))        /* the Ai's navigator (m_ai + 0x80) */
#define MONSTER_AI_7C4(m) (*(int *)((char *)(m) + 0x7C4))             /* m_ai + 0x2E4 */
#define MONSTER_SOUND(m) ((MonsterSound *)(m)->m_sound)
#define MONSTER_ULTRA_TAZER(m) ((MonsterState *)((char *)(m) + 0x11130))
#define MONSTER_IDLE(m) ((MonsterState *)((char *)(m) + 0x7984))
#define MONSTER_SPECIAL(m) (*(signed char *)((char *)(m) + 0xF3))     /* m_specialWeapon, written as a flag here */
/* locator `id` of a monster (0x800 + n; the feet are 0x804, 0x805, 0x816, 0x817, the tazer tip 0x809) */
#define MONSTER_LOCATOR(m, id) ((_fvector *)((char *)(m) + (id) * 16 - 0x4260))
/* vtable entry 0x28 of a MonsterState: transition feasible (vptr at 0x10, entries {delta, 0, fn}) */
#define STATE_FEASIBLE(st) (((int (*)(void *))*(void **)((char *)(st)->vptr + 0x2C))((char *)(st) + *(short *)((char *)(st)->vptr + 0x28)))

class FinalBoss : public DbInteractive {
public:
    float m_health;           /* 0x14: 75 per match-mode step + 100 (initBefore) */
    int m_active;             /* 0x18: 1 while the boss level runs; 0 sends it back */
    int m_phase;              /* 0x1C: 0 mecha, 1 jelly, 2 last monster fight, 3 ending */
    int m_state;              /* 0x20: state inside the phase (7 = takes hits, 9 = dying) */
    int m_state2;             /* 0x24 */
    int m_timer;              /* 0x28: fields */
    int m_attract;            /* 0x2C: jelly phase: attractPts index of the pool the jelly heads for */
    int m_flashFields;        /* 0x30: hit flash (red) while > 0 */
    float m_laser;            /* 0x34: mecha laser length, 0..1 of 4000 (drives m_anim3) */
    float m_heading;          /* 0x38: mecha turret heading (smoothed toward the player) */
    float m_prevHeading;      /* 0x3C */
    FinalBossSound m_sound;   /* 0x40 */
    FVECTOR m_dest;           /* 0x50: mecha phase: where it walks to (y follows the player, clamped to attractPts) */
    FVECTOR m_pinTrans;       /* 0x60: aim point (getPinTrans) */
    _cs *m_mechaCs;           /* 0x70: same cs as m_cs */
    Monster *m_monster74;     /* 0x74 */
    Monster *m_monster78;     /* 0x78 */
    Monster *m_monster7C;     /* 0x7C */
    AnimPappy m_pappy;        /* 0x80 */
    _animHandle m_anims[13];  /* 0xB0: hier 0x27A6 animations 0..12 */
    _animHandle m_anims2[3];  /* 0x180: hier 0x27A7 animations 0..2 */
    _animHandle m_anim3;      /* 0x1B0: hier 0x27A9 animation 0 */
    AnimBlend m_blend1;       /* 0x1C0 */
    AnimBlend m_blend2;       /* 0x228 */
    AnimContact m_contact;    /* 0x290 */
    HurtHistory m_hurt;       /* 0x2E0: damage the mecha laser deals to destructibles */
    char pad2E4[0x360 - 0x2E4];
    int m_360;                /* 0x360: cleared when the mecha starts firing */
    _hierhead *m_ep[4];       /* 0x364: ep nodes 0x27A6..0x27A9 */
    char pad374[0x380 - 0x374];
    FVECTOR m_loc27AAPos;     /* 0x380: setLocator 0x27AA */
    FMATRIX_16 m_loc27AAMat;  /* 0x390 */
    int m_splashTimer[4];     /* 0x3D0: fields until the next splash (jelly) or dust (mecha) of each foot (locators 0x804, 0x805, 0x816, 0x817) */
    FVECTOR m_feet[4];        /* 0x3E0: locators 0x804, 0x805, 0x816, 0x817 (mecha feet) */
    _hierhead *m_ep27AB;      /* 0x420 */
    char pad424[0x480 - 0x424];
    _hierhead *m_ep27AD;      /* 0x480 */
    char pad484[0x490 - 0x484];

    void initBefore(void);
    void initAfter(void);
    void update(void);
    void updateMecha(void);
    void updateJelly(void);
    void takeHit(_fvector *dir, float dmg, int attackerId);
    void addEpNode(_hierhead *h);
    void setLocator(int id, float (&m)[4][4], _fvector *pos);
};
typedef char _size_FinalBoss[sizeof(FinalBoss) == 0x490 ? 1 : -1];

extern FinalBoss finalBoss;
extern _fvector attractPts[2];
extern float mechaShadowRadius;
__asm__("#SNFIX_SMALL mechaShadowRadius");
extern int moviePlaying;
extern int movieAborted;
extern HitEvent s_takeHitInfo __asm__("_8HitEvent$s_takeHitInfo");
extern int s_endingCinema __asm__("D_006F8BF0");   /* the ending cinema was started (phase 3) */
extern int s_fadedOut __asm__("D_006F8BF4");       /* phase 3 faded out */
extern int s_toggleCount __asm__("toggleCount.2484"); /* static int in update: debug kill-boss repeat delay */
__asm__("#SNFIX_SMALL toggleCount.2484");
extern float s_smoothFactor __asm__("smoothFactor.2488"); /* static in updateMecha: turret turn rate */
__asm__("#SNFIX_SMALL smoothFactor.2488");
extern float s_degreeOffset __asm__("degreeOffset.2489"); /* static in updateMecha: smoothed toward 0 and never read */
__asm__("#SNFIX_SMALL degreeOffset.2489");
extern float s_inc __asm__("inc.2490");               /* static in updateMecha: laser push per field (5) */
__asm__("#SNFIX_SMALL inc.2490");
extern float s_laserDamage __asm__("D_006F8BEC");     /* laser damage per field, by match mode */
__asm__("#SNFIX_SMALL D_006F8BEC");
extern float bossMph;                                  /* mecha walk speed (75) */
__asm__("#SNFIX_SMALL bossMph");

#ifdef NON_MATCHING
/* 18/31 words: the 0x420 store is scheduled before the others instead of in the jr delay slot */
void FinalBoss::initBefore(void)
{
    m_health = (float)game->m_matchMode * 75.0f + 100.0f;
    m_active = 1;
    m_timer = 0x762;
    m_ep27AD = 0;
    m_phase = 0;
    m_state = 0;
    m_state2 = 0;
    m_mechaCs = 0;
    m_monster74 = 0;
    m_monster78 = 0;
    m_monster7C = 0;
    m_ep[0] = 0;
    m_ep[1] = 0;
    m_ep[2] = 0;
    m_ep[3] = 0;
    m_ep27AB = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", initBefore__9FinalBoss);
#endif

#ifdef NON_MATCHING
/* 185/201 words: saved-register allocation and the AnimPappy offset stores (retail via &m_pappy) */
void FinalBoss::initAfter(void)
{
    int i;

    for (i = 0; i < 13; i++)
        animationGetHandle(&m_anims[i], 0x27A6, 0, i);
    animationSetSpeed(m_anims[4], 2.0f);
    for (i = 0; i < 3; i++)
        animationGetHandle(&m_anims2[i], 0x27A7, 0, i);
    animationStart(m_anims2[1], true);
    animationGetHandle(&m_anim3, 0x27A9, 0, 0);
    animationInitModifierBlends(m_anims[0].ci);
    m_contact.init(m_anims[0]);
    m_pappy.init(m_anims[0]);
    m_pappy.m_offset.x = 0.0f;
    m_pappy.m_offset.y = 0.0f;
    m_pappy.m_offset.z = 141.964f;
    m_mechaCs = CsPool::csActivate();
    hierSetCsEpNode(m_mechaCs, m_ep[0]);
    m_mechaCs->drawMe = 1;
    m_mechaCs->epNode->id2 = Interactives::addInteractive(this);
    m_mechaCs->testCollision = 1;
    m_flags |= 0xE;
    vecCopy(&m_mechaCs->trans, &attractPts[0]);
    m_cs = m_mechaCs;
    {
        _pshadowblockerlist *b = psBlockerAdd(m_mechaCs, 8.0f);
        psBlockerSetWsOrigin(b, &m_mechaCs->trans);
        psBlockerSetRadius(b, &mechaShadowRadius);
        psBlockerSetLodRange(b, 20000.0f);
    }
    {
        AnimBlend *b = &m_blend1;
        b->init(m_anims[11]);
        b->m_node.blendType = 4;
        b->m_node.priority = 999;
        b->rampIn(200.0f);
        b->setSpeed(0.0f);
        b->setPercent(0.5f);
        b = &m_blend2;
        b->init(m_anims[12]);
        b->m_node.blendType = 4;
        b->m_node.priority = 999;
        b->rampIn(200.0f);
        b->setSpeed(0.0f);
        b->setPercent(0.5f);
    }
    ActionDispatch::callScriptFromId(0x226A, 0, 1);
    m_sound.initFinalBossSound(this);
    s_endingCinema = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", initAfter__9FinalBoss);
#endif

#ifdef NON_MATCHING
/* 247/261 words: v0/v1 swapped while zeroing the last monster cs trans/rot */
void FinalBoss::update(void)
{
    Monster *p;

    if (moviePlaying && movieAborted && m_timer >= 0x20)
        m_timer = 0x1F;

    if (--m_flashFields > 0) {
        m_mechaCs->color.chan.r = 0xFF;
        m_mechaCs->color.chan.g = 0;
        m_mechaCs->color.chan.b = 0;
    } else {
        m_mechaCs->color.chan.r = (int)smoothEasyIn((float)m_mechaCs->color.chan.r, 128.0f, 0.25f, 0.001f);
        m_mechaCs->color.chan.g = (int)smoothEasyIn((float)m_mechaCs->color.chan.g, 128.0f, 0.25f, 0.001f);
        m_mechaCs->color.chan.b = (int)smoothEasyIn((float)m_mechaCs->color.chan.b, 128.0f, 0.25f, 0.001f);
    }
    m_mechaCs->colorQuad.fVec[0] = (float)m_cs->color.chan.r * (1.0f / 128.0f);
    m_mechaCs->colorQuad.fVec[1] = (float)m_cs->color.chan.g * (1.0f / 128.0f);
    m_mechaCs->colorQuad.fVec[2] = (float)m_cs->color.chan.b * (1.0f / 128.0f);

    if (m_phase == 0) {
        updateMecha();
    } else if (m_phase == 1) {
        updateJelly();
    } else if (m_phase == 2) {
        p = m_monster78;
        if (p->m_dead && p->m_state == (int *)((char *)p + 0x8450)) {
            m_timer = 0x726;
            m_phase = 3;
            game->fadeOutAndIn(4);
            s_endingCinema = 0;
        }
    } else {
        m_timer -= timerGetFieldsLastFrame();
        if (m_timer <= 0) {
            finalBoss.m_active = 0;
            rtReturnToShell(0, 0);
        }
        if (m_timer <= 0x80 && !s_fadedOut) {
            game->fadeOut(1);
            s_fadedOut = 1;
        }
        if (m_timer <= 0x708 && !s_endingCinema) {
            _fvector *v;
            _cs *cs;

            animationResetCharacter(*(_animHandle *)((char *)m_monster74 + 0x1D10));
            s_endingCinema = 1;
            s_fadedOut = 0;
            ActionDispatch::callScriptFromId(0x226D, 0, 0);
            STREAMING_SOUND()->startCinemaMusicFromId(0x226D);
            v = &m_monster78->m_cs->trans;
            v->x = 0.0f;
            v->y = 0.0f;
            v->z = 0.0f;
            v = &m_monster78->m_cs->rot;
            v->x = 0.0f;
            v->y = 0.0f;
            v->z = 0.0f;
            cs = m_monster78->m_cs;
            mathfRotMatrixRPH(cs->mat, &cs->rot);
        }
        m_monster7C->cleanUpForMovie();
    }

    /* debug: byte 0xFD of player 1 kills the current phase (once per 60 updates) */
    s_toggleCount--;
    p = game->m_monsters[0];
    if (*((signed char *)p + 0xFD) && s_toggleCount <= 0) {
        s_toggleCount = 60;
        *((signed char *)p + 0xFD) = 0;
        if (m_phase == 0) {
            m_health = 0.0f;
            animationTransitionInto(m_anims[8], 50.0f, 1, 1);
            m_state = 9;
            m_timer = 0x708;
            s_endingCinema = 0;
        } else if (m_phase == 1) {
            m_monster74->takeDamage(1000.0f, true, 0);
        } else {
            m_monster78->takeDamage(1000.0f, true, 0);
            finalBoss.m_active = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", update__9FinalBoss);
#endif

/* Phase 0: the big robot. It turns its turret toward the player (m_monster7C), walks along y between the two attractPts,
 * stops, fires a laser for 240 fields (damages the player, pushes him, credits hits to destructibles), powers down
 * its shield (state 7: the only one where takeHit counts) and starts again. States: 0 intro cinema, 1 walk in,
 * 2 walk to m_dest, 3 get ready to fire, 4 firing, 5 laser off, 6 power down, 7 vulnerable, 8 shield up, 9 dying
 * (ending cinema 0x226B, then phase 1 with the jelly m_monster74). */
#ifdef NON_MATCHING
/* 183/1303 words: stack slots and saved registers differ throughout (locals laid out differently, animationIsRunning tested as int instead of bool), same blocks and calls */
void FinalBoss::updateMecha(void)
{
    FVECTOR rot;
    FVECTOR dir;
    FVECTOR target;
    FVECTOR spark;
    int i;

    if (m_monster7C && m_state) {
        int aim = 1;

        m_sound.updateFinalBossSound(&m_mechaCs->trans, m_state, m_anims);
        m_contact.update();
        vecCopy(&target, MONSTER_LOCATOR(m_monster7C, 0x809));
        if (m_state == 0 || m_state == 6 || m_state == 7 || m_state == 9)
            aim = 0;
        if (aim) {
            vecSub(&dir, &target, &m_mechaCs->trans);
            mathfRotationFromVector(&rot, &dir);
            if (m_state == 3) {
                rot.z += 0.17453292f;
                boundEulerAngle(&rot.z);
            } else if (m_state == 4) {
                if (m_timer >= 0xB0) {
                    rot.z -= 0.17453292f;
                    boundEulerAngle(&rot.z);
                } else if ((float)m_timer == 175.0f) {
                    s_smoothFactor = 0.015f;
                }
            }
            if (m_state != 3)
                s_smoothFactor = smoothEasyInTC(s_smoothFactor, 0.3f, 0.05f, 0.0001f);
            m_heading = smoothEasyInAngleTC(m_prevHeading, rot.z, s_smoothFactor, 0.0001f);
            boundEulerAngle(&m_heading);
            m_prevHeading = m_heading;
            m_blend1.setPercent(m_heading * 0.15915494f + 0.5f);
        }
    }

    switch (m_state) {
    case 0:
        m_pappy.update(*this);
        if (m_timer > 0) {
            m_timer -= timerGetFieldsLastFrame();
            if (m_timer <= 0) {
                m_state = 1;
                vecCopy(&m_mechaCs->trans, &attractPts[0]);
            }
            game->m_monsters[0]->cleanUpForMovie();
        }
        break;
    case 1:
        m_monster7C = game->m_monsters[0];
        if (!animationIsRunning(m_anims[0])) {
            animationTransitionInto(m_anims[0], 500.0f, 1, 1);
            m_sound.playShieldPowerUpSound(&m_mechaCs->trans);
        }
        if (!animationIsRunning(m_anims2[1]) && !animationIsRunning(m_anims2[2]))
            animationTransitionInto(m_anims2[2], 50.0f, 1, 1);
        if (mathfManhatDist2D(&m_mechaCs->trans, &attractPts[1]) < 25.0f) {
            animationTransitionInto(m_anims[2], 500.0f, 1, 1);
            m_dest.x = 0.0f;
            m_dest.z = 0.0f;
            m_dest.y = m_monster7C->m_cs->trans.y;
            if (m_dest.y < attractPts[0].y)
                m_dest.y = attractPts[0].y;
            if (attractPts[1].y < m_dest.y)
                m_dest.y = attractPts[1].y;
            m_state = 2;
            m_timer = 30;
        }
        m_mechaCs->trans.y += bossMph * 0.024444444f;
        break;
    case 2:
        if (mathfManhatDist2D(&m_mechaCs->trans, &m_dest) < 25.0f) {
            animationTransitionInto(m_anims[4], 500.0f, 1, 1);
            m_state2 = 0;
            m_state = 3;
            m_blend2.rampIn(200.0f);
            m_sound.playGetReadyToFireSound(&m_mechaCs->trans);
        } else if (m_state2 == 0) {
            /* wait, then start the walk animation toward the player's side */
            if (--m_timer == 0) {
                if (m_mechaCs->trans.y < m_monster7C->m_cs->trans.y)
                    animationTransitionInto(m_anims[0], 500.0f, 1, 1);
                else
                    animationTransitionInto(m_anims[1], 500.0f, 1, 1);
                m_state2 = 1;
            }
        } else {
            /* checks the distance again although it just failed above (retail) */
            if (mathfManhatDist2D(&m_mechaCs->trans, &m_dest) < 25.0f) {
                animationTransitionInto(m_anims[4], 500.0f, 1, 1);
                m_state2 = 0;
                m_state = 3;
                m_blend2.rampIn(200.0f);
            }
            if (m_mechaCs->trans.y < m_dest.y)
                m_mechaCs->trans.y += bossMph * 0.024444444f;
            else
                m_mechaCs->trans.y -= bossMph * 0.024444444f;
        }
        break;
    case 3:
        m_blend2.setPercent(smoothEasyInTC(m_blend2.getPercent(), 0.5f, 0.15f, 0.01f));
        if (!animationIsRunning(m_anims[4])) {
            animationTransitionInto(m_anims[6], 500.0f, 1, 1);
            m_state = 4;
            m_timer = 240;
            m_laser = 0.0f;
            m_360 = 0;
        }
        s_smoothFactor = smoothEasyInTC(s_smoothFactor, 0.0f, 0.08f, 0.001f);
        break;
    case 4: {
        float pitch;
        float dist;
        FVECTOR rph;
        float mat[4][4];
        _hdResult res;

        vecCopy(&spark, &target);
        if (target.z < 180.0f)
            target.z -= 50.0f;
        vecSub(&dir, &target, &m_loc27AAPos);
        pitch = mathfPitchFromVector(&dir);
        s_degreeOffset = smoothEasyInTC(s_degreeOffset, 0.0f, 0.01f, 0.001f);
        pitch = 0.5f - pitch * 0.31830987f;
        m_blend2.setPercent(smoothEasyInTC(m_blend2.getPercent(), pitch, s_smoothFactor, 0.001f));
        pitch = (m_blend1.getPercent() - 0.5f) * 360.0f * 0.017453292f;
        rph.x = mathfPitchFromPointToPoint(&m_loc27AAPos, MONSTER_LOCATOR(m_monster7C, 0x809));
        rph.z = pitch;
        rph.y = 0.0f;
        mathfRotMatrixRPH(mat, &rph);
        dir.z = 0.0f;
        dir.x = 0.0f;
        dir.y = 4000.0f;
        vecApplyRows(&dir, mat, &dir);
        vecAdd(&target, &m_loc27AAPos, &dir);
        hdLosCSTest(&m_loc27AAPos, &target, &res, 0x27A6);
        if (res.hit) {
            FVECTOR d;

            if ((res.ids & 0x7FF) == m_monster7C->m_id) {
                _fvector *tip = MONSTER_LOCATOR(m_monster7C, 0x809);

                vecSub(&d, &m_loc27AAPos, tip);
                dist = eeSqrtf(vecDot(&d, &d)) + 20.0f;
                vecCopy(&target, tip);
            } else {
                vecSub(&d, &m_loc27AAPos, &res.point);
                dist = eeSqrtf(vecDot(&d, &d));
                vecCopy(&target, &res.point);
                target.z += 50.0f;
            }
        } else {
            dist = 4000.0f;
        }
        if (dist < 4000.0f) {
            dist /= 4000.0f;
            if (m_laser < dist)
                m_laser = smoothEasyIn(m_laser, dist, 0.25f, 0.001f);
            else
                m_laser = dist;
            if (mathfRand(0, 7) == 0) {
                spark.x = target.x + mathfRandf(-35.0f, 35.0f);
                spark.y = target.y + mathfRandf(-35.0f, 35.0f);
                spark.z = target.z + mathfRandf(-35.0f, 35.0f);
                particleCreateFx(&spark, 0x58, mathfRandf(1.0f, 5.0f), 0, 0.0f);
            }
            if (mathfRand(0, 7) == 0) {
                spark.x = target.x + mathfRandf(-35.0f, 35.0f);
                spark.y = target.y + mathfRandf(-35.0f, 35.0f);
                spark.z = target.z + mathfRandf(-35.0f, 35.0f);
                particleCreateFx(&spark, 0x21, mathfRandf(1.0f, 5.0f), 0, 0.0f);
            }
            if (mathfRand(0, 7) == 0) {
                spark.x = target.x + mathfRandf(-35.0f, 35.0f);
                spark.y = target.y + mathfRandf(-35.0f, 35.0f);
                spark.z = target.z + mathfRandf(-35.0f, 35.0f);
                particleCreateFx(&spark, 0x1F, mathfRandf(1.0f, 5.0f), 0, 0.0f);
            }
        } else {
            m_laser = smoothEasyIn(m_laser, 1.0f, 0.25f, 0.001f);
        }
        animationSetToPercent(m_anim3, m_laser, 1);
        if (res.hit) {
            unsigned id1 = (res.ids >> 11) & 0x3FFF;

            if (id1 == (unsigned)m_monster7C->m_typeBits) {
                FVECTOR push;
                float inv;

                switch (game->m_matchMode) {
                case 0:
                    s_laserDamage = 0.5f;
                    break;
                case 1:
                    s_laserDamage = 0.575f;
                    break;
                case 2:
                    s_laserDamage = 0.65f;
                    break;
                }
                inv = 1.0f / eeSqrtf(vecDot(&dir, &dir));
                push.x = dir.x * inv;
                push.y = dir.y * inv;
                push.z = dir.z * inv;
                vecScale(&push, &push, s_inc);
                m_monster7C->m_cs->trans.x += push.x;
                m_monster7C->m_cs->trans.y += push.y;
                m_monster7C->takeDamage(s_laserDamage, false, 0);
                {
                    ActuatorData a = *(ActuatorData *)((char *)game + 0x1227E0);

                    inputSetActuator(0, &a);
                }
                *(int *)((char *)m_monster7C + 0x1A84) = 1;
                if (m_monster7C->m_dead)
                    m_timer = 1;
            } else if (res.ids & 0x7FF) {
                float amount = 0.16f;

                if (id1 - 0x27AE < 6) /* the capitol pillars */
                    amount = 1.1f;
                if (m_hurt.creditHit(res.ids & 0x7FF, amount) > 5.0f) {
                    FVECTOR hitDir;

                    m_hurt.creditHit(res.ids & 0x7FF, -5.0f);
                    HitEvent::create()->pos = &target;
                    vecSub(&hitDir, &target, &m_mechaCs->trans);
                    Interactives::dispatchHit(&hitDir, 5.01f, 0, res.ids & 0x7FF);
                }
            }
        }
        if (--m_timer == 0) {
            animationTransitionInto(m_anims[5], 500.0f, 1, 1);
            m_state = 5;
            m_blend2.rampOut(200.0f);
            animationSetToPercent(m_anim3, 0.0f, 1);
        }
        m_sound.updateMechLaser(&m_mechaCs->trans);
        break;
    }
    case 5:
        if (!animationIsRunning(m_anims[5])) {
            animationTransitionInto(m_anims[10], 500.0f, 1, 1);
            m_state = 6;
            m_sound.playMechaPowerDownSound(&m_mechaCs->trans);
        }
        m_sound.terminateMechLaser();
        break;
    case 6:
        if (animationGetCurrentPercent(m_anims[10]) >= 0.99f) {
            m_state = 7;
            m_timer = 240;
            m_sound.playShieldPowerDownSound(&m_mechaCs->trans);
            s_smoothFactor = 0.0f;
        }
        break;
    case 7:
        if (animationIsRunning(m_anims2[2]))
            animationTransitionInto(m_anims2[0], 50.0f, 1, 1);
        if (--m_timer == 0) {
            animationStart(m_anims[9], true);
            animationStart(m_anims2[1], true);
            m_sound.playShieldPowerUpSound(&m_mechaCs->trans);
            m_state = 8;
        }
        break;
    case 8:
        if (!animationIsRunning(m_anims[9])) {
            m_dest.y = m_monster7C->m_cs->trans.y;
            if (m_dest.y < attractPts[0].y)
                m_dest.y = attractPts[0].y;
            if (attractPts[1].y < m_dest.y)
                m_dest.y = attractPts[1].y;
            m_state = 2;
            m_timer = 1;
            m_state2 = 0;
        }
        if (!animationIsRunning(m_anims2[1]) && !animationIsRunning(m_anims2[2]))
            animationTransitionInto(m_anims2[2], 50.0f, 1, 1);
        break;
    case 9:
        m_timer -= timerGetFieldsLastFrame();
        if (m_timer <= 0) {
            _cs *cs = m_monster74->m_cs;

            mathfRotationFromPointToPoint(&cs->rot, &cs->trans, &m_monster7C->m_cs->trans);
            m_phase = 1;
            m_state = 3;
            *(short *)((char *)m_monster74->m_padFlags[0] + 0x2A) = 1;
            m_monster74->enterNewState((MonsterState *)m_monster74->m_stateRef);
            MONSTER_AI_7C4(m_monster74) = 1;
            m_monster74->m_unk49 = 1;
            m_flags &= ~6;
            m_mechaCs->drawMe = 0;
            m_monster74->setShadowOnOff(true);
        }
        if (m_timer == 60)
            game->fadeOutAndIn(2);
        if (m_timer < 0x6EB && !s_endingCinema) {
            m_monster74->m_cs->drawMe = 1;
            m_monster74->aiInit();
            MONSTER_SPECIAL(m_monster74) = 0;
            m_monster74->m_unk49 = 0;
            m_monster74->m_playerNum = 2;
            m_monster74->m_cs->drawMe = 1;
            s_endingCinema = 1;
            ActionDispatch::callScriptFromId(0x226B, 0, 1);
            STREAMING_SOUND()->startCinemaMusicFromId(0x226B);
            {
                _fvector *v = &m_mechaCs->trans;

                v->x = 0.0f;
                v->y = 497.0f;
                v->z = 3.0f;
            }
            m_mechaCs->drawMe = 0;
            m_monster74->setShadowOnOff(false);
        }
        game->m_monsters[0]->cleanUpForMovie();
        break;
    }

    if (!moviePlaying) {
        _fvector *v = &m_mechaCs->rot;

        v->x = 0.0f;
        v->y = 0.0f;
        v->z = 0.0f;
        mathfRotMatrixRPH(m_mechaCs->mat, &m_mechaCs->rot);
        m_mechaCs->trans.x = 0.0f;
        m_mechaCs->trans.z = 0.0f;
    }
    hdReparentCsGrid(m_mechaCs);
    vecCopy(&m_pinTrans, &m_mechaCs->trans);
    m_pinTrans.z += 200.0f;

    /* dust under the feet while walking */
    for (i = 0; i < 4; i++) {
        if ((unsigned)(m_state - 1) < 2) {
            int t = --m_splashTimer[i];

            if (m_feet[i].z <= 1.0f && t <= 0) {
                FVECTOR p;

                vecCopy(&p, &m_feet[i]);
                p.z = -20.0f;
                particleCreateFx(&p, 0x41, 0.4f, 0, 0.0f);
                m_splashTimer[i] = 30;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", updateMecha__9FinalBoss);
#endif
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", D_006E9008);
/* Phase 1: the player's jelly (m_monster74) fights m_monster7C, a tazer field zaps nearby projectiles and the jelly walks to
 * one of the two pools (attractPts) to refill. States: 1 approach, 2 ultra tazer, 3 back to the pool, 4 rest, 5 dying (ending
 * cinema 0x226C, then phase 2 with m_monster78). */
#ifdef NON_MATCHING
/* 35/673 words: this lands in s1 instead of s2 (every load shifts) and the splash loop is strength-reduced (timer pointer, -10.0f hoisted); same blocks and calls */
void FinalBoss::updateJelly(void)
{
    Weapon *w;
    int sw;
    int i;

    {
        float r = m_monster74->m_health / m_monster74->m_maxHealth;

        if (r > 0.8f)
            sw = 0;
        else if (r > 0.6f)
            sw = 1;
        else if (r > 0.4f)
            sw = 2;
        else if (r > 0.2f)
            sw = 3;
        else
            sw = 4;
    }
    hierSetSwitch((_hierswitch *)m_ep27AD, sw);

    if (gameWeapons()->WeaponInRadius(&m_monster74->m_cs->trans, 600.0f, &w)
        && (w->m_age >= 7 || mathfManhatDist2D(&m_monster74->m_cs->trans, &w->m_cs->trans) < 200.0f)) {
        FVECTOR from;

        vecCopy(&from, MONSTER_LOCATOR(m_monster74, 0x809));
        from.z -= 60.0f;
        electricArc(&from, &w->m_cs->trans, 0x800000FF, 0x800000FF, 0.0f, 3.0f, 0.0f, 0.6f, 1, rand(), 30);
        particleCreateFx(&w->m_cs->trans, 0x1A, 1.0f, 0, 0.0f);
        MONSTER_SOUND(m_monster74)->playJellyTazerOneShot();
        if (w->m_type == 0x17)
            LevelPickups::killPickup(w->m_pickup, 1);
        w->KillWeapon();
    }

    if (m_monster74->m_dead && m_state != 5) {
        m_timer = 0x708;
        m_state = 5;
        game->fadeOutAndIn(4);
        s_endingCinema = 0;
    }

    switch (m_state) {
    case 1:
        if (mathfManhatDist2D(&m_monster74->m_cs->trans, &m_monster7C->m_cs->trans) < 200.0f
            && !m_monster74->m_freeFalling && !m_monster7C->m_dead) {
            MONSTER_SPECIAL(m_monster74) = 1;
            if (((StateUltraTazer *)MONSTER_ULTRA_TAZER(m_monster74))->transitionOK()) {
                m_monster74->enterNewState(MONSTER_ULTRA_TAZER(m_monster74));
                m_state = 2;
            }
        }
        {
            AiNavigator *nav = MONSTER_NAV(m_monster74);

            if (nav->mode == 2) {
                nav->seek(*(DbInteractive *)m_monster7C, 0.0f);
                MONSTER_AI_7C4(m_monster74) = 1;
            }
        }
        break;
    case 2:
        if (m_monster7C->m_dead) {
            m_state = 3;
            *(short *)((char *)m_monster74->m_padFlags[0] + 0x2A) = 1;
            m_monster74->enterNewState((MonsterState *)m_monster74->m_stateRef);
            MONSTER_AI_7C4(m_monster74) = 1;
            break;
        }
        MONSTER_NAV(m_monster74)->target(m_monster7C->m_cs->trans, 0.01f);
        {
            float d0 = mathfManhatDist2D(&m_monster7C->m_cs->trans, &attractPts[0]);

            if (mathfManhatDist2D(&m_monster7C->m_cs->trans, &attractPts[1]) < d0)
                m_attract = 0;
            else
                m_attract = 1;
        }
        {
            float charge = *(float *)((char *)m_monster74 + 0x1115C); /* the ultra tazer's remaining charge */

            if (charge > 0.0f) {
                if (m_monster74->m_state != (int *)MONSTER_ULTRA_TAZER(m_monster74)) {
                    MONSTER_SPECIAL(m_monster74) = 1;
                    if (((StateUltraTazer *)MONSTER_ULTRA_TAZER(m_monster74))->transitionOK()) {
                        m_monster74->enterNewState(MONSTER_ULTRA_TAZER(m_monster74));
                        m_state = 2;
                    }
                }
            } else if (m_monster74->m_state != (int *)MONSTER_ULTRA_TAZER(m_monster74) && charge <= 0.0f) {
                MONSTER_SPECIAL(m_monster74) = 0;
                if (mathfManhatDist2D(&m_monster74->m_cs->trans, &attractPts[m_attract]) < 150.0f) {
                    m_state = 4;
                    m_monster74->enterNewState(MONSTER_IDLE(m_monster74));
                    m_timer = 300;
                } else {
                    m_state = 3;
                    *(short *)((char *)m_monster74->m_padFlags[0] + 0x2A) = 1;
                    m_monster74->enterNewState((MonsterState *)m_monster74->m_stateRef);
                    MONSTER_AI_7C4(m_monster74) = 1;
                }
            }
        }
        MONSTER_SOUND(m_monster74)->updateJellyTazer();
        break;
    case 3: {
        float d = mathfManhatDist2D(&m_monster74->m_cs->trans, &attractPts[m_attract]);

        if (m_monster74->m_cs->trans.z < 200.0f && d > 50.0f) {
            m_monster74->m_stamina.cur = m_monster74->m_stamina.max;
            MONSTER_AI_7C4(m_monster74) = 1;
        }
        if (m_monster74->m_freeFalling) {
            _cs *cs;

            cs = m_monster74->m_cs;
            cs->trans.x = smoothEasyIn(cs->trans.x, attractPts[m_attract].x, 0.02f, 0.001f);
            cs = m_monster74->m_cs;
            cs->trans.y = smoothEasyIn(cs->trans.y, attractPts[m_attract].y, 0.02f, 0.001f);
        }
        if (!m_monster74->m_freeFalling && *m_monster74->m_state != 4) {
            if (d <= 300.0f) {
                MonsterState *idle = MONSTER_IDLE(m_monster74);

                if (STATE_FEASIBLE(idle)) {
                    m_monster74->enterNewState(MONSTER_IDLE(m_monster74));
                    m_state = 4;
                    m_timer = 480;
                }
            } else {
                MONSTER_NAV(m_monster74)->tag(attractPts[m_attract], 300.0f);
            }
        }
        break;
    }
    case 4:
        MONSTER_NAV(m_monster74)->target(m_monster7C->m_cs->trans, 0.05f);
        if (m_timer > 0) {
            if (--m_timer <= 0) {
                m_state = 1;
                MONSTER_NAV(m_monster74)->seek(*(DbInteractive *)m_monster7C, 0.0f);
            }
            MONSTER_SPECIAL(m_monster74) = 0;
        }
        if (mathfManhatDist2D(&m_monster74->m_cs->trans, &m_monster7C->m_cs->trans) < 200.0f && !m_monster7C->m_dead) {
            MONSTER_SPECIAL(m_monster74) = 1;
            if (((StateUltraTazer *)MONSTER_ULTRA_TAZER(m_monster74))->transitionOK()) {
                m_monster74->enterNewState(MONSTER_ULTRA_TAZER(m_monster74));
                m_timer = 0;
                m_state = 2;
            }
        }
        break;
    case 5:
        m_timer -= timerGetFieldsLastFrame();
        if (m_timer <= 0) {
            _fvector *v;

            m_monster74->m_cs->drawMe = 0;
            m_monster74->m_playerNum = 0;
            v = &m_monster78->m_cs->rot;
            v->x = 0.0f;
            v->y = 0.0f;
            v->z = 0.0f;
            m_state = 0;
            m_phase = 2;
            m_monster7C->dropPickup();
            LevelPickups::turnOff2Handeds();
        }
        if (m_timer == 45)
            game->fadeOutAndIn(2);
        if (m_timer < 0x6EB && !s_endingCinema) {
            m_monster78->aiInit();
            m_monster78->m_cs->drawMe = 1;
            m_monster78->m_playerNum = 2;
            animationResetCharacter(m_monster74->m_anims[2]);
            s_endingCinema = 1;
            ActionDispatch::callScriptFromId(0x226C, 0, 1);
            STREAMING_SOUND()->startCinemaMusicFromId(0x226C);
        }
        game->m_monsters[0]->cleanUpForMovie();
        break;
    }

    if (m_state != 2)
        MONSTER_SOUND(m_monster74)->terminateJellyTazer();

    /* splashes under the jelly's feet while it wades in the pool area */
    {
        int locs[4] = { 0x804, 0x805, 0x816, 0x817 };

        for (i = 0; i < 4; i++) {
            if (m_state == 1 || m_state == 3) {
                _cs *cs = m_monster74->m_cs;

                if (cs->trans.x >= -200.0f && cs->trans.x <= 200.0f && cs->trans.y >= -150.0f && cs->trans.y <= 1050.0f) {
                    _fvector *foot;

                    m_splashTimer[i]--;
                    foot = MONSTER_LOCATOR(m_monster74, locs[i]);
                    if (foot->z <= -10.0f && m_splashTimer[i] <= 0) {
                        FVECTOR pos;

                        vecCopy(&pos, foot);
                        pos.z = 0.0f;
                        particleCreateFx(&pos, 0x56, 1.0f, 0, 0.0f);
                        MONSTER_SOUND(m_monster74)->playWaterSplashSound();
                        m_splashTimer[i] = 30;
                    }
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", updateJelly__9FinalBoss);
#endif

void FinalBoss::takeHit(_fvector *dir, float dmg, int attackerId)
{
    HitEvent *hit = &s_takeHitInfo;

    if (moviePlaying || m_state != 7)
        return;
    if ((unsigned)(hit->source - 5) < 2 || hit->type == 3 || hit->type == 2 || hit->type == 4 || hit->type == 5) {
        if (m_health > 0.0f) {
            if (m_flashFields < -4)
                m_flashFields = 5;
            m_health -= dmg;
            if (m_health <= 0.0f) {
                m_state = 9;
                m_timer = 0x708;
                game->fadeOutAndIn(4);
            } else {
                animationTransitionInto(m_anims[3], 50.0f, 1, 1);
            }
        }
        m_sound.playTakeHitSound(&m_mechaCs->trans);
    }
}

void FinalBoss::addEpNode(_hierhead *h)
{
    switch (h->id1) {
    case 0x27A6: m_ep[0] = h; break;
    case 0x27A7: m_ep[1] = h; break;
    case 0x27A8: m_ep[2] = h; break;
    case 0x27A9: m_ep[3] = h; break;
    case 0x27AB: m_ep27AB = h; break;
    case 0x27AD: m_ep27AD = h; break;
    }
}

#ifdef NON_MATCHING
/* 56/59 words: %hi(finalBoss+0x390) lands in v0 instead of a0 */
void FinalBoss::setLocator(int id, float (&m)[4][4], _fvector *pos)
{
    switch (id) {
    case 0x27AA: {
        float (*d)[4] = finalBoss.m_loc27AAMat;
        __asm__ volatile("lq $8, 0(%4)\n\t"
                         "lq $9, 16(%4)\n\t"
                         "lq $10, 32(%4)\n\t"
                         "lq $11, 48(%4)\n\t"
                         "sq $8, %0\n\t"
                         "sq $9, %1\n\t"
                         "sq $10, %2\n\t"
                         "sq $11, %3"
                         : "=m"(d[0][0]), "=m"(d[1][0]),
                           "=m"(d[2][0]), "=m"(d[3][0])
                         : "r"(m)
                         : "$8", "$9", "$10", "$11");
        vecCopy(&finalBoss.m_loc27AAPos, pos);
        break;
    }
    case 0x80E:
        vecCopy(&finalBoss.m_pinTrans, pos);
        break;
    case 0x804:
        vecCopy(&finalBoss.m_feet[0], pos);
        break;
    case 0x805:
        vecCopy(&finalBoss.m_feet[1], pos);
        break;
    case 0x816:
        vecCopy(&finalBoss.m_feet[2], pos);
        break;
    case 0x817:
        vecCopy(&finalBoss.m_feet[3], pos);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", setLocator__9FinalBossiRA3_A3_fP8_fvector);
#endif

INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", __static_initialization_and_destruction_0_00134228);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", func_001342F8);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", func_00134320);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", _vt$9FinalBoss);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", __tf9FinalBoss);
void * getPinTrans__9FinalBoss(void *self) __asm__("getPinTrans__9FinalBoss");
void * getPinTrans__9FinalBoss(void *self)
{
    return (char *)self + 0x60;
}
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", func_00134388);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", _GLOBAL_$I$finalBoss);
