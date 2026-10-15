#include "common.h"
#include "game/game.h"
#include "game/sound.h"
#include "game/hit_event.h"
#include "task_manager.h"
#include "cs_pool.h"
#include "hd.h"
#include "vecmath.h"

/* The sucker bug: a homing projectile (one pool of 12, 0xC0 each). A monster with the bug special (Kineticlops) launches one; it seeks the
 * closest visible monster, steers around the collision hulls it feels, latches onto the victim's head and drains health until it is shaken
 * off, the victim lost s_maxMonsterDamage or s_attachFields fields went by. States (m_state): 0 off, 1 reserved by getInstance, 2 shown in
 * the owner's hand, 3 seeking, 4 attached, 5 falling. Virtual in retail (DbInteractive: vptr at 0x10, takeHit in the vtable); the vtable,
 * ctor and __tf stay asm. */

extern "C" float cosf(float);
extern "C" float sinf(float);
extern "C" float fabsf(float);

int particleCreateFx(_fvector *pos, int id, float scale, float (*m)[4], float strength);
void particleKillFx(int &handle);
void hdRemoveCsFromGrid(_cs *cs);
void hdReparentCsGrid(_cs *cs);
int hdSphereTest(_fvector *p, float r, _hdResult *res, unsigned skipId2, int max);
int hdLosTest(_fvector *from, _fvector *to, _hdResult *res, bool option);
void mathfVectorCrossUp(_fvector *dst, _fvector *v);
void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
void mathfRotationFromPointToPoint(_fvector *rot, _fvector *from, _fvector *to);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rot);
struct ActuatorData {
    int w[8];
};
void inputSetActuator(int pad, ActuatorData *a);
extern float UNIT_MATRIX[4][4];

class DbInteractive {
public:
    int m_kind;               /* 0x00: the ctor sets 6 */
    unsigned short m_flags;   /* 0x04: bit 1 targetable, bit 2 ... (spawn sets 6, attach clears bit 1) */
    char pad6[6];
    _cs *m_cs;                /* 0x0C */
    void *m_vptr;             /* 0x10 */
};

class Interactives {
public:
    static int addInteractive(DbInteractive *p);
    static DbInteractive *getInteractive(int i);
    static void dispatchHit(_fvector *dir, float dmg, int attackerId, int targetId);
};

class PointMass {
public:
    _fvector *pos;
    char pad[12];
    FVECTOR vel;
    FVECTOR force;

    void init(float mass, _fvector *p);
    void applyDampeningForce(float d);
    void resolveForcesEuler(float dt);
};

class HomingBug : public DbInteractive {
public:
    SuckerBugSound m_sound;   /* 0x14 */
    char pad1C[4];
    PointMass m_pm;           /* 0x20: pos -> m_cs->trans; vel 0x30, force 0x40 */
    char pad50[4];
    _animHandle m_anims[3];   /* 0x54: 0 held (spawn), 1 flying (launch), 2 attached (attach) */
    int m_fx;                 /* 0x84: trail particles (0x6C) */
    int m_fx2;                /* 0x88: second trail (0x6D) */
    int m_state;              /* 0x8C */
    int m_fields;             /* 0x90: fields in the current state */
    int m_stillFields;        /* 0x94: consecutive slow fields while seeking */
    float m_health;           /* 0x98 */
    char pad9C[4];
    FVECTOR m_attachDir;      /* 0xA0: the bug's forward axis in the victim's head frame, kept while attached */
    Monster *m_owner;         /* 0xB0: the monster that launches it (last slot added when the model was registered) */
    Monster *m_target;        /* 0xB4 */
    int m_lockedInView;       /* 0xB8: target in range, in front and in line of sight (refreshed every 10 fields) */
    float m_targetHealth;     /* 0xBC: victim's health when the bug attached */

    static int s_numBugs;
    static HomingBug s_bugPool[12];
    static float s_homingFactor;
    static float s_avoidanceFactor;
    static float s_maxForce;
    static float s_maxVel;
    static float s_initialVel;
    static float s_initialHealth;
    static float s_attachRange;
    static float s_attachMinAngle;
    static float s_attachFields;
    static float s_maxSeekFields;
    static float s_headRadius;
    static float s_feelerRadius;
    static float s_collisRadius;
    static float s_damageRadius;
    static float s_damagePerSecond;
    static float s_maxMonsterDamage;
    static float s_targetLockMaxRange;
    static float s_targetLockMinAngle;
    static float s_targetLockDotMin;

