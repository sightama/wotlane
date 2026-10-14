#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/hit_event.h"
#include "game/anim_blend.h"
#include "game/streaming_sound.h"
#include "cs_pool.h"
#include "vecmath.h"

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
};

class ActionDispatch {
public:
    static void callScriptFromId(unsigned short id1, unsigned short id2, int fadeOut);
};

class AnimContact {
public:
    char pad[0x50];
    void init(_animHandle &h);
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

class FinalBoss : public DbInteractive {
public:
    float m_health;           /* 0x14: 75 per match-mode step + 100 (initBefore) */
    int m_active;             /* 0x18: 1 while the boss level runs; 0 sends it back */
    int m_phase;              /* 0x1C: 0 mecha, 1 jelly, 2 last monster fight, 3 ending */
    int m_state;              /* 0x20: state inside the phase (7 = takes hits, 9 = dying) */
    int m_state2;             /* 0x24 */
    int m_timer;              /* 0x28: fields */
    char pad2C[4];
    int m_flashFields;        /* 0x30: hit flash (red) while > 0 */
    char pad34[0x40 - 0x34];
    FinalBossSound m_sound;   /* 0x40 */
    char pad50[0x60 - 0x50];
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
    char pad2E0[0x364 - 0x2E0];
    _hierhead *m_ep[4];       /* 0x364: ep nodes 0x27A6..0x27A9 */
    char pad374[0x380 - 0x374];
    FVECTOR m_loc27AAPos;     /* 0x380: setLocator 0x27AA */
    FMATRIX_16 m_loc27AAMat;  /* 0x390 */
    char pad3D0[0x3E0 - 0x3D0];
    FVECTOR m_loc804;         /* 0x3E0 */
    FVECTOR m_loc805;         /* 0x3F0 */
    FVECTOR m_loc816;         /* 0x400 */
    FVECTOR m_loc817;         /* 0x410 */
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

INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", updateMecha__9FinalBoss);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", D_006E9008);
INCLUDE_ASM("asm/nonmatchings/game/FinalBoss", updateJelly__9FinalBoss);

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
        vecCopy(&finalBoss.m_loc804, pos);
        break;
    case 0x805:
        vecCopy(&finalBoss.m_loc805, pos);
        break;
    case 0x816:
        vecCopy(&finalBoss.m_loc816, pos);
        break;
    case 0x817:
        vecCopy(&finalBoss.m_loc817, pos);
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
