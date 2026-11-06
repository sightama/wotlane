// ABOUTME: Unit tests for the pure parts of the native layer (no game, no window): frame interpolation from wotm_interp.hpp (matrices,
// ABOUTME: camera, cuts, projection), the settings profile for network matches and the mod digest. Build and run: tools\test_native.bat
#include "wotm_interp.hpp"
#include "wotm_settings.hpp"
#include "wotm_mods.hpp"
#include "wotm_detmath.hpp"
#include "wotm_freecam.h"
#include "wotm_trig_goldens.hpp"
#include "net/wotm_modifiers.hpp"
#include "wotm_tint.hpp"
#include "wotm_creator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
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
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #expr);                                                          \
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
    using wotm::GameCamera;
    using wotm::M4;

    bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps * std::max(1.f, std::fabs(b)); }

    // rotation around Z (row vectors: row 0 = image of the X axis), per-row scale and translation
    M4 rotZ(float deg, float sx = 1.f, float sy = 1.f, float sz = 1.f, float tx = 0.f, float ty = 0.f, float tz = 0.f)
    {
        const float a = deg * 3.14159265f / 180.f, c = std::cos(a), s = std::sin(a);
        M4 m = wotm::ident();
        m.m[0][0] = c * sx, m.m[0][1] = s * sx;
        m.m[1][0] = -s * sy, m.m[1][1] = c * sy;
        m.m[2][2] = sz;
        m.m[3][0] = tx, m.m[3][1] = ty, m.m[3][2] = tz;
        return m;
    }

    float rowLen(const M4 &m, int r) { return std::sqrt(m.m[r][0] * m.m[r][0] + m.m[r][1] * m.m[r][1] + m.m[r][2] * m.m[r][2]); }

    float det3(const M4 &m)
    {
        return m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
               m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
    }

    M4 mulM4(const M4 &a, const M4 &b)   // a first, then b (like wotm::mul in wotm_native.hpp, which pulls in raylib)
    {
        M4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    r.m[i][j] += a.m[i][k] * b.m[k][j];
        return r;
    }

    bool sameM4(const M4 &a, const M4 &b, float eps)
    {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (!near(a.m[i][j], b.m[i][j], eps))
                    return false;
        return true;
    }

    TEST(lerp_ends_are_the_inputs)
    {
        const M4 a = rotZ(10.f, 1.f, 1.f, 1.f, 5.f, 6.f, 7.f), b = rotZ(70.f, 1.f, 1.f, 1.f, 8.f, 9.f, 10.f);
        CHECK(sameM4(ip::lerpM4(a, b, 0.f), a, 1e-4f));
        CHECK(sameM4(ip::lerpM4(a, b, 1.f), b, 1e-4f));
    }

    TEST(lerp_rotates_without_shrinking)
    {
        const M4 r = ip::lerpM4(rotZ(0.f), rotZ(90.f), 0.5f), want = rotZ(45.f);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                CHECK(near(r.m[i][j], want.m[i][j], 1e-3f));
        CHECK(near(rowLen(r, 0), 1.f) && near(rowLen(r, 1), 1.f));   // a per-component lerp would give 0.707
    }

    TEST(lerp_keeps_per_row_scale_and_translation)
    {
        const M4 r = ip::lerpM4(rotZ(0.f, 2.f, 1.f, 3.f), rotZ(60.f, 4.f, 1.f, 3.f, 10.f, -20.f, 4.f), 0.5f);
        CHECK(near(rowLen(r, 0), 3.f, 1e-3f) && near(rowLen(r, 1), 1.f, 1e-3f) && near(rowLen(r, 2), 3.f, 1e-3f));
        CHECK(near(r.m[3][0], 5.f) && near(r.m[3][1], -10.f) && near(r.m[3][2], 2.f) && near(r.m[3][3], 1.f));
        CHECK(near(std::atan2(r.m[0][1], r.m[0][0]) * 180.f / 3.14159265f, 30.f, 1e-3f));
    }

    TEST(lerp_keeps_mirrored_matrices_mirrored)
    {
        const M4 r = ip::lerpM4(rotZ(0.f, -1.f), rotZ(40.f, -1.f), 0.5f);   // both mirrored in X
        CHECK(det3(r) < 0.f);
        CHECK(near(rowLen(r, 0), 1.f, 1e-3f) && near(rowLen(r, 1), 1.f, 1e-3f));
    }

    TEST(lerp_falls_back_to_components_on_shear)
    {
        M4 b = rotZ(0.f);
        b.m[1][0] = 0.8f;   // row 1 is not perpendicular to row 0
        const M4 r = ip::lerpM4(rotZ(0.f), b, 0.5f);
        CHECK(near(r.m[1][0], 0.4f) && near(r.m[0][0], 1.f) && near(r.m[1][1], 1.f));
    }

    TEST(slerp_takes_the_short_way)
    {
        const ip::Quat a{1.f, 0.f, 0.f, 0.f}, b{-0.9999f, 0.f, 0.f, 0.0141f};   // almost the same rotation, opposite sign
        CHECK(std::fabs(ip::slerp(a, b, 0.5f).w) > 0.999f);
    }

    GameCamera cam(float x, float y, float z, float yawDeg, float fovy = 60.f)
    {
        GameCamera c;
        const float a = yawDeg * 3.14159265f / 180.f;
        c.ok = true;
        c.pos[0] = x, c.pos[1] = y, c.pos[2] = z;
        c.fwd[0] = std::cos(a), c.fwd[1] = std::sin(a), c.fwd[2] = 0.f;
        c.up[0] = 0.f, c.up[1] = 0.f, c.up[2] = 1.f;
        c.fovy = fovy;
        return c;
    }

    TEST(camera_lerp_moves_turns_and_zooms)
    {
        const GameCamera r = ip::lerpCamera(cam(0, 0, 0, 0, 60.f), cam(10, 0, 2, 30, 70.f), 0.5f);
        CHECK(near(r.pos[0], 5.f) && near(r.pos[2], 1.f) && near(r.fovy, 65.f));
        CHECK(near(std::atan2(r.fwd[1], r.fwd[0]) * 180.f / 3.14159265f, 15.f, 1e-3f));
        CHECK(near(r.fwd[0] * r.fwd[0] + r.fwd[1] * r.fwd[1] + r.fwd[2] * r.fwd[2], 1.f));
    }

    TEST(camera_cuts_are_not_blended)
    {
        CHECK(!ip::cameraCut(cam(0, 0, 0, 0), cam(30, 0, 0, 20)));
        CHECK(ip::cameraCut(cam(0, 0, 0, 0), cam(500, 0, 0, 0)));            // position jump
        CHECK(ip::cameraCut(cam(0, 0, 0, 0), cam(0, 0, 0, 60)));             // sudden turn (dot < 0.7)
        CHECK(ip::cameraCut(cam(0, 0, 0, 0, 60.f), cam(0, 0, 0, 0, 70.f)));  // sudden zoom
        GameCamera bad = cam(0, 0, 0, 0);
        bad.ok = false;
        CHECK(ip::cameraCut(bad, cam(0, 0, 0, 0)));                          // unreadable camera: no blending
    }

    // camera -> world and the rigid inverse: round trip of a point and a direction, and C * inverse = identity
    TEST(camera_to_world_and_back)
    {
        const GameCamera c = cam(100, 50, 10, 25);
        const M4 C = ip::cameraToWorld(c), inv = ip::rigidInverse(C);
        const float p[3] = {130.f, 70.f, 12.f}, d[3] = {0.3f, -0.2f, 0.9f};
        float q[3], back[3];
        ip::toCam(C, p, true, q);
        ip::fromCam(C, q, true, back);
        CHECK(near(back[0], p[0], 1e-3f) && near(back[1], p[1], 1e-3f) && near(back[2], p[2], 1e-3f));
        ip::toCam(C, d, false, q);
        ip::fromCam(C, q, false, back);
        CHECK(near(back[0], d[0], 1e-3f) && near(back[1], d[1], 1e-3f) && near(back[2], d[2], 1e-3f));
        CHECK(sameM4(mulM4(C, inv), wotm::ident(), 1e-4f));
    }

    // projection with the game's focal length: a point straight ahead lands in the center, to the right gives x > 0, above gives
    // y < 0 (lines go down), behind the camera doesn't project; the focal length comes out of the world -> screen matrix
    TEST(project_matches_the_game_focus)
    {
        const GameCamera c = cam(0, 0, 0, 0);
        const M4 C = ip::cameraToWorld(c);
        ip::Focus f;
        float x = 0.f, y = 0.f;
        const float ahead[3] = {100.f, 0.f, 0.f}, right[3] = {100.f, -10.f, 0.f}, up[3] = {100.f, 0.f, 10.f}, behind[3] = {-100.f, 0.f, 0.f};
        CHECK(ip::project(C, f, ahead, x, y) && near(x, 0.f) && near(y, 0.f));
        CHECK(ip::project(C, f, right, x, y) && near(x, 38.4f, 1e-3f));   // right = forward x up: -Y with forward +X and up +Z
        CHECK(ip::project(C, f, up, x, y) && near(y, -18.048f, 1e-3f));
        CHECK(!ip::project(C, f, behind, x, y));
        // rows: 0 = fh*right + c*forward, 1 = -fv*up + c*forward, 3 = forward (the camera above: forward +X, right -Y, up +Z)
        const float M[16] = {2048.f, -384.f, 0.f, 0.f, 2048.f, 0.f, -180.48f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f};
        ip::Focus g;
        CHECK(ip::focusOf(M, g) && near(g.fh, 384.f, 1e-3f) && near(g.fv, 180.48f, 1e-3f));
    }

    // Gameplay modifiers (net/wotm_modifiers.hpp): the name list the lobby, the launcher and WOTM_MODIFIERS use, in canonical order;
    // turbo's frame shape (2 fields per frame, paced at 1 vsync) and the offline turbo clock
    TEST(modifier_list_parse_and_format)
    {
        namespace md = wotm::modifiers;
        int bits = -1;
        CHECK(md::parse("turbo,jump", bits) && bits == (md::Jump | md::Turbo));
        CHECK(md::format(bits) == "jump,turbo");
        CHECK(md::parse("", bits) && bits == 0 && md::format(0).empty());
        CHECK(md::parse("fast, fast", bits) && bits == md::Fast);
        CHECK(md::parse("jump,fast,turbo", bits) && md::format(bits) == "jump,fast,turbo");
        bits = 7;
        CHECK(!md::parse("moon", bits) && bits == 7);
        CHECK(!md::parse("jump,", bits));
        CHECK(md::fieldsPerFrame(1, md::Turbo) == 2 && md::fieldsPerFrame(2, 0) == 2 && md::fieldsPerFrame(1, md::Jump) == 1);
        CHECK(md::vsyncsPerFrame(2, md::Turbo) == 1 && md::vsyncsPerFrame(2, 0) == 2 && md::vsyncsPerFrame(1, md::Turbo) == 1);
        CHECK(md::scaledClock(110u, 100u, 500u, 2u) == 520u);
        CHECK(md::scaledClock(5u, 0xFFFFFFF0u, 0u, 2u) == 42u);   // the host clock wrapped around
    }

    // Modifier options (wotm_settings.hpp): three on/off keys offline; WOTM_MODIFIERS sets all three; in a network match the lobby's
    // list (g_netMods) is forced and the player's own values stay stored
    TEST(modifier_settings_and_net_lock)
    {
        namespace st = wotm::settings;
        namespace md = wotm::modifiers;
        st::g_netProfile.store(false);
        st::g_val[st::SuperJump].store(1);
        st::g_val[st::FastMonsters].store(0);
        st::g_val[st::Turbo].store(0);
        CHECK(st::modifiers() == md::Jump && !st::forced(st::SuperJump));
        CHECK(st::applyModifierList("fast,turbo"));
        CHECK(st::modifiers() == (md::Fast | md::Turbo));
        CHECK(!st::applyModifierList("moon") && st::modifiers() == (md::Fast | md::Turbo));
        st::g_val[st::SuperJump].store(1);
        st::g_netProfile.store(true);
        st::g_netMods.store(md::Turbo);
        CHECK(st::modifiers() == md::Turbo);
        CHECK(st::forced(st::SuperJump) && st::forced(st::Turbo) && st::get(st::SuperJump) == 0 && st::get(st::Turbo) == 1);
        CHECK(st::g_val[st::SuperJump].load() == 1);   // the player's own choice is still stored
        st::g_netProfile.store(false);
        st::g_netMods.store(0);
        CHECK(st::modifiers() == (md::Jump | md::Fast | md::Turbo));
        int v = -1;
        CHECK(st::parse(st::Turbo, "1", v) && v == 1 && st::parse(st::Turbo, "0", v) && v == 0);
        CHECK(std::string(st::kEnv[st::SuperJump]) == "WOTM_SUPERJUMP" && std::string(st::kEnv[st::Turbo]) == "WOTM_TURBO");
        st::g_val[st::SuperJump].store(0), st::g_val[st::FastMonsters].store(0), st::g_val[st::Turbo].store(0);
    }

    // Network match profile (wotm_settings.hpp): settings that change game state take the fixed profile on every PC, the
    // player's own values are kept and come back outside network play; video settings stay free
    TEST(netplay_profile_forces_gameplay_settings_only)
    {
        namespace st = wotm::settings;
        st::g_val[st::Camera].store(1);
        st::g_val[st::CamSens].store(9);
        st::g_val[st::LockToggle].store(1);
        st::g_val[st::RenderScale].store(150);
        st::g_netProfile.store(false);
        CHECK(st::get(st::Camera) == 1 && st::get(st::CamSens) == 9 && st::get(st::LockToggle) == 1);
        CHECK(!st::forced(st::Camera));
        st::g_netProfile.store(true);
        CHECK(st::get(st::Camera) == 0);
        CHECK(st::get(st::CamSens) == 5);
        CHECK(st::get(st::LockToggle) == 0);
        CHECK(st::get(st::CloseCam) == 1);
        CHECK(st::get(st::CamInvertY) == 0);
        CHECK(st::forced(st::Camera) && st::forced(st::CamSens));
        CHECK(st::get(st::RenderScale) == 150 && !st::forced(st::RenderScale));
        CHECK(st::g_val[st::Camera].load() == 1 && st::g_val[st::CamSens].load() == 9);   // the player's values are still stored
        st::g_netProfile.store(false);
        CHECK(st::get(st::Camera) == 1 && st::get(st::CamSens) == 9);
    }

    // Digest of the enabled mods (wotm_mods.hpp), which goes into the network match's settings hash: the same set of files gives the
    // same value in any creation order; changing the content or a name, or adding a mod, changes it; a disabled folder ('_') doesn't count
    TEST(mod_digest_covers_active_mods_only)
    {
        namespace fs = std::filesystem;
        const fs::path root = fs::temp_directory_path() / "wotm_modtest";
        auto put = [](const fs::path &p, const std::string &s) {
            fs::create_directories(p.parent_path());
            std::ofstream(p, std::ios::binary) << s;
        };
        fs::remove_all(root);
        const fs::path a = root / "a" / "mods", b = root / "b" / "mods";
        CHECK(wotm::mods::digest(a) == 0);   // no folder: no mods
        put(a / "skin" / "MON" / "LIZARD1.TEX", "tex");
        put(a / "skin" / "MON" / "LIZARD1.NGP", "ngp");
        put(b / "skin" / "MON" / "LIZARD1.NGP", "ngp");
        put(b / "skin" / "MON" / "LIZARD1.TEX", "tex");
        const uint64_t base = wotm::mods::digest(a);
        CHECK(base != 0);
        CHECK(wotm::mods::digest(b) == base);
        put(b / "_off" / "MON" / "MONKEY1.TEX", "x");
        CHECK(wotm::mods::digest(b) == base);
        put(b / "skin" / "MON" / "LIZARD1.TEX", "teX");
        CHECK(wotm::mods::digest(b) != base);
        put(b / "skin" / "MON" / "LIZARD1.TEX", "tex");
        CHECK(wotm::mods::digest(b) == base);
        put(b / "extra" / "MON" / "MONKEY1.TEX", "x");
        CHECK(wotm::mods::digest(b) != base);
        fs::remove_all(b / "extra");
        fs::rename(b / "skin" / "MON" / "LIZARD1.NGP", b / "skin" / "MON" / "LIZARD2.NGP");
        CHECK(wotm::mods::digest(b) != base);
        fs::remove_all(root);
    }
}

