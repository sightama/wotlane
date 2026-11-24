// ABOUTME: Deterministic lockstep simulation of an online match's pacing: peers on 60 Hz vblank grids, inputs through a server with
// ABOUTME: latency and jitter, and optional frame-advantage time sync (proportional or PI stretch), counting each peer's waits per 5 s.
//
// Model (native/runtime/wotm_overrides.cpp and wotm_sync.inc): a frame works, waits for its vblank (the grid tick after the one it
// started on), optionally sleeps the time sync stretch, samples the input for frame+1+delay (sent to the server at once, in order, with
// latency and jitter), then waits for every other peer's input for frame+1 (relayed by the server). A wait shifts that peer's vblank
// grid by its length (shiftHostDeadlines). Time sync: every 100 ms the server estimates each peer's frame as
// nextMissing - delay + rtt/2/frame, sends its lead over the slowest peer, and the client smooths it (0.25 s) and stretches its frames.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr double kFrameMs = 1000.0 / 60.0;
    constexpr double kMaxStretch = 0.10;     // at most 10% longer frames
    constexpr double kDeadband = 0.5;        // proportional: no stretch up to half a frame of lead
    constexpr double kGain = 1.0 / 30.0;     // proportional: stretch = lead / 30 frames (Task 8 of the online-seamless plan)
    constexpr double kSmoothSec = 0.25;
    constexpr double kReturnRate = 0.05;

    enum class Sync { Off, Proportional, PI };

    struct Options
    {
        Sync sync = Sync::Off;
        double secs = 120, delay = 4, piTarget = 0.5, piGain = 0.05;
        double rtt[2] = {40, 60}, jitter = 15;
        double work[2] = {7, 13}, workSd[2] = {1.5, 2.5}, spikeP[2] = {0, 0.04}, spikeMs = 12;
        unsigned seed = 1;
    };

    // keeps a peer's frames period * (1 + stretch) long after its vblank wait, then eases back onto the grid
    class Pacer
    {
    public:
        void reset() { active_ = false; }
        double sleepMs(double now, double period, double stretch)
        {
            if (!active_)
            {
                if (stretch <= 0.0)
                    return 0.0;
                active_ = true;
                target_ = now + period * stretch;
                return period * stretch;
            }
            target_ += stretch > 0.0 ? period * (1.0 + stretch) : period * (1.0 - kReturnRate);
            const double wait = target_ - now;
            if (wait <= 0.0)
            {
                active_ = stretch > 0.0;
                target_ = now;
                return 0.0;
            }
            return std::min(wait, period * (1.0 + kMaxStretch));
        }

    private:
        bool active_ = false;
        double target_ = 0.0;
    };

    struct Peer
    {
        double t = 0, grid = 0, endT = 0, lastUp = 0, ahead = 0, smoothed = 0, integral = 0;
        uint32_t frame = 0, waits = 0;
        double waitMs = 0;
        bool blocked = false;
        std::vector<double> atServer;                       // when the input for frame F reached the server
        std::deque<std::pair<double, double>> aheadMsgs;    // (arrival, lead in frames)
        Pacer pacer;
    };

    struct Result
    {
        double gfps[2], waitsPer5s[2], waitShare[2];
    };

    Result simulate(const Options &o)
    {
        std::mt19937 rng(o.seed);
        std::normal_distribution<double> normal(0, 1);
        std::uniform_real_distribution<double> uniform(0, 1);
        const uint32_t delay = static_cast<uint32_t>(o.delay);
        Peer peers[2];
        for (Peer &p : peers)
            p.atServer.assign(delay + 1, 0.0);   // the neutral frames every peer starts with
        // q's input for frame F at peer p (the server relays it on arrival)
        auto arrival = [&](int q, int p, uint32_t f) {
            if (f <= delay)
                return 0.0;
            return f < peers[q].atServer.size() ? peers[q].atServer[f] + o.rtt[p] / 2 : 1e18;
        };
        double nextSync = 1000;
        for (;;)
        {
            const int p = peers[1].t < peers[0].t ? 1 : 0, other = 1 - p;
            Peer &me = peers[p];
            if (me.t > o.secs * 1000)
                break;
            for (; o.sync != Sync::Off && nextSync <= me.t; nextSync += 100)
            {
                double est[2];
                for (int q = 0; q < 2; ++q)
                {
                    uint32_t next = 0;
                    while (next < peers[q].atServer.size() && peers[q].atServer[next] <= nextSync)
                        ++next;
                    est[q] = next - o.delay + o.rtt[q] / 2 / kFrameMs;
                }
                const double slowest = std::min(est[0], est[1]);
                for (int q = 0; q < 2; ++q)
                    peers[q].aheadMsgs.push_back({nextSync + o.rtt[q] / 2, std::round((est[q] - slowest) * 100) / 100});
            }
            if (me.blocked)
            {
                const double need = std::max(me.endT, arrival(other, p, me.frame + 1));
                if (need >= 1e17)
                {
                    me.t = std::max(me.t, peers[other].t) + 0.01;   // the other peer has not sent it yet
                    continue;
                }
                if (need > me.endT)
                {
                    ++me.waits;
                    me.waitMs += need - me.endT;
                    me.grid += need - me.endT;
                    me.pacer.reset();
                }
                me.blocked = false;
                me.t = need;
                ++me.frame;
                continue;
            }
            const double start = me.t;
            const double tick = std::floor((start - me.grid) / kFrameMs + 1e-9);
            double work = std::max(1.0, o.work[p] + normal(rng) * o.workSd[p]);
            if (uniform(rng) < o.spikeP[p])
                work += o.spikeMs;
            double t = std::max(start + work, me.grid + (tick + 1) * kFrameMs);
            if (o.sync != Sync::Off)
            {
                while (!me.aheadMsgs.empty() && me.aheadMsgs.front().first <= t)
                    me.ahead = me.aheadMsgs.front().second, me.aheadMsgs.pop_front();
                const double dt = kFrameMs / 1000.0;
                me.smoothed += (me.ahead - me.smoothed) * (1.0 - std::exp(-dt / kSmoothSec));
                double stretch = me.smoothed > kDeadband ? std::min(kMaxStretch, me.smoothed * kGain) : 0.0;
                if (o.sync == Sync::PI)
                {
                    me.integral = std::clamp(me.integral + o.piGain * (me.smoothed - o.piTarget) * dt, 0.0, kMaxStretch);
                    stretch = std::min(kMaxStretch, stretch + me.integral);
                }
                t += me.pacer.sleepMs(t, kFrameMs, stretch);
            }
            const uint32_t f = me.frame + 1 + delay;
            if (me.atServer.size() <= f)
                me.atServer.resize(f + 1, 1e18);
            me.lastUp = std::max(me.lastUp, t + o.rtt[p] / 2 + (uniform(rng) - 0.5) * o.jitter);
            me.atServer[f] = me.lastUp;
            me.blocked = true;
            me.endT = me.t = t;
        }
        Result r;
        for (int q = 0; q < 2; ++q)
        {
            r.gfps[q] = peers[q].frame / o.secs;
            r.waitsPer5s[q] = peers[q].waits / (o.secs / 5);
            r.waitShare[q] = 100.0 * peers[q].waitMs / (o.secs * 1000);
        }
        return r;
    }

    const char *name(Sync s) { return s == Sync::Off ? "off" : s == Sync::Proportional ? "p" : "pi"; }

    void row(const char *label, const Options &o)
    {
        // the mean of several seeds, so one unlucky run does not decide a comparison
        constexpr int kSeeds = 5;
        Result sum{};
        for (int s = 0; s < kSeeds; ++s)
        {
            Options so = o;
            so.seed = o.seed + s;
            const Result r = simulate(so);
            for (int q = 0; q < 2; ++q)
                sum.gfps[q] += r.gfps[q] / kSeeds, sum.waitsPer5s[q] += r.waitsPer5s[q] / kSeeds, sum.waitShare[q] += r.waitShare[q] / kSeeds;
        }
        std::printf("%-34s %-3s  gfps %5.1f  waits/5s P1 %6.1f P2 %6.1f  wait share P1 %4.1f%% P2 %4.1f%%\n", label, name(o.sync), sum.gfps[0],
                    sum.waitsPer5s[0], sum.waitsPer5s[1], sum.waitShare[0], sum.waitShare[1]);
    }

    void table(const Options &base)
    {
        struct Case { const char *label; double slowWork, jitter, spike; };
        const Case cases[] = {
            {"P2 13 ms frames (keeps 60)", 13, 15, 0.04},
            {"P2 16.5 ms frames (~58 fps)", 16.5, 15, 0.04},
            {"P2 17.5 ms frames (~55 fps)", 17.5, 15, 0.04},
            {"P2 18.3 ms frames (~53 fps)", 18.3, 15, 0.04},
            {"P2 13 ms frames, 30 ms jitter", 13, 30, 0.04},
            {"P2 13 ms frames, 40 ms jitter", 13, 40, 0.06},
        };
        for (const Case &c : cases)
            for (Sync s : {Sync::Off, Sync::Proportional, Sync::PI})
            {
                Options o = base;
                o.sync = s;
                o.work[1] = c.slowWork;
                o.jitter = c.jitter;
                o.spikeP[1] = c.spike;
                row(c.label, o);
            }
    }

    const char *kUsage =
        "usage: timesync_sim [--table] [--sync off|p|pi] [--secs S] [--delay D] [--rtt A,B] [--jitter J] [--work A,B] [--spike P]\n"
        "                    [--pi-target F] [--pi-gain G] [--seed N]\n"
        "  --table (the default with no options): every scenario of docs/investigacoes/2026-11-24/time-sync, each with no time sync,\n"
        "  the proportional stretch (lead/30, Task 8) and PI (proportional plus an integral toward --pi-target frames of lead).\n"
        "  Otherwise one scenario. Two peers; P1 is the fast one (--work: mean ms of game work per frame, sd 1.5 and 2.5;\n"
        "  --spike: P2's chance per frame of 12 ms more). Each figure is the mean of 5 seeds from --seed.\n";
}

