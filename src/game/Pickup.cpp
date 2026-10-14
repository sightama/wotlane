#include "common.h"
#include "game/pickup.h"
#include "game/game.h"
#include "vecmath.h"
#include "memory_stack.h"
#include "game/military_pickup.h"
#include "game/military_formation.h"
#include "game/hud.h"
#include "game/pickup_fx.h"
#include "hd.h"
#include "game/hit_event.h"
#include "game/hit_history.h"

__asm__("#SNFIX_SMALL game");

void hdReparentCsGrid(_cs *cs, _fvector *pos, float f);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
void mathfRotationFromVector(_fvector *rot, _fvector *v);
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
    static void dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId);
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
/* FLT_MAX, the retail literal in Pickup's sdata */
extern float noPickupDist __asm__("D_006F7DD8");
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
    void throwToTarget(_fvector &target, DbInteractive *by);
    void throwInDir(_fvector &dir, DbInteractive *by);
};
class StatePickUp {
public:
    void getAttachRegion(_fvector &a, _fvector &b);
};
class CsPool {
public:
    static _cs *csActivate(void);
    static void csDeactivate(_cs *cs);
};
void hierSetCsEpNode(_cs *cs, _hierhead *ep);
void mathfMulMatrix(float (*d)[4], float (*a)[4], float (*b)[4]);
void particleSweepTrail(_fvector *a, _fvector *b, unsigned id);
int electricArc(_fvector *from, _fvector *to, unsigned c0, unsigned c1, float jitter, float size, float spread, float scale, int segs,
                unsigned seed, unsigned dur);
extern "C" int rand(void);
int hdSphereTest(_fvector *a, _fvector *b, float r, _hdResult *res, unsigned skipId2, int max);
_hdResult *hdGetNextCollis(_hdResult *r, int n, _hdResult *prev);
void mathfUnitMatrix(float (*m)[4]);
void mathfTransposeMatrix(float (*dst)[4], float (*src)[4]);
void mathfRPHFromMatrix(float (*m)[4], _fvector *rph);
extern "C" int printf(const char *fmt, ...);
int hdSphereTest(_fvector *p, float r, _hdResult *res, unsigned skipId2, int max);
class MonsterSound {
public:
    void playTakeBatHitSound(void);
};
class StateCounter {
public:
    int counterSuccess(Monster &by, _fvector &at);
};
/* constructors of the pickups createPickup builds (they return `this`) */
void *pickupCtor(void *p) __asm__("__6Pickup");
void *missileTruckPickupCtor(void *p) __asm__("__18MissileTruckPickup");
void *tankPickupCtor(void *p) __asm__("__10TankPickup");
void *heliPickupCtor(void *p, bool military) __asm__("__10HeliPickupb");
void *turretPickupCtor(void *p) __asm__("__12TurretPickup");
void *ufoPickupCtor(void *p) __asm__("__9UfoPickup");
void *carPickupCtor(void *p) __asm__("__9CarPickup");
void *subwayPickupCtor(void *p) __asm__("__12SubwayPickup");
void *rigidBodyCtor(void *p) __asm__("__9RigidBody");
void *hitHistoryCtor(void *p) __asm__("__10HitHistory");
extern char rigidPickupVtbl[] __asm__("_vt$11RigidPickup");
/* LevelPickups::tankerShock, the ExpandingRing<ShockWaveConfig> the tanker's explosion hands to ShockWave::Activate (by value, so by
   address). Only what init writes is laid out: the ring count, six rings (stride 0x1C from 0x20, four floats at +4) and seven more floats. */
struct ShockRing {
    char pad0[4];
    float f[4];
    char pad14[8];
};
typedef char _size_ShockRing[sizeof(ShockRing) == 0x1C ? 1 : -1];
struct TankerShock {
    int numRings;
    char pad4[0x1C];
    ShockRing ring[6];
    char padC8[0x250 - 0xC8];
    float f250[6];
    char pad268[0x2A0 - 0x268];
    float f2A0;
};
extern TankerShock tankerShock __asm__("_12LevelPickups$tankerShock");
void *shockWaveActivate(_fvector *pos, TankerShock *cfg, int cause, int id, void *anim)
    __asm__("Activate__9ShockWaveP8_fvectorGt13ExpandingRing1Z15ShockWaveConfigQ28HitEvent11DamageCauseiP10SpecFxAnim");
/* the two cs that draw the highlight around the pickup a monster can grab */
extern _cs *highlightCs[2] __asm__("_12LevelPickups$s_highlight");


INCLUDE_ASM("asm/nonmatchings/game/Pickup", __6Pickup);
/* Resets the pickup, takes a cs placed at `pos` with rotation m (transposed), and reads the pickup group `ep`: its radius, its visual
   switch reset to child 0, and the locators of its second child (or of that child's first child group); the extra children are dropped.
   bit 0 = regen, 4 = !ep id2 bit 1, 5 = ep id2 bit 2. 0 when out of cs or `ep` is not a group. */
