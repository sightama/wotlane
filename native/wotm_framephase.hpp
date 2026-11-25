// ABOUTME: Where a game frame's capture should fall against the display's frames (the page's requestAnimationFrame): just before one,
// ABOUTME: so the frame is shown at the next refresh instead of waiting about half of one. Pure; tested in native/tests.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace wotm::framephase
{
    // The display's frames as the page sees them: the last animation frame's time and the period, the median of the last intervals
    // (a late frame now and then does not move it; a change of display, a window dragged to another screen, moves it within a few
    // frames). Times in ms on one clock (wotm::latency::nowMs)
    class Display
    {
    public:
        void raf(double atMs)
        {
            if (last_ > 0.0 && atMs > last_)
            {
                intervals_[next_] = atMs - last_;
                next_ = (next_ + 1) % intervals_.size();
                count_ = std::min(count_ + 1, intervals_.size());
            }
            last_ = atMs;
        }
        double lastMs() const { return last_; }
        double periodMs() const
        {
            if (count_ < 8)
                return 0.0;
            std::array<double, 16> v{};
            std::copy(intervals_.begin(), intervals_.begin() + count_, v.begin());
            std::nth_element(v.begin(), v.begin() + count_ / 2, v.begin() + count_);
            return v[count_ / 2];
        }

    private:
        std::array<double, 16> intervals_{};
        std::size_t next_ = 0, count_ = 0;
        double last_ = 0.0;
    };

    // Whether the game's frame grid can hold one phase against the display: the display period divides the game period (60, 120,
    // 240 Hz for a 60 Hz game, within 1%); on 144 or 165 Hz the next animation frame after a capture moves every frame
    inline bool locks(double gamePeriodMs, double displayPeriodMs)
    {
        if (displayPeriodMs <= 0.0 || gamePeriodMs < displayPeriodMs * 0.99)
            return false;
        const double ratio = gamePeriodMs / displayPeriodMs;
        return std::fabs(ratio - std::round(ratio)) < 0.01 * ratio;
    }

    // How far (ms, within +-maxStepMs) to move the game's next frame boundary so that a capture at captureMs falls marginMs before an
    // animation frame of the grid lastRafMs + k * displayPeriodMs: later when it comes early, earlier when it comes past the target
    // (it then aims at the animation frame it just missed). 0 within the dead band, on a display that does not lock, or with no
    // animation frame for staleMs (a hidden tab draws none)
    inline double slewMs(double captureMs, double lastRafMs, double displayPeriodMs, double gamePeriodMs, double marginMs = 2.0,
                         double maxStepMs = 0.5, double deadBandMs = 0.25, double staleMs = 1000.0)
    {
        if (!locks(gamePeriodMs, displayPeriodMs) || captureMs - lastRafMs > staleMs)
            return 0.0;
        double since = std::fmod(captureMs - lastRafMs, displayPeriodMs);   // where the capture falls in the display's frame
        if (since < 0.0)
            since += displayPeriodMs;
        double error = (displayPeriodMs - marginMs) - since;               // + : the capture should come later
        if (error > displayPeriodMs / 2)
            error -= displayPeriodMs;
        else if (error <= -displayPeriodMs / 2)
            error += displayPeriodMs;
        if (std::fabs(error) < deadBandMs)
            return 0.0;
        return std::clamp(error, -maxStepMs, maxStepMs);
    }
}
