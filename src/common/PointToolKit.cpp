#include "common.h"
#include "point_tool_kit.h"
#include "memory_stack.h"

extern "C" int printf(const char *fmt, ...);
extern "C" void *memcpy(void *d, const void *s, unsigned n);
int fileReadf(char *name, void *buf);
float *viewGetMat(int view);
float *viewGetTrans(int view);

void PointToolKit::init(int v)
{
    view = v;
    numPoints = 0;
    mat = viewGetMat(v);
    trans = viewGetTrans(v);
    f4030 = 0;
    f4010 = 25000000.0f;
    pointHeight = 10.0f;
    pointWidth = 10.0f;
    pointMoveStep = 2.0f;
    reticleMoveStep = 0.0008f;
    resetReticle();
}
#ifdef NON_MATCHING
/* equivalent (untuned): 5/88 words, size 0x144 vs 0x160 */
/* A point file: version word 0x12282000, the count, then 64-byte points. The buffer comes from the high end of the global
   MemoryStack (released only on success, as in retail). notify: call the derived tool's "points loaded" virtual. */
void PointToolKit::loadPoints(char *name, bool notify)
{
    unsigned *buf;
    int r;

    numPoints = 0;
    MemoryStack::global.pushHighMark();
    buf = (unsigned *)MemoryStack::global.allocHigh16(0x4000);
    if (!buf) {
        printf("PointToolKit could not create load buffer, you will get 0 points\n");
    } else {
        r = fileReadf(name, buf);
        if (r == -2 || r == 0) {
            printf("PointToolKit file does not exist, you will get 0 points\n");
        } else if (buf[0] != 0x12282000) {
            printf("PointToolKit file is of version %8x, need %8x\n", buf[0], 0x12282000);
        } else {
            numPoints = buf[1];
            memcpy(points, buf + 2, buf[1] << 6);
            MemoryStack::global.popHighMark();
        }
    }
    if (notify) {
        struct VtEntry {
            short delta;
            short pad;
            void (*fn)(void *);
        } *vt = (VtEntry *)vptr;

        vt[3].fn((char *)this + vt[3].delta);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointToolKit", loadPoints__12PointToolKitPcb);
#endif
INCLUDE_ASM("asm/nonmatchings/common/PointToolKit", _vt$12PointToolKit);
#ifdef NON_MATCHING
/* equivalent (untuned): 0/7 words, size 0x18 vs 0x1c */
/* 0/7 words: retail writes reticle through a derived pointer (inline vector setter) */
void PointToolKit::resetReticle(void)
{
    reticleX = 0;
    reticleY = 10.0f;
    reticleZ = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointToolKit", resetReticle__12PointToolKit);
#endif
PointToolKit::Point *PointToolKit::getPoint(int i)
{
    return &points[i];
}
int PointToolKit::getPathType(Point *a, Point *b)
{
    int r = hasPath(a, b);

    return r + hasPath(b, a) * 2;
}
int PointToolKit::hasPath(Point *a, Point *b)
{
    int idx = getPointIndex(b);
    int i;

    for (i = 0; i < a->numPaths; i++) {
        if (a->paths[i] == idx)
            return 1;
    }
    return 0;
}
int PointToolKit::getPointIndex(Point *p)
{
    return p - points;
}
INCLUDE_ASM("asm/nonmatchings/common/PointToolKit", __tf12PointToolKit);
INCLUDE_ASM("asm/nonmatchings/common/PointToolKit", __12PointToolKit);
void PointToolKit::setPointHeight(float h)
{
    pointHeight = h * 0.5f;
}
void PointToolKit::setPointWidth(float w)
{
    pointWidth = w * 0.5f;
}
void PointToolKit::setPointMoveStep(float s)
{
    pointMoveStep = s;
}
void PointToolKit::setReticleMoveStep(float s)
{
    reticleMoveStep = s;
}
void exportPoints__12PointToolKit(void *self) __asm__("exportPoints__12PointToolKit");
void exportPoints__12PointToolKit(void *self)
{
}
int PointToolKit::getNumPoints(void)
{
    return numPoints;
}