#ifdef NON_MATCHING
/* equivalent: 99/166 words; bitfield update order and scheduling */
int Pickup::init(_hierhead *ep, _fvector &pos, float (*m)[4], bool regen, ePickupType type)
{
    QMat t;
    HierGroup *g = (HierGroup *)ep;
    HierHead *k;

    weapIdx = -1;
    regenTimer = 0xA8C;
    *(float *)((char *)this + 0x11C) = 10.0f;
    heldState = 0;
    *(short *)((char *)this + 0xDC) = 0;
    *(int *)((char *)this + 0xE4) = 0;
    *(int *)((char *)this + 0xE0) = 0;
    *(int *)((char *)this + 0xD4) = 0;
    *(int *)&speed = 0;
    bits = (bits & ~1ULL) | (regen & 1);
    bits &= ~4ULL;
    bits &= ~2ULL;
    bits &= ~8ULL;
    *(short *)((char *)this + 0xDE) = (int)((float *)&LevelPickups::s_info[type])[20];
    bits = (bits & ~0x10ULL) | ((unsigned long long)(~(*(unsigned *)ep >> 8) & 1) << 4);
    bits = (bits & ~0x20ULL) | ((unsigned long long)((*(unsigned *)ep >> 9) & 1) << 5);
    bits &= ~0x40ULL;
    mathfUnitMatrix((float (*)[4])handleMat);
    pickupType = type;
    *(int *)&handleRange = 0;
    *(int *)((char *)this + 0x160) = 0;
    ((PickupSound *)((char *)this + 0x100))->initPickupSound(this);
    cs = CsPool::csActivate();
    if (cs == 0) {
        printf("Pickup::Init - No more CSs available.\n");
        return 0;
    }
    mathfTransposeMatrix(t.m, m);
    vecCopy((_fvector *)((char *)this + 0x20), &pos);
    mathfRPHFromMatrix(t.m, (_fvector *)((char *)this + 0x30));
    vecCopy((_fvector *)&cs->trans, &pos);
    vecCopy((_fvector *)&cs->rot, (_fvector *)((char *)this + 0x30));
    matCopy(cs->mat, t.m);
    if (g->head.opcode != 1) {
        printf("Pickup::Init - Found a PickupId on something other than a GroupNode (%d)\n", g->head.opcode);
        return 0;
    }
    *(float *)((char *)this + 0x11C) = g->sphereRadius;
    hierSetCsEpNode(cs, ep);
    k = g->child[0];
    if (k->opcode == 6)
        ((_hierswitch *)k)->whichChild = 0;
    if (g->numKids >= 2) {
        HierGroup *k1 = (HierGroup *)g->child[1];

        if (k1->head.opcode == 1) {
            if (k1->child[0]->opcode == 1)
                initLocators(k1->child[0]);
            else
                initLocators((_hierhead *)k1);
        }
        g->numKids = 1;
    }
    cs->drawMe = 1;
    cs->testCollision = 0;
    initConfig();
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", init__6PickupP9_hierheadR8_fvectorPA3_fb11ePickupType);
#endif
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
/* False when an active AI monster stands in the throw lane: nearer than `target` along the pickup's forward axis (cs row 1) and
   within 75 units of that axis. The positions come from each interactive's virtual getPos (vtable 0x10). */
#ifdef NON_MATCHING
/* 0/105 words: register allocation and dot-product order */
bool Pickup::isTargetClear(DbInteractive *target)
{
    char *vt = *(char **)((char *)target + 0x10);
    _fvector *pos = ((_fvector *(*)(void *))*(void **)(vt + 0x14))((char *)target + *(short *)(vt + 0x10));
    float *fwd = cs->mat[1];
    _fvector d;
    float range;
    int i;

    vecSub(&d, pos, &cs->trans);
    range = d.x * fwd[0] + d.y * fwd[1] + d.z * fwd[2];
    for (i = 0; i < game->m_numAIs; i++) {
        Monster *m = game->m_monsters[4 + i];
        float along;

        if (*(int *)((char *)m + 0x18) == 0)
            continue;
        vt = *(char **)((char *)m + 0x10);
        pos = ((_fvector *(*)(void *))*(void **)(vt + 0x14))((char *)m + *(short *)(vt + 0x10));
        vecSub(&d, pos, &cs->trans);
        along = d.w = d.x * fwd[0] + d.y * fwd[1] + d.z * fwd[2];
        if (along < range && d.x * d.x + d.y * d.y + d.z * d.z - along * along < 75.0f * 75.0f)
            return false;
    }
    return true;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", isTargetClear__6PickupP13DbInteractive);
#endif
/* Grabbed by interactive i: held visual state, the type's held health (if any), out of the collision grid. In game mode 1 on
   Central, hud 0 shows text box 27 when an interactive whose word 0x18 is 1 grabs a type-14 pickup; dodgeball restarts its countdown. */
#ifdef NON_MATCHING
/* 33/70 words: register allocation of the game pointer */
void Pickup::grab(int i)
{
    DbInteractive *who = Interactives::getInteractive(i);
    float h;

    heldState = i;
    setVisualState(2);
    h = LevelPickups::s_info[pickupType].heldHealth;
    if (1e-10f < h)
        health = h;
    flags &= ~2;
    hdRemoveCsFromGrid(cs);
    if (game->m_gameMode == 1 && game->m_levelId == 1 && *(int *)((char *)who + 0x18) == 1 && pickupType == 14)
        gameHud(0)->addTextBoxMessage(27);
    if (game->m_gameMode == 7)
        ((DodgeBallLevel *)dodgeBallInst)->ResetCountdown();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", grab__6Pickupi);
#endif
void Pickup::drop(void)
{
    hatCheck();
    heldState = 0;
    setVisualState(0);
    hdReparentCsGrid(cs, (_fvector *)((char *)cs + 0x10), 0.0f);
}
/* Explosion of a destroyed pickup by type, then the cs goes back to the pool (unless shared) and the engine sound stops. Type 3 (tanker)
   fires the tanker shock wave and, with bit 4, a fire, both credited to the db id of 0xE4's node; 4 only plays sound 0x81. */
#ifdef NON_MATCHING
/* equivalent: 18/161 words; retail reads game via lui in some cases and gp in others (file is SNFIX_SMALL) */
void Pickup::kill(void)
{
    flags &= 0xFFFD;
    switch (pickupType) {
    case 4:
        PICKUP_SOUNDS()->playDestructibleSound(0x81, &cs->trans);
        break;
    case 1:
    default:
        particleCreateFx(&cs->trans, 0x20, 5.0f, 0, 0.0f);
        PICKUP_SOUNDS()->playDestructibleSound(0x65, &cs->trans);
        break;
    case 3: {
        char *by = *(char **)((char *)this + 0xE4);
        char *sw = (char *)shockWaveActivate(&cs->trans, &tankerShock, 14, by ? (int)(*(_cs **)(by + 0xC))->epNode->id2 : 0, 0);

        if (sw != 0)
            *(int *)(sw + 0x24) = 1;
        if (bits & 0x10) {
            by = *(char **)((char *)this + 0xE4);
            gameWeapons()->CreateFire(&cs->trans, 100.0f, by ? (int)(*(_cs **)(by + 0xC))->epNode->id2 : 0);
        }
        particleCreateFx(&cs->trans, 9, 20.0f, 0, 0.0f);
        particleCreateFx(&cs->trans, 0x1F, 10.0f, 0, 0.0f);
        PICKUP_SOUNDS()->playDestructibleSound(0x65, &cs->trans);
        break;
    }
    case 2:
    case 11:
    case 12:
    case 15:
    case 23:
    case 24:
    case 26:
        particleCreateFx(&cs->trans, 9, 5.0f, 0, 0.0f);
        PICKUP_SOUNDS()->playDestructibleSound(0x67, &cs->trans);
        break;
    case 14:
    case 27:
        particleCreateFx(&cs->trans, 0x20, 5.0f, 0, 0.0f);
        PICKUP_SOUNDS()->playDestructibleSound(0x65, &cs->trans);
        break;
    }
    if ((bits & 1) == 0) {
        CsPool::csDeactivate(cs);
        cs = 0;
    }
    ((PickupSound *)((char *)this + 0x100))->terminatePickupSound();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", kill__6Pickup);
#endif
void Pickup::initConfig(void)
{
    health = maxHealth = LevelPickups::s_info[pickupType].health;
}
/* Places the cs from the grabber's hand: rotation = hand * handle rotation (two rows negated when flipped), position = hand + rotated handle. */
void Pickup::updateCS(_fvector *pos, float (&m)[4][4], bool flip)
{
    _fvector t;

    mathfMulMatrix(cs->mat, handleMat, m);
    if (flip) {
        vecScale((_fvector *)cs->mat[0], (_fvector *)cs->mat[0], -1.0f);
        vecScale((_fvector *)cs->mat[1], (_fvector *)cs->mat[1], -1.0f);
    }
    vecApplyRows(&t, cs->mat, (_fvector *)handle);
    vecAdd((_fvector *)&cs->trans, pos, &t);
}
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
/* Drops the pickup onto the ground below it (from its holder's position when held): sweeps a point from 25 above down to 25 below
   (bit 6, cleared here) or 4096 below, skipping hat id1 0x2744 and live interactives. On a hit the cs sits at the hit height + 0xD4
   with its up axis on the hit normal; surface 6 while 0x1203D0 is 10, or no hit at all, leaves it at the bottom of the sweep with
   health 0. */
#ifdef NON_MATCHING
/* equivalent: 0/166 words; loop layout (retail keeps the masks in s-registers and tests the result as one doubleword) */
void Pickup::hatCheck(void)
{
    _fvector a;
    _fvector b;
    _hdResult res[16];
    _hdResult *r;
    int n;

    if (heldState != 0) {
        _cs *c = cs;
        char *g = (char *)Interactives::getInteractive(heldState);
        char *vt = *(char **)(g + 0x10);

        vecCopy((_fvector *)&c->trans, (*(_fvector *(**)(void *))(vt + 0x14))(g + *(short *)(vt + 0x10)));
    }
    vecCopy(&a, (_fvector *)&cs->trans);
    b = a;
    a.z += 25.0f;
    if (bits & 0x40)
        b.z -= 25.0f;
    else
        b.z -= 4096.0f;
    n = hdSphereTest(&a, &b, 0.0f, res, cs->epNode->id2, 0x10);
    bits &= ~0x40;
    for (r = hdGetNextCollis(res, n, 0); r != 0; r = hdGetNextCollis(res, n, r)) {
        if (((r->ids >> 11) & 0x3FFF) == 0x2744)
            continue;
        if ((r->ids & 0x7FF) != 0 && *(int *)((char *)Interactives::getInteractive(r->ids & 0x7FF) + 0xC) != 0)
            continue;
        if (((unsigned char *)r)[3] == 6 && *(int *)((char *)game + 0x1203D0) == 10)
            break;
        {
            _cs *c = cs;
            float *x = (float *)c->mat[0];
            float k;

            c->trans.z = r->point.z + *(float *)((char *)this + 0xD4);
            vecCopy((_fvector *)c->mat[2], (_fvector *)&r->normal);
            ((int *)c->mat[2])[3] = 0;
            vecCross((_fvector *)c->mat[0], (_fvector *)c->mat[1], (_fvector *)c->mat[2]);
            k = eeRsqrtf(1.0f, vecDot((_fvector *)x, (_fvector *)x));
            x[0] *= k;
            x[2] *= k;
            x[1] *= k;
            vecCross((_fvector *)c->mat[1], (_fvector *)c->mat[2], (_fvector *)c->mat[0]);
            return;
        }
    }
    cs->trans.z = b.z;
    health = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", hatCheck__6Pickup);
#endif
/* Scans the pickup's group for translate/rotate nodes whose child is a locator (opcode 0x28) and reads them by locator id: 0x7E4 grab
   handle, 0x7E5 swipe far point (collision spheres), 0x7E6 the negated z goes to 0xD4. */
#ifdef NON_MATCHING
/* equivalent: 93/99 words; load order of the locator translation */
void Pickup::initLocators(_hierhead *ep)
{
    HierGroup *g = (HierGroup *)ep;
    int i;

    for (i = 0; i < g->numKids; i++) {
        HierHead *k = g->child[i];

        switch (k->opcode) {
        case 3: {
            _hiertranslate *t = (_hiertranslate *)k;
            unsigned c = *(unsigned *)t->child[0];

            if ((c & 0x3F) == 0x28) {
                c &= 0xFFFC0000;
                if (c == 0x1F900000) {
                    initHandle(t);
                } else if (c == 0x1F940000) {
                    _fvector v;

                    float z = t->trans.z;
                    float x = t->trans.x;

                    v.x = x;
                    v.y = t->trans.y;
                    v.z = z;
                    initCollisSpheres(&v);
                } else if (c == 0x1F980000) {
                    *(float *)((char *)this + 0xD4) = -t->trans.z;
                }
            }
            break;
        }
        case 4: {
            _hierrotate *r = (_hierrotate *)k;
            unsigned c = *(unsigned *)r->child[0];

            if ((c & 0x3F) == 0x28) {
                c &= 0xFFFC0000;
                if (c == 0x1F900000)
                    initHandle(r);
                else if (c == 0x1F940000)
                    initCollisSpheres((_fvector *)r->mat[3]);
                else if (c == 0x1F980000)
                    *(float *)((char *)this + 0xD4) = -r->mat[3][2];
            }
            break;
        }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initLocators__6PickupP9_hierhead);
#endif
#ifdef NON_MATCHING
/* 26/29 words: instruction order */
/* The grab handle is the negated translation of the node; its range is the db id of the node's child. */
void Pickup::initHandle(_hiertranslate *t)
{
    handle[0] = -t->trans.x;
    handle[1] = -t->trans.y;
    handle[2] = -t->trans.z;
    handleRange = (unsigned)((*(unsigned *)t->child[0] >> 7) & 0x7FF);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initHandle__6PickupP14_hiertranslate);
#endif
#ifdef NON_MATCHING
/* 43/50 words: loop shape of the transpose */
/* Same from a rotate node: the handle rotation is the node's 3x3 transposed, the offset its negated translation row. */
void Pickup::initHandle(_hierrotate *r)
{
    int i;
    int j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            handleMat[i][j] = r->mat[j][i];
    }
    vecScale((_fvector *)handle, (_fvector *)r->mat[3], -1.0f);
    handleRange = (unsigned)((*(unsigned *)r->child[0] >> 7) & 0x7FF);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initHandle__6PickupP11_hierrotate);
#endif
/* The swipe segment runs from the far point p (0xB0) to the negated handle (0xC0). Type 7 gets one sphere of radius 10 at p (count left
   as is); type 6 gets five spheres stepping a quarter of the segment from the near end, radius = quarter length. */
#ifdef NON_MATCHING
/* equivalent: 2/70 words; this kept in a2 in retail (register allocation) */
void Pickup::initCollisSpheres(_fvector *p)
{
    _fvector d;
    float *s;
    int i;

    vecCopy((_fvector *)((char *)this + 0xB0), p);
    vecScale((_fvector *)((char *)this + 0xC0), (_fvector *)handle, -1.0f);
    vecSub(&d, p, (_fvector *)((char *)this + 0xC0));
    vecScale(&d, &d, 0.25f);
    if (pickupType == 7) {
        vecCopy((_fvector *)((char *)this + 0x110), p);
        *(float *)((char *)this + 0x11C) = 10.0f;
    } else if (pickupType == 6) {
        s = (float *)((char *)this + 0x110);
        for (i = 0; i < 5; i++, s += 4) {
            float f = i;
            float *dd = (float *)&d;
            float *n = &((float *)this)[0x30];
            float x = n[0] + dd[0] * f;
            float y = n[1] + dd[1] * f;
            float z = n[2] + dd[2] * f;
            register float r __asm__("$f1") = vecDot(&d, &d);

            __asm__(".word 0x46010044" : "+f"(r)); /* sqrt.s $f1, $f1 (R5900 form, see eeSqrtf) */
            s[0] = x;
            s[1] = y;
            s[2] = z;
            s[3] = r;
        }
        *(int *)((char *)this + 0x160) = 5;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", initCollisSpheres__6PickupP8_fvector);
#endif
/* Sweeps the particle trail between the world positions of the two ends of the swipe segment. */
void Pickup::swipeTrailEffect(void)
{
    _fvector a;
    _fvector b;

    vecApplyRows(&a, cs->mat, (_fvector *)((char *)this + 0xC0));
    vecAdd(&a, (_fvector *)&cs->trans, &a);
    vecApplyRows(&b, cs->mat, (_fvector *)((char *)this + 0xB0));
    vecAdd(&b, (_fvector *)&cs->trans, &b);
    particleSweepTrail(&a, &b, (unsigned)this);
}
/* Bat swing with this pickup: each collision sphere (placed by the cs) collects up to 16 hits in total. Every new target other than
   the attacker `byId` gets a hit event (type 2 with b/c when either is set, else 7 when d > 100, else 1) and is dispatched damage
   a * a factor: monsters (kind 1, who also hear the bat) and hat ids 0x27A6/0x271B/0x271C take s_info[6]'s 0x10, kind 3 its 0x14,
   anything else 100. A monster in its counter state (0x37) that counters the swing ends the test with 0. Returns 1 if anything was hit. */
#ifdef NON_MATCHING
/* equivalent (untuned): 46/242 words, size 0x394 vs 0x3c8 */
int Pickup::collisTestBatAttack(float a, unsigned byId, MonsterAnim anim, HitHistory &hist, float b, float c, float d)
{
    _hdResult res[16];
    _fvector p;
    _fvector dir;
    int n = 0;
    int i;
    int hit;
    float *sphere = (float *)((char *)this + 0x110);
    float *info = (float *)&LevelPickups::s_info[6];

    for (i = 0; i < *(int *)((char *)this + 0x160); i++, sphere += 4) {
        vecApplyRows(&p, cs->mat, (_fvector *)sphere);
        vecAdd(&p, &p, (_fvector *)&cs->trans);
        n += hdSphereTest(&p, sphere[3], &res[n], cs->epNode->id2, 16 - n);
    }
    hit = 0;
    for (i = 0; i < n; i++) {
        _hdResult *r = &res[i];
        unsigned id = r->ids & 0x7FF;
        HitEvent *e;
        int *mon;
        float dmg;

        if (id == byId || hist.newHit(id, false) == 0)
            continue;
        e = HitEvent::create();
        hit = 1;
        *(_hdResult **)((char *)e + 0x4) = r;
        e->source = 4;
        e->pos = (_fvector *)&r->point;
        *(unsigned *)((char *)e + 0x24) = byId;
        if (b != 0.0f || c != 0.0f) {
            e->type = 2;
            *(float *)((char *)e + 0x38) = c;
            *(float *)((char *)e + 0x34) = b;
        } else {
            e->type = d > 100.0f ? 7 : hit;
        }
        mon = (int *)Interactives::getInteractive(r->ids & 0x7FF);
        if (*mon == 1)
            ((MonsterSound *)((char *)mon + 0x1A7C))->playTakeBatHitSound();
        if (*mon == 1) {
            dmg = a * info[4];
            if (byId != 0) {
                Monster *att = (Monster *)Interactives::getInteractive(byId);

                if (**(int **)((char *)mon + 0x34) == 0x37 &&
                    ((StateCounter *)((char *)mon + 0x7DD8))->counterSuccess(*att, *(_fvector *)&r->point) != 0)
                    return 0;
                att->registerComboHit((Monster *)mon);
            }
        } else if (*mon == 3) {
            dmg = a * info[5];
        } else {
            unsigned id1 = (r->ids >> 11) & 0x3FFF;

            if (id1 == 0x27A6 || id1 == 0x271B || id1 == 0x271C)
                dmg = a * info[4];
            else
                dmg = a * 100.0f;
        }
        if (byId != 0) {
            Interactives::dispatchHit((_fvector *)(*(_cs **)((char *)Interactives::getInteractive(byId) + 0xC))->mat[1], dmg, byId,
                                      r->ids & 0x7FF);
        } else {
            vecScale(&dir, (_fvector *)&r->normal, -1.0f);
            Interactives::dispatchHit(&dir, dmg, byId, r->ids & 0x7FF);
        }
        particleCreateFx((_fvector *)&r->point, 0x20, 5.0f, 0, 0.0f);
        ((PickupSound *)((char *)this + 0x100))->playBatImpactSound();
    }
    return hit;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", collisTestBatAttack__6PickupfUi11MonsterAnimR10HitHistoryfff);
#endif
_fvector *Pickup::getCollisSphereTrans(int i)
{
    return (_fvector *)((char *)this + 0x110) + i;
}
float Pickup::getCollisSphereRadius(int i)
{
    return *(float *)((char *)this + (i << 4) + 0x11C);
}
/* Generator pickup (0xDC frame counter, 0xE0 victim monster, 0xE4 source). For 40 frames it arcs electricity from the victim (or 0xF0
   when none) upward with sparks; on frame 41 it knocks the victim away from the source and, if the source is a kind-1 object, stores
   it in the victim's knock-back state. Returns 1 while still arcing. */
#ifdef NON_MATCHING
/* equivalent: 10/131 words; register allocation and the 0x7EA0 + 0x524 offset folded */
int Pickup::updateGenerator(void)
{
    short *count = (short *)((char *)this + 0xDC);
    char *victim;
    _fvector a;
    _fvector b;

    if (++*count > 40) {
        int *src;

        victim = *(char **)((char *)this + 0xE0);
        if (victim == 0)
            return 0;
        src = *(int **)((char *)this + 0xE4);
        vecSub(&a, (_fvector *)(*(char **)((char *)src + 0xC) + 0x10), (_fvector *)(*(char **)(victim + 0xC) + 0x10));
        {
            float k = eeRsqrtf(1.0f, vecDot(&a, &a));

            a.z *= k;
            a.x *= k;
            a.y *= k;
        }
        ((Monster *)victim)->knockBack(a, 550.0f, 250.0f);
        src = *(int **)((char *)this + 0xE4);
        if (src != 0 && *src == 1)
            *(int **)(*(char **)((char *)this + 0xE0) + 0x7EA0 + 0x524) = src;
        return 0;
    }
    victim = *(char **)((char *)this + 0xE0);
    vecCopy(&a, victim != 0 ? (_fvector *)(victim + 0x3E30) : (_fvector *)((char *)this + 0xF0));
    b = a;
    b.z += mathfRandf(200.0f, 300.0f);
    electricArc(&a, &b, 0x40FFC864, 0x02FFFF80, 0.0f, 15.0f, 0.75f, 0.8f, *count > 29 ? 1 : 3, rand(), 2);
    if (mathfRand(0, 10) == 0) {
        particleCreateFx(&a, 0x20, 7.5f, 0, 0.0f);
        particleCreateFx(&a, 0x18, 15.0f, 0, 0.0f);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", updateGenerator__6Pickup);
#endif
/* Resets the highlight pulse, the 16 highlighted pickups, the five lists and the highlight cs, then the per-type tuning of all 28 types
   (word 1, heldHealth, is left alone) and the tanker explosion's shock wave. */
#ifdef NON_MATCHING
/* equivalent: 19/178 words; store scheduling of the table init (retail compares the loop pointer signed) */
void LevelPickups::init(void)
{
    LevelPickups::Info *p;
    int i;

    highlightColorInc = 5;
    highlightColor = 0x80;
    highlightGreen = 0x80;
    for (i = 15; i >= 0; i--)
        highlightList[i] = 0;
    activeList.next = &activeList;
    inUseList.next = &inUseList;
    inFlightList.next = &inFlightList;
    regenList.next = &regenList;
    deadList.next = &deadList;
    highlightCs[0] = 0;
    highlightCs[1] = 0;
    activeList.prev = &activeList;
    inUseList.prev = &inUseList;
    inFlightList.prev = &inFlightList;
    regenList.prev = &regenList;
    deadList.prev = &deadList;
    for (p = s_info; p < s_info + 28; p++) {
        float *f = (float *)p;

        f[0] = 10.0f;
        f[2] = 8.0f;
        f[3] = 10.0f;
        f[4] = 8.0f;
        f[5] = 8.0f;
        f[6] = 2.0f;
        f[7] = 100.0f;
        f[8] = 200.0f;
        f[9] = 200.0f;
        f[10] = 8.0f;
        f[11] = 10.0f;
        f[12] = 2.0f;
        f[13] = 800.0f;
        f[14] = -640.0f;
        f[15] = 1000.0f;
        ((int *)f)[16] = 0;
        ((int *)f)[17] = 0;
        f[20] = 4.0f;
        f[18] = 0.7853982f;
        f[19] = 0.7853982f;
    }
    ((float *)&s_info[7])[14] = -160.0f;
    tankerShock.numRings = 6;
    tankerShock.ring[0].f[3] = 440.0f;
    tankerShock.ring[1].f[3] = 490.0f;
    tankerShock.ring[2].f[3] = 420.0f;
    tankerShock.ring[3].f[3] = 350.0f;
    tankerShock.ring[4].f[3] = 90.0f;
    tankerShock.f250[0] = 20.0f;
    tankerShock.ring[0].f[0] = 10.0f;
    tankerShock.ring[0].f[2] = 540.0f;
    tankerShock.f250[1] = 70.0f;
    tankerShock.ring[1].f[0] = 9.0f;
    tankerShock.f250[2] = 100.0f;
    tankerShock.ring[2].f[0] = 8.0f;
    tankerShock.ring[2].f[2] = 360.0f;
    tankerShock.f250[3] = 140.0f;
    tankerShock.f250[4] = 170.0f;
    tankerShock.ring[4].f[0] = 7.0f;
    tankerShock.f250[5] = 200.0f;
    tankerShock.ring[5].f[0] = 6.0f;
    tankerShock.ring[5].f[1] = 44.0f;
    *(int *)&tankerShock.ring[5].f[3] = 0;
    tankerShock.f2A0 = 10.0f;
    tankerShock.ring[0].f[1] = 44.0f;
    tankerShock.ring[1].f[1] = 44.0f;
    tankerShock.ring[1].f[2] = 420.0f;
    tankerShock.ring[2].f[1] = 44.0f;
    tankerShock.ring[3].f[0] = 7.0f;
    tankerShock.ring[3].f[1] = 44.0f;
    tankerShock.ring[3].f[2] = 350.0f;
    tankerShock.ring[4].f[1] = 44.0f;
    tankerShock.ring[4].f[2] = 90.0f;
    *(int *)&tankerShock.ring[5].f[2] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", init__12LevelPickups);
#endif
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
/* Builds the pickup for hat node `ep` on the global memory stack. The type comes from the node's id1 (minus 5000) unless given, in
   which case it is written back into id1. In game mode 1 the military ones (missile truck, tank, helicopters, turret) and the UFO are
   built; outside it only a UFO whose id2 bit 0 is set. Then the other ids: helicopter (not military), rigid pickups, cars (id2 bit 0,
   else plain), subway, plain for any other id; ids already handled above, and 0x138D, give nothing. After the virtual init succeeds,
   the pickup gets a node at the head of the active list (and the virtual at 0x30 when 0x120468 is set; the dodge ball level keeps the
   node of id1 0x139D). Returns the node, or 0. */
#ifdef NON_MATCHING
/* equivalent (untuned): 4/308 words; switch ranges and game read via gp */
void *LevelPickups::createPickup(_hierhead *ep, _fvector &pos, float (*m)[4], bool regen, ePickupType type)
{
    unsigned *w = (unsigned *)ep;
    char *p = 0;
    ListNode *n;

    if (type == 0)
        type = (ePickupType)((*w >> 18) - 0x1388);
    else
        *w = (*w & 0x3FFFF) | ((type + 0x1388) << 18);
    if (*(int *)((char *)game + 0x1203C8) == 1) {
        switch (*w >> 18) {
        case 0x1391:
            p = (char *)missileTruckPickupCtor(operator new(0x2A0, MemoryStack::global.alloc16(0x2A0)));
            break;
        case 0x1392:
            p = (char *)tankPickupCtor(operator new(0x2B0, MemoryStack::global.alloc16(0x2B0)));
            break;
        case 0x1394:
            p = (char *)heliPickupCtor(operator new(0x300, MemoryStack::global.alloc16(0x300)), true);
            break;
        case 0x139F:
            p = (char *)heliPickupCtor(operator new(0x300, MemoryStack::global.alloc16(0x300)), true);
            break;
        case 0x13A0:
            p = (char *)turretPickupCtor(operator new(0x1D0, MemoryStack::global.alloc16(0x1D0)));
            break;
        case 0x13A1:
            p = (char *)ufoPickupCtor(operator new(0x230, MemoryStack::global.alloc16(0x230)));
            break;
        }
    } else if ((*w & 0xFFFFFF80) == 0x4E840080) {
        p = (char *)ufoPickupCtor(operator new(0x230, MemoryStack::global.alloc16(0x230)));
    }
    switch (*w >> 18) {
    case 0x1393:
        p = (char *)heliPickupCtor(operator new(0x300, MemoryStack::global.alloc16(0x300)), false);
        break;
    case 0x1395:
    case 0x1399:
    case 0x139A:
    case 0x139B:
    case 0x139C:
    case 0x139D:
    case 0x139E: {
        char *r = (char *)operator new(0x4C0, MemoryStack::global.alloc16(0x4C0));

        p = r;
        pickupCtor(r);
        *(void **)(r + 0x10) = rigidPickupVtbl;
        rigidBodyCtor(r + 0x190);
        hitHistoryCtor(r + 0x458);
        break;
    }
    case 0x138A:
    case 0x138B:
    case 0x1397:
    case 0x13A2:
        if ((*w >> 7) & 1) {
            p = (char *)carPickupCtor(operator new(0x270, MemoryStack::global.alloc16(0x270)));
            break;
        }
        /* fall through */
    default:
        p = (char *)pickupCtor(operator new(0x170, MemoryStack::global.alloc16(0x170)));
        break;
    case 0x13A3:
        p = (char *)subwayPickupCtor(operator new(0x250, MemoryStack::global.alloc16(0x250)));
        break;
    case 0x138D:
    case 0x1391:
    case 0x1392:
    case 0x1394:
    case 0x139F:
    case 0x13A0:
    case 0x13A1:
        break;
    }
    if (p == 0)
        return 0;
    {
        char *vt = *(char **)(p + 0x10);

        if ((*(int (**)(void *, _hierhead *, _fvector &, float (*)[4], bool, ePickupType))(vt + 0x5C))(p + *(short *)(vt + 0x58), ep, pos, m,
                                                                                                      regen, type) == 0)
            return 0;
    }
    n = (ListNode *)operator new(0xC, MemoryStack::global.alloc16(0xC));
    *(ListNode **)(p + 0x14) = n;
    n->data = p;
    {
        ListNode *first = activeList.next;

        activeList.next = n;
        n->prev = &activeList;
        n->next = first;
        if (first != 0)
            first->prev = n;
    }
    if (*(int *)((char *)game + 0x120468) != 0) {
        char *d = (char *)n->data;
        char *vt = *(char **)(d + 0x10);

        (*(void (**)(void *))(vt + 0x34))(d + *(short *)(vt + 0x30));
    }
    if (*(int *)((char *)game + 0x1203C8) == 7 && (*w & 0xFFFC0000) == 0x4E740000)
        *(ListNode **)(dodgeBallInst + 0xC) = n;
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", createPickup__12LevelPickupsP9_hierheadR8_fvectorPA3_fb11ePickupType);
#endif
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
/* A held pickup is thrown at the point `at`: its node moves to the in-flight list, nobody holds it, flying visual state. A rigid body
   flies by physics; anything else becomes a static weapon aimed along at - position, which remembers the thrower (0xE4). */
#ifdef NON_MATCHING
/* 68/87 words: register allocation */
void LevelPickups::throwPickup(PickupIter it, _fvector &at, DbInteractive *by, DbInteractive *target)
{
    ListNode *n = (ListNode *)it.cur;
    Pickup *p = (Pickup *)n->data;
    _fvector rot;
    _fvector d;
    int byId;
    int targetId;

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
    if ((int)(p->bits >> 2) & 1) {
        ((RigidPickup *)p)->throwToTarget(at, by);
        return;
    }
    vecSub(&d, &at, &p->cs->trans);
    mathfRotationFromVector(&rot, &d);
    byId = by ? (*(unsigned *)(*(_cs **)((char *)by + 0xC))->epNode >> 7) & 0x7FF : 0;
    targetId = target ? (*(unsigned *)(*(_cs **)((char *)target + 0xC))->epNode >> 7) & 0x7FF : 0;
    p->weapIdx = gameWeapons()->CreateStaticWeapon(it, byId, targetId, &rot);
    *(DbInteractive **)((char *)p + 0xE4) = by;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", throwPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractiveT3);
#endif
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
/* A thrown pickup is batted away by `by`: towards `target` if there is one, else along `dir`. A rigid body is rethrown; anything
   else gets a new static weapon (the old one is killed). */
#ifdef NON_MATCHING
/* 57/90 words: register allocation */
void LevelPickups::deflectThrownPickup(PickupIter it, _fvector &dir, DbInteractive *by, DbInteractive *target)
{
    Pickup *p = (Pickup *)((ListNode *)it.cur)->data;
    _fvector rot;
    _fvector d;
    int byId;
    int targetId;

    if ((int)(p->bits >> 2) & 1) {
        if (target != 0)
            ((RigidPickup *)p)->throwToTarget((*(_cs **)((char *)target + 0xC))->trans, by);
        else
            ((RigidPickup *)p)->throwInDir(dir, by);
        return;
    }
    if (target != 0) {
        vecSub(&d, &(*(_cs **)((char *)target + 0xC))->trans, &p->cs->trans);
        mathfRotationFromVector(&rot, &d);
    } else {
        mathfRotationFromVector(&rot, &dir);
    }
    byId = by ? (*(unsigned *)(*(_cs **)((char *)by + 0xC))->epNode >> 7) & 0x7FF : 0;
    targetId = target ? (*(unsigned *)(*(_cs **)((char *)target + 0xC))->epNode >> 7) & 0x7FF : 0;
    if (p->weapIdx >= 0)
        ((Weapon *)((char *)gameWeapons() + p->weapIdx * 0x170 + 0xC00))->KillWeapon();
    p->weapIdx = gameWeapons()->CreateStaticWeapon(it, byId, targetId, &rot);
    *(DbInteractive **)((char *)p + 0xE4) = by;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Pickup", deflectThrownPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractiveT3);
#endif
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

    bestDist = noPickupDist;
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
int Pickup::updateGenerator(void *p)
{
    return updateGenerator();
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
