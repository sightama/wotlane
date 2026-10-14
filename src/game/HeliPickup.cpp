#include "common.h"
#include "engine.h"
#include "hd.h"
#include "vecmath.h"
#include "game/game.h"
#include "game/hit_event.h"
#include "game/pickup.h"
#include "game/pickup_fx.h"
#include "cs_pool.h"

extern "C" float fabsf(float);

/* Virtual pad the helicopter's AI fills (same object the monsters read). Only the bytes written here are named. */
class GamePad {
public:
    unsigned char b0[7];
    unsigned char fire;       /* 0x07: 0xFF fires the gun */
    unsigned char b8[4];
    unsigned char rollNeg;    /* 0x0C: crash spin w when negative */
    unsigned char rollPos;    /* 0x0D */
    unsigned char bE;
    unsigned char lift;       /* 0x0F: crash spin z */
    unsigned char xNeg;       /* 0x10: crash spin x when negative */
    unsigned char xPos;       /* 0x11 */
    unsigned char yPos;       /* 0x12: crash spin y when positive */
    unsigned char yNeg;       /* 0x13 */
    unsigned char b14[4];
    int f18;                  /* 0x18: update sets 1 before clearInputs, which leaves it alone */

    void clearInputs(void);
    void loadPadInputs(int i);
};
typedef char _size_GamePad[sizeof(GamePad) == 0x1C ? 1 : -1];

struct ActuatorData {
    unsigned char b[0x20];
};
void inputSetActuator(int pad, ActuatorData *a);
int timerGetFieldCount(void);
int hdSphereTest(_fvector *pos, float r, _hdResult *res, unsigned id, int flags);

class Interactives {
public:
    static int addInteractive(DbInteractive *p);
    static Monster *getInteractive(int i);
};

class HeliPickup;
class HeliSound {
public:
    unsigned m_crashHandle;
    unsigned m_rotorHandle;
    unsigned m_gunHandle;
    float m_prevDist;

    void initHeliSound(void);
    void updateHeliSound(HeliPickup *heli);
    void terminateHeliSound(void);
};

class HeliVehicle {
public:
    char pad0[0x30];
    _fvector vel;             /* 0x30 (its PointMass velocity) */
    char pad40[0x70 - 0x40];

    void setCs(_cs *c);
    void updateControls(GamePad &pad);
    void updatePosition(void);
    void updateCollision(void);
};
typedef char _size_HeliVehicle[sizeof(HeliVehicle) == 0x70 ? 1 : -1];

class HeliVehicleNav {
public:
    char pad0[0x14];
    int active;               /* 0x14: zero once the navigator is done with its goal */
    char pad18[0xD0 - 0x18];

    void init(void);
    void update(GamePad &pad);
    void wander(void);
    void disable(void);
    void seek(_fvector &p, float r);
    void flee(_fvector &p, float r, float f);
};
typedef char _size_HeliVehicleNav[sizeof(HeliVehicleNav) == 0xD0 ? 1 : -1];

extern int s_padControl __asm__("s_padControl_006F7B44");
extern float s_seekRange;
extern float s_fireRange;
extern float s_fleeRange;
extern float s_safeRange;
extern int BURST_DELAY_MIN;
extern int BURST_DELAY_MAX;
extern int BURST_COUNT_MIN;
extern int BURST_COUNT_MAX;
extern int FIRE_DELAY_MIN;
extern int FIRE_DELAY_MAX;
extern int HOVER_DELAY_MIN;
extern int HOVER_DELAY_MAX;
__asm__("#SNFIX_SMALL s_padControl_006F7B44");
__asm__("#SNFIX_SMALL s_seekRange");
__asm__("#SNFIX_SMALL s_fireRange");
__asm__("#SNFIX_SMALL s_fleeRange");
__asm__("#SNFIX_SMALL s_safeRange");
__asm__("#SNFIX_SMALL BURST_DELAY_MIN");
__asm__("#SNFIX_SMALL BURST_DELAY_MAX");
__asm__("#SNFIX_SMALL BURST_COUNT_MIN");
__asm__("#SNFIX_SMALL BURST_COUNT_MAX");
__asm__("#SNFIX_SMALL FIRE_DELAY_MIN");
__asm__("#SNFIX_SMALL FIRE_DELAY_MAX");
__asm__("#SNFIX_SMALL HOVER_DELAY_MIN");
__asm__("#SNFIX_SMALL HOVER_DELAY_MAX");
extern int militaryStillAlive __asm__("_12LevelPickups$s_militaryStillAlive");
extern HitEvent s_takeHitInfo __asm__("_8HitEvent$s_takeHitInfo");

