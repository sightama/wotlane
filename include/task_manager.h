#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "bidir_link.h"

/* Per-frame task scheduler. `global` is the instance the game registers its update callbacks on. */
class TaskManager {
public:
    struct TaskConfig {
        TaskConfig *prev;
        TaskConfig *next;
        unsigned time;
        unsigned (*func)(void *);
        void *arg;
    };

    int unk0;
    TaskConfig *head;
    BidirLink freeList;

    static TaskManager global;

    TaskManager();
    void init(unsigned n);
    TaskConfig *add(unsigned (*f)(void *), void *arg, int delay);
    TaskConfig *add(unsigned (*f)(), int delay);
    void remove(void *&handle);
    int update(void);
    static unsigned update(void *tm);   /* task callback that runs another TaskManager (ActionDispatch) */
    void executeAndDeleteAll(void);
    TaskConfig *createTask(void);
    void deleteTask(TaskConfig *t);
};

/* A second scheduler instance 0x20 bytes after `global`; TheGame::Update runs this one, Update2 runs `global`. */
extern TaskManager gTaskManager0 __asm__("_11TaskManager$global0");

#endif
