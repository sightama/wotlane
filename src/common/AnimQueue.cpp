#include "common.h"

struct _animHandle {
    int a;
    int b;
    int c;
    int d;
};

void animationSetToBeginning(_animHandle h, bool b);
void animationPause(_animHandle h);
void animationStart(_animHandle h, bool b);
void animationStartReverse(_animHandle h, bool b);
float animationGetCurrentPercent(_animHandle h);

extern "C" int printf(const char *, ...);

class TaskManager {
public:
    void *add(unsigned (*f)(void *), void *arg, int delay);
};
extern TaskManager gTaskManager __asm__("_11TaskManager$global");

class AnimQueuable {
public:
    _animHandle *handle;
    int f4;
    int f8;
    int fC;
    int f10;
    int f14;

    AnimQueuable();
};

class AnimQueue {
public:
    AnimQueuable q[12];
    int count;

    AnimQueue();
    void Reset(void);
    void Push(_animHandle *h, int a, int b, int c);
    void Pop(void);
    int taskUpdate(void);
    void RequestInterruption(void);
    static unsigned taskUpdate(void *p);
};

AnimQueuable::AnimQueuable()
{
    handle = 0;
    f4 = 0;
    f8 = 0;
    fC = 0;
}
AnimQueue::AnimQueue()
{
    count = 0;
}
void AnimQueue::Reset(void)
{
    int i;

    for (i = 0; i < 12; i++) {
        AnimQueuable *p = &q[i];


        if (p->handle) {
            animationSetToBeginning(*p->handle, true);
            animationPause(*p->handle);
        }
        p->handle = 0;
        p->f4 = 0;
        p->f8 = 0;
        p->fC = 0;
        p->f10 = 0;
        p->f14 = 0;
    }
    count = 0;
}
#ifdef NON_MATCHING
/* 74/76 words: retail loads the TaskManager global's hi part into $a0 before the count branch */
void AnimQueue::Push(_animHandle *h, int a, int b, int c)
{
    if (count >= 12) {
        printf("Animation Queue is full, we need more than %i animhandles\n", 12);
        return;
    }
    if (h->a == 0) {
        printf("Animation at %p is not valid!  I am not pushing its handle.\n", h);
        return;
    }
    if (count)
        RequestInterruption();
    else
        gTaskManager.add(taskUpdate, this, 1);
    q[count].handle = h;
    q[count].f4 = a;
    q[count].f8 = b;
    q[count].fC = 0;
    q[count].f10 = c;
    q[count++].f14 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/AnimQueue", Push__9AnimQueueP11_animHandleiii);
#endif
void AnimQueue::Pop(void)
{
    int i;

    for (i = 0; i < count; i++)
        q[i] = q[i + 1];
    count--;
}
int AnimQueue::taskUpdate(void)
{
    AnimQueuable *a = q;

    if (count == 0)
        return 0;
    if (a->fC == 0) {
        if (a->f4)
            animationStart(*a->handle, a->f8 != 0);
        else
            animationStartReverse(*a->handle, a->f8 != 0);
        a->fC = 1;
    } else {
        int done;

        if (a->f4)
            done = animationGetCurrentPercent(*a->handle) > 0.9f;
        else
            done = animationGetCurrentPercent(*a->handle) < 0.1f;
        if (done) {
            Pop();
        } else if (a->f14) {
            if (a->f10) {
                _animHandle *h = a->handle;

                a->f4 = 0;
                a->f8 = 0;
                a->fC = 0;
                a->f10 = 0;
                animationPause(*h);
            }
        }
    }
    return count > 0;
}
void AnimQueue::RequestInterruption(void)
{
    int i;

    for (i = 0; i < count; i++)
        q[i].f14 = 1;
}
unsigned AnimQueue::taskUpdate(void *p)
{
    return ((AnimQueue *)p)->taskUpdate();
}
