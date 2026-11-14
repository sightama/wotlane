// ABOUTME: Netcode server (no game code): lobbies over TCP or WebSocket and lockstep matches over ENet or WebSocket, relaying inputs,
// ABOUTME: picking the delay from ping, comparing state hashes and dropping players. Used by wotm_server and the tests.
#pragma once

#include "wotm_lockstep.hpp"
#include "wotm_net_proto.hpp"
#include "wotm_net_socket.hpp"
#include "wotm_net_wslink.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace wotm_net
{
    struct ServerConfig
    {
        uint16_t port = proto::kDefaultPort;   // TCP (lobby) and UDP (match) on the same number
        int lobbies = 16;                       // maximum lobbies open at the same time
        int maxPlayers = proto::kMaxPlayers;
        uint32_t forcedDelay = 0;               // 0: delay from ping
        int startWaitMs = 1000;                 // once everyone arrives, measures ping for this long before starting
        int joinTimeoutMs = 90000;              // anyone who doesn't reach the match in this time is dropped (the game takes ~15 s to load)
        int dropInputPercent = 0;               // test: discards this percentage of the input packets sent
        bool log = true;
        // public listing: with announceHost set, a thread announces this server to that master every announceEveryMs, with its lobby counts
        std::string announceHost;
        uint16_t announcePort = proto::kDefaultMasterPort;
        std::string name = "wotlane";           // the name shown in the master's list (letters, digits, - and _; up to 16)
        int announceEveryMs = 10000;
        // WebSocket listener for the browser version: /lobby carries the lobby lines, /match the match packets. 0 = off. It binds to
        // wsBind only: a web server in front of it (HTTPS) is the public entry
        uint16_t wsPort = 0;
        std::string wsBind = "127.0.0.1";
    };

    class Server
    {
    public:
        explicit Server(ServerConfig c);
        ~Server();
        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;

        bool start(std::string &err);
        // services the lobby and the match; waits up to `ms` for traffic
        void poll(int ms);

    private:
        using Clock = std::chrono::steady_clock;

        struct Conn
        {
            std::unique_ptr<LineLink> link;   // TCP or WebSocket
            bool hello = false;
            std::string build;
            int lobby = -1;
            std::string name;
            int monster = 0;
        };

        // a browser in a match (WebSocket /match)
        struct WsPeer
        {
            std::unique_ptr<WsLink> link;
            void *data = nullptr;   // the match's (lobby, slot), as ENet's peer->data
            Clock::time_point lastPing{};
        };

        // a match connection: ENet (native) or WebSocket (browser)
        struct PeerRef
        {
            _ENetPeer *enet = nullptr;
            WsPeer *ws = nullptr;
            explicit operator bool() const { return enet || ws; }
            bool operator==(const PeerRef &o) const { return enet == o.enet && ws == o.ws; }
        };

        struct Slot
        {
            std::string token, name;
            PeerRef peer;
            bool joined = false, gone = false;
            std::vector<uint32_t> need;   // per player, the first frame this client doesn't have yet
            uint32_t lastAckOwn = 0;
            Clock::time_point lastSend{};
            bool dirty = false;
        };

        struct Match
        {
            int lobby = 0;
            std::vector<Slot> slots;
            std::string config;            // config of the first to join: everyone must match it
            bool started = false, desynced = false;
            uint32_t delay = 0;
            InputTimeline inputs{0, 0};
            struct SlotHash { uint64_t hash = 0; std::vector<uint64_t> blocks; };   // blocks: the 256 block hashes, when sent
            std::map<uint32_t, std::vector<std::optional<SlotHash>>> hashes;
            Clock::time_point created{}, allJoined{};
            bool allJoinedSet = false;
        };

        struct Lobby
        {
            bool open = false;             // slot in use (created, and has someone in it or is in a match)
            std::string code, name, password;
            int host = -1;                 // owner's index in conns_
            int stage = 1, wins = 0;
            std::string mods;              // gameplay modifiers, canonical list (wotm_modifiers.hpp); "" = none
            std::vector<int> members;      // indices into conns_
            std::string build;
            std::unique_ptr<Match> match;
        };

        void log(const char *fmt, ...);
        void acceptAndReadLobby();
        void addConn(std::unique_ptr<LineLink> link);
        void handleLine(int c, const std::string &line);
        void createLobby(int c, const std::vector<std::string> &w);
        void joinLobby(int c, const std::vector<std::string> &w);
        void setLobby(int c, const std::vector<std::string> &w);
        void changeMonster(int c, const std::vector<std::string> &w);
        // joins lobby li (already validated): member, owner if first, JOINED, SETTINGS and MEMBERS
        void addMember(int c, int li, const std::string &name, int monster);
        // `except`: a member left out of the costume check (the one changing monster)
        bool monsterFits(const Lobby &l, int monster, std::string &err, int except = -1) const;
        void leaveLobby(int c);
        bool closeIfEmpty(int lobby);
        void sendMembers(int lobby);
        // SETTINGS <code> stage=<n> wins=<n> mods=<list>
        std::string settingsLine(const Lobby &l) const;
        void sendSettings(int lobby);
        void startMatch(int lobby);
        std::string newCode();
        int lobbyByCode(const std::string &code) const;
        int openLobbies() const;
        std::string lobbyLine(int i) const;

        void updateCounts();
        void announceLoop();

        void serviceEnet(int ms);
        // WebSocket: accepts, answers handshakes, and hands each connection to the lobby (/lobby) or the match (/match)
        void serviceWebSockets();
        void onReceive(PeerRef peer, const std::vector<uint8_t> &data);
        void onDisconnect(PeerRef peer);
        void onJoin(PeerRef peer, const proto::Join &j);
        void dropSlot(Match &m, int slot);
        void tickMatches();
        void sendInputs(Match &m, int slot);
        void checkHashes(Match &m);
        void send(PeerRef peer, uint8_t channel, const std::vector<uint8_t> &data, bool reliable);
        Match *matchOf(PeerRef peer, int &slot);
        static void *&dataOf(PeerRef peer);
        static uint32_t rttOf(PeerRef peer);

        ServerConfig cfg_;
        Listener listener_, wsListener_;
        std::vector<std::unique_ptr<Conn>> conns_;
        std::vector<std::unique_ptr<WsLink>> wsPending_;   // accepted, handshake not done yet
        std::vector<std::unique_ptr<WsPeer>> wsPeers_;     // browsers on /match
        std::vector<Lobby> lobbies_;
        _ENetHost *host_ = nullptr;
        bool enetUp_ = false;
        std::mt19937_64 rng_;
        // what the announcer sends the master (updated by poll): open lobbies, lobbies waiting for players, players in all lobbies
        std::atomic<int> open_{0}, waiting_{0}, players_{0};
        std::thread announcer_;
        std::mutex announceMu_;
        std::condition_variable announceCv_;
        bool stopAnnouncing_ = false;
    };
}
