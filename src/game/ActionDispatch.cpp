#include "common.h"
#include "game/game.h"
#include "game/shell.h"
#include "game/sound.h"
#include "game/power_ups.h"
#include "game/level_pickups.h"
#include "game/streaming_sound.h"
#include "task_manager.h"

/* Level actions (ActHead trees in the database): scripts, destructible groups, animations, cinemas...
   addAction() copies the action with its placement into a DelayAction and runs it after head.delay seconds
   on the TU's own TaskManager. While stopAllActiveActions() flushes that manager, s_endingActions is set and
   doIt() runs the "end" variant of each action instead (animations jump to their last frame, no sounds). */

/* ActHead::type */
enum {
    ACT_GROUP = 0,
    ACT_TRANSFORM = 1,
    ACT_ANIM = 2,
    ACT_DEBRIS = 3,
    ACT_SOUND = 4,
    ACT_CAMSHAKE = 5,
    ACT_DOMINO = 8,
    ACT_CINEMA = 9,
    ACT_HATPICKUP = 10,
    ACT_PLAYERPOS = 11,
    ACT_RUNSCRIPT = 12,
    ACT_AINAVIGATION = 13,
    ACT_GENERICEVENT = 14,
};

struct ActHead {
    unsigned short type;    // 0x0
    unsigned short pad2;
    float delay;            // 0x4: seconds before the action runs
};
struct ActGroup : ActHead {
    unsigned count;         // 0x8
    ActHead *acts[1];       // 0xC
};
struct ActTransform : ActHead {
    ActHead *act;           // 0x8
    char padC[4];
    float mat[4][4];        // 0x10
};
/* Animation of a level object (animationGetHandle), or of a monster when a flag is set. */
enum {
    ACTANIM_PLAYER = 0x800,  // game->m_monsters[obj]
    ACTANIM_AI = 0x1000,     // game->m_lists.ais[obj]
};
struct ActAnim : ActHead {
    unsigned obj;           // 0x8: object, or monster index
    union {
        unsigned flags;     // 0xC: ACTANIM_*
        struct {
            unsigned short anim : 11;
            unsigned short : 5;
            unsigned short slot;    // 0xE: animation slot (Monster::m_anims)
        } ids;
    };
};
struct ActDebris;
struct ActCamShake : ActHead {
    float strength;         // 0x8
};
struct ActDomino : ActHead {
    int state;              // 0x8: new state of the target destructible
    _hierhead *target;      // 0xC
};
struct ActHatPickup : ActHead {
    float radius;           // 0x8
    _fvector3 offset;       // 0xC
};
struct ActCinema : ActHead {
    float length;           // 0x8: fields
    int flags;              // 0xC: bit 0 letterbox flag while it runs, bit 1 when it ends
};
struct ActPlayerPos : ActHead {
    unsigned idx : 16;      // 0x8
    unsigned ai : 1;        // 0x8.16: game->m_lists.ais[idx]
};
struct ActRunScript : ActHead {
    unsigned short id1;     // 0x8
    unsigned short id2;     // 0xA
    _hierScript *script;    // 0xC: null = look it up by id among the user defined scripts
};
class ActAiNavigation : public ActHead {
public:
    unsigned short idx;     // 0x8
};
struct ActGenericEvent : ActHead {
    unsigned event;         // 0x8
};

class Destructible {
public:
    void ChangeState(int state, bool a, bool b);
    void doDebris(ActDebris *act, float (*m)[4]);
};
class Interactives {
public:
    static Destructible *getInteractive(int id);
};
class PedGroup {
public:
    static void startCinema(void);
    static void endCinema(void);
};
class Letterboxing {
public:
    char pad0[0x28];
    int f28;
    void setEnabled(bool on);
};
extern Letterboxing hudLetterbox __asm__("_3Hud$m_letterbox");
/* Cameras::m_cameras, one per view (stride 0xEB0) */
struct CameraSlot {
    char pad0[0xD90];
    int fD90;
    char padD94[4];
    int fD98;
    char padD9C[0xEB0 - 0xD9C];
};
extern CameraSlot cameraSlots[] __asm__("_7Cameras$m_cameras");
/* through an inline: retail keeps &m_cameras in a register and adds 0xEB0 for view 1 instead of folding it */
static inline CameraSlot *getCamera(int view)
{
    return &cameraSlots[view];
}

float animationGetLastFrame(_animHandle h);
float animationGetTotalRunFrames(_animHandle h);
void animationSetToFrame(_animHandle h, float frame, int update);
void hierSetLosPointsPerView(int idx, int points);
void viewToggleSplitScreen(bool a, bool b);

