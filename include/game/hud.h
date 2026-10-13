#ifndef HUD_H
#define HUD_H

#include "engine.h"

/* One per player view; the array starts at the beginning of TheGame (stride 0x2E0), see gameHud(). */
class Hud {
public:
    int view;                   /* 0x00: index of this hud's view */
    char pad4[4];
    int comboHits;              /* 0x08 */
    int staminaCredit;          /* 0x0C: energy gained since the message appeared */
    int healthCredit;           /* 0x10 */
    int msgId[3];               /* 0x14: the three message slots (0 = free) */
    char pad20[0x2C - 0x20];
    int msgTimer[3];            /* 0x2C */
    char pad38[0x50 - 0x38];
    int msgAlpha[3];            /* 0x50 */
    char pad5C[0x98 - 0x5C];
    _cs *m_cs1F43;              /* 0x98: cs of HUD element 0x1F43 (hidden) */
    _cs *m_mcButtonCs[4];       /* 0x9C: cs of the memory card buttons 0x1F51..0x1F54 */
    char padAC[0xD0 - 0xAC];
    int f0D0;
    char pad0D4[0xE4 - 0xD4];
    _hierhead *m_node1F47;      /* 0xE4: HUD node 0x1F47 (hud 1 gets the one with a nonzero id2) */
    char padE8[0xEC - 0xE8];
    _hierhead *m_node1F4D;      /* 0xEC: HUD node 0x1F4D (same split) */
    _hierhead *m_node1F55;      /* 0xF0: HUD node 0x1F55 (hud 0 only) */
    char padF4[0x1D0 - 0xF4];
    _animHandle h1D0;
    char pad1E0[0x2E0 - 0x1E0];

    void initForReplay(void);
    static void parseHudElements(_hierhead *h);
    void addMessage(int a, int b);
    void addTextBoxMessage(int id);
    void registerHealthCredit(int amount);
    void registerComboHit(void);
    void registerStaminaCredit(int amount);
    void print(void);
};

typedef char _size_Hud[sizeof(Hud) == 0x2E0 ? 1 : -1];

#endif
