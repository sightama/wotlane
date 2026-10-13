#include "common.h"
#include "hieri_types.h"

/* Screen-space textured quads (HUD, menus): each source object owns a slice of a GS packet buffer, polygons are appended
   to it during the frame (addPoly) and the packets are sent at the end (dmaData). Positions are relative to the view
   and can be centered, mirrored or flipped per source or per polygon (resolveFX). */

class ScreenPolys {
public:
    /* One quad: screen rectangle and texture rectangle. */
    struct PolyData {
        int x0, y0, x1, y1;     // 0x0
        int u0, v0, u1, v1;     // 0x10
        unsigned short fx;      // 0x20 FX_* bits, combined with the source's
    };

    enum {
        FX_FLIP_U = 0x2,        // swap u0/u1
        FX_FLIP_V = 0x4,        // swap v0/v1
        FX_MIRROR_X = 0x8,      // x measured from the right edge of the view
        FX_CENTER = 0x10,       // (x, y) is the quad's center
    };

    static void clearSources(void) __asm__("clearSources__11ScreenPolysv");
    static int initSourceObject(_hierobject *obj, int qwords);
    static void dmaData(void) __asm__("dmaData__11ScreenPolysv");
    static void setColor(int src, int r, int g, int b, int a);
    static void getColor(int src, int &r, int &g, int &b, int &a);
    static void setDefaultColor(int src);
    static void dimColor(int src);
    static void setHilightColor(int src);
    static void setSourceFX(int src, unsigned short fx);
    static void addPoly(int src, int x, int y, PolyData &pd);
    static void addFractionalPoly(float fx, float fy, int src, int x, int y, PolyData &pd);
};
typedef char _size_PolyData[sizeof(ScreenPolys::PolyData) == 0x24 ? 1 : -1];

/* A source object: its packet slice and the color/fx its quads get. */
struct SourceObj {
    QwData *pkt;            // 0x0 start of its packet
    QwData *end;            // 0x4
    int numPolys;           // 0x8 quads added this frame
    int headerQw;           // 0xC qwords of the packet header (polyInitPacket)
    int curQw;              // 0x10 write position
    _hierobject *obj;       // 0x14 texture
    unsigned char r, g, b, a;  // 0x18
    unsigned short fx;      // 0x1C
};
typedef char _size_SourceObj[sizeof(SourceObj) == 0x20 ? 1 : -1];

/* A looping flip-book of quads: frame changes cross-fade over the last 60% of a frame's ticks. */
class SpriteAnimation {
public:
    int src;                            // 0x0 source object
    ScreenPolys::PolyData frames[10];   // 0x4
    int tick;                           // 0x16C
    int ticksPerFrame;                  // 0x170
    int numFrames;                      // 0x174

    SpriteAnimation();
    void clear(void);
    void addFrame(ScreenPolys::PolyData &pd);
    void draw(int x, int y, bool mirror);
    void crossfadeGraphics(bool mirror, int x, int y);
};

extern SourceObj s_sourceObjects[];
extern int s_numSourceObjects;
__asm__("#SNFIX_SMALL s_numSourceObjects");
extern int s_allocatedBufferSpace;
__asm__("#SNFIX_SMALL s_allocatedBufferSpace");
extern QwData s_polyBuffer[] __asm__("D_00793980");

int viewGetCurView(void);
void viewGetCenter(int view, int *x, int *y);
void viewGetWH(int view, int *w, int *h);
void polyBuildPrim(int src, int x, int y, ScreenPolys::PolyData &pd, QwData *pkt);
void resolveFX(int src, int x, int y, ScreenPolys::PolyData &out, ScreenPolys::PolyData &in);
void polyInitPacket(_hierobject *obj, SourceObj *s, QwData *pkt);

void ScreenPolys::clearSources(void)
{
    s_numSourceObjects = 0;
    s_allocatedBufferSpace = 0;
}