namespace
{
    namespace dm = wotm::detmath;

    uint32_t bitsOf(float f)
    {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        return u;
    }
    float floatOf(uint32_t u)
    {
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    // distance in representable floats (same sign only, which is all the accuracy checks below need)
    uint32_t ulps(float a, float b)
    {
        const uint32_t x = bitsOf(a) & 0x7FFFFFFFu, y = bitsOf(b) & 0x7FFFFFFFu;
        return (bitsOf(a) >> 31) != (bitsOf(b) >> 31) ? (x + y) : (x > y ? x - y : y - x);
    }
    // fixed inputs for the accuracy and determinism checks: a small LCG, so every machine sees the same list
    std::vector<float> sweep(int n, float lo, float hi)
    {
        std::vector<float> v;
        uint32_t s = 12345u;
        for (int i = 0; i < n; ++i)
        {
            s = s * 1664525u + 1013904223u;
            v.push_back(lo + (hi - lo) * float(s >> 8) / float(1u << 24));
        }
        return v;
    }
}

TEST(detmath_sin_cos_equal_retail_bit_for_bit)
{
    int bad = 0;
    for (const wotm::TrigGolden &g : wotm::kTrigGoldens)
        bad += bitsOf(dm::sinf(floatOf(g.x))) != g.sin || bitsOf(dm::cosf(floatOf(g.x))) != g.cos;
    CHECK(bad == 0);
}

TEST(detmath_sin_cos_within_one_ulp)
{
    uint32_t worst = 0;
    for (float x : sweep(20000, -1000.f, 1000.f))
    {
        worst = std::max(worst, ulps(dm::sinf(x), float(std::sin(double(x)))));
        worst = std::max(worst, ulps(dm::cosf(x), float(std::cos(double(x)))));
    }
    CHECK(worst <= 1);
}

TEST(detmath_atan2_special_cases)
{
    const float pi = floatOf(0x40490FDBu), halfPi = floatOf(0x3FC90FDBu);
    CHECK(bitsOf(dm::atan2f(0.f, 1.f)) == 0u);
    CHECK(bitsOf(dm::atan2f(-0.f, 1.f)) == 0x80000000u);
    CHECK(dm::atan2f(0.f, -1.f) == pi);
    CHECK(dm::atan2f(-0.f, -1.f) == -pi);
    CHECK(dm::atan2f(1.f, 0.f) == halfPi);
    CHECK(dm::atan2f(-1.f, 0.f) == -halfPi);
    CHECK(dm::atan2f(5.f, -0.f) == halfPi);
}

TEST(detmath_atan2_within_one_ulp)
{
    const std::vector<float> ys = sweep(20000, -500.f, 500.f), xs = sweep(20001, -500.f, 500.f);
    uint32_t worst = 0;
    for (size_t i = 0; i < ys.size(); ++i)
        worst = std::max(worst, ulps(dm::atan2f(ys[i], xs[i + 1]), float(std::atan2(double(ys[i]), double(xs[i + 1])))));
    CHECK(worst <= 1);
}

// The point of wotm_detmath.hpp: the same bits on every compiler and CPU. The expected digest was measured on the Mac (clang,
// arm64); Windows (MSVC, x64) must print the same, or its build computes the camera differently and a network match desyncs.
TEST(detmath_same_bits_on_every_machine)
{
    uint64_t h = 1469598103934665603ull;   // FNV-1a over the result bits
    auto mix = [&h](float f) {
        const uint32_t u = bitsOf(f);
        for (int k = 0; k < 4; ++k)
            h = (h ^ ((u >> (8 * k)) & 0xFFu)) * 1099511628211ull;
    };
    const std::vector<float> a = sweep(50000, -20000.f, 20000.f), b = sweep(50001, -3.f, 3.f);
    for (size_t i = 0; i < a.size(); ++i)
    {
        mix(dm::sinf(a[i]));
        mix(dm::cosf(a[i]));
        mix(dm::atan2f(a[i], b[i + 1]));
        mix(dm::atan2f(b[i], b[i + 1]));
    }
    std::printf("  detmath digest %016llx\n", static_cast<unsigned long long>(h));
    CHECK(h == 0xa6c8104174bda8ccull);
}

// The orbit (and the F5 free camera) write these axes into the game camera's matrix, which then decides culling, LOD and
// particles: they must come from wotm::detmath, not the system libm, or Windows and the Mac drift apart (stage 2, frame 1003).
TEST(freecam_axes_use_deterministic_trig)
{
    int bad = 0;
    const std::vector<float> hs = sweep(5000, -7.f, 7.f), ps = sweep(5001, -1.5f, 1.5f);
    for (size_t i = 0; i < hs.size(); ++i)
    {
        WotmFreeCamPose p;
        p.heading = hs[i];
        p.pitch = ps[i + 1];
        float r[3], f[3], u[3];
        wotmFreeCamAxes(p, r, f, u);
        const float sh = dm::sinf(p.heading), ch = dm::cosf(p.heading), sp = dm::sinf(p.pitch), cp = dm::cosf(p.pitch);
        const float want[9] = {ch, -sh, 0.f, cp * sh, cp * ch, sp, -sp * sh, -sp * ch, cp};
        const float got[9] = {r[0], r[1], r[2], f[0], f[1], f[2], u[0], u[1], u[2]};
        for (int k = 0; k < 9; ++k)
            bad += bitsOf(got[k]) != bitsOf(want[k]);
    }
    CHECK(bad == 0);
}

TEST(tint_identity_and_hue)
{
    using wotm::tint::shift;
    std::vector<uint32_t> px = {0xFF0000FFu, 0x80FF0000u, 0x00808080u, 0xC0123456u};   // A B G R per byte, little endian: R | G<<8 | B<<16 | A<<24
    std::vector<uint32_t> same = px;
    shift(same, 0.f, 0.f, 0.f);
    CHECK(same == px);   // all zero is the identity, bit for bit
    std::vector<uint32_t> hue = {0xFF0000FFu};   // pure red, alpha 255
    shift(hue, 120.f, 0.f, 0.f);
    CHECK(hue[0] == 0xFF00FF00u);   // pure green, alpha kept
    std::vector<uint32_t> back = {0xFF0000FFu};
    shift(back, -240.f, 0.f, 0.f);
    CHECK(back[0] == 0xFF00FF00u);   // wraps
    std::vector<uint32_t> gray = {0x80FF0000u};   // pure blue, alpha 0x80
    shift(gray, 0.f, -100.f, 0.f);
    CHECK(gray[0] == 0x80FFFFFFu);   // saturation gone: white at the same value, alpha kept
    std::vector<uint32_t> bright = {0xFF0000C0u};
    shift(bright, 0.f, 0.f, 100.f);
    CHECK(bright[0] == 0xFF0000FFu);   // clamps at 255
    std::vector<uint32_t> dark = {0xFF0000C0u};
    shift(dark, 0.f, 0.f, -50.f);
    CHECK(dark[0] == 0xFF000060u);
    std::vector<uint32_t> turn = px;
    shift(turn, 360.f, 0.f, 0.f);
    CHECK(turn == px);   // a full turn goes through HSV and back, bit for bit
    std::vector<uint32_t> grn = {0xFF00FF80u};   // R 0x80, G 0xFF, B 0: hue 90 (green max), s 1, v 1
    shift(grn, 120.f, 0.f, 0.f);
    CHECK(grn[0] == 0xFFFF8000u);   // hue 210: R 0, G 0x80, B 0xFF
}

TEST(creator_id_from_name)
{
    using wotm::creator::idFromName;
    CHECK(idFromName("TOGERA 2") == "togera_2");
    CHECK(idFromName("  Green!! Togera  ") == "green_togera");
    CHECK(idFromName("") == "skin");
    CHECK(idFromName("---") == "skin");
    CHECK(idFromName(std::string(40, 'A')).size() == 32);
    CHECK(idFromName(std::string(31, 'a') + " bcd") == std::string(31, 'a'));
}

TEST(creator_env_parse)
{
    using wotm::creator::Script;
    using wotm::creator::parseScript;
    Script s;
    std::string err;
    CHECK(parseScript("togera:1:green:120", s, err) && s.monster == 3 && s.costume == 1 && s.id == "green" && s.hue == 120.f && s.sat == 0.f && s.bright == 0.f);
    CHECK(parseScript("ultra-v:3:x", s, err) && s.monster == 4 && s.hue == 0.f);
    CHECK(parseScript("congar:0:blue:-90:50:-20", s, err) && s.sat == 50.f && s.bright == -20.f);
    CHECK(!parseScript("bogus:1:x", s, err) && err.find("monster") != std::string::npos);
    CHECK(!parseScript("togera:9:x", s, err) && err.find("costume") != std::string::npos);
    CHECK(!parseScript("togera:1", s, err));
    CHECK(!parseScript("togera:1:Bad Id", s, err) && err.find("id") != std::string::npos);
    CHECK(!parseScript("togera:1:x:nan", s, err) && err.find("number") != std::string::npos);
}

TEST(creator_texture_groups)
{
    using wotm::creator::TexKey;
    using wotm::creator::groupTextures;
    const auto g = groupTextures({TexKey{4, false}, TexKey{11, true}, TexKey{4, false}, TexKey{4, true}});
    CHECK(g.size() == 3);
    CHECK(g.size() == 3 && g[0] == std::vector<int>({0, 2}) && g[1] == std::vector<int>({1}) && g[2] == std::vector<int>({3}));
    CHECK(groupTextures({}).empty());
}

TEST(creator_skin_json_and_png_names)
{
    const std::string text = wotm::creator::skinJson("togera", "green", "GREEN TOGERA", 1);
    wotm::json::Value v;
    CHECK(wotm::json::parse(text, v) && v.get("monster")->s == "togera" && v.get("id")->s == "green" && v.get("name")->s == "GREEN TOGERA");
    CHECK(v.get("base")->asInt(-1) == 1 && v.get("ai")->asBool(true) == false);
    CHECK(wotm::creator::pngName(4, false) == "t0004.png");
    CHECK(wotm::creator::pngName(11, true) == "rtx_t0011.png");
}

TEST(creator_monster_json)
{
    const std::string text = wotm::creator::monsterJson("kaiju_king", "KAIJU KING", "togera", 1, false, "kaiju_king");
    wotm::json::Value v;
    CHECK(wotm::json::parse(text, v) && v.type == wotm::json::Value::Obj && v.obj.size() == 6);
    CHECK(v.get("id")->s == "kaiju_king" && v.get("name")->s == "KAIJU KING" && v.get("base")->s == "togera");
    CHECK(v.get("costume")->asInt(-1) == 1 && v.get("ai")->type == wotm::json::Value::Bool && !v.get("ai")->b);
    CHECK(v.get("skin")->s == "kaiju_king" && !text.empty() && text.back() == '\n');
}

TEST(creator_monster_script_parse)
{
    using wotm::creator::parseMonsterScript;
    wotm::creator::Script sc;
    std::string err;
    CHECK(parseMonsterScript("togera:1:kaiju:120", sc, err) && sc.monster == 3 && sc.costume == 1 && sc.id == "kaiju" && sc.hue == 120.f);
    for (const char *bad : {"togera:1", "togera:7:x", "assboss:1:x", "togera:1:x:nan"}) {
        err.clear();
        CHECK(!parseMonsterScript(bad, sc, err) && !err.empty());
    }
}

TEST(creator_portrait_tex)
{
    using wotm::creator::portraitTex;
    CHECK(portraitTex("LIZARD", 0) == 3 && portraitTex("LIZARD", 1) == 3 && portraitTex("LIZARD", 2) == 3 && portraitTex("LIZARD", 3) == 4);
    CHECK(portraitTex("SHOGUN", 0) == 11 && portraitTex("SHOGUN", 1) == 10 && portraitTex("SHOGUN", 2) == 11 && portraitTex("SHOGUN", 3) == 11);
    CHECK(portraitTex("ASSBOSS", 1) == -1 && portraitTex("LIZARD", -1) == -1 && portraitTex("LIZARD", 4) == -1);
}

TEST(creator_raw_alpha_round_trip)
{
    // A hiAlpha texture's PNG gets the disc's raw alpha (applyRawAlpha). Small mirrors of the formulas involved:
    // what the preview shows, the fixA lambda in Model::decodeOrig (wotm_model.hpp); what the fight reads, texmods::loadPng with
    // rawAlpha and hiAlpha (wotm_texmods.hpp: the PNG's alpha as is); what the model path reads, texmods::loadPng without rawAlpha
    // (min(255, 2a)).
    auto fixA = [](uint32_t r) { return r >= 128 ? 255u : r * 2; };
    auto fightRead = [](uint32_t a) { return a; };
    auto modelRead = [](uint32_t a) { return std::min(255u, a * 2); };
    std::vector<uint32_t> px;
    std::vector<uint8_t> raw;
    for (uint32_t r = 0; r < 256; ++r) {
        px.push_back(fixA(r) << 24 | 0x00123456u);   // the preview's pixel (tint::shift leaves alpha alone)
        raw.push_back(uint8_t(r));
    }
    CHECK(wotm::creator::applyRawAlpha(px, raw));
    bool fight = true, model = true, rgb = true;
    for (uint32_t r = 0; r < 256; ++r) {
        const uint32_t a = px[r] >> 24;
        fight &= fightRead(a) == r;          // the GS alpha the disc texture had
        model &= modelRead(a) == fixA(r);    // the selection and the preview draw what they drew before the save
        rgb &= (px[r] & 0x00FFFFFFu) == 0x00123456u;
    }
    CHECK(fight);
    CHECK(model);
    CHECK(rgb);
    std::vector<uint8_t> shortRaw(3, 0x40);
    CHECK(!wotm::creator::applyRawAlpha(px, shortRaw) && (px[200] >> 24) == 200u);   // size mismatch: untouched
}

TEST(mods_add_skin_after_scan)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_addskin";
    fs::remove_all(root);
    fs::create_directories(root / "Creator" / "skins" / "green");
    {
        std::ofstream o(root / "Creator" / "skins" / "green" / "skin.json");
        o << wotm::creator::skinJson("togera", "green", "GREEN TOGERA", 1);
        std::ofstream p(root / "Creator" / "skins" / "green" / "t0004.png");
        p << "not a real png";
    }
    wotm::mods::Registry reg;   // a fresh one, not get(): the singleton stays untouched
    reg.scan(root / "nothing_here");   // ready, empty
    CHECK(reg.ready() && reg.skins().empty());
    std::string why;
    const wotm::mods::Skin *k = reg.addSkin(root / "Creator" / "skins" / "green", why);
    CHECK(k && k->monster == 3 && k->id == "green" && k->index == 4 && k->base == 1 && k->mod == "Creator");
    CHECK(reg.skinByIndex(3, 4) == k);
    CHECK(reg.aliasOf("MON/LIZARD4.NGP") == "MON/LIZARD1.NGP");
    CHECK(reg.skinPngs().size() == 1 && reg.skinPngs()[0].stem == "LIZARD4");
    const wotm::mods::Skin *again = reg.addSkin(root / "Creator" / "skins" / "green", why);   // overwrite: same index
    CHECK(again && again->index == 4 && reg.skins().size() == 1 && reg.skinPngs().size() == 1);
    CHECK(again == k);   // overwritten in place
    // the same folder written another way is still an overwrite; the same monster/id from another folder is refused
    CHECK(reg.addSkin(root / "Creator" / "skins" / "." / "green" / "", why) == k && reg.skins().size() == 1);
    fs::create_directories(root / "Other" / "skins" / "green");
    { std::ofstream o(root / "Other" / "skins" / "green" / "skin.json"); o << wotm::creator::skinJson("togera", "green", "X", 0); }
    CHECK(!reg.addSkin(root / "Other" / "skins" / "green", why) && why.find("Creator") != std::string::npos && reg.skins().size() == 1);
    // a second skin of the same monster takes the next index; the first one stays
    fs::create_directories(root / "Creator" / "skins" / "blue");
    { std::ofstream o(root / "Creator" / "skins" / "blue" / "skin.json"); o << wotm::creator::skinJson("togera", "blue", "BLUE", 2); }
    const wotm::mods::Skin *blue = reg.addSkin(root / "Creator" / "skins" / "blue", why);
    CHECK(blue && blue->index == 5 && reg.skinByIndex(3, 4) && reg.skinByIndex(3, 4)->id == "green");
    CHECK(reg.aliasOf("MON/LIZARD5.TEX") == "MON/LIZARD2.TEX" && reg.aliasOf("MON/LIZARD4.TEX") == "MON/LIZARD1.TEX");
    fs::create_directories(root / "Creator" / "skins" / "bad");
    CHECK(!reg.addSkin(root / "Creator" / "skins" / "bad", why) && why.find("skin.json") != std::string::npos);
    fs::remove_all(root);
}

