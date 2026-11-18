// ABOUTME: Decides when one member holds a running lockstep match back for everyone: away (game in the background) for over 3 s, or the
// ABOUTME: one the others keep waiting on while the match's pace stays under 75% of its rate. No network or game code; used by the server.
#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <vector>

namespace wotm_net
{
    // In lockstep the match runs at the pace of its slowest member, so one device that can't keep up (or a tab in the background) slows
    // every player. The server feeds this every ~100 ms; a member it names is dropped from the match (neutral from then on, through the
    // same path as leaving), so the others play on. Nobody is named in the first kGraceSec (loading, the intro), nor when that would leave
    // fewer than two players.
    class Holdback
    {
    public:
        static constexpr double kGraceSec = 10.0;
        static constexpr double kLoadMaxSec = 60.0;    // a match that never moves (a page that never sends a real input) is judged anyway
        static constexpr uint32_t kMaxPathFrames = 2;  // at most this much trail is put down to a longer path
        static constexpr double kAwaySec = 3.0;        // in the background for longer than this
        static constexpr double kSlowWindowSec = 15.0; // the pace measured over this long...
        static constexpr double kSlowShare = 0.75;     // ...under this share of the match's rate
        static constexpr double kLastShare = 0.6;      // the bottleneck is alone at the back in at least this share of the samples

        enum class Reason { None, Away, Slow };
        struct Member
        {
            uint32_t behind = 0;   // frames this member's inputs trail the member furthest ahead
            bool away = false;     // its game is in the background (Away)
            bool active = true;    // still in the match
        };
        struct Decision
        {
            int slot = -1;
            Reason reason = Reason::None;
            double pace = 0;       // the match's frames per second over the window (Slow)
        };

        // The frames a member's inputs trail just from its longer path to the server (its round trip against the shortest one, one
        // way): a healthy device on a far connection trails this much, so it isn't counted against it. Capped: a client could make
        // its own round trip look longer
        static uint32_t latencyFrames(uint32_t rttMs, uint32_t minRttMs, double frameMs)
        {
            if (rttMs <= minRttMs || frameMs <= 0) return 0u;
            return std::min(kMaxPathFrames, static_cast<uint32_t>((rttMs - minRttMs) / 2.0 / frameMs));
        }

        // t: seconds (any origin); frame: the match's frame (the first one some member's input is missing for); hz: the match's rate
        Decision update(double t, uint32_t frame, int hz, const std::vector<Member> &members)
        {
            // the match moves once every page has loaded the stage and sent a real input: until then a page still loading holds every
            // frame back without being slow, so the grace counts from the first frame the match plays
            if (!seen_)
                seen_ = true, firstFrame_ = frame, firstSeen_ = t;
            if (!started_)
            {
                if (frame <= firstFrame_ && t - firstSeen_ < kLoadMaxSec)
                    return {};
                started_ = true, start_ = t;
            }
            // while a member is away: how long, and in how many samples it was the one alone at the back (holding everyone)
            awaySince_.resize(members.size(), -1.0);
            awaySamples_.resize(members.size(), 0);
            awayBack_.resize(members.size(), 0);
            const int back = aloneAtBack(members, members);
            for (size_t i = 0; i < members.size(); ++i)
            {
                if (!members[i].active || !members[i].away)
                {
                    awaySince_[i] = -1.0, awaySamples_[i] = awayBack_[i] = 0;
                    continue;
                }
                if (awaySince_[i] < 0)
                    awaySince_[i] = t;
                ++awaySamples_[i];
                awayBack_[i] += back == static_cast<int>(i);
            }
            samples_.push_back({t, frame, members});
            // the window starts at the newest sample that is at least kSlowWindowSec old (samples come every 100 ms or a little more)
            while (samples_.size() > 1 && samples_[1].t <= t - kSlowWindowSec + 1e-6)
                samples_.pop_front();
            if (t - start_ < kGraceSec)
                return {};
            int active = 0;
            for (const Member &m : members)
                active += m.active;
            if (active <= 2)
                return {};
            for (size_t i = 0; i < members.size(); ++i)
                if (awaySince_[i] >= 0 && t - awaySince_[i] > kAwaySec && awayBack_[i] >= kLastShare * awaySamples_[i])
                    return named(static_cast<int>(i), Reason::Away, 0);
            const Sample &first = samples_.front();
            const double span = t - first.t;
            if (first.t - start_ < kGraceSec - 1e-6 || span < kSlowWindowSec - 1e-6)
                return {};
            const double pace = (frame - first.frame) / span;
            if (pace >= kSlowShare * hz)
                return {};
            // the bottleneck: the member alone at the back (the only one with the largest trail, above 0) in most samples. How far it
            // trails is capped by the input delay, so on a short delay it is about one frame: the count is what tells it apart
            std::vector<int> last(members.size(), 0);
            for (const Sample &s : samples_)
                if (const int at = aloneAtBack(s.members, members); at >= 0)
                    ++last[static_cast<size_t>(at)];
            const auto worst = std::max_element(last.begin(), last.end());
            if (*worst < kLastShare * static_cast<double>(samples_.size()))
                return {};
            return named(static_cast<int>(worst - last.begin()), Reason::Slow, pace);
        }

    private:
        // the member alone at the back of a sample (the only one with the largest trail, above 0) among those active now; -1: none
        static int aloneAtBack(const std::vector<Member> &sample, const std::vector<Member> &now)
        {
            int at = -1;
            uint32_t most = 0;
            bool alone = false;
            for (size_t i = 0; i < sample.size() && i < now.size(); ++i)
            {
                if (!now[i].active)
                    continue;
                if (sample[i].behind > most)
                    most = sample[i].behind, at = static_cast<int>(i), alone = true;
                else if (sample[i].behind == most)
                    alone = false;
            }
            return alone ? at : -1;
        }

        struct Sample
        {
            double t;
            uint32_t frame;
            std::vector<Member> members;
        };
        // a decision starts a fresh window: the match's pace after a drop is a new measurement
        Decision named(int slot, Reason reason, double pace)
        {
            samples_.clear();
            awaySince_.assign(awaySince_.size(), -1.0);
            awaySamples_.assign(awaySamples_.size(), 0);
            awayBack_.assign(awayBack_.size(), 0);
            return {slot, reason, pace};
        }

        bool seen_ = false, started_ = false;
        uint32_t firstFrame_ = 0;
        double firstSeen_ = 0;
        double start_ = 0;
        std::deque<Sample> samples_;
        std::vector<double> awaySince_;
        std::vector<int> awaySamples_, awayBack_;
    };
}