    static void init(void);
    static void addInstance(_hierhead *h);
    static HomingBug *getInstance(Monster *m);
    static bool isAvailable(Monster *m);
    static void initAfterDbLoad(void);
    void registerModel(_hierhead *h);
    void initAfter(void);
    void spawn(void);
    void launch(Monster *target);
    unsigned update(void);
    void updateOrientFromDir(_fvector &dir);
    void takeHit(_fvector *dir, float dmg, int attackerId);
    void updateSeek(void);
    bool checkAttach(void);
    void attach(void);
    void updateAttached(void);
    void updateFalling(void);
    void disable(void);
    Monster *findTarget(_fvector &pos, _fvector &dir, float dotMin, float range);
    bool inView(Monster *m, float dotMin, float range, bool los);
    static unsigned update(void *p);
};
typedef char _size_HomingBug[sizeof(HomingBug) == 0xC0 ? 1 : -1];

extern float STILL_VEL;
extern float STILL_FIELDS;
__asm__("#SNFIX_SMALL STILL_VEL");
__asm__("#SNFIX_SMALL STILL_FIELDS");
__asm__("#SNFIX_SMALL _9HomingBug$s_numBugs");
extern int craterSwitched;
extern int numDeadHeads;
extern char plantBoss[];      /* the three plant heads, 0x250 apart; +0x1C0 is the head's aim point */
extern char finalBoss[];      /* +0x1C phase (0 = the big robot), +0x60 its aim point */

/* Monster fields the bug uses that game.h does not name yet */
#define MON_POS(m) ((_fvector *)((char *)(m) + 0x3E50))            /* root position */
#define MON_HEAD(m) ((_fvector *)((char *)(m) + 0x3E60))           /* head position (what the bug seeks and sits on) */
#define MON_HEADMAT(m) ((float (*)[4])((char *)(m) + 0x3400))      /* head frame (rows are the axes) */
#define MON_BUGGED(m) (*(int *)((char *)(m) + 0x130))              /* a bug is on it (not set for type bits 0x1A0 / 0x1C0) */
#define CS_FWD(cs) ((_fvector *)(cs)->mat[1])

/* Three Mile Island once the crater switched: the bugs fly at the plant boss head that is still alive (numDeadHeads picks it; with all
 * three dead `aim` stays uninitialised, as in retail). */
#define AIM_AT_PLANT_HEAD(cs, aim)                                                    \
    do {                                                                              \
        if (numDeadHeads == 0)                                                        \
            vecCopy(&aim, (_fvector *)(plantBoss + 0x1C0));                               \
        else if (numDeadHeads == 1)                                                   \
            vecCopy(&aim, (_fvector *)(plantBoss + 0x410));                               \
        else if (numDeadHeads == 2)                                                   \
            vecCopy(&aim, (_fvector *)(plantBoss + 0x660));                               \
        mathfRotationFromPointToPoint(&(cs)->rot, &(cs)->trans, &aim);                \
        mathfRotMatrixRPH((cs)->mat, &(cs)->rot);                                     \
    } while (0)