TEST(mods_custom_skin_first)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_customskin";
    // a custom kaiju (togera, costume 1) with one skin of its own; monster.json's "skin" given as `skin` (empty: no key)
    auto costumes = [&](const std::string &skin) {
        fs::remove_all(root);
        fs::create_directories(root / "Creator" / "monsters" / "kaiju");
        fs::create_directories(root / "Creator" / "skins" / "kaiju");
        {
            std::ofstream o(root / "Creator" / "monsters" / "kaiju" / "monster.json");
            o << (skin.empty() ? std::string(R"({"id": "kaiju", "name": "KAIJU", "base": "togera", "costume": 1, "ai": false})")
                               : wotm::creator::monsterJson("kaiju", "KAIJU", "togera", 1, false, skin));
            std::ofstream k(root / "Creator" / "skins" / "kaiju" / "skin.json");
            k << wotm::creator::skinJson("kaiju", "kaiju", "KAIJU", 1);
            std::ofstream p(root / "Creator" / "skins" / "kaiju" / "t0004.png");
            p << "not a real png";
        }
        wotm::mods::Registry reg;
        reg.scan(root);
        const wotm::mods::Custom *cu = reg.custom("kaiju");
        const wotm::mods::Skin *k = reg.skin(3, "kaiju/kaiju");
        std::vector<int> v;
        if (cu && k) v = reg.customCostumes(*cu);
        return std::make_pair(v, k ? k->index : -1);
    };
    const auto own = costumes("KaiJu");   // lowercased like the ids
    CHECK(own.second == 4 && own.first == std::vector<int>({4, 1}));
    const auto none = costumes("");
    CHECK(none.second == 4 && none.first == std::vector<int>({1, 4}));
    const auto unknown = costumes("nope");   // logs: "skin" nope is not one of its skins
    CHECK(unknown.second == 4 && unknown.first == std::vector<int>({1, 4}));
    fs::remove_all(root);
}

