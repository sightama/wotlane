// ABOUTME: Shadow silhouettes drawn on the GPU (WOTM_GPUSIL): the blockers' triangles in the light frame, then the same two 3x3 box
// ABOUTME: passes as the CPU path (Scene::shadowTexture); a redraw's silhouettes share two atlases, with the view's framebuffer put back.
#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "wotm_glsl.hpp"
#include <algorithm>
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
            "in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; uniform float side; uniform vec2 from; out vec4 finalColor;\n"
            // a 3x3 box of the coverage (alpha), each silhouette (an n x n tile) on its own: its border texels stay 0, as on the CPU
            // (CLAMP then doesn't extend the shadow), so no texel reads past its tile. `from` is where the tile starts in texture0
            // when the target holds only that silhouette (0 when both are atlases)
            "void main() { ivec2 p = ivec2(gl_FragCoord.xy); int n = int(side); ivec2 l = p - (p / n) * n;\n"
            "  if (l.x < 1 || l.y < 1 || l.x > n - 2 || l.y > n - 2) { finalColor = vec4(0.0); return; }\n"
            "  ivec2 q = p + ivec2(from); float s = 0.0;\n"
            "  for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) s += texelFetch(texture0, q + ivec2(dx, dy), 0).a;\n"
            "  finalColor = vec4(0.0, 0.0, 0.0, s / 9.0); }\n";
        blur_ = LoadShaderFromMemory(nullptr, wotm_glsl::forTarget(fs, WOTM_GLSL_ES).c_str());
        // a shader that doesn't compile comes back as raylib's default one
        if (!IsShaderValid(blur_) || blur_.id == rlGetShaderIdDefault()) return ok_ = false;
        sideLoc_ = GetShaderLocation(blur_, "side");
        fromLoc_ = GetShaderLocation(blur_, "from");
        cover_ = LoadRenderTexture(kTiles * n, kTiles * n);
        pass1_ = LoadRenderTexture(kTiles * n, kTiles * n);
        return ok_ = cover_.id != 0 && pass1_.id != 0;
    }

    // a target for one silhouette (n x n, RGBA8; drawn bilinear and clamped, like the CPU texture)
    static RenderTexture2D make(int n) {
        RenderTexture2D rt = LoadRenderTexture(n, n);
        if (rt.id) { SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR); SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP); }
        return rt;
    }

    // Draws `count` silhouettes: job(i) gives the i-th one's target and its triangles in texels (x, y per corner, 6 floats a triangle;
    // y = texture row), covering what they touch except the 1-texel border, then the two box passes. Up to kTiles^2 at a time share
    // the coverage atlas and the first pass's; only the second pass draws into each target. The caller has flushed the batch and saved
    // the view's target (GlTarget) before making any render texture, and puts it back after, with its own shader (this leaves raylib's
    // default one on).
    template <class Job>
    void render(size_t count, int n, Job job) {
        // the view's matrices are kept aside and put back as they were: rlPushMatrix on the modelview only switches to rlgl's per-vertex
        // transform, the camera's matrix stays in the modelview the batch draws with
        const Matrix viewProj = rlGetMatrixProjection(), viewModel = rlGetMatrixModelview();
        rlSetMatrixModelview(MatrixIdentity());
        rlDisableDepthTest();
        rlDisableColorBlend();   // coverage and the passes replace, they don't blend
        rlDisableBackfaceCulling();   // the silhouette takes every triangle, whichever way it faces
        const float side = float(n), atlas = float(kTiles * n), none[2] = {0.f, 0.f};
        SetShaderValue(blur_, sideLoc_, &side, SHADER_UNIFORM_FLOAT);

        auto into = [&](const RenderTexture2D &t, int w) {
            rlEnableFramebuffer(t.id);
            rlViewport(0, 0, w, w);
            rlSetMatrixProjection(MatrixOrtho(0, w, 0, w, -1, 1));   // y up: row 0 of the texture is y = 0, as in the CPU array
        };
        auto quad = [&](unsigned tex, float w) {
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
        };
        constexpr size_t perAtlas = size_t(kTiles * kTiles);
        for (size_t first = 0; first < count; first += perAtlas) {
            const size_t last = std::min(count, first + perAtlas);
            // coverage: each silhouette in its tile, clipped to the tile's inside (the CPU raster leaves the border at 0). The viewport
            // moves the tile, not the corners: adding the offset to them would round the corners differently from the CPU raster
            rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());   // the view's draw may have its own on
            into(cover_, kTiles * n);
            rlClearColor(0, 0, 0, 0);
            rlClearScreenBuffers();
            rlSetMatrixProjection(MatrixOrtho(0, n, 0, n, -1, 1));
            rlEnableScissorTest();
            for (size_t i = first; i < last; ++i) {
                const std::vector<float> &xy = job(i).second;
                const int ox = int((i - first) % kTiles) * n, oy = int((i - first) / kTiles) * n;
                rlViewport(ox, oy, n, n);
                rlScissor(ox + 1, oy + 1, n - 2, n - 2);
                rlBegin(RL_TRIANGLES);
                rlColor4ub(0, 0, 0, 255);
                for (size_t k = 0; k + 6 <= xy.size(); k += 6)
                    for (int c = 0; c < 3; ++c) rlVertex2f(xy[k + 2 * c], xy[k + 2 * c + 1]);
                rlEnd();
                rlDrawRenderBatchActive();
            }
            rlDisableScissorTest();

            // first pass: the whole coverage atlas into the other one
            into(pass1_, kTiles * n);
            SetShaderValue(blur_, fromLoc_, none, SHADER_UNIFORM_VEC2);
            BeginShaderMode(blur_);
            quad(cover_.texture.id, atlas);
            EndShaderMode();

            // second pass: each silhouette's tile into its own target
            for (size_t i = first; i < last; ++i) {
                const float from[2] = {float(int((i - first) % kTiles) * n), float(int((i - first) / kTiles) * n)};
                into(job(i).first, n);
                SetShaderValue(blur_, fromLoc_, from, SHADER_UNIFORM_VEC2);
                BeginShaderMode(blur_);
                quad(pass1_.texture.id, side);
                EndShaderMode();
            }
        }

        rlEnableColorBlend();
        rlEnableBackfaceCulling();
        rlEnableDepthTest();
        rlSetMatrixProjection(viewProj);
        rlSetMatrixModelview(viewModel);
    }

private:
    static constexpr int kTiles = 4;   // an atlas holds 4 x 4 silhouettes (a redraw makes at most one per blocker and view)
    bool tried_ = false, ok_ = false;
    Shader blur_{};
    int sideLoc_ = -1, fromLoc_ = -1;
    RenderTexture2D cover_{}, pass1_{};
};

}  // namespace wotm
