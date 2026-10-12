#include "common.h"
#include "engine.h"
#include "memory_stack.h"

/* Animation playback and blending for character instances.
 *
 * Each character instance (_animCharInstance) has one control node per animation (animCtx[], 0x3C bytes: start field,
 * start time, speed in deltaTime, active/loop flags) and an AnimPlayer kept in ci->activeTree. The player owns the
 * main tree (a control node, or a blend node 0x22 whose blendFrom/blendTo are subtrees) plus a priority-sorted list
 * of extra blends (static, override, additive, procedural) applied on top every frame. Nodes write their channel values
 * into an AnimationOutputBlock (val[] per channel and a dirty bit per channel). Scratch output blocks come from a
 * 16 KB MemoryStack on the scratchpad, released after each blend node. Blend nodes for transitions come from a pool of
 * 128 with a free-index stack.
 *
 * Times are in fields (60 per second); a control node's current time is startTime + deltaTime * (fields since startField)
 * while active. Blend times are in milliseconds (16.666668 per field). */

extern "C" int printf(const char *, ...);
extern "C" void animationClearDirty(_animCharInstance *ci, _animtransform *xf);
int timerGetFieldCount(void);
int getNgpFilesLoaded(void);
void boundEulerAngle(float *p);
float mathfApproxCos2(float x);
float animCurveEvaluate(AnimCurveHeader *curve, float t, unsigned short *cache);
void *operator new(unsigned, void *);

struct AnimBlendPool {
    AnimBlendNode nodes[128];      // 0x0000
    unsigned char freeIdx[128];    // 0x2C00 free-index stack
    signed char used;              // 0x2C80 entries taken (negative = full)
};

extern MemoryStack animStack __asm__("D_007329F0");       // scratch output blocks (0x70000000, 16 KB)
extern AnimBlendPool blendPool __asm__("D_00732A08");
extern HierAnimMgr *animMgrs[14] __asm__("D_00735690");   // one per loaded NGP file
/* "Could not start a blend of type %d, ---Bad Character Instance(s)\n" (still in the asm rodata) */
extern const char badInstanceMsg[] __asm__("D_006F3150");
extern int s_blendCurve;  // 0 linear, 1 cosine ease, 2 ease in (s^2), 3 ease out (s(2 - s))
extern int okToBlend;

void animationGetHandle(_animHandle *h, unsigned id1, unsigned id2, unsigned animId);
float animationGetCurrentTime(_animHandle h);
void animationStart(_animHandle h, bool reset);
void animationStartReverse(_animHandle h, bool reset);
void animationSetToPercent(_animHandle h, float pct, int update);
float animationGetCurrentPercent(_animHandle h);
void animationPause(_animHandle h);
void animationResume(_animHandle h);
void animationSetToBeginning(_animHandle h, bool update);
void animationSetToEnd(_animHandle h, bool update);
void animationSetDirection(_animHandle h, bool forward);
int animationGetDirection(_animHandle h);
void animationSetIterations(_animHandle h, unsigned short n);
float animationUpdateHandle(_animHandle &h);
void animationUpdateFromControlNode(_animControlNode *ctl, _animCharInstance *ci);
void animationUpdateActiveTree(_hierhead *node, _animCharInstance *ci);
void animationProcessActiveTree(_hierhead *node, _animCharInstance *ci);
void animationProcessTransitionBlend(_animBlendNode *b, _animCharInstance *ci);
void animationProcessStaticBlend(_animBlendNode *b, _animCharInstance *ci);
void animationProcessOverrideBlend(_animBlendNode *b, _animCharInstance *ci);
void animationProcessAdditiveBlend(_animBlendNode *b, _animCharInstance *ci);
void animationProcessProceeduralBlend(_animBlendNode *b, _animCharInstance *ci);
void animationEvaluateFromNode(_animBlendNode *b, _animCharInstance *ci, _animationOutputBlock **out);
void animationEvaluateToNode(_animBlendNode *b, _animCharInstance *ci, _animationOutputBlock **out);
void animationCollapseBlend(_animBlendNode *b, _animCharInstance *ci);
void animationCleanUpTree(_hierhead *node, _animCharInstance *ci);
void animationAddToActiveTree(_animHandle h);
void animationUpdate(_animation *anim, _animCharInstance *ci);
void animationManager(_animmgr *mgr);
void animationInitialize(_animation *anim, _animCharInstance *ci, int idx);

/* AnimPlayer (hieri_types.h has the data only): its methods, by their retail symbols */
AnimPlayer *AnimPlayer_ctor(AnimPlayer *self, _animCharInstance *ci) __asm__("__10AnimPlayerP17_animCharInstance");
void AnimPlayer_UpdateAnimations(AnimPlayer *self) __asm__("UpdateAnimations__10AnimPlayer");
int AnimPlayer_AddBlendToActiveList(AnimPlayer *self, AnimBlendNode *b) __asm__("AddBlendToActiveList__10AnimPlayerP14_animBlendNode");
int AnimPlayer_RemoveBlendFromActiveList(AnimPlayer *self, AnimBlendNode *b) __asm__("RemoveBlendFromActiveList__10AnimPlayerP14_animBlendNode");
int AnimPlayer_IsBlendInActiveList(AnimPlayer *self, AnimBlendNode *b) __asm__("IsBlendInActiveList__10AnimPlayerP14_animBlendNode");
HierHead *AnimPlayer_GetMainTreeNode(AnimPlayer *self) __asm__("GetMainTreeNode__10AnimPlayer");
HierHead *AnimPlayer_GetActiveList(AnimPlayer *self) __asm__("GetActiveList__10AnimPlayer");
int AnimPlayer_GetNumNodesInActiveList(AnimPlayer *self) __asm__("GetNumNodesInActiveList__10AnimPlayer");


/* small accessors as macros: matched functions use them too, and helpers would be emitted out of line */
#define ciPlayer(ci) ((AnimPlayer *)(ci)->activeTree)
#define handleAnim(h) ((h).ci->character->animations[(h).animIdx])
#define dirtyBit(bits, i) (((bits)[(i) / 8] >> ((i) - (i) / 8 * 8)) & 1)
#define setDirty(bits, i) ((bits)[(i) / 8] |= 1 << ((i) - (i) / 8 * 8))
#define clearDirty(bits, i) ((bits)[(i) / 8] &= ~(1 << ((i) - (i) / 8 * 8)))
#define copyHead(dst, src) ((dst)->id1 = (src)->id1, (dst)->id2 = (src)->id2)
#define makeHandle(h, ci_, c) ((h).ci = (ci_), (h).proc = 0, (h).ctl = (c), (h).animIdx = (c)->anim->animIdx)

