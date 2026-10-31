// wotm_flame.hpp: the fire on Agamo's head (preset 141 IdolHeadFlame) simulated without the game, per field, like particleEmit/particleMove
// (75f5be1:docs/investigacoes/2026-10-21/fogo_agamo.md; checked against a dump of an ISLAND fight). Used by the monster preview (wotm_model.hpp) and
// by photo mode (the paused game doesn't emit: with Agamo off screen during the pause, the particles had already died out).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace wotm::flame
{
struct Part { float p[3], v[3], size, age; uint8_t sel; };
struct Flame { bool have = false; float prev[3] = {0, 0, 0}, acc = 0.f; std::vector<Part> parts; };

inline float rnd(uint32_t &r) { r = r * 1664525u + 1013904223u; return float(r >> 8) / 16777216.f; }

// One field. e = emitter in the world; r0/r2 = rows 0 and 2 of the emitter's basis (StaticMat . node matrix; row 1 = the normal).
// Wind (0, 0, 3.2) added to the position, velocity starts at -wind (the particle is born at rest and accelerates upward), air drag
// 0.0390625, lifetime 20 fields, 2 per field while standing still.
inline void step(Flame &f, const float e[3], const float r0[3], const float r2[3], bool emit, uint32_t &rng)
{
    static const float kWind[3] = {0.f, 0.f, 3.2f};
    const float airF = 0.0390625f;
    for (size_t i = 0; i < f.parts.size();) {   // particleMove
        Part &q = f.parts[i];
        q.age += 1.f;
        if (q.age >= 20.f) { f.parts[i] = f.parts.back(); f.parts.pop_back(); continue; }
        const float vl = std::sqrt(q.v[0] * q.v[0] + q.v[1] * q.v[1] + q.v[2] * q.v[2]);
        const float den = std::min(1.f + airF * vl, 2.f);
        const float sc = int(q.age) < 5 ? 1.f - 0.2f * q.age + 0.2f * q.age / den : 1.f / den;
        for (int c = 0; c < 3; ++c) { q.v[c] *= sc; q.p[c] += kWind[c] + q.v[c]; }
        q.size += 1.f - 1.5f * std::min(q.age / 10.f, 1.f);
        ++i;
    }
    if (!f.have) { std::memcpy(f.prev, e, sizeof f.prev); f.have = true; }
    if (emit) {   // particleEmit
        float disp[3], rel[3];
        for (int c = 0; c < 3; ++c) { disp[c] = e[c] - f.prev[c]; rel[c] = disp[c] * 0.8f - kWind[c]; }
        const float dl = std::sqrt(disp[0] * disp[0] + disp[1] * disp[1] + disp[2] * disp[2]);
        f.acc += std::max(2.f, dl * 4.f / 15.f);
        const int n = int(f.acc);
        f.acc -= float(n);
        for (int k = 1; k <= n && f.parts.size() < 256; ++k) {
            const float frac = float(k) / float(n), remain = 1.f - frac;
            const float y = rnd(rng), r = std::sqrt(std::max(0.f, 1.f - y * y)), th = 6.2831853f * rnd(rng);
            Part q;
            for (int c = 0; c < 3; ++c) {
                const float u = r * (std::cos(th) * r0[c] + std::sin(th) * r2[c]);
                q.v[c] = rel[c] - u;
                q.p[c] = f.prev[c] + 5.f * u + disp[c] * frac - u * remain;
            }
            q.size = 7.f + 8.f * rnd(rng);
            q.age = std::floor(10.f * rnd(rng));
            q.sel = uint8_t((rng >> 13) & 3);
            f.parts.push_back(q);
        }
    }
    std::memcpy(f.prev, e, sizeof f.prev);
}

// RGBA color (0x80 = 1.0) by age: the preset's table (0x256C10 + 4 * 608), index age * 3072 in 4.12, interpolated
inline uint32_t color(float age)
{
    static const uint32_t kCol[16] = {0x00808080u, 0x28808080u, 0x4b808080u, 0x66808080u, 0x80808080u, 0x80808080u, 0x7d808080u, 0x72808080u,
                                      0x64808080u, 0x54838180u, 0x457d7f80u, 0x37687580u, 0x284b6880u, 0x19355f80u, 0x0c21537du, 0x01124276u};
    const float t = std::max(age, 0.f) * 3072.f / 4096.f;
    const int i0 = std::min(15, int(t)), i1 = std::min(15, i0 + 1);
    const float fr = t - float(int(t));
    uint32_t out = 0;
    for (int b = 0; b < 4; ++b) {
        const float a = float((kCol[i0] >> (8 * b)) & 0xFF), z = float((kCol[i1] >> (8 * b)) & 0xFF);
        out |= uint32_t(std::clamp(a + (z - a) * fr, 0.f, 255.f)) << (8 * b);
    }
    return out;
}
} // namespace wotm::flame
