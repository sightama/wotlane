#include "common.h"
#include "game/hit_history.h"

extern "C" int printf(const char *, ...);

struct DbInteractive;
struct _fvector;
class HurtHistory {
public:
    float creditHit(int id, float amount);
};

class Interactives {
public:
    static DbInteractive *s_dbInteractive[1024];
    static int s_numInteractives;

    static void init(void);
    static int addInteractive(DbInteractive *p); /* returns the slot: callers store it as the object's hat id */
    static void setInteractive(int i, DbInteractive *p);
    static DbInteractive *getInteractive(int i);
    static void dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId);
};

#ifdef NON_MATCHING
/* 10/14 words, untuned: zeroing loop shape */
void Interactives::init(void)
{
    int i;

    for (i = 1023; i >= 0; i--)
        s_dbInteractive[i] = 0;
    s_numInteractives = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Interactives", init__12Interactives);
#endif
#ifdef NON_MATCHING
/* 5/11 words, untuned: order of count update and store */
int Interactives::addInteractive(DbInteractive *p)
{
    int i = s_numInteractives++;

    s_dbInteractive[i] = p;
    return i;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Interactives", addInteractive__12InteractivesP13DbInteractive);
#endif
void Interactives::setInteractive(int i, DbInteractive *p)
{
    s_dbInteractive[i] = p;
}
DbInteractive *Interactives::getInteractive(int i)
{
    if (i > 0 && i < s_numInteractives)
        return s_dbInteractive[i];
    printf("interact Error:  Bad index passed to GetInteractive %d numInteractives %d\n", i, s_numInteractives);
    return s_dbInteractive[0];
}
#ifdef NON_MATCHING
/* Delivers a hit to interactive `targetId` through its virtual takeHit(dir, dmg, attackerId). */
void Interactives::dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId)
{
    if (targetId > 0 && targetId < s_numInteractives) {
        char *t = (char *)s_dbInteractive[targetId];
        char *vt = *(char **)(t + 0x10);

        ((void (*)(void *, _fvector *, int, float))*(void **)(vt + 0xC))(t + *(short *)(vt + 8), dir, attackerId, dmg);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Interactives", dispatchHit__12InteractivesP8_fvectorfii);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Interactives", create__8HitEvent);
HitHistory::HitHistory()
{
    f0 = 0;
    f4 = 0;
}
#ifdef NON_MATCHING
/* 1/3 words, untuned: store order */
void HitHistory::reset(void)
{
    f0 = 0;
    f4 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Interactives", reset__10HitHistory);
#endif
#ifdef NON_MATCHING
/* Registers a hit on interactive `id`; 1 = first time this swing hits it, 0 = already hit, table full, or (when `b`) a monster whose attacks are off.
   id 0 is a miss: counted in f4. The ids hit so far are a table of 16 at +8. */
int HitHistory::newHit(int id, bool b)
{
    int i;
    int *ids = (int *)((char *)this + 8);

    if (id == 0) {
        f4++;
        return 0;
    }
    if (b) {
        char *it = (char *)Interactives::getInteractive(id);

        if (*(int *)it == 1 && *(signed char *)(it + 0x49) == 0)
            return 0;
    }
    for (i = 0; i < f0; i++) {
        if (ids[i] == id)
            return 0;
    }
    if (f0 >= 16)
        return 0;
    ids[f0] = id;
    f0++;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Interactives", newHit__10HitHistoryib);
#endif
#ifdef NON_MATCHING
/* Damage taken per attacker: 16 entries of (id, total) with the count at +0x80; returns the new total (0 when the table is full). */
float HurtHistory::creditHit(int id, float amount)
{
    int *count = (int *)((char *)this + 0x80);
    int i;

    for (i = 0; i < *count; i++) {
        if (*(int *)((char *)this + i * 8) == id) {
            float *total = (float *)((char *)this + 4 + i * 8);

            *total = *total + amount;
            return *total;
        }
    }
    if (*count >= 16)
        return 0.0f;
    i = (*count)++;
    *(int *)((char *)this + i * 8) = id;
    *(float *)((char *)this + i * 8 + 4) = amount;
    return amount;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Interactives", creditHit__11HurtHistoryif);
#endif
/* retail rodata keeps an empty string plus alignment after the last literal of this TU */
__asm__(".section .rodata
	.word 0
	.text");
