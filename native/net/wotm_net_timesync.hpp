// ABOUTME: Frame-advantage time sync for lockstep matches: estimates each player's lead over the slowest active peer and
// ABOUTME: computes a smooth sub-frame period stretch (up to 10%) so the leader slows down gently instead of stalling.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace wotm_net::timesync
{
    constexpr double kMaxStretch = 0.10;        // cap frame lengthening at 10% (1.67 ms at 60 Hz, 3.33 ms at 30 Hz)
    constexpr double kAdvantageDivisor = 30.0;  // stretch = min(kMaxStretch, advantage / kAdvantageDivisor)
    constexpr double kDeadbandFrames = 0.5;     // ignore sub-half-frame packet-arrival quantization noise
    constexpr double kSmoothTauSec = 0.25;      // exponential smoothing time constant on the client (seconds)
    constexpr double kReturnRate = 0.05;        // gentle rate (5% per frame) to realign with the vblank grid when stretch returns to 0

    struct PlayerProgress
    {
        uint32_t nextMissing = 0;   // first frame with no input from this player at the server
        uint32_t delay = 0;         // this player's current input delay (frames)
        uint32_t rttMs = 0;         // this player's current round-trip time to the server (ms)
        bool active = false;        // joined, connected, not gone, not away
    };

    // Estimates how many frames each active player is ahead of the slowest active player at the current wall-clock instant.
    // A player's nextMissing at the server lags their current simulation frame by roughly (oneWayLatency - delay), so
    // (nextMissing - delay + rttMs / 2 / frameMs) recovers their simulation frame independent of asymmetric RTT or per-player delay.
    inline std::vector<double> computeAdvantages(const std::vector<PlayerProgress> &players, double frameMs)
    {
        std::vector<double> out(players.size(), 0.0);
        if (frameMs <= 0.0)
            return out;
        int activeCount = 0;
        double minFrame = 0.0;
        std::vector<double> est(players.size(), 0.0);
        for (size_t i = 0; i < players.size(); ++i)
        {
            if (!players[i].active)
                continue;
            est[i] = static_cast<double>(players[i].nextMissing) - static_cast<double>(players[i].delay) +
                     (static_cast<double>(players[i].rttMs) * 0.5) / frameMs;
            minFrame = activeCount == 0 ? est[i] : std::min(minFrame, est[i]);
            ++activeCount;
        }
        if (activeCount < 2)
            return out;
        for (size_t i = 0; i < players.size(); ++i)
            if (players[i].active)
                out[i] = std::max(0.0, est[i] - minFrame);
        return out;
    }

    inline double stretchFraction(double advantageFrames)
    {
        if (!std::isfinite(advantageFrames) || !(advantageFrames > kDeadbandFrames))
            return 0.0;
        return std::min(kMaxStretch, advantageFrames / kAdvantageDivisor);
    }

    // Exponential moving average of the server-reported frame advantage, updated once per game frame.
    class AdvantageSmoother
    {
    public:
        void reset() { value_ = 0.0; }
        double value() const { return value_; }

        double update(double targetFrames, double dtSec)
        {
            const double target = (std::isfinite(targetFrames) && targetFrames > 0.0) ? targetFrames : 0.0;
            const double alpha = (std::isfinite(dtSec) && dtSec > 0.0) ? std::clamp(1.0 - std::exp(-dtSec / kSmoothTauSec), 0.0, 1.0) : 1.0;
            value_ += (target - value_) * alpha;
            return value_;
        }

    private:
        double value_ = 0.0;
    };

    // Sub-frame deadline pacer that sits right after the fixed-step vblank wait. While stretching, it advances an internal
    // target deadline by period * (1 + stretch) each frame so every frame has a uniform stretched duration even across vblank
    // phase wraps. When stretch returns to 0, it steps back to the vblank grid at (1 - kReturnRate) per frame with no short
    // or double frames. Overruns and network stalls reset cleanly with no debt.
    class StretchPacer
    {
    public:
        void reset()
        {
            active_ = false;
            targetMs_ = 0.0;
        }

        double extraSleepMs(double nowMs, double periodMs, double stretchFrac)
        {
            if (!std::isfinite(nowMs) || !std::isfinite(periodMs) || periodMs <= 0.0)
                return 0.0;
            const double stretch = std::isfinite(stretchFrac) ? std::clamp(stretchFrac, 0.0, kMaxStretch) : 0.0;
            if (!active_)
            {
                if (stretch <= 0.0)
                    return 0.0;
                active_ = true;
                targetMs_ = nowMs + periodMs * stretch;
                return periodMs * stretch;
            }
            const double step = stretch > 0.0 ? periodMs * (1.0 + stretch) : periodMs * (1.0 - kReturnRate);
            targetMs_ += step;
            const double wait = targetMs_ - nowMs;
            if (wait <= 0.0)
            {
                if (stretch <= 0.0)
                {
                    active_ = false;
                    return 0.0;
                }
                targetMs_ = nowMs;
                return 0.0;
            }
            const double cap = periodMs * (1.0 + kMaxStretch);
            if (wait > cap)
            {
                targetMs_ = nowMs + cap;
                return cap;
            }
            return wait;
        }

    private:
        bool active_ = false;
        double targetMs_ = 0.0;
    };

    // The client's side, once per fixed-step frame right after its vblank wait: smooths the advantage the server reports (proto::Ahead)
    // and returns how long to sleep so this frame lasts period * (1 + stretchFraction).
    class Stretcher
    {
    public:
        void reset()
        {
            smoother_.reset();
            pacer_.reset();
        }
        // after a lockstep wait: the pacer's deadline restarts from the new time, but the advantage stays (the wait is what a leader
        // that stretches too little runs into)
        void waited() { pacer_.reset(); }
        double sleepMs(double nowMs, double periodMs, double aheadFrames)
        {
            const double adv = smoother_.update(aheadFrames, periodMs / 1000.0);
            return pacer_.extraSleepMs(nowMs, periodMs, stretchFraction(adv));
        }

    private:
        AdvantageSmoother smoother_;
        StretchPacer pacer_;
    };
}
