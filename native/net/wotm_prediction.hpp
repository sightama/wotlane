// ABOUTME: Rollback's input bookkeeping: what each frame ran with per player (the confirmed input or a prediction), the first frame whose
// ABOUTME: prediction was wrong once the real input arrives, which frames are final, and a player's input delay with a rollback window.
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

#include "wotm_lockstep.hpp"

namespace wotm_net
{
    // With a rollback window of W frames, every PC gives its own input W frames less delay than the server advises, never under the
    // floor (W = 0: the advice as it is, the lockstep of today). The others predict the frames it hasn't sent yet.
    inline uint32_t rollbackDelay(uint32_t advice, uint32_t window, uint32_t floor)
    {
        if (window == 0)
            return advice;
        return std::max(floor, advice > window ? advice - window : 0u);
    }

    // What each frame ran with, per player: the confirmed input or a prediction (that player's last confirmed input repeated). Once a
    // predicted input arrives, a right prediction is confirmed and the first wrong one names the frame a rollback starts from. A frame is
    // final once it and every frame before it ran on confirmed inputs only: no rollback can reach it any more.
    class PredictedInputs
    {
    public:
        static constexpr uint32_t kNone = 0xFFFFFFFFu;

        explicit PredictedInputs(int players) : players_(static_cast<size_t>(std::max(players, 0))) {}

        // frame `frame` ran with `used` for `player`; `predicted` when that input had not arrived. The game reads the pad twice per
        // frame: the first record of a (frame, player) is the one kept
        void ran(uint32_t frame, int player, const PadState &used, bool predicted)
        {
            if (player < 0 || static_cast<size_t>(player) >= players_ || (final_ != kNone && frame <= final_))
                return;
            auto &row = frames_[frame];
            if (row.empty())
                row.resize(players_);
            Entry &e = row[static_cast<size_t>(player)];
            if (e.set)
                return;
            e = {used, predicted, true};
            advance();
        }

        // the frames from `frame` on run again (a rollback): what they ran with is recorded anew
        void forget(uint32_t frame) { frames_.erase(frames_.lower_bound(frame), frames_.end()); }

        // `arrived(player, frame, state)`: true with the confirmed input when it has arrived. Confirms the right predictions in frame
        // order and returns the first frame with a wrong one (kNone: none)
        template <class Arrived> uint32_t firstWrong(const Arrived &arrived)
        {
            for (auto &[frame, row] : frames_)
                for (size_t q = 0; q < row.size(); ++q)
                {
                    Entry &e = row[q];
                    PadState real;
                    if (!e.set || !e.predicted || !arrived(static_cast<int>(q), frame, real))
                        continue;
                    if (real != e.used)
                    {
                        const uint32_t wrong = frame;
                        advance();   // the frames before it may be final now
                        return wrong;
                    }
                    e.predicted = false;
                }
            advance();
            return kNone;
        }

        // the newest final frame (kNone before the first)
        uint32_t finalThrough() const { return final_; }

    private:
        struct Entry
        {
            PadState used;
            bool predicted = false, set = false;
        };

        // the leading frames with no prediction left are final
        void advance()
        {
            while (!frames_.empty())
            {
                const auto &[frame, row] = *frames_.begin();
                if (std::any_of(row.begin(), row.end(), [](const Entry &e) { return !e.set || e.predicted; }))
                    return;
                if (final_ != kNone && frame != final_ + 1u)
                    return;
                final_ = frame;
                frames_.erase(frames_.begin());
            }
        }

        size_t players_;
        std::map<uint32_t, std::vector<Entry>> frames_;
        uint32_t final_ = kNone;
    };
}
