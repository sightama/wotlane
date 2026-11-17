// ABOUTME: Which snapshot the present draws as the local player's view in a network match: the live one, the one held from before the
// ABOUTME: fight's pause (PAUSE ALL runs only view 0), or none (view 0). Pure; used by wotm_live.inc, tested in native/tests.
#pragma once

#include <memory>

namespace wotm::pauseview
{
// The local view's last live snapshot from outside the pause
template <class Snap> struct Held
{
    std::shared_ptr<Snap> snap;
};

// One redraw. `network`: one view per player, this PC draws only its own (WOTM_LOCALPLAYER); `paused`: the frame was taken in the
// fight's pause menu; `live`: this frame's snapshot of the local view (nullptr: the frame has none). Returns the snapshot to draw as the
// local view, nullptr for view 0. Outside the pause the held snapshot follows the live one, so the first unpaused frame (and every
// fight start) drops it; offline nothing is held.
template <class Snap> Snap *pick(Held<Snap> &held, bool network, bool paused, const std::shared_ptr<Snap> &live)
{
    if (!network)
    {
        held.snap.reset();
        return nullptr;
    }
    if (!paused)
        held.snap = live;
    return live ? live.get() : held.snap.get();
}
}
