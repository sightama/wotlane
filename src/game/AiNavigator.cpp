#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

class Monster;
class AiPath;
class AiPathNet {
public:
    AiPath *getClosestPath(_fvector &pos, unsigned char flags);
};
struct _hdResult;
struct DbInteractive;

extern AiPathNet *s_net;
__asm__("#SNFIX_SMALL s_net");

static inline _cs *navCs(Monster *m)
{
    return *(_cs **)((char *)m + 0xC);
}
static inline unsigned hatId(DbInteractive &it)
{
    return (*(unsigned *)(*(_cs **)((char *)&it + 0xC))->epNode >> 7) & 0x7FF;
}

class AiPathFinder {
public:
    char pad[0x28];
    AiPathFinder(AiPathNet &net, Monster &m);
};

/* Steering/pathing helper of an AI monster. Members are reached by retail offset; sizeof is not known yet (PathInfo is the last member). */
class AiNavigator {
public:
    enum Status { STATUS_0, STATUS_1, STATUS_2, STATUS_3 };
    enum FailureHint { HINT_0 };

    /* Four probes in front of the monster: obstacle (0x10), second obstacle (0x50), floor (0x80), ceiling (0xC0). */
    class ObstacleSensor {
    public:
        AiNavigator *nav;       /* 0x00 */
        char pad4[0x10 - 4];
        char obstacleHit;       /* 0x10 */
        char pad11[0x28 - 0x11];
        float obstacleHeight;   /* 0x28 */
        char pad2C[0x40 - 0x2C];
        float obstacleRange;    /* 0x40 */
        char pad44[0x50 - 0x44];
        char obstacle2Hit;      /* 0x50 */
        char pad51[0x68 - 0x51];
        float obstacle2Height;  /* 0x68 */
        char pad6C[0x80 - 0x6C];
        char floorHit;          /* 0x80 */
        char pad81[0x98 - 0x81];
        float floorHeight;      /* 0x98 */
        char pad9C[0xC0 - 0x9C];
        char ceilHit;           /* 0xC0 */
        char padC1[0xD8 - 0xC1];
        float ceilHeight;       /* 0xD8 */
        char padDC[0x120 - 0xDC];
        float param120;         /* 0x120: 35.0 */
        float param124;         /* 0x124: 0.32 */
        float param128;         /* 0x128: 45.0 */
        float param12C;         /* 0x12C: 100.0 */

        ObstacleSensor(AiNavigator &n);
        void init(void);
        float getObstacleRange(void);
        float getObstacleHeight(void);
        _hdResult *getObstacle(void);
        float getCeilHeight(void);
        float getFloorHeight(void);
    };

    class PathInfo {
    public:
        char pad[0x20];
        void init(AiPath *path, _fvector *pos);
        void init(AiPath *path, int dir);
        void init(AiPath *path, AiPath *from, bool b);
    };

    Monster *monster;           /* 0x000 */
    float f4;                   /* 0x004 */
    float f8;                   /* 0x008 */
    float turn;                 /* 0x00C: accumulated steering to the side (orientTo) */
    char pad10[4];
    float strafe;               /* 0x014 */
    int mode;                   /* 0x018: 0 off, 1 wander, 3 target, 6 flee ... */
    int status;                 /* 0x01C */
    int failureHint;            /* 0x020 */
    char pad24[0x30 - 0x24];
    ObstacleSensor sensor;      /* 0x030 */
    AiPathFinder pathFinder;    /* 0x160 */
    _fvector *goal;             /* 0x188: where wander/target/flee steer */
    int f18C;                   /* 0x18C */
    int f190;                   /* 0x190 */
    float f194;                 /* 0x194 */
    float f198;                 /* 0x198 */
    float f19C;                 /* 0x19C */
    char pad1A0[0x218 - 0x1A0];
    int f218;                   /* 0x218 */
    int f21C;                   /* 0x21C */
    PathInfo pathInfo;          /* 0x220 */
    PathInfo nextPathInfo;      /* 0x240 */

    AiNavigator(Monster &m);
    void init(void);
    float orientTo(_fvector &dir, float tol);
    float strafeTo(_fvector &pos, float width, float tol);
    float targetPin(_fvector &p);
    void updateTarget(void);
    void seek(_fvector &p, float r);
    void seek(DbInteractive &it, float r);
    void arrive(_fvector &p, float a, float b, float c);
    void arrive(DbInteractive &it, float a, float b, float c);
    void wander(_fvector &p);
    void target(_fvector &p, float r);
    void flee(_fvector &p, float r);
    void disable(Status s, FailureHint h);
    void updateArrive(void);
    void updateSeek(void);
    AiPath *getFleePath(_fvector &p);
    void nextPathPoint(void);
    void tag(_fvector &p, float r);
};

