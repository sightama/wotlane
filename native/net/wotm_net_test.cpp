// ABOUTME: Netcode tests without the game: the input timeline (lockstep), message encoding, and a real ENet server and client
// ABOUTME: on localhost. Exits with 0 if everything passes; each failure prints the file, line and expression.
#include "wotm_confighash.hpp"
#include "wotm_lockstep.hpp"
#include "wotm_net_holdback.hpp"
#include "wotm_monsters.hpp"
#include "wotm_lobby_client.hpp"
#include "wotm_net_client.hpp"
#include "wotm_net_diag.hpp"
#include "wotm_net_master.hpp"
#include "wotm_net_proto.hpp"
#include "wotm_net_server.hpp"
#include "wotm_net_socket.hpp"
#include "wotm_net_ws.hpp"
#include "wotm_net_wslink.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <thread>
#include <functional>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0, g_checks = 0;

#define CHECK(expr)                                                                                                                  \
    do                                                                                                                               \
    {                                                                                                                                \
        ++g_checks;                                                                                                                  \
        if (!(expr))                                                                                                                 \
        {                                                                                                                            \
            ++g_failures;                                                                                                            \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);                                                          \
        }                                                                                                                            \
    } while (0)

    struct Test
    {
        const char *name;
        std::function<void()> run;
    };
    std::vector<Test> &tests()
    {
        static std::vector<Test> t;
        return t;
    }
    struct Register
    {
        Register(const char *name, std::function<void()> run) { tests().push_back({name, std::move(run)}); }
    };
