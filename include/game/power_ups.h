#ifndef POWER_UPS_H
#define POWER_UPS_H

#include "hieri_types.h"

class Monster;

/* One placed power-up (0x10 bytes; PowerUps keeps 100 of them). Its type comes from the hat id of the model node
   (id1 - 0x13EC). Type names are inferred from the credit table (s_powerUpCount) and the pickup fx size in TakeHit. */
class PowerUp {
public:
    enum Type {
        HEALTH_SMALL,      /* 0: credit 25 */
        HEALTH_MEDIUM,     /* 1: 50 */
        HEALTH_LARGE,      /* 2: 100 */
        STAMINA_SMALL,     /* 3: 25 */
        STAMINA_MEDIUM,    /* 4: 50 */
        STAMINA_LARGE,     /* 5: 100 */
        HEALTH_TINY,       /* 6: 10 */
        STAMINA_TINY,      /* 7: 10; update never regenerates it */
        SPECIAL_WEAPON,    /* 8 */
        CLOAK,             /* 9 */
        NUM_TYPES = 20     /* 10..19: story-mode pickups (text box message type - 10) */
    };

    unsigned short m_flags;   /* 0x0: bit 0 has a shadow, bit 1 update re-places the shadow (with bit 0), bit 2 placeShadow also lowers the
                                 rest height (trans.w), bit 4 update runs testHidden again, bit 5 hidden (something is above it), bits 6/7
                                 hidden by an interactive (its +0x30 == 1 or not), bit 8 hidden by a cinema */
    signed char m_type;       /* 0x2 */
    unsigned char m_index;    /* 0x3: slot when it was created (KillPowerUp does not update it) */
    short m_timer;            /* 0x4: fields until it reappears after being taken */
    unsigned short m_hiddenBy; /* 0x6: interactive id found above it (testHidden) */
    _cs *m_cs;                /* 0x8: trans.w is the rest height it eases down to */
    _cs *m_shadow;            /* 0xC: null without bit 0 */

    void testHidden(void);
    void placeShadow(void);
};

class PowerUps {
public:
    _hierhead *m_epNodes[20];        /* 0x0000: model of each type (hat id2 == 0) */
    _hierhead *m_shadowEpNodes[20];  /* 0x0050: its shadow (hat id2 != 0) */
    PowerUp m_powerUps[100];         /* 0x00A0: m_count in use, kept packed */
    int m_byType[100][20];           /* 0x06E0: [n][type] = index of the n-th power-up of that type (-1 unused) */
    int m_byTypeCount[20];           /* 0x2620 */
    int m_count;                     /* 0x2670 */
    int m_regenMin[20];              /* 0x2674: in fields (seconds * (int)(60 * multiplier)) */
    int m_regenMax[20];              /* 0x26C4 */

    static PowerUps instance;

    void initPowerUpsBefore(void);
    void initPowerUpsAfter(void);
    void update(void);
    int CreatePowerUp(int type, _fvector *pos);
    int CreatePowerUp(int type, unsigned char flags, _fvector *pos);
    void addPowerUpEpNode(_hierhead *h);
    void KillPowerUps(void);
    void KillPowerUp(int i);
    void KillPowerUp(_cs *cs);
    void RemovePowerUpsByTypeEntry(int i);
    void AddPowerUpsByTypeEntry(int i);
    void ChangePowerUpsByTypeEntry(int from, int to);
    void ChangePowerUpType(int i, int type);
    void setRegenTimeMultiplier(float m);
    void setRegenTime(PowerUp::Type type, int min, int max);
    void setCredit(PowerUp::Type type, int credit);
    void setCloakTime(int t);
    float getRegenTimeMultiplier(void);
    int getMinRegenTime(PowerUp::Type type);
    int getMaxRegenTime(PowerUp::Type type);
    int getCredit(PowerUp::Type type);
    int getCloakTime(void);
    void LoadRegenTimes(void);
    void TakeHit(Monster *m, _cs *cs);
    _cs *GetClosestPowerUp(_fvector *pos, int type);
    void startCinema(void);
    void endCinema(void);
};
typedef char _size_PowerUp[sizeof(PowerUp) == 0x10 ? 1 : -1];
typedef char _size_PowerUps[sizeof(PowerUps) == 0x2714 ? 1 : -1];

#endif