int main(int argc, char **argv)
{
    Options o;
    bool one = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto value = [&]() -> const char * {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "%s needs a value\n%s", a.c_str(), kUsage);
                std::exit(2);
            }
            return argv[++i];
        };
        auto pair = [&](double *out) {
            const char *v = value();
            if (std::sscanf(v, "%lf,%lf", &out[0], &out[1]) != 2)
            {
                std::fprintf(stderr, "%s: expected A,B (got %s)\n", a.c_str(), v);
                std::exit(2);
            }
        };
        if (a == "-h" || a == "--help")
            return std::printf("%s", kUsage), 0;
        else if (a == "--table")
            one = false;
        else if (a == "--sync")
        {
            const std::string v = value();
            o.sync = v == "p" ? Sync::Proportional : v == "pi" ? Sync::PI : Sync::Off;
            if (v != "p" && v != "pi" && v != "off")
                return std::fprintf(stderr, "--sync: off, p or pi\n"), 2;
            one = true;
        }
        else if (a == "--secs") o.secs = std::atof(value()), one = true;
        else if (a == "--delay") o.delay = std::atof(value()), one = true;
        else if (a == "--rtt") pair(o.rtt), one = true;
        else if (a == "--jitter") o.jitter = std::atof(value()), one = true;
        else if (a == "--work") pair(o.work), one = true;
        else if (a == "--spike") o.spikeP[1] = std::atof(value()), one = true;
        else if (a == "--pi-target") o.piTarget = std::atof(value()), one = true;
        else if (a == "--pi-gain") o.piGain = std::atof(value()), one = true;
        else if (a == "--seed") o.seed = static_cast<unsigned>(std::atoi(value())), one = true;
        else
            return std::fprintf(stderr, "unknown option %s\n%s", a.c_str(), kUsage), 2;
    }
    if (one)
        row("one scenario", o);
    else
        table(o);
    return 0;
}
