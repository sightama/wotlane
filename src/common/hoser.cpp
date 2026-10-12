#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

struct HoserBookmark {
    int state;
    int a;
    int b;
    int pad;
    float mat[4][4];
    int vec[4];
};
/* Free-camera bookmarks ("hoser", a debug tool): a doubly linked list threaded through 50 slots. */
struct HoserInfo {
    int f0;
    int f4;
    int f8;
    int fC;
    int f10;
    float f14;
    float f18;
    int f1C;
    int f20;
    int f24;
    int f28;
    int f2C;
    int f30;
    int f34;
    int f38;
    float f3C;
    float f40;
    int f44;
    int count;
    int cur;
    int f50;
    int f54;
    int f58;
    int f5C[9];
    HoserBookmark bm[50];
};

extern int D_006F8D80;
extern int D_006F8D84;
extern int D_006F8D88;
extern HoserInfo D_00778010;
void mathfUnitMatrix(float (*m)[4]);
extern "C" int printf(const char *fmt, ...);

int hoserFindFreeBookmark(HoserInfo *info);

#ifdef NON_MATCHING
/* equivalent (untuned): 7/115 words, size 0x1cc vs 0x178 */
void hoserInfoInit(HoserInfo *h)
{
    int i;

    printf("The Hoser Info Struct is %d\n", (int)sizeof(HoserInfo));
    h->fC = 20;
    h->f4 = 3;
    h->f10 = 5;
    h->f14 = 200.0f;
    h->f3C = 10.0f;
    h->f40 = 5.0f;
    h->f44 = 10;
    h->bm[0].state = 1;
    h->f8 = 1;
    h->f18 = 10.0f;
    h->f20 = 1;
    h->f24 = 1;
    h->f28 = 1;
    h->count = 1;
    h->bm[0].a = -1;
    h->f0 = 0;
    h->f1C = 0;
    h->f2C = 0;
    h->f30 = 0;
    h->f34 = 0;
    h->f38 = 0;
    h->cur = 0;
    h->f50 = 0;
    h->f54 = 0;
    h->f58 = 0;
    for (i = 0; i < 8; i++)
        h->f5C[i] = 0;
    h->bm[0].b = -1;
    mathfUnitMatrix(h->bm[0].mat);
    h->bm[0].vec[0] = 0;
    h->bm[0].vec[1] = 0;
    h->bm[0].vec[2] = 0;
    for (i = 1; i < 50; i++) {
        h->bm[i].state = -1;
        h->bm[i].a = -1;
        h->bm[i].b = -1;
        mathfUnitMatrix(h->bm[i].mat);
        h->bm[i].vec[0] = 0;
        h->bm[i].vec[1] = 0;
        h->bm[i].vec[2] = 0;
    }
    D_006F8D80 = 0;
    D_006F8D84 = 0;
    D_006F8D88 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hoser", hoserInfoInit__FP9HoserInfo);
#endif
int hoserFindFreeBookmark(HoserInfo *info)
{
    int r = -1;
    int i;

    for (i = 0; i < 50; i++) {
        if (info->bm[i].state == -1) {
            r = i;
            i = 50;
        }
    }
    return r;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/63 words, size 0xfc vs 0xec */
/* Inserts a bookmark (camera matrix m, position v) after the current one and makes it current. */
int hoserAddBookmark(HoserInfo *h, float (*m)[4], _fvector *v)
{
    int i = hoserFindFreeBookmark(h);
    HoserBookmark *b;

    if (i < 0)
        return i;
    D_006F8D80 = h->bm[h->cur].a;
    D_006F8D84 = h->bm[h->cur].b;
    if (D_006F8D80 != -1)
        h->bm[D_006F8D80].b = i;
    b = &h->bm[i];
    h->bm[h->cur].a = i;
    b->a = D_006F8D80;
    b->b = h->cur;
    b->state = 1;
    *(FVECTOR *)b->vec = *v;
    matCopy(b->mat, m);
    h->cur = i;
    h->count++;
    return i;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hoser", hoserAddBookmark__FP9HoserInfoPA3_fP8_fvector);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 4/71 words, size 0xe4 vs 0x11c */
/* Unlinks the current bookmark (never the first one) and moves to the previous; 1 if deleted, -1 if not. */
int hoserDeleteBookmark(HoserInfo *h)
{
    HoserBookmark *b;

    if (!h->cur)
        return -1;
    D_006F8D84 = h->bm[h->cur].b;
    D_006F8D80 = h->bm[h->cur].a;
    if (D_006F8D80 != -1)
        h->bm[D_006F8D80].b = D_006F8D84;
    if (D_006F8D84 != -1)
        h->bm[D_006F8D84].a = D_006F8D80;
    b = &h->bm[h->cur];
    b->state = -1;
    b->a = -1;
    b->b = -1;
    mathfUnitMatrix(b->mat);
    b->vec[0] = 0;
    b->vec[1] = 0;
    b->vec[2] = 0;
    h->cur = D_006F8D84;
    h->count--;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hoser", hoserDeleteBookmark__FP9HoserInfo);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 32/55 words, size 0xdc vs 0xdc */
/* 32/55 words: gcc turns the loop into a count-down; retail keeps an end-pointer compare */
int hoserDeleteAllBookmarks(HoserInfo *info)
{
    int i;

    info->cur = 0;
    info->f50 = 0;
    info->f54 = 0;
    info->f58 = 0;
    info->bm[0].state = 1;
    info->bm[0].b = -1;
    info->count = 1;
    info->bm[0].a = -1;
    mathfUnitMatrix(info->bm[0].mat);
    info->bm[0].vec[0] = 0;
    info->bm[0].vec[1] = 0;
    info->bm[0].vec[2] = 0;
    for (i = 1; i < 50; i++) {
        info->bm[i].state = -1;
        info->bm[i].a = -1;
        info->bm[i].b = -1;
        mathfUnitMatrix(info->bm[i].mat);
        info->bm[i].vec[0] = 0;
        info->bm[i].vec[1] = 0;
        info->bm[i].vec[2] = 0;
    }
    D_006F8D80 = 0;
    D_006F8D84 = 0;
    D_006F8D88 = 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hoser", hoserDeleteAllBookmarks__FP9HoserInfo);
#endif
int hoserGotoBookmark(HoserInfo *info)
{
    return 0;
}
int hoserNextBookmark(HoserInfo *info)
{
    D_006F8D80 = info->bm[info->cur].a;
    if (D_006F8D80 != -1)
        info->cur = D_006F8D80;
    return D_006F8D80;
}
int hoserPrevBookmark(HoserInfo *info)
{
    D_006F8D84 = info->bm[info->cur].b;
    if (D_006F8D84 != -1)
        info->cur = D_006F8D84;
    return D_006F8D84;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 4/42 words, size 0xa0 vs 0xa8 */
/* Makes the bookmark nearest to p current; returns it (-1 if there are none). */
int hoserClosestBookmark(HoserInfo *h, _fvector *p)
{
    float best = 9999999.0f;
    int idx = -1;
    int i;

    for (i = 0; i < 50; i++) {
        FVECTOR d;
        float l;

        if (h->bm[i].state == -1)
            continue;
        vecSub(&d, p, (_fvector *)h->bm[i].vec);
        l = eeSqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        if (l < best) {
            best = l;
            idx = i;
        }
    }
    if (idx != -1)
        h->cur = idx;
    return idx;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/hoser", hoserClosestBookmark__FP9HoserInfoP8_fvector);
#endif
HoserInfo *hoserGetHoserInfo(void)
{
    return &D_00778010;
}