/* virtual getPinTrans() of an interactive (vtable entry {delta, function} at vptr + 0x10) */
#define PIN_TRANS(t) \
    (((_fvector *(*)(void *))*(void **)(*(char **)((char *)(t) + 0x10) + 0x14))((char *)(t) + *(short *)(*(char **)((char *)(t) + 0x10) + 0x10)))

/* AI helicopter (military pickup flown by HeliVehicle + HeliVehicleNav). States: 0 wander, 1 seek the target, 2 hover and fire
   bursts, 3 flee, 4 crashing (random spin until it touches the ground), 5 held / dead. */
class HeliPickup : public Pickup {
public:
    enum State { STATE_0, STATE_1, STATE_2, STATE_3, STATE_4, STATE_5 };

    char padE0[0x170 - 0xE0];
    int canFire;              /* 0x170: armed; otherwise state 2 only hovers */
    int state;                /* 0x174 */
    Monster *target;          /* 0x178 */
    int nextBurstTime;        /* 0x17C: field count when a flee ends and the wander may seek again */
    int stateTime;            /* 0x180: end of the hover / burst in state 2 */
    int burstCount;           /* 0x184: shots left in the burst */
    int nextShotTime;         /* 0x188 */
    char pad18C[4];
    HeliVehicle hv;           /* 0x190 */
    HeliVehicleNav nav;       /* 0x200 */
    float spin[4];            /* 0x2D0: crash controls (x, y, lift, roll) */
    int fx;                   /* 0x2E0 */
    HeliSound sound;          /* 0x2E4 */

    bool update(void);
    void initAfterDbLoad(void);
    void takeHit(_fvector *pos, float dmg, int attacker);
    void updateAttackBehavior(GamePad &pad);
    _fvector *getVel(void);
    void drop(void);
    void grab(int i);
    void enterState(State s);
    void regen(void);
    void kill(void);
};
typedef char _at_HeliPickup_sound[(unsigned)&((HeliPickup *)0)->sound == 0x2E4 ? 1 : -1];

INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", __10HeliPickupb);
void HeliPickup::initAfterDbLoad(void)
{
    int id = Interactives::addInteractive((DbInteractive *)this);
    unsigned *head = (unsigned *)cs->epNode;

    *head = (*head & 0xFFFC007F) | ((id & 0x7FF) << 7);
    hdReparentCsGrid(cs);
    hv.setCs(cs);
    nav.init();
    cs->testCollision = 1;
    sound.initHeliSound();
    target = 0;
    burstCount = 0;
    nextBurstTime = 0;
    nextShotTime = 0;
    fx = -1;
    enterState(STATE_0);
    flags |= 2;
}
/* Small hits still count as 0.05. Only a hit under 5 that empties the health starts the crash (state 4); a bigger one leaves
   it at <= 0 so update() returns false and the pickup is killed outright. Rumbles the attacker's pad on a knockback-only hit. */
