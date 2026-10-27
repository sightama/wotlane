// ABOUTME: Testes do netcode sem o jogo: linha do tempo das entradas (lockstep), codificacao das mensagens, servidor e cliente ENet
// ABOUTME: de verdade em localhost. Sai com 0 se tudo passar; cada falha imprime arquivo, linha e a expressao.
#include "wotm_lockstep.hpp"
#include "wotm_net_client.hpp"
#include "wotm_net_proto.hpp"
#include "wotm_net_server.hpp"
#include "wotm_net_socket.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <thread>
#include <functional>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0, g_checks = 0;

#define CHECK(expr)                                                                                                                  \
    do                                                                                                                               \
    {                                                                                                                                \
        ++g_checks;                                                                                                                  \
        if (!(expr))                                                                                                                 \
        {                                                                                                                            \
            ++g_failures;                                                                                                            \
            std::printf("  FALHOU %s:%d: %s\n", __FILE__, __LINE__, #expr);                                                          \
        }                                                                                                                            \
    } while (0)

    struct Test
    {
        const char *name;
        std::function<void()> run;
    };
    std::vector<Test> &tests()
    {
        static std::vector<Test> t;
        return t;
    }
    struct Register
    {
        Register(const char *name, std::function<void()> run) { tests().push_back({name, std::move(run)}); }
    };
#define TEST(name)                                                                                                                   \
    void name();                                                                                                                     \
    Register reg_##name(#name, name);                                                                                                \
    void name()

    using wotm_net::InputTimeline;
    using wotm_net::PadState;

    PadState pressed(uint16_t mask)
    {
        PadState s;
        s.buttons = static_cast<uint16_t>(0xFFFFu & ~mask);
        return s;
    }

    TEST(timeline_first_frames_are_neutral_for_the_delay)
    {
        InputTimeline t(2, 3);
        CHECK(t.ready(0) && t.ready(1) && t.ready(2));
        CHECK(!t.ready(3));
        CHECK(t.get(0, 2) == PadState{});
        CHECK(t.nextMissing(0) == 3 && t.nextMissing(1) == 3);
    }

    TEST(timeline_frame_is_ready_only_with_every_player)
    {
        InputTimeline t(3, 2);
        t.set(0, 2, pressed(0x4000));
        t.set(2, 2, pressed(0x0010));
        CHECK(!t.ready(2));
        t.set(1, 2, PadState{});
        CHECK(t.ready(2));
        CHECK(t.get(0, 2) == pressed(0x4000));
        CHECK(t.get(2, 2) == pressed(0x0010));
    }

    TEST(timeline_next_missing_is_the_first_gap)
    {
        InputTimeline t(1, 1);
        t.set(0, 1, PadState{});
        t.set(0, 3, PadState{});
        CHECK(t.nextMissing(0) == 2);
        t.set(0, 2, PadState{});
        CHECK(t.nextMissing(0) == 4);
    }

    TEST(timeline_keeps_the_first_value_of_a_frame)
    {
        InputTimeline t(1, 0);
        t.set(0, 0, pressed(0x4000));
        t.set(0, 0, pressed(0x8000));   // repeticao (pacote redundante) nunca troca o que ja chegou
        CHECK(t.get(0, 0) == pressed(0x4000));
    }

    TEST(timeline_discard_drops_old_frames_but_keeps_progress)
    {
        InputTimeline t(1, 0);
        for (uint32_t f = 0; f < 10; ++f)
            t.set(0, f, pressed(static_cast<uint16_t>(f)));
        t.discardBefore(8);
        CHECK(t.nextMissing(0) == 10);
        CHECK(t.ready(9) && t.get(0, 9) == pressed(9));
        t.set(0, 3, pressed(0x1234));   // frame ja descartado: ignorado
        CHECK(t.nextMissing(0) == 10);
    }

    TEST(timeline_dropped_player_is_neutral_from_the_drop_frame)
    {
        InputTimeline t(2, 1);
        t.set(0, 1, pressed(1));
        t.set(1, 1, pressed(2));
        t.drop(1, 2);
        t.set(0, 2, pressed(3));
        CHECK(t.ready(2) && t.ready(500) == false);
        CHECK(t.get(1, 2) == PadState{});
        CHECK(t.get(1, 1) == pressed(2));
        t.set(1, 5, pressed(7));   // entrada atrasada depois da queda: ignorada
        CHECK(t.get(1, 5) == PadState{});
    }

    TEST(local_delay_schedules_sampled_input_ahead)
    {
        InputTimeline t(2, 3);
        CHECK(t.scheduleLocal(1, 0, pressed(0x4000)) == 3u);   // amostra do frame 0 vale no frame 0 + atraso
        CHECK(t.has(1, 3) && t.get(1, 3) == pressed(0x4000));
        CHECK(!t.has(0, 3));
    }

    // ---- protocolo ----
    namespace p = wotm_net::proto;

    TEST(proto_join_roundtrip)
    {
        p::Join j;
        j.token = "a1b2c3";
        j.buildId = "build-xyz";
        j.configHash = "cfg-1";
        j.name = "Lane";
        const std::vector<uint8_t> bytes = p::encode(j);
        CHECK(p::typeOf(bytes) == p::Type::Join);
        p::Join back;
        CHECK(p::decode(bytes, back));
        CHECK(back.protocol == p::kProtocol && back.token == j.token && back.buildId == j.buildId && back.configHash == j.configHash &&
              back.name == j.name);
    }

    TEST(proto_inputs_roundtrip_with_repeated_states)
    {
        p::Inputs m;
        m.ackOwn = 41;
        p::InputRun r;
        r.player = 2;
        r.first = 37;
        for (int i = 0; i < 20; ++i)
            r.states.push_back(i < 10 ? pressed(0x4000) : PadState{});
        m.runs.push_back(r);
        const std::vector<uint8_t> bytes = p::encode(m);
        CHECK(bytes.size() == 6u + 6u + 20u + 2u * 6u);   // cabecalhos, 1 byte por estado, 6 por estado novo (so 2 mudam)
        p::Inputs back;
        CHECK(p::decode(bytes, back));
        CHECK(back.ackOwn == 41u && back.runs.size() == 1u);
        CHECK(back.runs[0].player == 2 && back.runs[0].first == 37u && back.runs[0].states == r.states);
    }

    TEST(proto_input_roundtrip)
    {
        p::Input m;
        m.acks = {5, 9, 0, 12};
        m.own.player = 1;
        m.own.first = 9;
        m.own.states = {pressed(1), pressed(2), PadState{}};
        p::Input back;
        CHECK(p::decode(p::encode(m), back));
        CHECK(back.acks == m.acks && back.own.player == 1 && back.own.first == 9u && back.own.states == m.own.states);
    }

    TEST(proto_control_messages_roundtrip)
    {
        p::Welcome w{3, 4};
        p::Welcome wb;
        CHECK(p::decode(p::encode(w), wb) && wb.player == 3 && wb.players == 4);
        p::Start s;
        s.delay = 4;
        s.names = {"a", "b"};
        p::Start sb;
        CHECK(p::decode(p::encode(s), sb) && sb.delay == 4u && sb.names == s.names);
        p::Hash h{120, 0x1122334455667788ull};
        p::Hash hb;
        CHECK(p::decode(p::encode(h), hb) && hb.frame == 120u && hb.hash == h.hash);
        p::Drop d{1, 77};
        p::Drop db;
        CHECK(p::decode(p::encode(d), db) && db.player == 1 && db.frame == 77u);
        p::Desync x{90, "jogador 2 difere"};
        p::Desync xb;
        CHECK(p::decode(p::encode(x), xb) && xb.frame == 90u && xb.detail == x.detail);
        p::Reject r{"versao diferente"};
        p::Reject rb;
        CHECK(p::decode(p::encode(r), rb) && rb.reason == r.reason);
    }

    TEST(proto_truncated_message_is_rejected)
    {
        p::Inputs m;
        p::InputRun r;
        r.states = {pressed(3), pressed(4)};
        m.runs.push_back(r);
        std::vector<uint8_t> bytes = p::encode(m);
        bytes.resize(bytes.size() - 2);
        p::Inputs back;
        CHECK(!p::decode(bytes, back));
        p::Join j;
        CHECK(!p::decode(bytes, j));   // tipo errado
    }

    TEST(proto_delay_covers_the_relay_path)
    {
        // atraso = viagem de ida do mais lento ao servidor e do servidor ao segundo mais lento, em quadros, mais 1 de folga
        CHECK(p::pickDelay({10, 10}, 16) == 2u);
        CHECK(p::pickDelay({100, 60}, 16) == 6u);    // (100 + 60) / 2 = 80 ms = 5 quadros + 1
        CHECK(p::pickDelay({60, 100, 20}, 33) == 4u);   // 4 jogadores, quadro de 33 ms
        CHECK(p::pickDelay({2000, 2000}, 16) == 15u);   // teto
        CHECK(p::pickDelay({0}, 16) == 2u);             // piso
    }

    TEST(lobby_lines_split_words_and_key_values)
    {
        const std::vector<std::string> w = p::words("JOIN 1 Lane 3");
        CHECK(w.size() == 4u && w[0] == "JOIN" && w[3] == "3");
        const std::map<std::string, std::string> kv = p::keyValues({"MATCH", "players=2", "seed=99", "monsters=3,1"});
        CHECK(kv.at("players") == "2" && kv.at("seed") == "99" && kv.at("monsters") == "3,1" && kv.count("MATCH") == 0u);
        CHECK(p::cleanName("La ne<>!_9-xyzxyzxyzxyzxyz") == "Lane_9-xyzxyzxyz");   // 16 caracteres no maximo
        CHECK(p::cleanName("   ").empty());
    }

    // ---- servidor e cliente de verdade em localhost ----
    using wotm_net::ClientConfig;
    using wotm_net::LineSocket;
    using wotm_net::MatchClient;
    using wotm_net::Server;
    using wotm_net::ServerConfig;
    using Status = wotm_net::MatchClient::Status;

    uint16_t g_nextPort = 27770;

    // servidor rodando numa thread durante o teste
    struct RunningServer
    {
        Server server;
        std::atomic<bool> stop{false};
        std::thread thread;
        bool ok = false;
        explicit RunningServer(ServerConfig c) : server(withQuiet(c))
        {
            std::string err;
            ok = server.start(err);
            if (!ok)
                std::printf("  servidor nao subiu: %s\n", err.c_str());
            thread = std::thread([this] {
                while (!stop)
                    server.poll(1);
            });
        }
        ~RunningServer()
        {
            stop = true;
            thread.join();
        }
        static ServerConfig withQuiet(ServerConfig c)
        {
            c.log = false;
            return c;
        }
    };

    // porta livre em TCP e UDP (um processo de teste anterior que ainda esta fechando pode segurar as dele)
    uint16_t freePort()
    {
        for (int tries = 0; tries < 200; ++tries)
        {
            const uint16_t port = g_nextPort++;
            wotm_net::Listener tcp;
            if (!tcp.listen(port))
                continue;
            const wotm_net::SocketHandle udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_ANY);
            a.sin_port = htons(port);
            const bool ok = udp != wotm_net::kNoSocket && bind(udp, reinterpret_cast<sockaddr *>(&a), sizeof a) == 0;
            if (udp != wotm_net::kNoSocket)
                wotm_net::closeSocket(udp);
            if (ok)
                return port;
        }
        return g_nextPort++;
    }

    ServerConfig testConfig()
    {
        ServerConfig c;
        c.port = freePort();
        c.startWaitMs = 100;
        return c;
    }

    std::string readUntil(LineSocket &s, const std::string &prefix, int ms = 3000)
    {
        std::string line;
        while (s.readLine(line, ms))
            if (line.rfind(prefix, 0) == 0)
                return line;
        return {};
    }

    struct Member
    {
        LineSocket socket;
        std::map<std::string, std::string> match;
    };

    // forma uma partida pelo lobby: cada nome entra no lobby 0 com seu monstro e o primeiro manda START <fase>
    std::vector<std::unique_ptr<Member>> formMatch(uint16_t port, const std::vector<std::string> &names, const std::vector<int> &monsters,
                                                   int stage = 1, const std::string &build = "b1")
    {
        std::vector<std::unique_ptr<Member>> ms;
        for (size_t i = 0; i < names.size(); ++i)
        {
            ms.push_back(std::make_unique<Member>());
            Member &m = *ms.back();
            CHECK(m.socket.connect("127.0.0.1", port, 2000));
            m.socket.sendLine("HELLO 1 " + build);
            CHECK(readUntil(m.socket, "WELCOME") == "WELCOME 2");
            m.socket.sendLine("JOIN 0 " + names[i] + " " + std::to_string(monsters[i]));
            CHECK(readUntil(m.socket, "JOINED") == "JOINED 0");
        }
        ms[0]->socket.sendLine("START " + std::to_string(stage));
        for (auto &m : ms)
            m->match = p::keyValues(p::words(readUntil(m->socket, "MATCH")));
        return ms;
    }

    ClientConfig clientFor(uint16_t port, const Member &m, const std::string &config = "cfg")
    {
        ClientConfig c;
        c.host = "127.0.0.1";
        c.port = port;
        c.token = m.match.count("token") ? m.match.at("token") : "";
        c.buildId = "b1";
        c.configHash = config;
        c.name = "x";
        return c;
    }

    TEST(lobby_lists_joins_and_starts_a_match)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        CHECK(rs.ok);
        LineSocket watcher;
        CHECK(watcher.connect("127.0.0.1", cfg.port, 2000));
        watcher.sendLine("HELLO 1 b1");
        CHECK(readUntil(watcher, "WELCOME") == "WELCOME 2");
        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "LOBBY 0") == "LOBBY 0 waiting 0/8 -");
        CHECK(readUntil(watcher, "LOBBY 1") == "LOBBY 1 waiting 0/8 -");
        CHECK(readUntil(watcher, "END") == "END");

        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1}, 5);
        const auto &a = ms[0]->match, &b = ms[1]->match;
        CHECK(a.size() > 0u && b.size() > 0u);
        CHECK(a.count("players") && a.at("players") == "2" && b.at("players") == "2");
        CHECK(a.at("stage") == "5" && a.at("monsters") == "3,1" && b.at("monsters") == "3,1");
        CHECK(a.at("seed") == b.at("seed") && a.at("seed") != "0");
        CHECK(a.at("step") == "1" && a.at("names") == "Ana,Bia");
        CHECK(a.at("you") == "0" && b.at("you") == "1");
        CHECK(a.at("token") != b.at("token") && a.at("token").size() == 16u);

        watcher.sendLine("LIST");
        CHECK(readUntil(watcher, "LOBBY 0") == "LOBBY 0 playing 2/8 Ana,Bia");
    }

    TEST(lobby_refuses_other_protocols_builds_and_full_lobbies)
    {
        ServerConfig cfg = testConfig();
        cfg.maxPlayers = 2;
        RunningServer rs(cfg);
        LineSocket old;
        CHECK(old.connect("127.0.0.1", cfg.port, 2000));
        old.sendLine("HELLO 0 b1");
        CHECK(readUntil(old, "ERROR").find("protocolo") != std::string::npos);

        LineSocket a, b, c;
        for (LineSocket *s : {&a, &b, &c})
            CHECK(s->connect("127.0.0.1", cfg.port, 2000));
        a.sendLine("HELLO 1 b1");
        b.sendLine("HELLO 1 b2");
        c.sendLine("HELLO 1 b1");
        a.sendLine("JOIN 1 Ana 3");
        CHECK(readUntil(a, "JOINED") == "JOINED 1");
        b.sendLine("JOIN 1 Bia 2");
        CHECK(readUntil(b, "ERROR").find("build") != std::string::npos);
        c.sendLine("JOIN 1 Cai 2");
        CHECK(readUntil(c, "JOINED") == "JOINED 1");
        CHECK(readUntil(a, "MEMBERS 1 Ana:3,") == "MEMBERS 1 Ana:3,Cai:2");   // depois do "MEMBERS 1 Ana:3" da entrada da Ana
        b.sendLine("HELLO 1 b1");
        b.sendLine("JOIN 1 Bia 2");
        CHECK(readUntil(b, "ERROR").find("cheio") != std::string::npos);
    }

    // monstro sem arquivo no disco (spider 6, ogre 12, fora da tabela) ou sem traje livre (jelly so tem 1) nao entra: a carga do jogo
    // travaria em todos os PCs
    TEST(lobby_refuses_monsters_the_disc_does_not_have)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        LineSocket a, b;
        for (LineSocket *s : {&a, &b})
        {
            CHECK(s->connect("127.0.0.1", cfg.port, 2000));
            s->sendLine("HELLO 1 b1");
        }
        for (const char *bad : {"6", "12", "0", "16"})
        {
            a.sendLine(std::string("JOIN 0 Ana ") + bad);
            CHECK(readUntil(a, "ERROR").find("monstro") != std::string::npos);
        }
        a.sendLine("JOIN 0 Ana 14");
        CHECK(readUntil(a, "JOINED") == "JOINED 0");
        b.sendLine("JOIN 0 Bia 14");
        CHECK(readUntil(b, "ERROR").find("traje") != std::string::npos);
        b.sendLine("JOIN 0 Bia 3");
        CHECK(readUntil(b, "JOINED") == "JOINED 0");
        a.sendLine("LEAVE");
        a.sendLine("JOIN 0 Ana 3");
        CHECK(readUntil(a, "JOINED") == "JOINED 0");
    }

    // dois clientes da partida trocam 100 quadros de entrada e veem a mesma linha do tempo
    void exchange(MatchClient &a, MatchClient &b, uint32_t frames)
    {
        const uint32_t d = a.delay();
        for (uint32_t f = 0; f < frames; ++f)
        {
            a.pushLocal(f + d, pressed(static_cast<uint16_t>(f)));
            b.pushLocal(f + d, pressed(static_cast<uint16_t>(0x8000u | f)));
        }
        bool all = true;
        for (uint32_t f = 0; f < frames + d && all; ++f)
            all = a.waitFrame(f, 3000) && b.waitFrame(f, 3000);
        CHECK(all);
        bool same = true, right = true;
        for (uint32_t f = 0; f < frames + d; ++f)
            for (int pl = 0; pl < 2; ++pl)
            {
                same = same && a.input(pl, f) == b.input(pl, f);
                const PadState want = f < d ? PadState{} : pressed(static_cast<uint16_t>((pl ? 0x8000u : 0u) | (f - d)));
                right = right && a.input(pl, f) == want;
            }
        CHECK(same);
        CHECK(right);
    }

    TEST(match_clients_share_every_frame_of_input)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err));
        CHECK(b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        CHECK(a.localPlayer() == 0 && b.localPlayer() == 1 && a.players() == 2);
        CHECK(a.delay() >= 2u && a.delay() == b.delay());
        CHECK(a.names() == std::vector<std::string>({"Ana", "Bia"}));
        exchange(a, b, 100);
        CHECK(a.status() == Status::Running && b.status() == Status::Running);
    }

    // ate 8 jogadores (4 nos lugares de jogador do jogo e 4 em lugares de IA guiados pela rede): lobby, 30 Hz e entradas de todos
    TEST(match_with_eight_players_shares_every_input)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        const std::vector<std::string> names = {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8"};
        auto ms = formMatch(cfg.port, names, {3, 1, 2, 5, 4, 7, 8, 9});
        CHECK(ms[7]->match.count("players") && ms[7]->match.at("players") == "8" && ms[7]->match.at("step") == "2");
        if (!ms[7]->match.count("you"))
            return;   // o oitavo nao entrou na partida: o resto nao tem como rodar
        CHECK(ms[7]->match.at("you") == "7");
        std::vector<std::unique_ptr<MatchClient>> cs;
        std::string err;
        for (auto &m : ms)
        {
            cs.push_back(std::make_unique<MatchClient>());
            CHECK(cs.back()->start(clientFor(cfg.port, *m), err));
        }
        bool started = true;
        for (auto &c : cs)
            started = started && c->waitStarted(5000);
        CHECK(started);
        const uint32_t d = cs[0]->delay();
        for (uint32_t f = 0; f < 60; ++f)
            for (int p = 0; p < 8; ++p)
                cs[static_cast<size_t>(p)]->pushLocal(f + d, pressed(static_cast<uint16_t>((p << 8) | f)));
        bool all = true, same = true;
        for (auto &c : cs)
            all = all && c->waitFrame(d + 59, 5000);
        CHECK(all);
        for (uint32_t f = d; f < d + 60; ++f)
            for (int p = 0; p < 8; ++p)
                for (auto &c : cs)
                    same = same && c->input(p, f) == pressed(static_cast<uint16_t>((p << 8) | (f - d)));
        CHECK(same);
    }

    TEST(match_survives_lost_input_packets)
    {
        ServerConfig cfg = testConfig();
        cfg.dropInputPercent = 30;
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        ClientConfig ca = clientFor(cfg.port, *ms[0]), cb = clientFor(cfg.port, *ms[1]);
        ca.dropInputPercent = cb.dropInputPercent = 30;
        CHECK(a.start(ca, err) && b.start(cb, err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        exchange(a, b, 200);
    }

    TEST(match_rejects_unknown_tokens_and_other_configs)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient bad;
        std::string err;
        Member fake;
        fake.match["token"] = "0000000000000000";
        CHECK(bad.start(clientFor(cfg.port, fake), err));
        CHECK(!bad.waitStarted(1500) && bad.status() == Status::Rejected);
        CHECK(bad.message().find("ficha") != std::string::npos);

        MatchClient a, b;
        CHECK(a.start(clientFor(cfg.port, *ms[0], "cfgA"), err));
        CHECK(b.start(clientFor(cfg.port, *ms[1], "cfgB"), err));
        CHECK(!b.waitStarted(1500) && b.status() == Status::Rejected);
        CHECK(b.message().find("configura") != std::string::npos);
    }

    TEST(match_drops_a_player_that_leaves_at_an_agreed_frame)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        const uint32_t d = a.delay();
        for (uint32_t f = 0; f < 50; ++f)
            b.pushLocal(f + d, pressed(1));
        CHECK(b.waitAcked(3000));
        b.stop();
        for (uint32_t f = 0; f < 80; ++f)
            a.pushLocal(f + d, pressed(2));
        CHECK(a.waitFrame(d + 79, 5000));
        CHECK(a.dropFrame(1) == d + 50);
        CHECK(a.input(1, d + 49) == pressed(1) && a.input(1, d + 50) == PadState{});
    }

    TEST(match_stops_both_peers_on_a_state_hash_mismatch)
    {
        ServerConfig cfg = testConfig();
        RunningServer rs(cfg);
        auto ms = formMatch(cfg.port, {"Ana", "Bia"}, {3, 1});
        MatchClient a, b;
        std::string err;
        CHECK(a.start(clientFor(cfg.port, *ms[0]), err) && b.start(clientFor(cfg.port, *ms[1]), err));
        CHECK(a.waitStarted(5000) && b.waitStarted(5000));
        a.sendHash(30, 111);
        b.sendHash(30, 111);
        a.sendHash(60, 222);
        b.sendHash(60, 223);
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while ((a.status() != Status::Desynced || b.status() != Status::Desynced) && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(a.status() == Status::Desynced && b.status() == Status::Desynced);
        CHECK(a.desyncFrame() == 60u);
        CHECK(!a.waitFrame(a.delay(), 200));   // dessincronizado: nenhum quadro novo roda
    }
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // cada linha sai na hora: um teste travado mostra onde parou
    for (const Test &t : tests())
    {
        const int before = g_failures;
        try
        {
            t.run();
        }
        catch (const std::exception &e)   // uma excecao e falha do teste, nunca um abort (que deixa o processo preso no relatorio de erro)
        {
            ++g_failures;
            std::printf("  EXCECAO %s\n", e.what());
        }
        std::printf("%s %s\n", g_failures == before ? "ok     " : "FALHOU ", t.name);
    }
    std::printf("%d checagens, %d falha(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