#ifdef NON_MATCHING
/* equivalent (untuned): 34/41 words, size 0xa4 vs 0xa0 */
/* Gives obj a source object with room for qwords of packet; returns its index. */
int ScreenPolys::initSourceObject(_hierobject *obj, int qwords)
{
    SourceObj *s = &s_sourceObjects[s_numSourceObjects];
    QwData *pkt = &s_polyBuffer[s_allocatedBufferSpace];

    s->obj = obj;
    s->pkt = pkt;
    s->end = pkt + qwords;
    s->numPolys = 0;
    polyInitPacket(obj, s, pkt);
    s->curQw = s->headerQw;
    s_allocatedBufferSpace += qwords;
    setDefaultColor(s_numSourceObjects);
    return s_numSourceObjects++;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", initSourceObject__11ScreenPolysP11_hierobjecti);
#endif

/* packet tag, cache flush and the GIF DMA kick: hardware */
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", dmaData__11ScreenPolysv);

#ifdef NON_MATCHING
/* equivalent (untuned): 5/9 words, size 0x24 vs 0x24 */
void ScreenPolys::setColor(int src, int r, int g, int b, int a)
{
    SourceObj *s = &s_sourceObjects[src];

    s->a = a;
    s->r = r;
    s->g = g;
    s->b = b;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", setColor__11ScreenPolysiiiii);
#endif

void ScreenPolys::getColor(int src, int &r, int &g, int &b, int &a)
{
    SourceObj *s = &s_sourceObjects[src];

    r = s->r;
    g = s->g;
    b = s->b;
    a = s->a;
}

void ScreenPolys::setDefaultColor(int src)
{
    setColor(src, 0x80, 0x80, 0x80, 0x80);
}

void ScreenPolys::dimColor(int src)
{
    SourceObj *s = &s_sourceObjects[src];

    s->r >>= 1;
    s->g >>= 1;
    s->b >>= 1;
}

#ifdef NON_MATCHING
/* equivalent (untuned): 7/11 words, size 0x2c vs 0x2c */
void ScreenPolys::setHilightColor(int src)
{
    SourceObj *s = &s_sourceObjects[src];

    s->r = 0xFF;
    s->b = 0x80;
    s->g = 0xC4;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", setHilightColor__11ScreenPolysi);
#endif

void ScreenPolys::setSourceFX(int src, unsigned short fx)
{
    s_sourceObjects[src].fx = fx;
}

void ScreenPolys::addPoly(int src, int x, int y, PolyData &pd)
{
    if (src < s_numSourceObjects && src >= 0)
        polyBuildPrim(src, x, y, pd, s_sourceObjects[src].pkt);
}

#ifdef NON_MATCHING
/* equivalent (untuned): 32/67 words, size 0x10c vs 0x108 */
/* The part [0, fx] x [0, fy] of pd (a filling bar), texture cropped the same way. */
void ScreenPolys::addFractionalPoly(float fx, float fy, int src, int x, int y, PolyData &pd)
{
    PolyData p;

    if (fx <= 0.0f || fy <= 0.0f)
        return;
    p.x0 = pd.x0;
    p.y0 = pd.y0;
    p.x1 = pd.x0 + (int)((float)(pd.x1 - pd.x0) * fx);
    p.y1 = pd.y0 + (int)((float)(pd.y1 - pd.y0) * fy);
    p.u0 = pd.u0;
    p.v0 = pd.v0;
    p.u1 = pd.u0 + (int)((float)(pd.u1 - pd.u0) * fx);
    p.v1 = pd.v0 + (int)((float)(pd.v1 - pd.v0) * fy);
    p.fx = pd.fx;
    addPoly(src, x, y, p);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", addFractionalPoly__11ScreenPolysffiiiRQ211ScreenPolys8PolyData);
#endif

/* RGBAQ, UV and XYZ2 qwords of one sprite: hardware (GS packet) */
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", polyBuildPrim__FiiiRQ211ScreenPolys8PolyDataP6QwData);

#ifdef NON_MATCHING
/* equivalent (untuned): 110/116 words, size 0x1d0 vs 0x1d0 */
/* Places quad in at (x, y) relative to the top left of the current view, applying the source's and the quad's fx. */
void resolveFX(int src, int x, int y, ScreenPolys::PolyData &out, ScreenPolys::PolyData &in)
{
    SourceObj *s = &s_sourceObjects[src];
    int fx = (s->fx & (ScreenPolys::FX_FLIP_U | ScreenPolys::FX_MIRROR_X)) ^ ((s->fx & ScreenPolys::FX_CENTER) | in.fx);
    int cx, cy, w, h;
    int ox, oy;

    viewGetCenter(viewGetCurView(), &cx, &cy);
    viewGetWH(viewGetCurView(), &w, &h);
    if (fx & ScreenPolys::FX_CENTER) {
        x -= (in.x1 - in.x0) / 2;
        y -= (in.y1 - in.y0) / 2;
    }
    if (fx & ScreenPolys::FX_MIRROR_X)
        ox = cx + w / 2 - (x + (in.x1 - in.x0));
    else
        ox = x + (cx - w / 2);
    oy = y + (cy - h / 2);
    if (fx & ScreenPolys::FX_FLIP_U) {
        out.u0 = in.u1;
        out.u1 = in.u0;
    } else {
        out.u0 = in.u0;
        out.u1 = in.u1;
    }
    if (fx & ScreenPolys::FX_FLIP_V) {
        out.v0 = in.v1;
        out.v1 = in.v0;
    } else {
        out.v0 = in.v0;
        out.v1 = in.v1;
    }
    out.x0 = in.x0 + ox;
    out.x1 = in.x1 + ox;
    out.y0 = in.y0 + oy;
    out.y1 = in.y1 + oy;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", resolveFX__FiiiRQ211ScreenPolys8PolyDataT3);
#endif

/* DMA tag and GS register setup (texture of obj, alpha, test) at the start of the packet: hardware */
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", polyInitPacket__FP11_hierobjectP9SourceObjP6QwData);

SpriteAnimation::SpriteAnimation()
{
    clear();
}

#ifdef NON_MATCHING
/* equivalent (untuned): 5/7 words, size 0x1c vs 0x1c */
void SpriteAnimation::clear(void)
{
    src = -1;
    ticksPerFrame = 1;
    numFrames = 0;
    tick = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", clear__15SpriteAnimation);
#endif

void SpriteAnimation::addFrame(ScreenPolys::PolyData &pd)
{
    frames[numFrames] = pd;
    numFrames++;
}

void SpriteAnimation::draw(int x, int y, bool mirror)
{
    crossfadeGraphics(mirror, x, y);
    tick = (tick + 1) % (ticksPerFrame * numFrames);
}

#ifdef NON_MATCHING
/* equivalent (untuned): 26/116 words, size 0x1c8 vs 0x1d0 */
/* Draws the current frame fading out and the next one fading in, with the source's alpha scaled; the color is restored. */
void SpriteAnimation::crossfadeGraphics(bool mirror, int x, int y)
{
    int frame = tick / ticksPerFrame;
    float f = (float)(tick % ticksPerFrame) / (float)ticksPerFrame;
    int next = (frame + 1) % numFrames;
    float out = (f - 0.4f) / 0.6f;
    float in = f / 0.6f;
    int r, g, b, a;

    if (out < 0.0f)
        out = 0.0f;
    if (!(in < 1.0f))
        in = 1.0f;
    frames[frame].fx = mirror * (ScreenPolys::FX_FLIP_U | ScreenPolys::FX_MIRROR_X);
    frames[next].fx = mirror * (ScreenPolys::FX_FLIP_U | ScreenPolys::FX_MIRROR_X);
    ScreenPolys::getColor(src, r, g, b, a);
    ScreenPolys::setColor(src, r, g, b, (int)(128.0f - out * 128.0f));
    ScreenPolys::addPoly(src, x, y, frames[frame]);
    ScreenPolys::setColor(src, r, g, b, (int)(in * 128.0f));
    ScreenPolys::addPoly(src, x, y, frames[next]);
    ScreenPolys::setColor(src, r, g, b, a);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/ScreenPolys", crossfadeGraphics__15SpriteAnimationbii);
#endif
