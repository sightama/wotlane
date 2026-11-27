// ABOUTME: Drawing into a render texture from inside a view's draw (GPU shadow silhouettes, GPU dust): the view's framebuffer, viewport
// ABOUTME: and matrices kept aside and put back, and the target and whole-texture quad those passes draw with.
#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

extern "C" {
extern void (*glad_glGetIntegerv)(unsigned, int *);
}

namespace wotm {

// The framebuffer and viewport a view draws into (Scene::draw runs inside its BeginTextureMode), read before drawing elsewhere and
// put back after. raylib's EndTextureMode would bind the window instead, so the passes switch with rlEnableFramebuffer.
struct GlTarget {
    int fbo = 0, vp[4] = {};
    static GlTarget current() {
        GlTarget t;
        glad_glGetIntegerv(0x8CA6 /* GL_FRAMEBUFFER_BINDING */, &t.fbo);
        glad_glGetIntegerv(0x0BA2 /* GL_VIEWPORT */, t.vp);
        return t;
    }
    void restore() const {
        rlEnableFramebuffer(unsigned(fbo));
        rlViewport(vp[0], vp[1], vp[2], vp[3]);
    }
};

// The GL state of an offscreen pass while it lives: no depth test, no blending (the passes replace) and no culling (a silhouette takes
// every triangle, whichever way it faces). The view's matrices are kept aside and put back as they were: rlPushMatrix on the modelview
// only switches to rlgl's per-vertex transform, the camera's matrix stays in the modelview the batch draws with. The framebuffer is
// the caller's (GlTarget).
class OffscreenPass {
public:
    OffscreenPass() : viewProj_(rlGetMatrixProjection()), viewModel_(rlGetMatrixModelview()) {
        rlSetMatrixModelview(MatrixIdentity());
        rlDisableDepthTest();
        rlDisableColorBlend();
        rlDisableBackfaceCulling();
    }
    ~OffscreenPass() {
        rlEnableColorBlend();
        rlEnableBackfaceCulling();
        rlEnableDepthTest();
        rlSetMatrixProjection(viewProj_);
        rlSetMatrixModelview(viewModel_);
    }
    OffscreenPass(const OffscreenPass &) = delete;
    OffscreenPass &operator=(const OffscreenPass &) = delete;

    // draws into the framebuffer `fbo`, w x w texels, y up: row 0 of the texture is y = 0, as in the CPU arrays
    static void into(unsigned fbo, int w) {
        rlEnableFramebuffer(fbo);
        rlViewport(0, 0, w, w);
        rlSetMatrixProjection(MatrixOrtho(0, w, 0, w, -1, 1));
    }

    // the texture `tex` over the whole w x w target, drawn at once
    static void quad(unsigned tex, float w) {
        rlSetTexture(tex);
        rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(0, 0); rlVertex2f(0, 0);
        rlTexCoord2f(1, 0); rlVertex2f(w, 0);
        rlTexCoord2f(1, 1); rlVertex2f(w, w);
        rlTexCoord2f(0, 1); rlVertex2f(0, w);
        rlEnd();
        rlDrawRenderBatchActive();
        rlSetTexture(0);
    }

private:
    const Matrix viewProj_, viewModel_;
};

}  // namespace wotm
