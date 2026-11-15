// ABOUTME: How far the HUD (health and power bars, the WINS text) moves to reach the edge of a window wider than 4:3. Pure; used by
// ABOUTME: Scene::drawHudAtEdges and Scene::drawQuads (wotm_native.hpp) and the font hooks, tested in native/tests.
#pragma once

#include <cstdint>

namespace wotm::hudedge
{
// The game places the HUD for a 4:3 picture whose half-width is `half43` of the view's (1 = a 4:3 view). Returns the move in NDC
// (the view spans -1..1) that takes an element by the extra width to its edge: the left one when `right` is false. A 4:3 or narrower
// view (split-screen halves) returns 0, as the game placed it.
inline float shift(float half43, bool right)
{
    if (half43 >= 0.999f)
        return 0.f;
    return (right ? 1.f : -1.f) * (1.f - half43);
}

// The text that goes to the edge: Hud::update sprintfs "WINS n" into two buffers in Hud's .rodata (no symbol in the ELF: the left one
// and the right one) and passes them to fontSpritePrint(Right)XY as the string
inline bool isWinsText(uint32_t str)
{
    str &= 0x1FFFFFFFu;
    return str == 0x006EA920u || str == 0x006EA938u;
}
}
