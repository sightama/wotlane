#ifndef MONSTER_SELECT_H
#define MONSTER_SELECT_H

/* One monster-select slot (0x2C bytes); the table holds groups of four per monster (stride 0xB0), then the unlock
 * flags of the levels. */
struct SelectSlot {
    int taken;      /* 0x00: the slot holds a model (cleared when an AI is given this monster) */
    int state;      /* 0x04: 1 = selectable (unlocked) */
    int cost;       /* 0x08: battle tokens (Shell+0x2B24) needed to unlock it on the unlock screen */
    char costText[7]; /* 0x0C: shown on the unlock screen while it is locked */
    char name[25];  /* 0x13: name shown on the unlock screen */
};
typedef char _size_SelectSlot[sizeof(SelectSlot) == 0x2C ? 1 : -1];
extern SelectSlot monsterSelectMode[];

#endif
