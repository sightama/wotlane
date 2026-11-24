// ABOUTME: Network match client (on its own thread, over ENet or WebSocket): joins with the lobby token, sends local inputs, receives
// ABOUTME: the others' and hands the game the lockstep timeline. No game code; wotm.exe uses it through wotm_sync.inc.
#pragma once

#include "wotm_lockstep.hpp"
#include "wotm_net_proto.hpp"
#include "wotm_net_transport.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace wotm_net
{
    struct ClientConfig
    {
        std::string host;   // a host name or address (ENet, with port), or a ws:// or wss:// URL (WebSocket: the browser version)
        uint16_t port = proto::kDefaultPort;
        std::string token, buildId, configHash, name;
        int dropInputPercent = 0;   // test: discards this percentage of the input packets sent
    };

    class MatchClient
    {
    public:
        enum class Status
        {
            Idle,
            Connecting,   // looking for the server
            Waiting,      // in the match, waiting for the others to arrive
            Running,
            Rejected,     // the server rejected us (message() says why)
            Desynced,     // the states diverged; the match stopped
            Lost          // connection dropped
        };

        MatchClient() = default;
        ~MatchClient();
        MatchClient(const MatchClient &) = delete;
        MatchClient &operator=(const MatchClient &) = delete;

        bool start(const ClientConfig &c, std::string &err);
        void stop();

        Status status() const;
        std::string message() const;
        // true once the match has started (everyone arrived); false on timeout or if rejected
        bool waitStarted(int ms);
        int localPlayer() const;
        int players() const;
        uint32_t delay() const;   // this player's current input delay (it follows the server's Pace advice one frame at a time)
        std::vector<std::string> names() const;
        uint32_t rttMs() const { return rtt_.load(); }
        // frame advantage reported by the server (0.0 when synced or not ahead)
        double aheadFrames() const { return aheadHundredths_.load(std::memory_order_relaxed) / 100.0; }

        // local input that applies on frame `frame` (sampled `delay` frames earlier)
        void pushLocal(uint32_t frame, const PadState &s);
        // the local sample taken at the start of a frame: it fills the frame(s) the delay schedule gives it (LocalSchedule). Returns
        // the frame that carries this sample (kNoFrame: not in a running match, or the schedule skipped it)
        static constexpr uint32_t kNoFrame = 0xFFFFFFFFu;
        uint32_t pushSample(const PadState &s);
        // waits up to `ms` for everyone's inputs for the frame; false if some are missing, or if the match stopped
        bool waitFrame(uint32_t frame, int ms);
        PadState input(int player, uint32_t frame) const;
        // players with no input for the frame (for the "Waiting for ..." message)
        std::vector<int> missing(uint32_t frame) const;
        // state hash of the frame, with the 256 block hashes it came from (optional: the server then names the differing blocks)
        void sendHash(uint32_t frame, uint64_t hash, std::vector<uint64_t> blocks = {});
        // waits for the server to acknowledge every local input already sent
        bool waitAcked(int ms);
        uint32_t dropFrame(int player) const;   // 0xFFFFFFFF if not dropped
        uint32_t desyncFrame() const;           // 0xFFFFFFFF if none
        // this player's game went to the background (a hidden browser tab) or came back: the others' "Waiting for" message says so
        void setAway(bool away);
        bool away(int player) const;
        // "Waiting for <names>" for the players with no input for the frame, noting the ones whose game is in the background
        std::string waitingText(uint32_t frame) const;

    private:
        using Clock = std::chrono::steady_clock;
        void run();
        void handle(const std::vector<uint8_t> &data);
        void sendInput(Clock::time_point now);
        void setStatus(Status s, const std::string &msg);

        mutable std::mutex mu_;
        std::condition_variable cv_;
        std::thread thread_;
        std::atomic<bool> stop_{false};
        std::atomic<uint32_t> rtt_{0};
        // the round trip the server measures (proto::Rtt): once it has sent one, the ping reported is the server's, since a browser's
        // transport can't time the pings itself
        std::atomic<uint32_t> serverRtt_{0};
        std::atomic<bool> haveServerRtt_{false};
        std::atomic<uint16_t> aheadHundredths_{0};
        std::unique_ptr<Transport> net_;
        bool connected_ = false;
        ClientConfig cfg_;
        std::mt19937 rng_{12345u};

        // guarded by mu_
        Status status_ = Status::Idle;
        std::string message_;
        int local_ = -1, players_ = 0;
        LocalSchedule schedule_;
        std::vector<std::string> names_;
        InputTimeline inputs_{0, 0};
        std::vector<std::pair<int, uint32_t>> drops_;   // drops (including ones that arrive before the start)
        uint32_t serverAck_ = 0;                        // first local frame the server doesn't have yet
        std::vector<uint32_t> sentAcks_;
        std::vector<proto::Hash> hashQueue_;
        std::vector<proto::Away> awayQueue_;
        std::vector<bool> away_;                        // per player, from the server's Away messages
        bool dirty_ = false;
        Clock::time_point lastSend_{};
        uint32_t desyncFrame_ = 0xFFFFFFFFu;
    };
}
