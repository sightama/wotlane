// ABOUTME: Testes do netcode sem o jogo: linha do tempo das entradas (lockstep), codificacao das mensagens, servidor e cliente ENet
// ABOUTME: de verdade em localhost. Sai com 0 se tudo passar; cada falha imprime arquivo, linha e a expressao.
#include "wotm_lockstep.hpp"

#include <cstdio>
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
