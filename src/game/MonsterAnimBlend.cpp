#include "common.h"
#include "game/anim_blend.h"
#include "task_manager.h"
#include "vecmath.h"

/* only the cs pointer is read here */
class DbInteractive {
public:
    char pad0[0xC];
    _cs *m_cs; /* 0xC */
};

int timerGetFieldCount(void);
float *animationGetChannelVal(int id, _animHandle &h);
float animationGetTotalRunFrames(_animHandle h);
AnimBlendNode *animationAddBlend(_animHandle h, AnimBlendNode *b, int type, float weight, float blendTime, short priority,
                                 int reset);
void animationRemoveBlend(_animHandle h, AnimBlendNode *b, float);
void mathfRotMatrixPRH(float (*m)[4], _fvector *a);

void AnimPappy::init(_animHandle &h)
{
    int i;

    vecStoreUnitW(&m_offset);
    m_ch[0] = animationGetChannelVal(1, h);
    m_ch[1] = animationGetChannelVal(2, h);
    m_ch[2] = animationGetChannelVal(3, h);
    m_ch[3] = animationGetChannelVal(4, h);
    m_ch[4] = animationGetChannelVal(5, h);
    m_ch[5] = animationGetChannelVal(6, h);
    m_ch[6] = animationGetChannelVal(7, h);
    for (i = 0; i < 7; i++) {
        if (m_ch[i] == 0) {
            m_ch[0] = 0;
            break;
        }
    }
}
/* cs->rot = -(rx, ry, rz); cs->trans = (tx, ty, tz) + m_offset - mat * m_offset, so the cs turns around the pivot */
#ifdef NON_MATCHING
/* 59/71 palavras; só alocação de registradores (v0/v1) nas gravações de rot e trans */
void AnimPappy::update(DbInteractive &d)
{
    _fvector t;
    _fvector *v;
    _cs *cs;
    float x, y, z;

    if (m_ch[0] == 0 || !(*m_ch[6] > 1e-10f))
        return;
    x = -*m_ch[0];
    y = -*m_ch[1];
    z = -*m_ch[2];
    v = &d.m_cs->rot;
    v->x = x;
    v->y = y;
    v->z = z;
    mathfRotMatrixPRH(d.m_cs->mat, &d.m_cs->rot);
    x = *m_ch[3];
    y = *m_ch[4];
    z = *m_ch[5];
    v = &d.m_cs->trans;
    v->x = x;
    v->y = y;
    v->z = z;
    vecAdd(&d.m_cs->trans, &d.m_cs->trans, &m_offset);
    cs = d.m_cs;
    vecApplyRows(&t, cs->mat, &m_offset);
    vecSub(&cs->trans, &cs->trans, &t);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterAnimBlend", update__9AnimPappyR13DbInteractive);
#endif
AnimBlend::AnimBlend(AnimBlendTypes type, int priority)
{
    m_node.blendType = type;
    m_node.priority = priority;
    m_weight = 1.0f;
    m_task = 0;
    m_node.prev = 0;
    m_node.next = 0;
    m_node.active = 0;
}
void AnimBlend::init(_animHandle &h)
{
    if (m_task)
        TaskManager::global.remove(m_task);
    if (m_node.active)
        animationRemoveBlend(*m_handle, &m_node, 0.0f);
    m_handle = &h;
}
/* adds the layer over the main tree, reaching m_weight in `ms` milliseconds (restarts the animation) */
void AnimBlend::rampIn(float ms)
{
    if (m_handle == 0 || m_handle->ci == 0)
        return;
    if (m_task)
        TaskManager::global.remove(m_task);
    animationAddBlend(*m_handle, &m_node, m_node.blendType, m_weight, ms, m_node.priority, 1);
}
/* fades the layer out over `ms` milliseconds (16 per field); ms <= 0 drops it at once */
void AnimBlend::rampOut(float ms)
{
    if (m_handle == 0 || m_handle->ci == 0 || !m_node.active)
        return;
    if (m_task)
        TaskManager::global.remove(m_task);
    if (ms > 0.0f) {
        m_rate = -m_node.weight * 16.0f / ms;
        m_task = TaskManager::global.add(updateRampOut, this, 1);
    } else {
        m_rate = -m_node.weight;
        updateRampOut();
    }
}
/* rampOut for a pooled blend: goes back to AnimBlendPool when the weight reaches zero (at once without an animation) */
#ifdef NON_MATCHING
/* 13/76 palavras; AnimBlendPool::dealloc inlinado: o retail divide o índice com srl (sem sinal) e guarda m_top em outro registrador */
void AnimBlend::rampOutAndDealloc(float ms)
{
    if (m_handle == 0 || m_handle->ci == 0) {
        AnimBlendPool::dealloc(this);
        return;
    }
    if (m_task)
        TaskManager::global.remove(m_task);
    if (ms > 0.0f)
        m_rate = -m_node.weight * 16.0f / ms;
    else
        m_rate = -m_node.weight;
    m_task = TaskManager::global.add(updateRampOutAndDealloc, this, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterAnimBlend", rampOutAndDealloc__9AnimBlendf);
#endif
/* plays a pooled blend once: ramps in over inMs, and once the animation has run its frames ramps out over outMs
   and goes back to the pool */
#ifdef NON_MATCHING
/* 74/97 palavras; AnimBlendPool::dealloc inlinado: o retail divide o índice com srl (sem sinal) e guarda m_top em outro registrador */
void AnimBlend::runAndDealloc(float inMs, float outMs)
{
    if (m_handle == 0 || m_handle->ci == 0) {
        AnimBlendPool::dealloc(this);
        return;
    }
    if (m_task)
        TaskManager::global.remove(m_task);
    rampIn(inMs);
    if (outMs > 0.0f)
        m_rate = -m_node.weight * 16.0f / outMs;
    else
        m_rate = -m_node.weight;
    m_task = TaskManager::global.add(updateRampOutAndDealloc, this, (int)animationGetTotalRunFrames(*m_handle));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterAnimBlend", runAndDealloc__9AnimBlendff);
#endif
/* one field of ramp out; 0 (task done) once the weight is gone and the blend removed */
unsigned AnimBlend::updateRampOut(void)
{
    m_node.weight += m_rate;
    if (m_node.weight <= 0.0f) {
        m_rate = 0.0f;
        m_node.weight = 0.0f;
        animationRemoveBlend(*m_handle, &m_node, 0.0f);
        m_task = 0;
        return 0;
    }
    return 1;
}
#ifdef NON_MATCHING
/* 24/43 palavras; AnimBlendPool::dealloc inlinado: o retail divide o índice com srl (sem sinal) e guarda m_top em outro registrador */
unsigned AnimBlend::updateRampOutAndDealloc(void)
{
    if (updateRampOut())
        return 1;
    AnimBlendPool::dealloc(this);
    m_task = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/MonsterAnimBlend", updateRampOutAndDealloc__9AnimBlend);
#endif
/* jumps the animation (and this layer's copy of its timing) to p (0..1) of its frame range */
void AnimBlend::setPercent(float p)
{
    HierAnimation *anim;
    float t;
    int field;

    if (m_handle == 0 || m_handle->ci == 0)
        return;
    anim = m_handle->ci->character->animations[m_handle->animIdx];
    t = (anim->endTime - anim->startTime) * p + anim->startTime;
    field = timerGetFieldCount();
    m_handle->ctl->startField = m_node.toTime.startField = field;
    m_handle->ctl->startTime = m_node.toTime.startTime = t;
}
/* animation frames per field (0 freezes it) */
void AnimBlend::setSpeed(float s)
{
    if (m_handle == 0 || m_handle->ci == 0)
        return;
    m_handle->ctl->deltaTime = m_node.toTime.deltaTime = s;
}
float AnimBlend::getPercent(void)
{
    return animationGetCurrentPercent(*m_handle);
}
/* the layer's weight now: node.weight scaled by how far the ramp in is (16.67 ms per field) */
float AnimBlend::getWeight(void)
{
    float t = ((float)timerGetFieldCount() - m_node.blendStartField) * 16.66667f;

    if (t < m_node.blendTime)
        return m_node.weight * (t / m_node.blendTime);
    return m_node.weight;
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterAnimBlend", __static_initialization_and_destruction_0_00164670);
unsigned AnimBlend::updateRampOut(void *b)
{
    return ((AnimBlend *)b)->updateRampOut();
}
unsigned AnimBlend::updateRampOutAndDealloc(void *b)
{
    return ((AnimBlend *)b)->updateRampOutAndDealloc();
}
INCLUDE_ASM("asm/nonmatchings/game/MonsterAnimBlend", _GLOBAL_$I$_13AnimBlendPool$s_pool);
