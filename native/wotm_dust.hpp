// ABOUTME: The composite dust/smoke texture's math (Scene::dustTexture): the sprite list of the game's microprograms and its lighting,
// ABOUTME: and the GS rasterization of both passes on the CPU. No raylib: unit-tested, and the GPU path (wotm_dust_gpu.hpp) draws the same list.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#if defined(_M_X64) || defined(__SSE4_1__)
#include <smmintrin.h>   // raster: SSE4.1 rasterization
#endif

namespace wotm::dust {

constexpr int kSide = 64;   // the texture: CT32 64x64, four 32x32 cells

// What the packet and the game's tables give one regeneration (Scene::dustTexture reads them from RAM)
struct Inputs {
    float P[8];          // parameters (0x6F8DA4): half-side 1 + P0 + t P1, focal P2, phase P3, density P4, alpha scale P5, depth P6, P7
    float L[3][4];       // the stage lights' directions, one per column (dustLighting)
    float C[5][3];       // their colors, and the ambient in C[4]
    float ctx[8][4];     // the view (worldToScreenMat columns q0-q3, focal q5, q6)
    uint32_t nloop, seed;
    int16_t uvtab[16][4];   // the packet's UNPACK V4-16: 4 sub-images x 4 mirrorings of the source, in 1/16 texel
};

// A sprite of both passes: corners in 1/16 pixel of the texture, its sub-image of the source (1/16 texel), pass 1's alpha, the normal,
// and pass 2's lit color and alpha
struct Sprite { int x0, y0, x1, y1, a1; int16_t uv[4]; float n[3]; int rgb2[3], a2; };

// A decoded source texture (RGBA8, raw GS alpha)
struct Source { const uint32_t *px; uint32_t w, h; };

// Pass 1's sprite list (microprogram 0x2E98) and pass 2's light (MSCNT, 0x31C0), with the background's color in bg.
//  - A generator h = frac(c + h * b) draws 70 normals x 4 cells; each becomes a SPRITE with half-side 5 + 4t texels (t = frac(phase
//    + h.w)) at the cell center + A * n * t, with A from the worldToScreenMat columns (q0-q3) and the focal (q5); z = min(72 - 1000 t
//    (forward . n), 128), a = min(t, 1 - t) z, emitted only when a >= 1 (hemisphere facing the camera), up to 199 sprites.
//  - Color = ambient + 0.5 + sum of the stage lights (dustLighting) along the normal, and A = a * 1.20755 (P5).
// Plain float arithmetic (the VU truncates); the distribution matches the investigation's dustsim.py.
inline void sprites(const Inputs &in, std::vector<Sprite> &spr, int bg[3]) {
    const float(&P)[8] = in.P;
    const float(&L)[3][4] = in.L;
    const float(&C)[5][3] = in.C;
    const float(&ctx)[8][4] = in.ctx;
    spr.clear();
    auto fbits = [](uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; };
    auto fbitsOf = [](float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; };
    uint32_t rnd = 0x3F800000u | (in.seed & 0x7FFFFFu);   // the VU's RINIT / RNEXT
    auto rnext = [&]() { const uint32_t x = rnd & 0x7FFFFFu; rnd = ((((x << 1) | (((x >> 4) ^ (x >> 22)) & 1u)) & 0x7FFFFFu) | 0x3F800000u); return fbits(rnd); };
    float h[4];
    for (float &v : h) v = rnext();
    const float hc[4] = {fbits(0x3f3659ad), fbits(0x3f1ec458), fbits(0x3eb64807), fbits(0x3f16d60a)};
    const float hb[4] = {fbits(0x3f7e2a20), fbits(0x3f7cd6ea), fbits(0x3f66b74f), fbits(0x3f7e6ea7)};
    const float sx = P[2] / ctx[5][0], sy = P[2] / ctx[5][1];
    float col[3][3];
    for (int i = 0; i < 3; ++i) {
        const float *c = ctx[i];
        col[i][0] = (c[0] + ctx[6][0] * c[3]) * sx; col[i][1] = (c[1] + ctx[6][1] * c[3]) * sy; col[i][2] = P[6] * c[3];
    }
    const float fwd[3] = {ctx[0][3], ctx[1][3], ctx[2][3]};
    static const float kCenter[4][2] = {{2032, 2032}, {2064, 2032}, {2032, 2064}, {2064, 2064}};   // cells on the GS screen (2016 = texel 0)
    float r21[3] = {h[0] - 1.5f, h[1] - 1.5f, h[2] - 1.5f};
    int left = int(in.nloop), vi5 = 29;
    bool full = false;
    while (left > 0 && !full) {
        float t = P[3] + h[3];
        t -= t >= 0.f ? std::floor(t) : std::trunc(t);
        for (int i = 0; i < 4; ++i) { h[i] = hc[i] + h[i] * hb[i]; h[i] -= std::trunc(h[i]); }
        const float r2 = r21[0] * r21[0] + r21[1] * r21[1] + r21[2] * r21[2];
        const float half = (1.f + P[0]) + t * P[1];
        std::rotate(h, h + 1, h + 4);   // MR32
        const float rs = 1.f / std::sqrt(r2);
        float n[3] = {r21[0] * rs, r21[1] * rs, r21[2] * rs};
        for (int i = 0; i < 3; ++i) r21[i] = h[i] - 0.5f;
        for (int k = 0; k < 4; ++k) {
            const uint32_t idx = fbitsOf(h[0]) & 15;
            std::rotate(h, h + 1, h + 4);
            --left;
            if (vi5 >= 0x3FC) { full = true; break; }
            const float d[3] = {n[0] * t, n[1] * t, n[2] * t};
            const float cx = col[0][0] * d[0] + col[1][0] * d[1] + col[2][0] * d[2] + kCenter[k][0];
            const float cy = col[0][1] * d[0] + col[1][1] * d[1] + col[2][1] * d[2] + kCenter[k][1];
            const float cz = std::min(col[0][2] * d[0] + col[1][2] * d[1] + col[2][2] * d[2] + P[7], 128.f);
            const float a = std::min(1.f - t, t) * cz;
            if (a >= 1.f) {
                Sprite s{int((cx - half) * 16.f) - 2016 * 16, int((cy - half) * 16.f) - 2016 * 16, int((cx + half) * 16.f) - 2016 * 16,
                         int((cy + half) * 16.f) - 2016 * 16, int(a), {in.uvtab[idx][0], in.uvtab[idx][1], in.uvtab[idx][2], in.uvtab[idx][3]},
                         {n[0], n[1], n[2]}, {0, 0, 0}, 0};
                spr.push_back(s);
                vi5 += 5;
            }
            const float n0 = n[0];
            n[0] = -n[1]; n[1] = n[2]; n[2] = n0;   // the normal rotates from cell to cell: (x, y, z) -> (-y, z, x)
        }
    }

    auto light = [&](const float nv[3], int out[3]) {
        float dots[4];
        for (int j = 0; j < 4; ++j) dots[j] = std::max(0.f, L[0][j] * nv[0] + L[1][j] * nv[1] + L[2][j] * nv[2]);
        for (int i = 0; i < 3; ++i) {
            float v = C[4][i] + 0.5f;
            for (int j = 0; j < 4; ++j) v += C[j][i] * dots[j];
            out[i] = std::min(255, std::max(0, int(v)));
        }
    };
    light(fwd, bg);
    for (Sprite &s : spr) { light(s.n, s.rgb2); s.a2 = int(float(s.a1) * P[5]); }
}

// Both passes rasterized like the GS (bilinear with CLAMP on the source; top-left rule) into px (kSide^2, RGBA8 with raw GS alpha,
// 0x80 = 1.0, like the other particle textures):
//  - pass 1 draws source 1 with RGBA (1, g1, 1, a1) additive RGB only: density lands in G;
//  - half of RGB and A = G & 0x7F (copyGreenChannelToAlpha, the CT16 trick);
//  - pass 2: the background bg, then source 2 with each sprite's lit color in (Cs - Cd) * As + Cd RGB only.
inline void raster(const std::vector<Sprite> &spr, const int bg[3], int g1, const Source &src1, const Source &src2, std::vector<uint32_t> &px) {
    // The bilinear sample splits into column and row: index already CLAMPed and the weight of each side, computed once per column and
    // per row of the sprite with the same float math (the result is the same as sampling per pixel).
    struct Tap { int i0, i1; float w0, w1; };
    auto taps = [](float u, int n) {
        u -= 0.5f;
        const int i = int(std::floor(u));
        const float f = u - float(i);
        return Tap{std::min(std::max(i, 0), n - 1), std::min(std::max(i + 1, 0), n - 1), 1.f - f, f};
    };
    std::vector<std::array<int, 4>> fb(kSide * kSide, std::array<int, 4>{0, 0, 0, 0});
    // greenOnly (pass 1): only the source's G and A count; the frame's R and B are replaced by the background before pass 2
    auto draw = [&](const Sprite &s, const Source &src, const int rgb[3], int a, bool additive, bool greenOnly) {
        const float u0 = s.uv[0] / 16.f, v0 = s.uv[1] / 16.f, u1 = s.uv[2] / 16.f, v1 = s.uv[3] / 16.f;
        if (s.x1 <= s.x0 || s.y1 <= s.y0) return;
        const int pxa = std::max(0, (s.x0 + 15) >> 4), pxb = std::min(kSide, (s.x1 + 15) >> 4);
        const int pya = std::max(0, (s.y0 + 15) >> 4), pyb = std::min(kSide, (s.y1 + 15) >> 4);
        if (pxa >= pxb || pya >= pyb) return;
        Tap cx[kSide];
        for (int x = pxa; x < pxb; ++x)
            cx[x] = taps(u0 + (u1 - u0) * float(x * 16 - s.x0) / float(s.x1 - s.x0), int(src.w));
        const int c0 = greenOnly ? 1 : 0, c1 = greenOnly ? 2 : 3;
        for (int py = pya; py < pyb; ++py) {
            const Tap ry = taps(v0 + (v1 - v0) * float(py * 16 - s.y0) / float(s.y1 - s.y0), int(src.h));
            const uint32_t *row[2] = {src.px + size_t(ry.i0) * src.w, src.px + size_t(ry.i1) * src.w};
            const float wy[2] = {ry.w0, ry.w1};
#if defined(_M_X64) || defined(__SSE4_1__)
            // all 4 channels at once (RGBA in lanes): same sum and same integer math as the scalar version below
            const __m128i vrgb = _mm_setr_epi32(rgb[0], rgb[1], rgb[2], 0), v255 = _mm_set1_epi32(255), vzero = _mm_setzero_si128();
            (void)c0; (void)c1;
            auto texel = [](uint32_t p) { return _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(int(p)))); };
            for (int x = pxa; x < pxb; ++x) {
                const Tap &tx = cx[x];
                __m128 acc = _mm_setzero_ps();
                for (int dj = 0; dj < 2; ++dj) {
                    acc = _mm_add_ps(acc, _mm_mul_ps(texel(row[dj][tx.i0]), _mm_set1_ps(tx.w0 * wy[dj])));
                    acc = _mm_add_ps(acc, _mm_mul_ps(texel(row[dj][tx.i1]), _mm_set1_ps(tx.w1 * wy[dj])));
                }
                const __m128i t = _mm_cvttps_epi32(acc);
                const __m128i as = _mm_set1_epi32((a * _mm_extract_epi32(t, 3)) >> 7);
                int *dp = fb[size_t(py) * kSide + size_t(x)].data();
                const __m128i d = _mm_loadu_si128(reinterpret_cast<const __m128i *>(dp));
                const __m128i cs = _mm_srai_epi32(_mm_mullo_epi32(vrgb, t), 7);
                __m128i r;
                if (additive)
                    r = _mm_min_epi32(v255, _mm_add_epi32(_mm_srai_epi32(_mm_mullo_epi32(cs, as), 7), d));
                else
                    r = _mm_max_epi32(vzero, _mm_min_epi32(v255, _mm_add_epi32(_mm_srai_epi32(_mm_mullo_epi32(_mm_sub_epi32(cs, d), as), 7), d)));
                r = _mm_blend_epi16(r, d, 0xC0);   // the frame's alpha doesn't change
                _mm_storeu_si128(reinterpret_cast<__m128i *>(dp), r);
            }
#else
            for (int x = pxa; x < pxb; ++x) {
                const Tap &tx = cx[x];
                float acc[4] = {0, 0, 0, 0};
                for (int dj = 0; dj < 2; ++dj) {   // same summation order as the per-pixel sample
                    const float wa = tx.w0 * wy[dj], wb = tx.w1 * wy[dj];
                    const uint32_t pa = row[dj][tx.i0], pb = row[dj][tx.i1];
                    for (int c = c0; c < c1; ++c) acc[c] += float((pa >> (8 * c)) & 255) * wa;
                    acc[3] += float(pa >> 24) * wa;
                    for (int c = c0; c < c1; ++c) acc[c] += float((pb >> (8 * c)) & 255) * wb;
                    acc[3] += float(pb >> 24) * wb;
                }
                const int as = (a * int(acc[3])) >> 7;
                auto &d = fb[size_t(py) * kSide + size_t(x)];
                for (int c = c0; c < c1; ++c) {
                    const int cs = (rgb[c] * int(acc[c])) >> 7;
                    d[c] = additive ? std::min(255, ((cs * as) >> 7) + d[c]) : std::max(0, std::min(255, (((cs - d[c]) * as) >> 7) + d[c]));
                }
            }
#endif
        }
    };
    const int rgb1[3] = {1, g1, 1};
    for (const Sprite &s : spr) draw(s, src1, rgb1, s.a1, true, true);
    for (auto &p : fb) {
        for (int c = 0; c < 3; ++c) p[c] >>= 1;          // sprite (0, 0, 0, 64) in (Cs - Cd) * As + Cd: ((-Cd * 64) >> 7) + Cd
        p[3] = p[1] & 0x7F;                            // copyGreenChannelToAlpha (CT16's bit 15 comes out 0)
        for (int c = 0; c < 3; ++c) p[c] = bg[c];      // pass 2: background
    }
    for (const Sprite &s : spr) draw(s, src2, s.rgb2, s.a2, false, false);

    px.resize(fb.size());
    for (size_t i = 0; i < px.size(); ++i)
        px[i] = uint32_t(fb[i][0]) | (uint32_t(fb[i][1]) << 8) | (uint32_t(fb[i][2]) << 16) | (uint32_t(fb[i][3]) << 24);
}

}  // namespace wotm::dust
