// ABOUTME: Window size at start (#6): the largest window of the frame's aspect ratio inside 90% of the monitor, independent of the
// ABOUTME: render resolution (which is upscaled on present). Pure; used by fitWindow (wotm_live.inc), tested in native/tests.
#pragma once

#include <algorithm>
#include <cmath>

namespace wotm::windowfit
{
inline constexpr float kMonitorShare = 0.9f;   // room for the title bar, the menu bar and the dock or taskbar
inline constexpr int kFallbackHeight = 720;    // no monitor information yet

// w, h: the window for `aspect` (width / height) on a monitorW x monitorH monitor; maxHeight > 0 caps the height (WOTM_WINDOW, checks)
inline void size(float aspect, int monitorW, int monitorH, int &w, int &h, int maxHeight = 0)
{
    float fh = static_cast<float>(kFallbackHeight);
    if (monitorW > 0 && monitorH > 0)
        fh = std::min(kMonitorShare * monitorH, kMonitorShare * monitorW / aspect);
    if (maxHeight > 0)
        fh = std::min(fh, static_cast<float>(maxHeight));
    h = static_cast<int>(std::lround(fh));
    w = static_cast<int>(std::lround(fh * aspect));
}
}
