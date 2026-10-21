// ABOUTME: Cliente da partida em rede (ENet, numa thread propria): entra com a ficha do lobby, manda as entradas locais, recebe as
// ABOUTME: dos outros e entrega ao jogo a linha do tempo do lockstep. Sem codigo do jogo; o wotm.exe o usa por wotm_sync.inc.
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
        int dropInputPercent = 0;   // teste: descarta esta parte dos pacotes de entrada enviados
    };

    class MatchClient
    {
    public:
        enum class Status
        {
            Idle,
            Connecting,   // procurando o servidor
            Waiting,      // dentro da partida, esperando os outros chegarem
            Running,
            Rejected,     // o servidor recusou (message() diz por que)
            Desynced,     // os estados divergiram; a partida parou
            Lost          // conexao caiu
        };

        MatchClient() = default;
        ~MatchClient();
        MatchClient(const MatchClient &) = delete;
        MatchClient &operator=(const MatchClient &) = delete;

        bool start(const ClientConfig &c, std::string &err);
        void stop();

        Status status() const;
        std::string message() const;
        // true quando a partida comecou (todos chegaram); false se o tempo acabou ou se foi recusado
        bool waitStarted(int ms);
        int localPlayer() const;
        int players() const;
        uint32_t delay() const;
        std::vector<std::string> names() const;
        uint32_t rttMs() const { return rtt_.load(); }

        // entrada local que vale no quadro `frame` (amostrada `delay` quadros antes)
        void pushLocal(uint32_t frame, const PadState &s);
        // espera ate `ms` as entradas de todos para o quadro; false se faltam, ou se a partida parou
        bool waitFrame(uint32_t frame, int ms);
        PadState input(int player, uint32_t frame) const;
        // jogadores sem entrada para o quadro (para a mensagem "esperando ...")
        std::vector<int> missing(uint32_t frame) const;
        void sendHash(uint32_t frame, uint64_t hash);
        // espera o servidor confirmar todas as entradas locais ja enviadas
        bool waitAcked(int ms);
        uint32_t dropFrame(int player) const;   // 0xFFFFFFFF se nao caiu
        uint32_t desyncFrame() const;           // 0xFFFFFFFF se nao houve

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

        // protegidos por mu_
        Status status_ = Status::Idle;
        std::string message_;
        int local_ = -1, players_ = 0;
        uint32_t delay_ = 0;
        std::vector<std::string> names_;
        InputTimeline inputs_{0, 0};
        std::vector<std::pair<int, uint32_t>> drops_;   // quedas (inclusive as que chegam antes do inicio)
        uint32_t serverAck_ = 0;                        // primeiro quadro local que o servidor ainda nao tem
        std::vector<uint32_t> sentAcks_;
        std::vector<proto::Hash> hashQueue_;
        bool dirty_ = false;
        Clock::time_point lastSend_{};
        uint32_t desyncFrame_ = 0xFFFFFFFFu;
    };
}
