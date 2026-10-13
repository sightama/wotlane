#include "common.h"
#include "game/pickup.h"
#include "game/game.h"
#include "vecmath.h"
#include "memory_stack.h"
#include "game/military_pickup.h"
#include "game/military_formation.h"

__asm__("#SNFIX_SMALL game");

void hdReparentCsGrid(_cs *cs, _fvector *pos, float f);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
class Weapon {
public:
    void GetVel(_fvector &v);
    void KillWeapon(void);
};
class DodgeBallLevel {
public:
    void ResetCountdown(void);
};
extern char dodgeBallInst[] __asm__("_14DodgeBallLevel$instance");
void hdRemoveCsFromGrid(_cs *cs);
extern _fvector D_00731340;
/* Intrusive list node of LevelPickups' lists (the owner's data word, next, prev); the heads are LevelPickups' static sentinels. */
struct ListNode {
    void *data;
    ListNode *next;
    ListNode *prev;
};
extern ListNode inUseList __asm__("_12LevelPickups$s_inUseList");
extern ListNode inFlightList __asm__("_12LevelPickups$s_inFlightList");
extern ListNode activeList __asm__("_12LevelPickups$s_activeList");
extern ListNode *highlightList[] __asm__("_12LevelPickups$s_highlightPickup");
struct DbInteractive;
class Interactives {
public:
    static int addInteractive(DbInteractive *p);
    static DbInteractive *getInteractive(int i);
};
class TankPickup : public MilitaryPickup {
public:
    void enterState(State s);
};
class MissileTruckPickup : public MilitaryPickup {
public:
    void enterState(State s);
};
class HeliPickup : public Pickup {
public:
    enum State { STATE_0, STATE_1, STATE_2, STATE_3, STATE_4, STATE_5 };
    void enterState(State s);
};
void *operator new(unsigned, void *);
int hdCsCollect2D(_fvector *pos, float radius, unsigned a, unsigned b, _cs **out, int c);
int hdCsCollect(_fvector *pos, float radius, unsigned a, unsigned b, _cs **out, int c);
extern int moviePlaying;
/* highlight color pulse: green goes up and down by colorInc between 128 and 255 */
extern unsigned short highlightGreen __asm__("_12LevelPickups$s_green");
extern unsigned short highlightColor __asm__("_12LevelPickups$s_color");
extern signed char highlightColorInc __asm__("_12LevelPickups$s_colorInc");
extern ListNode deadList __asm__("_12LevelPickups$s_deadList");
extern ListNode regenList __asm__("_12LevelPickups$s_regenList");
extern int militaryStillAlive __asm__("_12LevelPickups$s_militaryStillAlive");
class RigidPickup {
public:
    void huck(_fvector &dir, DbInteractive *target);
};
class StatePickUp {
public:
    void getAttachRegion(_fvector &a, _fvector &b);
};
class CsPool {
public:
    static _cs *csActivate(void);
};
void hierSetCsEpNode(_cs *cs, _hierhead *ep);
/* the two cs that draw the highlight around the pickup a monster can grab */
extern _cs *highlightCs[2] __asm__("_12LevelPickups$s_highlight");


