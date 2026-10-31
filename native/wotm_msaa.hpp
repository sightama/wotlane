// wotm_msaa.hpp: MSAA for the native frame (ANTI-ALIASING option on the VIDEO tab, WOTM_MSAA). raylib only does single-sample render
// textures; here a framebuffer with multisample color and depth receives the drawing (BeginTextureMode(target.render())) and, at the end,
// is resolved (glBlitFramebuffer) into the usual frame texture, which the presentation, the snapshots and texId 0 (previous frame) keep using.
// The lens flare occlusion test reads depth with glReadPixels, which can't read from a multisample framebuffer: readDepth() resolves
// the depth into a single-sample framebuffer and leaves it bound for reading (drawing continues in the multisample one).
// The GL 3 functions come from the glad pointers raylib loads (static raylib, C symbols).
#pragma once

#include <algorithm>
#include <cstdio>

#include "raylib.h"
#include "rlgl.h"

extern "C" {
extern void (*glad_glGenFramebuffers)(int, unsigned *);
extern void (*glad_glDeleteFramebuffers)(int, const unsigned *);
extern void (*glad_glBindFramebuffer)(unsigned, unsigned);
extern unsigned (*glad_glCheckFramebufferStatus)(unsigned);
extern void (*glad_glGenRenderbuffers)(int, unsigned *);
extern void (*glad_glDeleteRenderbuffers)(int, const unsigned *);
extern void (*glad_glBindRenderbuffer)(unsigned, unsigned);
extern void (*glad_glRenderbufferStorage)(unsigned, unsigned, int, int);
extern void (*glad_glRenderbufferStorageMultisample)(unsigned, int, unsigned, int, int);
extern void (*glad_glFramebufferRenderbuffer)(unsigned, unsigned, unsigned, unsigned);
extern void (*glad_glBlitFramebuffer)(int, int, int, int, int, int, int, int, unsigned, unsigned);
extern void (*glad_glGetIntegerv)(unsigned, int *);
extern void (*glad_glDrawBuffer)(unsigned);
extern void (*glad_glReadBuffer)(unsigned);
}

namespace wotm::msaa
{
constexpr unsigned kFramebuffer = 0x8D40, kReadFb = 0x8CA8, kDrawFb = 0x8CA9, kRenderbuffer = 0x8D41, kColor0 = 0x8CE0, kDepthAtt = 0x8D00,
                   kRgba8 = 0x8058, kDepth24 = 0x81A6, kColorBit = 0x4000, kDepthBit = 0x100, kNearest = 0x2600, kComplete = 0x8CD5,
                   kMaxSamples = 0x8D57;

// The driver's maximum sample count (0 = no MSAA)
inline int maxSamples()
{
    static int m = -1;
    if (m < 0)
    {
        m = 0;
        if (glad_glGetIntegerv) glad_glGetIntegerv(kMaxSamples, &m);
    }
    return m;
}

struct Target
{
    unsigned fbo = 0, color = 0, depth = 0;   // multisample
    unsigned dfbo = 0, ddepth = 0;            // resolved depth (lens flare test)
    int w = 0, h = 0, samples = 0;

    bool on() const { return fbo != 0; }
    void release()
    {
        if (fbo) glad_glDeleteFramebuffers(1, &fbo);
        if (dfbo) glad_glDeleteFramebuffers(1, &dfbo);
        if (color) glad_glDeleteRenderbuffers(1, &color);
        if (depth) glad_glDeleteRenderbuffers(1, &depth);
        if (ddepth) glad_glDeleteRenderbuffers(1, &ddepth);
        *this = Target{};
    }
    // Ready for w x h with `samples` samples (capped at the driver's); samples <= 1 frees everything and returns false
    bool ensure(int nw, int nh, int ns, const char *what = "frame")
    {
        ns = std::min(ns, maxSamples());
        if (ns <= 1 || nw <= 0 || nh <= 0 || !glad_glRenderbufferStorageMultisample || !glad_glBlitFramebuffer)
        {
            if (fbo) release();
            return false;
        }
        if (fbo && w == nw && h == nh && samples == ns) return true;
        release();
        rlDrawRenderBatchActive();
        glad_glGenFramebuffers(1, &fbo);
        glad_glGenRenderbuffers(1, &color);
        glad_glGenRenderbuffers(1, &depth);
        glad_glBindRenderbuffer(kRenderbuffer, color);
        glad_glRenderbufferStorageMultisample(kRenderbuffer, ns, kRgba8, nw, nh);
        glad_glBindRenderbuffer(kRenderbuffer, depth);
        glad_glRenderbufferStorageMultisample(kRenderbuffer, ns, kDepth24, nw, nh);
        glad_glBindFramebuffer(kFramebuffer, fbo);
        glad_glFramebufferRenderbuffer(kFramebuffer, kColor0, kRenderbuffer, color);
        glad_glFramebufferRenderbuffer(kFramebuffer, kDepthAtt, kRenderbuffer, depth);
        const bool ok = glad_glCheckFramebufferStatus(kFramebuffer) == kComplete;
        // single-sample depth for glReadPixels (depth only: no color buffer to draw to or read from)
        glad_glGenFramebuffers(1, &dfbo);
        glad_glGenRenderbuffers(1, &ddepth);
        glad_glBindRenderbuffer(kRenderbuffer, ddepth);
        glad_glRenderbufferStorage(kRenderbuffer, kDepth24, nw, nh);
        glad_glBindFramebuffer(kFramebuffer, dfbo);
        glad_glFramebufferRenderbuffer(kFramebuffer, kDepthAtt, kRenderbuffer, ddepth);
        glad_glDrawBuffer(0);
        glad_glReadBuffer(0);
        const bool dok = glad_glCheckFramebufferStatus(kFramebuffer) == kComplete;
        glad_glBindRenderbuffer(kRenderbuffer, 0);
        glad_glBindFramebuffer(kFramebuffer, 0);
        w = nw;
        h = nh;
        samples = ns;
        std::fprintf(stderr, "[msaa] %s %dx%d with %d samples%s\n", what, nw, nh, ns, ok && dok ? "" : ": framebuffer incomplete, no MSAA");
        if (!ok || !dok)
        {
            release();
            return false;
        }
        return true;
    }
    // For raylib's BeginTextureMode: it only uses the id and the size
    RenderTexture2D render() const
    {
        RenderTexture2D r{};
        r.id = fbo;
        r.texture.width = w;
        r.texture.height = h;
        return r;
    }
    // After EndTextureMode: the resolved color goes into the frame texture's framebuffer (same size)
    void resolve(unsigned dstFbo) const
    {
        if (!fbo) return;
        glad_glBindFramebuffer(kReadFb, fbo);
        glad_glBindFramebuffer(kDrawFb, dstFbo);
        glad_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, kColorBit, kNearest);
        glad_glBindFramebuffer(kFramebuffer, 0);
    }
    // In the middle of drawing (multisample framebuffer in use): resolves depth and leaves it bound for reading
    void readDepth() const
    {
        if (!fbo) return;
        rlDrawRenderBatchActive();
        glad_glBindFramebuffer(kReadFb, fbo);
        glad_glBindFramebuffer(kDrawFb, dfbo);
        glad_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, kDepthBit, kNearest);
        glad_glBindFramebuffer(kDrawFb, fbo);
        glad_glBindFramebuffer(kReadFb, dfbo);
    }
};

// Multisample target in use (between its BeginTextureMode and EndTextureMode); the lens flare calls readDepth on it
inline const Target *g_current = nullptr;
inline void readDepthIfMsaa()
{
    if (g_current) g_current->readDepth();
}
} // namespace wotm::msaa
