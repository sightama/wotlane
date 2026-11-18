// ABOUTME: Input latency of an online match on this PC: a button press followed from the moment the page saw it through the sample,
// ABOUTME: the game frame that applies it, that frame's capture and the presents that draw it. Pure; tested in native/tests.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace wotm::latency
{
    // ms on a clock every thread shares: in the browser emscripten_get_now (performance.timeOrigin + performance.now() on every
    // thread, so the page's event times compare with it), natively steady_clock
    inline double nowMs()
    {
#ifdef __EMSCRIPTEN__
        return emscripten_get_now();
#else
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
    }

    // One window of presses that reached the screen (ms). total: press to the first present that draws its frame, its median, 95th
    // percentile and max. The stages are medians: sample (the page saw the press, the game sampled it), delay (sample to the start of the
    // frame that applies it, without the waits), stall (waits for the other players' input in between), sim (that frame's logic, start
    // to capture), present (capture to the first present that draws it), blend (frame interpolation: from that present to the first one
    // that shows the frame whole; not in total). unknownPress: presses the page gave no time for (counted from the sample)
    struct Window
    {
        uint32_t n = 0, unknownPress = 0;
        double total = 0, p95 = 0, max = 0, sample = 0, delay = 0, stall = 0, sim = 0, present = 0, blend = 0;
    };

    // Called from the game thread (sampled, frameStarted, captured) and the present thread (taken, drew, presented, take)
    class Tracker
    {
    public:
        static constexpr size_t kMaxPending = 32, kMaxDone = 4096;   // a build that never takes a window keeps at most kMaxDone

        // a press seen by the page at pressAt (0: unknown), sampled at `at`, applies on `frame`; stallMs: the waits for the network so far
        void sampled(uint32_t frame, double pressAt, double at, double stallMs)
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (pending_.size() >= kMaxPending)
                pending_.pop_front();
            Probe p;
            p.frame = frame;
            p.known = pressAt > 0.0 && pressAt <= at;
            p.press = p.known ? pressAt : at;
            p.sample = at;
            p.stallAtSample = stallMs;
            pending_.push_back(p);
        }

        // the game frame `frame` starts (its first pad read); called on every read, only the first counts
        void frameStarted(uint32_t frame, double at, double stallMs)
        {
            std::lock_guard<std::mutex> lk(mu_);
            running_ = frame;
            haveRunning_ = true;
            for (Probe &p : pending_)
                if (p.frame == frame && p.start == 0.0)
                    p.start = at, p.stall = stallMs - p.stallAtSample;
        }

        // the scene of the frame that last started was captured for the present (call it where the capture is published, under the same
        // lock as the present's take)
        void captured(double at)
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!haveRunning_)
                return;
            lastCaptured_ = running_;
            haveCaptured_ = true;
            for (Probe &p : pending_)
                if (p.frame == running_ && p.start != 0.0 && p.captured == 0.0)
                    p.captured = at;
        }

        // the present took the newest captured frame to draw (under the capture's lock); the frames it took before stay for drew()
        void taken()
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!haveCaptured_)
                return;
            std::copy_backward(taken_, taken_ + kTaken - 1, taken_ + kTaken);
            taken_[0] = lastCaptured_;
            tookCount_ = std::min<size_t>(tookCount_ + 1, kTaken);
        }

        // the present drew an image: the newest frame it gives any weight is the one `visibleBack` takes before the latest (0: the latest),
        // and the newest it shows whole is `wholeBack` takes back (interpolation blends the last frames; without it both are 0)
        void drew(int visibleBack, int wholeBack)
        {
            std::lock_guard<std::mutex> lk(mu_);
            drawn_ = Drawn{};
            if (visibleBack >= 0 && static_cast<size_t>(visibleBack) < tookCount_)
                drawn_.visible = taken_[visibleBack], drawn_.haveVisible = true;
            if (wholeBack >= 0 && static_cast<size_t>(wholeBack) < tookCount_)
                drawn_.whole = taken_[wholeBack], drawn_.haveWhole = true;
        }

        // the image drawn since the last present is on its way to the screen: the presses its frames carry are shown (the ones shown
        // whole are done); the ones before them that never got a capture are dropped
        void presented(double at)
        {
            std::lock_guard<std::mutex> lk(mu_);
            const Drawn d = drawn_;
            drawn_ = Drawn{};
            for (auto it = pending_.begin(); it != pending_.end();)
            {
                if (d.haveVisible && !after(it->frame, d.visible) && it->captured != 0.0 && it->shown == 0.0)
                    it->shown = at;
                if (!d.haveWhole || after(it->frame, d.whole))
                {
                    ++it;
                    continue;
                }
                if (it->captured != 0.0 && done_.size() < kMaxDone)
                {
                    const double shown = it->shown != 0.0 ? it->shown : at;
                    done_.push_back({shown - it->press, it->sample - it->press, it->start - it->sample - it->stall, it->stall,
                                     it->captured - it->start, shown - it->captured, at - shown, !it->known});
                }
                it = pending_.erase(it);
            }
        }

        // the numbers since the last take
        Window take()
        {
            std::lock_guard<std::mutex> lk(mu_);
            Window w;
            w.n = static_cast<uint32_t>(done_.size());
            if (w.n == 0)
                return w;
            std::vector<double> v;
            auto median = [&](double Done::*field) {
                v.clear();
                for (const Done &d : done_)
                    v.push_back(d.*field);
                std::sort(v.begin(), v.end());
                const size_t m = v.size() / 2;
                return v.size() % 2 ? v[m] : (v[m - 1] + v[m]) / 2.0;
            };
            w.sample = median(&Done::sample);
            w.delay = median(&Done::delay);
            w.stall = median(&Done::stall);
            w.sim = median(&Done::sim);
            w.present = median(&Done::present);
            w.blend = median(&Done::blend);
            w.total = median(&Done::total);   // last: v stays sorted by total
            w.p95 = v[static_cast<size_t>(std::ceil(0.95 * v.size())) - 1];
            w.max = v.back();
            for (const Done &d : done_)
                w.unknownPress += d.unknownPress ? 1u : 0u;
            done_.clear();
            return w;
        }

        size_t pending() const
        {
            std::lock_guard<std::mutex> lk(mu_);
            return pending_.size();
        }

    private:
        static constexpr size_t kTaken = 3;   // interpolation reaches two frames back at most (the pair before the newest)
        static bool after(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }

        struct Probe
        {
            uint32_t frame = 0;
            bool known = false;
            double press = 0, sample = 0, stallAtSample = 0, start = 0, stall = 0, captured = 0, shown = 0;
        };
        struct Done
        {
            double total, sample, delay, stall, sim, present, blend;
            bool unknownPress;
        };
        struct Drawn
        {
            uint32_t visible = 0, whole = 0;
            bool haveVisible = false, haveWhole = false;
        };

        mutable std::mutex mu_;
        std::deque<Probe> pending_;
        std::vector<Done> done_;
        uint32_t running_ = 0, lastCaptured_ = 0;
        bool haveRunning_ = false, haveCaptured_ = false;
        uint32_t taken_[kTaken] = {};   // the frames the present took, newest first
        size_t tookCount_ = 0;
        Drawn drawn_;
    };

    // the process's tracker (the game thread feeds it, the browser page reads it through wotm_web_latency)
    inline Tracker &tracker()
    {
        static Tracker t;
        return t;
    }
}
