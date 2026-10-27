// ABOUTME: Implementacao do servidor do netcode: lobby TCP (linhas de texto) e partidas ENet em lockstep (repasse das entradas,
// ABOUTME: atraso pelo ping, hashes de estado, quedas). Nenhum codigo do jogo; roda no wotm_server e nos testes.
#include "wotm_net_server.hpp"
#include "wotm_monsters.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace wotm_net
{
    namespace
    {
        // peer->data guarda (lobby, vaga) da partida: 1 + lobby * 64 + vaga (0 = ainda sem ficha)
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
        std::printf("[servidor] ");
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
            err = "nao consegui escutar TCP na porta " + std::to_string(cfg_.port);
            return false;
        }
        if (enet_initialize() != 0)
        {
            err = "enet_initialize falhou";
            return false;
        }
        enetUp_ = true;
        ENetAddress a;
        a.host = ENET_HOST_ANY;
        a.port = cfg_.port;
        host_ = enet_host_create(&a, static_cast<size_t>(cfg_.lobbies * cfg_.maxPlayers + 16), proto::kChannels, 0, 0);
        if (!host_)
        {
            err = "nao consegui escutar UDP na porta " + std::to_string(cfg_.port);
            return false;
        }
        log("escutando na porta %u (TCP lobby, UDP partida), %d lobbies de ate %d jogadores", cfg_.port, cfg_.lobbies, cfg_.maxPlayers);
        return true;
    }

    void Server::poll(int ms)
    {
        if (!host_)   // start falhou ou nao foi chamado
            return;
        acceptAndReadLobby();
        serviceEnet(ms);
        tickMatches();
    }

    // ---- lobby ----

    void Server::acceptAndReadLobby()
    {
        LineSocket s;
        while (listener_.accept(s))
        {
            auto c = std::make_unique<Conn>();
            c->socket = std::move(s);
            auto free = std::find(conns_.begin(), conns_.end(), nullptr);
            if (free != conns_.end())
                *free = std::move(c);
            else
                conns_.push_back(std::move(c));
        }
        for (size_t i = 0; i < conns_.size(); ++i)
        {
            if (!conns_[i])
                continue;
            const bool alive = conns_[i]->socket.pump();
            std::string line;
            while (conns_[i] && conns_[i]->socket.nextLine(line))
                handleLine(static_cast<int>(i), line);
            if (conns_[i] && (!alive || !conns_[i]->socket.open()))
            {
                leaveLobby(static_cast<int>(i));
                conns_[i].reset();
            }
        }
    }

    std::string Server::lobbyLine(int i) const
    {
        const Lobby &l = lobbies_[static_cast<size_t>(i)];
        std::vector<std::string> names;
        int count = 0;
        if (l.match)
            for (const Slot &s : l.match->slots)
                names.push_back(s.name), ++count;
        else
            for (int m : l.members)
                names.push_back(conns_[static_cast<size_t>(m)]->name), ++count;
        return "LOBBY " + std::to_string(i) + (l.match ? " playing " : " waiting ") + std::to_string(count) + "/" +
               std::to_string(cfg_.maxPlayers) + " " + (names.empty() ? "-" : join(names, ","));
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
                c.socket.sendLine("ERROR protocolo diferente (o servidor fala o " + std::to_string(proto::kProtocol) + "; atualize o wotlane)");
                return;
            }
            c.hello = true;
            c.build = w[2];
            c.socket.sendLine("WELCOME " + std::to_string(lobbies_.size()));
            return;
        }
        if (!c.hello)
        {
            c.socket.sendLine("ERROR mande HELLO primeiro");
            return;
        }
        if (cmd == "LIST")
        {
            for (size_t i = 0; i < lobbies_.size(); ++i)
                c.socket.sendLine(lobbyLine(static_cast<int>(i)));
            c.socket.sendLine("END");
        }
        else if (cmd == "JOIN" && w.size() >= 4)
        {
            const int li = std::atoi(w[1].c_str());
            const std::string name = proto::cleanName(w[2]);
            const int monster = std::atoi(w[3].c_str());
            if (li < 0 || li >= static_cast<int>(lobbies_.size()))
                return void(c.socket.sendLine("ERROR lobby inexistente"));
            if (name.empty())
                return void(c.socket.sendLine("ERROR nome vazio (use letras, digitos, _ ou -)"));
            if (wotm_monsters::costumes(monster) == 0)
                return void(c.socket.sendLine("ERROR monstro invalido (sem arquivos no disco)"));
            leaveLobby(ci);
            Lobby &l = lobbies_[static_cast<size_t>(li)];
            int list[wotm_monsters::kMaxList], n = 0;   // trajes como a partida os da (wotm_monsters::fits)
            for (int m : l.members)
                if (n < wotm_monsters::kMaxList - 1)
                    list[n++] = conns_[static_cast<size_t>(m)]->monster;
            list[n++] = monster;
            if (!wotm_monsters::fits(list, nullptr, n))
                return void(c.socket.sendLine("ERROR monstro sem traje livre no lobby (todos os trajes dele ja escolhidos)"));
            if (l.match)
                return void(c.socket.sendLine("ERROR lobby em partida; espere acabar"));
            if (static_cast<int>(l.members.size()) >= cfg_.maxPlayers)
                return void(c.socket.sendLine("ERROR lobby cheio"));
            if (!l.members.empty() && l.build != c.build)
                return void(c.socket.sendLine("ERROR build diferente do lobby (" + l.build + "); todos precisam do mesmo wotlane"));
            for (int m : l.members)
                if (conns_[static_cast<size_t>(m)]->name == name)
                    return void(c.socket.sendLine("ERROR nome ja usado no lobby"));
            if (l.members.empty())
                l.build = c.build;
            l.members.push_back(ci);
            c.lobby = li, c.name = name, c.monster = monster;
            c.socket.sendLine("JOINED " + std::to_string(li));
            log("lobby %d: %s entrou (monstro %d), %zu jogador(es)", li, name.c_str(), monster, l.members.size());
            sendMembers(li);
        }
        else if (cmd == "LEAVE")
            leaveLobby(ci);
        else if (cmd == "START")
        {
            if (c.lobby < 0)
                return void(c.socket.sendLine("ERROR entre num lobby primeiro"));
            startMatch(c.lobby, w.size() >= 2 ? std::atoi(w[1].c_str()) : 1);
        }
        else
            c.socket.sendLine("ERROR comando desconhecido: " + cmd);
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
        log("lobby %d: %s saiu", li, c.name.c_str());
        sendMembers(li);
    }

    void Server::sendMembers(int li)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        std::vector<std::string> v;
        for (int m : l.members)
            v.push_back(conns_[static_cast<size_t>(m)]->name + ":" + std::to_string(conns_[static_cast<size_t>(m)]->monster));
        const std::string line = "MEMBERS " + std::to_string(li) + " " + (v.empty() ? "-" : join(v, ","));
        for (int m : l.members)
            conns_[static_cast<size_t>(m)]->socket.sendLine(line);
    }

    void Server::startMatch(int li, int stage)
    {
        Lobby &l = lobbies_[static_cast<size_t>(li)];
        if (l.match || l.members.empty())
            return;
        auto m = std::make_unique<Match>();
        m->lobby = li;
        m->created = Clock::now();
        const int players = static_cast<int>(l.members.size());
        const uint32_t seed = static_cast<uint32_t>(rng_() % 0x7FFFFFFEull) + 1u;
        const int step = players >= 3 ? 2 : 1;   // 3+ jogadores: o jogo entra no proprio modo de 30 Hz
        std::vector<std::string> monsters, names;
        for (int ci : l.members)
        {
            const Conn &c = *conns_[static_cast<size_t>(ci)];
            monsters.push_back(std::to_string(c.monster));
            names.push_back(c.name);
            Slot s;
            s.token = hex64(rng_());
            s.name = c.name;
            s.need.assign(static_cast<size_t>(players), 0u);
            m->slots.push_back(std::move(s));
        }
        for (int i = 0; i < players; ++i)
        {
            Conn &c = *conns_[static_cast<size_t>(l.members[static_cast<size_t>(i)])];
            c.socket.sendLine("MATCH lobby=" + std::to_string(li) + " players=" + std::to_string(players) + " stage=" + std::to_string(stage) +
                              " monsters=" + join(monsters, ",") + " names=" + join(names, ",") + " seed=" + std::to_string(seed) +
                              " step=" + std::to_string(step) + " you=" + std::to_string(i) + " token=" + m->slots[static_cast<size_t>(i)].token +
                              " port=" + std::to_string(cfg_.port));
            c.lobby = -1;
        }
        log("lobby %d: partida com %d jogador(es) (%s), fase %d, semente %u", li, players, join(names, ",").c_str(), stage, seed);
        l.members.clear();
        l.match = std::move(m);
    }

    // ---- partida ----

    void Server::send(ENetPeer *peer, uint8_t channel, const std::vector<uint8_t> &data, bool reliable)
    {
        if (!peer)
            return;
        if (!reliable && cfg_.dropInputPercent > 0 && static_cast<int>(rng_() % 100u) < cfg_.dropInputPercent)
            return;
        ENetPacket *p = enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(peer, channel, p);
    }

    Server::Match *Server::matchOf(ENetPeer *peer, int &slot)
    {
        const uintptr_t tag = reinterpret_cast<uintptr_t>(peer->data);
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
                onReceive(ev.peer, data);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                onDisconnect(ev.peer);
                break;
            default:
                break;
            }
        }
    }

    void Server::onReceive(ENetPeer *peer, const std::vector<uint8_t> &data)
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
                s.dirty = true;   // confirma de novo (o pacote pode ter sido repetido porque a confirmacao se perdeu)
            break;
        }
        case proto::Type::Hash:
        {
            proto::Hash h;
            if (!m || !m->started || !proto::decode(data, h))
                break;
            auto &v = m->hashes[h.frame];
            v.resize(m->slots.size());
            v[static_cast<size_t>(si)] = h.hash;
            checkHashes(*m);
            break;
        }
        default:
            break;
        }
    }

    void Server::onJoin(ENetPeer *peer, const proto::Join &j)
    {
        auto reject = [&](const std::string &why) {
            send(peer, proto::kControl, proto::encode(proto::Reject{why}), true);
            enet_peer_disconnect_later(peer, 0);
            log("partida: recusado (%s)", why.c_str());
        };
        if (j.protocol != proto::kProtocol)
            return reject("protocolo diferente");
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
                    return reject("ficha ja usada");
                if (lobbies_[li].build != j.buildId && !lobbies_[li].build.empty())
                    return reject("build diferente (" + j.buildId + " contra " + lobbies_[li].build + ")");
                if (!m->config.empty() && m->config != j.configHash)
                    return reject("configuracao do jogo diferente dos outros (" + j.configHash + " contra " + m->config + ")");
                if (m->config.empty())
                    m->config = j.configHash;
                s.joined = true;
                s.peer = peer;
                peer->data = peerTag(static_cast<int>(li), static_cast<int>(si));
                send(peer, proto::kControl, proto::encode(proto::Welcome{static_cast<uint8_t>(si), static_cast<uint8_t>(m->slots.size())}), true);
                log("lobby %zu: %s chegou a partida (jogador %zu)", li, s.name.c_str(), si + 1);
                return;
            }
        }
        reject("ficha desconhecida (a partida acabou ou e de outro servidor)");
    }

    void Server::onDisconnect(ENetPeer *peer)
    {
        int si = 0;
        if (Match *m = matchOf(peer, si))
        {
            m->slots[static_cast<size_t>(si)].peer = nullptr;
            dropSlot(*m, si);
        }
        peer->data = nullptr;
    }

    // jogador fora: neutro a partir do primeiro quadro que o servidor nao tem dele (todos recebem os anteriores)
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
        log("lobby %d: %s saiu da partida (neutro desde o quadro %u)", m.lobby, s.name.c_str(), frame);
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
                    std::vector<uint32_t> rtt;
                    proto::Start st;
                    for (Slot &s : m->slots)
                    {
                        if (s.peer && !s.gone)
                            rtt.push_back(s.peer->roundTripTime);
                        st.names.push_back(s.name);
                    }
                    const uint32_t frameMs = m->slots.size() >= 3 ? 33u : 16u;
                    st.delay = cfg_.forcedDelay ? cfg_.forcedDelay : proto::pickDelay(rtt, frameMs);
                    m->delay = st.delay;
                    m->inputs = InputTimeline(static_cast<int>(m->slots.size()), st.delay);
                    for (size_t i = 0; i < m->slots.size(); ++i)
                        if (m->slots[i].gone)
                            m->inputs.drop(static_cast<int>(i), 0);
                    m->started = true;
                    std::string pings;
                    for (uint32_t r : rtt)
                        pings += (pings.empty() ? "" : ",") + std::to_string(r);
                    log("lobby %d: inicio, atraso %u quadro(s) (ping ms: %s)", m->lobby, st.delay, pings.c_str());
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
                for (size_t i = 0; i < m->slots.size(); ++i)
                {
                    Slot &s = m->slots[i];
                    if (!s.peer || s.gone)
                        continue;
                    bool behind = m->inputs.nextMissing(static_cast<int>(i)) != s.lastAckOwn;
                    for (size_t q = 0; q < m->slots.size(); ++q)
                        behind = behind || (q != i && s.need[q] < m->inputs.nextMissing(static_cast<int>(q)));
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
                log("lobby %d: partida acabou; lobby aberto de novo", m->lobby);
                l.match.reset();
                l.build.clear();
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
            const uint32_t from = s.need[q], to = std::min(m.inputs.nextMissing(static_cast<int>(q)), from + static_cast<uint32_t>(proto::kMaxRun));
            if (from >= to)
                continue;
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
            std::optional<uint64_t> first;
            std::string firstName;
            std::vector<std::string> differ;
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
                    first = v[i], firstName = m.slots[i].name;
                else if (*v[i] != *first)
                    differ.push_back(m.slots[i].name);
            }
            if (!complete)
            {
                ++it;
                continue;
            }
            if (!differ.empty() && !m.desynced)
            {
                m.desynced = true;
                std::string detail = "estado diferente no quadro " + std::to_string(frame) + ": " + firstName + " contra " + join(differ, ",");
                log("lobby %d: DESSINCRONIA %s", m.lobby, detail.c_str());
                for (Slot &s : m.slots)
                    if (s.peer && !s.gone)
                        send(s.peer, proto::kControl, proto::encode(proto::Desync{frame, detail}), true);
            }
            it = m.hashes.erase(it);
        }
    }
}
