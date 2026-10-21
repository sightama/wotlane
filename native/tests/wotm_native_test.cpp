// ABOUTME: Testes de unidade das partes puras da camada nativa (sem jogo, sem janela): por enquanto a interpolacao de quadros
// ABOUTME: (matrizes, camera, corte, relogio de exibicao, HUD rebaseado). Compilar e rodar: tools\test_native.bat
#include "wotm_interp.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
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

    namespace ip = wotm::interp;
    using Mat = float[4][4];

    bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps * std::max(1.f, std::fabs(b)); }

    void identity(Mat m)
    {
        std::memset(m, 0, sizeof(Mat));
        for (int i = 0; i < 4; ++i)
            m[i][i] = 1.f;
    }

    // rotacao em torno de Z (vetores-linha: linha 0 = imagem do eixo X), escala por linha e translacao
    void rotZ(Mat m, float deg, float sx = 1.f, float sy = 1.f, float sz = 1.f, float tx = 0.f, float ty = 0.f, float tz = 0.f)
    {
        const float a = deg * 3.14159265f / 180.f, c = std::cos(a), s = std::sin(a);
        identity(m);
        m[0][0] = c * sx, m[0][1] = s * sx;
        m[1][0] = -s * sy, m[1][1] = c * sy;
        m[2][2] = sz;
        m[3][0] = tx, m[3][1] = ty, m[3][2] = tz;
    }

    float rowLen(const Mat m, int r) { return std::sqrt(m[r][0] * m[r][0] + m[r][1] * m[r][1] + m[r][2] * m[r][2]); }

    float det3(const Mat m)
    {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
               m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    TEST(lerp_ends_are_the_inputs_bit_for_bit)
    {
        Mat a, b, r;
        rotZ(a, 10.f, 1.f, 1.f, 1.f, 5.f, 6.f, 7.f);
        rotZ(b, 70.f, 1.f, 1.f, 1.f, 8.f, 9.f, 10.f);
        ip::lerpMatrix(a, b, 0.f, r);
        CHECK(std::memcmp(r, a, sizeof(Mat)) == 0);
        ip::lerpMatrix(a, b, 1.f, r);
        CHECK(std::memcmp(r, b, sizeof(Mat)) == 0);
    }

    TEST(lerp_rotates_without_shrinking)
    {
        Mat a, b, r, want;
        rotZ(a, 0.f);
        rotZ(b, 90.f);
        ip::lerpMatrix(a, b, 0.5f, r);
        rotZ(want, 45.f);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                CHECK(near(r[i][j], want[i][j], 1e-3f));
        CHECK(near(rowLen(r, 0), 1.f) && near(rowLen(r, 1), 1.f));   // lerp por componente daria 0,707
    }

    TEST(lerp_keeps_per_row_scale_and_translation)
    {
        Mat a, b, r;
        rotZ(a, 0.f, 2.f, 1.f, 3.f, 0.f, 0.f, 0.f);
        rotZ(b, 60.f, 4.f, 1.f, 3.f, 10.f, -20.f, 4.f);
        ip::lerpMatrix(a, b, 0.5f, r);
        CHECK(near(rowLen(r, 0), 3.f, 1e-3f) && near(rowLen(r, 1), 1.f, 1e-3f) && near(rowLen(r, 2), 3.f, 1e-3f));
        CHECK(near(r[3][0], 5.f) && near(r[3][1], -10.f) && near(r[3][2], 2.f) && near(r[3][3], 1.f));
        const float ang = std::atan2(r[0][1], r[0][0]) * 180.f / 3.14159265f;
        CHECK(near(ang, 30.f, 1e-3f));
    }

    TEST(lerp_keeps_mirrored_matrices_mirrored)
    {
        Mat a, b, r;
        rotZ(a, 0.f, -1.f, 1.f, 1.f);   // espelho em X
        rotZ(b, 40.f, -1.f, 1.f, 1.f);
        ip::lerpMatrix(a, b, 0.5f, r);
        CHECK(det3(r) < 0.f);
        CHECK(near(rowLen(r, 0), 1.f, 1e-3f) && near(rowLen(r, 1), 1.f, 1e-3f));
    }

    TEST(lerp_falls_back_to_components_on_shear)
    {
        Mat a, b, r;
        rotZ(a, 0.f);
        rotZ(b, 0.f);
        b[1][0] = 0.8f;   // linha 1 nao e perpendicular a linha 0
        ip::lerpMatrix(a, b, 0.5f, r);
        CHECK(near(r[1][0], 0.4f) && near(r[0][0], 1.f) && near(r[1][1], 1.f));
    }

    TEST(slerp_takes_the_short_way)
    {
        const ip::Quat a{1.f, 0.f, 0.f, 0.f}, b{-0.9999f, 0.f, 0.f, 0.0141f};   // quase a mesma rotacao, sinal oposto
        const ip::Quat r = ip::slerp(a, b, 0.5f);
        CHECK(std::fabs(r.w) > 0.999f);
    }

    ip::Camera cam(float x, float y, float z, float yawDeg, float fovy = 60.f)
    {
        ip::Camera c;
        const float a = yawDeg * 3.14159265f / 180.f;
        c.pos[0] = x, c.pos[1] = y, c.pos[2] = z;
        c.fwd[0] = std::cos(a), c.fwd[1] = std::sin(a), c.fwd[2] = 0.f;
        c.up[0] = 0.f, c.up[1] = 0.f, c.up[2] = 1.f;
        c.fovy = fovy;
        return c;
    }

    TEST(camera_lerp_moves_turns_and_zooms)
    {
        const ip::Camera r = ip::lerpCamera(cam(0, 0, 0, 0, 60.f), cam(10, 0, 2, 30, 70.f), 0.5f);
        CHECK(near(r.pos[0], 5.f) && near(r.pos[2], 1.f) && near(r.fovy, 65.f));
        CHECK(near(std::atan2(r.fwd[1], r.fwd[0]) * 180.f / 3.14159265f, 15.f, 1e-3f));
        CHECK(near(r.fwd[0] * r.fwd[0] + r.fwd[1] * r.fwd[1] + r.fwd[2] * r.fwd[2], 1.f));
    }

    TEST(camera_cuts_are_not_blended)
    {
        CHECK(!ip::cameraCut(cam(0, 0, 0, 0), cam(30, 0, 0, 20)));
        CHECK(ip::cameraCut(cam(0, 0, 0, 0), cam(500, 0, 0, 0)));            // salto de posicao
        CHECK(ip::cameraCut(cam(0, 0, 0, 0), cam(0, 0, 0, 60)));             // giro brusco (dot < 0,7)
        CHECK(ip::cameraCut(cam(0, 0, 0, 0, 60.f), cam(0, 0, 0, 0, 70.f)));  // zoom brusco
    }

    // M' = M_B * inv(V_B) * V_I: o objeto fica na mesma posicao relativa a camera interpolada que tinha em relacao a de B
    TEST(hud_rebase_keeps_the_pose_relative_to_the_camera)
    {
        const ip::Camera cb = cam(100, 50, 10, 25), ci = cam(90, 45, 9, 18);
        Mat vb, vi, ivb, ivi, m, r, relB, relI;
        ip::cameraToWorld(cb, vb);
        ip::cameraToWorld(ci, vi);
        rotZ(m, 33.f, 0.2f, 0.2f, 0.2f, 0.f, 0.f, 0.f);
        // objeto 5 unidades a frente da camera B
        for (int k = 0; k < 3; ++k)
            m[3][k] = cb.pos[k] + 5.f * cb.fwd[k];
        ip::rebase(m, cb, ci, r);
        ip::invertRigid(vb, ivb);
        ip::invertRigid(vi, ivi);
        ip::mul(m, ivb, relB);
        ip::mul(r, ivi, relI);
        bool same = true;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                same = same && near(relB[i][j], relI[i][j], 1e-3f);
        CHECK(same);
    }

    // matriz mundo -> tela das particulas trocada para a camera misturada: um ponto parado em relacao a camera cai no mesmo pixel
    TEST(world_to_screen_follows_the_blended_camera)
    {
        const ip::Camera cb = cam(100, 50, 10, 25), ci = cam(90, 45, 9, 18);
        Mat w{};   // projecao qualquer (com perspectiva) da camera B
        Mat vb, ivb;
        ip::cameraToWorld(cb, vb);
        ip::invertRigid(vb, ivb);
        const Mat proj = {{384.f, 0.f, 0.f, 0.f}, {2048.f, 0.f, 0.f, 1.f}, {0.f, -180.48f, 0.f, 0.f}, {2048.f, 2048.f, 0.f, 0.f}};
        ip::mul(ivb, proj, w);
        Mat wi;
        ip::reprojectToCamera(w, cb, ci, wi);
        // ponto 30 a frente, 4 a direita e 2 acima da camera B; o mesmo ponto em relacao a camera I
        auto at = [](const ip::Camera &c, float f, float r, float u, float out[4]) {
            Mat v;
            ip::cameraToWorld(c, v);
            for (int j = 0; j < 3; ++j)
                out[j] = c.pos[j] + f * v[1][j] + r * v[0][j] + u * v[2][j];
            out[3] = 1.f;
        };
        auto xf = [](const float p[4], const Mat m, float out[4]) {
            for (int j = 0; j < 4; ++j)
                out[j] = p[0] * m[0][j] + p[1] * m[1][j] + p[2] * m[2][j] + p[3] * m[3][j];
        };
        float pb[4], pi[4], sb[4], si[4];
        at(cb, 30.f, 4.f, 2.f, pb);
        at(ci, 30.f, 4.f, 2.f, pi);
        xf(pb, w, sb);
        xf(pi, wi, si);
        bool same = true;
        for (int j = 0; j < 4; ++j)
            same = same && near(sb[j], si[j], 1e-3f);
        CHECK(same);
    }

    // folga do relogio ajustada pela frequencia de imagens seguradas (quadro do jogo chegou depois do relogio de exibicao)
    TEST(delay_grows_when_frames_arrive_late_and_shrinks_slowly)
    {
        ip::DelayTuner d(6.0, 4.0, 25.0);
        for (int i = 0; i < 300; ++i)
            d.onRedraw(i % 10 == 0);   // 10% seguradas
        CHECK(d.ms() > 6.0);
        const double up = d.ms();
        for (int k = 0; k < 20; ++k)
            for (int i = 0; i < 300; ++i)
                d.onRedraw(false);
        CHECK(d.ms() < up && d.ms() >= 4.0);
        for (int k = 0; k < 200; ++k)
            for (int i = 0; i < 300; ++i)
                d.onRedraw(true);
        CHECK(d.ms() == 25.0);   // teto
    }

    // relogio de conteudo: passo fixo por apresentacao, puxado devagar para "mais novo - atraso"; quadros do jogo a 60 chegando com
    // +-5 ms de variacao, tela a 144 Hz
    TEST(content_clock_steps_evenly_with_jittery_arrivals)
    {
        ip::ContentClock c;
        std::mt19937 rng(3);
        std::uniform_real_distribution<double> jitter(0.0, 0.010);
        const double present = 1.0 / 144.0, step = 60.0 / 144.0;
        std::vector<double> arrivals;
        for (int f = 0; f < 700; ++f)
            arrivals.push_back(f / 60.0 + jitter(rng));
        double newest = 0.0, last = -1.0;
        size_t next = 0;
        std::vector<double> steps;
        int held = 0;
        for (int k = 0; k < 1600; ++k)
        {
            const double now = k * present;
            while (next < arrivals.size() && arrivals[next] <= now)
                newest = static_cast<double>(next++);
            const double p = c.advance(step, newest, 1.35);
            if (k > 200)
            {
                steps.push_back(p - last);
                held += p >= newest;
            }
            CHECK(p <= newest);   // nunca extrapola
            last = p;
        }
        float mn = 1e9f, mx = -1e9f;
        for (double s : steps)
            mn = std::min(mn, static_cast<float>(s)), mx = std::max(mx, static_cast<float>(s));
        CHECK(mn > 0.30f && mx < 0.55f);   // passo de 0,417 field, sem trancos
        CHECK(held == 0);
    }

    TEST(content_clock_holds_on_a_stall_and_resyncs)
    {
        ip::ContentClock c;
        const double step = 60.0 / 144.0;
        double p = 0.0;
        for (int k = 0; k < 100; ++k)
            p = c.advance(step, 10.0 + k * step, 1.35);
        const double before = p;
        for (int k = 0; k < 30; ++k)   // o jogo parou: o mais novo nao muda
            p = c.advance(step, 50.0, 1.35);
        CHECK(p <= 50.0 && p > before);
        p = c.advance(step, 200.0, 1.35);   // voltou muito adiante (carga): pula, sem correr atras
        CHECK(near(static_cast<float>(p), 200.f - 1.35f));
    }

    TEST(alpha_is_clamped_and_never_extrapolates)
    {
        CHECK(ip::alphaBetween(5.0, 10, 11) == 0.f);
        CHECK(ip::alphaBetween(10.25, 10, 11) == 0.25f);
        CHECK(ip::alphaBetween(12.0, 10, 11) == 1.f);
        CHECK(ip::alphaBetween(11.0, 10, 12) == 0.5f);   // quadro de 2 fields (modo de 30 Hz)
        CHECK(ip::alphaBetween(11.0, 10, 10) == 1.f);
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