class DelayAction {
public:
    ActHead *m_act;         // 0x0
    char pad4[0xC];
    float m_mat[4][4];      // 0x10
    FVECTOR m_pos;          // 0x50
    Destructible *m_dest;   // 0x60
    int m_active;           // 0x64

    static DelayAction m_pool[64];

    static void init(void);
    static DelayAction *getInstance(void);
    unsigned doIt(void);
    unsigned endIt(void);
    unsigned fade(void);
    static unsigned doIt(void *p);
    static unsigned endIt(void *p);
    static unsigned fade(void *p);
};
typedef char _size_DelayAction[sizeof(DelayAction) == 0x70 ? 1 : -1];

namespace ActionDispatch {
unsigned stopAllActiveActions(void);
void initBefore(void);
void initGenericEvent(void (*cb)(unsigned));
void addUserDefinedScipt(_hierScript *s);
void callScriptFromId(unsigned short id1, unsigned short id2, int fadeOut);
void addAction(ActHead *act, float (*m)[4], _fvector *pos, Destructible *d);
}

extern int moviePlaying;
__asm__("#SNFIX_SMALL moviePlaying");
extern int movieAborted;
__asm__("#SNFIX_SMALL movieAborted");
extern void (*s_genericEventCB)(unsigned);
__asm__("#SNFIX_SMALL s_genericEventCB");
extern int s_numUserDefinedScripts;
__asm__("#SNFIX_SMALL s_numUserDefinedScripts");
extern int s_fadeOut;
__asm__("#SNFIX_SMALL s_fadeOut");
extern int s_endingActions;
__asm__("#SNFIX_SMALL s_endingActions");
/* retail names (local in the ELF); the bss is still the asm data file, labelled by address */
extern TaskManager s_actionTasks __asm__("D_006F9080");
extern _hierScript *s_userDefinedScripts[200] __asm__("D_006F9090");

typedef unsigned (*TaskFunc)(void *);

#define LEVEL_OBJECT_SOUND() ((LevelObjectSoundManager *)((char *)game + 0x121570))

/* sceVu0CopyMatrix / sceVu0CopyVector style quadword copies (the retail registers are fixed by the asm) */
static inline void vu0CopyVector(_fvector *dst, _fvector *src)
{
    __asm__ volatile("lq $2, %1\n\tsq $2, %0" : "=m"(*(FVECTOR *)dst) : "m"(*(FVECTOR *)src) : "$2");
}
static inline void vu0CopyMatrix(float (*dst)[4], float (*src)[4])
{
    FVECTOR *d = (FVECTOR *)dst;
    FVECTOR *s = (FVECTOR *)src;

    __asm__ volatile("lq $8, %4\n\tlq $9, %5\n\tlq $10, %6\n\tlq $11, %7\n\t"
                     "sq $8, %0\n\tsq $9, %1\n\tsq $10, %2\n\tsq $11, %3"
                     : "=m"(d[0]), "=m"(d[1]), "=m"(d[2]), "=m"(d[3])
                     : "m"(s[0]), "m"(s[1]), "m"(s[2]), "m"(s[3])
                     : "$8", "$9", "$10", "$11");
}

unsigned ActionDispatch::stopAllActiveActions(void)
{
    s_endingActions = 1;
    s_actionTasks.executeAndDeleteAll();
    s_endingActions = 0;
    moviePlaying = 0;
    movieAborted = 0;
    return 0;
}

void ActionDispatch::initBefore(void)
{
    s_numUserDefinedScripts = 0;
    s_genericEventCB = 0;
    s_actionTasks.init(64);
    TaskManager::global.add((TaskFunc)&TaskManager::update, &s_actionTasks, 1);
    DelayAction::init();
}

void ActionDispatch::initGenericEvent(void (*cb)(unsigned))
{
    s_genericEventCB = cb;
}

void ActionDispatch::addUserDefinedScipt(_hierScript *s)
{
    int n = s_numUserDefinedScripts;

    s_numUserDefinedScripts = n + 1;
    s_userDefinedScripts[n] = s;
}

void ActionDispatch::callScriptFromId(unsigned short id1, unsigned short id2, int fadeOut)
{
    int i;

    for (i = 0; i < s_numUserDefinedScripts; i++) {
        _hierScript *s = s_userDefinedScripts[i];
        if (s->head.id1 == id1 && s->head.id2 == id2) {
            addAction(s->root, s->mat, &s->trans, 0);
            s_fadeOut = fadeOut;
            return;
        }
    }
}