#ifdef NON_MATCHING
/* equivalent: 48/70 words; the shared 2.0/800.0 constants take other FPRs */
void HomingBug::init(void)
{
    s_avoidanceFactor = 0.5f;
    s_maxVel = 1200.0f;
    s_initialVel = 800.0f;
    s_initialHealth = 2.0f;
    s_attachRange = 25.0f;
    s_attachMinAngle = 45.0f;
    s_attachFields = 180.0f;
    s_maxSeekFields = 360.0f;
    s_headRadius = 5.0f;
    s_feelerRadius = 60.0f;
    s_collisRadius = 10.0f;
    s_damageRadius = 50.0f;
    s_damagePerSecond = 15.0f;
    s_maxMonsterDamage = 30.0f;
    s_targetLockMaxRange = 1000.0f;
    s_targetLockMinAngle = 90.0f;
    s_numBugs = 0;
    s_homingFactor = 2.0f;
    s_maxForce = 800.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", init__9HomingBug);
#endif

void HomingBug::addInstance(_hierhead *h)
{
    s_bugPool[s_numBugs++].registerModel(h);
}

/* Reserves (state 1) a free bug of monster m. */
HomingBug *HomingBug::getInstance(Monster *m)
{
    HomingBug *b = s_bugPool;
    int n;

    for (n = s_numBugs; n != 0; n--, b++) {
        if (b->m_owner == m && b->m_state == 0) {
            b->m_state = 1;
            return b;
        }
    }
    return 0;
}

bool HomingBug::isAvailable(Monster *m)
{
    HomingBug *b = s_bugPool;
    int n;

    for (n = s_numBugs; n != 0; n--, b++) {
        if (b->m_owner == m && b->m_state == 0)
            return true;
    }
    return false;
}

void HomingBug::initAfterDbLoad(void)
{
    HomingBug *b = s_bugPool;
    int n;

    for (n = s_numBugs; n != 0; n--, b++) {
        b->m_sound.initSuckerBugSound();
        b->initAfter();
    }
}

INCLUDE_ASM("asm/nonmatchings/game/HomingBug", __9HomingBug);

void HomingBug::registerModel(_hierhead *h)
{
    m_cs = CsPool::csActivate();
    m_cs->epNode = h;
    m_cs->drawMe = 0;
    m_cs->testCollision = 1;
    h->id2 = Interactives::addInteractive(this);
    m_owner = gameSlotBase(game->m_numSlots - 1);
}

void HomingBug::initAfter(void)
{
    int i;

    for (i = 0; i < 3; i++)
        animationGetHandle(&m_anims[i], *(unsigned *)m_cs->epNode >> 18, m_owner->m_dupId, i);
    disable();
}

/* Shows the bug at the owner, pointing where it looks, and starts its task. */
void HomingBug::spawn(void)
{
    if (m_cs != 0) {
        vecCopy(&m_cs->trans, MON_POS(m_owner));
        updateOrientFromDir(*CS_FWD(m_owner->m_cs));
        m_cs->drawMe = 1;
        m_cs->testCollision = 0;
        m_state = 2;
        m_target = 0;
        m_health = s_initialHealth;
        m_flags |= 6;
        m_fields = 0;
        animationStart(m_anims[0], true);
        TaskManager::global.add(update, this, 1);
    }
}

#ifdef NON_MATCHING
/* equivalent: 114/125 words; register allocation (a0/a1) around the level check */
void HomingBug::launch(Monster *target)
{
    s_targetLockDotMin = cosf(s_targetLockMinAngle * 0.5f); /* degrees passed as radians, as in retail */
    vecCopy(&m_cs->trans, MON_POS(m_owner));
    if (game->m_levelId == 6 && craterSwitched != 0) {
        FVECTOR aim;
        AIM_AT_PLANT_HEAD(m_cs, aim);
    } else {
        updateOrientFromDir(*CS_FWD(m_owner->m_cs));
    }
    m_cs->drawMe = 1;
    m_cs->testCollision = 1;
    m_pm.init(1.0f, &m_cs->trans);
    vecScale(&m_pm.vel, CS_FWD(m_cs), s_initialVel);
    m_target = target;
    m_state = 3;
    animationTransitionInto(m_anims[1], 160.0f, 1, 1);
    m_fx = particleCreateFx(&m_cs->trans, m_cs->mat, 0x6C, 1.0f, 0, 0, false, 0.0f);
    m_fx2 = particleCreateFx(&m_cs->trans, m_cs->mat, 0x6D, 1.0f, 0, 0, false, 0.0f);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", launch__9HomingBugP7Monster);
#endif

/* Task body: 0 once the bug is off (the task ends). */
unsigned HomingBug::update(void)
{
    unsigned alive = 1;

    switch (m_state) {
    case 0:
    case 1:
    default:
        alive = 0;
        break;
    case 2:
        vecCopy(&m_cs->trans, MON_POS(m_owner));
        if (game->m_levelId == 6 && craterSwitched != 0) {
            FVECTOR aim;
            AIM_AT_PLANT_HEAD(m_cs, aim);
        } else {
            updateOrientFromDir(*CS_FWD(m_owner->m_cs));
        }
        break;
    case 3:
        updateSeek();
        break;
    case 4:
        updateAttached();
        break;
    case 5:
        updateFalling();
        break;
    }
    hdReparentCsGrid(m_cs);
    m_fields++;
    m_sound.updateSuckerBugSound(&m_cs->trans, m_state);
    return alive;
}

/* Forward axis = dir (row 1), side = forward x up (row 0), up = side x forward (row 2). */
#ifdef NON_MATCHING
/* equivalent: 29/61 words; retail keeps the side axis in FPRs across the normalisation (FPR allocation, store order) */
void HomingBug::updateOrientFromDir(_fvector &dir)
{
    float k;

    vecCopy(CS_FWD(m_cs), &dir);
    m_cs->mat[1][3] = 0.0f;
    if (dir.z < 0.99f)
        mathfVectorCrossUp((_fvector *)m_cs->mat[0], &dir);
    else
        vecCross((_fvector *)m_cs->mat[0], &dir, (_fvector *)UNIT_MATRIX[1]);
    {
        _fvector *s = (_fvector *)m_cs->mat[0];

        k = eeRsqrtf(1.0f, vecDot(s, s));
        s->x *= k;
        s->z *= k;
        s->y *= k;
    }
    vecCross((_fvector *)m_cs->mat[2], (_fvector *)m_cs->mat[0], CS_FWD(m_cs));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", updateOrientFromDir__9HomingBugR8_fvector);
#endif

void HomingBug::takeHit(_fvector *dir, float dmg, int attackerId)
{
    if (dmg < 0.1f)
        return;
    m_health -= dmg;
    if (m_health <= 0.0f) {
        disable();
        particleKillFx(m_fx);
        particleKillFx(m_fx2);
    }
}

#ifdef NON_MATCHING
/* Hulls the bug bursts on, hurting their owner, instead of steering around them: boss parts by hat id (0x27A6 is the final boss's big
   robot in CAPITOL.NGP; the 0x271B-0x271D trio and 0x2728 are presumably the plant boss heads and its stalk). */
static inline bool isBossHull(unsigned id1)
{
    return id1 == 0x271B || id1 == 0x271C || id1 == 0x271D || id1 == 0x2728 || id1 == 0x27A6;
}
#endif

#ifdef NON_MATCHING
/* equivalent: 78/453 words; block layout (the boss-hull hit block sits before the steering tail in retail) and FPR allocation */
void HomingBug::updateSeek(void)
{
    FVECTOR steer;
    FVECTOR toTarget;
    FVECTOR aim;
    _hdResult res[10];
    FVECTOR avoid;
    float homing;
    int n;

    if ((float)m_fields > s_maxSeekFields) {
        m_state = 5;
        return;
    }
    if (m_target != 0) {
        if (m_fields % 10 == 0)
            m_lockedInView = inView(m_target, -2.0f, s_targetLockMaxRange, true);
        if (!(m_target->m_flags & 2))
            m_target = 0;
    } else {
        m_target = findTarget(m_cs->trans, *CS_FWD(m_cs), s_targetLockDotMin, s_targetLockMaxRange);
        m_lockedInView = 0;
    }
    homing = 0.5f;
    if (m_lockedInView != 0)
        homing = s_homingFactor;

    if (game->m_gameMode == 1 && game->m_levelId == 6 && craterSwitched != 0) {
        s_avoidanceFactor = 0.1f;
        AIM_AT_PLANT_HEAD(m_cs, aim);
        vecScale(&steer, CS_FWD(m_cs), s_maxForce);
    } else if (game->m_gameMode == 1 && game->m_levelId == 11 && *(int *)(finalBoss + 0x1C) == 0) {
        mathfRotationFromPointToPoint(&m_cs->rot, &m_cs->trans, (_fvector *)(finalBoss + 0x60));
        mathfRotMatrixRPH(m_cs->mat, &m_cs->rot);
        vecScale(&steer, CS_FWD(m_cs), s_maxForce);
    } else if (m_target != 0) {
        float k;

        vecSub(&toTarget, MON_HEAD(m_target), &m_cs->trans);
        k = eeRsqrtf(1.0f, vecDot(&toTarget, &toTarget));
        steer.z = toTarget.z * k;
        steer.x = toTarget.x * k;
        steer.y = toTarget.y * k;
        vecScale(&steer, &steer, s_maxVel);
        vecSub(&steer, &steer, &m_pm.vel);
        vecScale(&steer, &steer, homing);
    } else {
        vecScale(&steer, CS_FWD(m_cs), s_maxForce);
    }

    /* feelers: steer away from the deepest penetration on each axis */
    n = hdSphereTest(&m_cs->trans, s_feelerRadius, res, ((_hierhead *)m_cs->epNode)->id2, 10);
    if (n > 0) {
        _hdResult *r = res;
        float d;

        avoid.x = 0.0f;
        avoid.y = 0.0f;
        avoid.z = 0.0f;
        do {
            unsigned id1 = (r->ids >> 11) & 0x3FFF;
            FVECTOR push;

            if (id1 - 0x20 < 0x3E1) {
                DbInteractive *it = Interactives::getInteractive(r->ids & 0x7FF);

                if (it != (DbInteractive *)m_owner) {
                    m_target = (Monster *)it;
                    attach();
                    return;
                }
            } else if (isBossHull(id1)) {
                HitEvent *ev = HitEvent::create();

                ev->source = 5;
                ev->pos = &r->point;
                ev->type = 3;
                Interactives::dispatchHit(&toTarget, s_damagePerSecond * s_attachFields / 60.0f, m_owner->m_id, (int)r->ids & 0x7FF);
                disable();
                return;
            } else {
                vecSub(&push, &r->point, &m_cs->trans);
                if (vecDot(&push, &push) < s_collisRadius * s_collisRadius) {
                    disable();
                    return;
                }
            }
            vecScale(&push, &r->normal, r->depth);
            if (fabsf(avoid.x) < fabsf(push.x))
                avoid.x = push.x;
            if (fabsf(avoid.y) < fabsf(push.y))
                avoid.y = push.y;
            if (fabsf(avoid.z) < fabsf(push.z))
                avoid.z = push.z;
            r++;
        } while (--n != 0);

        {
            _fvector *f = CS_FWD(m_cs);
            float back = -vecDot(f, &avoid);

            d = (back > 0.0f ? back : 0.0f) / s_feelerRadius;
        }
        if (d > 0.0f) {
            float force = s_maxForce * s_avoidanceFactor;

            vecScale(&avoid, &avoid, force * d);
            vecScale(&steer, &steer, 1.0f - d);
            if (m_lockedInView == 0)
                steer.z += force * 0.7f;
            vecAdd(&steer, &steer, &avoid);
        }
    }

    m_pm.force.x = 0.0f;
    m_pm.force.y = 0.0f;
    m_pm.force.z = 0.0f;
    vecAdd(&m_pm.force, &m_pm.force, &steer);
    m_pm.applyDampeningForce(s_maxForce / s_maxVel);
    m_pm.resolveForcesEuler(1.0f / timerGetUpdateRate());
    {
        float l2 = vecDot(&m_pm.vel, &m_pm.vel);
        float k = eeRsqrtf(1.0f, l2);

        steer.z = m_pm.vel.z * k;
        steer.x = m_pm.vel.x * k;
        steer.y = m_pm.vel.y * k;
        if (l2 > 0.0001f)
            updateOrientFromDir(steer);
    }
    if (vecDot(&m_pm.vel, &m_pm.vel) < STILL_VEL * STILL_VEL) {
        /* retail drops the bug on the first slow field (the counter test reads inverted) */
        if ((float)++m_stillFields < STILL_FIELDS) {
            m_state = 5;
            return;
        }
    } else {
        m_stillFields = 0;
    }
    checkAttach();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", updateSeek__9HomingBug);
#endif

#ifdef NON_MATCHING
/* equivalent: 28/32 words; FPR allocation of the range compare */
bool HomingBug::checkAttach(void)
{
    if (m_target != 0) {
        FVECTOR d;

        vecSub(&d, &m_cs->trans, MON_HEAD(m_target));
        if (vecDot(&d, &d) <= s_attachRange * s_attachRange) {
            attach();
            return true;
        }
        return false;
    }
    return false;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", checkAttach__9HomingBug);
#endif

/* Latches onto the target's head: if the bug comes in more than s_attachMinAngle off the head's up axis it is turned onto that cone. */
#ifdef NON_MATCHING
/* equivalent: 44/130 words; load order of the dot product, then everything shifts by the sqrt/sinf delay slot */
void HomingBug::attach(void)
{
    float (*hm)[4] = MON_HEADMAT(m_target);
    _fvector *up = (_fvector *)hm[2];
    _fvector *f = CS_FWD(m_cs);
    float dot = vecDot(f, up);
    float ang = s_attachMinAngle * 0.017453292f;
    float c = cosf(ang);

    if (dot < c) {
        FVECTOR a, b;
        register float len __asm__("$f20");

        vecScale(&a, up, -dot);
        vecAdd(&b, CS_FWD(m_cs), &a);
        len = vecDot(&b, &b);
        __asm__(".word 0x46140504" : "+f"(len)); /* sqrt.s $f20, $f20 (R5900 form, see eeSqrtf) */
        vecScale(&b, &b, (sinf(ang) - len) / len);
        vecAdd(&a, &a, &b);
        vecScale(&b, up, -c);
        vecAdd(&a, &a, &b);
        vecAdd(CS_FWD(m_cs), CS_FWD(m_cs), &a);
    }
    mathfMulVec(hm, CS_FWD(m_cs), &m_attachDir);
    animationTransitionInto(m_anims[2], 96.0f, 1, 1);
    m_state = 4;
    m_fields = 0;
    m_cs->testCollision = 0;
    m_targetHealth = m_target->m_health;
    m_flags &= ~2;
    updateAttached();
    particleKillFx(m_fx);
    if (m_target->m_typeBits != 0x1A0 && m_target->m_typeBits != 0x1C0)
        MON_BUGGED(m_target) = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", attach__9HomingBug);
#endif

#ifdef NON_MATCHING
/* equivalent: 68/102 words; FPR allocation and branch shape of the two disable tests */
void HomingBug::updateAttached(void)
{
    FVECTOR dir;

    vecApplyRows(&dir, MON_HEADMAT(m_target), &m_attachDir);
    updateOrientFromDir(dir);
    vecScale(&m_cs->trans, &dir, -s_headRadius);
    vecAdd(&m_cs->trans, &m_cs->trans, MON_HEAD(m_target));
    m_target->takeDamage(s_damagePerSecond / 60.0f, false, m_owner);
    if (m_targetHealth - m_target->m_health > s_maxMonsterDamage || m_target->m_unkF6 != 0)
        disable();
    if ((float)m_fields > s_attachFields)
        disable();
    if (m_target->m_playerNum == 1) {
        ActuatorData a = *(ActuatorData *)((char *)game + 0x1227C0);
        inputSetActuator(m_target->m_index, &a);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", updateAttached__9HomingBug);
#endif

#ifdef NON_MATCHING
/* equivalent: 6/86 words; retail zeroes the force through a pointer and reads it via the PointMass base (scheduling shifts everything) */
void HomingBug::updateFalling(void)
{
    FVECTOR dir;
    _hdResult res;

    m_pm.force.x = 0.0f;
    m_pm.force.z = 0.0f;
    m_pm.force.y = 0.0f;
    m_pm.force.z += game->m_gravity;
    m_pm.resolveForcesEuler(1.0f / timerGetUpdateRate());
    {
        float l2 = vecDot(&m_pm.vel, &m_pm.vel);
        float k = eeRsqrtf(1.0f, l2);

        dir.z = m_pm.vel.z * k;
        dir.x = m_pm.vel.x * k;
        dir.y = m_pm.vel.y * k;
        if (l2 > 0.0001f)
            updateOrientFromDir(dir);
    }
    if (!checkAttach()) {
        if (hdSphereTest(&m_cs->trans, s_collisRadius, &res, ((_hierhead *)m_cs->epNode)->id2, 1) != 0 ||
            m_cs->trans.z < game->m_defaultHeight)
            disable();
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", updateFalling__9HomingBug);
#endif

void HomingBug::disable(void)
{
    m_cs->drawMe = 0;
    m_state = 0;
    hdRemoveCsFromGrid(m_cs);
    particleKillFx(m_fx);
    particleKillFx(m_fx2);
    particleCreateFx(&m_cs->trans, 0x4E, 3.0f, 0, 0.0f);
    particleCreateFx(&m_cs->trans, 0x4D, 5.0f, 0, 0.0f);
    particleCreateFx(&m_cs->trans, 0x4F, 5.0f, 0, 0.0f);
    if (m_target != 0)
        MON_BUGGED(m_target) = 0;
    m_sound.terminateSuckerBugSound();
}

/* The last monster (in slot order) other than the owner that is shown, not cloaked, targetable and in view; retail computes the distance
 * to pos but never compares it. dir is unused. */
#ifdef NON_MATCHING
/* equivalent: 66/71 words; FPR allocation of the (unused) distance */
Monster *HomingBug::findTarget(_fvector &pos, _fvector &dir, float dotMin, float range)
{
    Monster *best = 0;
    Monster *m = game->m_slots;
    int n;

    for (n = game->m_numSlots; n != 0; n--, m++) {
        FVECTOR d;
        float dist;

        if (m == m_owner || !m->m_cs->drawMe || m->m_cloaked != 0 || !(m->m_flags & 2))
            continue;
        if (!inView(m, dotMin, range, false))
            continue;
        vecSub(&d, &pos, MON_HEAD(m));
        dist = vecDot(&d, &d);
        best = m;
    }
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", findTarget__9HomingBugR8_fvectorT1ff);
#endif

/* m's head within range, in front of the bug, at most acos(dotMin) off its forward axis and (los) in line of sight. */
#ifdef NON_MATCHING
/* equivalent: 11/50 words; retail loads the forward axis through a pointer temp and returns through a shared epilogue */
bool HomingBug::inView(Monster *m, float dotMin, float range, bool los)
{
    FVECTOR d;
    _hdResult res;
    _fvector *f = CS_FWD(m_cs);
    float l2, dot, c;

    vecSub(&d, MON_HEAD(m), &m_cs->trans);
    l2 = vecDot(&d, &d);
    if (range * range < l2)
        return false;
    dot = vecDot(&d, f);
    if (dot < 0.0f)
        return false;
    __asm__("rsqrt.s %0, %1, %2" : "=f"(c) : "f"(dot), "f"(l2));
    if (c < dotMin)
        return false;
    if (los && !hdLosTest(&m_cs->trans, MON_HEAD(m), &res, false))
        return false;
    return true;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", inView__9HomingBugP7Monsterffb);
#endif

INCLUDE_ASM("asm/nonmatchings/game/HomingBug", __static_initialization_and_destruction_0_00142EB8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", func_00142F20);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", func_00142F30);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", func_00142F58);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", _vt$9HomingBug);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", __tf9HomingBug);

unsigned HomingBug::update(void *p)
{
    return ((HomingBug *)p)->update();
}

INCLUDE_ASM("asm/nonmatchings/game/HomingBug", func_00142FD8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", _GLOBAL_$I$_9HomingBug$s_numBugs);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A10);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A20);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A30);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A40);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A50);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A60);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A78);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A88);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9A98);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9AB0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9AC0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9AD0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9AE0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9AF8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B08);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B18);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B28);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B38);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B48);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B60);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B70);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B80);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9B98);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9BA8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9BB8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9BC8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9BD8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9BF0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C00);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C18);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C28);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C38);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C50);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C68);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C80);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9C90);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9CA0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9CC8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9CE8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9D00);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9D20);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9D48);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9D70);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9D90);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9DB0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9DD8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9E00);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9E28);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9E58);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9E80);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9EA8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9ED0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9EF8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9F20);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9F40);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9F58);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9F80);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9FA0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9FB8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006E9FD8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA008);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA030);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA058);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA078);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA098);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA0B8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA0D0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA0E8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA108);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA130);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA148);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA168);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA190);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA1B8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA1D0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA1F8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA220);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA248);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA268);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA288);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA2B0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA2D8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA2F8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA328);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA358);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA380);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA3B0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA3E0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA408);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA428);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA448);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA470);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA498);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA4C0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA4E8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA508);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA520);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA538);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA560);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA588);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA5A8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA5C8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA5E8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA610);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA638);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA650);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA670);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA690);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA6A8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA6D0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA6F8);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA720);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA740);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA760);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA778);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA798);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA7B0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA7D0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA7F0);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA808);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA820);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA848);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA870);
INCLUDE_ASM("asm/nonmatchings/game/HomingBug", D_006EA890);
