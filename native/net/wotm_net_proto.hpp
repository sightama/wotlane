// ABOUTME: Netcode protocol: binary match messages (ENet) and lobby text lines (TCP), with encoding and parsing.
// ABOUTME: No dependency on the game or on sockets; used by the client (wotm.exe), the server (wotm_server) and the tests.
#pragma once

#include "wotm_lockstep.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace wotm_net::proto
{
    constexpr uint16_t kProtocol = 5;   // 3: pad states carry mouse motion and camera settings; 4: Hash carries the 256 block hashes;
                                        // 5: the lobby carries saved map ids (stage) and skin ids (<monster>:<skin>)
    // The additive wire level (docs/ONLINE.md "Wire versions"): it moves when a message or an optional field is added, and kProtocol
    // only when a format changes. A client says its level in HELLO (wire=<n>; none: 0) and the server sends it only what that level
    // knows, so older retained builds keep playing on the newest server. 1: WELCOME carries the server's level (wire=<n>)
    constexpr int kWire = 1;
    constexpr int kMaxPlayers = 8;   // 4 in the game's player slots and 4 in AI slots driven by the network (wotm_multi.inc)
    constexpr uint16_t kDefaultPort = 7777;
    constexpr uint16_t kDefaultMasterPort = 7778;   // the master server (list of public servers, wotm_net_master.hpp)
    constexpr const char *kDefaultServer = "35.185.58.174:7777";   // the always-on shared game server (tools/net_lobby.py DEFAULT_SERVER,
                                                                   // tools/launcher LobbyClient.DefaultServer)
    constexpr size_t kMaxRun = 64;   // input frames per message (lockstep never leaves more than the delay pending)

    // ENet channels: reliable control (join, start, drop, hash, desync) and unacknowledged inputs (each packet resends whatever the
    // other side hasn't acknowledged yet, so losing one costs nothing)
    enum Channel : uint8_t
    {
        kControl = 0,
        kInputs = 1,
        kChannels = 2
    };

    enum class Type : uint8_t
    {
        None = 0,
        Join = 1,     // client -> server: lobby token and build identity
        Welcome,      // server -> client: player index and player count
        Reject,       // server -> client: reason, then the connection closes
        Start,        // server -> client: everyone arrived; input delay and names
        Input,        // client -> server: this player's inputs and what it has received from each player
        Inputs,       // server -> client: the others' inputs and what it has received from this client
        Hash,         // client -> server: state hash on a frame
        Desync,       // server -> client: the hashes for a frame differ; the match stops
        Drop,         // server -> client: a player left; neutral from a given frame
        Away,         // client -> server: this player's game went to the background or came back (a hidden browser tab);
                      // server -> client: the same for another player. Older peers ignore it
        Pace,         // server -> client: the input delay this player should move to (re-picked from the round trips during the
                      // match; the client steps toward it, wotm_lockstep.hpp LocalSchedule). Older peers ignore it and keep the
                      // Start delay, which stays in sync either way
        Rtt,          // server -> client: this player's round trip as the server measures it (a browser can't time the WebSocket
                      // pings itself), sent with every pace check. Older peers ignore it
    };

    struct Join
    {
        uint16_t protocol = kProtocol;
        std::string token, buildId, configHash, name;
    };
    struct Welcome
    {
        uint8_t player = 0, players = 0;
    };
    struct Reject
    {
        std::string reason;
    };
    struct Start
    {
        uint32_t delay = 0;
        std::vector<std::string> names;
    };
    struct InputRun
    {
        uint8_t player = 0;
        uint32_t first = 0;
        std::vector<PadState> states;
    };
    struct Input
    {
        std::vector<uint32_t> acks;   // per player: first frame this client doesn't have yet
        InputRun own;
    };
    struct Inputs
    {
        uint32_t ackOwn = 0;          // first frame of this client's own input the server doesn't have yet
        std::vector<InputRun> runs;
    };
    struct Hash
    {
        uint32_t frame = 0;
        uint64_t hash = 0;
        std::vector<uint64_t> blocks;   // the 256 hashes of the 128 KB blocks the total came from (empty: only the total), so a
                                        // desync names the memory block, not just the PC (wotm_sync.inc netHash, server checkHashes)
    };
    struct Desync
    {
        uint32_t frame = 0;
        std::string detail;
    };
    struct Drop
    {
        uint8_t player = 0;
        uint32_t frame = 0;
    };
    struct Away
    {
        uint8_t player = 0;   // the server fills it in; a client sends its own slot as 0
        bool away = false;
    };
    struct Pace
    {
        uint32_t delay = 0;
    };
    struct Rtt
    {
        uint32_t ms = 0;
    };

    class Writer
    {
    public:
        explicit Writer(Type t) { u8(static_cast<uint8_t>(t)); }
        void u8(uint8_t v) { b_.push_back(v); }
        void u16(uint16_t v) { u8(static_cast<uint8_t>(v)), u8(static_cast<uint8_t>(v >> 8)); }
        void u32(uint32_t v) { u16(static_cast<uint16_t>(v)), u16(static_cast<uint16_t>(v >> 16)); }
        void u64(uint64_t v) { u32(static_cast<uint32_t>(v)), u32(static_cast<uint32_t>(v >> 32)); }
        void str(const std::string &s)
        {
            const size_t n = std::min<size_t>(s.size(), 255u);
            u8(static_cast<uint8_t>(n));
            b_.insert(b_.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
        }
        void pad(const PadState &s)
        {
            u16(s.buttons), u8(s.rx), u8(s.ry), u8(s.lx), u8(s.ly);
            u16(static_cast<uint16_t>(s.mouseX)), u16(static_cast<uint16_t>(s.mouseY)), u16(s.cam);
        }
        // a state equal to the previous one costs 1 byte (0); a new one, 1 + 12 bytes
        void run(const InputRun &r)
        {
            const size_t n = std::min(r.states.size(), kMaxRun);
            u8(r.player), u32(r.first), u8(static_cast<uint8_t>(n));
            for (size_t i = 0; i < n; ++i)
            {
                const bool same = i > 0 && r.states[i] == r.states[i - 1];
                u8(same ? 0u : 1u);
                if (!same)
                    pad(r.states[i]);
            }
        }
        std::vector<uint8_t> take() { return std::move(b_); }

    private:
        std::vector<uint8_t> b_;
    };

    class Reader
    {
    public:
        Reader(const std::vector<uint8_t> &b, Type expected) : p_(b.data()), e_(b.data() + b.size())
        {
            ok_ = u8() == static_cast<uint8_t>(expected);
        }
        bool ok() const { return ok_; }
        bool done() const { return ok_ && p_ == e_; }
        uint8_t u8()
        {
            if (p_ >= e_)
            {
                ok_ = false;
                return 0;
            }
            return *p_++;
        }
        uint16_t u16()
        {
            const uint16_t lo = u8();
            return static_cast<uint16_t>(lo | (u8() << 8));
        }
        uint32_t u32()
        {
            const uint32_t lo = u16();
            return lo | (static_cast<uint32_t>(u16()) << 16);
        }
        uint64_t u64()
        {
            const uint64_t lo = u32();
            return lo | (static_cast<uint64_t>(u32()) << 32);
        }
        std::string str()
        {
            const size_t n = u8();
            if (static_cast<size_t>(e_ - p_) < n)
            {
                ok_ = false;
                return {};
            }
            std::string s(reinterpret_cast<const char *>(p_), n);
            p_ += n;
            return s;
        }
        PadState pad()
        {
            PadState s;
            s.buttons = u16();
            s.rx = u8(), s.ry = u8(), s.lx = u8(), s.ly = u8();
            s.mouseX = static_cast<int16_t>(u16()), s.mouseY = static_cast<int16_t>(u16());
            s.cam = u16();
            return s;
        }
        InputRun run()
        {
            InputRun r;
            r.player = u8();
            r.first = u32();
            const size_t n = u8();
            for (size_t i = 0; i < n && ok_; ++i)
            {
                const uint8_t kind = u8();
                if (kind == 0u && !r.states.empty())
                    r.states.push_back(r.states.back());
                else if (kind == 1u)
                    r.states.push_back(pad());
                else
                    ok_ = false;
            }
            return r;
        }

    private:
        const uint8_t *p_, *e_;
        bool ok_ = true;
    };

    inline Type typeOf(const std::vector<uint8_t> &b) { return b.empty() ? Type::None : static_cast<Type>(b[0]); }

    inline std::vector<uint8_t> encode(const Join &m)
    {
        Writer w(Type::Join);
        w.u16(m.protocol), w.str(m.token), w.str(m.buildId), w.str(m.configHash), w.str(m.name);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Join &m)
    {
        Reader r(b, Type::Join);
        m.protocol = r.u16(), m.token = r.str(), m.buildId = r.str(), m.configHash = r.str(), m.name = r.str();
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Welcome &m)
    {
        Writer w(Type::Welcome);
        w.u8(m.player), w.u8(m.players);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Welcome &m)
    {
        Reader r(b, Type::Welcome);
        m.player = r.u8(), m.players = r.u8();
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Reject &m)
    {
        Writer w(Type::Reject);
        w.str(m.reason);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Reject &m)
    {
        Reader r(b, Type::Reject);
        m.reason = r.str();
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Start &m)
    {
        Writer w(Type::Start);
        w.u32(m.delay), w.u8(static_cast<uint8_t>(m.names.size()));
        for (const std::string &n : m.names)
            w.str(n);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Start &m)
    {
        Reader r(b, Type::Start);
        m.delay = r.u32();
        const size_t n = r.u8();
        m.names.clear();
        for (size_t i = 0; i < n && r.ok(); ++i)
            m.names.push_back(r.str());
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Input &m)
    {
        Writer w(Type::Input);
        w.u8(static_cast<uint8_t>(m.acks.size()));
        for (uint32_t a : m.acks)
            w.u32(a);
        w.run(m.own);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Input &m)
    {
        Reader r(b, Type::Input);
        const size_t n = r.u8();
        m.acks.clear();
        for (size_t i = 0; i < n && r.ok(); ++i)
            m.acks.push_back(r.u32());
        m.own = r.run();
        return r.done() && n <= static_cast<size_t>(kMaxPlayers);
    }

    inline std::vector<uint8_t> encode(const Inputs &m)
    {
        Writer w(Type::Inputs);
        w.u32(m.ackOwn), w.u8(static_cast<uint8_t>(m.runs.size()));
        for (const InputRun &run : m.runs)
            w.run(run);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Inputs &m)
    {
        Reader r(b, Type::Inputs);
        m.ackOwn = r.u32();
        const size_t n = r.u8();
        m.runs.clear();
        for (size_t i = 0; i < n && r.ok(); ++i)
            m.runs.push_back(r.run());
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Hash &m)
    {
        Writer w(Type::Hash);
        w.u32(m.frame), w.u64(m.hash), w.u16(static_cast<uint16_t>(m.blocks.size()));
        for (uint64_t h : m.blocks)
            w.u64(h);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Hash &m)
    {
        Reader r(b, Type::Hash);
        m.frame = r.u32(), m.hash = r.u64();
        const size_t n = r.u16();
        m.blocks.clear();
        for (size_t i = 0; i < n && r.ok(); ++i)
            m.blocks.push_back(r.u64());
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Desync &m)
    {
        Writer w(Type::Desync);
        w.u32(m.frame), w.str(m.detail);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Desync &m)
    {
        Reader r(b, Type::Desync);
        m.frame = r.u32(), m.detail = r.str();
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Drop &m)
    {
        Writer w(Type::Drop);
        w.u8(m.player), w.u32(m.frame);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Drop &m)
    {
        Reader r(b, Type::Drop);
        m.player = r.u8(), m.frame = r.u32();
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Away &m)
    {
        Writer w(Type::Away);
        w.u8(m.player), w.u8(m.away ? 1u : 0u);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Away &m)
    {
        Reader r(b, Type::Away);
        m.player = r.u8(), m.away = r.u8() != 0;
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Pace &m)
    {
        Writer w(Type::Pace);
        w.u32(m.delay);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Pace &m)
    {
        Reader r(b, Type::Pace);
        m.delay = r.u32();
        return r.done();
    }

    inline std::vector<uint8_t> encode(const Rtt &m)
    {
        Writer w(Type::Rtt);
        w.u32(m.ms);
        return w.take();
    }
    inline bool decode(const std::vector<uint8_t> &b, Rtt &m)
    {
        Reader r(b, Type::Rtt);
        m.ms = r.u32();
        return r.done();
    }

    // fields per game frame (WOTM_FIXEDSTEP) in a match of this many players: 3 or more, or a member whose device can't compute 60 game
    // frames a second (a phone, HELLO hz=30), switch the game to its own 30 Hz mode. A frame is always one step of game time, so a
    // device that falls short of the rate slows the game itself for everyone; at 30 Hz it does half the frames for the same time
    inline int matchStep(int players, bool halfRateMember) { return players >= 3 || halfRateMember ? 2 : 1; }
    inline double frameMs(int step) { return step * 1000.0 / 60.0; }
    // The address of a lobby client for the owner's removals: the TCP peer's, except behind the proxy on this machine (a loopback peer),
    // where it is what the proxy says in its headers (may be empty). Kept in memory only, and "" means unknown: the ban falls back to the name.
    inline std::string clientAddress(const std::string &peerIp, const std::string &forwarded)
    {
        return peerIp == "127.0.0.1" ? forwarded : peerIp;
    }
    // a phone's or tablet's browser, from its lobby WebSocket's User-Agent: the server's own check, so a phone plays at 30 Hz even
    // when its page did not say hz=30 (an old page in the browser's memory, a pad as its input)
    inline bool halfRateUserAgent(std::string_view ua)
    {
        return ua.find("Android") != std::string_view::npos || ua.find("Mobile") != std::string_view::npos ||
               ua.find("iPhone") != std::string_view::npos || ua.find("iPad") != std::string_view::npos;
    }

    // Input delay (frames) of `player`, from each player's round trip to the server (ms, the level of the recent ones: RttWindow):
    // the input goes to the server and from there to every other player, so its path is half its own round trip plus half the
    // slowest other player's, plus kDelayMarginMs for sending and handling. Clamped to 1..15.
    constexpr double kDelayMarginMs = 3.0;
    // the path in frames (0 when nobody else is there to reach)
    inline double pathFrames(const std::vector<uint32_t> &rttMs, size_t player, double frameMs)
    {
        uint32_t other = 0;
        bool alone = true;
        for (size_t q = 0; q < rttMs.size(); ++q)
            if (q != player)
                other = std::max(other, rttMs[q]), alone = false;
        if (alone || player >= rttMs.size())
            return 0.0;
        return ((rttMs[player] + other) / 2.0 + kDelayMarginMs) / frameMs;
    }
    inline uint32_t framesFor(double frames) { return static_cast<uint32_t>(std::clamp(std::ceil(frames - 1e-9), 1.0, 15.0)); }
    inline uint32_t playerDelay(const std::vector<uint32_t> &rttMs, size_t player, double frameMs)
    {
        return framesFor(pathFrames(rttMs, player, frameMs));
    }

    // The delay to advise a player whose delay is `current` during the match: it rises as soon as the path for the rise levels needs
    // another frame, and falls once the path for the fall levels (RttWindow) leaves kDelayFallMarginFrames to spare at the lower delay,
    // so neither a ping sitting on a frame boundary nor spikes that keep coming move it back and forth
    constexpr double kDelayFallMarginFrames = 0.2;
    inline uint32_t adviseDelay(uint32_t current, const std::vector<uint32_t> &riseMs, const std::vector<uint32_t> &fallMs, size_t player,
                                double frameMs)
    {
        const uint32_t up = framesFor(pathFrames(riseMs, player, frameMs)),
                       down = framesFor(pathFrames(fallMs, player, frameMs) + kDelayFallMarginFrames);
        return up >= current ? up : std::min(current, down);
    }
    inline uint32_t adviseDelay(uint32_t current, const std::vector<uint32_t> &rttMs, size_t player, double frameMs)
    {
        return adviseDelay(current, rttMs, rttMs, player, frameMs);
    }

    // the delay everyone starts with (the frames before it are neutral for every player): the largest player's, and at least 2,
    // since a browser's ping is 0 until its first pong (the advice brings a LAN player down to 1 during the match)
    inline uint32_t startDelay(const std::vector<uint32_t> &rttMs, double frameMs)
    {
        uint32_t d = 2;
        for (size_t p = 0; p < rttMs.size(); ++p)
            d = std::max(d, playerDelay(rttMs, p, frameMs));
        return d;
    }

    // A player's round trip as the delay sees it, from their last kSamples round trips (8 s at the server's 500 ms). The delay rises
    // for the level they stay at or under 4/5 of the time (riseLevel: 3 high samples of 16 are left out, so a short spike costs the
    // others a short wait instead of a frame of delay for everyone, and a lasting rise counts once it holds for 4 samples, 2 s), and
    // falls only for the level all but one stay at or under (fallLevel), so spikes that keep coming hold the delay up instead of
    // moving it up and down. While the window fills, fewer samples are left out (the first ones count at their high end)
    class RttWindow
    {
    public:
        static constexpr size_t kSamples = 16;
        void add(uint32_t ms)
        {
            samples_[at_] = ms;
            at_ = (at_ + 1) % kSamples;
            n_ = std::min(n_ + 1, kSamples);
        }
        uint32_t riseLevel() const { return highestBut(n_ * 3 / kSamples); }
        uint32_t fallLevel() const { return highestBut(n_ / kSamples); }

    private:
        // the highest sample once the `skip` highest are left out
        uint32_t highestBut(size_t skip) const
        {
            if (n_ == 0)
                return 0;
            uint32_t sorted[kSamples];
            std::copy(samples_, samples_ + n_, sorted);
            std::sort(sorted, sorted + n_);
            return sorted[n_ - 1 - std::min(skip, n_ - 1)];
        }

        uint32_t samples_[kSamples] = {};
        size_t at_ = 0, n_ = 0;
    };

    // ---- lobby (TCP, one text line per message) ----
    // Whoever creates the lobby is the owner: only the owner changes the stage, wins and modifiers and starts the match; if the owner leaves, the
    // next member becomes owner. The lobby has a 4-letter code (to join without the list) and may have a password. Members stay in it
    // during the match and, when the match ends (everyone has left it: QUIT closes the game), it goes back to waiting with the same
    // members. An empty lobby closes.
    // client: HELLO <protocol> <build> [hz=30] [rejoin=1] [wire=<n>] | LIST | CREATE <lobby> <name> <monster> [password] | JOIN <code> <name> <monster> [password] |
    //         SET [stage=<stage>] [wins=<wins>] [mods=<list>] | MONSTER <monster> (any member, while the lobby waits) | START | LEAVE |
    //         CHAT <text> (printable text up to kMaxChatBytes, kChatBurst lines per kChatWindowMs, only while the lobby waits) |
    //         KICK <name> | OWNER <name> (owner only, while the lobby waits: remove a member, who can't join this lobby again for a while;
    //         hand the lobby to a member) | HEALTH <own fps> <work ms> <hz> [sw] | HEALTH sw (how this device ran its last match, and
    //         "sw" when its browser draws WebGL in software, shown to the lobby; an older server refuses the "sw" forms with an ERROR) |
    //         DIAG <key=value>... (a page's diagnostics report, at any time after HELLO: never answered, never relayed; see wotm_net_diag.hpp;
    //         an older server answers "ERROR unknown command: DIAG", which a client that sends it ignores)
    // server: WELCOME <open lobbies> [wire=<n>] (to a client at level 1 or above) | LOBBY <code> <waiting|playing> <n>/<max> <open|locked> <lobby> <owner> <names,...|-> |
    //         END | CREATED <code> | JOINED <code> | LEFT <code> | SETTINGS <code> stage=<stage> wins=<wins> mods=<list> | CHAT <code> <name> <text> |
    //         MEMBERS <code> <owner> <name:monster,...> |
    //         KICKED <code> <seconds> (this member was removed; the seconds are the ban) |
    //         HEALTH <code> <name:fps:work_ms:hz[:sw],...> (every second with PINGS, for the members that sent one; "-" for numbers not
    //         measured yet) |
    //         PINGS <code> <name:ms|name:->,... (every second while the lobby waits; - for a member the server can't measure, a TCP one) | MATCH <key=value>... | ERROR <text>
    // <monster> is "<number>" or "<number>:<skin id>" (Pick); <stage> is a stage number or a saved map's id (validStage)
    constexpr int kMaxWins = 99;
    constexpr size_t kMaxChatBytes = 200;   // a lobby chat line is cut to this many bytes
    constexpr int kChatBurst = 5, kChatWindowMs = 10000;   // a member may send this many lines per window
    constexpr int kDiagBurst = 10, kDiagWindowMs = 10000;  // a connection may send this many DIAG lines per window (a page sends 1 every 5 s)
    constexpr const char *kCodeLetters = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   // no I, O, 0 or 1 (easily confused when reading the code aloud)
    inline std::vector<std::string> words(const std::string &line)
    {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
                ++i;
            size_t j = i;
            while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j])))
                ++j;
            if (j > i)
                out.push_back(line.substr(i, j - i));
            i = j;
        }
        return out;
    }

    inline std::map<std::string, std::string> keyValues(const std::vector<std::string> &w)
    {
        std::map<std::string, std::string> kv;
        for (const std::string &s : w)
            if (const size_t eq = s.find('='); eq != std::string::npos && eq > 0)
                kv[s.substr(0, eq)] = s.substr(eq + 1);
        return kv;
    }

    // a HELLO's wire level (its key=values): 0 when it says none or something that is not a level (a build from before levels)
    inline int wireOf(const std::map<std::string, std::string> &kv)
    {
        const auto it = kv.find("wire");
        if (it == kv.end() || it->second.empty() || it->second.size() > 4)
            return 0;
        if (it->second.find_first_not_of("0123456789") != std::string::npos)
            return 0;
        return std::stoi(it->second);
    }

    // player name: letters, digits, '_' and '-', up to 16
    inline std::string cleanName(const std::string &s)
    {
        std::string out;
        for (char c : s)
            if ((std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') && out.size() < 16u)
                out += c;
        return out;
    }

    // well-formed UTF-8 (a browser closes a WebSocket that carries a text frame that isn't)
    inline bool validUtf8(const std::string &s)
    {
        size_t i = 0;
        while (i < s.size())
        {
            const unsigned char b = static_cast<unsigned char>(s[i]);
            const size_t more = b < 0x80 ? 0 : b >= 0xC2 && b <= 0xDF ? 1 : b >= 0xE0 && b <= 0xEF ? 2 : b >= 0xF0 && b <= 0xF4 ? 3 : SIZE_MAX;
            if (more == SIZE_MAX || i + more >= s.size())
                return false;
            for (size_t k = 1; k <= more; ++k)
                if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80)
                    return false;
            if (b == 0xE0 && static_cast<unsigned char>(s[i + 1]) < 0xA0 || b == 0xED && static_cast<unsigned char>(s[i + 1]) > 0x9F ||
                b == 0xF0 && static_cast<unsigned char>(s[i + 1]) < 0x90 || b == 0xF4 && static_cast<unsigned char>(s[i + 1]) > 0x8F)
                return false;
            i += more + 1;
        }
        return true;
    }

    // A lobby member's monster: "<monster>" or "<monster>:<skin id>" (a mod skin, mods/<mod>/skins/<id>/: a-z, 0-9 and _, up to 32,
    // not starting with a digit, as WOTM_MATCH reads them). In the MATCH line it is what WOTM_MATCH reads
    struct Pick
    {
        int monster = 0;
        std::string skin;
        std::string text() const { return std::to_string(monster) + (skin.empty() ? "" : ":" + skin); }
    };
    inline bool parsePick(const std::string &s, Pick &out)
    {
        const size_t colon = s.find(':');
        const std::string num = s.substr(0, colon);
        if (num.empty() || num.size() > 3 || num.find_first_not_of("0123456789") != std::string::npos)
            return false;
        out.monster = std::atoi(num.c_str());
        out.skin = colon == std::string::npos ? "" : s.substr(colon + 1);
        // a skin id that starts with a digit would reach WOTM_MATCH as a disc costume number (past the lobby's costume check)
        if (colon != std::string::npos &&
            (out.skin.empty() || out.skin.size() > 32u || (out.skin[0] >= '0' && out.skin[0] <= '9') ||
             out.skin.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos))
            return false;
        return true;
    }

    // A lobby stage: a stock stage number, or a saved map's id (mods/<mod>/stages/<id>/: a letter, then a-z, 0-9 and _, up to 16)
    inline bool validStage(const std::string &s)
    {
        if (!s.empty() && s.size() <= 3u && s.find_first_not_of("0123456789") == std::string::npos)
            return true;
        return !s.empty() && s.size() <= 16u && s[0] >= 'a' && s[0] <= 'z' &&
               s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
    }
}