INCLUDE_ASM("asm/nonmatchings/game/Pickup", __6Pickup);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", init__6PickupP9_hierheadR8_fvectorPA3_fb11ePickupType);
void Pickup::initAfterDbLoad(void)
{
    int id = Interactives::addInteractive((DbInteractive *)this);
    unsigned *head = (unsigned *)cs->epNode;

    *head = (*head & 0xFFFC007F) | ((id & 0x7FF) << 7);
    hdReparentCsGrid(cs, (_fvector *)((char *)cs + 0x10), 0.0f);
}
bool Pickup::update(void)
{
    ((PickupSound *)((char *)this + 0x100))->updatePickupSound();
    unsigned long long b = bits & 0x40;

    if (b)
        hatCheck();
    return health > 0.0f;
}
bool Pickup::regenUpdate(void)
{
    if (--regenTimer <= 0) {
        unsigned long long b = bits & 1;

        if (b)
            return true;
    }
    return false;
}
#ifdef NON_MATCHING
/* 21/25 words: register choice for the home pointer and the lq/sq asm */
void Pickup::regen(void)
{
    register int t __asm__("$2");

    regenTimer = 0xA8C;
    cs->drawMe = 1;
    health = maxHealth;
    __asm__ volatile("lq %1, 0(%2)
	sq %1, %0" : "=m"(cs->trans), "=r"(t) : "r"((char *)this + 0x20));
    mathfRotMatrixRPH((float (*)[4])((char *)cs + 0x20), (_fvector *)((char *)this + 0x30));
    hatCheck();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", regen__6Pickup);
#endif
void Pickup::takeHit(_fvector *pos, float dmg, int x)
{
    health -= dmg;
}
void Pickup::takeUseDamage(float dmg)
{
    health -= dmg;
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", isTargetClear__6PickupP13DbInteractive);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", grab__6Pickupi);
void Pickup::drop(void)
{
    hatCheck();
    heldState = 0;
    setVisualState(0);
    hdReparentCsGrid(cs, (_fvector *)((char *)cs + 0x10), 0.0f);
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", kill__6Pickup);
void Pickup::initConfig(void)
{
    health = maxHealth = LevelPickups::s_info[pickupType].health;
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", updateCS__6PickupP8_fvectorRA3_A3_fb);
void Pickup::getVel(_fvector &v)
{
    if (weapIdx >= 0)
        ((Weapon *)((char *)gameWeapons() + (weapIdx * 0x170 + 0xC00)))->GetVel(v);
    else
        vecScale(&v, (_fvector *)((char *)cs + 0x30), speed);
}
_fvector *Pickup::getVel(void)
{
    char *vt = *(char **)((char *)this + 0x10);

    (*(void (**)(void *, _fvector *))(vt + 0x2C))((char *)this + *(short *)(vt + 0x28), &D_00731340);
    return &D_00731340;
}
void Pickup::setVisualState(int s)
{
    HierSwitch *sw = *(HierSwitch **)((char *)cs->epNode + 0x20);

    if (sw->head.opcode == 6)
        sw->whichChild = (s < sw->numKids) ? s : sw->numKids - 1;
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", hatCheck__6Pickup);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initLocators__6PickupP9_hierhead);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initHandle__6PickupP14_hiertranslate);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initHandle__6PickupP11_hierrotate);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initCollisSpheres__6PickupP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", swipeTrailEffect__6Pickup);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", collisTestBatAttack__6PickupfUi11MonsterAnimR10HitHistoryfff);
_fvector *Pickup::getCollisSphereTrans(int i)
{
    return (_fvector *)((char *)this + 0x110) + i;
}
float Pickup::getCollisSphereRadius(int i)
{
    return *(float *)((char *)this + (i << 4) + 0x11C);
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", updateGenerator__6Pickup);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", init__12LevelPickups);
/* Prunes every active vehicle pickup (types 1-16, 23-24 and 26-27; 25 too unless the game mode is 1 on Three Mile Island). */
#ifdef NON_MATCHING
/* 21/49 words: switch layout */
void LevelPickups::turnOffAllVehicles(void)
{
    ListNode *n = activeList.next;

    while (n != &activeList) {
        switch (((Pickup *)n->data)->pickupType) {
        case 25:
            if (game->m_gameMode == 1 && game->m_levelId == 6)
                break;
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16:
        case 23: case 24: case 26: case 27: {
            PickupIter it;

            it.cur = (Pickup *)n;
            n = n->next;
            prunePickup(it);
            continue;
        }
        }
        n = n->next;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", turnOffAllVehicles__12LevelPickups);
#endif
/* Military pickups (tanks 5 and 10, missile truck 9, helicopters 11, 12 and 23) go to their parked state and stop drawing. */
#ifdef NON_MATCHING
/* matches with NON_MATCHING enabled; kept out of the retail build because the switch jump table must come from the retail rodata */
void LevelPickups::turnOffMilitary(void)
{
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        int military = 1;

        switch (((Pickup *)n->data)->pickupType) {
        case 5:
        case 10:
            ((TankPickup *)n->data)->enterState(MilitaryPickup::STATE_7);
            break;
        case 9:
            ((MissileTruckPickup *)n->data)->enterState(MilitaryPickup::STATE_7);
            break;
        case 11:
        case 12:
        case 23:
            ((HeliPickup *)n->data)->enterState(HeliPickup::STATE_5);
            break;
        default:
            military = 0;
        }
        if (military) {
            _cs *cs = ((Pickup *)n->data)->cs;

            if (cs != 0)
                cs->drawMe = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", turnOffMilitary__12LevelPickups);
#endif
/* The reverse of turnOffMilitary: state 0 and drawn again. */
#ifdef NON_MATCHING
/* matches with NON_MATCHING enabled; kept out of the retail build because the switch jump table must come from the retail rodata */
void LevelPickups::turnOnMilitary(void)
{
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        int military = 1;

        switch (((Pickup *)n->data)->pickupType) {
        case 5:
        case 10:
            ((TankPickup *)n->data)->enterState(MilitaryPickup::STATE_0);
            break;
        case 9:
            ((MissileTruckPickup *)n->data)->enterState(MilitaryPickup::STATE_0);
            break;
        case 11:
        case 12:
        case 23:
            ((HeliPickup *)n->data)->enterState(HeliPickup::STATE_0);
            break;
        default:
            military = 0;
        }
        if (military) {
            _cs *cs = ((Pickup *)n->data)->cs;

            if (cs != 0)
                cs->drawMe = 1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", turnOnMilitary__12LevelPickups);
#endif
/* Prunes every active two-handed pickup (types 13 and 17-22). */
#ifdef NON_MATCHING
/* 28/56 words: the retail keeps the type constants in s registers */
void LevelPickups::turnOff2Handeds(void)
{
    ListNode *n = activeList.next;

    while (n != &activeList) {
        int t = ((Pickup *)n->data)->pickupType;

        if (t == 13 || t == 17 || t == 18 || t == 19 || t == 20 || t == 21 || t == 22) {
            PickupIter it;

            it.cur = (Pickup *)n;
            n = n->next;
            prunePickup(it);
        } else {
            n = n->next;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", turnOff2Handeds__12LevelPickups);
#endif
/* Central: every tank (type 10) is shown and joins one 2-column formation; the first one leads. */
#ifdef NON_MATCHING
/* 5/81 words: the formation constants are loaded before the allocation */
void LevelPickups::initMilitaryForCentral(void)
{
    BoxFormation *f = new (MemoryStack::global.alloc16(sizeof(BoxFormation))) BoxFormation(2, 65.0f, 150.0f);
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        MilitaryPickup *p = (MilitaryPickup *)n->data;

        if (p->pickupType == 10) {
            p->cs->drawMe = 1;
            if (f->leader == 0)
                f->setLeader(p);
            else
                f->addMember(p);
        }
    }
    if (f->leader != 0)
        f->initFormation();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initMilitaryForCentral__12LevelPickups);
#endif
/* Vegas: two 2-column formations of up to 4 tanks (type 10) and 2 missile trucks (type 9) each, placed at fixed spots; extra tanks and
   trucks are hidden. Helicopters (11, 12, 23) are shown in state 0. */
#ifdef NON_MATCHING
/* 34/218 words: register allocation and branch layout of the type test */
void LevelPickups::initMilitaryForVegas(void)
{
    BoxFormation *a = new (MemoryStack::global.alloc16(sizeof(BoxFormation))) BoxFormation(2, 83.0f, 150.0f);
    BoxFormation *b = new (MemoryStack::global.alloc16(sizeof(BoxFormation))) BoxFormation(2, 83.0f, 150.0f);
    int tanksA = 4;
    int tanksB = 4;
    int trucksA = 2;
    int trucksB = 2;
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        MilitaryPickup *p = (MilitaryPickup *)n->data;

        switch (p->pickupType) {
        case 11:
        case 12:
        case 23:
            ((Pickup *)n->data)->cs->drawMe = 1;
            ((HeliPickup *)n->data)->enterState(HeliPickup::STATE_0);
            break;
        case 10:
            if (tanksA-- > 0) {
                p->cs->drawMe = 1;
                if (a->leader == 0)
                    a->setLeader(p);
                else
                    a->addMember(p);
            } else if (tanksB-- > 0) {
                p->cs->drawMe = 1;
                if (b->leader == 0)
                    b->setLeader(p);
                else
                    b->addMember(p);
            } else {
                p->cs->drawMe = 0;
            }
            break;
        }
    }
    for (n = activeList.next; n != &activeList; n = n->next) {
        MilitaryPickup *p = (MilitaryPickup *)n->data;

        if (p->pickupType == 9) {
            if (trucksA-- > 0) {
                p->cs->drawMe = 1;
                a->addMember(p);
            } else if (trucksB-- > 0) {
                p->cs->drawMe = 1;
                b->addMember(p);
            } else {
                p->cs->drawMe = 0;
            }
        }
    }
    if (a->leader != 0) {
        _cs *cs = ((Pickup *)a->leader)->cs;

        cs->trans.x = 167.0f;
        cs->trans.y = -2030.0f;
        cs->trans.z = 0.2f;
        a->initFormation();
    }
    if (b->leader != 0) {
        _cs *cs = ((Pickup *)b->leader)->cs;

        cs->trans.x = -89.0f;
        cs->trans.y = -2030.0f;
        cs->trans.z = 0.2f;
        b->initFormation();
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initMilitaryForVegas__12LevelPickups);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Pickup", createPickup__12LevelPickupsP9_hierheadR8_fvectorPA3_fb11ePickupType);
/* Calls initAfterDbLoad() (virtual) on every active pickup. */
#ifdef NON_MATCHING
/* 3/25 words: the list head lands in s1 later */
void LevelPickups::initAfterDbLoad(void)
{
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        char *p = (char *)n->data;
        char *vt = *(char **)(p + 0x10);

        ((void (*)(void *))*(void **)(vt + 0x34))(p + *(short *)(vt + 0x30));
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initAfterDbLoad__12LevelPickups);
#endif
/* Creates (once) the two highlight cs from `ep`, hidden and without collision. */
void LevelPickups::createPickupHighlight(_hierhead *ep)
{
    if (highlightCs[0] == 0) {
        highlightCs[0] = CsPool::csActivate();
        hierSetCsEpNode(highlightCs[0], ep);
        highlightCs[0]->drawMe = 0;
        highlightCs[0]->testCollision = 0;
        highlightCs[1] = CsPool::csActivate();
        hierSetCsEpNode(highlightCs[1], ep);
        highlightCs[1]->drawMe = 0;
        highlightCs[1]->testCollision = 0;
    }
}
/* Hides the free and thrown pickups during a cinema; rot.w of the cs remembers whether it was drawn. A rigid-body pickup whose
   word at 0x174 is 1 is left alone. */
#ifdef NON_MATCHING
/* 47/62 words: register allocation */
void LevelPickups::startCinema(void)
{
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        Pickup *p = (Pickup *)n->data;

        if (((int)(p->bits >> 2) & 1) && *(int *)((char *)p + 0x174) == 1)
            continue;
        if (p->cs->drawMe) {
            p->cs->drawMe = 0;
            ((Pickup *)n->data)->cs->rot.w = 1.0f;
        } else {
            p->cs->rot.w = 0.0f;
        }
    }
    for (n = inFlightList.next; n != &inFlightList; n = n->next) {
        Pickup *p = (Pickup *)n->data;

        if (((int)(p->bits >> 2) & 1) && *(int *)((char *)p + 0x174) == 1)
            continue;
        if (p->cs->drawMe) {
            p->cs->drawMe = 0;
            ((Pickup *)n->data)->cs->rot.w = 1.0f;
        } else {
            p->cs->rot.w = 0.0f;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", startCinema__12LevelPickups);
#endif
/* Shows again the pickups startCinema hid. */
#ifdef NON_MATCHING
/* 31/44 words: register allocation */
void LevelPickups::endCinema(void)
{
    ListNode *n;

    for (n = activeList.next; n != &activeList; n = n->next) {
        _cs *cs = ((Pickup *)n->data)->cs;

        if (!cs->drawMe && 0.0f < cs->rot.w)
            cs->drawMe = 1;
    }
    for (n = inFlightList.next; n != &inFlightList; n = n->next) {
        _cs *cs = ((Pickup *)n->data)->cs;

        if (!cs->drawMe && 0.0f < cs->rot.w)
            cs->drawMe = 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", endCinema__12LevelPickups);
#endif
/* Per-frame update. Active pickups run update() (virtual) and are killed (to the regen list if regenable) when it returns false; held
   and thrown pickups update their sound and, for rigid bodies, their physics; regenerating pickups whose timer ran out go back to the
   active list and regen(). */
#ifdef NON_MATCHING
/* 4/127 words: register allocation and branch layout */
void LevelPickups::update(void)
{
    ListNode *n;
    ListNode *next;
    char *p;
    char *vt;

    militaryStillAlive = 0;
    n = activeList.next;
    while (n != &activeList) {
        p = (char *)n->data;
        vt = *(char **)(p + 0x10);
        if (((bool (*)(void *))*(void **)(vt + 0x64))(p + *(short *)(vt + 0x60))) {
            n = n->next;
        } else {
            PickupIter it;

            next = n->next;
            it.cur = (Pickup *)n;
            killPickup(it, 1);
            n = next;
        }
    }
    for (n = inUseList.next; n != &inUseList; n = next) {
        p = (char *)n->data;
        next = n->next;
        if ((int)(((Pickup *)p)->bits >> 2) & 1) {
            vt = *(char **)(p + 0x10);
            ((bool (*)(void *))*(void **)(vt + 0x64))(p + *(short *)(vt + 0x60));
        }
        ((PickupSound *)((char *)n->data + 0x100))->updatePickupSound();
    }
    for (n = inFlightList.next; n != &inFlightList; n = next) {
        p = (char *)n->data;
        next = n->next;
        if ((int)(((Pickup *)p)->bits >> 2) & 1) {
            vt = *(char **)(p + 0x10);
            ((bool (*)(void *))*(void **)(vt + 0x64))(p + *(short *)(vt + 0x60));
        }
        if (((Pickup *)n->data)->pickupType != 27)
            ((PickupSound *)((char *)n->data + 0x100))->updatePickupSound();
    }
    n = regenList.next;
    while (n != &regenList) {
        if (((Pickup *)n->data)->regenUpdate()) {
            next = n->next;
            if (n->prev != 0)
                n->prev->next = n->next;
            if (n->next != 0)
                n->next->prev = n->prev;
            n->next = 0;
            n->prev = &activeList;
            n->next = activeList.next;
            activeList.next = n;
            if (n->next != 0)
                n->next->prev = n;
            p = (char *)n->data;
            n = next;
            vt = *(char **)(p + 0x10);
            ((void (*)(void *))*(void **)(vt + 0x3C))(p + *(short *)(vt + 0x38));
        } else {
            n = n->next;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", update__12LevelPickups);
#endif
/* Pickups whose cs lies within sqrt(r2) of `p` (helicopters excluded) redo their hat check: a rigid body is flagged to do it in its own
   update, a hidden pickup waiting for a delayed hat (bit 3 of `bits`) is shown again first. */
#ifdef NON_MATCHING
/* 53/75 words: eeSqrtf moves (retail sqrt.s reads $f12) and register allocation */
void LevelPickups::hatCheck(_fvector *p, float r2)
{
    _cs *list[100];
    int n = hdCsCollect2D(p, eeSqrtf(r2), 5000, 5099, list, 100);
    int i;

    for (i = 0; i < n; i++) {
        Pickup *pk = (Pickup *)Interactives::getInteractive((*(unsigned *)list[i]->epNode >> 7) & 0x7FF);
        int t = pk->pickupType;

        if (t >= 11 && (t < 13 || t == 23))
            continue;
        if (pk->cs->drawMe) {
            if ((int)(pk->bits >> 2) & 1) {
                *(int *)((char *)pk + 0x170) = 0;
                *(unsigned long long *)((char *)pk + 0x4A0) |= 1;
            } else {
                pk->hatCheck();
            }
        } else if (pk->bits & 8) {
            pk->cs->drawMe = 1;
            pk->bits &= ~8;
            pk->hatCheck();
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", hatCheck__12LevelPickupsP8_fvectorf);
#endif
/* Hides the pickups near `p` until their hat check runs (bit 3 of `bits`); rigid bodies are flagged instead. Helicopters are left alone. */
#ifdef NON_MATCHING
/* 1/64 words: eeSqrtf moves and the loop counter register */
void LevelPickups::turnOffForDelayedHat(_fvector *p, float r2)
{
    _cs *list[100];
    int n = hdCsCollect2D(p, eeSqrtf(r2), 5000, 5099, list, 100);
    int i;

    for (i = 0; i < n; i++) {
        Pickup *pk = (Pickup *)Interactives::getInteractive((*(unsigned *)list[i]->epNode >> 7) & 0x7FF);
        int t = pk->pickupType;

        if (t >= 11 && (t < 13 || t == 23))
            continue;
        if ((int)(pk->bits >> 2) & 1) {
            *(int *)((char *)pk + 0x170) = 0;
            *(unsigned long long *)((char *)pk + 0x4A0) |= 1;
        } else {
            pk->cs->drawMe = 0;
            pk->bits |= 8;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", turnOffForDelayedHat__12LevelPickupsP8_fvectorf);
#endif
/* Restores the color of the monster's current highlighted pickup and picks the closest one in its attach region. */
#ifdef NON_MATCHING
/* 3/52 words: register allocation of the slot pointer */
void LevelPickups::computeHighlight(Monster &m)
{
    int idx = *(int *)((char *)&m + 0x28);
    ListNode *h = highlightList[idx];
    _fvector a, b;
    int i;
    bool lit = h != 0 && ((Pickup *)h->data)->cs != 0;

    if (lit) {
        for (i = 0; i < 3; i++)
            ((Pickup *)h->data)->cs->colorQuad.fVec[i] = 1.0f;
    }
    ((StatePickUp *)((char *)&m + 0xDC70))->getAttachRegion(a, b);
    highlightList[idx] = (ListNode *)getClosestPickup(a, b);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", computeHighlight__12LevelPickupsR7Monster);
#endif
/* Places player i's highlight on its highlighted pickup and pulses the pickup's color (green between 128 and 255, red/blue fading
   to at most 128). With two or more players in split view, the other player's highlight is hidden and its pickup's color restored,
   unless both highlight the same one. */
#ifdef NON_MATCHING
/* 5/184 words: the retail duplicates the other-player block for i == 0 and i != 0 */
void LevelPickups::enableHighlights(int i)
{
    _cs *hl = highlightCs[i];
    ListNode *h;
    ListNode *other;
    int o;
    int k;

    if (hl == 0 || moviePlaying)
        return;
    h = highlightList[*(int *)((char *)game->m_monsters[i] + 0x28)];
    if (h == 0) {
        hl->drawMe = 0;
    } else {
        hl->trans = ((Pickup *)h->data)->cs->trans;
        highlightCs[i]->trans.y -= *(float *)((char *)h->data + 0xD4);
        highlightCs[i]->drawMe = 1;
        *((unsigned char *)highlightCs[i]->epNode + 8) = 0;
        highlightGreen += highlightColorInc;
        if (highlightColor >= 6 || highlightColorInc < 0)
            highlightColor -= highlightColorInc;
        if (highlightGreen >= 255) {
            highlightGreen = 255;
            highlightColorInc = -highlightColorInc;
        } else if (highlightGreen <= 128) {
            highlightGreen = 128;
            highlightColorInc = -highlightColorInc;
        }
        if (highlightColor > 128)
            highlightColor = 128;
        ((Pickup *)h->data)->cs->colorQuad.fVec[1] = highlightGreen * (1.0f / 128.0f);
        ((Pickup *)h->data)->cs->colorQuad.fVec[0] = highlightColor * (1.0f / 128.0f);
        ((Pickup *)h->data)->cs->colorQuad.fVec[2] = highlightColor * (1.0f / 128.0f);
    }
    if (gUseUnifiedView || game->m_numMonsters < 2)
        return;
    o = (i == 0) ? 1 : 0;
    highlightCs[o]->drawMe = 0;
    other = highlightList[*(int *)((char *)game->m_monsters[o] + 0x28)];
    if (other != 0 && other != h) {
        for (k = 0; k < 3; k++)
            ((Pickup *)other->data)->cs->colorQuad.fVec[k] = 1.0f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", enableHighlights__12LevelPickupsi);
#endif
/* Moves the pickup's list node to the head of the in-use list. */
void LevelPickups::impalePickup(PickupIter it)
{
    ListNode *n = (ListNode *)it.cur;

    if (n->prev != 0)
        n->prev->next = n->next;
    if (n->next != 0)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = &inUseList;
    n->next = inUseList.next;
    inUseList.next = n;
    if (n->next != 0)
        n->next->prev = n;
}
/* Moves the pickup's node to the in-use list and calls its grab(i). */
void LevelPickups::grabPickup(PickupIter it, int i)
{
    ListNode *n = (ListNode *)it.cur;
    char *p;
    char *vt;

    if (n->prev != 0)
        n->prev->next = n->next;
    if (n->next != 0)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = &inUseList;
    n->next = inUseList.next;
    inUseList.next = n;
    if (n->next != 0)
        n->next->prev = n;
    p = (char *)n->data;
    vt = *(char **)(p + 0x10);
    ((void (*)(void *, int))*(void **)(vt + 0x44))(p + *(short *)(vt + 0x40), i);
}
/* Moves the pickup's node to the active list and calls its drop(). */
void LevelPickups::dropPickup(PickupIter it)
{
    ListNode *n = (ListNode *)it.cur;
    char *p;
    char *vt;

    if (n->prev != 0)
        n->prev->next = n->next;
    if (n->next != 0)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = &activeList;
    n->next = activeList.next;
    activeList.next = n;
    if (n->next != 0)
        n->next->prev = n;
    p = (char *)n->data;
    vt = *(char **)(p + 0x10);
    ((void (*)(void *))*(void **)(vt + 0x4C))(p + *(short *)(vt + 0x48));
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", throwPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractiveT3);
/* A rigid-body pickup is thrown: its node moves to the in-flight list, nobody holds it, flying visual state, and the body is hucked. */
void LevelPickups::throwRBPickup(PickupIter it, _fvector &dir, DbInteractive *target)
{
    ListNode *n = (ListNode *)it.cur;
    Pickup *p = (Pickup *)n->data;

    if ((int)(p->bits >> 2) & 1) {
        if (n->prev != 0)
            n->prev->next = n->next;
        if (n->next != 0)
            n->next->prev = n->prev;
        n->next = 0;
        n->prev = &inFlightList;
        n->next = inFlightList.next;
        inFlightList.next = n;
        if (n->next != 0)
            n->next->prev = n;
        p->heldState = 0;
        p->setVisualState(1);
        ((RigidPickup *)p)->huck(dir, target);
    }
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", deflectThrownPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractiveT3);
#ifdef NON_MATCHING
/* A thrown pickup is caught by slot `i`: back to the in-use list, held visual state, its weapon (if any, unless bit 1 of `bits`) is killed;
   dodgeball restarts its countdown. */
void LevelPickups::grabThrownPickup(PickupIter it, int i)
{
    ListNode *n = (ListNode *)it.cur;
    Pickup *p;

    if (n->prev != 0)
        n->prev->next = n->next;
    if (n->next != 0)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = &inUseList;
    n->next = inUseList.next;
    inUseList.next = n;
    if (n->next != 0)
        n->next->prev = n;
    ((Pickup *)n->data)->heldState = i;
    ((Pickup *)n->data)->setVisualState(2);
    p = (Pickup *)n->data;
    if (!((p->bits >> 1) & 1)) {
        if (p->weapIdx >= 0) {
            ((Weapon *)((char *)game + 0x112490 + p->weapIdx * 0x170 + 0xC00))->KillWeapon();
            ((Pickup *)n->data)->weapIdx = -1;
        }
    }
    if (*(int *)((char *)game + 0x1203C8) == 7)
        ((DodgeBallLevel *)dodgeBallInst)->ResetCountdown();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", grabThrownPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori);
#endif
#ifdef NON_MATCHING
/* Removes the pickup from the world: its cs leaves the collision grid and stops drawing, the node leaves its list, and no highlight slot keeps it. */
void LevelPickups::prunePickup(PickupIter it)
{
    ListNode *n = (ListNode *)it.cur;
    int k;

    hdRemoveCsFromGrid(((Pickup *)n->data)->cs);
    ((Pickup *)n->data)->cs->drawMe = 0;
    ((Pickup *)n->data)->heldState = 0;
    if (n->prev != 0)
        n->prev->next = n->next;
    if (n->next != 0)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = 0;
    for (k = 0; k < 16; k++) {
        if (highlightList[k] == n)
            highlightList[k] = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", prunePickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iterator);
#endif
/* Kills the pickup: its cs leaves the grid and stops drawing, kill() (virtual) runs, the holder (if any) lets go, and the node moves
   to the regen list (regenable pickup and `regen` set) or the dead list; no highlight slot keeps it. */
#ifdef NON_MATCHING
/* 52/76 words: branch layout of the list choice */
void LevelPickups::killPickup(PickupIter it, int regen)
{
    ListNode *n = (ListNode *)it.cur;
    ListNode *list;
    char *p;
    char *vt;
    int k;

    if (((Pickup *)n->data)->cs == 0)
        return;
    hdRemoveCsFromGrid(((Pickup *)n->data)->cs);
    ((Pickup *)n->data)->cs->drawMe = 0;
    p = (char *)n->data;
    vt = *(char **)(p + 0x10);
    ((void (*)(void *))*(void **)(vt + 0x54))(p + *(short *)(vt + 0x50));
    if (((Pickup *)n->data)->heldState != 0)
        *(int *)((char *)Interactives::getInteractive(((Pickup *)n->data)->heldState) + 0x68A4) = 0;
    ((Pickup *)n->data)->heldState = 0;
    if (n->prev != 0)
        n->prev->next = n->next;
    if (n->next != 0)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = 0;
    if (((int)((Pickup *)n->data)->bits & 1) && regen)
        list = &regenList;
    else
        list = &deadList;
    n->prev = list;
    n->next = list->next;
    list->next = n;
    if (n->next != 0)
        n->next->prev = n;
    for (k = 0; k < 16; k++) {
        if (highlightList[k] == n)
            highlightList[k] = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", killPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori);
#endif
bool LevelPickups::inFlight(PickupIter it)
{
    ListNode *n = inFlightList.next;

    while (n != &inFlightList) {
        if (n == (ListNode *)it.cur)
            return true;
        n = n->next;
    }
    return false;
}
/* Node of the free pickup a monster can grab: among the drawn pickups whose handle sphere (handleRange) touches the attach sphere `a`
   (w = radius), not held and not in flight, the one closest to `b` (distance to the handle sphere when it has a radius). 0 if none. */
#ifdef NON_MATCHING
/* 42/137 words: eeSqrtf moves and register allocation */
void *LevelPickups::getClosestPickup(_fvector &a, _fvector &b)
{
    _cs *list[10];
    _fvector d;
    Pickup *best = 0;
    float bestDist;
    int n = hdCsCollect(&a, a.w + 100.0f, 5000, 5099, list, 10);
    int i;

    bestDist = 3.402823466e+38f;
    for (i = 0; i < n; i++) {
        _cs *cs = list[i];
        unsigned h;
        Pickup *p;
        float hr;
        float dist;
        float r;

        if (!cs->drawMe)
            continue;
        h = *(unsigned *)cs->epNode;
        if ((h & 0xFFFC0000) == 0x4E6C0000)
            continue;
        p = (Pickup *)Interactives::getInteractive((h >> 7) & 0x7FF);
        if (p->heldState != 0)
            continue;
        hr = p->handleRange;
        vecSub(&d, &p->cs->trans, &a);
        dist = vecLenSq(&d);
        r = a.w + hr;
        if (r * r < dist)
            continue;
        if (1e-10f < hr) {
            vecSub(&d, &p->cs->trans, &b);
            dist = eeSqrtf(vecLenSq(&d)) - hr;
            dist = dist > 0.0f ? dist : 0.0f;
            dist = dist * dist;
        } else {
            vecSub(&d, &p->cs->trans, &b);
            dist = vecLenSq(&d);
        }
        if (dist < bestDist && !inFlight(p->ref)) {
            bestDist = dist;
            best = p;
        }
    }
    return best ? (void *)best->ref.cur : 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", getClosestPickup__12LevelPickupsR8_fvectorT1);
#endif
/* Node of the thrown pickup closest to `p` within `r`, or 0. */
#ifdef NON_MATCHING
/* 31/34 words: register allocation */
void *LevelPickups::getClosestThrownPickup(_fvector &p, float r)
{
    ListNode *n;
    ListNode *best = 0;
    _fvector d;
    float r2 = r * r;

    for (n = inFlightList.next; n != &inFlightList; n = n->next) {
        float dist;

        vecSub(&d, &((Pickup *)n->data)->cs->trans, &p);
        dist = vecLenSq(&d);
        if (dist < r2) {
            r2 = dist;
            best = n;
        }
    }
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", getClosestThrownPickup__12LevelPickupsR8_fvectorf);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Pickup", __static_initialization_and_destruction_0_00183128);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", func_001831D0);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", func_001831E0);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", __tf6Pickup);
PickupIter Pickup::getRef(void)
{
    return ref;
}
float Pickup::getHealth(void)
{
    return health;
}
float Pickup::getSpeed(void)
{
    return speed;
}
int Pickup::getDbId(void)
{
    return *(unsigned *)*(char **)cs >> 18;
}
int Pickup::getPickupType(void)
{
    return pickupType;
}
int Pickup::getWeapIdx(void)
{
    return weapIdx;
}
int Pickup::getGrabber(void)
{
    return heldState;
}
int Pickup::isRegenable(void)
{
    return (int)bits & 1;
}
int Pickup::isTwoHanded(void)
{
    return (int)(bits >> 1) & 1;
}
int Pickup::isRigidBody(void)
{
    return (int)(bits >> 2) & 1;
}
int Pickup::isADragonHead(void)
{
    return (int)(bits >> 5) & 1;
}
float Pickup::getSwipeRange(void)
{
    return swipeFar - swipeNear;
}
int Pickup::getNumCollisSpheres(void)
{
    return *(int *)((char *)this + 0x160);
}
void Pickup::setRef(PickupIter it)
{
    ref = it;
    *(Pickup **)it.cur = this;
}
void Pickup::setGrabber(int i)
{
    heldState = i;
}
void Pickup::setWeapIdx(int i)
{
    weapIdx = i;
}
void Pickup::updateGenerator(void *p)
{
    updateGenerator();
}
void Pickup::resetHealth(void)
{
    health = maxHealth;
}
float Pickup::getHandleRange(void)
{
    return handleRange;
}
INCLUDE_ASM("asm/nonmatchings/game/Pickup", _vt$6Pickup);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", func_00183368);
INCLUDE_ASM("asm/nonmatchings/game/Pickup", _GLOBAL_$I$__6Pickup);
