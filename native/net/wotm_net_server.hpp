// ABOUTME: Servidor do netcode (sem codigo do jogo): lobbies por TCP e partidas em lockstep por ENet, com repasse das entradas,
// ABOUTME: atraso pelo ping, comparacao dos hashes de estado e queda de jogadores. Usado pelo wotm_server e pelos testes.
#pragma once

#include "wotm_lockstep.hpp"
#include "wotm_net_proto.hpp"
#include "wotm_net_socket.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace wotm_net
{
    struct ServerConfig
    {
        uint16_t port = proto::kDefaultPort;   // TCP (lobby) e UDP (partida) no mesmo numero
        int lobbies = 2;
        int maxPlayers = proto::kMaxPlayers;
        uint32_t forcedDelay = 0;               // 0: atraso pelo ping
        int startWaitMs = 1000;                 // depois que todos chegam, mede o ping por este tempo antes do inicio
        int joinTimeoutMs = 90000;              // quem nao chega a partida neste tempo cai (o jogo leva ~15 s para carregar)
        int dropInputPercent = 0;               // teste: descarta esta parte dos pacotes de entrada enviados
        bool log = true;
    };

    class Server
    {
    public:
        explicit Server(ServerConfig c);
        ~Server();
        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;

        bool start(std::string &err);
        // atende o lobby e a partida; espera ate `ms` por trafego
        void poll(int ms);

    private:
        using Clock = std::chrono::steady_clock;

        struct Conn
        {
            LineSocket socket;
            bool hello = false;
            std::string build;
            int lobby = -1;
            std::string name;
            int monster = 0;
        };

        struct Slot
        {
            std::string token, name;
            _ENetPeer *peer = nullptr;
            bool joined = false, gone = false;
            std::vector<uint32_t> need;   // primeiro quadro de cada jogador que este cliente ainda nao tem
            uint32_t lastAckOwn = 0;
            Clock::time_point lastSend{};
            bool dirty = false;
        };

        struct Match
        {
            int lobby = 0;
            std::vector<Slot> slots;
            std::string config;            // configuracao do primeiro que entrou: todos iguais
            bool started = false, desynced = false;
            uint32_t delay = 0;
            InputTimeline inputs{0, 0};
            std::map<uint32_t, std::vector<std::optional<uint64_t>>> hashes;
            Clock::time_point created{}, allJoined{};
            bool allJoinedSet = false;
        };

        struct Lobby
        {
            std::vector<int> members;      // indices em conns_
            std::string build;
            std::unique_ptr<Match> match;
        };

        void log(const char *fmt, ...);
        void acceptAndReadLobby();
        void handleLine(int c, const std::string &line);
        void leaveLobby(int c);
        void sendMembers(int lobby);
        void startMatch(int lobby, int stage);
        std::string lobbyLine(int i) const;

        void serviceEnet(int ms);
        void onReceive(_ENetPeer *peer, const std::vector<uint8_t> &data);
        void onDisconnect(_ENetPeer *peer);
        void onJoin(_ENetPeer *peer, const proto::Join &j);
        void dropSlot(Match &m, int slot);
        void tickMatches();
        void sendInputs(Match &m, int slot);
        void checkHashes(Match &m);
        void send(_ENetPeer *peer, uint8_t channel, const std::vector<uint8_t> &data, bool reliable);
        Match *matchOf(_ENetPeer *peer, int &slot);

        ServerConfig cfg_;
        Listener listener_;
        std::vector<std::unique_ptr<Conn>> conns_;
        std::vector<Lobby> lobbies_;
        _ENetHost *host_ = nullptr;
        bool enetUp_ = false;
        std::mt19937_64 rng_;
    };
}
