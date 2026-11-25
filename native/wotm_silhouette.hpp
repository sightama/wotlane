// ABOUTME: Shadow silhouettes drawn on the GPU (WOTM_GPUSIL): the blocker's triangles into a small render texture in the light frame,
// ABOUTME: then the same two 3x3 box passes as the CPU path (Scene::shadowTexture), with the view's framebuffer saved and put back.
#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "wotm_glsl.hpp"
#include <vector>

extern "C" {
extern void (*glad_glGetIntegerv)(unsigned, int *);
}

namespace wotm {

// The framebuffer and viewport a view draws into (Scene::draw runs inside its BeginTextureMode), read before drawing elsewhere and
// put back after. raylib's EndTextureMode would bind the window instead, so the silhouettes switch with rlEnableFramebuffer.
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

class GpuSilhouette {
public:
    // false when the blur shader did not load: the caller keeps the CPU path
    bool ready(int n) {
        if (tried_) return ok_;
        tried_ = true;
        static const char *fs =
            "#version 330\n"
            "in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; uniform float side; out vec4 finalColor;\n"
            // a 3x3 box of the coverage (alpha); the border texels stay 0, as on the CPU (CLAMP then doesn't extend the shadow)
            "void main() { ivec2 p = ivec2(gl_FragCoord.xy); int n = int(side);\n"
            "  if (p.x < 1 || p.y < 1 || p.x > n - 2 || p.y > n - 2) { finalColor = vec4(0.0); return; }\n"
            "  float s = 0.0;\n"
            "  for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) s += texelFetch(texture0, p + ivec2(dx, dy), 0).a;\n"
            "  finalColor = vec4(0.0, 0.0, 0.0, s / 9.0); }\n";
        blur_ = LoadShaderFromMemory(nullptr, wotm_glsl::forTarget(fs, WOTM_GLSL_ES).c_str());
        // a shader that doesn't compile comes back as raylib's default one
        if (!IsShaderValid(blur_) || blur_.id == rlGetShaderIdDefault()) return ok_ = false;
        sideLoc_ = GetShaderLocation(blur_, "side");
        tmp_ = make(n);
        return ok_ = tmp_.id != 0;
    }

    // a target for one silhouette (n x n, RGBA8; drawn bilinear and clamped, like the CPU texture)
    static RenderTexture2D make(int n) {
        RenderTexture2D rt = LoadRenderTexture(n, n);
        if (rt.id) { SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR); SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP); }
        return rt;
    }

    // Draws one silhouette into `rt`: `xy` holds the triangles in texels (x, y per corner, 6 floats a triangle; y = texture row),
    // covering what they touch except the 1-texel border, then the two box passes. The caller has flushed the batch and saved the
    // view's target (GlTarget), and puts it back after its last silhouette.
    void render(RenderTexture2D &rt, const std::vector<float> &xy, int n) {
        auto into = [&](const RenderTexture2D &t) {
            rlEnableFramebuffer(t.id);
            rlViewport(0, 0, n, n);
        };
        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlLoadIdentity();
        rlOrtho(0, n, 0, n, -1, 1);   // y up: row 0 of the texture is y = 0, as in the CPU array
        rlMatrixMode(RL_MODELVIEW);
        rlPushMatrix();
        rlLoadIdentity();
        rlDisableDepthTest();
        rlDisableColorBlend();   // coverage and the passes replace, they don't blend

        into(rt);
        rlClearColor(0, 0, 0, 0);
        rlClearScreenBuffers();
        rlEnableScissorTest();
        rlScissor(1, 1, n - 2, n - 2);   // the CPU raster leaves the border at 0
        rlBegin(RL_TRIANGLES);
        rlColor4ub(0, 0, 0, 255);
        for (size_t k = 0; k + 6 <= xy.size(); k += 6)
            for (int i = 0; i < 3; ++i) rlVertex2f(xy[k + 2 * i], xy[k + 2 * i + 1]);
        rlEnd();
        rlDrawRenderBatchActive();
        rlDisableScissorTest();

        const float side = float(n);
        SetShaderValue(blur_, sideLoc_, &side, SHADER_UNIFORM_FLOAT);
        auto pass = [&](const RenderTexture2D &from, const RenderTexture2D &to) {
            into(to);
            BeginShaderMode(blur_);
            rlSetTexture(from.texture.id);
            rlBegin(RL_QUADS);
            rlColor4ub(255, 255, 255, 255);
            rlTexCoord2f(0, 0); rlVertex2f(0, 0);
            rlTexCoord2f(1, 0); rlVertex2f(side, 0);
            rlTexCoord2f(1, 1); rlVertex2f(side, side);
            rlTexCoord2f(0, 1); rlVertex2f(0, side);
            rlEnd();
            rlDrawRenderBatchActive();
            rlSetTexture(0);
            EndShaderMode();
        };
        pass(rt, tmp_);
        pass(tmp_, rt);

        rlEnableColorBlend();
        rlEnableDepthTest();
        rlMatrixMode(RL_MODELVIEW);
        rlPopMatrix();
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();
        rlMatrixMode(RL_MODELVIEW);
    }

private:
    bool tried_ = false, ok_ = false;
    Shader blur_{};
    int sideLoc_ = -1;
    RenderTexture2D tmp_{};
};

}  // namespace wotm
