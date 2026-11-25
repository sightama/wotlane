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
#include <random>

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

    namespace
    {
        diag::Limits diagLimits(const ServerConfig &c)
        {
            diag::Limits l;
            l.keepDays = c.diagKeepDays;
            l.maxTotalBytes = c.diagMaxTotalBytes;
            l.maxFileBytes = c.diagMaxFileBytes;
            return l;
        }

        bool validLobbyCode(const std::string &code)
        {
            return code.size() == 4u && code.find_first_not_of(proto::kCodeLetters) == std::string::npos;
        }
    }

    Server::Server(ServerConfig c) : cfg_(c), archive_(c.diagDir, diagLimits(c)), rng_(std::random_device{}())
    {
        lobbies_.resize(static_cast<size_t>(std::max(1, c.lobbies)));
    }

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
        if (archive_.enabled())
        {
            archive_.prune();
            lastPrune_ = Clock::now();
            log("diagnostics: one file per match in %s (kept %d days, at most %llu MB in all)", cfg_.diagDir.c_str(), cfg_.diagKeepDays,
                static_cast<unsigned long long>(cfg_.diagMaxTotalBytes >> 20));
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
        // the wait is for any socket, not only ENet's: a browser's pong or input is read as it arrives, not after the poll's wait
        std::vector<SocketHandle> sockets{host_->socket, listener_.handle(), wsListener_.handle()};
        for (const std::unique_ptr<Conn> &c : conns_)
            if (c)
                sockets.push_back(c->link->handle());
        for (const std::unique_ptr<WsLink> &l : wsPending_)
            sockets.push_back(l->handle());
        for (const std::unique_ptr<WsPeer> &p : wsPeers_)
            sockets.push_back(p->link->handle());
        waitAnyReadable(sockets, ms);
        acceptAndReadLobby();
        serviceWebSockets();
        serviceEnet(0);
        tickMatches();
        tickLobbyPings();
        expireGhosts();
        enet_host_flush(host_);   // what those queued for ENet (the relays) goes out now, not after the next poll's wait
        updateCounts();
        if (archive_.enabled() && Clock::now() - lastPrune_ >= std::chrono::hours(1))
        {
            archive_.prune();
            lastPrune_ = Clock::now();
        }
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
        std::string peerIp;
        while (listener_.accept(s, &peerIp))
            addConn(std::make_unique<LineSocket>(std::move(s)), false, proto::clientAddress(peerIp, ""));
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
                dropMember(static_cast<int>(i));
                conns_[i].reset();
            }
        }
    }

    void Server::addConn(std::unique_ptr<LineLink> link, bool halfRate, std::string address)
    {
        auto c = std::make_unique<Conn>();
        c->address = std::move(address);
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
            c.rejoin = proto::keyValues(w)["rejoin"] == "1";                // [rejoin=1]: TOKEN on join, a ghost when it drops
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
                if (lobbies_[i].open && !(lobbies_[i].members.empty() && !lobbies_[i].ghosts.empty()))   // a lingering lobby isn't open to others
                    c.link->sendLine(lobbyLine(static_cast<int>(i)));
            c.link->sendLine("END");
        }
        else if (cmd == "CREATE" && w.size() >= 4)
            createLobby(ci, w);
        else if (cmd == "JOIN" && w.size() >= 4)
            joinLobby(ci, w);
        else if (cmd == "REJOIN" && w.size() >= 3)
            rejoinLobby(ci, w);
        else if (cmd == "SET")
            setLobby(ci, w);
        else if (cmd == "CHAT")
            chat(ci, line);
        else if (cmd == "DIAG")
            diagReport(ci, line);
        else if (cmd == "MONSTER" && w.size() >= 2)
            changeMonster(ci, w);
        else if (cmd == "KICK" && w.size() >= 2)
            kickMember(ci, w[1]);
        else if (cmd == "OWNER" && w.size() >= 2)
            handOver(ci, w[1]);
        else if (cmd == "HEALTH")
            setHealth(ci, w);
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
        l.diagFile = archive_.begin(l.code, "lobby");
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
        finishMatchIfEveryoneGone(li);
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (!l.open)
            return void(c.link->sendLine("ERROR no such lobby (check the code)"));
        if (c.lobby == li)
            return void(c.link->sendLine("ERROR you are already in this lobby"));
        if (name.empty())
            return void(c.link->sendLine("ERROR empty name (use letters, digits, _ or -)"));
        if (!l.password.empty() && password != l.password)
            return void(c.link->sendLine("ERROR wrong password (the lobby has a password)"));
        if (l.members.empty() && !l.ghosts.empty())   // a lobby waiting for its players' rematch takes only them, by REJOIN
            return void(c.link->sendLine("ERROR this lobby's players are coming back for a rematch; host or join another one"));
        if (const std::string err = admitError(l, c, name, l.members.size() + l.ghosts.size()); !err.empty())
            return void(c.link->sendLine(err));
        for (int m : l.members)
            if (conns_[static_cast<size_t>(m)]->name == name)
                return void(c.link->sendLine("ERROR name already taken in the lobby"));
        for (const Lobby::Ghost &g : l.ghosts)   // a member whose page is reloading keeps its name
            if (g.name == name)
                return void(c.link->sendLine("ERROR name already taken in the lobby"));
        std::string err;
        if (!monsterFits(l, pick, err))
            return void(c.link->sendLine("ERROR " + err));
        leaveLobby(ci);
        addMember(ci, li, name, pick);
    }

    std::string Server::admitError(Lobby &l, const Conn &c, const std::string &name, size_t seats)
    {
        const auto now = Clock::now();
        l.bans.erase(std::remove_if(l.bans.begin(), l.bans.end(), [now](const Lobby::Ban &b) { return b.until <= now; }), l.bans.end());
        for (const Lobby::Ban &b : l.bans)
            if (b.name == name || (!b.address.empty() && b.address == c.address))
                return "ERROR you were removed from this lobby; try again in " +
                       std::to_string((std::chrono::duration_cast<std::chrono::milliseconds>(b.until - now).count() + 999) / 1000) + " s";
        if (l.match)
            return "ERROR lobby is in a match; wait for it to end";
        if (static_cast<int>(seats) >= cfg_.maxPlayers)
            return "ERROR lobby is full";
        if (l.build != c.build)
            return "ERROR build differs from the lobby's (" + l.build + "); everyone needs the same wotlane";
        return {};
    }

    void Server::rejoinLobby(int ci, const std::vector<std::string> &w)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        const int li = lobbyByCode(w[1]);
        if (li < 0)
            return void(c.link->sendLine("ERROR the lobby is gone; host or join a new one"));
        if (c.lobby == li)
            return void(c.link->sendLine("ERROR you are already in this lobby"));
        finishMatchIfEveryoneGone(li);
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (!l.open)
            return void(c.link->sendLine("ERROR the lobby is gone; host or join a new one"));
        const std::string &token = w[2];
        // the place's old connection may still be open (a phone that switched networks reloads before the server sees the drop): it
        // gives the place up, as a ghost, and this connection takes it
        for (int m : l.members)
            if (!token.empty() && conns_[static_cast<size_t>(m)]->token == token)
            {
                dropMember(m);
                break;
            }
        auto g = std::find_if(l.ghosts.begin(), l.ghosts.end(), [&](const Lobby::Ghost &x) { return x.token == token; });
        if (token.empty() || g == l.ghosts.end())
            return void(c.link->sendLine("ERROR can't rejoin (that place is gone or was already used); join again with the code"));
        if (const std::string err = admitError(l, c, g->name, l.members.size()); !err.empty())
            return void(c.link->sendLine(err));
        std::string err;
        if (!monsterFits(l, g->pick, err))
            return void(c.link->sendLine("ERROR " + err));
        leaveLobby(ci);   // out of any other lobby first; this one keeps the ghost, so it stays open
        g = std::find_if(l.ghosts.begin(), l.ghosts.end(), [&](const Lobby::Ghost &x) { return x.token == token; });
        if (!l.open || g == l.ghosts.end())
            return void(c.link->sendLine("ERROR the lobby is gone; host or join a new one"));
        const Lobby::Ghost ghost = *g;
        l.ghosts.erase(g);   // a token is good once: the member gets a new one
        if (ghost.owner && l.host < 0)
            l.host = ci;
        log("lobby %s: %s is back", l.code.c_str(), ghost.name.c_str());
        addMember(ci, li, ghost.name, ghost.pick);
    }

    void Server::dropMember(int ci)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0 || !c.rejoin || c.token.empty())
            return leaveLobby(ci);
        const int li = c.lobby;
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        const bool owner = l.host == ci;
        l.ghosts.push_back({c.name, c.token, c.address, c.pick, owner, Clock::now() + std::chrono::milliseconds(cfg_.lingerMs)});
        l.members.erase(std::remove(l.members.begin(), l.members.end(), ci), l.members.end());
        if (owner)
            l.host = -1;   // kept for the owner's REJOIN
        c.lobby = -1;
        c.token.clear();
        log("lobby %s: %s's page went away; their place is kept %d s", l.code.c_str(), c.name.c_str(), cfg_.lingerMs / 1000);
        diagEvent(l, "event=ghost player=" + c.name + " owner=" + (owner ? "1" : "0"));
        capLingering(li);
        if (!closeIfEmpty(li))
            sendMembers(li);
    }

    // a lobby left with only ghosts lingers only while fewer than maxLingering others do; past that its ghosts go and it closes
    void Server::capLingering(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (l.members.empty() && !l.ghosts.empty() && !l.match && lingeringLobbies() > cfg_.maxLingering)
        {
            log("lobby %s: too many lobbies waiting for a rematch; this one closes", l.code.c_str());
            l.ghosts.clear();
        }
    }

    void Server::expireGhosts()
    {
        const auto now = Clock::now();
        for (size_t li = 0; li < lobbies_.size(); ++li)
        {
            Lobby &l = lobbies_[li];
            if (!l.open || l.ghosts.empty())
                continue;
            bool any = false;
            for (const Lobby::Ghost &g : l.ghosts)
                if (g.until <= now)
                {
                    log("lobby %s: %s didn't come back", l.code.c_str(), g.name.c_str());
                    any = true;
                }
            if (!any)
                continue;
            l.ghosts.erase(std::remove_if(l.ghosts.begin(), l.ghosts.end(), [now](const Lobby::Ghost &g) { return g.until <= now; }), l.ghosts.end());
            const bool ownerAway = std::any_of(l.ghosts.begin(), l.ghosts.end(), [](const Lobby::Ghost &g) { return g.owner; });
            if (l.host < 0 && !ownerAway && !l.members.empty())
                l.host = l.members.front();   // the owner didn't come back: the lobby passes on, as when an owner leaves
            if (!closeIfEmpty(static_cast<int>(li)))
                sendMembers(static_cast<int>(li));
        }
    }

    int Server::lingeringLobbies() const
    {
        return static_cast<int>(std::count_if(lobbies_.begin(), lobbies_.end(), [](const Lobby &l) { return l.open && l.members.empty() && !l.ghosts.empty(); }));
    }

    std::string Server::newToken()
    {
        static std::random_device rd;   // unguessable: the token is the only thing a REJOIN shows
        static const char *const hex = "0123456789abcdef";
        std::string t;
        for (int i = 0; i < 4; ++i)
            for (uint32_t v = rd(), k = 0; k < 8; ++k, v >>= 4)
                t += hex[v & 15u];
        return t;
    }

    void Server::addMember(int ci, int li, const std::string &name, const proto::Pick &pick)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        l.members.push_back(ci);
        const bool ownerAway = std::any_of(l.ghosts.begin(), l.ghosts.end(), [](const Lobby::Ghost &g) { return g.owner; });
        if (l.host < 0 && !ownerAway)   // an owner's ghost keeps the lobby until it rejoins or expires
            l.host = ci;
        c.lobby = li, c.name = name, c.pick = pick;
        c.link->sendLine("JOINED " + l.code);
        c.link->sendLine(settingsLine(l));
        log("lobby %s: %s joined (monster %s), %zu player(s)", l.code.c_str(), name.c_str(), pick.text().c_str(), l.members.size());
        diagEvent(l, "event=join player=" + name + " transport=" + (dynamic_cast<WsLink *>(c.link.get()) ? "ws" : "tcp") +
                         " half_rate=" + (c.halfRate ? "1" : "0") + " members=" + std::to_string(l.members.size()));
        sendMembers(li);
        if (c.rejoin)
        {
            c.token = newToken();
            c.link->sendLine("TOKEN " + c.token);   // REJOIN <code> <token> brings this member back after its page reloads
        }
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

    int Server::memberNamed(const Lobby &l, const std::string &name) const
    {
        for (int m : l.members)
            if (conns_[static_cast<size_t>(m)]->name == name)
                return m;
        return -1;
    }

    Server::Lobby *Server::ownedWaitingLobby(int ci, const char *notOwner)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0)
            return c.link->sendLine("ERROR join a lobby first"), nullptr;
        Lobby &l = lobbies_[static_cast<size_t>(c.lobby)];
        if (l.host != ci)
            return c.link->sendLine(std::string("ERROR ") + notOwner), nullptr;
        if (l.match)
            return c.link->sendLine("ERROR the match has started"), nullptr;
        return &l;
    }

    // KICK <name>: the member gets KICKED <code> <seconds of ban>, leaves the lobby (the others get MEMBERS) and keeps its connection
    void Server::kickMember(int ci, const std::string &name)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        Lobby *l = ownedWaitingLobby(ci, "only the lobby owner can remove a member");
        if (!l)
            return;
        const int target = memberNamed(*l, name);
        if (target == ci)
            return void(c.link->sendLine("ERROR you can't remove yourself; leave instead"));
        const auto now = Clock::now();
        const auto ghost = std::find_if(l->ghosts.begin(), l->ghosts.end(), [&](const Lobby::Ghost &g) { return g.name == name; });
        if (target < 0 && ghost != l->ghosts.end())
        {
            // a member whose page is reloading: its place and token go, and it can't join again for the ban, as a live member
            l->bans.push_back({name, ghost->address, now + std::chrono::milliseconds(cfg_.kickBanMs)});
            l->ghosts.erase(ghost);
            log("lobby %s: %s removed %s (away)", l->code.c_str(), c.name.c_str(), name.c_str());
            diagEvent(*l, "event=kick owner=" + c.name + " player=" + name);
            return;
        }
        if (target < 0)
            return void(c.link->sendLine("ERROR " + name + " is not in this lobby"));
        l->bans.erase(std::remove_if(l->bans.begin(), l->bans.end(), [now](const Lobby::Ban &b) { return b.until <= now; }), l->bans.end());
        l->bans.push_back({name, conns_[static_cast<size_t>(target)]->address, now + std::chrono::milliseconds(cfg_.kickBanMs)});
        log("lobby %s: %s removed %s", l->code.c_str(), c.name.c_str(), name.c_str());
        diagEvent(*l, "event=kick owner=" + c.name + " player=" + name);
        conns_[static_cast<size_t>(target)]->link->sendLine("KICKED " + l->code + " " + std::to_string((cfg_.kickBanMs + 999) / 1000));
        leaveLobby(target);
    }

    // OWNER <name>: the lobby's settings and START go to that member; MEMBERS tells everyone
    void Server::handOver(int ci, const std::string &name)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        Lobby *l = ownedWaitingLobby(ci, "only the lobby owner can hand the lobby to someone else");
        if (!l)
            return;
        const int target = memberNamed(*l, name);
        if (target == ci)
            return void(c.link->sendLine("ERROR you are the owner already"));
        if (target < 0)
            return void(c.link->sendLine("ERROR " + name + " is not in this lobby"));
        l->host = target;
        log("lobby %s: %s handed the lobby to %s", l->code.c_str(), c.name.c_str(), name.c_str());
        diagEvent(*l, "event=owner from=" + c.name + " to=" + name);
        sendMembers(c.lobby);
    }

    void Server::setHealth(int ci, const std::vector<std::string> &w)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        if (c.lobby < 0)
            return void(c.link->sendLine("ERROR join a lobby first"));
        const auto number = [&w](size_t i, double max, double &out) {
            if (i >= w.size())
                return false;
            char *end = nullptr;
            out = std::strtod(w[i].c_str(), &end);
            return *end == 0 && std::isfinite(out) && out >= 0 && out <= max;
        };
        // "HEALTH sw" alone: drawing in software, not measured yet; else the numbers and an optional "sw"
        if (w.size() == 2 && w[1] == "sw")
            return void(c.softwareGl = true);
        double fps = 0, workMs = 0, hz = 0;
        const bool sw = w.size() == 5 && w[4] == "sw";
        if ((w.size() != 4 && !sw) || !number(1, 1000, fps) || !number(2, 1000, workMs) || !number(3, 240, hz) || hz < 1 || hz != std::floor(hz))
            return void(c.link->sendLine("ERROR HEALTH <own fps> <work ms> <hz>: numbers in range"));
        c.hasHealth = true, c.healthFps = fps, c.healthWorkMs = workMs, c.healthHz = static_cast<int>(hz);
        c.softwareGl = sw;
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
        c.token.clear();   // leaving on purpose gives the rejoin up
        c.hasHealth = false;
        c.softwareGl = false;
        if (l.host == ci)
            l.host = l.members.empty() ? -1 : l.members.front();
        log("lobby %s: %s left", l.code.c_str(), c.name.c_str());
        capLingering(li);
        if (!closeIfEmpty(li))
            sendMembers(li);
    }

    // a lobby with no members and no match closes (drops off the list and frees the slot); returns whether it closed
    bool Server::closeIfEmpty(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (!l.open || !l.members.empty() || !l.ghosts.empty() || l.match)
            return false;
        log("lobby %s: empty, closed", l.code.c_str());
        l.open = false;
        l.code.clear(), l.name.clear(), l.password.clear(), l.build.clear(), l.diagFile.clear();
        l.bans.clear();
        l.ghosts.clear();
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
            v.clear();
            for (int m : l.members)
            {
                const Conn &c = *conns_[static_cast<size_t>(m)];
                char buf[64] = ":-:-:-";
                if (c.hasHealth)
                    std::snprintf(buf, sizeof buf, ":%.1f:%.1f:%d", c.healthFps, c.healthWorkMs, c.healthHz);
                if (c.hasHealth || c.softwareGl)
                    v.push_back(c.name + buf + (c.softwareGl ? ":sw" : ""));
            }
            if (!v.empty())
                for (int m : l.members)
                    conns_[static_cast<size_t>(m)]->link->sendLine("HEALTH " + l.code + " " + join(v, ","));
        }
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
        if (!proto::validUtf8(text))
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

    // ---- diagnostics ----

    void Server::diagLine(const std::string &file, const std::string &line, bool fromServer)
    {
        if (cfg_.log)
        {
            std::printf("[diag] %s\n", line.c_str());
            std::fflush(stdout);
        }
        archive_.append(file, diag::utcNow() + " " + line, fromServer);
    }

    void Server::diagEvent(const Lobby &l, const std::string &text)
    {
        diagLine(l.diagFile, "src=server lobby=" + l.code + " " + text);
    }

    // The player and lobby come from the connection when it is in a lobby (src=client). A page back on its front panel after a match has
    // no lobby connection, so its report says which lobby and who it was; nothing proves that, so such a report goes to the day's
    // reports file as a claim (src=client_outside claimed_lobby= claimed_player=) and never into a lobby's own file. The keys the
    // server's own lines use can't be repeated by a report
    void Server::diagReport(int ci, const std::string &line)
    {
        Conn &c = *conns_[static_cast<size_t>(ci)];
        const auto now = Clock::now();
        if (now - c.diagWindow >= std::chrono::milliseconds(proto::kDiagWindowMs))
        {
            c.diagWindow = now;
            c.diagLines = 0;
        }
        if (++c.diagLines > proto::kDiagBurst)
            return;
        diag::Report r;
        if (!diag::parse(proto::words(line), r))
            return;
        const std::string claimedLobby = r.get("lobby"), claimedName = r.get("name");
        static const char *const kServerKeys[] = {"src", "lobby", "name", "player", "event", "slot", "transport", "claimed_lobby", "claimed_player"};
        r.fields.erase(std::remove_if(r.fields.begin(), r.fields.end(),
                                      [](const auto &f) { return std::find(std::begin(kServerKeys), std::end(kServerKeys), f.first) != std::end(kServerKeys); }),
                       r.fields.end());
        if (c.lobby >= 0)
        {
            const Lobby &l = lobbies_[static_cast<size_t>(c.lobby)];
            diagLine(l.diagFile, "src=client lobby=" + l.code + " player=" + c.name + " " + diag::format(r), false);
            return;
        }
        const std::string code = validLobbyCode(claimedLobby) ? claimedLobby : "-";
        const std::string name = proto::cleanName(claimedName).empty() ? "-" : proto::cleanName(claimedName);
        diagLine(archive_.reportsFile(), "src=client_outside claimed_lobby=" + code + " claimed_player=" + name + " " + diag::format(r), false);
    }

    // For each player of a started match, every 100 ms: how many frames their inputs are behind the player furthest ahead. A player
    // whose inputs arrive late holds everyone else's frames back by this much
    void Server::sampleBehind(Match &m)
    {
        const Clock::time_point now = Clock::now();
        if (now - m.lastBehind < std::chrono::milliseconds(100))
            return;
        m.lastBehind = now;
        uint32_t lead = 0;
        for (size_t i = 0; i < m.slots.size(); ++i)
            if (!m.slots[i].gone)
                lead = std::max(lead, m.inputs.nextMissing(static_cast<int>(i)));
        for (size_t i = 0; i < m.slots.size(); ++i)
        {
            Slot &s = m.slots[i];
            if (s.gone || !s.peer)
                continue;
            const uint32_t behind = lead - m.inputs.nextMissing(static_cast<int>(i));
            s.window.behindMax = std::max(s.window.behindMax, behind);
            s.window.behindSum += behind;
            ++s.window.behindSamples;
        }
    }

    // Every 100 ms of a started match: the match frame, how far each member's inputs trail and who is away go to the Holdback; a member
    // it names hears why (Reject: the page stops with the reason) and is dropped like one who left, so the others play on
    void Server::holdbackTick(Match &m, const Lobby &l)
    {
        const Clock::time_point now = Clock::now();
        if (now - m.lastHoldback < std::chrono::milliseconds(100))
            return;
        m.lastHoldback = now;
        uint32_t lead = 0, frame = 0xFFFFFFFFu;
        for (size_t i = 0; i < m.slots.size(); ++i)
            if (!m.slots[i].gone)
            {
                lead = std::max(lead, m.inputs.nextMissing(static_cast<int>(i)));
                frame = std::min(frame, m.inputs.nextMissing(static_cast<int>(i)));
            }
        // each member's trail, less what its longer path to the server alone accounts for
        uint32_t minRtt = 0xFFFFFFFFu;
        for (const Slot &s : m.slots)
            if (!s.gone && s.peer)
                minRtt = std::min(minRtt, rttHighOf(s.peer));
        std::vector<Holdback::Member> members;
        for (size_t i = 0; i < m.slots.size(); ++i)
        {
            const Slot &s = m.slots[i];
            const bool active = !s.gone && s.peer;
            uint32_t behind = 0;
            if (active)
            {
                behind = lead - m.inputs.nextMissing(static_cast<int>(i));
                const uint32_t path = Holdback::latencyFrames(rttHighOf(s.peer), minRtt, proto::frameMs(m.step));
                behind = behind > path ? behind - path : 0u;
            }
            members.push_back({behind, s.away, active});
        }
        const double t = std::chrono::duration<double>(now.time_since_epoch()).count();
        const int hz = 60 / std::max(1, m.step);
        const Holdback::Decision d = m.holdback.update(t, frame == 0xFFFFFFFFu ? 0u : frame, hz, members);
        if (d.slot < 0)
            return;
        Slot &s = m.slots[static_cast<size_t>(d.slot)];
        char why[200];
        if (d.reason == Holdback::Reason::Away)
            std::snprintf(why, sizeof why, "you were taken out of the match: your game was in the background for over %.0f s, which stops "
                          "everyone (the others play on)", Holdback::kAwaySec);
        else
            std::snprintf(why, sizeof why, "you were taken out of the match: your device or connection could not keep up (the match ran at "
                          "%.0f of %d frames a second; the others play on)", d.pace, hz);
        send(s.peer, proto::kControl, proto::encode(proto::Reject{why}), true);
        const char *reason = d.reason == Holdback::Reason::Away ? "away" : "slow";
        log("lobby %s: %s held the match back (%s, pace %.1f of %d frames/s): taken out", l.code.c_str(), s.name.c_str(), reason, d.pace, hz);
        char ev[160];
        std::snprintf(ev, sizeof ev, " reason=%s pace=%.1f hz=%d", reason, d.pace, hz);
        diagEvent(l, "event=dropped player=" + s.name + ev);
        dropSlot(m, d.slot);
    }

    void Server::diagTick(Match &m, const Lobby &l)
    {
        if (cfg_.diagEveryMs <= 0)
            return;
        sampleBehind(m);
        const Clock::time_point now = Clock::now();
        if (now - m.lastDiag < std::chrono::milliseconds(cfg_.diagEveryMs))
            return;
        m.lastDiag = now;
        for (size_t i = 0; i < m.slots.size(); ++i)
        {
            Slot &s = m.slots[i];
            Slot::InputWindow w = s.window;
            s.window = {};
            if (s.gone || !s.peer)
                continue;
            const uint32_t rtt = (s.peer.enet ? s.peer.enet->roundTripTime : s.peer.ws->link->rttMs()) + testRttMs_.load();
            char stats[200];
            std::snprintf(stats, sizeof stats, " rtt_ms=%u rtt_high_ms=%u rtt_level_ms=%u in_pkts=%u in_gap_avg_ms=%.0f in_gap_max_ms=%.0f behind_max=%u behind_avg=%.1f away=%d", rtt,
                          rttHighOf(s.peer), s.rtt.riseLevel(), w.packets, w.packets ? w.gapSumMs / w.packets : 0.0, w.gapMaxMs, w.behindMax,
                          w.behindSamples ? w.behindSum / w.behindSamples : 0.0, s.away ? 1 : 0);
            diagEvent(l, "player=" + s.name + " slot=" + std::to_string(i) + " transport=" + (s.peer.ws ? "ws" : "enet") + " hz=" +
                             std::to_string(60 / m.step) + " delay=" + std::to_string(s.advised) + stats);
        }
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
        l.diagFile = archive_.begin(l.code, "match");
        diagEvent(l, "event=match_start players=" + std::to_string(players) + " hz=" + std::to_string(60 / step) + " half_rate_member=" +
                         (halfRate ? "1" : "0") + " stage=" + l.stage + " wins=" + std::to_string(l.wins) + " mods=" + (l.mods.empty() ? "none" : l.mods));
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

    uint32_t Server::rttOf(PeerRef peer) const
    {
        return (peer.enet ? peer.enet->roundTripTime : peer.ws->link->rttMs()) + testRttMs_.load();
    }

    // each player's round trip as the delay sees it for a rise, or with `fall` for a fall (RttWindow; 0 for a player who left: nobody
    // waits for them)
    std::vector<uint32_t> Server::rttLevels(Match &m, bool fall)
    {
        std::vector<uint32_t> out;
        for (Slot &s : m.slots)
            out.push_back(s.peer && !s.gone ? (fall ? s.rtt.fallLevel() : s.rtt.riseLevel()) : 0u);
        return out;
    }

    // During the match, every paceMs: tells each player their current round trip (Rtt), and once every active player has played
    // through the 1-second warmup window past stage load, samples each player's round trip into RttWindow and sends a player whose
    // delay should change its new one (Pace). Holding RttWindow until warmup finishes keeps boot/shader-compile main-thread stalls
    // from poisoning delay at match start.
    void Server::pace(Match &m, const Lobby &l)
    {
        const Clock::time_point now = Clock::now();
        if (cfg_.forcedDelay || now - m.lastPace < std::chrono::milliseconds(cfg_.paceMs))
            return;
        m.lastPace = now;
        for (Slot &s : m.slots)
            if (s.peer && !s.gone)
                send(s.peer, proto::kControl, proto::encode(proto::Rtt{rttOf(s.peer)}), true);
        if (!m.paceWarmedUp)
        {
            const uint32_t warmupFrames = static_cast<uint32_t>(60 / std::max(1, m.step));
            const uint32_t readyFrame = m.delay + warmupFrames;
            bool anyActive = false, allReady = true;
            for (size_t i = 0; i < m.slots.size(); ++i)
            {
                if (!m.slots[i].peer || m.slots[i].gone)
                    continue;
                anyActive = true;
                if (m.inputs.nextMissing(static_cast<int>(i)) < readyFrame)
                {
                    allReady = false;
                    break;
                }
            }
            if (!anyActive || !allReady)
                return;
            m.paceWarmedUp = true;
        }
        for (Slot &s : m.slots)
            if (s.peer && !s.gone)
                s.rtt.add(rttHighOf(s.peer));
        const std::vector<uint32_t> levels = rttLevels(m), fallLevels = rttLevels(m, true);
        const double frame = proto::frameMs(m.step);
        for (size_t i = 0; i < m.slots.size(); ++i)
        {
            Slot &s = m.slots[i];
            const uint32_t d = proto::adviseDelay(s.advised, levels, fallLevels, i, frame);
            if (!s.peer || s.gone || d == s.advised)
                continue;
            log("lobby %s: %s's delay %u -> %u frame(s) (round trip the delay covers: %u ms)", l.code.c_str(), s.name.c_str(), s.advised, d, levels[i]);
            diagEvent(l, "event=delay player=" + s.name + " from=" + std::to_string(s.advised) + " to=" + std::to_string(d) +
                             " rtt_level_ms=" + std::to_string(levels[i]));
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
            const Clock::time_point arrived = Clock::now();
            if (s.lastInput != Clock::time_point{})
            {
                const double gap = std::chrono::duration<double, std::milli>(arrived - s.lastInput).count();
                s.window.gapSumMs += gap;
                s.window.gapMaxMs = std::max(s.window.gapMaxMs, gap);
            }
            s.lastInput = arrived;
            ++s.window.packets;
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
            diagEvent(lobbies_[static_cast<size_t>(m->lobby)], "event=away player=" + m->slots[static_cast<size_t>(si)].name + " away=" + (a.away ? "1" : "0"));
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
        std::string peerIp;
        while (wsListener_.accept(s, &peerIp))
        {
            wsPending_.push_back(std::make_unique<WsLink>(std::move(s)));
            wsPending_.back()->setPeerIp(peerIp);
        }
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
                const std::string address = proto::clientAddress(link->peerIp(), link->forwardedAddress());
                addConn(std::move(link), phone, address);   // lines that arrived with the handshake wait in the link for acceptAndReadLobby
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
        diagEvent(lobbies_[static_cast<size_t>(m.lobby)], "event=left player=" + s.name + " frame=" + std::to_string(frame));
        checkHashes(m);
    }

    bool Server::finishMatchIfEveryoneGone(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (!l.match)
            return false;
        for (const Slot &s : l.match->slots)
            if (!s.gone)
                return false;
        log("lobby %s: match over; lobby waiting again with %zu member(s)", l.code.c_str(), l.members.size());
        diagEvent(l, "event=match_over members=" + std::to_string(l.members.size()));
        l.match.reset();
        if (!closeIfEmpty(li))
            sendMembers(li);   // tells the members (each one's launcher) the lobby is waiting again
        return true;
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
                    std::vector<uint32_t> rtt;
                    for (Slot &s : m->slots)
                    {
                        rtt.push_back(s.peer && !s.gone ? rttHighOf(s.peer) : 0u);
                        st.names.push_back(s.name);
                    }
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
                    diagEvent(l, "event=start delay=" + std::to_string(st.delay) + " rtt_level_ms=" + (pings.empty() ? "-" : pings));
                    m->lastDiag = m->lastBehind = now;
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
                diagTick(*m, l);
                holdbackTick(*m, l);
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
            finishMatchIfEveryoneGone(m->lobby);
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
                diagEvent(lobbies_[static_cast<size_t>(m.lobby)], "event=desync frame=" + std::to_string(frame) + " differ=" + join(differ, ",") +
                                                                   " blocks=" + std::to_string(blocks.size()));
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