#ifdef NON_MATCHING
/* which channels hold angles (wrapped with boundEulerAngle when blending): inline for up to 16 channels */
static inline unsigned char *angularBits(HierAnimCharacter *chr)
{
    if (chr->numChannels < 17)
        return chr->angularChannelBits.bytes;
    return (unsigned char *)chr + chr->angularChannelBits.offset;
}
static inline AnimBlendNode *blendAlloc(void)
{
    if (blendPool.used >= 0) {
        unsigned char i = blendPool.used;
        blendPool.used = i + 1;
        return &blendPool.nodes[blendPool.freeIdx[i]];
    }
    return 0;
}
static inline void blendFree(AnimBlendNode *b)
{
    unsigned char used = blendPool.used;
    if (used) {
        unsigned char idx = b - blendPool.nodes;
        unsigned char *p = &blendPool.freeIdx[used];
        unsigned char n;
        for (n = 128 - used; n; n--, p++)
            if (*p == idx)
                return;
        blendPool.used = --used;
        blendPool.freeIdx[used] = idx;
    }
}
/* blend progress in [0, 1] shaped by s_blendCurve */
static inline float blendCurve(float s)
{
    switch (s_blendCurve) {
    case 1:
        return (mathfApproxCos2(s + -1.0f) + 1.0f) * 0.5f;
    case 2:
        return s * s;
    case 3:
        return s * (2.0f - s);
    }
    return s;
}
static inline float blendElapsedMs(AnimBlendNode *b)
{
    return ((float)timerGetFieldCount() - b->blendStartField) * 16.666668f;
}
/* weight of a layered blend: ramps up over blendTime, then stays at b->weight */
static inline float blendLayerWeight(AnimBlendNode *b)
{
    float e = blendElapsedMs(b);
    if (e < b->blendTime)
        return blendCurve(e / b->blendTime) * b->weight;
    return b->weight;
}
/* scratch output block sized for the character's channels (dirty bits cleared) */
static inline void allocOutput(AnimationOutputBlock *o, _animCharInstance *ci)
{
    unsigned char *d;
    int n;
    o->val = (float *)animStack.alloc16((unsigned short)(ci->character->numChannels * 4));
    n = (ci->character->numChannels >> 3) + 1;
    d = (unsigned char *)animStack.alloc16(n);
    for (; n; n--)
        d[n - 1] = 0;
    o->dirty = d;
    o->atMat = ci->animOutput.atMat;
}
#endif

INCLUDE_ASM("asm/nonmatchings/common/animation", D_006F3150);
/* buffer that keeps the main tree's pure output while modifier blends are layered on top */
static inline AnimPlayer *playerOf(_animCharInstance *ci)
{
    return (AnimPlayer *)ci->activeTree;
}
void animationInitModifierBlends(_animCharInstance *ci)
{
    playerOf(ci)->mainTreePureOut = (float *)MemoryStack::global.alloc16((unsigned short)(ci->character->numChannels * 4));
}
#ifdef NON_MATCHING
/* equivalent (untuned): 69/231 words, size 0x39c vs 0x39c */
/* Cross-fades from whatever the main tree plays into h over blendTime ms (type 1), or (type 7) into h started at
 * the same percent as the current animation. Without a main tree, or with the instance disabled, h just starts. */
void animationTransitionInto(_animHandle &h, float blendTime, int reset, int type)
{
    AnimControlNode *ctl;
    AnimPlayer *player;
    HierHead *cur;
    AnimBlendNode *b;

    if (!h.ci || !h.ctl)
        return;
    ctl = &h.ci->animCtx[h.animIdx];
    player = ciPlayer(h.ci);
    cur = AnimPlayer_GetMainTreeNode(player);
    if (type != BLEND_TRANSITION && type != BLEND_FREEZETRANS)
        return;
    if (!cur || h.ci->bits.disableCI) {
        animationStart(h, reset != 0);
        return;
    }
    if ((HierHead *)h.ctl == cur && ((AnimControlNode *)cur)->active)
        return;
    b = blendAlloc();
    if (b)
        new (b) AnimBlendNode;
    b->blendType = type;
    b->head.opcode = ANIMATION_BLEND_NODE;
    if (reset)
        animationSetToBeginning(h, false);
    if (type == BLEND_FREEZETRANS) {
        b->blendValue = animationGetCurrentPercent(h);
        animationSetToPercent(h, b->blendValue, 0);
    }
    animationResume(h);
    if (cur->opcode == ANIMATION_CONTROL_NODE) {
        AnimControlNode *c = (AnimControlNode *)cur;
        if (!c->active) {
            c->active = 1;
            c->startField = timerGetFieldCount() - c->startField;
        }
        b->fromTime.startField = c->startField;
        b->fromTime.startTime = c->startTime;
        b->fromTime.deltaTime = c->deltaTime;
    } else if (cur->opcode == ANIMATION_BLEND_NODE) {
        /* interrupting a transition: shorten it and keep its progress */
        AnimBlendNode *p = (AnimBlendNode *)cur;
        int t0, t1;
        p->blendTime *= 0.4f;
        t0 = timerGetFieldCount();
        t1 = timerGetFieldCount();
        p->parent = b;
        p->blendStartField = ((float)t0 - p->blendStartField) * -0.4f + (float)t1;
        b->fromTime.startField = p->toTime.startField;
        b->fromTime.startTime = p->toTime.startTime;
        b->fromTime.deltaTime = p->toTime.deltaTime;
    }
    b->blendFrom = cur;
    b->blendTo = (HierHead *)ctl;
    copyHead(&b->head, &ctl->head);
    b->blendTime = blendTime;
    b->weight = 0.0f;
    b->blendStartField = (float)timerGetFieldCount();
    b->parent = 0;
    b->active = 1;
    b->toTime.startField = ctl->startField;
    b->toTime.startTime = ctl->startTime;
    b->toTime.deltaTime = ctl->deltaTime;
    ctl->iterations = 0;
    player->mainTree = (HierHead *)b;
    okToBlend = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationTransitionInto__FR11_animHandlefii);
