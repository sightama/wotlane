// ABOUTME: Lockstep input timeline (per player and per frame, the pad, mouse and camera settings state every PC applies) and the delay schedule.
// ABOUTME: No dependency on the game or the network; used by the client (wotm.exe) and the server.
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace wotm_net
{
    // Pad state as the runtime stores it (PadInputState in Pad.cpp): buttons are active-low, analog sticks centered at 0x80. mouseX/Y:
    // the mouse motion (pixels) of the keyboard's player on this frame, which turns that player's modern camera (wotm_orbit.inc). cam:
    // that player's camera settings (wotm_camsettings.hpp; 0 = not carried), which every PC uses to turn that player's camera
    struct PadState
    {
        uint16_t buttons = 0xFFFFu;
        uint8_t rx = 0x80u, ry = 0x80u, lx = 0x80u, ly = 0x80u;
        int16_t mouseX = 0, mouseY = 0;
        uint16_t cam = 0;
        bool operator==(const PadState &o) const
        {
            return buttons == o.buttons && rx == o.rx && ry == o.ry && lx == o.lx && ly == o.ly && mouseX == o.mouseX && mouseY == o.mouseY &&
                   cam == o.cam;
        }
        bool operator!=(const PadState &o) const { return !(*this == o); }
    };

    // Frame F (counted from the stage load) only runs once every player has input for F. Local input sampled on frame F applies on
    // frame F + delay; frames 0 .. delay - 1 are neutral for everyone (nobody could have sent them in time). A player who dropped is
    // neutral from the drop frame onward. Each (player, frame) gets exactly one value: the first one to arrive (packets resend
    // inputs that were already sent).
    class InputTimeline
    {
    public:
        InputTimeline(int players, uint32_t delay) : delay_(delay), players_(static_cast<size_t>(std::max(players, 0)))
        {
            for (Player &p : players_)
                for (uint32_t f = 0; f < delay; ++f)
                    p.frames[f] = PadState{};
            for (size_t i = 0; i < players_.size(); ++i)
                advance(players_[i]);
        }

        uint32_t delay() const { return delay_; }
        int players() const { return static_cast<int>(players_.size()); }

        bool has(int player, uint32_t frame) const
        {
            const Player *p = find(player);
            return p && (frame >= p->dropFrame || p->frames.count(frame) != 0 || frame < p->base);
        }

        bool ready(uint32_t frame) const
        {
            for (int i = 0; i < players(); ++i)
                if (!has(i, frame))
                    return false;
            return true;
        }

        PadState get(int player, uint32_t frame) const
        {
            const Player *p = find(player);
            if (!p || frame >= p->dropFrame)
                return {};
            const auto it = p->frames.find(frame);
            return it != p->frames.end() ? it->second : PadState{};
        }

        // first frame with no input from this player (everything before it has arrived)
        uint32_t nextMissing(int player) const
        {
            const Player *p = find(player);
            return p ? p->contiguous : 0u;
        }

        void set(int player, uint32_t frame, const PadState &s)
        {
            Player *p = find(player);
            if (!p || frame < p->base || frame >= p->dropFrame)
                return;
            p->frames.emplace(frame, s);
            advance(*p);
        }

        // local input sampled on frame sampledFrame: returns the frame it applies on
        uint32_t scheduleLocal(int player, uint32_t sampledFrame, const PadState &s)
        {
            const uint32_t frame = sampledFrame + delay_;
            set(player, frame, s);
            return frame;
        }

        void drop(int player, uint32_t frame)
        {
            if (Player *p = find(player))
            {
                p->dropFrame = std::min(p->dropFrame, frame);
                p->frames.erase(p->frames.lower_bound(p->dropFrame), p->frames.end());
                advance(*p);
            }
        }

        bool dropped(int player, uint32_t frame) const
        {
            const Player *p = find(player);
            return p && frame >= p->dropFrame;
        }

        // frames before `frame` won't be read again (they already ran everywhere that matters)
        void discardBefore(uint32_t frame)
        {
            for (Player &p : players_)
            {
                p.frames.erase(p.frames.begin(), p.frames.lower_bound(frame));
                p.base = std::max(p.base, std::min(frame, p.contiguous));
            }
        }

    private:
        struct Player
        {
            std::map<uint32_t, PadState> frames;
            uint32_t base = 0;                    // frames before this one were already discarded
            uint32_t contiguous = 0;              // first missing frame
            uint32_t dropFrame = 0xFFFFFFFFu;     // neutral from here on
        };

        static void advance(Player &p)
        {
            while (p.contiguous < p.dropFrame && p.frames.count(p.contiguous) != 0)
                ++p.contiguous;
            if (p.contiguous >= p.dropFrame)
                p.contiguous = p.dropFrame;
        }

        Player *find(int player) { return player >= 0 && static_cast<size_t>(player) < players_.size() ? &players_[player] : nullptr; }
        const Player *find(int player) const
        {
            return player >= 0 && static_cast<size_t>(player) < players_.size() ? &players_[player] : nullptr;
        }

        uint32_t delay_;
        std::vector<Player> players_;
    };

    // This PC's own input delay, which may change during the match (the server's advice, proto::Pace). The sample taken at frame S
    // fills frame S + delay. Growing by one repeats the previous frame (with no mouse motion) and shrinking by one skips a sample (its
    // mouse motion goes into the next), so every frame of this player still gets exactly one input, whenever and however often the
    // delay moves: the other PCs only see a value per frame, and stay in sync. A step waits for a sample with the same buttons as the
    // previous one, so it neither repeats nor drops a press (a repeated or skipped stick sample only shifts the stick by a frame, and
    // a held stick is never exactly still); growing goes ahead anyway after kForceGrowAfter samples, repeating whatever the previous
    // frame held (a delay that is too short stalls everyone). Steps are kStepGap samples apart.
    class LocalSchedule
    {
    public:
        static constexpr uint32_t kStepGap = 8, kForceGrowAfter = 30;

        explicit LocalSchedule(uint32_t delay = 0) : delay_(delay), target_(delay), next_(delay) {}

        uint32_t delay() const { return delay_; }
        uint32_t target() const { return target_; }
        void setTarget(uint32_t t)
        {
            if (t != target_)
                target_ = t, waiting_ = 0;
        }

        // the frames the next sample fills, in order (none when it is skipped)
        std::vector<std::pair<uint32_t, PadState>> push(PadState s)
        {
            std::vector<std::pair<uint32_t, PadState>> out;
            const bool steady = s.buttons == prev_.buttons;
            const bool canStep = sinceStep_ >= kStepGap;
            ++sinceStep_;
            if (target_ > delay_ && canStep && (steady || waiting_ >= kForceGrowAfter))
            {
                PadState repeat = prev_;
                repeat.mouseX = repeat.mouseY = 0;
                out.push_back({next_++, repeat});
                ++delay_;
                step();
            }
            else if (target_ < delay_ && canStep && steady && delay_ > 1u)
            {
                carryX_ += s.mouseX;
                carryY_ += s.mouseY;
                --delay_;
                step();
                return out;
            }
            else if (target_ != delay_)
                ++waiting_;
            s.mouseX = clampMouse(s.mouseX + carryX_);
            s.mouseY = clampMouse(s.mouseY + carryY_);
            carryX_ = carryY_ = 0;
            out.push_back({next_++, s});
            prev_ = s;
            return out;
        }

    private:
        static int16_t clampMouse(int v) { return static_cast<int16_t>(std::clamp(v, -32768, 32767)); }
        void step()
        {
            sinceStep_ = 0;
            waiting_ = 0;
        }

        uint32_t delay_, target_, next_;
        PadState prev_;   // the state of frame next_ - 1 (the frames before the first delay are neutral)
        uint32_t sinceStep_ = kStepGap, waiting_ = 0;
        int carryX_ = 0, carryY_ = 0;
    };
}