void doRunScript(ActRunScript *act)
{
    _hierScript *s = act->script;

    if (s == 0) {
        ActionDispatch::callScriptFromId(act->id1, act->id2, 0);
    } else {
        ActionDispatch::addAction(s->root, s->mat, &s->trans, 0);
    }
}

void doGroup(_fvector *pos, float (*m)[4], ActGroup *grp, Destructible *d)
{
    unsigned short i;
    unsigned short j;

    for (i = 0; i < grp->count; i++) {
        int delayed = 0;

        if (grp->acts[i]->type == ACT_HATPICKUP) {
            grp->acts[i]->delay = 0.06666667f;
            for (j = 0; j < grp->count; j++) {
                ActAnim *a = (ActAnim *)grp->acts[j];
                _animHandle h;

                if (a->type == ACT_ANIM) {
                    if (a->flags & (ACTANIM_PLAYER | ACTANIM_AI)) {
                        Monster *mon = 0;
                        if (a->flags & ACTANIM_PLAYER) {
                            if (a->obj < (unsigned)game->m_numMonsters)
                                mon = game->m_monsters[a->obj];
                        } else {
                            if (a->obj < (unsigned)game->m_numAIs)
                                mon = game->m_lists.ais[a->obj];
                        }
                        if (mon != 0) {
                            _animHandle *ph = &mon->m_anims[a->ids.slot];
                            delayed = 1;
                            h = *ph;
                            grp->acts[i]->delay += animationGetTotalRunFrames(h) / 60.0f;
                        }
                    } else {
                        h.proc = 0;
                        delayed = 1;
                        animationGetHandle(&h, a->obj, a->ids.anim, a->ids.slot);
                        grp->acts[i]->delay += animationGetTotalRunFrames(h) / 60.0f;
                    }
                }
            }
        }
        ActionDispatch::addAction(grp->acts[i], m, pos, d);
        if (delayed) {
            ActHatPickup *hat = (ActHatPickup *)grp->acts[i];
            _fvector p;
            p.x = pos->x + hat->offset.x;
            p.y = pos->y + hat->offset.y;
            p.z = pos->z + hat->offset.z;
            LevelPickups::turnOffForDelayedHat(&p, hat->radius);
        }
    }
}

void doTransform(_fvector *pos, float (*m)[4], ActTransform *act, Destructible *d)
{
    ActionDispatch::addAction(act->act, act->mat, pos, d);
}

void triggerAnimation(DelayAction *da, ActAnim *act)
{
    if (act->flags & (ACTANIM_PLAYER | ACTANIM_AI)) {
        Monster *mon;
        _animHandle *h;

        if (act->flags & ACTANIM_PLAYER) {
            if (act->obj >= (unsigned)game->m_numMonsters)
                return;
            mon = game->m_monsters[act->obj];
        } else {
            if (act->obj >= (unsigned)game->m_numAIs)
                return;
            mon = game->m_lists.ais[act->obj];
        }
        if (mon == 0)
            return;
        h = &mon->m_anims[act->ids.slot];
        animationSetSpeed(*h, 1.0f);
        animationTransitionInto(*h, 200.0f, 1, 1);
        s_actionTasks.add((TaskFunc)&DelayAction::endIt, da, (int)animationGetLastFrame(*h));
    } else {
        _animHandle h;
        h.proc = 0;
        animationGetHandle(&h, act->obj, act->ids.anim, act->ids.slot);
        animationTransitionInto(h, 200.0f, 1, 1);
        s_actionTasks.add((TaskFunc)&DelayAction::endIt, da, (int)animationGetLastFrame(h));
    }
    da->m_active = 1;
}

void endAnimation(ActAnim *act)
{
    if (s_endingActions == 0)
        return;
    if (act->flags & (ACTANIM_PLAYER | ACTANIM_AI)) {
        Monster *mon;
        _animHandle *h;

        if (act->flags & ACTANIM_PLAYER) {
            if (act->obj >= (unsigned)game->m_numMonsters)
                return;
            mon = game->m_monsters[act->obj];
        } else {
            if (act->obj >= (unsigned)game->m_numAIs)
                return;
            mon = game->m_lists.ais[act->obj];
        }
        if (mon == 0)
            return;
        h = &mon->m_anims[act->ids.slot];
        animationSetToFrame(*h, animationGetLastFrame(*h) - 3.0f, 1);
        animationStart(*h, false);
    } else {
        _animHandle h;
        h.proc = 0;
        animationGetHandle(&h, act->obj, act->ids.anim, act->ids.slot);
        animationSetToFrame(h, animationGetLastFrame(h) - 3.0f, 1);
        animationStart(h, false);
    }
}

