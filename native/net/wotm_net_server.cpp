// ABOUTME: Netcode server implementation: lobby (text lines over TCP or WebSocket) and lockstep matches over ENet or WebSocket
// ABOUTME: (input relay, delay from ping, state hashes, drops). No game code; runs in wotm_server and in the tests.
#include "wotm_net_server.hpp"
#include "wotm_confighash.hpp"
#include "wotm_modifiers.hpp"
#include "wotm_monsters.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace wotm_net
{
    namespace
    {
        const char *const kBadPick = "ERROR bad monster (a monster number, or <monster>:<skin id> with a-z, 0-9 and _ in the skin id, "
                                     "not starting with a digit)";

        // peer->data holds the match's (lobby, slot): 1 + lobby * 64 + slot (0 = no token yet)
        void *peerTag(int lobby, int slot) { return reinterpret_cast<void *>(static_cast<uintptr_t>(1 + lobby * 64 + slot)); }

        std::string hex64(uint64_t v)
        {
            char b[17];
            std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
            return b;
        }

        std::string join(const std::vector<std::string> &v, const char *sep)
        {
            std::string s;
            for (size_t i = 0; i < v.size(); ++i)
                s += (i ? sep : "") + v[i];
            return s;
        }
    }

    Server::Server(ServerConfig c) : cfg_(c), rng_(std::random_device{}()) { lobbies_.resize(static_cast<size_t>(std::max(1, c.lobbies))); }

    Server::~Server()
    {
        {
            std::lock_guard<std::mutex> lk(announceMu_);
            stopAnnouncing_ = true;
        }
        announceCv_.notify_all();
        if (announcer_.joinable())
            announcer_.join();
        if (host_)
            enet_host_destroy(host_);
        if (enetUp_)
            enet_deinitialize();
    }

    void Server::log(const char *fmt, ...)
    {
        if (!cfg_.log)
            return;
        va_list ap;
        va_start(ap, fmt);
        std::printf("[server] ");
        std::vprintf(fmt, ap);
        std::printf("\n");
        std::fflush(stdout);
        va_end(ap);
    }

    bool Server::start(std::string &err)
    {
        cfg_.maxPlayers = std::clamp(cfg_.maxPlayers, 1, proto::kMaxPlayers);
        if (!listener_.listen(cfg_.port))
        {
            err = "could not listen on TCP port " + std::to_string(cfg_.port);
            return false;
        }
        if (enet_initialize() != 0)
        {
            err = "enet_initialize failed";
            return false;
        }
        enetUp_ = true;
        ENetAddress a;
        a.host = ENET_HOST_ANY;
        a.port = cfg_.port;
        host_ = enet_host_create(&a, static_cast<size_t>(cfg_.lobbies * cfg_.maxPlayers + 16), proto::kChannels, 0, 0);
        if (!host_)
        {
            err = "could not listen on UDP port " + std::to_string(cfg_.port);
            return false;
        }
        log("listening on port %u (TCP lobby, UDP match), up to %d lobbies of up to %d players", cfg_.port, cfg_.lobbies, cfg_.maxPlayers);
        if (cfg_.wsPort)
        {
            if (!wsListener_.listen(cfg_.wsPort, cfg_.wsBind.c_str()))
            {
                err = "could not listen for WebSocket on " + cfg_.wsBind + ":" + std::to_string(cfg_.wsPort);
                return false;
            }
            log("WebSocket (browser) lobby and match on %s:%u (/lobby, /match)", cfg_.wsBind.c_str(), cfg_.wsPort);
        }
        if (!cfg_.announceHost.empty())
        {
            log("announcing as %s to the master %s:%u every %d s", cfg_.name.c_str(), cfg_.announceHost.c_str(), cfg_.announcePort,
                cfg_.announceEveryMs / 1000);
            announcer_ = std::thread([this] { announceLoop(); });
        }
        return true;
    }

    void Server::poll(int ms)
    {
        if (!host_)   // start failed or was never called
            return;
        acceptAndReadLobby();
        serviceWebSockets();
        serviceEnet(ms);
        tickMatches();
        tickLobbyPings();
        updateCounts();
    }

    // ---- public listing (master server) ----

    void Server::updateCounts()
    {
        int open = 0, waiting = 0, players = 0;
        for (const Lobby &l : lobbies_)
        {
            if (!l.open)
                continue;
            ++open;
            waiting += l.match ? 0 : 1;
            players += static_cast<int>(l.match ? l.match->slots.size() : l.members.size());
        }
        open_ = open, waiting_ = waiting, players_ = players;
    }

    // On its own thread, so a slow or unreachable master never holds up the lobby or a match. A failed announce is retried at the
    // next interval (the master keeps a server listed for 3 intervals).
    void Server::announceLoop()
    {
        const std::string name = proto::cleanName(cfg_.name).empty() ? "wotlane" : proto::cleanName(cfg_.name);
        bool warned = false;
        std::unique_lock<std::mutex> lk(announceMu_);
        while (!stopAnnouncing_)
        {
            lk.unlock();
            LineSocket s;
            std::string reply;
            const bool ok = s.connect(cfg_.announceHost, cfg_.announcePort, 2000) &&
                            s.sendLine("ANNOUNCE " + std::to_string(proto::kProtocol) + " " + std::to_string(cfg_.port) + " " +
                                       std::to_string(open_.load()) + " " + std::to_string(waiting_.load()) + " " +
                                       std::to_string(players_.load()) + " " + std::to_string(cfg_.lobbies) + " " + name) &&
                            s.readLine(reply, 2000) && reply == "OK";
            if (ok == warned)   // log the first failure and the recovery, not every attempt
            {
                warned = !ok;
                log(ok ? "master %s:%u reachable again" : "could not announce to the master %s:%u (%s)", cfg_.announceHost.c_str(),
                    cfg_.announcePort, ok ? "" : (reply.empty() ? "no answer" : reply.c_str()));
            }
            lk.lock();
            announceCv_.wait_for(lk, std::chrono::milliseconds(cfg_.announceEveryMs), [this] { return stopAnnouncing_; });
        }
    }

    // ---- lobby ----

    void Server::acceptAndReadLobby()
    {
        LineSocket s;
        while (listener_.accept(s))
            addConn(std::make_unique<LineSocket>(std::move(s)));
        for (size_t i = 0; i < conns_.size(); ++i)
        {
            if (!conns_[i])
                continue;
            const bool alive = conns_[i]->link->pump();
            std::string line;
            while (conns_[i] && conns_[i]->link->nextLine(line))
                handleLine(static_cast<int>(i), line);
            if (conns_[i] && (!alive || !conns_[i]->link->open()))
            {
                leaveLobby(static_cast<int>(i));
                conns_[i].reset();
            }
        }
    }

    void Server::addConn(std::unique_ptr<LineLink> link, bool halfRate)
    {
        auto c = std::make_unique<Conn>();
        c->link = std::move(link);
        c->halfRate = halfRate;
        auto free = std::find(conns_.begin(), conns_.end(), nullptr);
        if (free != conns_.end())
            *free = std::move(c);
        else
            conns_.push_back(std::move(c));
    }

    int Server::openLobbies() const
    {
        int n = 0;
        for (const Lobby &l : lobbies_)
            n += l.open ? 1 : 0;
        return n;
    }

    int Server::lobbyByCode(const std::string &code) const
    {
        std::string up = code;
        for (char &ch : up)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        for (size_t i = 0; i < lobbies_.size(); ++i)
            if (lobbies_[i].open && lobbies_[i].code == up)
                return static_cast<int>(i);
        return -1;
    }

    std::string Server::newCode()
    {
        const std::string letters = proto::kCodeLetters;
        for (;;)
        {
            std::string code;
            for (int i = 0; i < 4; ++i)
                code += letters[static_cast<size_t>(rng_() % letters.size())];
            if (lobbyByCode(code) < 0)
                return code;
        }
    }

    std::string Server::lobbyLine(int i) const
    {
        const Lobby &l = lobbies_[static_cast<size_t>(i)];
        std::vector<std::string> names;
        if (l.match)
            for (const Slot &s : l.match->slots)
                names.push_back(s.name);
        else
            for (int m : l.members)
                names.push_back(conns_[static_cast<size_t>(m)]->name);
        const std::string host = l.host >= 0 && conns_[static_cast<size_t>(l.host)] ? conns_[static_cast<size_t>(l.host)]->name : "-";
        return "LOBBY " + l.code + (l.match ? " playing " : " waiting ") + std::to_string(names.size()) + "/" + std::to_string(cfg_.maxPlayers) +
               (l.password.empty() ? " open " : " locked ") + l.name + " " + host + " " + (names.empty() ? "-" : join(names, ","));
    }

    void Server::handleLine(int ci, const std::string &line)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        const std::vector<std::string> w = proto::words(line);
        if (w.empty())
            return;
        const std::string &cmd = w[0];
        if (cmd == "HELLO")
        {
            if (w.size() < 3 || w[1] != std::to_string(proto::kProtocol))
            {
                c.link->sendLine("ERROR different protocol (the server speaks " + std::to_string(proto::kProtocol) + "; update wotlane)");
                return;
            }
            c.hello = true;
            c.build = w[2];
            c.halfRate = c.halfRate || proto::keyValues(w)["hz"] == "30";   // HELLO <protocol> <build> [hz=30], or a phone's User-Agent
            c.link->sendLine("WELCOME " + std::to_string(openLobbies()));
            return;
        }
        if (!c.hello)
        {
            c.link->sendLine("ERROR send HELLO first");
            return;
        }
        if (cmd == "LIST")
        {
            for (size_t i = 0; i < lobbies_.size(); ++i)
                if (lobbies_[i].open)
                    c.link->sendLine(lobbyLine(static_cast<int>(i)));
            c.link->sendLine("END");
        }
        else if (cmd == "CREATE" && w.size() >= 4)
            createLobby(ci, w);
        else if (cmd == "JOIN" && w.size() >= 4)
            joinLobby(ci, w);
        else if (cmd == "SET")
            setLobby(ci, w);
        else if (cmd == "CHAT")
            chat(ci, line);
        else if (cmd == "MONSTER" && w.size() >= 2)
            changeMonster(ci, w);
        else if (cmd == "LEAVE")
        {
            const std::string code = c.lobby >= 0 ? lobbies_[static_cast<size_t>(c.lobby)].code : "";
            leaveLobby(ci);   // the last one out closes the lobby (and the code goes away)
            if (!code.empty())
                c.link->sendLine("LEFT " + code);
        }
        else if (cmd == "START")
        {
            if (c.lobby < 0)
                return void(c.link->sendLine("ERROR join a lobby first"));
            if (lobbies_[static_cast<size_t>(c.lobby)].host != ci)
                return void(c.link->sendLine("ERROR only the lobby owner can start the match"));
            startMatch(c.lobby);
        }
        else
            c.link->sendLine("ERROR unknown command: " + cmd);
    }

    // the monster exists on the disc and still has a free costume given the other members' (costumes as the match hands them out:
    // wotm_monsters::fits). A skin is its own costume: no other member may have the same one, and it takes no disc costume
    bool Server::monsterFits(const Lobby &l, const proto::Pick &pick, std::string &err, int except) const
    {
        const int monster = pick.monster;
        if (wotm_monsters::costumes(monster) == 0)
        {
            err = "invalid monster (no files on the disc)";
            return false;
        }
        if (!pick.skin.empty())
        {
            for (int m : l.members)
                if (m != except && conns_[static_cast<size_t>(m)]->pick.monster == monster && conns_[static_cast<size_t>(m)]->pick.skin == pick.skin)
                {
                    err = "that skin is already taken in the lobby (pick another skin or costume)";
                    return false;
                }
            return true;
        }
        int list[wotm_monsters::kMaxList], n = 0;
        for (int m : l.members)
            if (m != except && n < wotm_monsters::kMaxList - 1 && conns_[static_cast<size_t>(m)]->pick.skin.empty())
                list[n++] = conns_[static_cast<size_t>(m)]->pick.monster;
        list[n++] = monster;
        if (!wotm_monsters::fits(list, nullptr, n))
        {
            err = "monster has no free costume in the lobby (all its costumes are taken)";
            return false;
        }
        return true;
    }

    // CREATE <lobby> <name> <monster> [password]
    void Server::createLobby(int ci, const std::vector<std::string> &w)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        const std::string lobbyName = proto::cleanName(w[1]), name = proto::cleanName(w[2]);
        proto::Pick pick;
        const std::string password = w.size() >= 5 ? proto::cleanName(w[4]) : "";
        if (lobbyName.empty() || name.empty())
            return void(c.link->sendLine("ERROR empty name (use letters, digits, _ or -)"));
        if (!proto::parsePick(w[3], pick))
            return void(c.link->sendLine(kBadPick));
        int li = -1;
        for (size_t i = 0; i < lobbies_.size() && li < 0; ++i)
            if (!lobbies_[i].open && !lobbies_[i].match)
                li = static_cast<int>(i);
        if (li < 0)
            return void(c.link->sendLine("ERROR the server already has " + std::to_string(lobbies_.size()) + " lobbies open; join one of them"));
        Lobby probe;
        std::string err;
        if (!monsterFits(probe, pick, err))
            return void(c.link->sendLine("ERROR " + err));
        leaveLobby(ci);
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        l.open = true;
        l.code = newCode();
        l.name = lobbyName;
        l.password = password;
        l.host = -1;
        l.stage = "1";
        l.wins = 0;
        l.mods.clear();
        l.members.clear();
        l.build = c.build;
        c.link->sendLine("CREATED " + l.code);
        log("lobby %s (%s): created by %s%s", l.code.c_str(), l.name.c_str(), name.c_str(), password.empty() ? "" : ", with password");
        addMember(ci, li, name, pick);
    }

    // JOIN <code> <name> <monster> [password]
    void Server::joinLobby(int ci, const std::vector<std::string> &w)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        const int li = lobbyByCode(w[1]);
        const std::string name = proto::cleanName(w[2]);
        proto::Pick pick;
        const std::string password = w.size() >= 5 ? w[4] : "";
        if (!proto::parsePick(w[3], pick))
            return void(c.link->sendLine(kBadPick));
        if (li < 0)
            return void(c.link->sendLine("ERROR no such lobby (check the code)"));
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (c.lobby == li)
            return void(c.link->sendLine("ERROR you are already in this lobby"));
        if (name.empty())
            return void(c.link->sendLine("ERROR empty name (use letters, digits, _ or -)"));
        if (!l.password.empty() && password != l.password)
            return void(c.link->sendLine("ERROR wrong password (the lobby has a password)"));
        if (l.match)
            return void(c.link->sendLine("ERROR lobby is in a match; wait for it to end"));
        if (static_cast<int>(l.members.size()) >= cfg_.maxPlayers)
            return void(c.link->sendLine("ERROR lobby is full"));
        if (l.build != c.build)
            return void(c.link->sendLine("ERROR build differs from the lobby's (" + l.build + "); everyone needs the same wotlane"));
        for (int m : l.members)
            if (conns_[static_cast<size_t>(m)]->name == name)
                return void(c.link->sendLine("ERROR name already taken in the lobby"));
        std::string err;
        if (!monsterFits(l, pick, err))
            return void(c.link->sendLine("ERROR " + err));
        leaveLobby(ci);
        addMember(ci, li, name, pick);
    }

    void Server::addMember(int ci, int li, const std::string &name, const proto::Pick &pick)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        l.members.push_back(ci);
        if (l.host < 0)
            l.host = ci;
        c.lobby = li, c.name = name, c.pick = pick;
        c.link->sendLine("JOINED " + l.code);
        c.link->sendLine(settingsLine(l));
        log("lobby %s: %s joined (monster %s), %zu player(s)", l.code.c_str(), name.c_str(), pick.text().c_str(), l.members.size());
        sendMembers(li);
    }

    // SET [stage=<stage number or map id>] [wins=<wins>] [mods=<list>]: owner only; an unknown modifier or a bad stage changes nothing
    void Server::setLobby(int ci, const std::vector<std::string> &w)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0)
            return void(c.link->sendLine("ERROR join a lobby first"));
        Lobby &l = lobbies_[static_cast<size_t>(c.lobby)];
        if (l.host != ci)
            return void(c.link->sendLine("ERROR only the lobby owner can change the stage, wins and modifiers"));
        const std::map<std::string, std::string> kv = proto::keyValues(w);
        int mods = 0;
        if (auto it = kv.find("mods"); it != kv.end() && !wotm::modifiers::parse(it->second, mods))
            return void(c.link->sendLine("ERROR unknown modifier in \"" + it->second + "\" (known: jump, fast, turbo)"));
        if (auto it = kv.find("stage"); it != kv.end() && !proto::validStage(it->second))
            return void(c.link->sendLine("ERROR bad stage \"" + it->second + "\" (a stage number, or a saved map's id: a letter, then a-z, 0-9 "
                                         "and _, up to 16)"));
        if (kv.count("mods"))
            l.mods = wotm::modifiers::format(mods);
        if (auto it = kv.find("stage"); it != kv.end())
            l.stage = it->second;
        if (auto it = kv.find("wins"); it != kv.end())
            l.wins = std::clamp(std::atoi(it->second.c_str()), 0, proto::kMaxWins);
        sendSettings(c.lobby);
    }

    // MONSTER <monster>[:<skin>]: any member, while the lobby waits; the same checks as JOIN, against the other members (#14)
    void Server::changeMonster(int ci, const std::vector<std::string> &w)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0)
            return void(c.link->sendLine("ERROR join a lobby first"));
        const Lobby &l = lobbies_[static_cast<size_t>(c.lobby)];
        if (l.match)
            return void(c.link->sendLine("ERROR lobby is in a match; change monster after it ends"));
        proto::Pick pick;
        if (!proto::parsePick(w[1], pick))
            return void(c.link->sendLine(kBadPick));
        std::string err;
        if (!monsterFits(l, pick, err, ci))
            return void(c.link->sendLine("ERROR " + err));
        c.pick = pick;
        log("lobby %s: %s changed monster to %s", l.code.c_str(), c.name.c_str(), pick.text().c_str());
        sendMembers(c.lobby);
    }

    void Server::leaveLobby(int ci)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0)
            return;
        Lobby &l = lobbies_[static_cast<size_t>(c.lobby)];
        const int li = c.lobby;
        l.members.erase(std::remove(l.members.begin(), l.members.end(), ci), l.members.end());
        c.lobby = -1;
        if (l.host == ci)
            l.host = l.members.empty() ? -1 : l.members.front();
        log("lobby %s: %s left", l.code.c_str(), c.name.c_str());
        if (!closeIfEmpty(li))
            sendMembers(li);
    }

    // a lobby with no members and no match closes (drops off the list and frees the slot); returns whether it closed
    bool Server::closeIfEmpty(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (!l.open || !l.members.empty() || l.match)
            return false;
        log("lobby %s: empty, closed", l.code.c_str());
        l.open = false;
        l.code.clear(), l.name.clear(), l.password.clear(), l.build.clear();
        l.host = -1;
        return true;
    }

    void Server::sendMembers(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        std::vector<std::string> v;
        for (int m : l.members)
            v.push_back(conns_[static_cast<size_t>(m)]->name + ":" + conns_[static_cast<size_t>(m)]->pick.text());
        const std::string host = l.host >= 0 ? conns_[static_cast<size_t>(l.host)]->name : "-";
        const std::string line = "MEMBERS " + l.code + " " + host + " " + (v.empty() ? "-" : join(v, ","));
        for (int m : l.members)
            conns_[static_cast<size_t>(m)]->link->sendLine(line);
    }

    void Server::tickLobbyPings()
    {
        const auto now = Clock::now();
        for (Lobby &l : lobbies_)
        {
            if (!l.open || l.match || l.members.empty() || now - l.lastPings < std::chrono::seconds(1))
                continue;
            l.lastPings = now;
            std::vector<std::string> v;
            for (int m : l.members)
            {
                Conn &c = *conns_[static_cast<size_t>(m)];
                c.link->ping();
                const int ms = c.link->pingMs();
                v.push_back(c.name + ":" + (ms < 0 ? "-" : std::to_string(ms)));
            }
            const std::string line = "PINGS " + l.code + " " + join(v, ",");
            for (int m : l.members)
                conns_[static_cast<size_t>(m)]->link->sendLine(line);
        }
    }

    // well-formed UTF-8 (a browser closes a WebSocket that carries a text frame that isn't)
    static bool validUtf8(const std::string &s)
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

    void Server::chat(int ci, const std::string &line)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0)
            return void(c.link->sendLine("ERROR join a lobby first"));
        Lobby &l = lobbies_[static_cast<size_t>(c.lobby)];
        if (l.match)
            return void(c.link->sendLine("ERROR chat is only open before the match"));
        // printable text only: control characters go, the ends are trimmed, and the cut never splits a UTF-8 character
        std::string text;
        for (size_t i = 4; i < line.size(); ++i)
            if (static_cast<unsigned char>(line[i]) >= 0x20 && static_cast<unsigned char>(line[i]) != 0x7f)
                text += line[i];
        if (!validUtf8(text))
            return void(c.link->sendLine("ERROR chat text is not valid text"));
        const auto trim = [&text] {
            const size_t first = text.find_first_not_of(' '), last = text.find_last_not_of(' ');
            text = first == std::string::npos ? "" : text.substr(first, last - first + 1);
        };
        trim();
        if (text.size() > proto::kMaxChatBytes)
        {
            size_t cut = proto::kMaxChatBytes;
            while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                --cut;
            text.resize(cut);
            trim();
        }
        if (text.empty())
            return;
        const auto now = Clock::now();
        if (now - c.chatWindow >= std::chrono::milliseconds(proto::kChatWindowMs))
        {
            c.chatWindow = now;
            c.chatLines = 0;
        }
        if (++c.chatLines > proto::kChatBurst)
            return void(c.link->sendLine("ERROR chat too fast; wait a few seconds"));
        const std::string out = "CHAT " + l.code + " " + c.name + " " + text;
        for (int m : l.members)
            conns_[static_cast<size_t>(m)]->link->sendLine(out);
    }

    std::string Server::settingsLine(const Lobby &l) const
    {
        return "SETTINGS " + l.code + " stage=" + l.stage + " wins=" + std::to_string(l.wins) + " mods=" + l.mods;
    }

    void Server::sendSettings(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        const std::string line = settingsLine(l);
        for (int m : l.members)
            conns_[static_cast<size_t>(m)]->link->sendLine(line);
    }

    // members stay in the lobby during the match (the launcher goes back to it when the game closes)
    void Server::startMatch(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (l.match || l.members.empty())
            return;
        auto m = std::make_unique<Match>();
        m->lobby = li;
        m->created = Clock::now();
        const int players = static_cast<int>(l.members.size());
        const uint32_t seed = static_cast<uint32_t>(rng_() % 0x7FFFFFFEull) + 1u;
        std::vector<std::string> halfRateNames;   // members whose device asked for 30 Hz
        for (int ci : l.members)
            if (conns_[static_cast<size_t>(ci)]->halfRate)
                halfRateNames.push_back(conns_[static_cast<size_t>(ci)]->name);
        const bool halfRate = !halfRateNames.empty();
        const int step = proto::matchStep(players, halfRate);
        m->step = step;
        std::vector<std::string> monsters, names;
        for (int ci : l.members)
        {
            const Conn &c = *conns_[static_cast<size_t>(ci)];
            monsters.push_back(c.pick.text());
            names.push_back(c.name);
            Slot s;
            s.token = hex64(rng_());
            s.name = c.name;
            s.need.assign(static_cast<size_t>(players), 0u);
            s.sent.assign(static_cast<size_t>(players), 0u);
            m->slots.push_back(std::move(s));
        }
        for (int i = 0; i < players; ++i)
        {
            Conn &c = *conns_[static_cast<size_t>(l.members[static_cast<size_t>(i)])];
            c.link->sendLine("MATCH lobby=" + l.code + " players=" + std::to_string(players) + " stage=" + l.stage +
                              " wins=" + std::to_string(l.wins) + " mods=" + l.mods + " monsters=" + join(monsters, ",") +
                              " names=" + join(names, ",") +
                              " seed=" + std::to_string(seed) + " step=" + std::to_string(step) + " you=" + std::to_string(i) +
                              " token=" + m->slots[static_cast<size_t>(i)].token + " port=" + std::to_string(cfg_.port));
        }
        log("lobby %s: match with %d player(s) (%s), stage %s, %d win(s), seed %u, %d Hz%s", l.code.c_str(), players, join(names, ",").c_str(),
            l.stage.c_str(), l.wins, seed, 60 / step, halfRate ? (" (30 Hz device: " + join(halfRateNames, ",") + ")").c_str() : "");
        l.match = std::move(m);
    }

    // ---- match ----

    void Server::send(PeerRef peer, uint8_t channel, const std::vector<uint8_t> &data, bool reliable)
    {
        if (!peer)
            return;
        if (peer.ws)
        {
            // WebSocket is reliable and ordered: one binary message per packet, the channel in front
            std::vector<uint8_t> packet;
            packet.reserve(data.size() + 1);
            packet.push_back(channel);
            packet.insert(packet.end(), data.begin(), data.end());
            peer.ws->link->sendPacket(packet);
            return;
        }
        // the test's packet loss: only ENet loses packets (a WebSocket is sent each input once)
        if (!reliable && cfg_.dropInputPercent > 0 && static_cast<int>(rng_() % 100u) < cfg_.dropInputPercent)
            return;
        ENetPacket *p = enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(peer.enet, channel, p);
    }

    void *&Server::dataOf(PeerRef peer) { return peer.enet ? peer.enet->data : peer.ws->data; }

    uint32_t Server::rttHighOf(PeerRef peer) const
    {
        const uint32_t rtt = peer.enet ? peer.enet->roundTripTime + 2u * peer.enet->roundTripTimeVariance : peer.ws->link->rttMs();
        return rtt + testRttMs_.load();
    }

    // each player's recent round trip at its high end (0 for a player who left: nobody waits for them)
    std::vector<uint32_t> Server::rttHighs(Match &m)
    {
        std::vector<uint32_t> out;
        for (Slot &s : m.slots)
            out.push_back(s.peer && !s.gone ? s.rtt.high() : 0u);
        return out;
    }

    // During the match, every paceMs: samples each player's round trip and sends a player whose delay should change its new one (Pace).
    // The window makes a rise immediate and a fall wait until the high samples leave it
    void Server::pace(Match &m, const Lobby &l)
    {
        const Clock::time_point now = Clock::now();
        if (cfg_.forcedDelay || now - m.lastPace < std::chrono::milliseconds(cfg_.paceMs))
            return;
        m.lastPace = now;
        for (Slot &s : m.slots)
            if (s.peer && !s.gone)
                s.rtt.add(rttHighOf(s.peer));
        const std::vector<uint32_t> highs = rttHighs(m);
        const double frame = proto::frameMs(m.step);
        for (size_t i = 0; i < m.slots.size(); ++i)
        {
            Slot &s = m.slots[i];
            const uint32_t d = proto::adviseDelay(s.advised, highs, i, frame);
            if (!s.peer || s.gone || d == s.advised)
                continue;
            log("lobby %s: %s's delay %u -> %u frame(s) (ping ms, high end: %u)", l.code.c_str(), s.name.c_str(), s.advised, d, highs[i]);
            s.advised = d;
            send(s.peer, proto::kControl, proto::encode(proto::Pace{d}), true);
        }
    }

    Server::Match *Server::matchOf(PeerRef peer, int &slot)
    {
        const uintptr_t tag = reinterpret_cast<uintptr_t>(dataOf(peer));
        if (tag == 0)
            return nullptr;
        const int li = static_cast<int>((tag - 1) / 64);
        slot = static_cast<int>((tag - 1) % 64);
        if (li >= static_cast<int>(lobbies_.size()) || !lobbies_[static_cast<size_t>(li)].match)
            return nullptr;
        Match *m = lobbies_[static_cast<size_t>(li)].match.get();
        return slot < static_cast<int>(m->slots.size()) && m->slots[static_cast<size_t>(slot)].peer == peer ? m : nullptr;
    }

    void Server::serviceEnet(int ms)
    {
        ENetEvent ev;
        int wait = ms;
        while (enet_host_service(host_, &ev, static_cast<enet_uint32>(wait)) > 0)
        {
            wait = 0;
            switch (ev.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                ev.peer->data = nullptr;
                enet_peer_ping_interval(ev.peer, 100);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
            {
                const std::vector<uint8_t> data(ev.packet->data, ev.packet->data + ev.packet->dataLength);
                enet_packet_destroy(ev.packet);
                onReceive(PeerRef{ev.peer, nullptr}, data);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                onDisconnect(PeerRef{ev.peer, nullptr});
                break;
            default:
                break;
            }
        }
    }

    void Server::onReceive(PeerRef peer, const std::vector<uint8_t> &data)
    {
        int si = 0;
        Match *m = matchOf(peer, si);
        switch (proto::typeOf(data))
        {
        case proto::Type::Join:
        {
            proto::Join j;
            if (proto::decode(data, j))
                onJoin(peer, j);
            break;
        }
        case proto::Type::Input:
        {
            proto::Input in;
            if (!m || !m->started || !proto::decode(data, in))
                break;
            Slot &s = m->slots[static_cast<size_t>(si)];
            for (size_t q = 0; q < in.acks.size() && q < s.need.size(); ++q)
                s.need[q] = std::max(s.need[q], in.acks[q]);
            const uint32_t before = m->inputs.nextMissing(si);
            for (size_t k = 0; k < in.own.states.size(); ++k)
                m->inputs.set(si, in.own.first + static_cast<uint32_t>(k), in.own.states[k]);
            if (m->inputs.nextMissing(si) != before)
                for (Slot &o : m->slots)
                    o.dirty = true;
            else
                s.dirty = true;   // ack again (the packet may be a resend because the ack was lost)
            break;
        }
        case proto::Type::Hash:
        {
            proto::Hash h;
            if (!m || !m->started || !proto::decode(data, h))
                break;
            auto &v = m->hashes[h.frame];
            v.resize(m->slots.size());
            v[static_cast<size_t>(si)] = Match::SlotHash{h.hash, std::move(h.blocks)};
            checkHashes(*m);
            break;
        }
        case proto::Type::Away:
        {
            proto::Away a;
            if (!m || !proto::decode(data, a) || m->slots[static_cast<size_t>(si)].away == a.away)
                break;
            m->slots[static_cast<size_t>(si)].away = a.away;
            a.player = static_cast<uint8_t>(si);
            for (size_t q = 0; q < m->slots.size(); ++q)
                if (static_cast<int>(q) != si && m->slots[q].peer && !m->slots[q].gone)
                    send(m->slots[q].peer, proto::kControl, proto::encode(a), true);
            log("lobby %s: %s's game %s", lobbies_[static_cast<size_t>(m->lobby)].code.c_str(), m->slots[static_cast<size_t>(si)].name.c_str(),
                a.away ? "went to the background" : "is back");
            break;
        }
        default:
            break;
        }
    }

    void Server::refuse(PeerRef peer, const std::string &why)
    {
        send(peer, proto::kControl, proto::encode(proto::Reject{why}), true);
        if (peer.enet)
            enet_peer_disconnect_later(peer.enet, 0);
        else
            peer.ws->link->close();   // serviceWebSockets drops it on the next pass
        log("match: rejected (%s)", why.c_str());
    }

    void Server::onJoin(PeerRef peer, const proto::Join &j)
    {
        auto reject = [&](const std::string &why) { refuse(peer, why); };
        if (j.protocol != proto::kProtocol)
            return reject("different protocol");
        if (dataOf(peer))
            return reject("this connection already holds a player");
        for (size_t li = 0; li < lobbies_.size(); ++li)
        {
            Match *m = lobbies_[li].match.get();
            if (!m)
                continue;
            for (size_t si = 0; si < m->slots.size(); ++si)
            {
                Slot &s = m->slots[si];
                if (s.token != j.token)
                    continue;
                if (s.joined)
                    return reject("token already used");
                // a seat that can't play this match: refused, and remembered so the match never starts with it neutral
                auto refuseSeat = [&](const std::string &why) {
                    if (m->refusal.empty())
                        m->refusal = s.name + " could not join (" + why + ")";
                    reject(why);
                };
                if (lobbies_[li].build != j.buildId && !lobbies_[li].build.empty())
                    return refuseSeat("different build (" + j.buildId + " vs " + lobbies_[li].build + ")");
                if (!m->config.empty() && m->config != j.configHash)
                {
                    const std::string what = confighash::mismatch(j.configHash, m->config);
                    return refuseSeat("game configuration differs from the others" + (what.empty() ? "" : ": " + what) + " (" +
                                      j.configHash + " vs " + m->config + ")");
                }
                if (m->config.empty())
                    m->config = j.configHash;
                s.joined = true;
                s.peer = peer;
                dataOf(peer) = peerTag(static_cast<int>(li), static_cast<int>(si));
                send(peer, proto::kControl, proto::encode(proto::Welcome{static_cast<uint8_t>(si), static_cast<uint8_t>(m->slots.size())}), true);
                log("lobby %zu: %s reached the match (player %zu)", li, s.name.c_str(), si + 1);
                return;
            }
        }
        reject("unknown token (the match ended or it belongs to another server)");
    }

    void Server::onDisconnect(PeerRef peer)
    {
        int si = 0;
        if (Match *m = matchOf(peer, si))
        {
            m->slots[static_cast<size_t>(si)].peer = {};
            dropSlot(*m, si);
        }
        dataOf(peer) = nullptr;
    }

    void Server::serviceWebSockets()
    {
        if (!cfg_.wsPort)
            return;
        LineSocket s;
        while (wsListener_.accept(s))
            wsPending_.push_back(std::make_unique<WsLink>(std::move(s)));
        for (size_t i = 0; i < wsPending_.size();)
        {
            const bool alive = wsPending_[i]->pump();
            if (!wsPending_[i]->ready())
            {
                if (alive)
                    ++i;
                else
                    wsPending_.erase(wsPending_.begin() + static_cast<long>(i));
                continue;
            }
            std::unique_ptr<WsLink> link = std::move(wsPending_[i]);
            wsPending_.erase(wsPending_.begin() + static_cast<long>(i));
            if (link->path() == "/lobby")
            {
                const bool phone = proto::halfRateUserAgent(link->userAgent());
                addConn(std::move(link), phone);   // lines that arrived with the handshake wait in the link for acceptAndReadLobby
            }
            else if (link->path() == "/match")
            {
                auto p = std::make_unique<WsPeer>();
                p->link = std::move(link);
                wsPeers_.push_back(std::move(p));
            }
            else
                link->close();
        }
        const Clock::time_point now = Clock::now();
        for (size_t i = 0; i < wsPeers_.size();)
        {
            WsPeer &p = *wsPeers_[i];
            const PeerRef ref{nullptr, &p};
            const bool alive = p.link->pump();
            std::vector<uint8_t> packet;
            while (p.link->nextPacket(packet))
                if (!packet.empty())
                    onReceive(ref, std::vector<uint8_t>(packet.begin() + 1, packet.end()));   // the channel byte only matters to ENet
            if (!alive || !p.link->open())
            {
                onDisconnect(ref);
                wsPeers_.erase(wsPeers_.begin() + static_cast<long>(i));
                continue;
            }
            if (now - p.lastPing >= std::chrono::milliseconds(100))
            {
                p.link->ping();
                p.lastPing = now;
            }
            ++i;
        }
    }

    // player gone: neutral from the first frame of theirs the server doesn't have (everyone gets the earlier ones)
    void Server::dropSlot(Match &m, int si)
    {
        Slot &s = m.slots[static_cast<size_t>(si)];
        if (s.gone)
            return;
        s.gone = true;
        const uint32_t frame = m.started ? m.inputs.nextMissing(si) : 0u;
        if (m.started)
            m.inputs.drop(si, frame);
        for (Slot &o : m.slots)
            if (o.peer && !o.gone)
            {
                send(o.peer, proto::kControl, proto::encode(proto::Drop{static_cast<uint8_t>(si), frame}), true);
                o.dirty = true;
            }
        if (!m.started)
            m.hashes.clear();
        log("lobby %s: %s left the match (neutral from frame %u)", lobbies_[static_cast<size_t>(m.lobby)].code.c_str(), s.name.c_str(), frame);
        checkHashes(m);
    }

    void Server::tickMatches()
    {
        const Clock::time_point now = Clock::now();
        for (Lobby &l : lobbies_)
        {
            Match *m = l.match.get();
            if (!m)
                continue;
            if (!m->started && !m->refusal.empty())
            {
                // a refused seat: nobody plays this match; everyone already in hears who and why, and the lobby waits again
                log("lobby %s: match cancelled, %s", l.code.c_str(), m->refusal.c_str());
                for (Slot &s : m->slots)
                    if (s.peer && !s.gone)
                    {
                        dataOf(s.peer) = nullptr;
                        refuse(s.peer, "the match did not start: " + m->refusal);
                    }
                const int li = m->lobby;
                l.match.reset();
                if (!closeIfEmpty(li))
                    sendMembers(li);
                continue;
            }
            if (!m->started)
            {
                bool all = true;
                for (Slot &s : m->slots)
                    all = all && (s.joined || s.gone);
                if (!all && now - m->created > std::chrono::milliseconds(cfg_.joinTimeoutMs))
                    for (size_t i = 0; i < m->slots.size(); ++i)
                        if (!m->slots[i].joined)
                            dropSlot(*m, static_cast<int>(i));
                if (all && !m->allJoinedSet)
                    m->allJoinedSet = true, m->allJoined = now;
                if (all && now - m->allJoined >= std::chrono::milliseconds(cfg_.startWaitMs))
                {
                    proto::Start st;
                    for (Slot &s : m->slots)
                    {
                        if (s.peer && !s.gone)
                            s.rtt.add(rttHighOf(s.peer));
                        st.names.push_back(s.name);
                    }
                    const std::vector<uint32_t> rtt = rttHighs(*m);
                    st.delay = cfg_.forcedDelay ? cfg_.forcedDelay : proto::startDelay(rtt, proto::frameMs(m->step));
                    for (Slot &s : m->slots)
                        s.advised = st.delay;
                    m->lastPace = now;
                    m->delay = st.delay;
                    m->inputs = InputTimeline(static_cast<int>(m->slots.size()), st.delay);
                    for (size_t i = 0; i < m->slots.size(); ++i)
                        if (m->slots[i].gone)
                            m->inputs.drop(static_cast<int>(i), 0);
                    m->started = true;
                    std::string pings;
                    for (uint32_t r : rtt)
                        pings += (pings.empty() ? "" : ",") + std::to_string(r);
                    log("lobby %s: start, delay %u frame(s) (ping ms: %s)", l.code.c_str(), st.delay, pings.c_str());
                    for (Slot &s : m->slots)
                        if (s.peer && !s.gone)
                        {
                            send(s.peer, proto::kControl, proto::encode(st), true);
                            for (size_t q = 0; q < m->slots.size(); ++q)
                                if (m->slots[q].gone)
                                    send(s.peer, proto::kControl, proto::encode(proto::Drop{static_cast<uint8_t>(q), 0}), true);
                        }
                }
            }
            else if (!m->desynced)
            {
                pace(*m, l);
                for (size_t i = 0; i < m->slots.size(); ++i)
                {
                    Slot &s = m->slots[i];
                    if (!s.peer || s.gone)
                        continue;
                    bool behind = m->inputs.nextMissing(static_cast<int>(i)) != s.lastAckOwn;
                    for (size_t q = 0; q < m->slots.size(); ++q)
                        behind = behind || (q != i && s.sendFrom(q) < m->inputs.nextMissing(static_cast<int>(q)));
                    if (s.dirty || (behind && now - s.lastSend > std::chrono::milliseconds(16)))
                        sendInputs(*m, static_cast<int>(i));
                }
                uint32_t oldest = 0xFFFFFFFFu;
                for (const Slot &s : m->slots)
                    if (!s.gone)
                        for (uint32_t n : s.need)
                            oldest = std::min(oldest, n);
                if (oldest != 0xFFFFFFFFu)
                    m->inputs.discardBefore(oldest);
            }
            bool everyoneGone = true;
            for (const Slot &s : m->slots)
                everyoneGone = everyoneGone && s.gone;
            if (everyoneGone)
            {
                const int li = m->lobby;
                log("lobby %s: match over; lobby waiting again with %zu member(s)", l.code.c_str(), l.members.size());
                l.match.reset();
                if (!closeIfEmpty(li))
                    sendMembers(li);   // tells the members (each one's launcher) the lobby is waiting again
            }
        }
    }

    void Server::sendInputs(Match &m, int si)
    {
        Slot &s = m.slots[static_cast<size_t>(si)];
        proto::Inputs out;
        out.ackOwn = m.inputs.nextMissing(si);
        for (size_t q = 0; q < m.slots.size(); ++q)
        {
            if (static_cast<int>(q) == si)
                continue;
            const uint32_t from = s.sendFrom(q), to = std::min(m.inputs.nextMissing(static_cast<int>(q)), from + static_cast<uint32_t>(proto::kMaxRun));
            if (from >= to)
                continue;
            s.sent[q] = to;
            proto::InputRun r;
            r.player = static_cast<uint8_t>(q);
            r.first = from;
            for (uint32_t f = from; f < to; ++f)
                r.states.push_back(m.inputs.get(static_cast<int>(q), f));
            out.runs.push_back(std::move(r));
        }
        send(s.peer, proto::kInputs, proto::encode(out), false);
        s.lastAckOwn = out.ackOwn;
        s.lastSend = Clock::now();
        s.dirty = false;
    }

    void Server::checkHashes(Match &m)
    {
        for (auto it = m.hashes.begin(); it != m.hashes.end();)
        {
            const uint32_t frame = it->first;
            auto &v = it->second;
            v.resize(m.slots.size());
            bool complete = true;
            const Match::SlotHash *first = nullptr;
            std::string firstName;
            std::vector<std::string> differ;
            std::vector<size_t> blocks;   // 128 KB blocks whose hash differs from the first player's, in any differing player
            for (size_t i = 0; i < v.size(); ++i)
            {
                if (m.slots[i].gone || m.inputs.dropped(static_cast<int>(i), frame))
                    continue;
                if (!v[i])
                {
                    complete = false;
                    break;
                }
                if (!first)
                    first = &*v[i], firstName = m.slots[i].name;
                else if (v[i]->hash != first->hash)
                {
                    differ.push_back(m.slots[i].name);
                    for (size_t k = 0; k < v[i]->blocks.size() && k < first->blocks.size(); ++k)
                        if (v[i]->blocks[k] != first->blocks[k] && std::find(blocks.begin(), blocks.end(), k) == blocks.end())
                            blocks.push_back(k);
                }
            }
            if (!complete)
            {
                ++it;
                continue;
            }
            if (!differ.empty() && !m.desynced)
            {
                m.desynced = true;
                std::sort(blocks.begin(), blocks.end());
                std::string detail = "state differs on frame " + std::to_string(frame) + ": " + firstName + " vs " + join(differ, ",");
                // the differing 128 KB blocks (index and guest address: python tools/statediff.py --blocks <list> names the symbols);
                // the players see the first few, the server log all of them
                std::string shown, all;
                for (size_t k = 0; k < blocks.size(); ++k)
                {
                    char b[48];
                    std::snprintf(b, sizeof b, "%zu (0x%07zx)", blocks[k], blocks[k] * (32u * 1024u * 1024u / 256u));
                    all += (k ? ", " : "") + std::string(b);
                    if (k < 4)
                        shown += (k ? ", " : "") + std::string(b);
                    else if (k == 4)
                        shown += " +" + std::to_string(blocks.size() - 4) + " more";
                }
                log("lobby %s: DESYNC %s%s%s", lobbies_[static_cast<size_t>(m.lobby)].code.c_str(), detail.c_str(), blocks.empty() ? "" : "; blocks ",
                    all.c_str());
                if (!blocks.empty())
                    detail += "; blocks " + shown;
                for (Slot &s : m.slots)
                    if (s.peer && !s.gone)
                        send(s.peer, proto::kControl, proto::encode(proto::Desync{frame, detail}), true);
            }
            it = m.hashes.erase(it);
        }
    }
}
