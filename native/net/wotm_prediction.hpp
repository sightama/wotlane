// ABOUTME: Rollback's input bookkeeping and window: the predictions each frame ran on and the first wrong one once the real inputs
// ABOUTME: arrive, and how many frames a PC can afford to run past its newest confirmed input (from its measured frame costs).
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

#include "wotm_lockstep.hpp"

namespace wotm_net
{
    // The predictions frames ran on, per (frame, player): a remote input that hadn't arrived is predicted as that player's newest
    // confirmed input. Once the real one arrives, a right prediction is dropped and the first wrong one names the frame a rollback
    // starts from; that one and every later one are dropped too, since the redo records the frames again.
    class PredictedInputs
    {
    public:
        static constexpr uint32_t kNone = 0xFFFFFFFFu;

        // frame `frame` read `used` for `player` before that input arrived. The game reads the pad twice per frame: the first record of
        // a (frame, player) is what the frame used
        void predicted(uint32_t frame, int player, const PadState &used) { used_.emplace(std::make_pair(frame, player), used); }

        // `arrived(player, frame, state)`: true with the real input when it has arrived. Drops the right predictions and returns the
        // first frame with a wrong one (kNone: none)
        template <class Arrived> uint32_t firstWrong(const Arrived &arrived)
        {
            for (auto it = used_.begin(); it != used_.end();)
            {
                PadState real;
                if (!arrived(it->first.second, it->first.first, real))
                {
                    ++it;
                    continue;
                }
                if (real != it->second)
                {
                    const uint32_t wrong = it->first.first;
                    used_.erase(used_.lower_bound({wrong, -1}), used_.end());
                    return wrong;
                }
                it = used_.erase(it);
            }
            return kNone;
        }

        // the oldest frame that ran on a prediction not yet checked (kNone: none)
        uint32_t oldest() const { return used_.empty() ? kNone : used_.begin()->first.first; }

        // a barrier (stage load): nothing before it can be rolled back
        void clear() { used_.clear(); }

    private:
        std::map<std::pair<uint32_t, int>, PadState> used_;
    };

    // A frame's costs on the game thread (ms): the frame as played (without its pacing wait), a snapshot saved and restored, and a
    // frame redone by a rollback
    struct FrameCosts
    {
        double played = 0, save = 0, restore = 0, redo = 0;
    };

    // The share of the frame a PC may fill: a rollback frame that runs long is absorbed by the next one's slack, but one that keeps
    // running long slows the whole match
    constexpr double kWindowBudgetShare = 0.9;

    // The most frames w (up to `cap`) a PC can redo at every frame: played + save + restore + w redone frames within the budget. A
    // remote player moving a stick or the mouse makes nearly every prediction wrong, so every frame may roll back
    inline uint32_t affordableWindow(const FrameCosts &c, double frameMs, uint32_t cap)
    {
        const double room = kWindowBudgetShare * frameMs - (c.played + c.save + c.restore);
        if (cap == 0 || room < c.redo)
            return 0;
        if (c.redo <= 0)
            return cap;
        return std::min(cap, static_cast<uint32_t>(room / c.redo));
    }

    // The window a PC declares, from what it can afford each measuring window: down at once (a PC over budget slows the match), up only
    // after two windows in a row with room, to the smaller of the two (one quiet window doesn't move it)
    class WindowChooser
    {
    public:
        uint32_t window() const { return window_; }
        uint32_t update(uint32_t affordable)
        {
            if (affordable <= window_)
            {
                window_ = affordable;
                room_ = 0;
                return window_;
            }
            if (room_ == 0)
            {
                room_ = affordable;
                return window_;
            }
            window_ = std::min(room_, affordable);
            room_ = 0;
            return window_;
        }

    private:
        uint32_t window_ = 0;
        uint32_t room_ = 0;   // what the previous window could afford, when it had room (0: no run with room)
    };
}
