#ifndef MEMORY_STACK_H
#define MEMORY_STACK_H

/* Linear allocator with a mark chain: every mark stores the previous mark in the word it occupies. */
class MemoryStack {
public:
    void *start;
    char *low;
    int mark;
    char *end;
    char *hi1;
    char *hi2;

    static MemoryStack global;

    MemoryStack(void *mem, unsigned size);
    void clear(void);

    /* Pushes a mark at the low end (inlined everywhere in retail). */
    void pushMark(void)
    {
        int *q = (int *)(((int)low + 3) & ~3);
        int old = mark;

        mark = (int)q;
        low = (char *)(q + 1);
        *q = old;
    }
    /* Releases everything allocated since the last pushMark. */
    void popMark(void)
    {
        low = (char *)mark;
        mark = *(int *)mark;
    }
    /* The same three at the high end (hi1 grows down, hi2 is the mark chain). */
    void pushHighMark(void)
    {
        int *q = (int *)(((int)hi1 - 4) & ~3);
        int old = (int)hi2;

        hi1 = (char *)q;
        hi2 = (char *)q;
        *q = old;
    }
    void popHighMark(void)
    {
        hi1 = hi2 + 4;
        hi2 = *(char **)hi2;
    }
    void *allocHigh16(unsigned size)
    {
        char *p = (char *)(((int)hi1 - size) & ~15);

        hi1 = p;
        return p;
    }
    /* 16-byte aligned allocation from the low end. */
    void *alloc16(unsigned size)
    {
        char *p = (char *)(((int)low + 15) & ~15);

        low = p + size;
        return p;
    }
};

#endif
