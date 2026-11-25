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

    // How far (ms) a capture at captureMs is from the target marginMs before an animation frame of the grid lastRafMs + k *
    // displayPeriodMs: positive when it came early (the boundary may move later), negative past the target (it then aims at the
    // animation frame it just missed), within +-half a display frame. NaN on a display that does not lock, or with no animation frame
    // for staleMs (a hidden tab draws none)
    inline double errorMs(double captureMs, double lastRafMs, double displayPeriodMs, double gamePeriodMs, double marginMs = 2.0,
                          double staleMs = 1000.0)
    {
        if (!locks(gamePeriodMs, displayPeriodMs) || captureMs - lastRafMs > staleMs)
            return std::nan("");
        double since = std::fmod(captureMs - lastRafMs, displayPeriodMs);   // where the capture falls in the display's frame
        if (since < 0.0)
            since += displayPeriodMs;
        double error = (displayPeriodMs - marginMs) - since;
        if (error > displayPeriodMs / 2)
            error -= displayPeriodMs;
        else if (error <= -displayPeriodMs / 2)
            error += displayPeriodMs;
        return error;
    }

    // How far (ms, within +-maxStepMs) to move the next frame boundary for one capture's error: 0 within the dead band or for NaN
    inline double stepFor(double error, double maxStepMs = 0.5, double deadBandMs = 0.25)
    {
        if (std::isnan(error) || std::fabs(error) < deadBandMs)
            return 0.0;
        return std::clamp(error, -maxStepMs, maxStepMs);
    }

    // Aims the late captures instead of the middle one: a frame time that jitters by a few ms leaves half the frames past an animation
    // frame when the median is aimed (each then waits a whole display frame). The step comes from a low percentile (the 10th: the
    // latest captures) of the last 32 captures' errors, once 8 have counted. Each step taken moves the remembered errors with it (a
    // boundary moved later by s makes every capture's error s smaller), so the window does not keep pushing for 32 frames after the grid
    // got there. No step when the window's spread (90th minus 10th percentile, unwrapped around the newest) passes half a display
    // frame: the errors then look uniform, there is no phase to aim at, and stepping would change the game's pace without end. A NaN
    // error (no lock, a hidden tab) or a display period that changed by over 1% starts the window over (what it held is stale)
    class Aim
    {
    public:
        explicit Aim(double percentile = 0.1) : percentile_(percentile) {}
        void add(double error, double displayPeriodMs)
        {
            if (std::isnan(error) || std::fabs(displayPeriodMs - period_) > 0.01 * displayPeriodMs)
            {
                count_ = next_ = 0;
                period_ = std::isnan(error) ? 0.0 : displayPeriodMs;
                if (std::isnan(error))
                    return;
            }
            errors_[next_] = error;
            next_ = (next_ + 1) % errors_.size();
            count_ = std::min(count_ + 1, errors_.size());
        }
        double slewMs(double maxStepMs = 0.5, double deadBandMs = 0.25)
        {
            if (count_ < 8)
                return 0.0;
            // the errors unwrapped around the newest one, so captures that cluster where the wrap falls (opposite the target) read as
            // the one cluster they are, not as a spread of a whole display frame
            const double ref = errors_[(next_ + errors_.size() - 1) % errors_.size()];
            std::array<double, 32> v{};
            for (std::size_t i = 0; i < count_; ++i)
                v[i] = ref + std::remainder(errors_[i] - ref, period_);
            std::sort(v.begin(), v.begin() + count_);
            const double low = v[count_ / 10], high = v[count_ - 1 - count_ / 10];
            if (high - low > period_ / 2)
                return 0.0;
            const double step = stepFor(v[static_cast<std::size_t>(percentile_ * (count_ - 1))], maxStepMs, deadBandMs);
            for (std::size_t i = 0; i < count_; ++i)
                errors_[i] -= step;
            return step;
        }

    private:
        std::array<double, 32> errors_{};
        std::size_t next_ = 0, count_ = 0;
        double period_ = 0.0, percentile_;
    };
}