void doCamShake(_fvector *pos, ActCamShake *act)
{
    Cameras::StartShake(*pos, act->strength);
}

void doDomino(ActDomino *act)
{
    Interactives::getInteractive(act->target->id2)->ChangeState(act->state, false, true);
}

void doPickupHat(_fvector *pos, ActHatPickup *act)
{
    _fvector p;
    p.x = pos->x + act->offset.x;
    p.y = pos->y + act->offset.y;
    p.z = pos->z + act->offset.z;
    LevelPickups::hatCheck(&p, act->radius);
}

void doCinema(DelayAction *da, ActCinema *act)
{
    int i;

    moviePlaying = 1;
    hudLetterbox.f28 = act->flags & 1;
    movieAborted = 0;
    hudLetterbox.setEnabled(true);
    s_actionTasks.add((TaskFunc)&DelayAction::endIt, da, (int)act->length);
    if (timerGetFieldCount() < 60 || s_fadeOut)
        s_actionTasks.add((TaskFunc)&DelayAction::fade, da, (int)(act->length - 30.0f));
    i = 0;
    if (i < game->m_numSlots) {
        do {
            game->m_slots[i].startCinema();
        } while (++i < game->m_numSlots);
    }
    game->m_playerMask ^= 7;
    PowerUps::instance.startCinema();
    PedGroup::startCinema();
    LevelPickups::startCinema();
    hierSetLosPointsPerView(0, 0);
    cameraSlots[0].fD90 = 0;
    if (game->m_numMonsters >= 2)
        viewToggleSplitScreen(true, false);
    da->m_active = 1;
    shell->m_inCinema = 1;
    STREAMING_SOUND()->startCinemaMusic();
}

void endCinema(DelayAction *da, ActCinema *act)
{
    int i;

    if (s_endingActions) {
        da->m_active = 1;
        TaskManager::global.add((TaskFunc)&DelayAction::endIt, da, 4);
        moviePlaying = 0;
        shell->m_inCinema = 0;
        return;
    }
    i = 0;
    if (i < game->m_numSlots) {
        do {
            game->m_slots[i].endCinema();
        } while (++i < game->m_numSlots);
    }
    moviePlaying = 0;
    movieAborted = 0;
    hudLetterbox.f28 = (act->flags >> 1) & 1;
    hudLetterbox.setEnabled(false);
    game->m_playerMask |= 7;
    PowerUps::instance.endCinema();
    PedGroup::endCinema();
    LevelPickups::endCinema();
    if (game->m_numMonsters >= 2) {
        viewToggleSplitScreen(true, false);
        getCamera(1)->fD98 = 1;
        getCamera(1)->fD90 = 1;
    }
    getCamera(0)->fD98 = 1;
    getCamera(0)->fD90 = 1;
    shell->m_inCinema = 0;
    STREAMING_SOUND()->stopCinemaMusic();
}

void doSound(_fvector *pos, ActPlaySound *act)
{
    if (act->m_time != 0)
        LEVEL_OBJECT_SOUND()->playTimedBuildingDestructibleSound(act, pos);
    else
        LEVEL_OBJECT_SOUND()->playDestructibleSound(act->m_soundId, pos);
}

void doSetPlayerPos(_fvector *pos, float (&m)[4][4], ActPlayerPos *act)
{
    Monster *mon;

    if (act->ai)
        mon = game->m_lists.ais[act->idx];
    else
        mon = game->m_monsters[act->idx];
    if (mon != 0) {
        mon->setTrans(*pos);
        mon->setMat(m);
    }
}

void doAiNavigate(ActAiNavigation *act)
{
    Monster *mon = game->m_monsters[act->idx];

    if (mon != 0)
        mon->handleAction(act);
}

void doGenericEvent(ActGenericEvent *act)
{
    if (s_genericEventCB != 0)
        s_genericEventCB(act->event);
}

void ActionDispatch::addAction(ActHead *act, float (*m)[4], _fvector *pos, Destructible *d)
{
    DelayAction *da = DelayAction::getInstance();

    if (da == 0)
        return;
    da->m_dest = d;
    da->m_act = act;
    vu0CopyMatrix(da->m_mat, m);
    vu0CopyVector(&da->m_pos, pos);
    if (s_endingActions)
        da->doIt();
    else
        s_actionTasks.add((TaskFunc)&DelayAction::doIt, da, (int)(act->delay * 60.0f));
}

