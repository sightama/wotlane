// ABOUTME: Netcode server (no game code): lobbies over TCP or WebSocket and lockstep matches over ENet or WebSocket, relaying inputs,
// ABOUTME: picking the delay from ping, comparing state hashes and dropping players. Used by wotm_server and the tests.
#pragma once

#include "wotm_lockstep.hpp"
#include "wotm_net_diag.hpp"
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
        uint32_t forcedDelay = 0;               // 0: delay from ping (and re-picked during the match); otherwise fixed
        int paceMs = 500;                       // during a match, each player's round trip is sampled and its delay re-picked this often
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
        // diagnostics: the pages' DIAG reports and the server's own per-player view are journaled and, with diagDir set, also appended to
        // one file per match there (wotm_net_diag.hpp); the files are rotated by age and by total size
        std::string diagDir;
        int diagKeepDays = 7;
        uint64_t diagMaxTotalBytes = 64ull << 20;
        uint64_t diagMaxFileBytes = 2ull << 20;
        int diagEveryMs = 5000;                 // during a match, the server's view of each player is written this often (0: never)
        int kickBanMs = 60000;                  // a member the owner removed can't join that lobby again for this long
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
        // tests: added to every round trip measured (to move the delay during a match)
        void setTestRtt(uint32_t ms) { testRttMs_ = ms; }

    private:
        using Clock = std::chrono::steady_clock;

        struct Conn
        {
            std::unique_ptr<LineLink> link;   // TCP or WebSocket
            bool hello = false;
            std::string build;
            bool halfRate = false;   // HELLO hz=30: the device can't compute 60 game frames a second (a phone); its matches run at 30 Hz
            int lobby = -1;
            std::string name;
            proto::Pick pick;   // monster and skin for the next match
            Clock::time_point chatWindow{};   // start of the current chat window and the lines sent in it
            int chatLines = 0;
            Clock::time_point diagWindow{};   // the same for DIAG reports
            int diagLines = 0;
            // what the page last said about how this device runs a match (HEALTH): its own frames a second, ms of work per frame, game Hz
            bool hasHealth = false;
            double healthFps = 0, healthWorkMs = 0;
            int healthHz = 0;
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
            bool away = false;               // the game is in the background (Away); only changes are passed on
            std::vector<uint32_t> need;   // per player, the first frame this client doesn't have yet
            std::vector<uint32_t> sent;   // per player, the first frame not sent to this client yet
            uint32_t lastAckOwn = 0;
            proto::RttWindow rtt;            // recent round trips (sampled every paceMs)
            uint32_t advised = 0;            // the delay last sent to this player (Start or Pace)
            Clock::time_point lastSend{};
            bool dirty = false;
            // how this player's input packets arrived since the last diagnostics line (Server::diagTick)
            struct InputWindow
            {
                uint32_t packets = 0, behindSamples = 0, behindMax = 0;
                double gapSumMs = 0, gapMaxMs = 0, behindSum = 0;
            } window;
            Clock::time_point lastInput{};
            // per player, the first frame to send next: what the client lacks, except that a WebSocket (reliable and ordered) delivers
            // everything sent, so nothing goes twice (a silent browser isn't sent the same frames every tick until its buffers fill)
            uint32_t sendFrom(size_t player) const { return peer.ws ? std::max(need[player], sent[player]) : need[player]; }
        };

        struct Match
        {
            int lobby = 0;
            std::vector<Slot> slots;
            std::string config;            // config of the first to join: everyone must match it
            std::string refusal;           // the first seat refused (other build or settings): the match never starts
            bool started = false, desynced = false;
            uint32_t delay = 0;
            int step = 1;                  // fields per game frame (proto::matchStep): a frame lasts proto::frameMs(step)
            InputTimeline inputs{0, 0};
            struct SlotHash { uint64_t hash = 0; std::vector<uint64_t> blocks; };   // blocks: the 256 block hashes, when sent
            std::map<uint32_t, std::vector<std::optional<SlotHash>>> hashes;
            Clock::time_point created{}, allJoined{}, lastPace{}, lastDiag{}, lastBehind{};
            bool allJoinedSet = false;
        };

        struct Lobby
        {
            bool open = false;             // slot in use (created, and has someone in it or is in a match)
            std::string code, name, password;
            int host = -1;                 // owner's index in conns_
            std::string stage = "1";       // a stock stage number or a saved map's id (proto::validStage)
            int wins = 0;
            std::string mods;              // gameplay modifiers, canonical list (wotm_modifiers.hpp); "" = none
            std::vector<int> members;      // indices into conns_
            std::string build;
            std::unique_ptr<Match> match;
            Clock::time_point lastPings{};   // the last PINGS line sent to the members
            std::string diagFile;            // where this lobby's diagnostics go: its own file, then its match's
            struct Ban { std::string name; Clock::time_point until; };
            std::vector<Ban> bans;           // members the owner removed, by name, until the time given (the lobby's own: gone when it closes)
        };

        void log(const char *fmt, ...);
        void acceptAndReadLobby();
        void addConn(std::unique_ptr<LineLink> link, bool halfRate = false);   // halfRate: a phone (its WebSocket User-Agent)
        void handleLine(int c, const std::string &line);
        void createLobby(int c, const std::vector<std::string> &w);
        void joinLobby(int c, const std::vector<std::string> &w);
        void setLobby(int c, const std::vector<std::string> &w);
        void changeMonster(int c, const std::vector<std::string> &w);
        // KICK <name> and OWNER <name>: owner only, while the lobby waits
        void kickMember(int c, const std::string &name);
        void handOver(int c, const std::string &name);
        // the lobby `c` owns and that has not started, or null after answering the ERROR
        Lobby *ownedWaitingLobby(int c, const char *notOwner);
        int memberNamed(const Lobby &l, const std::string &name) const;
        // HEALTH <own fps> <work ms> <hz>
        void setHealth(int c, const std::vector<std::string> &w);
        // joins lobby li (already validated): member, owner if first, JOINED, SETTINGS and MEMBERS
        void addMember(int c, int li, const std::string &name, const proto::Pick &pick);
        // `except`: a member left out of the costume check (the one changing monster)
        bool monsterFits(const Lobby &l, const proto::Pick &pick, std::string &err, int except = -1) const;
        void leaveLobby(int c);
        bool closeIfEmpty(int lobby);
        void sendMembers(int lobby);
        // pings the members of waiting lobbies and tells each lobby its members' round trips (PINGS)
        void tickLobbyPings();
        // CHAT <text>: plain text to everyone in the member's waiting lobby, rate limited
        void chat(int c, const std::string &line);
        // SETTINGS <code> stage=<n or map id> wins=<n> mods=<list>
        std::string settingsLine(const Lobby &l) const;
        void sendSettings(int lobby);
        void startMatch(int lobby);
        // DIAG <key=value>...: a page's report, journaled and filed under its player and lobby (never answered, never relayed)
        void diagReport(int c, const std::string &line);
        // a line of the server's own diagnostics, "src=server lobby=<code> <text>", to the journal and to the lobby's file
        void diagEvent(const Lobby &l, const std::string &text);
        void diagLine(const std::string &file, const std::string &line, bool fromServer = true);
        // the server's view of every player of a started match: transport, Hz, delay, round trip, how input packets arrived
        void diagTick(Match &m, const Lobby &l);
        void sampleBehind(Match &m);
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
        void refuse(PeerRef peer, const std::string &why);   // sends Reject{why} and closes the connection
        void onJoin(PeerRef peer, const proto::Join &j);
        void dropSlot(Match &m, int slot);
        void tickMatches();
        void sendInputs(Match &m, int slot);
        void checkHashes(Match &m);
        void send(PeerRef peer, uint8_t channel, const std::vector<uint8_t> &data, bool reliable);
        Match *matchOf(PeerRef peer, int &slot);
        static void *&dataOf(PeerRef peer);
        // a round trip at its high end: ENet's smoothed one plus twice its variation (a WebSocket's is a single ping)
        uint32_t rttHighOf(PeerRef peer) const;
        std::vector<uint32_t> rttHighs(Match &m);
        void pace(Match &m, const Lobby &l);

        ServerConfig cfg_;
        Listener listener_, wsListener_;
        std::vector<std::unique_ptr<Conn>> conns_;
        std::vector<std::unique_ptr<WsLink>> wsPending_;   // accepted, handshake not done yet
        std::vector<std::unique_ptr<WsPeer>> wsPeers_;     // browsers on /match
        std::vector<Lobby> lobbies_;
        diag::Archive archive_;
        Clock::time_point lastPrune_{};
        _ENetHost *host_ = nullptr;
        bool enetUp_ = false;
        std::mt19937_64 rng_;
        // what the announcer sends the master (updated by poll): open lobbies, lobbies waiting for players, players in all lobbies
        std::atomic<int> open_{0}, waiting_{0}, players_{0};
        std::atomic<uint32_t> testRttMs_{0};
        std::thread announcer_;
        std::mutex announceMu_;
        std::condition_variable announceCv_;
        bool stopAnnouncing_ = false;
    };
}
