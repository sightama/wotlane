#include "common.h"
#include "game/game.h"
#include "game/power_ups.h"
#include "cs_pool.h"
#include "hd.h"
#include "vecmath.h"

/* Health, stamina, special weapon and cloak power-ups placed by PowerUpTool (exportPoints -> CreatePowerUp). A monster that touches one
   (TakeHit, from the collision code) gets the credit; the model is hidden and comes back after a random regen time (update). Story-mode
   pickups (types 10..19) only exist in game mode 1 and are removed for good when taken. Everything works on PowerUps::instance. */

extern "C" int rand(void);
typedef unsigned int u128 __attribute__((mode(TI)));

/* Tweakables (data/sdata of this TU, kept in asm). The regen tables are in seconds; m_regenMin/Max hold them in fields. */
extern int s_powerUpCount[20];        /* credit (health/stamina points) of each type */
extern int s_tweakMinRegenCount[20];
extern int s_tweakMaxRegenCount[20];
extern int hitPowerUps;               /* 0 disables taking power-ups */
__asm__("#SNFIX_SMALL hitPowerUps");
extern int s_grabHealthWhenFull;
__asm__("#SNFIX_SMALL s_grabHealthWhenFull");
extern int s_grabStaminaWhenFull;
__asm__("#SNFIX_SMALL s_grabStaminaWhenFull");
extern float s_regenMultiplier;
__asm__("#SNFIX_SMALL s_regenMultiplier");
extern int s_cloakTime;
__asm__("#SNFIX_SMALL s_cloakTime");

extern int moviePlaying;
extern int assBossActive;
extern int craterSwitched;

void hdPolyCheckBackface(bool on);
int hdLosTest(_fvector *from, _fvector *to, _hdResult *res, bool option);
float hdHatTest(_fvector *pos, _fvector *normal, float up, bool option, float down);
void hdRemoveCsFromGrid(_cs *cs);
void boundEulerAngle(float *p);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rot);
int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);
int timerGetFieldCount(void);
void soundPlayPowerUpSound(int a, int b, int c, _fvector *pos);

class DbInteractive {
public:
    int m_kind;               /* 0x00: 3 for the interactives that can sit over a power-up */
    char pad4[0x30 - 0x4];
    int m_x30;                /* 0x30: 1 or not selects which "hidden by an interactive" bit testHidden sets */
};

class Interactives {
public:
    static DbInteractive *getInteractive(int i);
};

class Ai {
public:
    int isMinion(void);
};

/* Looks straight up from the power-up: anything within 1500 units above it with an upward facing normal hides it (bit 5) until a later
   call finds it clear again. An interactive found there is remembered (m_hiddenBy, bits 6/7). Backface tests are on during the cast. */
