// ABOUTME: The composite dust/smoke texture drawn on the GPU (WOTM_GPUDUST): one pass over the 64x64 target whose fragments run both
// ABOUTME: passes of wotm_dust.hpp's raster over the same sprite list, with its taps and integer math, instead of the CPU raster.
#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "wotm_dust.hpp"
#include "wotm_glsl.hpp"
#include "wotm_gpupass.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

namespace wotm {

class GpuDust {
public:
    // false when the shader or the sprite list's texture did not load: the caller keeps the CPU path
    bool ready() {
        if (tried_) return ok_;
        tried_ = true;
        static const char *fs =
            "#version 330\n"
            "in vec2 fragTexCoord; in vec4 fragColor; out vec4 finalColor;\n"
            // the sprite list, sprite i in column i: corners (1/16 pixel), sub-image (texels), pass 2's color and alpha, pass 1's alpha
            "uniform sampler2D texture0; uniform sampler2D src1; uniform sampler2D src2;\n"
            "uniform int count; uniform vec3 bg; uniform float g1;\n"
            "vec4 texel(sampler2D s, int x, int y) { return floor(texelFetch(s, ivec2(x, y), 0) * 255.0 + 0.5); }\n"
            // dust::raster's bilinear sample with CLAMP at pixel p: the same taps, the same order of the sums, truncated
            "ivec4 sampleAt(sampler2D s, ivec2 p, ivec4 b, vec4 uv) {\n"
            "  ivec2 n = textureSize(s, 0);\n"
            "  float u = uv.x + (uv.z - uv.x) * float(p.x * 16 - b.x) / float(b.z - b.x) - 0.5;\n"
            "  float v = uv.y + (uv.w - uv.y) * float(p.y * 16 - b.y) / float(b.w - b.y) - 0.5;\n"
            "  int i = int(floor(u)), j = int(floor(v)); float fu = u - float(i), fv = v - float(j);\n"
            "  int i0 = clamp(i, 0, n.x - 1), i1 = clamp(i + 1, 0, n.x - 1), j0 = clamp(j, 0, n.y - 1), j1 = clamp(j + 1, 0, n.y - 1);\n"
            "  vec4 acc = vec4(0.0);\n"
            "  acc += texel(s, i0, j0) * ((1.0 - fu) * (1.0 - fv)); acc += texel(s, i1, j0) * (fu * (1.0 - fv));\n"
            "  acc += texel(s, i0, j1) * ((1.0 - fu) * fv); acc += texel(s, i1, j1) * (fu * fv);\n"
            "  return ivec4(acc); }\n"
            // the GS's top-left rule on the corners in 1/16 pixel
            "bool covers(ivec2 p, ivec4 b) { return b.z > b.x && b.w > b.y && p.x >= ((b.x + 15) >> 4) && p.x < ((b.z + 15) >> 4) &&\n"
            "  p.y >= ((b.y + 15) >> 4) && p.y < ((b.w + 15) >> 4); }\n"
            "void main() { ivec2 p = ivec2(gl_FragCoord.xy); int g = 0;\n"
            // pass 1: source 1 additive into G, clamped at 255 (every step adds, so the order doesn't change the sum)
            "  for (int i = 0; i < count; ++i) { ivec4 b = ivec4(texelFetch(texture0, ivec2(i, 0), 0)); if (!covers(p, b)) continue;\n"
            "    ivec4 t = sampleAt(src1, p, b, texelFetch(texture0, ivec2(i, 1), 0));\n"
            "    int as = (int(texelFetch(texture0, ivec2(i, 3), 0).x) * t.a) >> 7, cs = (int(g1) * t.g) >> 7;\n"
            "    g = min(255, ((cs * as) >> 7) + g); }\n"
            // half of G into A (copyGreenChannelToAlpha), the background in RGB; pass 2: source 2 in (Cs - Cd) * As + Cd, in list order
            "  ivec4 d = ivec4(ivec3(bg), (g >> 1) & 0x7F);\n"
            "  for (int i = 0; i < count; ++i) { ivec4 b = ivec4(texelFetch(texture0, ivec2(i, 0), 0)); if (!covers(p, b)) continue;\n"
            "    ivec4 c = ivec4(texelFetch(texture0, ivec2(i, 2), 0)); ivec4 t = sampleAt(src2, p, b, texelFetch(texture0, ivec2(i, 1), 0));\n"
            "    int as = (c.a * t.a) >> 7; ivec3 cs = (c.rgb * t.rgb) >> 7;\n"
            "    d.rgb = clamp((((cs - d.rgb) * as) >> 7) + d.rgb, 0, 255); }\n"
            "  finalColor = vec4(d) / 255.0; }\n";
        shader_ = LoadShaderFromMemory(nullptr, wotm_glsl::forTarget(fs, WOTM_GLSL_ES).c_str());
        // a shader that doesn't compile comes back as raylib's default one
        if (!IsShaderValid(shader_) || shader_.id == rlGetShaderIdDefault()) return ok_ = false;
        countLoc_ = GetShaderLocation(shader_, "count");
        bgLoc_ = GetShaderLocation(shader_, "bg");
        g1Loc_ = GetShaderLocation(shader_, "g1");
        src1Loc_ = GetShaderLocation(shader_, "src1");
        src2Loc_ = GetShaderLocation(shader_, "src2");
        list_.assign(size_t(kMaxSprites) * 4 * 4, 0.f);
        listTex_ = Texture2D{rlLoadTexture(list_.data(), kMaxSprites, 4, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1), kMaxSprites, 4, 1,
                             PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};
        return ok_ = listTex_.id != 0;
    }

