// ABOUTME: Hue, saturation and brightness shift of RGBA pixels (R | G << 8 | B << 16 | A << 24), the tint tool of the character
// ABOUTME: creator (and later the map creator). Pure: no raylib, no game. All three zero is the identity, bit for bit.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace wotm::tint {

// hueDeg: degrees added to the hue (wraps); sat and bright: percent, -100..+100 (saturation and value scaled by 1 + x/100);
// alpha untouched. Values outside those ranges are clamped.
inline void shift(std::vector<uint32_t> &rgba, float hueDeg, float sat, float bright) {
    if (hueDeg == 0.f && sat == 0.f && bright == 0.f) return;
    const float sk = 1.f + std::clamp(sat, -100.f, 100.f) / 100.f, vk = 1.f + std::clamp(bright, -100.f, 100.f) / 100.f;
    float hd = std::fmod(hueDeg, 360.f);
    if (hd < 0.f) hd += 360.f;
    for (uint32_t &px : rgba) {
        const float r = float(px & 0xFF) / 255.f, g = float((px >> 8) & 0xFF) / 255.f, b = float((px >> 16) & 0xFF) / 255.f;
        const uint32_t a = px & 0xFF000000u;
        const float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
        float h = 0.f;
        if (d > 0.f) {
            if (mx == r) h = 60.f * std::fmod((g - b) / d + 6.f, 6.f);
            else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
            else h = 60.f * ((r - g) / d + 4.f);
        }
        float s = mx > 0.f ? d / mx : 0.f, v = mx;
        h = std::fmod(h + hd, 360.f);
        s = std::clamp(s * sk, 0.f, 1.f);
        v = std::clamp(v * vk, 0.f, 1.f);
        const float c = v * s, x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f)), m = v - c;
        float rr, gg, bb;
        if (h < 60.f) { rr = c; gg = x; bb = 0.f; }
        else if (h < 120.f) { rr = x; gg = c; bb = 0.f; }
        else if (h < 180.f) { rr = 0.f; gg = c; bb = x; }
        else if (h < 240.f) { rr = 0.f; gg = x; bb = c; }
        else if (h < 300.f) { rr = x; gg = 0.f; bb = c; }
        else { rr = c; gg = 0.f; bb = x; }
        auto u8 = [](float f) { return uint32_t(std::lround(std::clamp(f, 0.f, 1.f) * 255.f)) & 0xFFu; };
        px = a | u8(rr + m) | u8(gg + m) << 8 | u8(bb + m) << 16;
    }
}

} // namespace wotm::tint
