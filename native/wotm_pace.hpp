// ABOUTME: The runtime watchdog's pace window: game frames per second over a window and whether it counts toward a SLOW warning.
// ABOUTME: Pure; used by the watchdog in the PS2Recomp seam (ps2_runtime.cpp), tested in native/tests.
#pragma once

#include <cstdint>

namespace wotm::pace
{
struct Window
{
    bool counted;   // the window says something about the game's pace
    double fps;     // game frames per second; 0 when not counted
};

// A window from g_frame `startFrame` to `endFrame` over `seconds`. It doesn't count when g_frame didn't move (the freeze check
// handles that) or went back (it starts again after a match, and the unsigned difference would wrap), a movie played, a gap of
// over 1 s between frames (`maxGap`, loading) happened or the game was paused. The caller starts the next window either way.
inline Window measure(uint32_t startFrame, uint32_t endFrame, double seconds, bool movie, double maxGap, bool paused)
{
    if (endFrame <= startFrame || movie || maxGap > 1.0 || paused)
        return {false, 0.0};
    return {true, static_cast<double>(endFrame - startFrame) / seconds};
}
}