#define TEST(name)                                                                                                                   \
    void name();                                                                                                                     \
    Register reg_##name(#name, name);                                                                                                \
    void name()

    using wotm_net::InputTimeline;
    using wotm_net::PadState;

    PadState pressed(uint16_t mask)
    {
        PadState s;
        s.buttons = static_cast<uint16_t>(0xFFFFu & ~mask);
        return s;
    }

    // ---- holding the match back (wotm_net_holdback.hpp) ----

    // feeds `secs` of 100 ms samples: the match frame advances `fps` a second; `behind[i]` frames behind for each player; returns the first
    // drop decided (or a decision with slot -1)
    wotm_net::Holdback::Decision runHoldback(wotm_net::Holdback &h, double &t, uint32_t &frame, double secs, double fps,
                                             const std::vector<uint32_t> &behind, const std::vector<bool> &away, int hz = 60)
    {
        double carry = 0;
        for (double end = t + secs; t < end - 1e-9; t += 0.1)
        {
            carry += fps * 0.1;
            frame += static_cast<uint32_t>(carry);
            carry -= static_cast<uint32_t>(carry);
            std::vector<wotm_net::Holdback::Member> ms;
            for (size_t i = 0; i < behind.size(); ++i)
                ms.push_back({behind[i], away[i]});
            const auto d = h.update(t, frame, hz, ms);
            if (d.slot >= 0)
                return d;
        }
        return {};
    }

    // the one everyone waits on, with the match's pace under 80% of its rate for 15 s after the first 10 s, is named
    TEST(holdback_names_the_member_everyone_waits_on)
    {
        wotm_net::Holdback h;
        double t = 0;
        uint32_t frame = 0;
        CHECK(runHoldback(h, t, frame, 10, 30, {0, 0, 3}, {false, false, false}).slot < 0);   // the first 10 s don't count
        CHECK(runHoldback(h, t, frame, 14, 30, {0, 0, 3}, {false, false, false}).slot < 0);   // 14 s of slow pace: not yet
        const auto d = runHoldback(h, t, frame, 2, 30, {0, 0, 3}, {false, false, false});
        CHECK(d.slot == 2 && d.reason == wotm_net::Holdback::Reason::Slow);
    }

    // on a short input delay the one waited on trails by about one frame, and only some of the time: it is still named when it is
    // the one alone at the back in most samples
    TEST(holdback_names_the_bottleneck_on_a_one_frame_delay)
    {
        wotm_net::Holdback h;
        double t = 0;
        uint32_t frame = 0;
        wotm_net::Holdback::Decision d;
        for (int i = 0; i < 400 && d.slot < 0; ++i)   // 40 s: behind 1 in 7 samples of 10, level in the rest
            d = runHoldback(h, t, frame, 0.1, 10, {0, 0, i % 10 < 7 ? 1u : 0u}, {false, false, false});
        CHECK(d.slot == 2 && d.reason == wotm_net::Holdback::Reason::Slow);
    }

    // the server samples every 100 ms or a little more: the window still fills
    TEST(holdback_window_fills_with_uneven_sampling)
    {
        wotm_net::Holdback h;
        uint32_t frame = 0;
        wotm_net::Holdback::Decision d;
        for (double t = 0; t < 40 && d.slot < 0; t += 0.107)
        {
            frame += 1;
            d = h.update(t, frame, 60, {{0, false, true}, {0, false, true}, {2, false, true}});
        }
        CHECK(d.slot == 2);
    }

    // a page still loading the stage after the start holds every frame back, but it isn't slow: the grace counts from the first frame
    // the match plays
    TEST(holdback_waits_for_the_match_to_move_before_judging)
    {
        wotm_net::Holdback h;
        double t = 0;
        uint32_t frame = 3;   // the start delay's neutral frames, then nothing until the last page has loaded
        CHECK(runHoldback(h, t, frame, 40, 0, {0, 0, 3}, {false, false, false}).slot < 0);
        CHECK(runHoldback(h, t, frame, 20, 60, {0, 0, 0}, {false, false, false}).slot < 0);
    }

    // a member on a longer path to the server trails by its extra one-way time in frames even on a healthy device: that much is not
    // counted against it (so a whole match slowed by something else doesn't name the highest ping)
    TEST(holdback_latency_frames_are_the_extra_one_way_time)
    {
        CHECK(wotm_net::Holdback::latencyFrames(100, 20, 1000.0 / 30) == 1u);   // 40 ms more one way at 33 ms frames
        CHECK(wotm_net::Holdback::latencyFrames(160, 20, 1000.0 / 60) == 4u);   // 70 ms at 16.7 ms
        CHECK(wotm_net::Holdback::latencyFrames(20, 20, 1000.0 / 60) == 0u);
        CHECK(wotm_net::Holdback::latencyFrames(10, 20, 1000.0 / 60) == 0u);
    }

    // away for 2 s, back, away for 2 s again: never over 3 s in one go, so nobody; and a member that left is never named
    TEST(holdback_counts_away_in_one_go_and_skips_members_who_left)
    {
        wotm_net::Holdback h;
        double t = 0;
        uint32_t frame = 0;
        CHECK(runHoldback(h, t, frame, 12, 60, {0, 0, 0}, {false, false, false}).slot < 0);
        CHECK(runHoldback(h, t, frame, 2, 60, {0, 0, 0}, {false, true, false}).slot < 0);
        CHECK(runHoldback(h, t, frame, 0.5, 60, {0, 0, 0}, {false, false, false}).slot < 0);
        CHECK(runHoldback(h, t, frame, 2, 60, {0, 0, 0}, {false, true, false}).slot < 0);
        wotm_net::Holdback g;
        uint32_t f2 = 0;
        wotm_net::Holdback::Decision d;
        for (double u = 0; u < 40 && d.slot < 0; u += 0.1)
            d = g.update(u, f2 += 1, 60, {{0, false, true}, {0, false, true}, {0, false, true}, {5, true, false}});
        CHECK(d.slot < 0);
    }

    // a slow pace with no member clearly behind the others (the network, or everyone a bit slow), or a pace at 80% and over: nobody
    TEST(holdback_drops_nobody_without_a_clear_bottleneck_or_a_slow_pace)
    {
        wotm_net::Holdback a;
        double t = 0;
        uint32_t frame = 0;
        CHECK(runHoldback(a, t, frame, 40, 30, {1, 1, 1}, {false, false, false}).slot < 0);
        wotm_net::Holdback b;
        t = 0, frame = 0;
        CHECK(runHoldback(b, t, frame, 40, 50, {0, 0, 3}, {false, false, false}).slot < 0);   // 50 of 60 is over 80%
        wotm_net::Holdback c;
        t = 0, frame = 0;
        CHECK(runHoldback(c, t, frame, 40, 20, {0, 0, 3}, {false, false, false}, 30).slot == 2);   // a 30 Hz match: 20 of 30 is slow
    }

    // a member whose game is in the background for over 3 s is named at once (one hidden tab froze a 4-player match for 9 s)
    TEST(holdback_names_a_member_away_for_over_3_s)
    {
        wotm_net::Holdback h;
        double t = 0;
        uint32_t frame = 0;
        CHECK(runHoldback(h, t, frame, 12, 60, {0, 0, 0}, {false, false, false}).slot < 0);
        CHECK(runHoldback(h, t, frame, 2.9, 0, {0, 0, 0}, {false, true, false}).slot < 0);
        const auto d = runHoldback(h, t, frame, 0.3, 0, {0, 0, 0}, {false, true, false});
        CHECK(d.slot == 1 && d.reason == wotm_net::Holdback::Reason::Away);
    }

    // never down to one player: in a 2-player match nobody is named, whatever happens
    TEST(holdback_never_leaves_a_match_with_one_player)
    {
        wotm_net::Holdback h;
        double t = 0;
        uint32_t frame = 0;
        CHECK(runHoldback(h, t, frame, 60, 20, {0, 4}, {false, true}).slot < 0);
    }

    // A config hash is "<settings hash>:<mods digest>": a refusal says which part differs, so a player knows whether to import the
    // host's creations or to check the match settings
    TEST(config_mismatch_names_the_part_that_differs)
    {
        namespace ch = wotm_net::confighash;
        CHECK(ch::compose("1111", "aaaa") == "1111:aaaa");
        CHECK(ch::mismatch("1111:aaaa", "1111:bbbb") == "the mods (creations) differ");
        CHECK(ch::mismatch("1111:aaaa", "2222:aaaa") == "the game settings differ");
        CHECK(ch::mismatch("1111:aaaa", "2222:bbbb") == "the game settings and the mods (creations) differ");
        CHECK(ch::mismatch("cfgA", "cfgB").empty());   // a hash without the two parts (an older client): nothing more to say
        CHECK(ch::mismatch("1111:aaaa", "cfgB").empty());
    }

    TEST(timeline_first_frames_are_neutral_for_the_delay)
    {
        InputTimeline t(2, 3);
        CHECK(t.ready(0) && t.ready(1) && t.ready(2));
        CHECK(!t.ready(3));
        CHECK(t.get(0, 2) == PadState{});
        CHECK(t.nextMissing(0) == 3 && t.nextMissing(1) == 3);
    }

    TEST(timeline_frame_is_ready_only_with_every_player)
    {
        InputTimeline t(3, 2);
        t.set(0, 2, pressed(0x4000));
        t.set(2, 2, pressed(0x0010));
        CHECK(!t.ready(2));
        t.set(1, 2, PadState{});
        CHECK(t.ready(2));
        CHECK(t.get(0, 2) == pressed(0x4000));
        CHECK(t.get(2, 2) == pressed(0x0010));
    }

    TEST(timeline_next_missing_is_the_first_gap)
    {
        InputTimeline t(1, 1);
        t.set(0, 1, PadState{});
        t.set(0, 3, PadState{});
        CHECK(t.nextMissing(0) == 2);
        t.set(0, 2, PadState{});
        CHECK(t.nextMissing(0) == 4);
    }

    TEST(timeline_keeps_the_first_value_of_a_frame)
    {
        InputTimeline t(1, 0);
        t.set(0, 0, pressed(0x4000));
        t.set(0, 0, pressed(0x8000));   // a repeat (redundant packet) never replaces what already arrived
        CHECK(t.get(0, 0) == pressed(0x4000));
    }

    TEST(timeline_discard_drops_old_frames_but_keeps_progress)
    {
        InputTimeline t(1, 0);
        for (uint32_t f = 0; f < 10; ++f)
            t.set(0, f, pressed(static_cast<uint16_t>(f)));
        t.discardBefore(8);
        CHECK(t.nextMissing(0) == 10);
        CHECK(t.ready(9) && t.get(0, 9) == pressed(9));
        t.set(0, 3, pressed(0x1234));   // frame already discarded: ignored
        CHECK(t.nextMissing(0) == 10);
    }

    TEST(timeline_dropped_player_is_neutral_from_the_drop_frame)
    {
        InputTimeline t(2, 1);
        t.set(0, 1, pressed(1));
        t.set(1, 1, pressed(2));
        t.drop(1, 2);
        t.set(0, 2, pressed(3));
        CHECK(t.ready(2) && t.ready(500) == false);
        CHECK(t.get(1, 2) == PadState{});
        CHECK(t.get(1, 1) == pressed(2));
        t.set(1, 5, pressed(7));   // late input after the drop: ignored
        CHECK(t.get(1, 5) == PadState{});
    }

    TEST(local_delay_schedules_sampled_input_ahead)
    {
        InputTimeline t(2, 3);
        CHECK(t.scheduleLocal(1, 0, pressed(0x4000)) == 3u);   // the sample from frame 0 applies at frame 0 + delay
        CHECK(t.has(1, 3) && t.get(1, 3) == pressed(0x4000));
        CHECK(!t.has(0, 3));
    }

    // ---- local delay schedule: the delay moves one frame at a time and every frame still gets exactly one input ----
    using wotm_net::LocalSchedule;
    using Fills = std::vector<std::pair<uint32_t, PadState>>;

    PadState moved(uint16_t mask, int16_t mx)
    {
        PadState s = pressed(mask);
        s.mouseX = mx;
        return s;
    }

    TEST(schedule_puts_each_sample_delay_frames_ahead)
    {
        LocalSchedule s(3);
        CHECK(s.delay() == 3u);
        CHECK(s.push(pressed(1)) == Fills({{3u, pressed(1)}}));
        CHECK(s.push(pressed(2)) == Fills({{4u, pressed(2)}}));
    }

    TEST(schedule_grows_on_a_steady_sample_by_repeating_the_last_frame_without_its_mouse)
    {
        LocalSchedule s(3);
        s.push(moved(1, 5));
        s.setTarget(4);
        CHECK(s.push(moved(2, 0)) == Fills({{4u, moved(2, 0)}}));   // input changed: no step yet
        CHECK(s.delay() == 3u);
        CHECK(s.push(moved(2, 7)) == Fills({{5u, moved(2, 0)}, {6u, moved(2, 7)}}));   // the same buttons: steps
        CHECK(s.delay() == 4u);
        CHECK(s.push(moved(2, 0)) == Fills({{7u, moved(2, 0)}}));
    }

    TEST(schedule_shrinks_on_a_steady_sample_by_skipping_it_and_keeps_its_mouse)
    {
        LocalSchedule s(4);
        s.push(pressed(1));
        s.setTarget(3);
        CHECK(s.push(moved(1, 9)).empty());   // the same buttons as frame 4: skipped, frame 5 comes from the next sample
        CHECK(s.delay() == 3u);
        CHECK(s.push(moved(1, 1)) == Fills({{5u, moved(1, 10)}}));   // the skipped sample's mouse motion is not lost
        CHECK(s.push(pressed(1)) == Fills({{6u, pressed(1)}}));
    }

    // a held stick wobbles a little every frame: only the buttons decide (a repeated or skipped stick sample only shifts it by a frame)
    TEST(schedule_steps_while_a_stick_wobbles)
    {
        LocalSchedule s(4);
        s.setTarget(3);
        PadState held = pressed(1);
        for (uint8_t i = 0; i < 4 && s.delay() == 4u; ++i)
        {
            held.lx = static_cast<uint8_t>(0x20 + i);
            s.push(held);
        }
        CHECK(s.delay() == 3u);
    }

    // a target that came back to the delay forgets the wait, so the next rise waits its full kForceGrowAfter again
    TEST(schedule_restarts_the_wait_when_the_target_changes)
    {
        LocalSchedule s(3);
        s.setTarget(4);
        for (uint32_t i = 0; i < 25; ++i)
            s.push(pressed(static_cast<uint16_t>(i + 1)));
        s.setTarget(3);
        s.setTarget(4);
        uint32_t n = 0;
        while (s.delay() == 3u && n < 1000u)
            s.push(pressed(static_cast<uint16_t>(100 + n++)));
        CHECK(n == LocalSchedule::kForceGrowAfter + 1u);
    }

    TEST(schedule_never_shrinks_while_the_input_changes)
    {
        LocalSchedule s(4);
        s.setTarget(2);
        for (uint32_t i = 0; i < 200; ++i)
            CHECK(s.push(pressed(static_cast<uint16_t>(i + 1))).size() == 1u);
        CHECK(s.delay() == 4u);
    }

    TEST(schedule_grows_anyway_after_waiting_long_enough)
    {
        LocalSchedule s(3);
        s.setTarget(4);
        uint32_t i = 0;
        while (s.delay() == 3u && i < 1000u)
        {
            s.push(pressed(static_cast<uint16_t>(i + 1)));
            ++i;
        }
        CHECK(s.delay() == 4u && i == LocalSchedule::kForceGrowAfter + 1u);
    }

    TEST(schedule_steps_one_frame_at_a_time_with_a_gap)
    {
        LocalSchedule s(2);
        s.setTarget(5);
        std::vector<uint32_t> stepAt;
        for (uint32_t i = 0; i < 100; ++i)
        {
            const uint32_t before = s.delay();
            s.push(PadState{});
            CHECK(s.delay() <= before + 1u);
            if (s.delay() != before)
                stepAt.push_back(i);
        }
        CHECK(s.delay() == 5u && stepAt.size() == 3u);
        for (size_t k = 1; k < stepAt.size(); ++k)
            CHECK(stepAt[k] - stepAt[k - 1] >= LocalSchedule::kStepGap);
    }

    // random inputs and targets: the frames filled are exactly delay, delay + 1, ... with no gap and no repeat
    TEST(schedule_fills_every_frame_once_whatever_the_targets)
    {
        LocalSchedule s(3);
        uint32_t next = 3, seed = 7;
        bool contiguous = true;
        for (uint32_t i = 0; i < 5000; ++i)
        {
            seed = seed * 1103515245u + 12345u;
            if ((seed >> 16) % 97u == 0u)
                s.setTarget(1u + (seed >> 8) % 15u);
            const PadState in = pressed(static_cast<uint16_t>((seed >> 20) % 4u));
            for (const auto &f : s.push(in))
                contiguous = contiguous && f.first == next++;
            contiguous = contiguous && next == i + 1u + s.delay();
        }
        CHECK(contiguous);
    }

    // ---- protocol ----
    namespace p = wotm_net::proto;

    TEST(proto_join_roundtrip)
    {
        p::Join j;
        j.token = "a1b2c3";
        j.buildId = "build-xyz";
        j.configHash = "cfg-1";
        j.name = "Lane";
        const std::vector<uint8_t> bytes = p::encode(j);
        CHECK(p::typeOf(bytes) == p::Type::Join);
        p::Join back;
        CHECK(p::decode(bytes, back));
        CHECK(back.protocol == p::kProtocol && back.token == j.token && back.buildId == j.buildId && back.configHash == j.configHash &&
              back.name == j.name);
    }

    TEST(proto_inputs_roundtrip_with_repeated_states)
    {
        p::Inputs m;
        m.ackOwn = 41;
        p::InputRun r;
        r.player = 2;
        r.first = 37;
        for (int i = 0; i < 20; ++i)
            r.states.push_back(i < 10 ? pressed(0x4000) : PadState{});
        m.runs.push_back(r);
        const std::vector<uint8_t> bytes = p::encode(m);
        CHECK(bytes.size() == 6u + 6u + 20u + 2u * 12u);   // headers, 1 byte per state, 12 per new state (only 2 change)
        p::Inputs back;
        CHECK(p::decode(bytes, back));
        CHECK(back.ackOwn == 41u && back.runs.size() == 1u);
        CHECK(back.runs[0].player == 2 && back.runs[0].first == 37u && back.runs[0].states == r.states);
    }

    TEST(proto_input_roundtrip)
    {
        p::Input m;
        m.acks = {5, 9, 0, 12};
        m.own.player = 1;
        m.own.first = 9;
        m.own.states = {pressed(1), pressed(2), PadState{}};
        p::Input back;
        CHECK(p::decode(p::encode(m), back));
        CHECK(back.acks == m.acks && back.own.player == 1 && back.own.first == 9u && back.own.states == m.own.states);
    }

    // the modern camera's mouse motion is part of the player's input (every PC turns that player's camera by the same amount)
    TEST(proto_mouse_motion_travels_with_the_input)
    {
        PadState moved;
        moved.mouseX = -300;
        moved.mouseY = 7;
        CHECK(moved != PadState{});
        PadState extreme = moved;
        extreme.mouseX = 32767;
        extreme.mouseY = -32768;
        p::Input m;
        m.own.player = 0;
        m.own.first = 3;
        m.own.states = {moved, moved, PadState{}, extreme};
        p::Input back;
        CHECK(p::decode(p::encode(m), back));
        CHECK(back.own.states == m.own.states);
        CHECK(back.own.states[0].mouseX == -300 && back.own.states[0].mouseY == 7);
        CHECK(back.own.states[3].mouseX == 32767 && back.own.states[3].mouseY == -32768);
    }

    // the sender's packed camera settings (wotm_camsettings.hpp) travel too: every PC scales that player's camera with them
    TEST(proto_camera_settings_travel_with_the_input)
    {
        PadState withCam;
        withCam.cam = 0x00A7u;
        CHECK(withCam != PadState{});   // a settings change alone is a new state
        PadState all = withCam;
        all.cam = 0xFFFFu;
        p::Input m;
        m.own.player = 1;
        m.own.first = 0;
        m.own.states = {withCam, withCam, all, PadState{}};
        p::Input back;
        CHECK(p::decode(p::encode(m), back));
        CHECK(back.own.states == m.own.states);
        CHECK(back.own.states[0].cam == 0x00A7u && back.own.states[2].cam == 0xFFFFu && back.own.states[3].cam == 0u);
    }

    TEST(proto_control_messages_roundtrip)
    {
        p::Welcome w{3, 4};
        p::Welcome wb;
        CHECK(p::decode(p::encode(w), wb) && wb.player == 3 && wb.players == 4);
        p::Start s;
        s.delay = 4;
        s.names = {"a", "b"};
        p::Start sb;
        CHECK(p::decode(p::encode(s), sb) && sb.delay == 4u && sb.names == s.names);
        p::Hash h{120, 0x1122334455667788ull};
        p::Hash hb;
        CHECK(p::decode(p::encode(h), hb) && hb.frame == 120u && hb.hash == h.hash && hb.blocks.empty());
        h.blocks = {1, 2, 0xffffffffffffffffull};
        CHECK(p::decode(p::encode(h), hb) && hb.frame == 120u && hb.hash == h.hash && hb.blocks == h.blocks);
        p::Drop d{1, 77};
        p::Drop db;
        CHECK(p::decode(p::encode(d), db) && db.player == 1 && db.frame == 77u);
        p::Desync x{90, "player 2 differs"};
        p::Desync xb;
        CHECK(p::decode(p::encode(x), xb) && xb.frame == 90u && xb.detail == x.detail);
        p::Reject r{"different version"};
        p::Reject rb;
        CHECK(p::decode(p::encode(r), rb) && rb.reason == r.reason);
    }

    TEST(proto_truncated_message_is_rejected)
    {
        p::Inputs m;
        p::InputRun r;
        r.states = {pressed(3), pressed(4)};
        m.runs.push_back(r);
        std::vector<uint8_t> bytes = p::encode(m);
        bytes.resize(bytes.size() - 2);
        p::Inputs back;
        CHECK(!p::decode(bytes, back));
        p::Join j;
        CHECK(!p::decode(bytes, j));   // wrong type
    }

    TEST(proto_player_delay_covers_its_path_to_the_slowest_other)
    {
        // a player's input goes to the server and from there to each other player: half its own round trip plus half the slowest
        // other's, plus a few ms of margin, in frames
        const double f60 = 1000.0 / 60.0, f30 = 1000.0 / 30.0;
        CHECK(p::playerDelay({20, 20}, 0, f60) == 2u);     // 20 + 3 = 23 ms = 1.4 frames
        CHECK(p::playerDelay({60, 60}, 1, f60) == 4u);     // 63 ms = 3.8 frames
        CHECK(p::playerDelay({120, 120}, 0, f60) == 8u);   // 123 ms = 7.4 frames
        CHECK(p::playerDelay({100, 60}, 0, f60) == 5u);    // (100 + 60) / 2 + 3 = 83 ms
        // with two slow players, the fast ones only wait for the slow path
        CHECK(p::playerDelay({20, 20, 120, 120}, 0, f30) == 3u);   // 73 ms
        CHECK(p::playerDelay({20, 20, 120, 120}, 2, f30) == 4u);   // 123 ms
        CHECK(p::playerDelay({0, 0}, 0, f60) == 1u);               // floor
        CHECK(p::playerDelay({2000, 2000}, 0, f60) == 15u);        // ceiling
        CHECK(p::playerDelay({40}, 0, f60) == 1u);                 // nobody else to reach
        CHECK(p::startDelay({20, 20, 120, 120}, f30) == 4u);       // everyone starts at the largest
        CHECK(p::startDelay({0, 0}, f60) == 2u);   // at least 2: a browser ping may not be measured yet (0), the advice lowers it later
    }

    // the advice rises as soon as the path needs another frame, but only falls with half a frame to spare, so a ping sitting on a
    // frame boundary does not move the delay back and forth
    TEST(proto_advice_rises_at_once_and_falls_with_half_a_frame_to_spare)
    {
        const double f60 = 1000.0 / 60.0;
        CHECK(p::adviseDelay(2, {32, 32}, 0, f60) == 3u);   // 35 ms = 2.1 frames: rises
        CHECK(p::adviseDelay(3, {29, 29}, 0, f60) == 3u);   // 32 ms = 1.9 frames: 2 would fit, but not with half a frame spare
        CHECK(p::adviseDelay(3, {20, 20}, 0, f60) == 2u);   // 23 ms = 1.4 frames: falls
        CHECK(p::adviseDelay(9, {20, 20}, 0, f60) == 2u);   // a large fall goes straight to the target
        CHECK(p::adviseDelay(3, {40}, 0, f60) == 1u);       // nobody else to reach
    }

    TEST(proto_rtt_window_keeps_the_highest_recent_round_trip)
    {
        p::RttWindow w;
        CHECK(w.high() == 0u);
        w.add(40);
        w.add(90);
        w.add(50);
        CHECK(w.high() == 90u);
        for (size_t i = 0; i < p::RttWindow::kSamples; ++i)
            w.add(30);
        CHECK(w.high() == 30u);   // the spike left the window
    }

    TEST(proto_pace_roundtrip)
    {
        p::Pace a{7};
        p::Pace b;
        CHECK(p::decode(p::encode(a), b) && b.delay == 7u);
        CHECK(p::typeOf(p::encode(a)) == p::Type::Pace);
    }

    TEST(lobby_lines_split_words_and_key_values)
    {
        const std::vector<std::string> w = p::words("JOIN 1 Lane 3");
        CHECK(w.size() == 4u && w[0] == "JOIN" && w[3] == "3");
        const std::map<std::string, std::string> kv = p::keyValues({"MATCH", "players=2", "seed=99", "monsters=3,1"});
        CHECK(kv.at("players") == "2" && kv.at("seed") == "99" && kv.at("monsters") == "3,1" && kv.count("MATCH") == 0u);
        CHECK(p::cleanName("La ne<>!_9-xyzxyzxyzxyzxyz") == "Lane_9-xyzxyzxyz");   // 16 characters at most
        CHECK(p::cleanName("   ").empty());
    }

    // ---- real server and client on localhost ----
    using wotm_net::ClientConfig;
    using wotm_net::LineSocket;
    using wotm_net::LobbyClient;
    using wotm_net::matchEnv;
    using wotm_net::MatchClient;
    using wotm_net::Server;
    using wotm_net::ServerConfig;
    using Status = wotm_net::MatchClient::Status;

    uint16_t g_nextPort = 27770;

    // server running on a thread for the duration of the test
    struct RunningServer
    {
        Server server;
        std::atomic<bool> stop{false};
        std::thread thread;
        bool ok = false;
        explicit RunningServer(ServerConfig c) : server(withQuiet(c))
        {
            std::string err;
            ok = server.start(err);
            if (!ok)
                std::printf("  server failed to start: %s\n", err.c_str());
            thread = std::thread([this] {
                while (!stop)
                    server.poll(1);
            });
        }
        ~RunningServer()
        {
            stop = true;
            thread.join();
        }
        static ServerConfig withQuiet(ServerConfig c)
        {
            c.log = false;
            return c;
        }
    };

    // a port free on both TCP and UDP (an earlier test process that is still shutting down may be holding its ports)
    uint16_t freePort()
    {
        for (int tries = 0; tries < 200; ++tries)
        {
            const uint16_t port = g_nextPort++;
            wotm_net::Listener tcp;
            if (!tcp.listen(port))
                continue;
            const wotm_net::SocketHandle udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_ANY);
            a.sin_port = htons(port);
            const bool ok = udp != wotm_net::kNoSocket && bind(udp, reinterpret_cast<sockaddr *>(&a), sizeof a) == 0;
            if (udp != wotm_net::kNoSocket)
                wotm_net::closeSocket(udp);
            if (ok)
                return port;
        }
        return g_nextPort++;
    }

    ServerConfig testConfig()
    {
        ServerConfig c;
        c.port = freePort();
        c.startWaitMs = 100;
        return c;
    }

    std::string readUntil(wotm_net::LineLink &s, const std::string &prefix, int ms = 3000)
    {
        std::string line;
        while (s.readLine(line, ms))
            if (line.rfind(prefix, 0) == 0)
                return line;
        return {};
    }

    struct Member
    {
        LineSocket socket;
        std::map<std::string, std::string> match;
    };

    // connects and sends the HELLO for the current protocol (with `extra` words, such as hz=30); returns the WELCOME line
    std::string hello(LineSocket &s, uint16_t port, const std::string &build = "b1", const std::string &extra = "")
    {
        CHECK(s.connect("127.0.0.1", port, 2000));
        s.sendLine("HELLO " + std::to_string(p::kProtocol) + " " + build + (extra.empty() ? "" : " " + extra));
        return readUntil(s, "WELCOME");
    }

    // CREATE <lobby> <name> <monster> [password]: returns the lobby code (empty if the server refused)
    std::string createLobby(LineSocket &s, const std::string &lobby, const std::string &name, int monster, const std::string &password = "")
    {
        s.sendLine("CREATE " + lobby + " " + name + " " + std::to_string(monster) + (password.empty() ? "" : " " + password));
        const std::string created = readUntil(s, "CREATED");
        return created.size() > 8 ? created.substr(8) : std::string{};
    }

    // forms a match through the lobby: the first name creates the lobby (and is the host), the others join by code, and the host
    // picks the stage and the wins (SET) and starts it (START); `hellos`: each member's extra HELLO words
    std::vector<std::unique_ptr<Member>> formMatch(uint16_t port, const std::vector<std::string> &names, const std::vector<int> &monsters,
                                                   int stage = 1, const std::string &build = "b1", int wins = 0,
                                                   const std::vector<std::string> &hellos = {})
    {
        std::vector<std::unique_ptr<Member>> ms;
        std::string code;
        for (size_t i = 0; i < names.size(); ++i)
        {
            ms.push_back(std::make_unique<Member>());
            Member &m = *ms.back();
            CHECK(hello(m.socket, port, build, i < hellos.size() ? hellos[i] : "").rfind("WELCOME", 0) == 0);
            if (i == 0)
            {
                code = createLobby(m.socket, "Lobby", names[i], monsters[i]);
                CHECK(code.size() == 4u);
                continue;
            }
            m.socket.sendLine("JOIN " + code + " " + names[i] + " " + std::to_string(monsters[i]));
            CHECK(readUntil(m.socket, "JOINED") == "JOINED " + code);
        }
        ms[0]->socket.sendLine("SET stage=" + std::to_string(stage) + " wins=" + std::to_string(wins));
        CHECK(readUntil(ms[0]->socket, "SETTINGS " + code + " stage=" + std::to_string(stage)).size() > 0u);
        ms[0]->socket.sendLine("START");
        for (auto &m : ms)
            m->match = p::keyValues(p::words(readUntil(m->socket, "MATCH")));
        return ms;
    }

    ClientConfig clientFor(uint16_t port, const Member &m, const std::string &config = "cfg")
    {
        ClientConfig c;
        c.host = "127.0.0.1";
        c.port = port;
        c.token = m.match.count("token") ? m.match.at("token") : "";
        c.buildId = "b1";
        c.configHash = config;
        c.name = "x";
        return c;
    }

    // whoever creates a lobby is its host: the server hands out a 4-letter code, and the list shows name, host, slots and whether it
    // has a password; the others join by code (upper or lower case); only the host changes the stage and wins (SETTINGS reaches
    // everyone) and starts; the MATCH line carries the code and the wins
    TEST(lobby_create_join_by_code_settings_and_start)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket watcher, a, b;
        CHECK(hello(watcher, cfg.port) == "WELCOME 0");
        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "") == "END");

        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby_1", "Ana", 3);
        CHECK(code.size() == 4u);
        CHECK(readUntil(a, "JOINED") == "JOINED " + code);
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=1 wins=0 mods=");
        CHECK(readUntil(a, "MEMBERS") == "MEMBERS " + code + " Ana Ana:3");
        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "LOBBY") == "LOBBY " + code + " waiting 1/8 open Lobby_1 Ana Ana");
        CHECK(readUntil(watcher, "") == "END");

        CHECK(hello(b, cfg.port) == "WELCOME 1");
        std::string lower = code;
        for (char &ch : lower)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        b.sendLine("JOIN " + lower + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        CHECK(readUntil(b, "SETTINGS") == "SETTINGS " + code + " stage=1 wins=0 mods=");
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:3,") == "MEMBERS " + code + " Ana Ana:3,Bia:1");

        b.sendLine("SET wins=3");
        CHECK(readUntil(b, "ERROR") == "ERROR only the lobby owner can change the stage, wins and modifiers");
        b.sendLine("START");
        CHECK(readUntil(b, "ERROR").find("owner") != std::string::npos);
        a.sendLine("SET stage=5 wins=3 mods=");
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=5 wins=3 mods=");
        CHECK(readUntil(b, "SETTINGS") == "SETTINGS " + code + " stage=5 wins=3 mods=");
        a.sendLine("SET wins=500");
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=5 wins=99 mods=");
        // gameplay modifiers (net/wotm_modifiers.hpp): owner only, any order in, canonical order out; an unknown name changes nothing
        b.sendLine("SET mods=jump");
        CHECK(readUntil(b, "ERROR").find("owner") != std::string::npos);
        a.sendLine("SET mods=turbo,jump");
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=5 wins=99 mods=jump,turbo");
        CHECK(readUntil(b, "SETTINGS") == "SETTINGS " + code + " stage=5 wins=99 mods=jump,turbo");
        a.sendLine("SET mods=moon stage=2");
        CHECK(readUntil(a, "ERROR").find("unknown modifier") != std::string::npos);
        a.sendLine("SET wins=99");
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=5 wins=99 mods=jump,turbo");

        a.sendLine("START");
        const auto ma = p::keyValues(p::words(readUntil(a, "MATCH"))), mb = p::keyValues(p::words(readUntil(b, "MATCH")));
        CHECK(ma.size() > 0u && mb.size() > 0u);
        if (ma.empty() || mb.empty())
            return;
        CHECK(ma.at("lobby") == code && ma.at("players") == "2" && mb.at("players") == "2");
        CHECK(ma.at("stage") == "5" && ma.at("wins") == "99" && mb.at("wins") == "99");
        CHECK(ma.at("monsters") == "3,1" && mb.at("monsters") == "3,1");
        CHECK(ma.at("seed") == mb.at("seed") && ma.at("seed") != "0");
        CHECK(ma.at("step") == "1" && ma.at("names") == "Ana,Bia");
        CHECK(ma.count("mods") && ma.at("mods") == "jump,turbo" && mb.count("mods") && mb.at("mods") == "jump,turbo");
        CHECK(ma.at("you") == "0" && mb.at("you") == "1");
        CHECK(ma.at("token") != mb.at("token") && ma.at("token").size() == 16u);

        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "LOBBY") == "LOBBY " + code + " playing 2/8 open Lobby_1 Ana Ana,Bia");
        LineSocket c;
        hello(c, cfg.port);
        c.sendLine("JOIN " + code + " Cai 2");
        CHECK(readUntil(c, "ERROR").find("match") != std::string::npos);
    }

    // password-protected lobby: shows as locked in the list and only someone sending the right password gets in; an unknown code is
    // refused
    TEST(lobby_password_and_unknown_code)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket a, b, watcher;
        hello(a, cfg.port);
        hello(b, cfg.port);
        hello(watcher, cfg.port);
        const std::string code = createLobby(a, "Friends", "Ana", 3, "pw123");
        CHECK(code.size() == 4u);
        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "LOBBY") == "LOBBY " + code + " waiting 1/8 locked Friends Ana Ana");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "ERROR").find("password") != std::string::npos);
        b.sendLine("JOIN " + code + " Bia 1 wrong");
        CHECK(readUntil(b, "ERROR").find("password") != std::string::npos);
        b.sendLine("JOIN " + code + " Bia 1 pw123");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        LineSocket c;
        hello(c, cfg.port);
        c.sendLine(code == "ZZZZ" ? "JOIN YYYY Cai 2" : "JOIN ZZZZ Cai 2");
        CHECK(readUntil(c, "ERROR").find("no such lobby") != std::string::npos);
    }

    // a leaving host (LEAVE or a dropped connection) passes the lobby to the next member; an empty lobby closes and leaves the list
    TEST(lobby_host_passes_on_and_empty_lobby_closes)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket watcher, b;
        hello(watcher, cfg.port);
        std::string code;
        {
            LineSocket a;
            hello(a, cfg.port);
            code = createLobby(a, "Lobby", "Ana", 3);
            hello(b, cfg.port);
            b.sendLine("JOIN " + code + " Bia 1");
            CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        }   // Ana's connection drops
        CHECK(readUntil(b, "MEMBERS " + code + " Bia") == "MEMBERS " + code + " Bia Bia:1");
        b.sendLine("SET wins=2");
        CHECK(readUntil(b, "SETTINGS") == "SETTINGS " + code + " stage=1 wins=2 mods=");
        b.sendLine("LEAVE");
        CHECK(readUntil(b, "LEFT") == "LEFT " + code);
        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "") == "END");
    }

    // members stay in the lobby during the match; once everyone leaves the match (QUIT closes the game), the lobby goes back to
    // waiting with the same members and the host can start another one
    TEST(lobby_members_stay_through_a_match_and_start_again)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1}, 2, "b1", 3);
        {
            MatchClient a, b;
            std::string err;
            CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
            CHECK(a.waitStarted(5000) && b.waitStarted(5000));
            a.stop();
            b.stop();
        }
        const std::string code = ms[0]->match.count("lobby") ? ms[0]->match.at("lobby") : "";
        CHECK(readUntil(ms[0]->socket, "MEMBERS " + code, 5000) == "MEMBERS " + code + " Ana Ana:3,Bia:1");
        LineSocket watcher;
        hello(watcher, cfg.port);
        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "LOBBY") == "LOBBY " + code + " waiting 2/8 open Lobby Ana Ana,Bia");
        ms[0]->socket.sendLine("START");
        const auto again = p::keyValues(p::words(readUntil(ms[1]->socket, "MATCH")));
        CHECK(again.count("wins") && again.at("wins") == "3" && again.at("stage") == "2");
        CHECK(again.count("token") && ms[1]->match.count("token") && again.at("token") != ms[1]->match.at("token"));
    }

    TEST(lobby_refuses_other_protocols_builds_and_full_lobbies)
    {
        ServerConfig cfg = testConfig();
        cfg.maxPlayers = 2;
        cfg.lobbies = 1;
        RunningServer rs(cfg);
        LineSocket old;
        CHECK(old.connect("127.0.0.1", cfg.port, 2000));
        old.sendLine("HELLO 2 b1");   // protocol 2: pad states without mouse motion
        CHECK(readUntil(old, "ERROR").find("protocol") != std::string::npos);

        LineSocket a, b, c, d;
        hello(a, cfg.port);
        hello(b, cfg.port, "b2");
        hello(c, cfg.port);
        hello(d, cfg.port);
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        b.sendLine("JOIN " + code + " Bia 2");
        CHECK(readUntil(b, "ERROR").find("build") != std::string::npos);
        c.sendLine("JOIN " + code + " Cai 2");
        CHECK(readUntil(c, "JOINED") == "JOINED " + code);
        d.sendLine("JOIN " + code + " Dan 2");
        CHECK(readUntil(d, "ERROR").find("full") != std::string::npos);
        d.sendLine("CREATE Other Dan 2");
        CHECK(readUntil(d, "ERROR").find("lobbies") != std::string::npos);   // the server only has 1 lobby
    }

    // a monster with no file on the disc (spider 6, ogre 12, outside the table) or with no free costume (jelly has only 1) can't join
    // (or create a lobby): the game's load would hang on every PC
    TEST(lobby_refuses_monsters_the_disc_does_not_have)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket a, b;
        hello(a, cfg.port);
        hello(b, cfg.port);
        for (const char *bad : {"6", "12", "0", "16"})
        {
            a.sendLine(std::string("CREATE Lobby Ana ") + bad);
            CHECK(readUntil(a, "ERROR").find("monster") != std::string::npos);
        }
        const std::string code = createLobby(a, "Lobby", "Ana", 14);
        CHECK(code.size() == 4u);
        for (const char *bad : {"6", "12"})
        {
            b.sendLine("JOIN " + code + " Bia " + bad);
            CHECK(readUntil(b, "ERROR").find("monster") != std::string::npos);
        }
        b.sendLine("JOIN " + code + " Bia 14");
        CHECK(readUntil(b, "ERROR").find("costume") != std::string::npos);
        b.sendLine("JOIN " + code + " Bia 3");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
    }

    // costumes handed out the way userintMainDirect does: requested ones first, then the lowest free one (Goliath, 13, starts at 1:
    // costume 0 is a prototype)
    TEST(monster_list_fits_with_the_game_costume_rule)
    {
        int bad = -1;
        const int goliaths[] = {13, 13};
        CHECK(wotm_monsters::fits(goliaths, nullptr, 1));
        CHECK(!wotm_monsters::fits(goliaths, nullptr, 2, &bad));
        CHECK(bad == 1);
        const int goliathProto[] = {0, -1};
        CHECK(wotm_monsters::fits(goliaths, goliathProto, 2));   // the requested prototype plus the automatic 1
        const int jellies[] = {14, 14};
        CHECK(!wotm_monsters::fits(jellies, nullptr, 2));
        const int monkeys[] = {1, 1, 1, 1, 1};
        CHECK(wotm_monsters::fits(monkeys, nullptr, 5));   // the fifth repeats costume 0
        const int asked[] = {3, -1, -1};
        const int lizards[] = {3, 3, 3};
        CHECK(wotm_monsters::fits(lizards, asked, 3));   // 3 requested, the others get 0 and 1
        const int missing[] = {1, -1};
        const int jellyAsked[] = {14, 14};
        CHECK(!wotm_monsters::fits(jellyAsked, missing, 2, &bad));   // jelly only has costume 0
        CHECK(bad == 0);
        const int spider[] = {6};
        CHECK(!wotm_monsters::fits(spider, nullptr, 1));
    }

    TEST(lobby_refuses_a_second_goliath)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket a, b;
        hello(a, cfg.port);
        hello(b, cfg.port);
        const std::string code = createLobby(a, "Lobby", "Ana", 13);
        CHECK(code.size() == 4u);
        b.sendLine("JOIN " + code + " Bia 13");
        CHECK(readUntil(b, "ERROR").find("costume") != std::string::npos);
    }

    // MONSTER <monster>: a member changes monster while the lobby waits (#14); everyone gets the new MEMBERS line and the match uses it.
    // Same checks as JOIN, against the other members only; refused outside a lobby and while its match runs
    TEST(lobby_member_changes_monster_before_the_match)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket a, b, c;
        hello(a, cfg.port);
        hello(b, cfg.port);
        hello(c, cfg.port);
        c.sendLine("MONSTER 3");
        CHECK(readUntil(c, "ERROR") == "ERROR join a lobby first");
        const std::string code = createLobby(a, "Lobby", "Ana", 14);
        CHECK(code.size() == 4u);
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:14,") == "MEMBERS " + code + " Ana Ana:14,Bia:1");

        b.sendLine("MONSTER 5");
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:14,Bia:5") == "MEMBERS " + code + " Ana Ana:14,Bia:5");
        CHECK(readUntil(b, "MEMBERS " + code + " Ana Ana:14,Bia:5") == "MEMBERS " + code + " Ana Ana:14,Bia:5");
        b.sendLine("MONSTER 6");
        CHECK(readUntil(b, "ERROR").find("monster") != std::string::npos);
        b.sendLine("MONSTER 14");   // the jelly has one costume and Ana has it
        CHECK(readUntil(b, "ERROR").find("costume") != std::string::npos);
        a.sendLine("MONSTER 14");   // keeping your own monster is not a clash with yourself
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:14,Bia:5") == "MEMBERS " + code + " Ana Ana:14,Bia:5");

        a.sendLine("START");
        const auto match = p::keyValues(p::words(readUntil(b, "MATCH")));
        CHECK(match.count("monsters") && match.at("monsters") == "14,5");
        b.sendLine("MONSTER 3");
        CHECK(readUntil(b, "ERROR").find("match") != std::string::npos);
    }

    // creations online (protocol 5): the stage can be a saved map's id and a member's monster can carry a skin id
    // ("<monster>:<skin>"); both go into the MATCH line as WOTM_MATCH reads them. Every PC needs the same mods (the config hash)
    TEST(lobby_carries_map_ids_and_skins)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket a, b;
        hello(a, cfg.port);
        hello(b, cfg.port);
        a.sendLine("CREATE Lobby Ana 3:green");
        const std::string created = readUntil(a, "CREATED");
        const std::string code = created.size() > 8 ? created.substr(8) : std::string{};
        CHECK(code.size() == 4u);
        CHECK(readUntil(a, "MEMBERS") == "MEMBERS " + code + " Ana Ana:3:green");
        b.sendLine("JOIN " + code + " Bia 3:Bad!");   // a skin id is a-z, 0-9 and _ (up to 32), as the game scans them
        CHECK(readUntil(b, "ERROR").find("skin") != std::string::npos);
        b.sendLine("JOIN " + code + " Bia 3:2");   // a leading digit is a disc costume number to WOTM_MATCH, which skips the costume check
        CHECK(readUntil(b, "ERROR").find("skin") != std::string::npos);
        b.sendLine("JOIN " + code + " Bia 3:green");   // the same skin twice: the game would give the second a disc costume
        CHECK(readUntil(b, "ERROR").find("skin") != std::string::npos);
        b.sendLine("JOIN " + code + " Bia 3");   // the same monster with its disc costume is fine
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        b.sendLine("MONSTER 1:blue");
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:3:green,Bia:1") == "MEMBERS " + code + " Ana Ana:3:green,Bia:1:blue");

        a.sendLine("SET stage=t1");
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=t1 wins=0 mods=");
        a.sendLine("SET stage=9lives");   // a map id starts with a letter, then a-z, 0-9 and _ (up to 16)
        CHECK(readUntil(a, "ERROR").find("stage") != std::string::npos);
        a.sendLine("SET stage=Big-Map");
        CHECK(readUntil(a, "ERROR").find("stage") != std::string::npos);
        a.sendLine("SET wins=1");
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=t1 wins=1 mods=");

        a.sendLine("START");
        const auto ma = p::keyValues(p::words(readUntil(a, "MATCH"))), mb = p::keyValues(p::words(readUntil(b, "MATCH")));
        CHECK(ma.count("stage") && ma.at("stage") == "t1" && mb.count("stage") && mb.at("stage") == "t1");
        CHECK(ma.count("monsters") && ma.at("monsters") == "3:green,1:blue");
        a.sendLine("SET stage=4");   // back to a stock stage by number
        CHECK(readUntil(a, "SETTINGS") == "SETTINGS " + code + " stage=4 wins=1 mods=");
    }

    // the lobby client the game and wotm_lobby use (as tools/net_lobby.py): the owner creates, sets the stage and wins, waits for the
    // second player and starts; the other joins by code; both get the MATCH and its game environment
    TEST(lobby_client_creates_joins_and_gets_the_match_environment)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LobbyClient a, b;
        std::string err;
        CHECK(a.connect("127.0.0.1", cfg.port, "b1", err));
        const std::string code = a.create("Sala", "Ana", "3:green", "", err);
        CHECK(code.size() == 4u);
        CHECK(a.set("t1", 2, "turbo", err));
        CHECK(b.connect("127.0.0.1", cfg.port, "b1", err));
        CHECK(b.join(code, "Bia", "1", "", err));
        CHECK(a.waitMembers(2, err, 5000));
        CHECK(a.start(err));
        std::map<std::string, std::string> ma, mb;
        CHECK(a.waitMatch(ma, err, 5000) && b.waitMatch(mb, err, 5000));
        const auto env = matchEnv(mb, "127.0.0.1");
        std::map<std::string, std::string> e(env.begin(), env.end());
        CHECK(e["WOTM_NET"] == "127.0.0.1:" + std::to_string(cfg.port));
        CHECK(e["WOTM_MATCH"] == "3:green,1,t1");
        CHECK(e["WOTM_PLAYERS"] == "2" && e["WOTM_LOCALPLAYER"] == "2" && e["WOTM_NETNAME"] == "Bia");
        CHECK(e["WOTM_KILLTARGET"] == "2" && e["WOTM_MODIFIERS"] == "turbo");
        CHECK(!e["WOTM_NETTOKEN"].empty() && e["WOTM_NETTOKEN"] != matchEnv(ma, "127.0.0.1")[1].second);

        LobbyClient c;
        CHECK(c.connect("127.0.0.1", cfg.port, "b1", err));
        CHECK(!c.join("QQQQ", "Cid", "1", "", err) && err.find("no such lobby") != std::string::npos);
        LobbyClient old;
        CHECK(!old.connect("127.0.0.1", cfg.port, "b1", err, p::kProtocol - 1) && err.find("protocol") != std::string::npos);
    }

    // two match clients exchange frames of input and see the same timeline; each frame is checked as soon as it is ready, because
    // waitFrame keeps only the last 256 frames
    void exchange(MatchClient &a, MatchClient &b, uint32_t frames)
    {
        const uint32_t d = a.delay();
        for (uint32_t f = 0; f < frames; ++f)
        {
            a.pushLocal(f + d, pressed(static_cast<uint16_t>(f)));
            b.pushLocal(f + d, pressed(static_cast<uint16_t>(0x8000u | f)));
        }
        bool all = true, same = true, right = true;
        for (uint32_t f = 0; f < frames + d && all; ++f)
        {
            all = a.waitFrame(f, 3000) && b.waitFrame(f, 3000);
            for (int pl = 0; pl < 2 && all; ++pl)
            {
                same = same && a.input(pl, f) == b.input(pl, f);
                const PadState want = f < d ? PadState{} : pressed(static_cast<uint16_t>((pl ? 0x8000u : 0u) | (f - d)));
                right = right && a.input(pl, f) == want;
            }
        }
        CHECK(all);
        CHECK(same);
        CHECK(right);
    }

    // ---- master server (the list of public servers) ----

    using wotm_net::Master;
    using wotm_net::MasterConfig;

    struct RunningMaster
    {
        Master master;
        std::atomic<bool> stop{false};
        std::thread thread;
        explicit RunningMaster(MasterConfig c) : master(quiet(c))
        {
            std::string err;
            if (!master.start(err))
                std::printf("  master failed to start: %s\n", err.c_str());
            thread = std::thread([this] {
                while (!stop)
                    master.poll(1);
            });
        }
        ~RunningMaster()
        {
            stop = true;
            thread.join();
        }
        static MasterConfig quiet(MasterConfig c)
        {
            c.log = false;
            return c;
        }
    };

    // SERVERS: the SERVER lines up to END
    std::vector<std::string> serverRows(uint16_t masterPort)
    {
        LineSocket s;
        std::vector<std::string> rows;
        if (!s.connect("127.0.0.1", masterPort, 2000))
            return rows;
        s.sendLine("SERVERS");
        std::string line;
        while (s.readLine(line, 2000) && line != "END")
            if (line.rfind("SERVER ", 0) == 0)
                rows.push_back(line);
        return rows;
    }

    // waits up to 3 s for the master's list to be exactly `want`
    bool listBecomes(uint16_t masterPort, const std::vector<std::string> &want)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < until)
        {
            if (serverRows(masterPort) == want)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    std::string masterReply(uint16_t masterPort, const std::string &line)
    {
        LineSocket s;
        std::string reply;
        if (s.connect("127.0.0.1", masterPort, 2000) && s.sendLine(line))
            s.readLine(reply, 2000);
        return reply;
    }

    // a server started with announce settings appears in the master's list under the address the master saw, with its port, name and
    // lobby counts (open lobbies, how many of them are waiting for players, players in lobbies), and the counts follow the server
    TEST(master_lists_announcing_servers_with_their_lobby_counts)
    {
        MasterConfig mc;
        mc.port = freePort();
        RunningMaster master(mc);
        ServerConfig sc = testConfig();
        sc.announceHost = "127.0.0.1";
        sc.announcePort = mc.port;
        sc.name = "LanesPC";
        sc.announceEveryMs = 50;
        RunningServer server(sc);
        const std::string row = "SERVER 127.0.0.1 " + std::to_string(sc.port) + " " + std::to_string(p::kProtocol) + " ";
        CHECK(listBecomes(mc.port, {row + "0 0 0 16 LanesPC"}));

        LineSocket ana, bia;
        hello(ana, sc.port);
        const std::string code = createLobby(ana, "Fun", "Ana", 3);
        CHECK(listBecomes(mc.port, {row + "1 1 1 16 LanesPC"}));
        hello(bia, sc.port);
        bia.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(bia, "JOINED") == "JOINED " + code);
        CHECK(listBecomes(mc.port, {row + "1 1 2 16 LanesPC"}));
        ana.close();
        bia.close();
        CHECK(listBecomes(mc.port, {row + "0 0 0 16 LanesPC"}));
    }

    // a server that stops announcing (closed, crashed, offline) leaves the list once its last announce is older than expireMs; one that
    // announces again comes back; two servers on the same address are told apart by their port
    TEST(master_forgets_servers_that_stop_announcing)
    {
        MasterConfig mc;
        mc.port = freePort();
        mc.expireMs = 300;
        RunningMaster master(mc);
        const std::string proto = std::to_string(p::kProtocol);
        CHECK(masterReply(mc.port, "ANNOUNCE " + proto + " 7777 2 1 3 16 Alpha") == "OK");
        CHECK(masterReply(mc.port, "ANNOUNCE " + proto + " 7790 0 0 0 4 Beta") == "OK");
        CHECK(listBecomes(mc.port, {"SERVER 127.0.0.1 7777 " + proto + " 2 1 3 16 Alpha", "SERVER 127.0.0.1 7790 " + proto + " 0 0 0 4 Beta"}));
        CHECK(listBecomes(mc.port, {}));
        CHECK(masterReply(mc.port, "ANNOUNCE " + proto + " 7777 1 0 2 16 Alpha") == "OK");
        CHECK(listBecomes(mc.port, {"SERVER 127.0.0.1 7777 " + proto + " 1 0 2 16 Alpha"}));
    }

    // malformed announces are refused and listed nowhere; servers of another protocol are listed with it (so a client can show them as
    // needing another version) instead of disappearing
    TEST(master_refuses_malformed_announces)
    {
        MasterConfig mc;
        mc.port = freePort();
        RunningMaster master(mc);
        CHECK(masterReply(mc.port, "ANNOUNCE 2 7777 1 1").rfind("ERROR", 0) == 0);
        CHECK(masterReply(mc.port, "ANNOUNCE 2 notaport 1 1 1 16 X").rfind("ERROR", 0) == 0);
        CHECK(masterReply(mc.port, "ANNOUNCE 2 7777 1 1 1 16 bad!name").rfind("ERROR", 0) == 0);
        CHECK(masterReply(mc.port, "HELLO").rfind("ERROR", 0) == 0);
        CHECK(masterReply(mc.port, "ANNOUNCE 9 7777 0 0 0 16 Future") == "OK");
        CHECK(listBecomes(mc.port, {"SERVER 127.0.0.1 7777 9 0 0 0 16 Future"}));
    }

    TEST(match_clients_share_every_frame_of_input)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err));
        CHECK(b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        CHECK(a.localPlayer() == 0 && b.localPlayer() == 1 && a.players() == 2);
        CHECK(a.delay() >= 1u && a.delay() == b.delay());
        CHECK(a.names() == std::vector<std::string>({"Ana", "Bia"}));
        exchange(a, b, 100);
        CHECK(a.status() == Status::Running && b.status() == Status::Running);
    }

    // up to 8 players (4 in the game's player slots and 4 in AI slots driven by the network): lobby, 30 Hz and everyone's inputs
    TEST(match_with_eight_players_shares_every_input)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        const std::vector<std::string> names = {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8"};
        auto ms = formMatch(cfg.port, names, {3, 1, 2, 5, 4, 7, 8, 9});
        CHECK(ms[7]->match.count("players") && ms[7]->match.at("players") == "8" && ms[7]->match.at("step") == "2");
        if (!ms[7]->match.count("you"))
            return;   // the eighth didn't get into the match: the rest can't run
        CHECK(ms[7]->match.at("you") == "7");
        std::vector<std::unique_ptr<MatchClient>> cs;
        std::string err;
        for (auto &m : ms)
        {
            cs.push_back(std::make_unique<MatchClient>());
            CHECK(cs.back()->start(clientFor(cfg.port, *m), err));
        }
        bool started = true;
        for (auto &c : cs)
            started = started && c->waitStarted(5000);
        CHECK(started);
        const uint32_t d = cs[0]->delay();
        for (uint32_t f = 0; f < 60; ++f)
            for (int p = 0; p < 8; ++p)
                cs[static_cast<size_t>(p)]->pushLocal(f + d, pressed(static_cast<uint16_t>((p << 8) | f)));
        bool all = true, same = true;
        for (auto &c : cs)
            all = all && c->waitFrame(d + 59, 5000);
        CHECK(all);
        for (uint32_t f = d; f < d + 60; ++f)
            for (int p = 0; p < 8; ++p)
                for (auto &c : cs)
                    same = same && c->input(p, f) == pressed(static_cast<uint16_t>((p << 8) | (f - d)));
        CHECK(same);
    }

    // The server re-picks each player's delay from the round trips while the match runs and the clients follow it one frame at a time;
    // every frame still has exactly one input per player, the same on both PCs
    TEST(match_delay_follows_the_ping_without_losing_a_frame)
    {
        ServerConfig cfg = testConfig();
        cfg.paceMs = 20;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err));
        CHECK(b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        MatchClient *cs[2] = {&a, &b};
        // a held button state that changes every 20 samples (steady most of the time, so the delay can step), counting down
        const auto sample = [](int player, uint32_t i) {
            PadState s;
            s.buttons = static_cast<uint16_t>(0x7000u - 0x1000u * static_cast<uint32_t>(player) - i / 20u);
            return s;
        };
        uint32_t i = 0, most = 0;
        bool ok = true, same = true, ordered = true;
        PadState last[2];
        const auto run = [&](uint32_t samples) {
            for (const uint32_t end = i + samples; i < end && ok; ++i)
            {
                for (int p = 0; p < 2; ++p)
                    cs[p]->pushSample(sample(p, i));
                for (MatchClient *c : cs)
                    ok = ok && c->waitFrame(i, 5000);
                most = std::max({most, a.delay(), b.delay()});
                // both PCs see the same input on every frame, and each player's frames are their samples in order: a step repeats or
                // skips only a steady sample, so the value never jumps over one
                for (int p = 0; p < 2 && ok; ++p)
                {
                    const PadState x = a.input(p, i);
                    same = same && x == b.input(p, i);
                    if (last[p] != PadState{})
                        ordered = ordered && x.buttons <= last[p].buttons && last[p].buttons - x.buttons <= 1;
                    last[p] = x;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        };
        run(60);
        rs.server.setTestRtt(150);   // 150 ms everywhere: (150 + 150) / 2 + 3 = 153 ms = 10 frames (more on a loaded machine)
        run(400);
        CHECK(ok && a.delay() >= 10u && b.delay() >= 10u);
        rs.server.setTestRtt(0);
        run(600);
        CHECK(ok && most >= 10u && a.delay() + 4u <= most && b.delay() + 4u <= most);   // back down (how far depends on the local ping)
        CHECK(same && ordered);
        CHECK(a.status() == Status::Running && b.status() == Status::Running);
    }

    // a member whose device can't compute 60 game frames a second (a phone: HELLO ... hz=30) puts a 2-player match in the game's
    // 30 Hz mode, and the delay counts 33 ms frames: 150 ms everywhere is (150 + 150) / 2 + 3 = 153 ms = 5 frames, not 10
    TEST(match_with_a_30hz_member_runs_at_30hz)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        rs.server.setTestRtt(150);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1}, 1, "b1", 0, {"", "hz=30"});
        CHECK(ms[0]->match.count("step") && ms[0]->match.at("step") == "2" && ms[1]->match.at("step") == "2");
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        CHECK(a.delay() >= 5u && a.delay() <= 7u && a.delay() == b.delay());
        exchange(a, b, 100);
        CHECK(a.delay() >= 5u && a.delay() <= 7u && b.delay() >= 5u && b.delay() <= 7u);
        rs.server.setTestRtt(0);
    }

    // a member whose game stays in the background is taken out (Holdback): its page hears why, and the other two play on (about 14 s)
    TEST(match_drops_a_member_away_too_long_and_the_others_play_on)
    {
        ServerConfig cfg = testConfig();
        cfg.forcedDelay = 2;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia", "Caio"}, {3, 1, 2});
        MatchClient a, b, c;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err) && c.start(clientFor(cfg.port, *ms[2]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000) && c.waitStarted(5000));
        c.setAway(true);
        const uint32_t d = a.delay();
        uint32_t f = 0;
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(25);
        while (c.status() == Status::Running && std::chrono::steady_clock::now() < until)
        {
            for (MatchClient *x : {&a, &b, &c})
                x->pushLocal(f + d, pressed(static_cast<uint16_t>(f)));
            a.waitFrame(f, 50), b.waitFrame(f, 50);
            ++f;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        CHECK(c.status() == Status::Rejected);
        CHECK(c.message().find("background") != std::string::npos);
        // the two left keep going: Caio is neutral from the frame he was dropped at
        bool going = true;
        for (const uint32_t end = f + 60; f < end && going; ++f)
        {
            a.pushLocal(f + d, pressed(static_cast<uint16_t>(f)));
            b.pushLocal(f + d, pressed(static_cast<uint16_t>(f)));
            going = a.waitFrame(f, 3000) && b.waitFrame(f, 3000);
        }
        CHECK(going);
        CHECK(a.status() == Status::Running && b.status() == Status::Running);
    }

    TEST(match_survives_lost_input_packets)
    {
        ServerConfig cfg = testConfig();
        cfg.dropInputPercent = 30;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        ClientConfig ca = clientFor(cfg.port, *ms[0]), cb = clientFor(cfg.port, *ms[1]);
        ca.dropInputPercent = cb.dropInputPercent = 30;
        CHECK(a.start(ca, err) && b.start(cb, err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        exchange(a, b, 200);
    }

    TEST(match_rejects_unknown_tokens_and_other_configs)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient bad;
        std::string err;
        Member fake;
        fake.match["token"] = "0000000000000000";
        CHECK(bad.start(clientFor(cfg.port, fake), err));
        CHECK(!bad.waitStarted(1500) && bad.status() == Status::Rejected);
        CHECK(bad.message().find("token") != std::string::npos);

        MatchClient a, b;
        CHECK(a.start(clientFor(cfg.port, *ms[0], "cfgA"), err));
        // the server keeps the config of whoever joined first: b only starts after a has joined
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (a.status() != Status::Waiting && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(a.status() == Status::Waiting);
        CHECK(b.start(clientFor(cfg.port, *ms[1], "cfgB"), err));
        CHECK(!b.waitStarted(1500) && b.status() == Status::Rejected);
        CHECK(b.message().find("configuration") != std::string::npos);

        // the same settings with other mods: the refusal says the mods differ
        auto ms2 = formMatch(cfg.port, {"Caio", "Duda"}, {3, 1});
        MatchClient c, d;
        CHECK(c.start(clientFor(cfg.port, *ms2[0], "1111:aaaa"), err));
        const auto until2 = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (c.status() != Status::Waiting && std::chrono::steady_clock::now() < until2)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(c.status() == Status::Waiting);
        CHECK(d.start(clientFor(cfg.port, *ms2[1], "1111:bbbb"), err));
        CHECK(!d.waitStarted(1500) && d.status() == Status::Rejected);
        CHECK(d.message().find("game configuration differs from the others: the mods (creations) differ") != std::string::npos);
    }

    // A refused seat (other build or settings) never becomes a neutral opponent: the match does not start, even after the join
    // timeout that drops players who never arrive, and the players already in are told who could not join and why
    TEST(match_never_starts_when_a_seat_was_refused)
    {
        ServerConfig cfg = testConfig();
        cfg.joinTimeoutMs = 1500;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0], "1111:aaaa"), err));
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (a.status() != Status::Waiting && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(a.status() == Status::Waiting);
        CHECK(b.start(clientFor(cfg.port, *ms[1], "1111:bbbb"), err));
        CHECK(!b.waitStarted(1500) && b.status() == Status::Rejected);
        CHECK(!a.waitStarted(3000));   // past the join timeout
        CHECK(a.status() == Status::Rejected);
        CHECK(a.message().find("Bia") != std::string::npos);
        CHECK(a.message().find("the mods (creations) differ") != std::string::npos);
    }

    TEST(match_drops_a_player_that_leaves_at_an_agreed_frame)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        const uint32_t d = a.delay();
        for (uint32_t f = 0; f < 50; ++f)
            b.pushLocal(f + d, pressed(1));
        CHECK(b.waitAcked(3000));
        b.stop();
        for (uint32_t f = 0; f < 80; ++f)
            a.pushLocal(f + d, pressed(2));
        CHECK(a.waitFrame(d + 79, 5000));
        CHECK(a.dropFrame(1) == d + 50);
        CHECK(a.input(1, d + 49) == pressed(1) && a.input(1, d + 50) == PadState{});
    }

    TEST(proto_away_roundtrip)
    {
        p::Away a{2, true};
        p::Away ab;
        CHECK(p::decode(p::encode(a), ab) && ab.player == 2 && ab.away);
        p::Away back{1, false};
        CHECK(p::decode(p::encode(back), ab) && ab.player == 1 && !ab.away);
    }

    // true once `cond` holds, polling for up to 3 s
    template <typename F> bool soon(F cond)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!cond())
        {
            if (std::chrono::steady_clock::now() > until)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    // a player whose game goes to the background (a hidden browser tab) tells the others through the server, so their "Waiting for"
    // message says why; coming back clears it
    TEST(match_tells_the_others_a_player_is_away)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        const uint32_t f = a.delay() + 10;   // nobody has sent this frame yet
        CHECK(a.waitingText(f) == "Waiting for Ana, Bia");
        b.setAway(true);
        CHECK(soon([&] { return a.away(1); }));
        CHECK(a.waitingText(f) == "Waiting for Ana, Bia (their game tab is in the background)");
        b.setAway(false);
        CHECK(soon([&] { return !a.away(1); }));
        CHECK(a.waitingText(f) == "Waiting for Ana, Bia");
    }

    TEST(match_stops_both_peers_on_a_state_hash_mismatch)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        a.sendHash(30, 111);
        b.sendHash(30, 111);
        // the block hashes that went with the total: the server names the blocks that differ (index and guest address)
        std::vector<uint64_t> blocksA(256, 7), blocksB(256, 7);
        blocksB[5] = 8, blocksB[131] = 9;
        a.sendHash(60, 222, blocksA);
        b.sendHash(60, 223, blocksB);
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while ((a.status() != Status::Desynced || b.status() != Status::Desynced) && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(a.status() == Status::Desynced && b.status() == Status::Desynced);
        CHECK(a.desyncFrame() == 60u);
        CHECK(a.message() == "desync: state differs on frame 60: Ana vs Bia; blocks 5 (0x00a0000), 131 (0x1060000)");
        CHECK(!a.waitFrame(a.delay(), 200));   // desynced: no new frame runs
    }

    // ---- WebSocket (the browser version's transport) ----

    std::vector<uint8_t> bytesOf(const std::string &s) { return std::vector<uint8_t>(s.begin(), s.end()); }

    // WOTM_NET: host[:port] for ENet, or a whole ws:// / wss:// URL (its colons are not a port)
    TEST(server_address_splits_host_and_port)
    {
        std::string host;
        uint16_t port = 0;
        wotm_net::splitServerAddress("10.0.0.5:7790", host, port);
        CHECK(host == "10.0.0.5" && port == 7790);
        wotm_net::splitServerAddress("example.org", host, port);
        CHECK(host == "example.org" && port == p::kDefaultPort);
        wotm_net::splitServerAddress("wss://example.org/match", host, port);
        CHECK(host == "wss://example.org/match" && port == p::kDefaultPort);
        wotm_net::splitServerAddress("ws://127.0.0.1:7780/match", host, port);
        CHECK(host == "ws://127.0.0.1:7780/match" && port == p::kDefaultPort);
    }

    TEST(ws_accept_key_matches_rfc_example)
    {
        CHECK(wotm_net::ws::acceptKey("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    TEST(ws_handshake_parses_path_and_key)
    {
        const std::string req = "GET /match HTTP/1.1\r\nHost: example\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
        const auto h = wotm_net::ws::parseHandshake(req);
        CHECK(h.has_value());
        CHECK(h && h->path == "/match");
        CHECK(h && h->key == "dGhlIHNhbXBsZSBub25jZQ==");
        CHECK(!wotm_net::ws::parseHandshake("GET / HTTP/1.1\r\nHost: example\r\n\r\n").has_value());   // not an upgrade
        CHECK(!wotm_net::ws::parseHandshake("POST /match HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: x\r\n\r\n").has_value());
        // a proxy (Caddy) may send the header names in another case
        const auto lower = wotm_net::ws::parseHandshake("GET /lobby HTTP/1.1\r\nupgrade: WebSocket\r\nsec-websocket-key: abc\r\n\r\n");
        CHECK(lower && lower->path == "/lobby" && lower->key == "abc");
        const auto ua = wotm_net::ws::parseHandshake("GET /lobby HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: k\r\n"
                                                     "User-Agent: Mozilla/5.0 (Linux; Android 15; Pixel 9 Pro XL)\r\n\r\n");
        CHECK(ua && ua->userAgent == "Mozilla/5.0 (Linux; Android 15; Pixel 9 Pro XL)");
        CHECK(lower && lower->userAgent.empty());
    }

    // the client's address behind the proxy: Cloudflare's CF-Connecting-IP, else the first X-Forwarded-For hop (what Caddy sets for a
    // client that reaches it directly); text that is not an address is ignored
    TEST(handshake_reads_the_clients_address_from_the_proxy_headers)
    {
        const auto addr = [](const std::string &headers) {
            const auto h = wotm_net::ws::parseHandshake("GET /lobby HTTP/1.1\r\nUpgrade: websocket\r\nSec-WebSocket-Key: k\r\n" + headers + "\r\n");
            return h ? h->clientAddress : std::string("<refused>");
        };
        CHECK(addr("") == "");
        CHECK(addr("CF-Connecting-IP: 198.51.100.7\r\n") == "198.51.100.7");
        CHECK(addr("cf-connecting-ip: 2001:db8::1\r\n") == "2001:db8::1");
        CHECK(addr("X-Forwarded-For: 203.0.113.9\r\n") == "203.0.113.9");
        CHECK(addr("X-Forwarded-For: 203.0.113.9, 10.0.0.2\r\n") == "203.0.113.9");
        CHECK(addr("X-Forwarded-For: 203.0.113.9\r\nCF-Connecting-IP: 198.51.100.7\r\n") == "198.51.100.7");
        CHECK(addr("CF-Connecting-IP: <script>\r\n") == "");
        CHECK(addr("CF-Connecting-IP: 198.51.100.7 \t\r\n") == "198.51.100.7");   // trailing blanks
        CHECK(addr("CF-Connecting-IP: 2001:DB8::A\r\n") == "2001:db8::a");         // one spelling per address
        CHECK(addr("X-Forwarded-For:  203.0.113.9 ,10.0.0.2\r\n") == "203.0.113.9");
        // a CF-Connecting-IP that is there but not an address is no reason to take X-Forwarded-For: through Cloudflare that is a shared edge
        CHECK(addr("CF-Connecting-IP: junk!\r\nX-Forwarded-For: 203.0.113.9\r\n") == "");
        CHECK(addr("CF-Connecting-IP: " + std::string(60, '1') + "\r\n") == "");
        // the proxy's own address is the TCP peer: only then is the header believed
        CHECK(p::clientAddress("127.0.0.1", "198.51.100.7") == "198.51.100.7");
        CHECK(p::clientAddress("127.0.0.1", "") == "");
        CHECK(p::clientAddress("203.0.113.5", "198.51.100.7") == "203.0.113.5");
        CHECK(p::clientAddress("203.0.113.5", "") == "203.0.113.5");
    }

    // a phone or tablet's browser (its lobby WebSocket's User-Agent) plays online at 30 Hz, whatever its page says
    TEST(half_rate_user_agents_are_phones_and_tablets)
    {
        CHECK(p::halfRateUserAgent("Mozilla/5.0 (Linux; Android 15; Pixel 9 Pro XL) AppleWebKit/537.36 (KHTML, like Gecko) "
                                   "Chrome/131.0.0.0 Mobile Safari/537.36"));
        CHECK(p::halfRateUserAgent("Mozilla/5.0 (Linux; Android 14; SM-X710) AppleWebKit/537.36 (KHTML, like Gecko) "
                                   "Chrome/131.0.0.0 Safari/537.36"));   // an Android tablet
        CHECK(!p::halfRateUserAgent("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
                                    "Chrome/131.0.0.0 Safari/537.36"));
        CHECK(!p::halfRateUserAgent("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"));
        CHECK(!p::halfRateUserAgent(""));
    }

    TEST(ws_frame_roundtrip_masked)
    {
        using namespace wotm_net::ws;
        const std::vector<uint8_t> payload = bytesOf("hello lockstep");
        const std::vector<uint8_t> wire = encode(kBinary, payload.data(), payload.size(), true);
        FrameParser p(true);
        p.feed(wire.data(), wire.size());
        Frame f;
        CHECK(p.next(f) == FrameParser::Result::Ok);
        CHECK(f.opcode == kBinary && f.payload == payload);
        CHECK(p.next(f) == FrameParser::Result::NeedMore);
    }

    TEST(ws_frame_sizes_use_the_long_length_forms)
    {
        using namespace wotm_net::ws;
        for (size_t n : {size_t(125), size_t(126), size_t(300), size_t(65535)})
        {
            const std::vector<uint8_t> payload(n, 0x5A);
            const std::vector<uint8_t> wire = encode(kBinary, payload.data(), payload.size(), false);
            FrameParser p(false);   // a client reads the server's unmasked frames
            p.feed(wire.data(), wire.size());
            Frame f;
            CHECK(p.next(f) == FrameParser::Result::Ok && f.payload.size() == n);
        }
    }

    TEST(ws_frame_split_across_feeds)
    {
        using namespace wotm_net::ws;
        const std::vector<uint8_t> wire = encode(kText, reinterpret_cast<const uint8_t *>("LIST"), 4, true);
        FrameParser p(true);
        Frame f;
        for (size_t i = 0; i + 1 < wire.size(); ++i)
        {
            p.feed(&wire[i], 1);
            CHECK(p.next(f) == FrameParser::Result::NeedMore);
        }
        p.feed(&wire.back(), 1);
        CHECK(p.next(f) == FrameParser::Result::Ok && f.opcode == kText && f.payload == bytesOf("LIST"));
    }

    TEST(ws_fragmented_message_is_joined)
    {
        using namespace wotm_net::ws;
        std::vector<uint8_t> wire = encode(kText, reinterpret_cast<const uint8_t *>("HEL"), 3, true, false);
        const std::vector<uint8_t> rest = encode(kContinuation, reinterpret_cast<const uint8_t *>("LO 4"), 4, true, true);
        wire.insert(wire.end(), rest.begin(), rest.end());
        FrameParser p(true);
        p.feed(wire.data(), wire.size());
        Frame f;
        CHECK(p.next(f) == FrameParser::Result::Ok && f.opcode == kText && f.payload == bytesOf("HELLO 4"));
    }

    TEST(ws_rejects_unmasked_client_frame)
    {
        using namespace wotm_net::ws;
        const std::vector<uint8_t> wire = encode(kText, reinterpret_cast<const uint8_t *>("LIST"), 4, false);
        FrameParser p(true);   // the server requires masked frames (RFC 6455 5.1)
        p.feed(wire.data(), wire.size());
        Frame f;
        CHECK(p.next(f) == FrameParser::Result::Error);
    }

    TEST(ws_rejects_oversized_frame)
    {
        using namespace wotm_net::ws;
        const std::vector<uint8_t> payload(kMaxMessage + 1, 1);
        const std::vector<uint8_t> wire = encode(kBinary, payload.data(), payload.size(), true);
        FrameParser p(true);
        p.feed(wire.data(), 16);   // the header alone is enough to refuse it
        Frame f;
        CHECK(p.next(f) == FrameParser::Result::Error);
    }

    TEST(ws_ping_pong_and_close_opcodes)
    {
        using namespace wotm_net::ws;
        std::vector<uint8_t> wire = encode(kPing, reinterpret_cast<const uint8_t *>("12345678"), 8, true);
        const std::vector<uint8_t> close = encode(kClose, nullptr, 0, true);
        wire.insert(wire.end(), close.begin(), close.end());
        FrameParser p(true);
        p.feed(wire.data(), wire.size());
        Frame f;
        CHECK(p.next(f) == FrameParser::Result::Ok && f.opcode == kPing && f.payload == bytesOf("12345678"));
        CHECK(p.next(f) == FrameParser::Result::Ok && f.opcode == kClose && f.payload.empty());
    }

    // a server with its WebSocket listener on (the browser version's lobby and match, behind the web server's proxy)
    ServerConfig wsTestConfig()
    {
        ServerConfig c = testConfig();
        c.wsPort = freePort();
        return c;
    }

    std::string wsUrl(const ServerConfig &c, const std::string &path) { return "ws://127.0.0.1:" + std::to_string(c.wsPort) + path; }

    // a browser in the lobby: the same lines as TCP, one per text frame
    TEST(ws_lobby_member_joins_a_tcp_members_lobby)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        CHECK(code.size() == 4u);
        wotm_net::WsLink b;
        std::string err;
        CHECK(b.connect(wsUrl(cfg, "/lobby"), 2000, err));
        b.sendLine("HELLO " + std::to_string(p::kProtocol) + " b1");
        CHECK(readUntil(b, "WELCOME") == "WELCOME 1");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:3,") == "MEMBERS " + code + " Ana Ana:3,Bia:1");
        a.sendLine("START");
        const auto mb = p::keyValues(p::words(readUntil(b, "MATCH")));
        CHECK(mb.count("you") && mb.at("you") == "1");
    }

    // a removed member can't come back from the same address under another name, though a different address (a proxy's header) can join;
    // the server believes the header only from a loopback peer
    TEST(ws_lobby_ban_follows_the_clients_address_behind_the_proxy)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        const auto viaProxy = [&](wotm_net::WsLink &l, const std::string &address) {
            std::string err;
            CHECK(l.connect(wsUrl(cfg, "/lobby"), 2000, err, "CF-Connecting-IP: " + address + "\r\n"));
            l.sendLine("HELLO " + std::to_string(p::kProtocol) + " b1");
            CHECK(readUntil(l, "WELCOME").rfind("WELCOME", 0) == 0);
        };
        wotm_net::WsLink bia, bia2, dee;
        viaProxy(bia, "198.51.100.7");
        bia.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(bia, "JOINED") == "JOINED " + code);
        a.sendLine("KICK Bia");
        CHECK(readUntil(bia, "KICKED").rfind("KICKED " + code, 0) == 0);
        viaProxy(bia2, "198.51.100.7");
        bia2.sendLine("JOIN " + code + " Bia2 1");
        CHECK(readUntil(bia2, "ERROR").rfind("ERROR you were removed from this lobby", 0) == 0);
        viaProxy(dee, "198.51.100.8");
        dee.sendLine("JOIN " + code + " Dee 1");
        CHECK(readUntil(dee, "JOINED") == "JOINED " + code);
    }

    // a waiting lobby tells its members each one's round trip: measured for a browser (WebSocket pings), "-" for a TCP member
    TEST(ws_lobby_members_see_each_others_ping)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        wotm_net::WsLink b;
        std::string err;
        CHECK(b.connect(wsUrl(cfg, "/lobby"), 2000, err));
        b.sendLine("HELLO " + std::to_string(p::kProtocol) + " b1");
        CHECK(readUntil(b, "WELCOME") == "WELCOME 1");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        std::string pings, line;
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < until)
        {
            b.pump();   // answers the server's pings
            if (!a.readLine(line, 50) || line.rfind("PINGS " + code + " ", 0) != 0 || line.find("Bia:") == std::string::npos)
                continue;
            pings = line;   // the first ones may come before Bia's first pong
            if (line.find("Bia:-") == std::string::npos)
                break;
        }
        CHECK(pings.rfind("PINGS " + code + " Ana:-,Bia:", 0) == 0);
        CHECK(pings.find("Bia:-") == std::string::npos);
        CHECK(pings.find_first_not_of("0123456789", pings.find("Bia:") + 4) == std::string::npos);
    }

    // the owner hands the lobby to another member (OWNER <name>): MEMBERS names the new owner to everyone and only the new owner may
    // change the settings and start; a guest, a stranger, oneself and a name that is not a member are refused
    TEST(lobby_owner_hands_the_lobby_to_another_member)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a, b, c;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        CHECK(hello(b, cfg.port) == "WELCOME 1");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:3,") == "MEMBERS " + code + " Ana Ana:3,Bia:1");

        b.sendLine("OWNER Bia");
        CHECK(readUntil(b, "ERROR") == "ERROR only the lobby owner can hand the lobby to someone else");
        a.sendLine("OWNER Ana");
        CHECK(readUntil(a, "ERROR") == "ERROR you are the owner already");
        a.sendLine("OWNER Zed");
        CHECK(readUntil(a, "ERROR") == "ERROR Zed is not in this lobby");
        CHECK(hello(c, cfg.port) == "WELCOME 1");
        c.sendLine("OWNER Bia");
        CHECK(readUntil(c, "ERROR") == "ERROR join a lobby first");

        a.sendLine("OWNER Bia");
        CHECK(readUntil(a, "MEMBERS " + code + " Bia ") == "MEMBERS " + code + " Bia Ana:3,Bia:1");
        CHECK(readUntil(b, "MEMBERS " + code + " Bia ") == "MEMBERS " + code + " Bia Ana:3,Bia:1");
        a.sendLine("SET wins=3");
        CHECK(readUntil(a, "ERROR").find("owner") != std::string::npos);
        a.sendLine("START");
        CHECK(readUntil(a, "ERROR").find("owner") != std::string::npos);
        b.sendLine("SET wins=3");
        CHECK(readUntil(b, "SETTINGS") == "SETTINGS " + code + " stage=1 wins=3 mods=");
        // the old owner leaving no longer moves the lobby: Bia keeps it
        a.sendLine("LEAVE");
        CHECK(readUntil(b, "MEMBERS " + code + " Bia ") == "MEMBERS " + code + " Bia Bia:1");
    }

    // the owner removes a member (KICK <name>): the member is told (KICKED <code> <seconds>), the others see the new list, and the
    // member can't join that lobby again until the ban (cfg.kickBanMs) ends; only the owner may, and only before the match
    TEST(lobby_owner_kicks_a_member_who_cannot_rejoin_for_a_while)
    {
        ServerConfig cfg = testConfig();
        cfg.kickBanMs = 1500;
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a, b, c;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        CHECK(hello(b, cfg.port) == "WELCOME 1");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        CHECK(hello(c, cfg.port) == "WELCOME 1");
        c.sendLine("JOIN " + code + " Cai 2");
        CHECK(readUntil(c, "JOINED") == "JOINED " + code);
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:3,Bia:1,Cai:2") == "MEMBERS " + code + " Ana Ana:3,Bia:1,Cai:2");

        b.sendLine("KICK Cai");
        CHECK(readUntil(b, "ERROR") == "ERROR only the lobby owner can remove a member");
        a.sendLine("KICK Ana");
        CHECK(readUntil(a, "ERROR") == "ERROR you can't remove yourself; leave instead");
        a.sendLine("KICK Zed");
        CHECK(readUntil(a, "ERROR") == "ERROR Zed is not in this lobby");

        a.sendLine("KICK Bia");
        CHECK(readUntil(b, "KICKED") == "KICKED " + code + " 2");
        CHECK(readUntil(a, "MEMBERS " + code + " Ana Ana:3,Cai:2") == "MEMBERS " + code + " Ana Ana:3,Cai:2");
        CHECK(readUntil(c, "MEMBERS " + code + " Ana Ana:3,Cai:2") == "MEMBERS " + code + " Ana Ana:3,Cai:2");
        // the kicked connection stays open and is out of the lobby: lobby commands are refused, and the lobby is closed to its name
        b.sendLine("CHAT hello");
        CHECK(readUntil(b, "ERROR") == "ERROR join a lobby first");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "ERROR") == "ERROR you were removed from this lobby; try again in 2 s");
        // someone else is welcome meanwhile, and Bia can come back once the ban is over
        LineSocket d;
        CHECK(hello(d, cfg.port) == "WELCOME 1");
        d.sendLine("JOIN " + code + " Dee 4");
        CHECK(readUntil(d, "JOINED") == "JOINED " + code);
        std::this_thread::sleep_for(std::chrono::milliseconds(1700));
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
    }

    // once the match has started the lobby is closed to KICK and OWNER (the match owns its members)
    TEST(lobby_kick_is_refused_once_the_match_started)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        CHECK(ms.size() == 2u);
        if (ms.size() < 2)
            return;
        ms[0]->socket.sendLine("KICK Bia");
        CHECK(readUntil(ms[0]->socket, "ERROR") == "ERROR the match has started");
        ms[0]->socket.sendLine("OWNER Bia");
        CHECK(readUntil(ms[0]->socket, "ERROR") == "ERROR the match has started");
    }

    // HEALTH <own fps> <work ms> <hz>: a page tells the lobby how its device ran its last match; the lobby relays every member's
    // numbers each second with the pings (HEALTH <code> <name:fps:work_ms:hz,...>); a member that sent none is left out; bad numbers
    // are refused
    TEST(lobby_members_see_each_others_device_health)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a, b;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        CHECK(hello(b, cfg.port) == "WELCOME 1");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        for (const char *bad : {"HEALTH", "HEALTH 28 13", "HEALTH x 13 60", "HEALTH 28 -1 60", "HEALTH 28 13 0", "HEALTH 99999 13 60", "HEALTH nan 13 60"})
        {
            b.sendLine(bad);
            CHECK(readUntil(b, "ERROR") == "ERROR HEALTH <own fps> <work ms> <hz>: numbers in range");
        }
        b.sendLine("HEALTH 28.46 13.2 60");
        CHECK(readUntil(a, "HEALTH " + code + " ") == "HEALTH " + code + " Bia:28.5:13.2:60");
        a.sendLine("HEALTH 60 4.5 30");
        CHECK(readUntil(b, "HEALTH " + code + " Ana:60.0:4.5:30,Bia") == "HEALTH " + code + " Ana:60.0:4.5:30,Bia:28.5:13.2:60");
        // the numbers leave with the member: Bia back without sending any is not listed
        b.sendLine("LEAVE");
        CHECK(readUntil(b, "LEFT") == "LEFT " + code);
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        std::string alone;
        for (int i = 0; i < 4 && alone != "HEALTH " + code + " Ana:60.0:4.5:30"; ++i)   // lines sent before the LEAVE may still arrive
            alone = readUntil(a, "HEALTH " + code + " ");
        CHECK(alone == "HEALTH " + code + " Ana:60.0:4.5:30");
    }

    // plain chat in a waiting lobby: everyone in it (the sender too) gets "CHAT <code> <name> <text>"; control characters are dropped,
    // the text is cut at proto::kMaxChatBytes, and a member can send only proto::kChatBurst lines per proto::kChatWindowMs
    TEST(lobby_chat_reaches_the_lobby_is_cleaned_and_rate_limited)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        wotm_net::WsLink b;
        std::string err;
        CHECK(b.connect(wsUrl(cfg, "/lobby"), 2000, err));
        b.sendLine("HELLO " + std::to_string(p::kProtocol) + " b1");
        CHECK(readUntil(b, "WELCOME") == "WELCOME 1");
        b.sendLine("JOIN " + code + " Bia 1");
        CHECK(readUntil(b, "JOINED") == "JOINED " + code);
        a.sendLine("CHAT  hello \t there ");
        CHECK(readUntil(b, "CHAT ") == "CHAT " + code + " Ana hello  there");
        CHECK(readUntil(a, "CHAT ") == "CHAT " + code + " Ana hello  there");
        a.sendLine("CHAT " + std::string(p::kMaxChatBytes + 50, 'x'));
        CHECK(readUntil(b, "CHAT ") == "CHAT " + code + " Ana " + std::string(p::kMaxChatBytes, 'x'));
        a.sendLine("CHAT bad \xff\xfe bytes");   // not UTF-8: a browser would close its connection on it
        CHECK(readUntil(a, "ERROR").rfind("ERROR chat text is not valid text", 0) == 0);
        a.sendLine("CHAT caf\xc3\xa9");
        CHECK(readUntil(b, "CHAT ") == "CHAT " + code + " Ana caf\xc3\xa9");
        a.sendLine("CHAT    ");   // nothing to say: nothing sent, nothing counted
        for (int i = 0; i < p::kChatBurst; ++i)
            a.sendLine("CHAT burst");
        const std::string refused = readUntil(a, "ERROR");
        CHECK(refused.rfind("ERROR chat too fast", 0) == 0);
        LineSocket stranger;
        CHECK(hello(stranger, cfg.port) == "WELCOME 1");
        stranger.sendLine("CHAT hi");
        CHECK(readUntil(stranger, "ERROR").rfind("ERROR join a lobby first", 0) == 0);
    }

    TEST(lobby_chat_is_closed_once_the_match_starts)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        ms[0]->socket.sendLine("CHAT too late");
        CHECK(readUntil(ms[0]->socket, "ERROR").rfind("ERROR chat is only open before the match", 0) == 0);
    }

    TEST(ws_match_clients_share_every_frame_of_input)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        ClientConfig ca = clientFor(cfg.port, *ms[0]), cb = clientFor(cfg.port, *ms[1]);
        ca.host = cb.host = wsUrl(cfg, "/match");
        MatchClient a, b;
        std::string err;
        CHECK(a.start(ca, err) && b.start(cb, err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        CHECK(a.localPlayer() == 0 && b.localPlayer() == 1 && a.delay() == b.delay());
        exchange(a, b, 300);
        CHECK(a.status() == Status::Running && b.status() == Status::Running);
    }

    // a browser and a native client in the same match: the server relays between ENet and WebSocket
    TEST(ws_match_mixes_enet_and_websocket_clients)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        ClientConfig ca = clientFor(cfg.port, *ms[0]), cb = clientFor(cfg.port, *ms[1]);
        cb.host = wsUrl(cfg, "/match");
        MatchClient a, b;
        std::string err;
        CHECK(a.start(ca, err) && b.start(cb, err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        exchange(a, b, 300);
    }

    // the test's packet loss is an ENet thing: a WebSocket loses nothing, and the server sends it each input once
    TEST(ws_match_survives_the_servers_test_packet_loss)
    {
        ServerConfig cfg = wsTestConfig();
        cfg.dropInputPercent = 30;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        ClientConfig ca = clientFor(cfg.port, *ms[0]), cb = clientFor(cfg.port, *ms[1]);
        cb.host = wsUrl(cfg, "/match");
        CHECK(a.start(ca, err) && b.start(cb, err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        exchange(a, b, 200);
    }

    // a malformed frame (unmasked, from a client) closes that connection only; the server and its other clients carry on
    TEST(ws_bad_frame_closes_only_that_client)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        LineSocket raw;
        CHECK(raw.connect("127.0.0.1", cfg.wsPort, 2000));
        const std::string req = wotm_net::ws::handshakeRequest("127.0.0.1", "/lobby", "dGhlIHNhbXBsZSBub25jZQ==");
        raw.sendBytes(req.data(), req.size());
        const std::vector<uint8_t> bad = wotm_net::ws::encode(wotm_net::ws::kText, reinterpret_cast<const uint8_t *>("LIST"), 4, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        raw.sendBytes(bad.data(), bad.size());
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool closed = false;
        while (!closed && std::chrono::steady_clock::now() < until)
        {
            closed = !raw.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CHECK(closed);
        wotm_net::WsLink good;
        std::string err;
        CHECK(good.connect(wsUrl(cfg, "/lobby"), 2000, err));
        good.sendLine("HELLO " + std::to_string(p::kProtocol) + " b1");
        CHECK(readUntil(good, "WELCOME") == "WELCOME 0");
    }

    // a browser whose traffic was held up (a stalled tab, a network that stopped and came back) sends everything it queued at once:
    // hundreds of KB of small input messages in one go must not cost it the connection
    TEST(ws_burst_of_queued_messages_keeps_the_connection)
    {
        using namespace wotm_net::ws;
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        LineSocket raw;
        CHECK(raw.connect("127.0.0.1", cfg.wsPort, 2000));
        const std::string req = handshakeRequest("127.0.0.1", "/match", "dGhlIHNhbXBsZSBub25jZQ==");
        raw.sendBytes(req.data(), req.size());
        auto frame = [](uint8_t channel, const std::vector<uint8_t> &body) {
            std::vector<uint8_t> packet{channel};
            packet.insert(packet.end(), body.begin(), body.end());
            return encode(kBinary, packet.data(), packet.size(), true);
        };
        // about a minute of an 8-player client's input messages (acks for every player, a few frames of its own), then its Join
        p::Input in;
        in.acks.assign(8, 300u);
        in.own.player = 7, in.own.first = 300, in.own.states.assign(2, PadState{});
        const std::vector<uint8_t> one = frame(p::kInputs, p::encode(in));
        std::vector<uint8_t> burst;
        while (burst.size() < 256u * 1024u)
            burst.insert(burst.end(), one.begin(), one.end());
        p::Join j;
        j.token = ms[0]->match["token"], j.buildId = "b1", j.configHash = "cfg", j.name = "Ana";
        const std::vector<uint8_t> join = frame(p::kControl, p::encode(j));
        burst.insert(burst.end(), join.begin(), join.end());
        CHECK(raw.sendBytes(burst.data(), burst.size()));
        std::string head;
        FrameParser parser(false);
        bool welcomed = false, alive = true;
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (alive && !welcomed && std::chrono::steady_clock::now() < until)
        {
            alive = raw.pump();
            const std::string bytes = raw.takeReceived();
            if (head.find("\r\n\r\n") == std::string::npos)
            {
                head += bytes;
                const size_t end = head.find("\r\n\r\n");
                if (end != std::string::npos)
                    parser.feed(reinterpret_cast<const uint8_t *>(head.data()) + end + 4, head.size() - end - 4);
            }
            else
                parser.feed(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
            Frame f;
            while (parser.next(f) == FrameParser::Result::Ok)
                welcomed = welcomed || (f.opcode == kBinary && f.payload.size() > 1 &&
                                        p::typeOf(std::vector<uint8_t>(f.payload.begin() + 1, f.payload.end())) == p::Type::Welcome);
            raw.waitReadable(10);
        }
        CHECK(welcomed);
    }

    // a browser whose network stopped: over a WebSocket (reliable and ordered) the server sends it each input once, not again every
    // tick while it doesn't answer, so a long outage neither fills the connection's buffers (the server would block on it, then drop the
    // player) nor leaves the player a minute of repeats to chew through when it comes back. Only the server's pings keep going.
    TEST(ws_silent_player_is_not_sent_repeats)
    {
        using namespace wotm_net::ws;
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        const std::vector<std::string> names = {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8"};
        auto ms = formMatch(cfg.port, names, {3, 1, 2, 5, 4, 7, 8, 9});
        std::vector<std::unique_ptr<MatchClient>> cs;
        std::string err;
        for (size_t i = 0; i + 1 < ms.size(); ++i)
        {
            cs.push_back(std::make_unique<MatchClient>());
            CHECK(cs.back()->start(clientFor(cfg.port, *ms[i]), err));
        }
        LineSocket raw;
        CHECK(raw.connect("127.0.0.1", cfg.wsPort, 2000));
        const std::string req = handshakeRequest("127.0.0.1", "/match", "dGhlIHNhbXBsZSBub25jZQ==");
        raw.sendBytes(req.data(), req.size());
        p::Join j;
        j.token = ms[7]->match["token"], j.buildId = "b1", j.configHash = "cfg", j.name = "P8";
        std::vector<uint8_t> packet{p::kControl};
        const std::vector<uint8_t> body = p::encode(j);
        packet.insert(packet.end(), body.begin(), body.end());
        const std::vector<uint8_t> join = encode(kBinary, packet.data(), packet.size(), true);
        raw.sendBytes(join.data(), join.size());
        bool started = true;
        for (auto &c : cs)
            started = started && c->waitStarted(5000);
        CHECK(started);
        if (!started)
            return;
        // the seven play a second ahead; P8 never answers, so they end up waiting for it
        const uint32_t d = cs[0]->delay();
        for (uint32_t f = 0; f < 30; ++f)
            for (auto &c : cs)
                c->pushLocal(f + d, pressed(1));
        // what reaches P8: the bytes, and per player the first frame it doesn't have yet (from the Inputs it decodes)
        std::string head;
        FrameParser parser(false);
        std::vector<uint32_t> have(8, 0u);
        auto drain = [&] {
            size_t total = 0;
            for (bool alive = true; alive;)
            {
                alive = raw.pump();
                const std::string bytes = raw.takeReceived();
                total += bytes.size();
                if (bytes.empty())
                    break;
                if (head.find("\r\n\r\n") == std::string::npos)
                {
                    head += bytes;
                    const size_t end = head.find("\r\n\r\n");
                    if (end != std::string::npos)
                        parser.feed(reinterpret_cast<const uint8_t *>(head.data()) + end + 4, head.size() - end - 4);
                }
                else
                    parser.feed(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
                Frame f;
                p::Inputs got;
                while (parser.next(f) == FrameParser::Result::Ok)
                    if (f.opcode == kBinary && f.payload.size() > 1 && p::decode(std::vector<uint8_t>(f.payload.begin() + 1, f.payload.end()), got))
                        for (const p::InputRun &r : got.runs)
                            if (r.first <= have[r.player])
                                have[r.player] = std::max(have[r.player], r.first + static_cast<uint32_t>(r.states.size()));
            }
            return total;
        };
        // every frame of the seven reaches it
        auto complete = [&] {
            for (size_t q = 0; q < 7; ++q)
                if (have[q] < d + 30)
                    return false;
            return true;
        };
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!complete() && std::chrono::steady_clock::now() < until)
        {
            drain();
            raw.waitReadable(10);
        }
        CHECK(complete());
        // and nothing more while it stays silent
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        drain();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        const size_t silent = drain();
        CHECK(silent < 1024u);   // 2 s of pings (20 of 10 bytes)
        CHECK(raw.open());
    }

    // a lobby connection that sends 64 KB without ending a line is dropped
    TEST(lobby_line_without_end_closes_the_connection)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket s;
        CHECK(hello(s, cfg.port) == "WELCOME 0");
        const std::string junk(70000, 'a');
        s.sendBytes(junk.data(), junk.size());
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool closed = false;
        while (!closed && std::chrono::steady_clock::now() < until)
        {
            closed = !s.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CHECK(closed);
    }

    TEST(ws_unknown_path_is_refused)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        wotm_net::WsLink w;
        std::string err;
        const bool connected = w.connect(wsUrl(cfg, "/other"), 2000, err);
        bool alive = connected;
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (alive && std::chrono::steady_clock::now() < until)
        {
            alive = w.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CHECK(!alive);
    }

    // one connection holds one player: a second Join (another member's token) is refused and the connection closed, so the slot
    // stays free for its owner and no slot is left pointing at a connection that went away
    TEST(ws_match_connection_cannot_take_two_slots)
    {
        ServerConfig cfg = wsTestConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        wotm_net::WsLink w;
        std::string err;
        CHECK(w.connect(wsUrl(cfg, "/match"), 2000, err));
        auto joinPacket = [&](const std::string &token) {
            p::Join j;
            j.token = token, j.buildId = "b1", j.configHash = "cfg", j.name = "x";
            std::vector<uint8_t> packet{p::kControl};
            const std::vector<uint8_t> body = p::encode(j);
            packet.insert(packet.end(), body.begin(), body.end());
            return packet;
        };
        w.sendPacket(joinPacket(ms[0]->match["token"]));
        w.sendPacket(joinPacket(ms[1]->match["token"]));
        std::vector<p::Type> got;
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool alive = true;
        while (alive && got.size() < 2 && std::chrono::steady_clock::now() < until)
        {
            alive = w.pump();
            std::vector<uint8_t> packet;
            while (w.nextPacket(packet))
                if (packet.size() > 1)
                    got.push_back(p::typeOf(std::vector<uint8_t>(packet.begin() + 1, packet.end())));
            w.waitReadable(10);
        }
        CHECK(got.size() == 2u && got[0] == p::Type::Welcome && got[1] == p::Type::Reject);
        w.close();
        // Bia's slot is still free: her own client gets in, and the match starts once both are there
        MatchClient b;
        ClientConfig cb = clientFor(cfg.port, *ms[1]);
        CHECK(b.start(cb, err));
        const auto joined = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (b.status() == Status::Connecting && std::chrono::steady_clock::now() < joined)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(b.status() == Status::Waiting);
    }

    TEST(ws_match_client_is_lost_when_the_server_stops)
    {
        ServerConfig cfg = wsTestConfig();
        auto rs = std::make_unique<RunningServer>(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        ClientConfig ca = clientFor(cfg.port, *ms[0]);
        ca.host = wsUrl(cfg, "/match");
        MatchClient a;
        std::string err;
        CHECK(a.start(ca, err));
        const auto joined = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (a.status() == Status::Connecting && std::chrono::steady_clock::now() < joined)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(a.status() == Status::Waiting);
        rs.reset();
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (a.status() != Status::Lost && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(a.status() == Status::Lost);
    }

    // ---- diagnostics: DIAG lines from the pages, the server's own per-player view, the per-match files ----

    namespace fs = std::filesystem;
    namespace diag = wotm_net::diag;

    fs::path tempDir(const std::string &tag)
    {
        const fs::path dir = fs::temp_directory_path() / ("wotm_net_test_" + tag + "_" + std::to_string(freePort()));
        fs::remove_all(dir);
        fs::create_directories(dir);
        return dir;
    }

    std::string readFile(const fs::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // everything the server wrote under `dir`, files in name order
    std::string readDiagDir(const fs::path &dir)
    {
        std::vector<fs::path> files;
        for (const auto &e : fs::directory_iterator(dir))
            files.push_back(e.path());
        std::sort(files.begin(), files.end());
        std::string all;
        for (const fs::path &f : files)
            all += readFile(f);
        return all;
    }

    // waits (up to 3 s) until what the server wrote under `dir` contains `text`; returns it all
    std::string waitForDiag(const fs::path &dir, const std::string &text)
    {
        std::string all;
        for (int i = 0; i < 300; ++i)
        {
            all = readDiagDir(dir);
            if (all.find(text) != std::string::npos)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return all;
    }

    size_t countOf(const std::string &haystack, const std::string &needle)
    {
        size_t n = 0;
        for (size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1))
            ++n;
        return n;
    }

    TEST(diag_report_keeps_clean_fields_and_a_decoded_note)
    {
        diag::Report r;
        CHECK(diag::parse(p::words("DIAG v=1 why=periodic fps=59.8 lobby=AB2C note=slow%20and%20laggy%20caf%C3%A9"), r));
        CHECK(r.get("fps") == "59.8" && r.get("why") == "periodic" && r.get("lobby") == "AB2C");
        CHECK(r.get("missing").empty());
        CHECK(r.note == "slow and laggy caf\xc3\xa9");
        CHECK(diag::format(r) == "v=1 why=periodic fps=59.8 lobby=AB2C note=\"slow and laggy caf\xc3\xa9\"");
    }

    TEST(diag_report_drops_bad_keys_long_values_control_characters_and_bad_notes)
    {
        diag::Report r;
        CHECK(diag::parse(p::words("DIAG Bad-Key=1 ok=1 long=" + std::string(diag::kMaxValueBytes + 1, 'x') + " a=b=c quote=a\"b empty= noeq 9x=1 " +
                                   "note=a%0Ab%00c%7F%22quoted%22%5Cslash"), r));
        CHECK(r.fields.size() == 1u && r.get("ok") == "1");   // the bad key, the long value, a second '=', a quote, an empty value, a word without '=' are dropped
        CHECK(r.note == "abc\"quoted\"\\slash");               // control characters go, the rest stays
        CHECK(diag::format(r) == "ok=1 note=\"abc\\\"quoted\\\"\\\\slash\"");   // quotes and backslashes are escaped in the log line

        diag::Report bad;
        CHECK(diag::parse(p::words("DIAG ok=1 note=%FF%FEbad"), bad));
        CHECK(bad.note.empty());   // not UTF-8: the note is dropped, the numbers stay
        diag::Report long_;
        CHECK(diag::parse(p::words("DIAG note=" + std::string(diag::kMaxNoteBytes + 50, 'n')), long_));
        CHECK(long_.note.size() == diag::kMaxNoteBytes);
        diag::Report many;
        std::string line = "DIAG";
        for (size_t i = 0; i < diag::kMaxFields + 10; ++i)
            line += " k" + std::to_string(i) + "=1";
        CHECK(diag::parse(p::words(line), many) && many.fields.size() == diag::kMaxFields);
        diag::Report none;
        CHECK(!diag::parse(p::words("DIAG"), none));
        CHECK(!diag::parse(p::words("DIAG Bad-Key=1"), none));
    }

    TEST(diag_archive_writes_per_match_files_and_prunes_old_ones_and_the_oldest_over_the_cap)
    {
        const fs::path dir = tempDir("archive");
        diag::Limits lim;
        lim.keepDays = 7;
        lim.maxTotalBytes = 300;
        lim.maxFileBytes = 100;
        diag::Archive a(dir.string(), lim);
        CHECK(a.enabled());
        const std::string f1 = a.begin("AB2C", "match");
        CHECK(f1.size() > 15u && f1.find("-AB2C-match.log") != std::string::npos);
        a.append(f1, "line one");
        a.append(f1, "line two");
        CHECK(readFile(dir / f1) == "line one\nline two\n");
        CHECK(a.latest("AB2C") == f1 && a.latest("ZZZZ").empty());
        const std::string f1b = a.begin("AB2C", "match");   // a second one in the same second is its own file
        CHECK(f1b != f1 && a.latest("AB2C") == f1b);

        for (int i = 0; i < 20; ++i)
            a.append(f1, std::string(20, 'x'));   // past the per-file cap: dropped, with one marker
        const std::string capped = readFile(dir / f1);
        CHECK(capped.size() < 100u + 60u && countOf(capped, "truncated") == 1u);

        const std::string stale = a.begin("OLD2", "lobby");
        a.append(stale, "old");
        fs::last_write_time(dir / stale, fs::file_time_type::clock::now() - std::chrono::hours(24 * 8));
        a.prune();
        CHECK(!fs::exists(dir / stale) && fs::exists(dir / f1));

        // over the total: the oldest go first, the newest stays
        std::vector<std::string> names;
        for (int i = 0; i < 4; ++i)
        {
            names.push_back(a.begin("T" + std::to_string(i) + "XX", "match"));
            a.append(names.back(), std::string(90, 'y'));
            fs::last_write_time(dir / names.back(), fs::file_time_type::clock::now() - std::chrono::hours(1 + (3 - i)));
        }
        a.prune();
        uint64_t total = 0;
        for (const auto &e : fs::directory_iterator(dir))
            total += fs::file_size(e.path());
        CHECK(total <= lim.maxTotalBytes);
        CHECK(fs::exists(dir / names[3]) && !fs::exists(dir / names[0]));
    }

    TEST(diag_archive_is_off_without_a_folder)
    {
        diag::Archive a("", diag::Limits{});
        CHECK(!a.enabled());
        CHECK(a.begin("AB2C", "match").empty());
        a.append("", "nowhere");
        a.prune();
    }

    // a page's report is journaled under its player's name and lobby, in that lobby's file; chat text never reaches the files
    TEST(diag_report_is_filed_under_the_player_and_never_holds_the_chat)
    {
        const fs::path dir = tempDir("report");
        ServerConfig cfg = testConfig();
        cfg.diagDir = dir.string();
        RunningServer rs(cfg);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        a.sendLine("CHAT my secret plans");
        a.sendLine("DIAG v=1 why=join cores=8 dpr=2 ua=Chrome-131");
        a.sendLine("DIAG v=1 why=periodic fps=58.1 gfps=30 lobby=ZZZZ name=Mallory");   // what the connection is in wins over what it claims
        const std::string all = waitForDiag(dir, "fps=58.1");
        CHECK(all.find("src=client lobby=" + code + " player=Ana v=1 why=join cores=8 dpr=2 ua=Chrome-131") != std::string::npos);
        CHECK(all.find("player=Ana v=1 why=periodic fps=58.1 gfps=30") != std::string::npos);
        CHECK(all.find("Mallory") == std::string::npos && all.find("ZZZZ") == std::string::npos);
        CHECK(all.find("secret") == std::string::npos);
        CHECK(all.size() > 21u && all[4] == '-' && all[10] == 'T' && all[19] == 'Z' && all[20] == ' ');   // each line starts with an ISO UTC time
    }

    // After a match the page is back on its front panel, with no lobby connection: its report says which lobby and who it was. Nothing
    // proves that, so it is filed apart (reports-<date>.log) and labeled as a claim: it can't put lines into a lobby's own file
    TEST(diag_report_from_outside_a_lobby_is_filed_apart_as_a_claim)
    {
        const fs::path dir = tempDir("stray");
        ServerConfig cfg = testConfig();
        cfg.diagDir = dir.string();
        RunningServer rs(cfg);
        LineSocket a, stray;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        CHECK(hello(stray, cfg.port) == "WELCOME 1");
        stray.sendLine("DIAG v=1 why=report lobby=" + code + " name=Ana fps=12 note=laggy%20after%20the%20first%20minute");
        stray.sendLine("DIAG v=1 why=report lobby=WXYZ name=Zed fps=13 note=no%20such%20lobby");
        stray.sendLine("DIAG v=1 why=report fps=14");
        const std::string all = waitForDiag(dir, "fps=14");
        CHECK(all.find("src=client_outside claimed_lobby=" + code + " claimed_player=Ana v=1 why=report fps=12 note=\"laggy after the first minute\"") !=
              std::string::npos);
        CHECK(all.find("src=client_outside claimed_lobby=WXYZ claimed_player=Zed v=1 why=report fps=13") != std::string::npos);
        CHECK(all.find("src=client_outside claimed_lobby=- claimed_player=- v=1 why=report fps=14") != std::string::npos);
        size_t looseFiles = 0;
        for (const auto &e : fs::directory_iterator(dir))
        {
            const std::string name = e.path().filename().string();
            if (name.rfind("reports-", 0) == 0)
            {
                ++looseFiles;
                CHECK(readFile(e.path()).find("fps=12") != std::string::npos);
            }
            else
                CHECK(readFile(e.path()).find("fps=12") == std::string::npos);   // the lobby's own file never holds a claim
        }
        CHECK(looseFiles == 1u);
    }

    // a client's keys never read as the server's: the line's own fields (src, lobby, player, event, ...) can't be repeated by a report
    TEST(diag_report_cannot_repeat_the_servers_own_keys)
    {
        const fs::path dir = tempDir("keys");
        ServerConfig cfg = testConfig();
        cfg.diagDir = dir.string();
        RunningServer rs(cfg);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        const std::string code = createLobby(a, "Lobby", "Ana", 3);
        a.sendLine("DIAG src=server event=match_over player=Bia slot=1 transport=ws fps=9");
        const std::string all = waitForDiag(dir, "fps=9");
        CHECK(all.find("src=client lobby=" + code + " player=Ana fps=9") != std::string::npos);
        CHECK(countOf(all, "event=match_over") == 0u && all.find("player=Bia") == std::string::npos && all.find("slot=1") == std::string::npos);
    }

    // a lobby member can't fill a file so far that the server's own lines stop: reports only get part of the cap
    TEST(diag_reports_leave_room_for_the_servers_own_lines)
    {
        const fs::path dir = tempDir("room");
        diag::Limits lim;
        lim.maxFileBytes = 400;
        diag::Archive a(dir.string(), lim);
        const std::string file = a.begin("AB2C", "match");
        for (int i = 0; i < 20; ++i)
            a.append(file, std::string(40, 'c'), false);
        const size_t afterClients = readFile(dir / file).size();
        CHECK(afterClients <= 300u + 60u);   // three quarters of the cap, plus the one marker line
        a.append(file, std::string(40, 's'), true);
        a.append(file, std::string(40, 's'), true);
        CHECK(readFile(dir / file).find(std::string(40, 's')) != std::string::npos);
        CHECK(readFile(dir / file).size() <= 400u + 60u);
    }

    TEST(diag_reports_are_rate_limited_per_connection)
    {
        const fs::path dir = tempDir("rate");
        ServerConfig cfg = testConfig();
        cfg.diagDir = dir.string();
        RunningServer rs(cfg);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        createLobby(a, "Lobby", "Ana", 3);
        for (int i = 0; i < p::kDiagBurst + 25; ++i)
            a.sendLine("DIAG v=1 why=periodic fps=" + std::to_string(i));
        a.sendLine("PING-LIKE flush");   // answered with an ERROR once every line before it was handled
        CHECK(readUntil(a, "ERROR").rfind("ERROR unknown command", 0) == 0);
        CHECK(countOf(readDiagDir(dir), "why=periodic") == static_cast<size_t>(p::kDiagBurst));
    }

    TEST(diag_needs_a_hello_and_the_file_only_exists_when_a_folder_is_set)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);   // no diag folder: reports are only journaled
        LineSocket raw;
        CHECK(raw.connect("127.0.0.1", cfg.port, 2000));
        raw.sendLine("DIAG v=1 fps=1");
        CHECK(readUntil(raw, "ERROR").rfind("ERROR send HELLO first", 0) == 0);
        LineSocket a;
        CHECK(hello(a, cfg.port) == "WELCOME 0");
        a.sendLine("DIAG v=1 fps=1");
        a.sendLine("DIAG garbage");   // nothing usable: ignored, no answer
        a.sendLine("LIST");
        CHECK(readUntil(a, "END") == "END");   // the connection still works
    }

    // the server's own view of each player in a started match: transport, Hz, delay, round trip, how input packets arrived
    TEST(diag_server_view_reports_each_player_of_a_match)
    {
        const fs::path dir = tempDir("view");
        ServerConfig cfg = wsTestConfig();
        cfg.diagDir = dir.string();
        cfg.diagEveryMs = 150;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        ClientConfig ca = clientFor(cfg.port, *ms[0]), cb = clientFor(cfg.port, *ms[1]);
        cb.host = wsUrl(cfg, "/match");
        MatchClient a, b;
        std::string err;
        CHECK(a.start(ca, err) && b.start(cb, err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        exchange(a, b, 120);
        const std::string all = waitForDiag(dir, "player=Bia slot=1");
        CHECK(all.find("event=match_start players=2 hz=60") != std::string::npos);
        CHECK(all.find("src=server lobby=") != std::string::npos);
        CHECK(all.find("player=Ana slot=0 transport=enet hz=60 delay=") != std::string::npos);
        CHECK(all.find("player=Bia slot=1 transport=ws hz=60 delay=") != std::string::npos);
        CHECK(all.find("in_pkts=") != std::string::npos && all.find("in_gap_max_ms=") != std::string::npos && all.find("behind_max=") != std::string::npos);
        size_t files = 0;
        for (const auto &e : fs::directory_iterator(dir))
            files += e.path().filename().string().find("-match.log") != std::string::npos ? 1 : 0;
        CHECK(files == 1u);
    }
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // every line goes out immediately: a hung test shows where it stopped
    for (const Test &t : tests())
    {
        const int before = g_failures;
        try
        {
            t.run();
        }
        catch (const std::exception &e)   // an exception is a test failure, never an abort (which leaves the process stuck in the error report)
        {
            ++g_failures;
            std::printf("  EXCEPTION %s\n", e.what());
        }
        std::printf("%s %s\n", g_failures == before ? "ok     " : "FAILED ", t.name);
    }
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
