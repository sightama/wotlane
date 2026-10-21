// ABOUTME: Testes do netcode sem o jogo: linha do tempo das entradas (lockstep), codificacao das mensagens, servidor e cliente ENet
// ABOUTME: de verdade em localhost. Sai com 0 se tudo passar; cada falha imprime arquivo, linha e a expressao.
#include "wotm_lockstep.hpp"
#include "wotm_net_proto.hpp"

#include <cstdio>
#include <map>
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
}

int main()
{
    for (const Test &t : tests())
    {
        const int before = g_failures;
        t.run();
        std::printf("%s %s\n", g_failures == before ? "ok     " : "FALHOU ", t.name);
    }
    std::printf("%d checagens, %d falha(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