void DelayAction::init(void)
{
    DelayAction *da = m_pool;
    int i;

    for (i = 0; i < 64; i++, da++)
        da->m_active = 0;
}

DelayAction *DelayAction::getInstance(void)
{
    DelayAction *da = m_pool;
    int i;

    for (i = 0; i < 64; i++, da++) {
        if (da->m_active == 0)
            break;
    }
    if (i == 64)
        return 0;
    da->m_active = 1;
    return da;
}

unsigned DelayAction::doIt(void)
{
    m_active = 0;
    if (s_endingActions == 0) {
        switch (m_act->type) {
        case ACT_GROUP:
            doGroup(&m_pos, m_mat, (ActGroup *)m_act, m_dest);
            break;
        case ACT_TRANSFORM:
            doTransform(&m_pos, m_mat, (ActTransform *)m_act, m_dest);
            break;
        case ACT_ANIM:
            triggerAnimation(this, (ActAnim *)m_act);
            break;
        case ACT_DEBRIS:
            m_dest->doDebris((ActDebris *)m_act, m_mat);
            break;
        case ACT_CAMSHAKE:
            doCamShake(&m_pos, (ActCamShake *)m_act);
            break;
        case ACT_SOUND:
            doSound(&m_pos, (ActPlaySound *)m_act);
            break;
        case ACT_DOMINO:
            doDomino((ActDomino *)m_act);
            break;
        case ACT_CINEMA:
            doCinema(this, (ActCinema *)m_act);
            break;
        case ACT_HATPICKUP:
            doPickupHat(&m_pos, (ActHatPickup *)m_act);
            break;
        case ACT_PLAYERPOS:
            doSetPlayerPos(&m_pos, m_mat, (ActPlayerPos *)m_act);
            break;
        case ACT_RUNSCRIPT:
            doRunScript((ActRunScript *)m_act);
            break;
        case ACT_AINAVIGATION:
            doAiNavigate((ActAiNavigation *)m_act);
            break;
        case ACT_GENERICEVENT:
            doGenericEvent((ActGenericEvent *)m_act);
            break;
        }
    } else {
        switch (m_act->type) {
        case ACT_GROUP:
            doGroup(&m_pos, m_mat, (ActGroup *)m_act, m_dest);
            break;
        case ACT_TRANSFORM:
            doTransform(&m_pos, m_mat, (ActTransform *)m_act, m_dest);
            break;
        case ACT_ANIM:
            endAnimation((ActAnim *)m_act);
            break;
        case ACT_DEBRIS:
            m_dest->doDebris((ActDebris *)m_act, m_mat);
            break;
        case ACT_DOMINO:
            doDomino((ActDomino *)m_act);
            break;
        case ACT_HATPICKUP:
            doPickupHat(&m_pos, (ActHatPickup *)m_act);
            break;
        case ACT_PLAYERPOS:
            doSetPlayerPos(&m_pos, m_mat, (ActPlayerPos *)m_act);
            break;
        case ACT_RUNSCRIPT:
            doRunScript((ActRunScript *)m_act);
            break;
        case ACT_AINAVIGATION:
            doAiNavigate((ActAiNavigation *)m_act);
            break;
        case ACT_GENERICEVENT:
            doGenericEvent((ActGenericEvent *)m_act);
            break;
        }
    }
    return 0;
}

/* retail pads the second jump table of doIt to 16 words */
__asm__(".section .rodata\n\t.word 0\n\t.text");

unsigned DelayAction::endIt(void)
{
    m_active = 0;
    if (m_act->type != ACT_ANIM) {
        if (m_act->type == ACT_CINEMA)
            endCinema(this, (ActCinema *)m_act);
    } else {
        endAnimation((ActAnim *)m_act);
    }
    return 0;
}

unsigned DelayAction::fade(void)
{
    if (s_endingActions == 0)
        game->fadeOutAndIn(4);
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/game/ActionDispatch", __static_initialization_and_destruction_0_001011D0);

unsigned TaskManager::update(void *tm)
{
    return ((TaskManager *)tm)->update();
}

unsigned DelayAction::doIt(void *p)
{
    return ((DelayAction *)p)->doIt();
}

unsigned DelayAction::endIt(void *p)
{
    return ((DelayAction *)p)->endIt();
}

unsigned DelayAction::fade(void *p)
{
    return ((DelayAction *)p)->fade();
}

INCLUDE_ASM("asm/nonmatchings/game/ActionDispatch", _GLOBAL_$I$moviePlaying);
INCLUDE_ASM("asm/nonmatchings/game/ActionDispatch", _vt$18AiThrashActionList);