TEST(mods_add_custom_after_scan)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_addcustom";
    fs::remove_all(root);
    fs::create_directories(root / "Base" / "monsters" / "first");
    { std::ofstream o(root / "Base" / "monsters" / "first" / "monster.json"); o << wotm::creator::monsterJson("first", "FIRST", "congar", 0, true, ""); }
    wotm::mods::Registry reg;   // a fresh one, not get(): the singleton stays untouched
    reg.scan(root);   // one custom from the scan, none from Creator
    CHECK(reg.customs().size() == 1 && !reg.custom("kaiju"));
    // the creator saves kaiju (togera, costume 1) and its skin
    fs::create_directories(root / "Creator" / "monsters" / "kaiju");
    fs::create_directories(root / "Creator" / "skins" / "kaiju");
    {
        std::ofstream o(root / "Creator" / "monsters" / "kaiju" / "monster.json");
        o << wotm::creator::monsterJson("kaiju", "KAIJU", "togera", 1, false, "kaiju");
        std::ofstream k(root / "Creator" / "skins" / "kaiju" / "skin.json");
        k << wotm::creator::skinJson("kaiju", "kaiju", "KAIJU", 1);
    }
    std::string why;
    const wotm::mods::Custom *cu = reg.addCustom(root / "Creator" / "monsters" / "kaiju", why);
    CHECK(cu && cu->id == "kaiju" && cu->name == "KAIJU" && cu->type == 3 && cu->costume == 1 && cu->mod == "Creator" && cu->skin == "kaiju");
    CHECK(reg.custom("kaiju") == cu && reg.customs().size() == 2 && reg.customs()[0].id == "first" && reg.customs()[1].id == "kaiju");
    const wotm::mods::Skin *k = reg.addSkin(root / "Creator" / "skins" / "kaiju", why);
    CHECK(k && k->custom == "kaiju" && k->monster == 3 && k->index == 4);
    CHECK(cu && reg.customCostumes(*cu) == std::vector<int>({4, 1}));   // its "skin" first
    // saved again (renamed, another costume): updated in place, same position
    { std::ofstream o(root / "Creator" / "monsters" / "kaiju" / "monster.json"); o << wotm::creator::monsterJson("kaiju", "KAIJU II", "togera", 2, true, ""); }
    const wotm::mods::Custom *again = reg.addCustom(root / "Creator" / "monsters" / "." / "kaiju" / "", why);
    CHECK(again == cu && reg.customs().size() == 2 && reg.customs()[1].name == "KAIJU II" && reg.customs()[1].costume == 2);
    const std::vector<wotm::mods::Custom> copy = reg.customsCopy();
    CHECK(copy.size() == 2 && copy[1].name == "KAIJU II");
    // the same id from another mod's folder is refused, naming the mod that has it
    fs::create_directories(root / "Other" / "monsters" / "kaiju");
    { std::ofstream o(root / "Other" / "monsters" / "kaiju" / "monster.json"); o << wotm::creator::monsterJson("kaiju", "X", "congar", 0, false, ""); }
    CHECK(!reg.addCustom(root / "Other" / "monsters" / "kaiju", why) && why.find("Creator") != std::string::npos);
    CHECK(reg.customs().size() == 2 && reg.custom("kaiju") && reg.custom("kaiju")->name == "KAIJU II");
    // an id the boot scan registered from another mod is refused too
    fs::create_directories(root / "Other" / "monsters" / "first");
    { std::ofstream o(root / "Other" / "monsters" / "first" / "monster.json"); o << wotm::creator::monsterJson("first", "Y", "congar", 0, false, ""); }
    CHECK(!reg.addCustom(root / "Other" / "monsters" / "first", why) && why.find("Base") != std::string::npos);
    CHECK(reg.customs().size() == 2 && reg.custom("first") && reg.custom("first")->name == "FIRST");
    // a re-save that changes the base is refused live (its skins keep indices of the old type)
    { std::ofstream o(root / "Creator" / "monsters" / "kaiju" / "monster.json"); o << wotm::creator::monsterJson("kaiju", "KAIJU C", "congar", 0, false, ""); }
    why.clear();
    CHECK(!reg.addCustom(root / "Creator" / "monsters" / "kaiju", why) && !why.empty());
    CHECK(reg.custom("kaiju") && reg.custom("kaiju")->type == 3 && reg.custom("kaiju")->name == "KAIJU II");
    // an invalid monster.json changes nothing
    { std::ofstream o(root / "Creator" / "monsters" / "kaiju" / "monster.json"); o << R"({"id": "kaiju", "base": "nope"})"; }
    CHECK(!reg.addCustom(root / "Creator" / "monsters" / "kaiju", why) && why.find("base") != std::string::npos);
    CHECK(reg.customs().size() == 2 && reg.custom("kaiju") && reg.custom("kaiju")->name == "KAIJU II");
    fs::create_directories(root / "Creator" / "monsters" / "empty");
    CHECK(!reg.addCustom(root / "Creator" / "monsters" / "empty", why) && why.find("monster.json") != std::string::npos && reg.customs().size() == 2);
    fs::remove_all(root);
}