#ifdef NON_MATCHING
/* 41/74 palavras: o retail soma o offset 0x1227A0 de game num registrador (lui/ori) e le m_index antes da copia do ActuatorData */
void HeliPickup::takeHit(_fvector *pos, float dmg, int attacker)
{
    HitEvent *info = &s_takeHitInfo;

    if (dmg <= 0.1f)
        dmg = 0.05f;
    health -= dmg;
    if (health <= 0.0f && dmg < 5.0f && state != STATE_4) {
        enterState(STATE_4);
        if (attacker != 0 && info->type == 0 && info->source == 8) {
            Monster *m = Interactives::getInteractive(attacker);
            ActuatorData a = *(ActuatorData *)((char *)game + 0x1227A0);

            inputSetActuator(m->m_index, &a);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", takeHit__10HeliPickupP8_fvectorfi);
#endif
void HeliPickup::grab(int i)
{
    Pickup::grab(i);
    particleKillFx(fx);
    enterState(STATE_5);
}
void HeliPickup::drop(void)
{
    heldState = 0;
    Pickup::setVisualState(0);
    Pickup::hatCheck();
    hdReparentCsGrid(cs);
}
void HeliPickup::kill(void)
{
    cs->drawMe = 0;
    cs->testCollision = 0;
    particleCreateFx(&cs->trans, 0xB, 8.0f, 0, 0.0f);
    PICKUP_SOUNDS()->playDestructibleSound(0x65, &cs->trans);
    sound.terminateHeliSound();
    particleKillFx(fx);
    flags &= 0xFFFD;
    if ((bits & 1) == 0) {
        CsPool::csDeactivate(cs);
        cs = 0;
    }
}
/* Alive while it has health or is still crashing. */
#ifdef NON_MATCHING
/* 70/72 palavras: ordem de carga de vel.y/vel.z no produto escalar */
bool HeliPickup::update(void)
{
    GamePad pad;

    pad.f18 = 1;
    pad.clearInputs();
    if (state != STATE_5) {
        if (s_padControl) {
            pad.loadPadInputs(1);
        } else {
            updateAttackBehavior(pad);
            nav.update(pad);
        }
        hv.updateControls(pad);
        hv.updatePosition();
        hv.updateCollision();
        {
            float *v = (float *)&hv.vel;
            register float x __asm__("$f3") = v[0];
            register float z __asm__("$f2") = v[2];
            register float y __asm__("$f0") = v[1];
            register float s __asm__("$f1");

            __asm__ volatile("mula.s %1, %1
	madda.s %2, %2
	madd.s %0, %3, %3" : "=f"(s) : "f"(x), "f"(y), "f"(z));
            __asm__ volatile(".word 0x46010044" : "+f"(s));
            speed = s;
        }
        sound.updateHeliSound(this);
        if (health > 0.0f)
            militaryStillAlive = 1;
    }
    return health > 0.0f || state == STATE_4;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", update__10HeliPickup);
#endif
void HeliPickup::regen(void)
{
    Pickup::regen();
    cs->testCollision = 1;
    target = 0;
    burstCount = 0;
    nextBurstTime = 0;
    fx = -1;
    enterState(STATE_0);
}
#ifdef NON_MATCHING
/* 22/366 palavras: o retail mantem o pad em s2, um distSq por caso e o caso 5 na tabela de saltos (6 entradas); aqui o 5 cai no default */
void HeliPickup::updateAttackBehavior(GamePad &pad)
{
    _fvector d;
    _hdResult res;
    float distSq;
    Monster *m;

    switch (state) {
    case STATE_0:
        target = game->getClosestPlayer(cs->trans, s_seekRange, distSq);
        if (target != 0 && distSq < s_fleeRange * s_fleeRange) {
            enterState(STATE_3);
            return;
        }
        if (nextBurstTime < timerGetFieldCount() && target != 0)
            enterState(STATE_1);
        return;
    case STATE_1:
        m = game->getClosestPlayer(cs->trans, s_fleeRange, distSq);
        if (m != 0) {
            target = m;
            enterState(STATE_3);
            return;
        }
        if (!(target->m_flags & 2)) {
            enterState(STATE_0);
            return;
        }
        vecSub(&d, &cs->trans, &target->m_cs->trans);
        if (d.x * d.x + d.y * d.y < s_fireRange * s_fireRange) {
            vecSub(&d, PIN_TRANS(target), &cs->trans);
            if (fabsf(d.z) < 150.0f) {
                enterState(STATE_2);
                return;
            }
        }
        if (nav.active == 0)
            enterState(STATE_0);
        return;
    case STATE_2:
        m = game->getClosestPlayer(cs->trans, s_fleeRange, distSq);
        if (m != 0) {
            target = m;
            enterState(STATE_3);
            return;
        }
        if (!(target->m_flags & 2)) {
            enterState(STATE_0);
            return;
        }
        if (burstCount <= 0 || nav.active == 0 || stateTime < timerGetFieldCount()) {
            enterState(STATE_3);
            return;
        }
        vecSub(&d, &cs->trans, PIN_TRANS(target));
        if ((s_fireRange + 100.0f) * (s_fireRange + 100.0f) < d.x * d.x + d.y * d.y) {
            enterState(STATE_1);
            return;
        }
        if (canFire == 0 || !(nextShotTime < timerGetFieldCount()))
            return;
        vecSub(&d, PIN_TRANS(target), &cs->trans);
        if (fabsf(d.z) < 150.0f) {
            float k;

            d.z = 0.0f;
            {
                register float l __asm__("$f2") = vecDot(&d, &d);
                __asm__(".word 0x46020084" : "+f"(l));
                k = 1.0f / l;
            }
            d.z *= k;
            d.x *= k;
            d.y *= k;
            if (0.99f < vecDot(&d, (_fvector *)cs->mat[1]) && isTargetClear((DbInteractive *)target)) {
                pad.fire = 0xFF;
                nextShotTime = timerGetFieldCount() + mathfRand(FIRE_DELAY_MIN, FIRE_DELAY_MAX);
                burstCount--;
            }
        }
        return;
    case STATE_3:
        m = game->getClosestPlayer(cs->trans, s_safeRange, distSq);
        if (m == 0) {
            enterState(STATE_0);
            return;
        }
        if (target != m) {
            target = m;
            enterState(STATE_3);
        }
        return;
    case STATE_4:
        if (spin[0] < 0.0f)
            pad.xNeg = (int)(-spin[0] * 255.0f);
        else
            pad.xPos = (int)(spin[0] * 255.0f);
        if (spin[1] < 0.0f)
            pad.yNeg = (int)(-spin[1] * 255.0f);
        else
            pad.yPos = (int)(spin[1] * 255.0f);
        pad.lift = (int)(spin[2] * 255.0f);
        if (spin[3] < 0.0f)
            pad.rollNeg = (int)(-spin[3] * 255.0f);
        else
            pad.rollPos = (int)(spin[3] * 255.0f);
        /* crashes into anything but the hats 0x20..0x400 */
        if (hdSphereTest(&cs->trans, 20.0f, &res, (*(unsigned *)cs->epNode >> 7) & 0x7FF, 1)
            && (unsigned)(((res.ids >> 11) & 0x3FFF) - 0x20) > 0x3E0)
            enterState(STATE_5);
        return;
    case STATE_5:
    default:
        nav.disable();
        return;
    }
}
/* retail pads the first jump table of this TU to 8 words */
__asm__(".section .rodata
	.word 0
	.word 0
	.text");
#else
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", updateAttackBehavior__10HeliPickupR7GamePad);
#endif
#ifdef NON_MATCHING
/* 132/137 palavras: agendamento dos mov.s/swc1 no caso 4 */
void HeliPickup::enterState(State s)
{
    switch (s) {
    case STATE_0:
        target = 0;
        nav.wander();
        break;
    case STATE_1:
        nav.seek(*PIN_TRANS(target), s_fireRange);
        break;
    case STATE_2:
        burstCount = mathfRand(BURST_COUNT_MIN, BURST_COUNT_MAX);
        if (canFire)
            stateTime = timerGetFieldCount() + burstCount * FIRE_DELAY_MAX;
        else
            stateTime = timerGetFieldCount() + mathfRand(HOVER_DELAY_MIN, HOVER_DELAY_MAX);
        break;
    case STATE_3:
        nav.flee(*PIN_TRANS(target), s_safeRange, 0.0f);
        nextBurstTime = timerGetFieldCount() + mathfRand(BURST_DELAY_MIN, BURST_DELAY_MAX);
        break;
    case STATE_4: {
        float f;

        nav.disable();
        f = mathfRandf(0.5f, 1.0f);
        if (mathfRand(0, 1) == 0)
            f = -f;
        spin[0] = f;
        f = mathfRandf(0.5f, 1.0f);
        spin[1] = mathfRand(0, 1) ? f : -f;
        spin[2] = mathfRandf(0.25f, 1.0f);
        f = mathfRandf(0.25f, 1.0f);
        if (mathfRand(0, 1) == 0)
            f = -f;
        spin[3] = f;
        break;
    }
    case STATE_5:
        nav.disable();
        break;
    }
    state = s;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", enterState__10HeliPickupQ210HeliPickup5State);
#endif
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", _vt$10HeliPickup);
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", func_0013F610);
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", func_0013F620);
INCLUDE_ASM("asm/nonmatchings/game/HeliPickup", __tf10HeliPickup);
_fvector *HeliPickup::getVel(void)
{
    return &hv.vel;
}
