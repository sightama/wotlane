#include "common.h"
#include "hieri_types.h"
#include "cs_pool.h"

extern "C" int printf(const char *, ...);

struct StackLayout {
    void *start;
    char *low;
    int mark;
    char *end;
};

extern StackLayout gMemStack __asm__("_11MemoryStack$global");
void *operator new(unsigned, void *);
void InitColGridCsListArray(void);
void hierInitCs(_cs *);
void hdRemoveCsFromGrid(_cs *);

#ifdef NON_MATCHING
/* equivalent (untuned): 55/57 words, size 0xe4 vs 0xe4 */
/* 55/57 words: order of the six list-head stores differs */
void CsPool::init(void)
{
    CsNode *node;
    int i;

    m_activeList.next = &m_activeList;
    m_activeList.prev = &m_activeList;
    m_inactiveList.prev = &m_inactiveList;
    m_inactiveList.next = &m_inactiveList;
    m_HPActiveList.prev = &m_HPActiveList;
    m_HPActiveList.next = &m_HPActiveList;
    for (i = 0; i < 1024; i++) {
        char *mem = (char *)(((int)gMemStack.low + 0xF) & -16);

        gMemStack.low = mem + 0xC;
        node = new (mem) CsNode;
        node->cs = &m_csPool[i];
        node->prev = &m_inactiveList;
        node->next = m_inactiveList.next;
        m_inactiveList.next = node;
        if (node->next)
            node->next->prev = node;
    }
    InitColGridCsListArray();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/CsPool", init__6CsPool);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/37 words, size 0x88 vs 0x94 */
/* Moves node n from its list to the front of list l (the lists are circular through their head node, whose cs is 0). */
static inline void csNodeMove(CsNode *n, CsNode *l)
{
    if (n->prev)
        n->prev->next = n->next;
    if (n->next)
        n->next->prev = n->prev;
    n->next = 0;
    n->prev = l;
    n->next = l->next;
    l->next = n;
    if (n->next)
        n->next->prev = n;
}
/* Takes a cs from the inactive list (0 when the pool is exhausted) and initializes it. */
_cs *CsPool::csActivate(void)
{
    CsNode *n = m_inactiveList.next;

    if (!n->cs)
        return 0;
    csNodeMove(n, &m_activeList);
    hierInitCs(n->cs);
    return n->cs;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/CsPool", csActivate__6CsPool);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 32/46 words, size 0xb8 vs 0xb8 */
void CsPool::csDeactivate(_cs *cs)
{
    CsNode *n;
    int found = 0;

    for (n = m_activeList.next; n != &m_activeList; n = n->next) {
        if (n->cs == cs) {
            csNodeMove(n, &m_inactiveList);
            hdRemoveCsFromGrid(n->cs);
            found = 1;
            break;
        }
    }
    if (!found)
        printf("CsPool::csDeactivate - Trying to deactivate a cs that is not in active list.\n");
}
#else
INCLUDE_ASM("asm/nonmatchings/common/CsPool", csDeactivate__6CsPoolP3_cs);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 3/37 words, size 0x88 vs 0x94 */
/* The same with the high-priority active list. */
_cs *CsPool::csHPActivate(void)
{
    CsNode *n = m_inactiveList.next;

    if (!n->cs)
        return 0;
    csNodeMove(n, &m_HPActiveList);
    hierInitCs(n->cs);
    return n->cs;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/CsPool", csHPActivate__6CsPool);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 32/46 words, size 0xb8 vs 0xb8 */
void CsPool::csHPDeactivate(_cs *cs)
{
    CsNode *n;
    int found = 0;

    for (n = m_HPActiveList.next; n != &m_HPActiveList; n = n->next) {
        if (n->cs == cs) {
            csNodeMove(n, &m_inactiveList);
            hdRemoveCsFromGrid(n->cs);
            found = 1;
            break;
        }
    }
    if (!found)
        printf("CsPool::csHPDeactivate - Trying to deactivate a cs that is not in active list.\n");
}
#else
INCLUDE_ASM("asm/nonmatchings/common/CsPool", csHPDeactivate__6CsPoolP3_cs);
#endif
INCLUDE_ASM("asm/nonmatchings/common/CsPool", __static_initialization_and_destruction_0_001F7858);
INCLUDE_ASM("asm/nonmatchings/common/CsPool", _GLOBAL_$I$_6CsPool$m_activeList);