    // a target for one dust texture (64 x 64, RGBA8; drawn bilinear and clamped, like the CPU texture)
    static RenderTexture2D make() {
        RenderTexture2D rt = LoadRenderTexture(dust::kSide, dust::kSide);
        if (rt.id) { SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR); SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP); }
        return rt;
    }

    // Draws the sprites of dust::sprites into rt, as dust::raster would with the same arguments; src1 and src2 are the sources
    // as textures of their pixels. The caller has flushed the batch and saved the view's target (GlTarget) before making any render
    // texture, and puts it back after, with its own shader (this leaves raylib's default one on).
    void render(const RenderTexture2D &rt, const std::vector<dust::Sprite> &spr, const int bg[3], int g1, const Texture2D &src1,
                const Texture2D &src2) {
        const int n = int(std::min(spr.size(), size_t(kMaxSprites)));   // dust::sprites makes at most 199
        for (int i = 0; i < n; ++i) {
            const dust::Sprite &s = spr[size_t(i)];
            const float row[4][4] = {{float(s.x0), float(s.y0), float(s.x1), float(s.y1)},
                                     {s.uv[0] / 16.f, s.uv[1] / 16.f, s.uv[2] / 16.f, s.uv[3] / 16.f},
                                     {float(s.rgb2[0]), float(s.rgb2[1]), float(s.rgb2[2]), float(s.a2)},
                                     {float(s.a1), 0.f, 0.f, 0.f}};
            for (int r = 0; r < 4; ++r) std::copy(row[r], row[r] + 4, list_.begin() + (ptrdiff_t(r) * kMaxSprites + i) * 4);
        }
        UpdateTexture(listTex_, list_.data());

        const OffscreenPass pass;
        OffscreenPass::into(rt.id, dust::kSide);
        // the shader first: switching it flushes the batch, which forgets the sampler textures set before
        BeginShaderMode(shader_);
        const float bgf[3] = {float(bg[0]), float(bg[1]), float(bg[2])}, g1f = float(g1);
        SetShaderValue(shader_, countLoc_, &n, SHADER_UNIFORM_INT);
        SetShaderValue(shader_, bgLoc_, bgf, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader_, g1Loc_, &g1f, SHADER_UNIFORM_FLOAT);
        SetShaderValueTexture(shader_, src1Loc_, src1);
        SetShaderValueTexture(shader_, src2Loc_, src2);
        OffscreenPass::quad(listTex_.id, float(dust::kSide));
        EndShaderMode();
    }

private:
    static constexpr int kMaxSprites = 256;
    bool tried_ = false, ok_ = false;
    Shader shader_{};
    int countLoc_ = -1, bgLoc_ = -1, g1Loc_ = -1, src1Loc_ = -1, src2Loc_ = -1;
    std::vector<float> list_;   // the sprite list's texture: 4 rows of kMaxSprites RGBA32F texels
    Texture2D listTex_{};
};

}  // namespace wotm