#endif
int animationIsTransitioning(_animHandle &h)
{
    HierHead *n;
    if (!h.ci)
        return 0;
    n = AnimPlayer_GetMainTreeNode(ciPlayer(h.ci));
    if (n && n->opcode == ANIMATION_BLEND_NODE)
        return 1;
    return 0;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 2/24 words, size 0x60 vs 0x58 */
/* is h the animation the main tree is heading to (following blendTo through transitions) */
int animationIsTargetAnim(_animHandle &h)
{
    HierHead *n;
    if (!h.ci)
        return 0;
    for (n = ciPlayer(h.ci)->mainTree; n;) {
        if (n->opcode == ANIMATION_CONTROL_NODE)
            return n == (HierHead *)h.ctl;
        if (n->opcode != ANIMATION_BLEND_NODE)
            return 0;
        n = ((AnimBlendNode *)n)->blendTo;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationIsTargetAnim__FR11_animHandle);
#endif
/* layers h over the main tree (static/override/additive), ordered by priority in the player's list */
AnimBlendNode *animationAddBlend(_animHandle h, AnimBlendNode *b, int type, float weight, float blendTime, short priority,
                                 int reset)
{
    AnimControlNode *ctl;
    AnimPlayer *player;

    if (!h.ci) {
        printf(badInstanceMsg, type);
        return 0;
    }
    ctl = h.ctl;
    if (!ctl)
        return 0;
    player = ciPlayer(h.ci);
    if (AnimPlayer_IsBlendInActiveList(player, b))
        AnimPlayer_RemoveBlendFromActiveList(player, b);
    if (reset)
        animationSetToBeginning(h, false);
    animationResume(h);
    b->blendType = type;
    b->blendFrom = 0;
    b->blendTo = (HierHead *)ctl;
    b->head.opcode = ANIMATION_BLEND_NODE;
    copyHead(&b->head, &ctl->head);
    b->weight = weight;
    b->blendTime = blendTime;
    b->blendStartField = (float)timerGetFieldCount();
    b->priority = priority;
    b->parent = 0;
    b->active = 1;
    b->next = 0;
    b->prev = 0;
    b->toTime.startField = ctl->startField;
    b->toTime.startTime = ctl->startTime;
    b->toTime.deltaTime = ctl->deltaTime;
    ctl->iterations = 0;
    AnimPlayer_AddBlendToActiveList(player, b);
    return b;
}
void animationRemoveBlend(_animHandle h, AnimBlendNode *b, float)
{
    AnimPlayer *player;
    if (!h.ci)
        return;
    player = ciPlayer(h.ci);
    if (player && AnimPlayer_IsBlendInActiveList(player, b))
        AnimPlayer_RemoveBlendFromActiveList(player, b);
    b->active = 0;
}
/* layers the procedural node h.proc over the main tree */
AnimBlendNode *animationAddProcBlend(_animHandle h, AnimBlendNode *b, int type, float weight, short priority)
{
    AnimProcNode *proc;
    AnimPlayer *player;

    if (!h.ci) {
        printf(badInstanceMsg, type);
        return 0;
    }
    proc = h.proc;
    if (!proc)
        return 0;
    player = ciPlayer(h.ci);
    if (AnimPlayer_IsBlendInActiveList(player, b))
        AnimPlayer_RemoveBlendFromActiveList(player, b);
    b->blendType = type;
    b->blendFrom = 0;
    b->blendTo = (HierHead *)proc;
    b->head.opcode = ANIMATION_BLEND_NODE;
    copyHead(&b->head, &proc->head);
    b->weight = weight;
    b->blendTime = 0.0f;
    b->blendStartField = (float)timerGetFieldCount();
    b->priority = priority;
    b->parent = 0;
    b->next = 0;
    b->prev = 0;
    b->toTime.startField = 0;
    b->toTime.startTime = 0.0f;
    b->toTime.deltaTime = 0.0f;
    b->active = 1;
    AnimPlayer_AddBlendToActiveList(player, b);
    return b;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 22/79 words, size 0x134 vs 0x13c */
/* Looks up animation animId of the character instance (id1, id2) in every loaded animation manager. */
void animationGetHandle(_animHandle *h, unsigned id1, unsigned id2, unsigned animId)
{
    int i;
    h->animIdx = -1;
    h->ci = 0;
    h->ctl = 0;
    for (i = 0; i < getNgpFilesLoaded(); i++) {
        HierAnimMgr *m = animMgrs[i];
        _animCharInstance **p;
        unsigned n;
        if (!m)
            return;
        for (n = m->numCharInstances, p = m->charInstance; n; n--, p++) {
            _animCharInstance *ci = *p;
            unsigned k;
            if (ci->head.id1 != id1 || ci->head.id2 != id2)
                continue;
            for (k = 0; k < ci->character->numAnims; k++) {
                if (ci->character->animations[k]->head.id1 == animId) {
                    h->ci = ci;
                    h->animIdx = k;
                    h->ctl = &ci->animCtx[k];
                    return;
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetHandle__FP11_animHandleUiUiUi);
#endif
/* handle to the character instance (id1, id2) without an animation */
void animationGetNullHandle(_animHandle *h, unsigned id1, unsigned id2)
{
    int i;
    h->animIdx = -1;
    h->ci = 0;
    h->ctl = 0;
    for (i = 0; i < getNgpFilesLoaded(); i++) {
        HierAnimMgr *m = animMgrs[i];
        _animCharInstance **p;
        unsigned n;
        if (!m)
            return;
        for (n = m->numCharInstances, p = m->charInstance; n; n--, p++) {
            if ((*p)->head.id1 == id1 && (*p)->head.id2 == id2) {
                h->ci = *p;
                return;
            }
        }
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 3/43 words, size 0xac vs 0xa8 */
float animationGetCurrentTime(_animHandle h)
{
    AnimControlNode *c;
    float fields;
    if (!h.ci || !(c = h.ctl))
        return 0.0f;
    if (c->active)
        fields = (float)(timerGetFieldCount() - (int)c->startField);
    else
        fields = (float)(int)c->startField;  /* paused: startField holds the fields already played */
    return c->deltaTime * fields + h.ctl->startTime;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetCurrentTime__FG11_animHandle);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/35 words, size 0x8c vs 0x6c */
/* fields one pass of the animation takes at the current speed (0 when stopped) */
float animationCalculatePlayTime(_animHandle h)
{
    if (h.ci && h.ctl) {
        float d = h.ctl->deltaTime;
        if (d != 0.0f)
            return (h.ctl->anim->endTime - h.ctl->anim->startTime) / (d < 0.0f ? -d : d);
    }
    return 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationCalculatePlayTime__FG11_animHandle);
#endif
/* drops the main tree and every layered blend */
void animationResetCharacter(_animHandle &h)
{
    _animCharInstance *ci = h.ci;
    AnimPlayer *player;
    HierHead *main, *n;
    if (!ci)
        return;
    player = ciPlayer(ci);
    main = AnimPlayer_GetMainTreeNode(player);
    n = AnimPlayer_GetActiveList(player);
    if (main) {
        animationCleanUpTree(main, ci);
        player->mainTree = 0;
    }
    if (n) {
        do
            AnimPlayer_RemoveBlendFromActiveList(player, (AnimBlendNode *)n);
        while ((n = AnimPlayer_GetActiveList(player)) != 0);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 30/53 words, size 0xd4 vs 0xcc */
/* plays h forwards as the main tree, from the start when `reset` or when it is before its first frame */
void animationStart(_animHandle h, bool reset)
{
    HierAnimation *a;
    if (!h.ci)
        return;
    a = handleAnim(h);
    animationAddToActiveTree(h);
    animationResume(h);
    animationSetDirection(h, true);
    if (reset || animationGetCurrentTime(h) < a->startTime)
        animationSetToBeginning(h, false);
    animationSetIterations(h, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationStart__FG11_animHandleb);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 30/53 words, size 0xd4 vs 0xcc */
void animationStartReverse(_animHandle h, bool reset)
{
    HierAnimation *a;
    if (!h.ci)
        return;
    a = handleAnim(h);
    animationAddToActiveTree(h);
    animationResume(h);
    animationSetDirection(h, false);
    if (reset || a->endTime < animationGetCurrentTime(h))
        animationSetToEnd(h, false);
    animationSetIterations(h, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationStartReverse__FG11_animHandleb);
#endif
/* jumps to `frame` (clamped to the animation) and pauses there; `update` evaluates the pose right away */
void animationSetToFrame(_animHandle h, float frame, int update)
{
    HierAnimation *a;
    if (!h.ci || !h.ctl)
        return;
    a = handleAnim(h);
    animationResume(h);
    h.ctl->startField = timerGetFieldCount();
    if (a->endTime < frame)
        h.ctl->startTime = a->endTime;
    else if (frame < a->startTime)
        h.ctl->startTime = a->startTime;
    else
        h.ctl->startTime = frame;
    if (update)
        animationUpdate(a, h.ci);
    animationPause(h);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 1/21 words, size 0x54 vs 0x50 */
float animationGetCurrentFrame(_animHandle h)
{
    if (!h.ci)
        return -1.0f;
    return animationGetCurrentTime(h);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetCurrentFrame__FG11_animHandle);
#endif
void animationSetToPercent(_animHandle h, float pct, int update)
{
    HierAnimation *a;
    AnimControlNode *c;
    float t;
    if (!h.ci || !(c = h.ctl))
        return;
    a = handleAnim(h);
    t = (a->endTime - a->startTime) * pct + a->startTime;
    c->startField = c->active ? timerGetFieldCount() : 0;
    h.ctl->startTime = t;
    if (update) {
        if (!h.ctl->active)
            animationResume(h);
        animationUpdate(a, h.ci);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 1/36 words, size 0x90 vs 0x8c */
float animationGetCurrentPercent(_animHandle h)
{
    HierAnimation *a;
    float len;
    if (!h.ci)
        return -1.0f;
    a = handleAnim(h);
    len = a->endTime - a->startTime;
    return (animationGetCurrentTime(h) - a->startTime) / len;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetCurrentPercent__FG11_animHandle);
#endif
/* paused nodes keep the fields already played in startField */
void animationPause(_animHandle h)
{
    if (h.ci && h.ctl && h.ctl->active) {
        h.ctl->startField = timerGetFieldCount() - h.ctl->startField;
        h.ctl->active = 0;
    }
}
void animationResume(_animHandle h)
{
    if (!h.ci || !h.ctl)
        return;
    if (!h.ctl->active) {
        h.ctl->startField = timerGetFieldCount() - h.ctl->startField;
        h.ctl->active = 1;
    }
    /* no player on the instance: start it (retail tests activeTree itself, not the player's main tree) */
    if (!h.ci->activeTree) {
        if (animationGetDirection(h))
            animationStart(h, false);
        else
            animationStartReverse(h, false);
    }
}
void animationLoop(_animHandle h, bool loop)
{
    if (h.ci && h.ctl)
        h.ctl->loop = loop;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 2/58 words, size 0xe0 vs 0xe8 */
/* sets |speed| keeping the current direction (or the sign of `speed` when stopped); time restarts from now */
void animationSetSpeed(_animHandle h, float speed)
{
    float mag = speed < 0.0f ? -speed : speed;
    float t;
    int now;
    if (!h.ci || !h.ctl)
        return;
    t = animationGetCurrentTime(h);
    now = timerGetFieldCount();
    h.ctl->startTime = t;
    h.ctl->startField = h.ctl->active ? now : 0;
    if (h.ctl->deltaTime == 0.0f)
        h.ctl->deltaTime = speed;
    else
        h.ctl->deltaTime = h.ctl->deltaTime > 0.0f ? mag : -mag;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationSetSpeed__FG11_animHandlef);
#endif
void animationResetSpeed(_animHandle h)
{
    if (h.ci)
        animationSetSpeed(h, 1.0f);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/48 words, size 0xc0 vs 0xb4 */
void animationSetToBeginning(_animHandle h, bool update)
{
    HierAnimation *a;
    if (!h.ci || !h.ctl)
        return;
    a = handleAnim(h);
    if (update) {
        animationSetToFrame(h, a->startTime, 1);
        return;
    }
    h.ctl->startField = h.ctl->active ? timerGetFieldCount() : 0;
    h.ctl->startTime = a->startTime;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationSetToBeginning__FG11_animHandleb);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/48 words, size 0xc0 vs 0xb4 */
void animationSetToEnd(_animHandle h, bool update)
{
    HierAnimation *a;
    if (!h.ci || !h.ctl)
        return;
    a = handleAnim(h);
    if (update) {
        animationSetToFrame(h, a->endTime, 1);
        return;
    }
    h.ctl->startField = h.ctl->active ? timerGetFieldCount() : 0;
    h.ctl->startTime = a->endTime;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationSetToEnd__FG11_animHandleb);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 21/74 words, size 0x128 vs 0x118 */
/* flips the sign of deltaTime if needed, rebasing the start time so the current frame does not jump */
void animationSetDirection(_animHandle h, bool forward)
{
    int now;
    unsigned fields;
    float d;
    if (!h.ci || !h.ctl)
        return;
    now = timerGetFieldCount();
    fields = now - h.ctl->startField;
    d = h.ctl->deltaTime;
    if (forward ? d < 0.0f : d > 0.0f) {
        if (h.ctl->active) {
            h.ctl->startTime = (float)fields * d + h.ctl->startTime;
            h.ctl->startField = now;
        }
        h.ctl->deltaTime = -h.ctl->deltaTime;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationSetDirection__FG11_animHandleb);
#endif
/* 1 forwards (or invalid handle), 0 backwards */
int animationGetDirection(_animHandle h)
{
    if (h.ci && h.ctl && h.ctl->deltaTime < 0.0f)
        return 0;
    return 1;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 11/23 words, size 0x58 vs 0x5c */
float animationGetLastFrame(_animHandle h)
{
    if (!h.ci)
        return 0.0f;
    return handleAnim(h)->endTime;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetLastFrame__FG11_animHandle);
#endif
int animationIsRunning(_animHandle h)
{
    if (h.ci && h.ctl && h.ctl->active)
        return 1;
    return 0;
}
/* speed so one pass takes `frames` fields */
void animationSetTotalRunFrames(_animHandle h, float frames)
{
    HierAnimation *a;
    if (!h.ci || !h.ctl)
        return;
    a = handleAnim(h);
    h.ctl->deltaTime = (a->endTime - a->startTime) / frames;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 1/29 words, size 0x74 vs 0x6c */
float animationGetTotalRunFrames(_animHandle h)
{
    HierAnimation *a;
    if (!h.ci || !h.ctl)
        return 0.0f;
    a = handleAnim(h);
    return (a->endTime - a->startTime) / h.ctl->deltaTime;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetTotalRunFrames__FG11_animHandle);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/26 words, size 0x68 vs 0x64 */
/* length times pct (relative to the start, not an absolute frame) */
float animationGetFrameAtPercent(_animHandle h, float pct)
{
    HierAnimation *a;
    if (!h.ci)
        return -1.0f;
    a = handleAnim(h);
    return (a->endTime - a->startTime) * pct;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetFrameAtPercent__FG11_animHandlef);
#endif
void animationSetIterations(_animHandle h, unsigned short n)
{
    if (h.ci && h.ctl)
        h.ctl->iterations = n;
}
unsigned short animationGetIterations(_animHandle h)
{
    if (h.ci && h.ctl)
        return h.ctl->iterations;
    return 0;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 12/103 words, size 0x184 vs 0x19c */
/* marks the transform's rotate/translate as dirty when any of its channels changed */
void animationCheckDirty(_animCharInstance *ci, _animtransform *xf)
{
    unsigned char *d = ci->animOutput.dirty;
    int r = 0, t = 0;
    if (xf->rXidx >= 0)
        r = dirtyBit(d, xf->rXidx);
    if (xf->rYidx >= 0)
        r |= dirtyBit(d, xf->rYidx);
    if (xf->rZidx >= 0)
        r |= dirtyBit(d, xf->rZidx);
    if (r)
        xf->dirty.rotate = 1;
    if (xf->tXidx >= 0)
        t = dirtyBit(d, xf->tXidx);
    if (xf->tYidx >= 0)
        t |= dirtyBit(d, xf->tYidx);
    if (xf->tZidx >= 0)
        t |= dirtyBit(d, xf->tZidx);
    if (t)
        xf->dirty.translate = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationCheckDirty__FP17_animCharInstanceP14_animtransform);
#endif
extern "C" void animationClearDirty(_animCharInstance *ci, _animtransform *xf)
{
    unsigned char *d;
    if (xf->rXidx >= 0)
        clearDirty(ci->animOutput.dirty, xf->rXidx);
    if (xf->rYidx >= 0)
        clearDirty(ci->animOutput.dirty, xf->rYidx);
    if (xf->rZidx >= 0)
        clearDirty(ci->animOutput.dirty, xf->rZidx);
    if (xf->tXidx >= 0)
        clearDirty(ci->animOutput.dirty, xf->tXidx);
    if (xf->tYidx >= 0)
        clearDirty(ci->animOutput.dirty, xf->tYidx);
    if (xf->tZidx >= 0)
        clearDirty(ci->animOutput.dirty, xf->tZidx);
    (void)d;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/30 words, size 0x78 vs 0x74 */
/* binary search of channel id in the sorted map; -1 if absent (note: starts with hi = nIds, one past the end) */
int animationGetChannelIdx(int id, AnimChannelIdMap *map)
{
    int lo, hi, mid;
    if (!map)
        return -1;
    hi = map->nIds;
    lo = 0;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        if ((int)map->entries[mid].id < id)
            lo = mid + 1;
        else if (id < (int)map->entries[mid].id)
            hi = mid - 1;
        else
            return map->entries[mid].chanIdx;
    }
    return -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetChannelIdx__FiP16AnimChannelIdMap);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 13/19 words, size 0x4c vs 0x4c */
/* the output value of channel id, for code that reads or overrides it */
float *animationGetChannelVal(int id, _animHandle &h)
{
    _animCharInstance *ci = h.ci;
    int idx;
    if (!ci)
        return 0;
    idx = animationGetChannelIdx(id, ci->character->animChannelIdMap);
    if (idx >= 0)
        return &ci->animOutput.val[idx];
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationGetChannelVal__FiR11_animHandle);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 50/158 words, size 0x264 vs 0x278 */
/* Current time of h with the end handled: past the end it loops (counting iterations) or stops at the last frame
 * (pausing if it is the main tree); the same backwards. */
float animationUpdateHandle(_animHandle &h)
{
    float t = animationGetCurrentTime(h);
    AnimControlNode *c = h.ctl;
    HierAnimation *a = c->anim;
    if (a->endTime < t && 0.0f < c->deltaTime) {
        if (!c->loop) {
            if (ciPlayer(h.ci)->mainTree == (HierHead *)c)
                animationPause(h);
            animationSetToEnd(h, false);
        } else {
            c->iterations++;
            animationSetToBeginning(h, false);
        }
        t = animationGetCurrentTime(h);
    } else if (t < a->startTime && c->deltaTime < 0.0f) {
        if (!c->loop) {
            if (ciPlayer(h.ci)->mainTree == (HierHead *)c)
                animationPause(h);
            animationSetToBeginning(h, false);
        } else {
            c->iterations++;
            animationSetToEnd(h, false);
        }
        t = animationGetCurrentTime(h);
    }
    return t;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationUpdateHandle__FR11_animHandle);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/64 words, size 0xe8 vs 0x100 */
/* evaluates every curve of the node's animation into its output block */
void animationUpdateFromControlNode(_animControlNode *ctl, _animCharInstance *ci)
{
    _animHandle h;
    HierAnimation *a = ctl->anim;
    AnimCurveHeader **ch = a->channels;
    unsigned short *key;
    unsigned n;
    float t;

    h.ci = ci;
    h.proc = 0;
    h.ctl = ctl;
    h.animIdx = a->animIdx;
    t = animationUpdateHandle(h);
    key = ctl->prevKey;
    for (n = a->numChannels; n; n--, ch++) {
        float v = animCurveEvaluate(*ch, t, key);
        key++;
        ctl->animOutput.val[(*ch)->dataIdx] = v;
        setDirty(ctl->animOutput.dirty, (*ch)->dataIdx);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationUpdateFromControlNode__FP16_animControlNodeP17_animCharInstance);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/39 words, size 0x98 vs 0x9c */
/* a tree root writes straight into the instance's output */
void animationUpdateActiveTree(_hierhead *node, _animCharInstance *ci)
{
    switch (node->opcode) {
    case ANIMATION_CONTROL_NODE:
    case ANIMATION_BLEND_NODE:
        ((AnimBlendNode *)node)->animOutput = ci->animOutput;
        animationProcessActiveTree(node, ci);
        break;
    case ANIMATION_PROC_NODE:
        ((AnimProcNode *)node)->animOutput = ci->animOutput;
        animationProcessActiveTree(node, ci);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationUpdateActiveTree__FP9_hierheadP17_animCharInstance);
#endif
void animationProcessActiveTree(_hierhead *node, _animCharInstance *ci)
{
    switch (node->opcode) {
    case ANIMATION_CONTROL_NODE:
        animationUpdateFromControlNode((AnimControlNode *)node, ci);
        break;
    case ANIMATION_PROC_NODE:
        ((void (*)(_hierhead *, _animCharInstance *))((AnimProcNode *)node)->procCallback)(node, ci);
        break;
    case ANIMATION_BLEND_NODE: {
        AnimBlendNode *b = (AnimBlendNode *)node;
        animStack.pushMark();
        switch (b->blendType) {
        case BLEND_TRANSITION:
        case BLEND_FREEZETRANS:
            animationProcessTransitionBlend(b, ci);
            break;
        case BLEND_STATIC:
            animationProcessStaticBlend(b, ci);
            break;
        case BLEND_OVERRIDE:
            animationProcessOverrideBlend(b, ci);
            break;
        case BLEND_ADDITIVE:
            animationProcessAdditiveBlend(b, ci);
            break;
        case BLEND_PROCEEDURAL:
            animationProcessProceeduralBlend(b, ci);
            break;
        default:
            b->weight = 0.0f;
            break;
        }
        animStack.popMark();
        break;
    }
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 26/307 words, size 0x4cc vs 0x4b0 */
/* Cross-fade: while running, mixes from/to per channel by the curve-shaped progress (angles wrapped); when done,
 * collapses into blendTo and frees the node. */
void animationProcessTransitionBlend(_animBlendNode *b, _animCharInstance *ci)
{
    AnimationOutputBlock *from = 0;
    AnimationOutputBlock *to = 0;
    float e = blendElapsedMs(b);

    if (e < b->blendTime) {
        HierAnimCharacter *chr;
        unsigned char *ang;
        int i;

        animationEvaluateFromNode(b, ci, &from);
        animationEvaluateToNode(b, ci, &to);
        b->weight = blendCurve(e / b->blendTime);
        chr = ci->character;
        ang = angularBits(chr);
        for (i = 0; i < chr->numChannels; i++) {
            int fd = from ? dirtyBit(from->dirty, i) : 0;
            int td = to ? dirtyBit(to->dirty, i) : 0;
            if (fd && td) {
                float a = from->val[i];
                float c = to->val[i];
                float d;
                if (dirtyBit(ang, i)) {
                    boundEulerAngle(&a);
                    boundEulerAngle(&c);
                }
                d = c - a;
                if (dirtyBit(ang, i))
                    boundEulerAngle(&d);
                b->animOutput.val[i] = a + d * b->weight;
                setDirty(b->animOutput.dirty, i);
            } else if (fd) {
                if (dirtyBit(ang, i))
                    boundEulerAngle(&from->val[i]);
                b->animOutput.val[i] = from->val[i];
                setDirty(b->animOutput.dirty, i);
            } else if (td) {
                if (dirtyBit(ang, i))
                    boundEulerAngle(&to->val[i]);
                b->animOutput.val[i] = to->val[i];
                setDirty(b->animOutput.dirty, i);
            } else {
                clearDirty(b->animOutput.dirty, i);
            }
        }
    } else {
        animationCollapseBlend(b, ci);
        to = &b->animOutput;
        animationEvaluateToNode(b, ci, &to);
        if (b->blendType == BLEND_FREEZETRANS) {
            AnimControlNode *c = (AnimControlNode *)b->blendTo;
            c->deltaTime = b->toTime.deltaTime;
            c->startField = timerGetFieldCount();
            c->active = 1;
        }
        blendFree(b);
        okToBlend = 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationProcessTransitionBlend__FP14_animBlendNodeP17_animCharInstance);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 15/192 words, size 0x300 vs 0x2e4 */
/* Static layer: moves the channels blendTo writes towards its values by the layer weight (angles wrapped).
 * The dirty bits are scanned 64 at a time. */
void animationProcessStaticBlend(_animBlendNode *b, _animCharInstance *ci)
{
    AnimationOutputBlock *from = 0;
    AnimationOutputBlock *to = 0;
    float w = blendLayerWeight(b);
    unsigned char *ang;
    unsigned char *mask;
    int i, words, rem;

    if (w < 1.0e-10f)
        return;
    animationEvaluateFromNode(b, ci, &from);
    animationEvaluateToNode(b, ci, &to);
    ang = angularBits(ci->character);
    words = (ci->character->numChannels >> 6) + 1;
    rem = ci->character->numChannels & 0x3F;
    mask = to->dirty;
    i = 0;
    for (; words; words--) {
        unsigned long bits;
        int n;
        __builtin_memcpy(&bits, mask, 8);
        mask += 8;
        for (n = words > 1 ? 64 : rem; n; n--, i++, bits >>= 1) {
            if ((int)(bits & 1)) {
                float base = b->animOutput.val[i];
                float t = to->val[i];
                if (dirtyBit(ang, i))
                    boundEulerAngle(&t);
                t = t - base;
                if (dirtyBit(ang, i))
                    boundEulerAngle(&t);
                b->animOutput.val[i] = base + t * w;
                setDirty(b->animOutput.dirty, i);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationProcessStaticBlend__FP14_animBlendNodeP17_animCharInstance);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 9/141 words, size 0x234 vs 0x218 */
/* Override layer: the channels blendTo writes become its values times the layer weight. */
void animationProcessOverrideBlend(_animBlendNode *b, _animCharInstance *ci)
{
    AnimationOutputBlock *from = 0;
    AnimationOutputBlock *to = 0;
    float w = blendLayerWeight(b);
    unsigned char *mask;
    int i, words, rem;

    if (w < 1.0e-10f)
        return;
    animationEvaluateFromNode(b, ci, &from);
    animationEvaluateToNode(b, ci, &to);
    words = (ci->character->numChannels >> 6) + 1;
    rem = ci->character->numChannels & 0x3F;
    mask = to->dirty;
    i = 0;
    for (; words; words--) {
        unsigned long bits;
        int n;
        __builtin_memcpy(&bits, mask, 8);
        mask += 8;
        for (n = words > 1 ? 64 : rem; n; n--, i++, bits >>= 1) {
            if ((int)(bits & 1)) {
                b->animOutput.val[i] = to->val[i] * w;
                setDirty(b->animOutput.dirty, i);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationProcessOverrideBlend__FP14_animBlendNodeP17_animCharInstance);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 24/148 words, size 0x250 vs 0x248 */
/* Additive layer: evaluates blendTo's curves directly and adds them, times the layer weight, to the output. */
void animationProcessAdditiveBlend(_animBlendNode *b, _animCharInstance *ci)
{
    AnimControlNode *ctl = (AnimControlNode *)b->blendTo;
    HierAnimation *a;
    _animHandle h;
    AnimCurveHeader **ch;
    unsigned short *key;
    unsigned char *ang;
    unsigned n;
    float w, t;

    if (ctl->head.opcode != ANIMATION_CONTROL_NODE)
        return;
    a = ctl->anim;
    w = blendLayerWeight(b);
    makeHandle(h, ci, ctl);
    t = animationUpdateHandle(h);
    ang = angularBits(ci->character);
    ch = a->channels;
    key = ctl->prevKey;
    for (n = a->numChannels; n; n--, ch++, key++) {
        int idx = (*ch)->dataIdx;
        float v = animCurveEvaluate(*ch, t, key);
        b->animOutput.val[idx] = b->animOutput.val[idx] + w * v;
        if (dirtyBit(ang, idx))
            boundEulerAngle(&b->animOutput.val[idx]);
        setDirty(b->animOutput.dirty, idx);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationProcessAdditiveBlend__FP14_animBlendNodeP17_animCharInstance);
#endif
void animationProcessProceeduralBlend(_animBlendNode *, _animCharInstance *)
{
}
#ifdef NON_MATCHING
/* equivalent (untuned): 92/176 words, size 0x2c0 vs 0x2b8 */
/* Evaluates blendFrom into a fresh scratch block (*out gets it). A control node runs with the time state saved in
 * b->fromTime, which is updated back afterwards. */
void animationEvaluateFromNode(_animBlendNode *b, _animCharInstance *ci, _animationOutputBlock **out)
{
    HierHead *from = b->blendFrom;
    _animHandle h;

    h.proc = 0;
    if (!from)
        return;
    if (from->opcode == ANIMATION_CONTROL_NODE) {
        AnimControlNode *c = (AnimControlNode *)from;
        allocOutput(&c->animOutput, ci);
        h.ci = ci;
        h.ctl = c;
        h.animIdx = c->anim->animIdx;
        animationResume(h);
        c->startField = b->fromTime.startField;
        c->startTime = b->fromTime.startTime;
        c->deltaTime = b->fromTime.deltaTime;
        animationProcessActiveTree(b->blendFrom, ci);
        c->deltaTime = b->fromTime.deltaTime;
        b->fromTime.startField = c->startField;
        b->fromTime.startTime = c->startTime;
        *out = &c->animOutput;
    } else if (from->opcode == ANIMATION_PROC_NODE) {
        AnimProcNode *p = (AnimProcNode *)from;
        allocOutput(&p->animOutput, ci);
        animationProcessActiveTree(b->blendFrom, ci);
        *out = &p->animOutput;
    } else if (from->opcode == ANIMATION_BLEND_NODE) {
        AnimBlendNode *n = (AnimBlendNode *)from;
        allocOutput(&n->animOutput, ci);
        animationProcessActiveTree(b->blendFrom, ci);
        b->fromTime = n->toTime;
        *out = &n->animOutput;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationEvaluateFromNode__FP14_animBlendNodeP17_animCharInstancePP21_animationOutputBlock);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 27/212 words, size 0x350 vs 0x334 */
/* Evaluates blendTo into *out when given, else into a fresh scratch block; *out gets the block. A control node runs
 * with the time state in b->toTime (frozen, speed 0, for the percent-synced transition). */
void animationEvaluateToNode(_animBlendNode *b, _animCharInstance *ci, _animationOutputBlock **out)
{
    HierHead *to = b->blendTo;
    _animHandle h;

    h.proc = 0;
    if (!to)
        return;
    if (to->opcode == ANIMATION_CONTROL_NODE) {
        AnimControlNode *c = (AnimControlNode *)to;
        if (*out)
            c->animOutput = **out;
        else
            allocOutput(&c->animOutput, ci);
        h.ci = ci;
        h.ctl = c;
        h.animIdx = c->anim->animIdx;
        animationResume(h);
        c->startField = b->toTime.startField;
        c->startTime = b->toTime.startTime;
        c->deltaTime = b->blendType == BLEND_FREEZETRANS ? 0.0f : b->toTime.deltaTime;
        animationProcessActiveTree(b->blendTo, ci);
        b->toTime.startField = c->startField;
        b->toTime.startTime = c->startTime;
        *out = &c->animOutput;
    } else if (to->opcode == ANIMATION_PROC_NODE) {
        AnimProcNode *p = (AnimProcNode *)to;
        if (*out)
            p->animOutput = **out;
        else
            allocOutput(&p->animOutput, ci);
        animationProcessActiveTree(b->blendTo, ci);
        *out = &p->animOutput;
    } else if (to->opcode == ANIMATION_BLEND_NODE) {
        AnimBlendNode *n = (AnimBlendNode *)to;
        if (*out)
            n->animOutput = **out;
        else
            allocOutput(&n->animOutput, ci);
        animationProcessActiveTree(b->blendTo, ci);
        b->toTime = n->toTime;
        *out = &n->animOutput;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationEvaluateToNode__FP14_animBlendNodeP17_animCharInstancePP21_animationOutputBlock);
#endif
/* a finished transition: blendTo takes its place in the tree, blendFrom is released */
void animationCollapseBlend(_animBlendNode *b, _animCharInstance *ci)
{
    b->active = 0;
    if (b->parent) {
        b->parent->blendFrom = b->blendTo;
        b->parent->fromTime.startField = b->toTime.startField;
        b->parent->fromTime.startTime = b->toTime.startTime;
        b->parent->fromTime.deltaTime = b->toTime.deltaTime;
    } else {
        ciPlayer(ci)->mainTree = b->blendTo;
    }
    animationCleanUpTree(b->blendFrom, ci);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/68 words, size 0x108 vs 0x110 */
/* pauses the control nodes of a subtree and returns its blend nodes to the pool */
void animationCleanUpTree(_hierhead *node, _animCharInstance *ci)
{
    if (!node || !ci)
        return;
    if (node->opcode == ANIMATION_CONTROL_NODE) {
        _animHandle h;
        makeHandle(h, ci, (AnimControlNode *)node);
        animationPause(h);
    } else if (node->opcode == ANIMATION_BLEND_NODE) {
        AnimBlendNode *b = (AnimBlendNode *)node;
        animationCleanUpTree(b->blendFrom, ci);
        animationCleanUpTree(b->blendTo, ci);
        blendFree(b);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationCleanUpTree__FP9_hierheadP17_animCharInstance);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 6/52 words, size 0xcc vs 0xd0 */
/* makes h the main tree, pausing the control node or discarding the blend tree it replaces */
void animationAddToActiveTree(_animHandle h)
{
    AnimPlayer *player;
    HierHead *cur;
    if (!h.ci || !h.ctl)
        return;
    player = ciPlayer(h.ci);
    cur = AnimPlayer_GetMainTreeNode(player);
    if (cur) {
        if (cur->opcode == ANIMATION_CONTROL_NODE) {
            _animHandle old;
            old.ci = h.ci;
            old.proc = 0;
            old.ctl = (AnimControlNode *)cur;
            old.animIdx = ((AnimControlNode *)cur)->anim->animIdx;
            animationPause(old);
        } else if (cur->opcode == ANIMATION_BLEND_NODE) {
            animationCleanUpTree(cur, h.ci);
        }
    }
    player->mainTree = (HierHead *)h.ctl;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationAddToActiveTree__FG11_animHandle);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/19 words, size 0x34 vs 0x4c */
/* evaluates anim right away into the instance's output */
void animationUpdate(_animation *anim, _animCharInstance *ci)
{
    AnimControlNode *c = &ci->animCtx[anim->animIdx];
    c->animOutput = ci->animOutput;
    animationUpdateFromControlNode(c, ci);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationUpdate__FP10_animationP17_animCharInstance);
#endif
void animationManager(_animmgr *mgr)
{
    unsigned i;
    _animCharInstance **p = mgr->charInstance;
    for (i = 0; i < mgr->numCharInstances; i++, p++)
        if (!(*p)->bits.lazyEvaluate && (*p)->activeTree)
            AnimPlayer_UpdateAnimations(ciPlayer(*p));
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/22 words, size 0x58 vs 0x58 */
void animationSetInstance(_animmgr *mgr, int id2)
{
    unsigned i;
    _animCharInstance **p = mgr->charInstance;
    for (i = 0; i < mgr->numCharInstances; i++, p++)
        (*p)->head.id2 = id2;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationSetInstance__FP8_animmgri);
#endif
/* per frame: every character instance of every loaded NGP */
void animationRunGlobal(void)
{
    int i;
    HierAnimMgr **m = animMgrs;
    for (i = 0; i < getNgpFilesLoaded(); i++, m++)
        if (*m)
            animationManager(*m);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 45/60 words, size 0xd8 vs 0xf0 */
/* control node idx gets anim's ids and starts at its first frame (running if marked active in the data) */
void animationInitialize(_animation *anim, _animCharInstance *ci, int idx)
{
    AnimControlNode *c;
    anim->animIdx = idx;
    c = &ci->animCtx[idx];
    copyHead(&c->head, &c->anim->head);
    if (c->active) {
        _animHandle h;
        makeHandle(h, ci, c);
        c->startField = timerGetFieldCount();
        c->startTime = anim->startTime;
        animationAddToActiveTree(h);
        return;
    }
    c->startField = 0;
    c->startTime = anim->startTime;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationInitialize__FP10_animationP17_animCharInstancei);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 15/34 words, size 0x88 vs 0x80 */
void animationInitGlobal(void)
{
    int i;
    animStack.clear();
    for (i = 127; i >= 0; i--)
        blendPool.freeIdx[i] = i;
    blendPool.used = 0;
    for (i = 13; i >= 0; i--)
        animMgrs[i] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", animationInitGlobal__Fv);
#endif
/* registers the animation manager of NGP slot `slot`: an AnimPlayer per instance and every control node set up */
void animationSetGlobal(_hierhead *node, int slot)
{
    unsigned i, k;
    animMgrs[slot] = (HierAnimMgr *)node;
    for (i = 0; i < animMgrs[slot]->numCharInstances; i++) {
        _animCharInstance *ci = animMgrs[slot]->charInstance[i];
        if (!ci->activeTree)
            ci->activeTree = (HierHead *)AnimPlayer_ctor((AnimPlayer *)operator new(0x18, MemoryStack::global.alloc16(0x18)), ci);
        for (k = 0; k < ci->character->numAnims; k++)
            animationInitialize(ci->character->animations[k], ci, k);
    }
}
void animationInitTweakers(void)
{
}
void animationUpdateTweakers(int, void *)
{
}
AnimPlayer *AnimPlayer_ctor(AnimPlayer *self, _animCharInstance *ci)
{
    self->animCI = ci;
    self->mainTree = 0;
    self->mainTreePureOut = 0;
    self->head.opcode = ANIMATION_PLAYER_NODE;
    self->activeList = 0;
    self->numNodes = 0;
    return self;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 52/83 words, size 0x14c vs 0x140 */
/* Runs the main tree, then the layered blends in priority order. With modifier blends set up, channels the main
 * tree did not write this frame are restored from (and written ones saved to) mainTreePureOut. */
void AnimPlayer_UpdateAnimations(AnimPlayer *self)
{
    _animCharInstance *ci = self->animCI;
    HierHead *main;
    AnimBlendNode *b;
    if (ci->bits.disableCI)
        return;
    main = self->mainTree;
    if (main && !(main->opcode == ANIMATION_CONTROL_NODE && !((AnimControlNode *)main)->active))
        animationUpdateActiveTree(main, ci);
    if (!self->activeList)
        return;
    if (self->mainTreePureOut) {
        float *pure = self->mainTreePureOut;
        float *val = self->animCI->animOutput.val;
        unsigned char *dirty = self->animCI->animOutput.dirty;
        int i;
        for (i = 0; i < self->animCI->character->numChannels; i++, val++, pure++) {
            if (dirtyBit(dirty, i))
                *pure = *val;
            else
                *val = *pure;
        }
    }
    for (b = (AnimBlendNode *)self->activeList; b;) {
        AnimBlendNode *next;
        animationUpdateActiveTree((HierHead *)b, self->animCI);
        next = b->next;
        if (b == next) {
            printf("Animation blend list is broke.  Next pointer equals itself.\n");
            return;
        }
        b = next;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", UpdateAnimations__10AnimPlayer);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 16/108 words, size 0x1b0 vs 0x1a8 */
/* inserts b before the first node of higher priority value (list sorted ascending); returns 1 when inserted */
int AnimPlayer_AddBlendToActiveList(AnimPlayer *self, AnimBlendNode *b)
{
    AnimBlendNode *n = (AnimBlendNode *)self->activeList;
    int inserted = 0;

    if (!n) {
        inserted = 1;
        self->activeList = (HierHead *)b;
        if (self->mainTreePureOut) {
            float *pure = self->mainTreePureOut;
            float *val = self->animCI->animOutput.val;
            unsigned k;
            for (k = self->animCI->character->numChannels; k; k--)
                *pure++ = *val++;
        }
    } else {
        do {
            if (!n->prev && n != (AnimBlendNode *)self->activeList)
                printf("STOP HERE(3)\n");
            if (b->priority < n->priority) {
                if (!n->prev) {
                    self->activeList = (HierHead *)b;
                    b->next = n;
                    n->prev = b;
                    if (b == n)
                        printf("AddBlendToActiveList doubly inserting a node(1)\n");
                    inserted = 1;
                } else {
                    if (b == n->prev || b == n)
                        printf("AddBlendToActiveList doubly inserting a node(2)\n");
                    inserted = 1;
                    n->prev->next = b;
                    b->next = n;
                    b->prev = n->prev;
                    n->prev = b;
                }
            } else if (n->next) {
                n = n->next;
            } else {
                if (b == n)
                    printf("AddBlendToActiveList doubly inserting a node(3)\n");
                n->next = b;
                inserted = 1;
                b->prev = n;
            }
        } while (n && !inserted);
    }
    self->numNodes++;
    return inserted;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", AddBlendToActiveList__10AnimPlayerP14_animBlendNode);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 50/53 words, size 0xd4 vs 0xd4 */
int AnimPlayer_RemoveBlendFromActiveList(AnimPlayer *self, AnimBlendNode *b)
{
    AnimBlendNode *n;
    for (n = (AnimBlendNode *)self->activeList; n; n = n->next) {
        if (!n->prev && n != (AnimBlendNode *)self->activeList)
            printf("STOP HERE(1)\n");
        if (n == b) {
            if ((AnimBlendNode *)self->activeList == n)
                self->activeList = (HierHead *)n->next;
            if (n->prev)
                n->prev->next = n->next;
            if (n->next)
                n->next->prev = n->prev;
            n->next = 0;
            n->prev = 0;
            self->numNodes--;
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", RemoveBlendFromActiveList__10AnimPlayerP14_animBlendNode);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 2/39 words, size 0x9c vs 0x94 */
/* also repairs a missing prev link on the way */
int AnimPlayer_IsBlendInActiveList(AnimPlayer *self, AnimBlendNode *b)
{
    AnimBlendNode *n, *prev = 0;
    for (n = (AnimBlendNode *)self->activeList; n;) {
        AnimBlendNode *next;
        if (!n->prev) {
            n->prev = prev;
            if (n != (AnimBlendNode *)self->activeList)
                printf("STOP HERE(2)\n");
        }
        if (n == b)
            return 1;
        next = n->next;
        if (n == next)
            return 0;
        prev = n;
        n = next;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/animation", IsBlendInActiveList__10AnimPlayerP14_animBlendNode);
#endif
HierHead *AnimPlayer_GetMainTreeNode(AnimPlayer *self)
{
    return self->mainTree;
}
HierHead *AnimPlayer_GetActiveList(AnimPlayer *self)
{
    return self->activeList;
}
int AnimPlayer_GetNumNodesInActiveList(AnimPlayer *self)
{
    return self->numNodes;
}
INCLUDE_ASM("asm/nonmatchings/common/animation", __static_initialization_and_destruction_0_001F1FC8);
INCLUDE_ASM("asm/nonmatchings/common/animation", _GLOBAL_$I$animationInitModifierBlends__FP17_animCharInstance);
INCLUDE_ASM("asm/nonmatchings/common/animation", _GLOBAL_$D$animationInitModifierBlends__FP17_animCharInstance);