TEST(mods_custom_of_type_and_costume)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_customof";
    fs::remove_all(root);
    auto write = [&](const fs::path &rel, const std::string &text) {
        fs::create_directories((root / rel).parent_path());
        std::ofstream(root / rel) << text;
    };
    // a Togera-based custom with a camera and a skin of its own (no "skin" key), a creator custom on Togera with its "skin", a
    // plain Togera skin and Goliath
    write("Old/monsters/modded/monster.json", R"({"id": "modded", "name": "MODDED", "base": "togera", "costume": 0, "camera": {"from": "lizard", "scale": 150}})");
    write("Old/skins/pink/skin.json", wotm::creator::skinJson("modded", "pink", "PINK", 0));
    write("Old/skins/green/skin.json", wotm::creator::skinJson("togera", "green", "GREEN", 2));
    write("Old/monsters/goliath/monster.json", R"({"id": "goliath", "name": "GOLIATH PRIME", "base": "assboss", "costume": 1})");
    write("Creator/monsters/kaiju_king/monster.json", wotm::creator::monsterJson("kaiju_king", "KAIJU KING", "togera", 1, false, "kaiju_king"));
    write("Creator/skins/kaiju_king/skin.json", wotm::creator::skinJson("kaiju_king", "kaiju_king", "KAIJU KING", 1));
    wotm::mods::Registry reg;
    reg.scan(root);
    const wotm::mods::Skin *pink = reg.skin(3, "modded/pink"), *green = reg.skin(3, "togera/green"), *own = reg.skin(3, "kaiju_king/kaiju_king");
    CHECK(pink && green && own && reg.customs().size() == 3);
    const wotm::mods::Custom *modded = reg.custom("modded"), *kaiju = reg.custom("kaiju_king"), *goliath = reg.custom("goliath");
    if (pink && green && own && modded && kaiju && goliath)
    {
        CHECK(reg.customOf(3, own->index) == kaiju && reg.customOf(3, pink->index) == modded);   // on one of its skins
        CHECK(reg.customOf(3, 1) == kaiju && reg.customOf(3, 0) == modded);                    // on its monster.json costume
        CHECK(!reg.customOf(3, 2) && !reg.customOf(3, green->index) && !reg.customOf(1, own->index));   // plain monsters
        CHECK(reg.customOf(13, 1) == goliath && !reg.customOf(13, 0));
        CHECK(reg.customByType(3) == kaiju);   // the first of its type (customs are ordered by id): it used to hide modded's camera
    }
    CHECK(wotm::mods::hudName("KAIJU KING") == "kaiju king" && wotm::mods::hudName("GOLIATH PRIME") == "goliath");
    CHECK(wotm::mods::hudName("ABCDEFGHIJKLMNO") == "abcdefghijk" && wotm::mods::hudName("Kaiju") == "kaiju");
    fs::remove_all(root);
}

int main()
{
    for (const Test &t : tests())
    {
        const int before = g_failures;
        t.run();
        std::printf("%s %s\n", g_failures == before ? "ok     " : "FAILED ", t.name);
    }
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