#ifdef NON_MATCHING
/* equivalent: 36/89 words, 0x150 vs 0x164; retail reads the _hdResult ids with a 64-bit ld and keeps the locals at other stack offsets */
void PowerUp::testHidden(void)
{
    _hdResult res;
    FVECTOR top;
    unsigned short f;
    bool wasHidden;

    m_hiddenBy = 0;
    f = m_flags;
    m_flags = f & 0xFF1F;
    wasHidden = (f & 0x20) != 0;
    hdPolyCheckBackface(true);
    vecCopy(&top, &m_cs->trans);
    top.z += 1500.0f;
    if (!hdLosTest(&m_cs->trans, &top, &res, false)) {
        if (res.normal.z > 0.0f) {
            m_flags |= 0x20;
            m_cs->drawMe = 0;
        }
        if (res.ids & 0x7FF) {
            DbInteractive *it = Interactives::getInteractive(res.ids & 0x7FF);
            if (it->m_kind == 3) {
                if (it->m_x30 == 1)
                    m_flags |= 0x40;
                else
                    m_flags |= 0x80;
                m_hiddenBy = res.ids & 0x7FF;
            }
        }
    }
    if (wasHidden && !(m_flags & 0x20)) {
        m_cs->drawMe = 1;
        if (m_shadow)
            m_shadow->drawMe = 1;
        hdReparentCsGrid(m_cs);
        m_flags &= ~0x10;
    }
    hdPolyCheckBackface(false);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", testHidden__7PowerUp);
#endif
/* Drops the shadow onto the ground under the power-up and lays it on the ground normal (mat[2] gets the normal, mat[0] = model y x normal,
   mat[1] = normal x mat[0]). With bit 2 the model's rest height (trans.w) drops by the same amount. */
#ifdef NON_MATCHING
/* equivalent: 24/73 words, 0x124 vs 0x11c; retail loads x/y/z once for the normalize (ours reloads after the first store) and keeps d in $f6 */
void PowerUp::placeShadow(void)
{
    float h, d, s;
    float *x;

    h = hdHatTest(&m_shadow->trans, (_fvector *)m_shadow->mat[2], m_cs->trans.z - m_shadow->trans.z, false, 4096.0f);
    m_shadow->mat[2][3] = 0.0f;
    d = h - 0.1f;
    m_shadow->trans.z -= d;
    vecCross((_fvector *)m_shadow->mat[0], (_fvector *)m_cs->mat[1], (_fvector *)m_shadow->mat[2]);
    x = m_shadow->mat[0];
    s = eeRsqrtf(1.0f, vecDot((_fvector *)x, (_fvector *)x));
    x[0] *= s;
    x[2] *= s;
    x[1] *= s;
    vecCross((_fvector *)m_shadow->mat[1], (_fvector *)m_shadow->mat[2], (_fvector *)m_shadow->mat[0]);
    if (m_flags & 4)
        m_cs->trans.w -= d;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", placeShadow__7PowerUp);
#endif
#ifdef NON_MATCHING
/* equivalent: 51/56 words; two registers swapped around the inner loop */
void PowerUps::initPowerUpsBefore(void)
{
    int i, j;

    for (i = 0; i < 20; i++) {
        m_epNodes[i] = 0;
        m_shadowEpNodes[i] = 0;
        m_byTypeCount[i] = 0;
        m_regenMin[i] = s_tweakMinRegenCount[i];
        m_regenMax[i] = s_tweakMaxRegenCount[i];
    }
    LoadRegenTimes();
    for (i = 0; i < 100; i++) {
        m_powerUps[i].m_type = 0;
        m_powerUps[i].m_cs = 0;
        for (j = 19; j >= 0; j--)
            m_byType[i][j] = -1;
    }
    m_count = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", initPowerUpsBefore__8PowerUps);
#endif
void PowerUps::initPowerUpsAfter(void)
{
}
/* Visible power-ups ease down to their rest height and story pickups vanish on two scripted levels; taken ones count down m_timer and
   reappear (new random regen time, sound). One power-up per field (round robin) re-places its shadow and redoes testHidden. */
#ifdef NON_MATCHING
/* equivalent: 151/163 words; retail tests (flags & 3) == 3 with andi/xori and keeps the modulo in other registers */
void PowerUps::update(void)
{
    PowerUp *p;
    int n;

    if (moviePlaying)
        return;
    p = m_powerUps;
    for (n = m_count; n != 0; n--, p++) {
        if (p->m_cs && p->m_cs->drawMe) {
            _cs *cs = p->m_cs;

            if (cs->trans.z - cs->trans.w > 0.1f && p->m_type != 7)
                p->m_cs->trans.z = smoothEasyInTC(cs->trans.z, cs->trans.w, 0.1f, 0.01f);
            if (game->m_gameMode == 1) {
                if (game->m_levelId == 3 && assBossActive && p->m_type == 10)
                    p->m_cs->drawMe = 0;
                if (game->m_levelId == 6 && craterSwitched && (p->m_type == 10 || p->m_type == 11))
                    p->m_cs->drawMe = 0;
            }
        } else {
            if (p->m_shadow && p->m_shadow->drawMe)
                p->m_shadow->drawMe = 0;
            /* hidden ones, cinema-hidden ones and type 7 never come back by themselves */
            if (p->m_flags & 0x20)
                continue;
            if (p->m_flags & 0x100)
                continue;
            if (p->m_type == 7)
                continue;
            if (--p->m_timer > 0)
                continue;
            /* retail does not check m_cs here */
            p->m_cs->drawMe = 1;
            if (p->m_shadow)
                p->m_shadow->drawMe = 1;
            p->m_cs->trans.z = p->m_cs->trans.w;
            hdReparentCsGrid(p->m_cs);
            p->m_timer = mathfRand(m_regenMin[p->m_type], m_regenMax[p->m_type]);
            soundPlayPowerUpSound(0, p->m_type, 1, &p->m_cs->trans);
        }
    }
    if (m_count > 0) {
        p = &m_powerUps[timerGetFieldCount() % m_count];
        if ((p->m_flags & 3) == 3)
            p->placeShadow();
        if (p->m_flags & 0x10)
            p->testHidden();
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", update__8PowerUps);
#endif
int PowerUps::CreatePowerUp(int type, _fvector *pos)
{
    return CreatePowerUp(type, 3, pos);
}
/* Returns the new slot or -1. Story pickups (10..19) need game mode 1; on the final boss level (11) of that mode the cloak is never
   placed and specials above z 500 neither. The model gets a random heading, the shadow (flag bit 0) is dropped to the ground. */
#ifdef NON_MATCHING
/* equivalent: 32/171 words, 0x2ac vs 0x2a0; the story-pickup/game-mode tests branch differently and game is kept in another register */
int PowerUps::CreatePowerUp(int type, unsigned char flags, _fvector *pos)
{
    FVECTOR rot;
    PowerUp *p;

    *(u128 *)&rot = 0;   /* one por/sq; z is overwritten below, the 0.1 never reaches mathfRotMatrixRPH */
    rot.z = 0.1f;
    if ((unsigned)type >= 20 || m_epNodes[type] == 0)
        return -1;
    if ((unsigned)(type - 10) < 10 && game->m_gameMode != 1)
        return -1;
    if (game->m_gameMode == 1 && game->m_levelId == 11) {
        if (type == 9)
            return -1;
        if (type == 8 && pos->z > 500.0f)
            return -1;
    }
    if (m_count >= 100)
        return -1;
    p = &m_powerUps[m_count];
    p->m_flags = flags;
    p->m_cs = CsPool::csActivate();
    if (flags & 1)
        p->m_shadow = CsPool::csActivate();
    else
        p->m_shadow = 0;
    /* retail leaks the shadow cs when the model cs could not be had */
    if (p->m_cs == 0)
        return -1;
    p->m_type = type;
    p->m_index = m_count;
    p->m_timer = mathfRand(m_regenMin[type], m_regenMax[type]);
    AddPowerUpsByTypeEntry(m_count);
    vecCopy(&p->m_cs->trans, pos);
    p->m_cs->trans.w = pos->z;
    rot.z = (rand() & 0x1FF) * 0.017453292f;
    boundEulerAngle(&rot.z);
    mathfRotMatrixRPH(p->m_cs->mat, &rot);
    p->m_cs->drawMe = 1;
    p->m_cs->testCollision = 1;
    p->testHidden();
    if (p->m_flags & 0x20)
        p->m_flags |= 0x10;
    hierSetCsEpNode(p->m_cs, m_epNodes[type]);
    hdReparentCsGrid(p->m_cs);
    if (p->m_shadow) {
        p->m_shadow->drawMe = 1;
        p->m_shadow->testCollision = 0;
        hierSetCsEpNode(p->m_shadow, m_shadowEpNodes[type]);
        vecCopy(&p->m_shadow->trans, &p->m_cs->trans);
        /* the first placement never lowers the rest height */
        if (p->m_flags & 4) {
            p->m_flags &= ~4;
            p->placeShadow();
            p->m_flags |= 4;
        } else {
            p->placeShadow();
        }
    }
    return m_count++;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", CreatePowerUp__8PowerUpsiUcP8_fvector);
#endif
/* Level models: hat id1 0x13EC + type; id2 == 0 is the power-up, anything else its shadow. */
void PowerUps::addPowerUpEpNode(_hierhead *h)
{
    unsigned type = h->id1 - 0x13EC;

    if (type < 20) {
        if (h->id2 == 0)
            m_epNodes[type] = h;
        else
            m_shadowEpNodes[type] = h;
    }
}
#ifdef NON_MATCHING
/* equivalent: 51/53 words; the three stores that clear the slot come out in another order */
void PowerUps::KillPowerUps(void)
{
    int i, j;

    for (i = 0; i < m_count; i++) {
        PowerUp *p = &m_powerUps[i];

        CsPool::csDeactivate(p->m_cs);
        if (p->m_shadow)
            CsPool::csDeactivate(p->m_shadow);
        p->m_shadow = 0;
        p->m_cs = 0;
        p->m_type = 0;
        for (j = 0; j < 20; j++) {
            m_byType[i][j] = -1;
            m_byTypeCount[j] = 0;
        }
    }
    m_count = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", KillPowerUps__8PowerUps);
#endif
/* Frees slot i and shifts the later ones down (renumbering their by-type entries first). m_index is not updated. */
void PowerUps::KillPowerUp(int i)
{
    PowerUp *p;

    RemovePowerUpsByTypeEntry(i);
    p = &m_powerUps[i];
    CsPool::csDeactivate(p->m_cs);
    if (p->m_shadow)
        CsPool::csDeactivate(p->m_shadow);
    p->m_cs = 0;
    p->m_shadow = 0;
    for (; i < m_count - 1; i++) {
        ChangePowerUpsByTypeEntry(i + 1, i);
        m_powerUps[i] = m_powerUps[i + 1];
    }
    m_count--;
}
void PowerUps::KillPowerUp(_cs *cs)
{
    int i;

    for (i = 0; i < m_count; i++) {
        if (m_powerUps[i].m_cs == cs)
            break;
    }
    if (i < m_count)
        KillPowerUp(i);
}
/* Removes index i from its type's list. The count drops even when i was not found (retail). */
#ifdef NON_MATCHING
/* equivalent: 6/59 words, 0xec vs 0xe8; loops laid out differently (retail keeps n * 0x50 in its own register) */
void PowerUps::RemovePowerUpsByTypeEntry(int i)
{
    int type = m_powerUps[i].m_type;
    int n;

    for (n = 0; n < m_byTypeCount[type]; n++) {
        if (m_byType[n][type] == i) {
            for (; n < m_byTypeCount[type] - 1; n++)
                m_byType[n][type] = m_byType[n + 1][type];
            break;
        }
    }
    m_byTypeCount[type]--;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", RemovePowerUpsByTypeEntry__8PowerUpsi);
#endif
void PowerUps::AddPowerUpsByTypeEntry(int i)
{
    int type = m_powerUps[i].m_type;

    m_byType[m_byTypeCount[type]][type] = i;
    m_byTypeCount[type]++;
}
void PowerUps::ChangePowerUpsByTypeEntry(int from, int to)
{
    int type = m_powerUps[from].m_type;
    int n;

    for (n = 0; n < m_byTypeCount[type]; n++) {
        if (m_byType[n][type] == from) {
            m_byType[n][type] = to;
            break;
        }
    }
}
/* Retail sets the shadow's model without checking that there is a shadow. */
void PowerUps::ChangePowerUpType(int i, int type)
{
    PowerUp *p;

    RemovePowerUpsByTypeEntry(i);
    p = &m_powerUps[i];
    p->m_type = type;
    hierSetCsEpNode(p->m_cs, m_epNodes[type]);
    hierSetCsEpNode(p->m_shadow, m_shadowEpNodes[type]);
    AddPowerUpsByTypeEntry(i);
}
void PowerUps::setRegenTimeMultiplier(float m)
{
    s_regenMultiplier = m;
}
void PowerUps::setRegenTime(PowerUp::Type type, int min, int max)
{
    int mult = (int)(s_regenMultiplier * 60.0f);

    s_tweakMinRegenCount[type] = min;
    m_regenMin[type] = min * mult;
    s_tweakMaxRegenCount[type] = max;
    m_regenMax[type] = max * mult;
}
void PowerUps::setCredit(PowerUp::Type type, int credit)
{
    s_powerUpCount[type] = credit;
}
void PowerUps::setCloakTime(int t)
{
    s_cloakTime = t;
}
float PowerUps::getRegenTimeMultiplier(void)
{
    return s_regenMultiplier;
}
int PowerUps::getMinRegenTime(PowerUp::Type type)
{
    return s_tweakMinRegenCount[type];
}
int PowerUps::getMaxRegenTime(PowerUp::Type type)
{
    return s_tweakMaxRegenCount[type];
}
int PowerUps::getCredit(PowerUp::Type type)
{
    return s_powerUpCount[type];
}
int PowerUps::getCloakTime(void)
{
    return s_cloakTime;
}
#ifdef NON_MATCHING
/* equivalent: same instructions, 0x6c vs 0x64; gas puts 2 extra nops in the .p2align 3 after the li.s (as in StickShaker::DefaultSetup) */
void PowerUps::LoadRegenTimes(void)
{
    int mult = (int)(s_regenMultiplier * 60.0f);
    int i;

    for (i = 0; i < 20; i++) {
        m_regenMin[i] = s_tweakMinRegenCount[i] * mult;
        m_regenMax[i] = s_tweakMaxRegenCount[i] * mult;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", LoadRegenTimes__8PowerUps);
#endif
/* Monster::isFullHealth/isFullStamina, inlined in retail (the 0/1 result is materialised before the test) */
static inline bool puFullHealth(Monster *m)
{
    return m->m_health >= m->m_maxHealth;
}
static inline bool puFullStamina(Monster *m)
{
    StaminaMeter *s = &m->m_stamina;

    return s->cur >= s->maxLevel;
}
/* Monster m touched cs. In order: health (unless full or m_typeBits is 0x1A0), stamina (same), the special weapon (not when a special is
   already available, for m_typeBits 0x1C0/0x1E0 or when m_playerNum is 2 and its Ai is a minion), the cloak (not while cloaked), then story pickups of
   player 1 in game mode 1 (text box, removed for good). A power-up taken by the first four is hidden until update brings it back. */
void PowerUps::TakeHit(Monster *m, _cs *cs)
{
    int taken;
    int type;

    if (!cs->drawMe)
        return;
    taken = 0;
    type = (*(unsigned *)cs->epNode >> 18) - 0x13EC;
    if (!hitPowerUps)
        return;
    if ((s_grabHealthWhenFull || !puFullHealth(m)) && m->m_typeBits != 0x1A0) {
        taken = 1;
        switch (type) {
        case 6:
            particleCreateFx(&cs->trans, 0x3A, 1.0f, 0, 0.0f);
            m->m_healthGlow[0] = 1;
            m->m_healthGlow[1] = m->m_healthGlow[2] = 100;
            break;
        case 0:
            particleCreateFx(&cs->trans, 0x3A, 2.0f, 0, 0.0f);
            m->m_healthGlow[0] = 2;
            m->m_healthGlow[1] = m->m_healthGlow[2] = 120;
            break;
        case 1:
            particleCreateFx(&cs->trans, 0x3A, 4.0f, 0, 0.0f);
            particleCreateFx((_fvector *)((char *)m + 0x3E30), m->m_cs->mat, 0x3B, 1.0f, 0, 0, false, 0.0f);
            m->m_healthGlow[0] = 3;
            m->m_healthGlow[1] = m->m_healthGlow[2] = 140;
            break;
        case 2:
            particleCreateFx(&cs->trans, 0x3A, 8.0f, 0, 0.0f);
            particleCreateFx((_fvector *)((char *)m + 0x3E30), m->m_cs->mat, 0x3C, 1.0f, 0, 0, false, 0.0f);
            m->m_healthGlow[0] = 4;
            m->m_healthGlow[1] = m->m_healthGlow[2] = 160;
            break;
        default:
            taken = 0;
            break;
        }
        if (taken)
            m->creditHealth((float)s_powerUpCount[type]);
    }
    if (!taken) {
        if ((s_grabStaminaWhenFull || !puFullStamina(m)) && m->m_typeBits != 0x1A0) {
            taken = 1;
            switch (type) {
            case 7:
                particleCreateFx(&cs->trans, 0x3D, 1.0f, 0, 0.0f);
                m->m_staminaGlow[0] = 1;
                m->m_staminaGlow[1] = m->m_staminaGlow[2] = 100;
                break;
            case 3:
                particleCreateFx(&cs->trans, 0x3D, 2.0f, 0, 0.0f);
                m->m_staminaGlow[0] = 2;
                m->m_staminaGlow[1] = m->m_staminaGlow[2] = 120;
                break;
            case 4:
                particleCreateFx(&cs->trans, 0x3D, 4.0f, 0, 0.0f);
                particleCreateFx((_fvector *)((char *)m + 0x3E30), m->m_cs->mat, 0x3E, 1.0f, 0, 0, false, 0.0f);
                m->m_staminaGlow[0] = 3;
                m->m_staminaGlow[1] = m->m_staminaGlow[2] = 140;
                break;
            case 5:
                particleCreateFx(&cs->trans, 0x3D, 8.0f, 0, 0.0f);
                particleCreateFx((_fvector *)((char *)m + 0x3E30), m->m_cs->mat, 0x3F, 1.0f, 0, 0, false, 0.0f);
                m->m_staminaGlow[0] = 4;
                m->m_staminaGlow[1] = m->m_staminaGlow[2] = 160;
                break;
            default:
                taken = 0;
                break;
            }
            if (taken) {
                /* retail has a separate (identical) branch for type 7 */
                if (type == 7)
                    m->creditStamina((float)s_powerUpCount[7], false);
                else
                    m->creditStamina((float)s_powerUpCount[type], false);
            }
        }
        if (!taken) {
            if (!m->isSpecialAvailable() && m->m_typeBits != 0x1C0 && m->m_typeBits != 0x1E0 &&
                (m->m_playerNum != 2 || !((Ai *)m->m_ai)->isMinion())) {
                taken = 1;
                switch (type) {
                case 8:
                    m->m_specialWeapon = 1;
                    if (m->m_cameraFollows) {
                        gameHud(m->m_cameraView)->fAC = 0;
                        gameHud(m->m_cameraView)->addMessage(9, 0);
                        if (game->m_gameMode == 1 && game->m_levelId == 1 && m->m_playerNum == 1)
                            game->m_huds[0].addTextBoxMessage(0x18);
                    }
                    break;
                default:
                    taken = 0;
                    break;
                }
            }
            if (!taken) {
                if (!m->m_cloaked) {
                    taken = 1;
                    switch (type) {
                    case 9:
                        if (m->m_cameraFollows)
                            gameHud(m->m_cameraView)->addMessage(10, 0);
                        m->setCloakOn();
                        break;
                    default:
                        taken = 0;
                        break;
                    }
                }
                if (!taken) {
                    if (game->m_gameMode == 1 && m->m_playerNum == 1 && (unsigned)(type - 10) < 10) {
                        /* on canyon (3) the text box id is the player's controller configuration (Monster +0x672C) */
                        if (game->m_levelId == 3)
                            game->m_huds[0].addTextBoxMessage(game->m_monsters[0]->m_padFlags.f16EC);
                        else
                            game->m_huds[0].addTextBoxMessage(type - 10);
                        KillPowerUp(cs);
                        taken = 0;
                    }
                    if (!taken)
                        return;
                }
            }
        }
    }
    hdRemoveCsFromGrid(cs);
    cs->drawMe = 0;
    if (m->m_cameraFollows == 1)
        soundPlayPowerUpSound(m->m_index, type, 0, 0);
}
/* Closest visible power-up to pos, of any type (type < 0) or of one type. Retail bug kept: when there were candidates but none was
   visible it returns m_powerUps[-1].m_cs, which reads m_shadowEpNodes[18]. */
#ifdef NON_MATCHING
/* equivalent: 72/88 words; delay slots filled differently in the first loop and the tail */
_cs *PowerUps::GetClosestPowerUp(_fvector *pos, int type)
{
    float best = 1e8f;
    int bestIdx = -1;
    int n = 0;
    FVECTOR d;

    if (type < 0) {
        PowerUp *p = m_powerUps;

        for (n = 0; n < m_count; n++, p++) {
            _cs *cs = p->m_cs;

            if (cs->drawMe) {
                float l;

                vecSub(&d, pos, &cs->trans);
                l = vecLenSq(&d);
                if (l < best) {
                    best = l;
                    bestIdx = n;
                }
            }
        }
    } else {
        for (n = 0; n < m_byTypeCount[type]; n++) {
            int i = m_byType[n][type];
            _cs *cs = m_powerUps[i].m_cs;

            if (cs->drawMe) {
                float l;

                vecSub(&d, pos, &cs->trans);
                l = vecLenSq(&d);
                if (l < best) {
                    bestIdx = i;
                    best = l;
                }
            }
        }
    }
    return n > 0 ? m_powerUps[bestIdx].m_cs : 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", GetClosestPowerUp__8PowerUpsP8_fvectori);
#endif
void PowerUps::startCinema(void)
{
    PowerUp *p = m_powerUps;
    int n;

    for (n = m_count; n != 0; n--, p++) {
        if (p->m_cs->drawMe) {
            p->m_cs->drawMe = 0;
            p->m_flags |= 0x100;
            if (p->m_shadow)
                p->m_shadow->drawMe = 0;
        }
    }
}
#ifdef NON_MATCHING
/* equivalent: 19/21 words; retail tests bit 8 with lbu 1(p) & 1 (ours lhu & 0x100) */
void PowerUps::endCinema(void)
{
    PowerUp *p = m_powerUps;
    int n;

    for (n = m_count; n != 0; n--, p++) {
        if (p->m_flags & 0x100) {
            p->m_cs->drawMe = 1;
            p->m_flags &= ~0x100;
            if (p->m_shadow)
                p->m_shadow->drawMe = 1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/PowerUps", endCinema__8PowerUps);
#endif