AiNavigator::AiNavigator(Monster &m) : monster(&m), sensor(*this), pathFinder(*s_net, m)
{
}
#ifdef NON_MATCHING
/* 21/23 words: store/call scheduling */
void AiNavigator::init(void)
{
    disable(STATUS_3, HINT_0);
    f4 = 1.0f;
    f8 = 1.0f;
    pathInfo.init(*(AiPath **)((char *)monster + 0x1A10), (_fvector *)((char *)navCs(monster) + 0x10));
    sensor.init();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", init__11AiNavigator);
#endif
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", update__11AiNavigatorR7GamePad);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", updateSteering__11AiNavigatorR7GamePad);
void AiNavigator::disable(Status s, FailureHint h)
{
    f218 = 0;
    mode = 0;
    f21C = 0;
    failureHint = h;
    status = s;
}
void AiNavigator::wander(_fvector &p)
{
    goal = &p;
    mode = 1;
    status = 0;
}
#ifdef NON_MATCHING
/* Navigation mode 2: go to `p`, stopping within `r`. */
void AiNavigator::seek(_fvector &p, float r)
{
    goal = &p;
    f18C = 0;
    f190 = (int)s_net->getClosestPath(p, *(unsigned char *)(*(char **)((char *)monster + 0x1A10) + 0xC));
    f198 = r;
    f19C = 1.0f;
    mode = 2;
    f194 = r;
    status = 0;
    pathInfo.init(*(AiPath **)((char *)monster + 0x1A10), goal);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", seek__11AiNavigatorR8_fvectorf);
#endif
void AiNavigator::seek(DbInteractive &it, float r)
{
    seek(*(_fvector *)((char *)*(_cs **)((char *)&it + 0xC) + 0x10), r);
    f18C = hatId(it);
}
void AiNavigator::target(_fvector &p, float r)
{
    status = 0;
    f18C = 0;
    f19C = r;
    mode = 3;
    goal = &p;
}
/* Navigation mode 4: go to `p` and slow down on arrival (a, b, c are the arrival parameters). */
void AiNavigator::arrive(_fvector &p, float a, float b, float c)
{
    goal = &p;
    f18C = 0;
    f190 = (int)s_net->getClosestPath(p, *(unsigned char *)(*(char **)((char *)monster + 0x1A10) + 0xC));
    f194 = a;
    f198 = b;
    f19C = c;
    mode = 4;
    status = 0;
    pathInfo.init(*(AiPath **)((char *)monster + 0x1A10), goal);
}
void AiNavigator::arrive(DbInteractive &it, float a, float b, float c)
{
    arrive(*(_fvector *)((char *)*(_cs **)((char *)&it + 0xC) + 0x10), a, b, c);
    f18C = hatId(it);
}
#ifdef NON_MATCHING
/* Navigation mode 5: chase `p` (tag game). */
void AiNavigator::tag(_fvector &p, float r)
{
    goal = &p;
    f18C = 0;
    f190 = (int)s_net->getClosestPath(p, *(unsigned char *)(*(char **)((char *)monster + 0x1A10) + 0xC));
    f198 = r;
    f19C = 1.0f;
    mode = 5;
    f194 = r;
    status = 0;
    pathInfo.init(*(AiPath **)((char *)monster + 0x1A10), goal);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", tag__11AiNavigatorR8_fvectorf);
#endif
#ifdef NON_MATCHING
/* 10/12 words: store order of the eight fields differs */
void AiNavigator::flee(_fvector &p, float r)
{
    f194 = r;
    goal = &p;
    f198 = r;
    f190 = 0;
    f19C = 1.0f;
    f18C = 0;
    mode = 6;
    status = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", flee__11AiNavigatorR8_fvectorf);
#endif
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", updateSeek__11AiNavigator);
#ifdef NON_MATCHING
/* 16/19 words: register choice */
void AiNavigator::updateTarget(void)
{
    float t = targetPin(*goal);

    if (fabsf(t) <= f19C)
        disable(STATUS_1, HINT_0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", updateTarget__11AiNavigator);
#endif
void AiNavigator::updateArrive(void)
{
    updateSeek();
}
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", updateTag__11AiNavigator);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", updateFlee__11AiNavigator);
#ifdef NON_MATCHING
/* 9/36 words: register allocation of the normalising multiplies */
float AiNavigator::targetPin(_fvector &p)
{
    _fvector d;
    register float inv;

    vecSub(&d, &p, (_fvector *)((char *)navCs(monster) + 0x10));
    float len2 = vecLenSq(&d);

    __asm__("rsqrt.s %0, %1, %2" : "=f"(inv) : "f"(1.0f), "f"(len2));
    d.x *= inv;
    d.y *= inv;
    d.z *= inv;
    return orientTo(d, 0.05f);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", targetPin__11AiNavigatorR8_fvector);
#endif
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", pathSeekIntercept__11AiNavigator);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", pathSeek__11AiNavigator);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", avoid__11AiNavigatorR8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", avoidClimbing__11AiNavigatorR8_fvectorT1);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", avoidJumping__11AiNavigatorR8_fvectorT1);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", avoidFlying__11AiNavigatorR8_fvectorT1);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", avoidGeneral__11AiNavigatorR8_fvectorT1);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", attemptClimb__11AiNavigatorR8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", handleNoJumpOrClimb__11AiNavigatorP9_hdResultR8_fvectorT2);
#ifdef NON_MATCHING
/* 0/45 words: retail builds both dot products with mula.s/madda.s/madd.s */
float AiNavigator::orientTo(_fvector &dir, float tol)
{
    float (*m)[4] = (float (*)[4])((char *)navCs(monster) + 0x20);
    float side = m[0][0] * dir.x + m[0][1] * dir.y + m[0][2] * dir.z;
    float fwd = m[1][0] * dir.x + m[1][1] * dir.y + m[1][2] * dir.z;

    if (fwd <= 0.0f)
        side = (side > 0.0f) ? 1.0f : -1.0f;
    if (tol < fabsf(side))
        turn += side;
    return side;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", orientTo__11AiNavigatorR8_fvectorf);
#endif
#ifdef NON_MATCHING
/* 2/45 words: retail uses mula.s dot product and branch-likely layout */
float AiNavigator::strafeTo(_fvector &pos, float width, float tol)
{
    _fvector d;
    float (*m)[4] = (float (*)[4])((char *)navCs(monster) + 0x20);

    vecSub(&d, (_fvector *)((char *)navCs(monster) + 0x10), &pos);
    float side = m[0][0] * d.x + m[0][1] * d.y + m[0][2] * d.z;

    if (side - width > tol)
        strafe -= 1.0f;
    else if (side + width < -tol)
        strafe += 1.0f;
    return side;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", strafeTo__11AiNavigatorR8_fvectorff);
#endif
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", followPath__11AiNavigatorRQ211AiNavigator8PathInfoT1R8_fvector);
typedef int u128 __attribute__((mode(TI)));
#ifdef NON_MATCHING
/* Advances along the planned path: the next path info becomes the current one, the monster's closest path follows, and the info after it is prepared. */
void AiNavigator::nextPathPoint(void)
{
    unsigned cur = f21C;

    if ((unsigned)f218 >= cur) {
        *(u128 *)&pathInfo = *(u128 *)&nextPathInfo;
        f21C = cur + 1;
        *(u128 *)((char *)&pathInfo + 0x10) = *(u128 *)((char *)&nextPathInfo + 0x10);
        *(AiPath **)((char *)monster + 0x1A10) = *(AiPath **)&pathInfo;
        cur = f21C;
        if (cur < (unsigned)f218)
            nextPathInfo.init(*(AiPath **)((char *)this + 0x1A0 + cur * 4), *(AiPath **)&pathInfo, false);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", nextPathPoint__11AiNavigator);
#endif
AiPath *AiNavigator::getFleePath(_fvector &p)
{
    return (AiPath *)((char *)s_net + 0x3850);
}
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", inPathTransition__11AiNavigatorRQ211AiNavigator8PathInfoR8_fvectorb);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", getMaxJumpHeight__11AiNavigatorb);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", getMaxStandingJumpHeight__11AiNavigatorb);
AiNavigator::ObstacleSensor::ObstacleSensor(AiNavigator &n)
{
    nav = &n;
    param120 = 35.0f;
    param124 = 0.32f;
    param128 = 45.0f;
    param12C = 100.0f;
}
void AiNavigator::ObstacleSensor::init(void)
{
    obstacleHit = 0;
    obstacle2Hit = 0;
    floorHit = 0;
    ceilHit = 0;
}
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", test__Q211AiNavigator14ObstacleSensor);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", getNextCollis__Q211AiNavigator14ObstacleSensorP9_hdResultiT1);
_hdResult *AiNavigator::ObstacleSensor::getObstacle(void)
{
    if (obstacleHit)
        return (_hdResult *)((char *)this + 0x10);
    return 0;
}
float AiNavigator::ObstacleSensor::getObstacleRange(void)
{
    return obstacleRange;
}
float AiNavigator::ObstacleSensor::getObstacleHeight(void)
{
    register float b __asm__("$f1") = obstacle2Height;
    register float a __asm__("$f2") = obstacleHeight;
    register float r __asm__("$f0");

    __asm__("max.s %0, %1, %2" : "=f"(r) : "f"(b), "f"(a));
    return r;
}
float AiNavigator::ObstacleSensor::getCeilHeight(void)
{
    if (ceilHit)
        return ceilHeight;
    return 99999.0f;
}
float AiNavigator::ObstacleSensor::getFloorHeight(void)
{
    if (floorHit)
        return floorHeight;
    return -99999.0f;
}
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", shouldAvoid__Q211AiNavigator14ObstacleSensorR8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", isObstacleClimbable__Q211AiNavigator14ObstacleSensor);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", isObstacleClimbable__Q211AiNavigator14ObstacleSensorR9_hdResult);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", init__Q211AiNavigator8PathInfoP6AiPathi);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", init__Q211AiNavigator8PathInfoP6AiPathT1b);
INCLUDE_ASM("asm/nonmatchings/game/AiNavigator", init__Q211AiNavigator8PathInfoP6AiPathP8_fvector);
