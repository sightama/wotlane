// ABOUTME: Network match client (ENet, on its own thread): joins with the lobby token, sends local inputs, receives the others'
// ABOUTME: and hands the game the lockstep timeline. No game code; wotm.exe uses it through wotm_sync.inc.
#pragma once

#include "wotm_lockstep.hpp"
#include "wotm_net_proto.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace wotm_net
{
    struct ClientConfig
    {
        std::string host;
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
        uint32_t delay() const;
        std::vector<std::string> names() const;
        uint32_t rttMs() const { return rtt_.load(); }

        // local input that applies on frame `frame` (sampled `delay` frames earlier)
        void pushLocal(uint32_t frame, const PadState &s);
        // waits up to `ms` for everyone's inputs for the frame; false if some are missing, or if the match stopped
        bool waitFrame(uint32_t frame, int ms);
        PadState input(int player, uint32_t frame) const;
        // players with no input for the frame (for the "Waiting for ..." message)
        std::vector<int> missing(uint32_t frame) const;
        void sendHash(uint32_t frame, uint64_t hash);
        // waits for the server to acknowledge every local input already sent
        bool waitAcked(int ms);
        uint32_t dropFrame(int player) const;   // 0xFFFFFFFF if not dropped
        uint32_t desyncFrame() const;           // 0xFFFFFFFF if none

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
        _ENetHost *host_ = nullptr;
        _ENetPeer *peer_ = nullptr;
        bool enetUp_ = false;
        ClientConfig cfg_;
        std::mt19937 rng_{12345u};

        // guarded by mu_
        Status status_ = Status::Idle;
        std::string message_;
        int local_ = -1, players_ = 0;
        uint32_t delay_ = 0;
        std::vector<std::string> names_;
        InputTimeline inputs_{0, 0};
        std::vector<std::pair<int, uint32_t>> drops_;   // drops (including ones that arrive before the start)
        uint32_t serverAck_ = 0;                        // first local frame the server doesn't have yet
        std::vector<uint32_t> sentAcks_;
        std::vector<proto::Hash> hashQueue_;
        bool dirty_ = false;
        Clock::time_point lastSend_{};
        uint32_t desyncFrame_ = 0xFFFFFFFFu;
    };
}
