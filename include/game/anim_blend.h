#ifndef GAME_ANIM_BLEND_H
#define GAME_ANIM_BLEND_H

#include "engine.h"
#include "hieri_types.h"

class DbInteractive;
void *operator new(unsigned, void *);

/* A monster's extra animation layer (game/MonsterAnimBlend): one _animBlendNode laid over the main tree of the
   animation `*m_handle` (common/animation, animationAddBlend). Ramping in adds the blend with weight m_weight;
   ramping out lowers node.weight by m_rate every field (a TaskManager task) and removes the blend at zero. */
class AnimBlend {
public:
    void *m_task;              /* 0x0: TaskManager::global task of the running ramp out (0 = none) */
    _animHandle *m_handle;     /* 0x4: the animation this layer plays (set by init) */
    _animBlendNode m_node;     /* 0x8: active 0xC, blendType 0xD, priority 0xE, toTime 0x38, weight 0x48, blendTime 0x4C */
    float m_rate;              /* 0x60: weight added per field while ramping out (negative) */
    float m_weight;            /* 0x64: weight given to animationAddBlend by rampIn (1.0) */

    AnimBlend(AnimBlendTypes type, int priority);
    void init(_animHandle &h);
    void rampIn(float ms);
    void rampOut(float ms);
    void rampOutAndDealloc(float ms);
    void runAndDealloc(float inMs, float outMs);
    unsigned updateRampOut(void);
    unsigned updateRampOutAndDealloc(void);
    void setPercent(float p);
    void setSpeed(float s);
    float getPercent(void);
    float getWeight(void);
    static unsigned updateRampOut(void *b);
    static unsigned updateRampOutAndDealloc(void *b);
};
typedef char _size_AnimBlend[sizeof(AnimBlend) == 0x68 ? 1 : -1];

/* 48 AnimBlends (static blends, priority 10) for temporary layers. m_free[m_top..47] are the free indexes:
   allocating takes m_free[m_top++], freeing stores at m_free[--m_top] (m_top == 0: everything is free). */
class AnimBlendPool {
public:
    AnimBlend m_blends[48];        /* 0x0 */
    unsigned char m_free[48];      /* 0x1380 */
    unsigned char m_top;           /* 0x13B0 */

    static AnimBlendPool s_pool;

    /* inlined by the retail wherever a temporary blend is taken: a fresh static blend of priority 10, or 0 when all 48 are in use */
    static AnimBlend *alloc(void)
    {
        AnimBlendPool *pool = &s_pool;
        unsigned char top = pool->m_top;
        AnimBlend *b;

        if (top < 48) {
            pool->m_top = top + 1;
            b = &pool->m_blends[pool->m_free[top]];
        } else {
            b = 0;
        }
        if (b != 0)
            new (b) AnimBlend(BLEND_STATIC, 10);
        return b;
    }

    /* inlined by the retail wherever a blend goes back to the pool; an index already free is left alone */
    static void dealloc(AnimBlend *b)
    {
        AnimBlendPool *pool = &s_pool;
        unsigned char top = pool->m_top;

        if (top != 0) {
            unsigned char idx = b - pool->m_blends;
            unsigned char *p = &pool->m_free[top];
            unsigned char n;
            for (n = 48 - top; n != 0; n--, p++) {
                if (*p == idx)
                    return;
            }
            pool->m_free[--pool->m_top] = idx;
        }
    }
};

/* Moves a monster's cs by the animation channels 1..7 (rotation x/y/z, translation x/y/z, on/off) of a cinema
   animation, pivoting around m_offset. Every channel must exist, otherwise m_ch[0] = 0 and update does nothing. */
class AnimPappy {
public:
    _fvector m_offset;         /* 0x0: pivot, in the cs frame (starts as (0, 0, 0, 1)) */
    float *m_ch[7];            /* 0x10: rx, ry, rz (negated into cs->rot), tx, ty, tz, on (> 1e-10) */
    char pad2C[0x30 - 0x2C];

    void init(_animHandle &h);
    void update(DbInteractive &d);
};
typedef char _size_AnimPappy[sizeof(AnimPappy) == 0x30 ? 1 : -1];

#endif
