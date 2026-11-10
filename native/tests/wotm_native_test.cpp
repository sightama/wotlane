// ABOUTME: Unit tests for the pure parts of the native layer (no game, no window): frame interpolation from wotm_interp.hpp (matrices,
// ABOUTME: camera, cuts, projection), the settings profile for network matches, the mod digest and the map creator's stagemap. Build and run: tools\test_native.bat
#include "wotm_interp.hpp"
#include "wotm_settings.hpp"
#include "wotm_mods.hpp"
#include "wotm_detmath.hpp"
#include "wotm_freecam.h"
#include "wotm_trig_goldens.hpp"
#include "net/wotm_modifiers.hpp"
#include "wotm_tint.hpp"
#include "wotm_creator.hpp"
#include "wotm_stagemap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <set>
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

// Saved maps (mods/<mod>/stages/<id>/stage.json): listed by Registry::stages(), never in the disc override list; a bad one is
// logged with its reason; the same id in a later mod loses to the first one
const char *const kStageJson =
    R"({"name": "DOWNTOWN MESS", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [{"node": "0x100", "remove": true}, {"node": "0x200", "turn": 90}]})";
TEST(mods_scan_stages)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_stages";
    auto put = [](const fs::path &p, const std::string &s) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << s;
    };
    fs::remove_all(root);
    put(root / "A" / "stages" / "m1" / "stage.json", kStageJson);
    put(root / "A" / "stages" / "m1" / "photo.png", "not a real png");
    put(root / "A" / "stages" / "bad" / "stage.json", R"({"name": "X", "base": "CENTRAL", "edits": []})");
    put(root / "A" / "stages" / "9lives" / "stage.json", kStageJson);   // id must start with a letter
    put(root / "A" / "stages" / "abcdefghijklmnopq" / "stage.json", kStageJson);   // 17 chars: too long
    put(root / "B" / "Stages" / "M1" / "stage.json", R"({"name": "LOSER", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": []})");
    put(root / "C" / "Stages" / "M2" / "stage.json", kStageJson);   // mixed case: lists as m2
    put(root / "A" / "stages" / "m3" / "stage.json",
        R"({"name": "COPY TOWN", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [{"copy": "0x100", "at": [1, 2, 3]},
            {"node": "0x200", "turn": 90}, {"addpowerup": 1, "from": [0, 0, 0], "at": [1, 1, 1]}, {"copy": "0x100", "at": [4, 5, 6], "turn": 45}]})");
    put(root / "A" / "MON" / "LIZARD1.TEX", "tex");   // a real override, so the file list is not empty
    wotm::mods::Registry reg;
    reg.scan(root);
    const std::vector<wotm::mods::Stage> st = reg.stages();
    CHECK(st.size() == 3);
    if (st.size() == 3)
    {
        CHECK(st[1].map.id == "m2" && st[1].mod == "C");
        CHECK(st[0].map.id == "m1" && st[0].map.name == "DOWNTOWN MESS" && st[0].map.base == "CENTRAL" && st[0].map.edits.size() == 2);
        CHECK(fs::equivalent(st[0].map.dir, root / "A" / "stages" / "m1") && st[0].mod == "A");
        CHECK(!st[0].photo.empty() && st[0].photo.filename() == "photo.png");
    }
    auto logged = [&](const std::string &a, const std::string &b) {
        for (const std::string &l : reg.stageLog())
            if (l.find(a) != std::string::npos && l.find(b) != std::string::npos) return true;
        return false;
    };
    CHECK(logged("A/stages/bad ignored", "baseHash"));
    CHECK(logged("A/stages/9lives ignored", "start with a letter"));
    CHECK(logged("A/stages/abcdefghijklmnopq ignored", "up to 16"));
    CHECK(logged("map m1: A beats B", ""));
    CHECK(logged("[stages] map m1 (\"DOWNTOWN MESS\", A): base CENTRAL, 2 edits", ""));
    CHECK(logged("[stages] map m3 (\"COPY TOWN\", A): base CENTRAL, 1 edits, 2 copies, 1 new power-up places", ""));
    CHECK(!logged("[stages] map m1 (", "copies"));
    CHECK(reg.files().count("MON/LIZARD1.TEX") == 1);
    bool stagesInFiles = false;
    for (const auto &[rel, f] : reg.files())
        if (rel.rfind("STAGES/", 0) == 0) stagesInFiles = true;
    CHECK(!stagesInFiles);
    fs::remove_all(root);
}

// The network match's mod digest covers stage.json: a changed byte changes it
TEST(mods_digest_includes_stages)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_stage_digest";
    auto put = [](const fs::path &p, const std::string &s) {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << s;
    };
    fs::remove_all(root);
    std::string text = kStageJson;
    put(root / "A" / "stages" / "m1" / "stage.json", text);
    const uint64_t before = wotm::mods::digest(root);
    CHECK(before != 0);
    text[text.find("0x200") + 4] = '4';   // node 0x200 -> 0x204
    put(root / "A" / "stages" / "m1" / "stage.json", text);
    CHECK(wotm::mods::digest(root) != before);
    fs::remove_all(root);
}

TEST(json_numbers_keep_fractions)
{
    wotm::json::Value v;
    CHECK(wotm::json::parse("[1.5, -0.25, 3, 1e3, 2.5E-2, 0.1, 120.0]", v) && v.arr.size() == 7);
    if (v.arr.size() == 7)
    {
        CHECK(v.arr[0].asNum() == 1.5 && v.arr[0].i == 1);   // integer readers still see the truncated value
        CHECK(v.arr[1].asNum() == -0.25 && v.arr[2].asNum() == 3.0 && !v.arr[2].frac);
        CHECK(v.arr[3].asNum() == 1000.0 && v.arr[4].asNum() == 0.025 && v.arr[5].asNum() == 0.1);
        CHECK(v.arr[6].frac && v.arr[6].asNum() == 120.0);
    }
    CHECK(wotm::json::dump(wotm::json::Value::number(0.1)) == "0.1\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(2.0)) == "2.0\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(-1234.5)) == "-1234.5\n");
    // plain decimal while the exponent is small, whole numbers keep ".0" (stage_map.py writes 120.0), -0 stays -0
    CHECK(wotm::json::dump(wotm::json::Value::number(120.0)) == "120.0\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(-40.0)) == "-40.0\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(10.0)) == "10.0\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(1234567.0)) == "1234567.0\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(0.00001)) == "0.00001\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(1e-7)) == "1e-07\n");
    CHECK(wotm::json::dump(wotm::json::Value::number(-0.0)) == "-0.0\n");
    CHECK(wotm::json::parse("-0.0", v) && v.asNum() == 0.0 && std::signbit(v.asNum()));
    CHECK(!wotm::json::parse("1.", v) && !wotm::json::parse("1e", v));
}

namespace
{
    namespace sm = wotm::stagemap;

    // Synthetic stage image for the stagemap tests: the header (root count + roots) at kBase, nodes 16-aligned after it, and a
    // separate buffer mapped at the reserve. Layouts as include/hieri_types.h.
    struct Img
    {
        std::vector<uint8_t> img, res;
        uint32_t top = sm::kBase + 0x100;
        explicit Img(uint32_t imgSize = 0x8000, uint32_t resSize = 0x400) : img(imgSize), res(resSize) {}

        uint8_t *at(uint32_t a) { return a >= sm::kReserve ? &res[a - sm::kReserve] : &img[a - sm::kBase]; }
        void w32(uint32_t a, uint32_t v) { std::memcpy(at(a), &v, 4); }
        void w16(uint32_t a, uint16_t v) { std::memcpy(at(a), &v, 2); }
        void wf(uint32_t a, float v) { std::memcpy(at(a), &v, 4); }
        uint32_t r32(uint32_t a) { uint32_t v; std::memcpy(&v, at(a), 4); return v; }
        uint16_t r16(uint32_t a) { uint16_t v; std::memcpy(&v, at(a), 2); return v; }
        float rf(uint32_t a) { float v; std::memcpy(&v, at(a), 4); return v; }

        uint32_t alloc(uint32_t size)
        {
            const uint32_t a = top;
            top += (size + 15) & ~15u;
            return a;
        }
        uint32_t node(uint32_t op, uint32_t size, uint32_t id1 = 0)
        {
            const uint32_t a = alloc(size);
            w32(a, id1 << 18 | op);
            return a;
        }
        void kids(uint32_t a, const std::vector<uint32_t> &ks)
        {
            for (size_t i = 0; i < ks.size(); ++i)
                w32(a + 4 * uint32_t(i), ks[i]);
        }
        void sphere(uint32_t a, float x, float y, float z, float r)
        {
            wf(a + 0xC, r * r), wf(a + 0x10, x), wf(a + 0x14, y), wf(a + 0x18, z), wf(a + 0x1C, r);
        }
        uint32_t object(uint32_t id1 = 0, float x = 0, float y = 0, float z = 0, float r = 10)
        {
            const uint32_t a = node(0, 0x20, id1);
            sphere(a, x, y, z, r);
            wf(a + 8, r * r);   // an OBJECT keeps radiusSqrd at +8
            wf(a + 0xC, 0);
            return a;
        }
        // an OBJECT at its full size (0xA0) whose animation channels (+0x30..+0x3C) are all -1: it reads none
        uint32_t stillObject(float x = 0, float y = 0, float z = 0, float r = 10)
        {
            const uint32_t a = node(0, 0xA0);
            sphere(a, x, y, z, r);
            wf(a + 8, r * r);
            wf(a + 0xC, 0);
            for (uint32_t k = 0; k < 7; ++k)
                w16(a + 0x30 + 2 * k, 0xFFFF);
            return a;
        }
        uint32_t polycoll() { return node(42, 0x10); }
        // INTERACTIVE_STATE: child +0xC
        uint32_t istate(uint32_t child, uint32_t id1 = 0)
        {
            const uint32_t a = node(39, 0x10, id1);
            w32(a + 0xC, child);
            return a;
        }
        // PARTICLE_EMITTER (0xB0 bytes, no children)
        uint32_t emitter(uint32_t id1 = 0) { return node(22, 0xB0, id1); }
        // SWITCH: numKids u8 +0xB, child[] +0xC
        uint32_t switchNode(const std::vector<uint32_t> &ks)
        {
            const uint32_t a = node(6, 0xC + 4 * uint32_t(ks.size()));
            *at(a + 0xB) = uint8_t(ks.size());
            kids(a + 0xC, ks);
            return a;
        }
        // actions (ActHead: type u16 at +0)
        uint32_t act(uint16_t type, uint32_t size)
        {
            const uint32_t a = alloc(size);
            w32(a, type);
            return a;
        }
        // ACT_DEBRIS {numChunks +8, chunks[] +0x10 of 0x30}; a chunk's pickup type is bits 5..9 of its u16 at +0xE (kind 1 in
        // bits 0..2, damage 3 in bits 10..15 around it, so the field is read alone)
        uint32_t debris(const std::vector<uint32_t> &pickupTypes)
        {
            const uint32_t a = act(sm::kActDebris, 0x10 + 0x30 * uint32_t(pickupTypes.size()));
            w32(a + 8, uint32_t(pickupTypes.size()));
            for (size_t i = 0; i < pickupTypes.size(); ++i)
                w16(a + 0x10 + 0x30 * uint32_t(i) + 0xE, uint16_t(1 | pickupTypes[i] << 5 | 3u << 10));
            return a;
        }
        uint32_t hat(float r, float x, float y, float z)
        {
            const uint32_t a = act(sm::kActHatPickup, 0x18);
            wf(a + 8, r), wf(a + 0xC, x), wf(a + 0x10, y), wf(a + 0x14, z);
            return a;
        }
        // ACT_DOMINO {state +8, target +0xC}
        uint32_t domino(uint32_t target, uint32_t state)
        {
            const uint32_t a = act(sm::kActDomino, 0x10);
            w32(a + 8, state), w32(a + 0xC, target);
            return a;
        }
        // ACT_GROUP {count +8, acts[] +0xC}
        uint32_t actGroup(const std::vector<uint32_t> &acts)
        {
            const uint32_t a = act(sm::kActGroup, 0xC + 4 * uint32_t(acts.size()));
            w32(a + 8, uint32_t(acts.size()));
            kids(a + 0xC, acts);
            return a;
        }
        // DESTRUCTIBLE: numKids u16 +0xA, states[] +0xC of 12 bytes (child +0, actions +4)
        uint32_t destructible(const std::vector<std::pair<uint32_t, uint32_t>> &states)   // (child, actions)
        {
            const uint32_t d = node(31, 0xC + 12 * uint32_t(states.size()));
            w16(d + 0xA, uint16_t(states.size()));
            for (size_t i = 0; i < states.size(); ++i)
                w32(d + 0xC + 12 * uint32_t(i), states[i].first), w32(d + 0x10 + 12 * uint32_t(i), states[i].second);
            return d;
        }
        uint32_t group(const std::vector<uint32_t> &ks, float x = 0, float y = 0, float z = 0, float r = 20, uint32_t id1 = 0)
        {
            const uint32_t a = node(1, 0x20 + 4 * uint32_t(ks.size()), id1);
            w16(a + 8, uint16_t(ks.size()));
            w16(a + 0xA, 1);
            sphere(a, x, y, z, r);
            kids(a + 0x20, ks);
            return a;
        }
        uint32_t translate(const std::vector<uint32_t> &ks, float x, float y, float z, uint32_t id1 = 0)
        {
            const uint32_t a = node(3, 0x1C + 4 * uint32_t(ks.size()), id1);
            w32(a + 8, uint32_t(ks.size()));
            wf(a + 0x10, x), wf(a + 0x14, y), wf(a + 0x18, z);
            kids(a + 0x1C, ks);
            return a;
        }
        uint32_t rotate(const std::vector<uint32_t> &ks, float deg, float x, float y, float z)
        {
            const uint32_t a = node(4, 0x50 + 4 * uint32_t(ks.size()));
            w32(a + 8, uint32_t(ks.size()));
            const M4 m = rotZ(deg, 1.f, 1.f, 1.f, x, y, z);
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    wf(a + 0x10 + 16 * r + 4 * c, m.m[r][c]);
            kids(a + 0x50, ks);
            return a;
        }
        uint32_t holder(uint32_t op, uint32_t countOff, uint32_t kidsOff, const std::vector<uint32_t> &ks)
        {
            const uint32_t a = node(op, kidsOff + 4 * uint32_t(ks.size()));
            w32(a + countOff, uint32_t(ks.size()));
            kids(a + kidsOff, ks);
            return a;
        }
        uint32_t control(uint32_t child1, uint32_t child2)
        {
            const uint32_t a = node(8, 0x20);
            w32(a + 4, child1), w32(a + 8, child2);
            return a;
        }
        uint32_t lod(uint32_t child, float x, float y, float z, float r)
        {
            const uint32_t a = node(2, 0x30);
            w32(a + 4, 1);
            wf(a + 0x10, x), wf(a + 0x14, y), wf(a + 0x18, z), wf(a + 0x1C, r);
            w32(a + 0x28, child);
            return a;
        }
        uint32_t path(float x0, float y0, float z0, float x1, float y1, float z1, const std::vector<uint32_t> &nodes)
        {
            const uint32_t a = alloc(0x24 + 4 * uint32_t(nodes.size()));
            wf(a, x0), wf(a + 4, y0), wf(a + 8, z0), wf(a + 0xC, x1), wf(a + 0x10, y1), wf(a + 0x14, z1);
            w32(a + 0x1C, uint32_t(nodes.size()));
            kids(a + 0x24, nodes);
            return a;
        }
        uint32_t grid(int nc, int nr, float cell, float minx, float miny, float maxx, float maxy)
        {
            const uint32_t a = node(15, 0x20 + 4 * uint32_t(nc * nr));
            w16(a + 4, uint16_t(nc)), w16(a + 6, uint16_t(nr));
            wf(a + 8, cell), wf(a + 0xC, minx), wf(a + 0x10, miny), wf(a + 0x14, maxx), wf(a + 0x18, maxy);
            return a;
        }
        int cols(uint32_t g) { return int(int16_t(r32(g + 4) & 0xFFFF)); }
        int rows(uint32_t g) { return int(int16_t(r32(g + 4) >> 16)); }

        // Cells a path's box covers, written here on its own (floor-inclusive, clamped, row * nCols + col: tools/stage_map.py covered())
        std::vector<int> cover(uint32_t g, uint32_t p)
        {
            const int nc = cols(g), nr = rows(g);
            const double cs = rf(g + 8), mx = rf(g + 0xC), my = rf(g + 0x10);
            auto span = [&](double lo, double hi, double o, int k, int &a, int &b) {
                a = std::max(int(std::floor((lo - o) / cs)), 0);
                b = std::min(int(std::floor((hi - o) / cs)), k - 1);
            };
            int c0, c1, r0, r1;
            span(rf(p), rf(p + 0xC), mx, nc, c0, c1);
            span(rf(p + 4), rf(p + 0x10), my, nr, r0, r1);
            std::vector<int> out;
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c)
                    out.push_back(r * nc + c);
            return out;
        }
        // lists each path in the cells its box covers (list nodes appended after the image's nodes)
        void link(uint32_t g, const std::vector<uint32_t> &paths)
        {
            for (uint32_t p : paths)
                for (int cell : cover(g, p))
                {
                    const uint32_t ln = alloc(8);
                    w32(ln, 0), w32(ln + 4, p);
                    uint32_t slot = g + 0x20 + 4 * uint32_t(cell);
                    while (r32(slot))
                        slot = r32(slot);
                    w32(slot, ln);
                }
        }
        void roots(const std::vector<uint32_t> &rs)
        {
            w32(sm::kBase, uint32_t(rs.size()));
            kids(sm::kBase + 4, rs);
        }
        // (cell, list node) pairs that list path p, in cell order
        std::vector<std::pair<int, uint32_t>> listing(uint32_t g, uint32_t p)
        {
            std::vector<std::pair<int, uint32_t>> out;
            for (int i = 0; i < cols(g) * rows(g); ++i)
                for (uint32_t ln = r32(g + 0x20 + 4 * uint32_t(i)); ln; ln = r32(ln))
                    if (r32(ln + 4) == p)
                        out.push_back({i, ln});
            return out;
        }
        std::vector<int> cellsOf(uint32_t g, uint32_t p)
        {
            std::vector<int> out;
            for (const auto &c : listing(g, p))
                out.push_back(c.first);
            return out;
        }
        // the grid rule: every listed path in exactly the cells its box covers, once per cell, no list node in two cells
        int gridProblems(uint32_t g)
        {
            int bad = 0;
            std::vector<uint32_t> seen, paths;
            for (int i = 0; i < cols(g) * rows(g); ++i)
            {
                std::vector<uint32_t> here;
                for (uint32_t ln = r32(g + 0x20 + 4 * uint32_t(i)); ln; ln = r32(ln))
                {
                    bad += std::find(seen.begin(), seen.end(), ln) != seen.end();
                    seen.push_back(ln);
                    const uint32_t p = r32(ln + 4);
                    bad += std::find(here.begin(), here.end(), p) != here.end();
                    here.push_back(p);
                    if (std::find(paths.begin(), paths.end(), p) == paths.end())
                        paths.push_back(p);
                }
            }
            for (uint32_t p : paths)
                bad += cellsOf(g, p) != cover(g, p);
            return bad;
        }
        sm::Mem mem() { return sm::Mem{{{sm::kBase, img.data(), uint32_t(img.size())}, {sm::kReserve, res.data(), uint32_t(res.size())}}}; }
        sm::Reserve reserve() { return sm::Reserve{sm::kReserve, sm::kReserve + uint32_t(res.size())}; }
    };

    // A 4 x 4 grid of 200-unit cells from (0, 0); piece A (TRANSLATE at 100,100 under a GROUP `mid`), piece B (ROTATE at
    // 500,100, 30 degrees), piece C (TRANSLATE at 700,700), and a ground hull under the world root listed in all 16 cells.
    struct Stage
    {
        Img im;
        uint32_t g, world, mid, tA, gA, objA, pcA, pA, rB, objB, pcB, pB, pcG, pG, tC, pcC, pC;
        Stage()
        {
            objA = im.object(), pcA = im.polycoll();
            gA = im.group({objA, pcA});
            tA = im.translate({gA}, 100, 100, 0, 7);
            im.w32(tA, im.r32(tA) | 3u << 7);   // id2 3 as well: both ids survive edits
            mid = im.group({tA}, 100, 100, 0, 50);
            objB = im.object(), pcB = im.polycoll();
            rB = im.rotate({objB, pcB}, 30, 500, 100, 0);
            pcG = im.polycoll();
            pcC = im.polycoll();
            tC = im.translate({im.object(), pcC}, 700, 700, 0);
            world = im.group({mid, rB, pcG, tC}, 400, 400, 0, 600, 1);
            g = im.grid(4, 4, 200, 0, 0, 800, 800);
            pA = im.path(90, 95, 0, 130, 105, 30, {world, mid, tA, gA, pcA});
            pB = im.path(480, 90, 0, 540, 110, 10, {world, rB, pcB});
            pG = im.path(0, 0, 0, 800, 800, 0, {world, pcG});
            pC = im.path(690, 690, 0, 710, 710, 20, {world, tC, pcC});
            im.link(g, {pA, pB, pG, pC});
            im.roots({world, g});
        }
        uint32_t off(uint32_t a) const { return a - sm::kBase; }
        sm::Result run(std::vector<sm::Edit> edits, sm::Reserve &res)
        {
            sm::Map m;
            m.edits = std::move(edits);
            return sm::apply(im.mem(), uint32_t(im.img.size()), m, res);
        }
        sm::Result run(std::vector<sm::Edit> edits)
        {
            sm::Reserve res = im.reserve();
            return run(std::move(edits), res);
        }
    };

    // The copy tests' stage: a 4 x 4 grid of 200-unit cells from (0, 0).
    //   world (GROUP, id1 1, flags 3) = {mid, tE, oldLast}
    //   piece tD (TRANSLATE 300,300,0) under mid = {i1, i2}: i1 -> d1, state 0 g0 = {sw {obj0, obj1}, pc, em} with actions
    //     a1 = {hat1, dom (-> i2, state 1), domOut (-> iE)}, state 1 obj2 with actions hat1; i2 -> d2 (state 0 an OBJECT)
    //   piece tE (TRANSLATE 700,100,0): iE -> dE, state 0 gE = {OBJECT, pcE}
    //   pickup root proot (GROUP, id1 0) = {holder}, holder = {tP (TRANSLATE 500,500,10) -> gP (GROUP id1 0x12C1) = {swP {OBJECT}}}
    //   paths pD {world, mid, tD, i1, d1, g0, pc} and pE {world, tE, iE, dE, gE, pcE}; roots {world, proot, grid}
    struct CopyStage
    {
        Img im{0x8000, 0x4000};
        uint32_t g, world, mid, oldLast, tD, i1, d1, g0, sw, obj0, obj1, pc, em, a1, hat1, dom, domOut, obj2, i2, d2;
        uint32_t tE, iE, dE, gE, pcE, proot, holder, tP, gP, swP, pD, pE;
        CopyStage()
        {
            pcE = im.polycoll();
            gE = im.group({im.stillObject(), pcE}, 700, 100, 0, 20);
            dE = im.destructible({{gE, 0}});
            iE = im.istate(dE);
            tE = im.translate({iE}, 700, 100, 0);
            d2 = im.destructible({{im.stillObject(), 0}});
            i2 = im.istate(d2);
            hat1 = im.hat(300, 10, 0, 5);
            dom = im.domino(i2, 1);
            domOut = im.domino(iE, 0);
            a1 = im.actGroup({hat1, dom, domOut});
            obj0 = im.stillObject(), obj1 = im.stillObject();
            sw = im.switchNode({obj0, obj1});
            pc = im.polycoll();
            em = im.emitter();
            g0 = im.group({sw, pc, em}, 300, 300, 0, 20);
            obj2 = im.stillObject();
            d1 = im.destructible({{g0, a1}, {obj2, hat1}});
            i1 = im.istate(d1);
            tD = im.translate({i1, i2}, 300, 300, 0);
            mid = im.group({tD}, 300, 300, 0, 100);
            oldLast = im.group({im.stillObject()});
            world = im.group({mid, tE, oldLast}, 400, 400, 0, 600, 1);
            im.w16(world + 0xA, 3);
            swP = im.switchNode({im.stillObject()});
            gP = im.group({swP}, 500, 500, 10, 20, 0x12C1);
            tP = im.translate({gP}, 500, 500, 10);
            holder = im.group({tP});
            proot = im.group({holder});
            g = im.grid(4, 4, 200, 0, 0, 800, 800);
            pD = im.path(290, 290, 0, 320, 310, 40, {world, mid, tD, i1, d1, g0, pc});
            pE = im.path(690, 90, 0, 710, 110, 20, {world, tE, iE, dE, gE, pcE});
            im.link(g, {pD, pE});
            im.roots({world, proot, g});
        }
        uint32_t off(uint32_t a) const { return a - sm::kBase; }
        sm::CopySet copySet(uint32_t node)
        {
            const sm::Mem m = im.mem();
            return sm::copySet(m, sm::parents(m), node);
        }
        sm::Result run(const sm::Map &map, sm::Reserve &res) { return sm::apply(im.mem(), uint32_t(im.img.size()), map, res); }
        sm::Result run(const sm::Map &map)
        {
            sm::Reserve res = im.reserve();
            return run(map, res);
        }
        bool inReserve(uint32_t a) const { return a >= sm::kReserve && a < sm::kReserve + uint32_t(im.res.size()); }
    };

    sm::Copy copyAt(uint32_t node, float x, float y, float z, float turn = 0)
    {
        sm::Copy c;
        c.node = node, c.at[0] = x, c.at[1] = y, c.at[2] = z, c.turn = turn;
        return c;
    }

    // Copy `copy` of node `orig` (`size` bytes) holds the original's bytes, except child slots that held a copied node (`to`
    // maps original -> copy): those hold its copy
    bool copiedBytes(Img &im, uint32_t orig, uint32_t copy, uint32_t size, const std::map<uint32_t, uint32_t> &to)
    {
        const sm::Mem m = im.mem();
        const std::vector<uint32_t> sl = sm::slots(m, orig);
        for (uint32_t i = 0; i < size; i += 4)
        {
            uint32_t want = im.r32(orig + i);
            if (std::find(sl.begin(), sl.end(), orig + i) != sl.end() && to.count(want))
                want = to.at(want);
            if (im.r32(copy + i) != want)
                return false;
        }
        return true;
    }

    // Review focus 1: nothing reachable from a copy's root is one of its source's copied (per-instance) nodes, and every
    // per-instance node reachable from the header roots has exactly one holder slot
    bool copiesStandAlone(Img &im, const sm::Result &r, const std::map<uint32_t, std::vector<uint32_t>> &sets)
    {
        const sm::Mem m = im.mem();
        for (const sm::CopyInfo &c : r.copies)
        {
            const std::vector<uint32_t> &orig = sets.at(c.source);
            for (uint32_t k : sm::subtree(m, c.root))
                if (std::find(orig.begin(), orig.end(), k) != orig.end())
                    return false;
        }
        std::map<uint32_t, int> held;
        const std::map<uint32_t, uint32_t> par = sm::parents(m);
        for (const auto &kv : par)
            for (uint32_t s : sm::slots(m, kv.first))
                if (const uint32_t k = m.u32(s))
                    ++held[k];
        for (const auto &kv : par)
        {
            const uint32_t w = m.u32(kv.first), op = w & 63, id = w >> 18;
            const bool own = op == sm::kOpInteractive || op == sm::kOpDestructible || op == sm::kOpSwitch || op == sm::kOpEmitter ||
                             op == sm::kOpScriptAction || (op == sm::kOpGroup && id >= sm::kPickupFirst && id <= sm::kPickupLast);
            if (own && held[kv.first] != 1)
                return false;
        }
        return true;
    }

    uint32_t aligned(uint32_t n) { return (n + 15) & ~15u; }
    bool has(const std::string &s, const std::string &what) { return s.find(what) != std::string::npos; }

    sm::Edit moveTo(uint32_t node, float x, float y, float z, float turn = 0)
    {
        sm::Edit e;
        e.node = node, e.move = true, e.pos[0] = x, e.pos[1] = y, e.pos[2] = z, e.turn = turn;
        return e;
    }
    sm::Edit turnBy(uint32_t node, float turn)
    {
        sm::Edit e;
        e.node = node, e.turn = turn;
        return e;
    }
    sm::Edit removal(uint32_t node)
    {
        sm::Edit e;
        e.node = node, e.remove = true;
        return e;
    }
    sm::PowerUpEdit powerUp(uint32_t index, std::array<float, 3> from, std::array<float, 3> at)
    {
        sm::PowerUpEdit e;
        e.index = index;
        for (int k = 0; k < 3; ++k)
            e.from[k] = from[size_t(k)], e.at[k] = at[size_t(k)];
        return e;
    }
    bool logHas(const sm::Result &r, const std::string &what)
    {
        for (const std::string &l : r.log)
            if (l.find(what) != std::string::npos)
                return true;
        return false;
    }
    std::string hexOff(uint32_t off)
    {
        char b[16];
        std::snprintf(b, sizeof b, "0x%x", off);
        return b;
    }
}

TEST(stagemap_fnv)
{
    CHECK(sm::fnv1a(nullptr, 0) == 0xCBF29CE484222325ull);
    CHECK(sm::fnv1a(reinterpret_cast<const uint8_t *>("a"), 1) == 0xAF63DC4C8601EC8Cull);
}

TEST(stagemap_parse)
{
    const std::string good = R"({
  "name": "DOWNTOWN MESS",
  "base": "CENTRAL",
  "baseHash": "0123456789abcdef",
  "edits": [
    { "node": "0x61e340", "move": [120.5, -40.0, 0], "turn": 90 },
    { "node": "0x62a1c0", "remove": true },
    { "node": "0x100", "turn": -15.25 }
  ]
})";
    sm::Map m;
    std::string why;
    CHECK(sm::parse(good, m, why));
    CHECK(m.name == "DOWNTOWN MESS" && m.base == "CENTRAL" && m.baseHash == 0x0123456789abcdefull && m.edits.size() == 3);
    if (m.edits.size() == 3)
    {
        const sm::Edit &a = m.edits[0], &b = m.edits[1], &c = m.edits[2];
        CHECK(a.node == 0x61e340 && !a.remove && a.move && a.pos[0] == 120.5f && a.pos[1] == -40.f && a.pos[2] == 0.f && a.turn == 90.f);
        CHECK(b.node == 0x62a1c0 && b.remove && !b.move && b.turn == 0.f);
        CHECK(c.node == 0x100 && !c.remove && !c.move && c.turn == -15.25f);
    }
    auto bad = [](const std::string &edit, const std::string &needle, const char *top = nullptr) {
        const std::string text = top ? std::string(top)
                                     : R"({"name": "X", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [)" + edit + "]}";
        sm::Map out;
        std::string w;
        const bool ok = sm::parse(text, out, w);
        if (ok || w.find(needle) == std::string::npos)
            std::printf("  parse(%s) -> %d \"%s\"\n", text.c_str(), int(ok), w.c_str());
        return !ok && w.find(needle) != std::string::npos;
    };
    CHECK(bad("", "base", R"({"name": "X", "baseHash": "0123456789abcdef", "edits": []})"));
    CHECK(bad("", "baseHash", R"({"name": "X", "base": "CENTRAL", "baseHash": "0123", "edits": []})"));
    CHECK(bad("", "name", R"({"name": "lower case", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": []})"));
    CHECK(bad("", "JSON", "{ nope"));
    CHECK(bad(R"({"node": "0xZZ", "remove": true})", "node"));
    CHECK(bad(R"({"node": "0x102", "remove": true})", "node"));   // not a multiple of 4
    CHECK(bad(R"({"node": "0x100", "remove": true, "move": [1, 2, 3]})", "remove"));
    CHECK(bad(R"({"node": "0x100", "remove": false})", "remove"));
    CHECK(bad(R"({"node": "0x100", "spin": 1})", "spin"));
    CHECK(bad(R"({"node": "0x100", "move": ["a", 0, 0]})", "move"));
    CHECK(bad(R"({"node": "0x100", "move": [1, 2]})", "move"));
    CHECK(bad(R"({"node": "0x100", "turn": "90"})", "turn"));
    CHECK(bad(R"({"node": "0x100"})", "nothing"));
    sm::Map empty;
    CHECK(sm::parse(R"({"name": "A-1 B", "base": "3-MILE", "baseHash": "FFFFFFFFFFFFFFFF", "edits": []})", empty, why) &&
          empty.edits.empty() && empty.baseHash == ~0ull);
}

TEST(stagemap_write_roundtrip)
{
    sm::Map m;
    m.name = "TEST ONE";
    m.base = "CENTRAL";
    m.baseHash = 0x00ab00cd00ef0012ull;
    m.edits.push_back(moveTo(0x61e340, 0.1f, -1234.567f, 1e-5f, 22.5f));
    m.edits.push_back(removal(0x62a1c0));
    m.edits.push_back(turnBy(0x400, -90.f));
    m.edits.push_back(moveTo(0x404, 3.f, -0.f, 120.f));
    m.powerups.push_back(powerUp(4, {777.f, 600.7f, 630.2f}, {-338.684f, 471.567f, 50.6f}));
    m.powerups.push_back(powerUp(0, {-0.f, 1e-5f, 0.1f}, {1234.567f, -0.f, 3.f}));
    sm::Map back;
    std::string why;
    const std::string text = sm::write(m);
    CHECK(sm::parse(text, back, why));
    CHECK(text.find("e-") == std::string::npos && text.find("\"powerup\": 4") != std::string::npos);   // plain decimals
    bool samePups = back.powerups.size() == m.powerups.size();
    for (size_t i = 0; samePups && i < m.powerups.size(); ++i)
    {
        const sm::PowerUpEdit &a = m.powerups[i], &b = back.powerups[i];
        samePups = a.index == b.index;
        for (int k = 0; k < 3; ++k)
            samePups = samePups && bitsOf(a.from[k]) == bitsOf(b.from[k]) && bitsOf(a.at[k]) == bitsOf(b.at[k]);
    }
    CHECK(samePups);
    CHECK(back.name == m.name && back.base == m.base && back.baseHash == m.baseHash && back.edits.size() == m.edits.size());
    bool same = back.edits.size() == m.edits.size();
    for (size_t i = 0; same && i < m.edits.size(); ++i)
    {
        const sm::Edit &a = m.edits[i], &b = back.edits[i];
        same = a.node == b.node && a.remove == b.remove && a.move == b.move && a.turn == b.turn;
        for (int k = 0; k < 3; ++k)
            same = same && (!a.move || bitsOf(a.pos[k]) == bitsOf(b.pos[k]));
    }
    CHECK(same);
}

TEST(stagemap_powerup_parse)
{
    sm::Map m;
    std::string why;
    CHECK(sm::parse(R"({"name": "P", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [
        { "node": "0x100", "remove": true },
        { "powerup": 4, "from": [777, 600.7, 630.2], "at": [-338.5, 471.5, 680] },
        { "node": "0x200", "turn": 90 },
        { "powerup": 15, "from": [947.1, 615.8, 319.1], "at": [947.1, 615.8, 42.1] }
    ]})", m, why));
    CHECK(m.edits.size() == 2 && m.powerups.size() == 2);
    if (m.powerups.size() == 2)
    {
        const sm::PowerUpEdit &a = m.powerups[0], &b = m.powerups[1];
        CHECK(a.index == 4 && a.from[0] == 777.f && a.from[1] == 600.7f && a.from[2] == 630.2f && a.at[0] == -338.5f &&
              a.at[1] == 471.5f && a.at[2] == 680.f);
        CHECK(b.index == 15 && b.at[2] == 42.1f);
    }
    auto bad = [](const std::string &edit, const std::string &needle) {
        const std::string text = R"({"name": "X", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [)" + edit + "]}";
        sm::Map out;
        std::string w;
        const bool ok = sm::parse(text, out, w);
        if (ok || w.find(needle) == std::string::npos)
            std::printf("  parse(%s) -> %d \"%s\"\n", text.c_str(), int(ok), w.c_str());
        return !ok && w.find(needle) != std::string::npos;
    };
    CHECK(bad(R"({"powerup": -1, "from": [0, 0, 0], "at": [1, 1, 1]})", "powerup"));
    CHECK(bad(R"({"powerup": 256, "from": [0, 0, 0], "at": [1, 1, 1]})", "powerup"));
    CHECK(bad(R"({"powerup": 1.5, "from": [0, 0, 0], "at": [1, 1, 1]})", "powerup"));
    CHECK(bad(R"({"powerup": "1", "from": [0, 0, 0], "at": [1, 1, 1]})", "powerup"));
    CHECK(bad(R"({"powerup": 1, "at": [1, 1, 1]})", "from"));
    CHECK(bad(R"({"powerup": 1, "from": [0, 0], "at": [1, 1, 1]})", "from"));
    CHECK(bad(R"({"powerup": 1, "from": [0, 0, 0]})", "at"));
    CHECK(bad(R"({"powerup": 1, "from": [0, 0, 0], "at": [1, "a", 1]})", "at"));
    CHECK(bad(R"({"powerup": 1, "from": [0, 0, 0], "at": [1, 1, 1], "node": "0x100"})", "node"));
    CHECK(bad(R"({"powerup": 1, "from": [0, 0, 0], "at": [1, 1, 1], "turn": 90})", "turn"));
    CHECK(bad(R"({"powerup": 7, "from": [0, 0, 0], "at": [1, 1, 1]}, {"node": "0x100", "remove": true},
                 {"powerup": 7, "from": [0, 0, 0], "at": [2, 2, 2]})", "edit 3: power-up 7 is edited twice"));
}

// Copies ({"copy": ...}) and power-up places added ({"addpowerup": ...}) share the one "edits" list with the step 1 edits
TEST(stagemap_copy_parse)
{
    sm::Map m;
    std::string why;
    CHECK(sm::parse(R"({"name": "C", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [
        { "copy": "0x2d85a0", "at": [768.1, 499.6, 74.1], "turn": 90 },
        { "node": "0x100", "remove": true },
        { "addpowerup": 4, "from": [1, 2, 3], "at": [4, 5, 6] },
        { "copy": "0x2d85a0", "at": [1, 2, 3] },
        { "powerup": 4, "from": [1, 2, 3], "at": [7, 8, 9] },
        { "addpowerup": 4, "from": [1, 2, 3], "at": [10, 11, 12.5] }
    ]})", m, why));
    if (!why.empty())
        std::printf("  parse -> \"%s\"\n", why.c_str());
    CHECK(m.edits.size() == 1 && m.copies.size() == 2 && m.powerups.size() == 1 && m.addPowerUps.size() == 2);
    if (m.copies.size() == 2)
    {
        const sm::Copy &a = m.copies[0], &b = m.copies[1];
        CHECK(a.node == 0x2d85a0 && a.at[0] == 768.1f && a.at[1] == 499.6f && a.at[2] == 74.1f && a.turn == 90.f);
        CHECK(b.node == 0x2d85a0 && b.at[0] == 1.f && b.at[1] == 2.f && b.at[2] == 3.f && b.turn == 0.f);
    }
    if (m.powerups.size() == 1)
        CHECK(m.powerups[0].index == 4 && m.powerups[0].at[0] == 7.f);
    if (m.addPowerUps.size() == 2)
    {
        const sm::PowerUpEdit &a = m.addPowerUps[0], &b = m.addPowerUps[1];
        CHECK(a.index == 4 && a.from[0] == 1.f && a.from[1] == 2.f && a.from[2] == 3.f && a.at[0] == 4.f && a.at[1] == 5.f &&
              a.at[2] == 6.f);
        CHECK(b.index == 4 && b.at[0] == 10.f && b.at[1] == 11.f && b.at[2] == 12.5f);
    }
    auto bad = [](const std::string &edits, const std::string &needle) {
        const std::string text = R"({"name": "X", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": [)" + edits + "]}";
        sm::Map out;
        std::string w;
        const bool ok = sm::parse(text, out, w);
        if (ok || w.find(needle) == std::string::npos)
            std::printf("  parse(%.200s) -> %d \"%s\"\n", text.c_str(), int(ok), w.c_str());
        return !ok && w.find(needle) != std::string::npos;
    };
    const std::string first = R"({"node": "0x100", "remove": true}, )";   // so the bad edit is edit 2
    CHECK(bad(first + R"({"copy": "0xZZ", "at": [1, 2, 3]})", "edit 2: \"copy\""));
    CHECK(bad(first + R"({"copy": "0x102", "at": [1, 2, 3]})", "edit 2: \"copy\""));       // not a multiple of 4
    CHECK(bad(first + R"({"copy": "0x1600000", "at": [1, 2, 3]})", "edit 2: \"copy\""));   // kMaxNode
    CHECK(bad(first + R"({"copy": 256, "at": [1, 2, 3]})", "edit 2: \"copy\""));
    CHECK(bad(first + R"({"copy": "0x100"})", "edit 2: \"at\""));
    CHECK(bad(first + R"({"copy": "0x100", "at": [1, 2]})", "edit 2: \"at\""));
    CHECK(bad(first + R"({"copy": "0x100", "at": [1, 2, 3], "turn": "90"})", "edit 2: \"turn\""));
    CHECK(bad(first + R"({"copy": "0x100", "at": [1, 2, 3], "move": [1, 2, 3]})", "edit 2: unknown key \"move\""));
    CHECK(bad(first + R"({"addpowerup": -1, "from": [0, 0, 0], "at": [1, 1, 1]})", "edit 2: \"addpowerup\""));
    CHECK(bad(first + R"({"addpowerup": 256, "from": [0, 0, 0], "at": [1, 1, 1]})", "edit 2: \"addpowerup\""));
    CHECK(bad(first + R"({"addpowerup": 1, "at": [1, 1, 1]})", "edit 2: \"from\""));
    CHECK(bad(first + R"({"addpowerup": 1, "from": [0, 0, 0], "at": [1, 1, 1], "turn": 9})", "edit 2: unknown key \"turn\""));
    CHECK(bad(first + R"({"copy": "0x100", "addpowerup": 1, "at": [1, 2, 3]})", "edit 2: unknown key \"addpowerup\""));   // one kind per entry
    CHECK(bad(first + R"({"copy": "0x100", "at": [1, 2, 3], "turn": 1e39})", "edit 2: \"turn\""));   // past float: not finite
    CHECK(bad(first + R"({"addpowerup": 1.5, "from": [0, 0, 0], "at": [1, 1, 1]})", "edit 2: \"addpowerup\""));
    std::string many;
    for (int i = 0; i < 4097; ++i)
        many += std::string(i ? ", " : "") + (i % 2 ? R"({"copy": "0x100", "at": [1, 2, 3]})" : R"({"addpowerup": 1, "from": [0, 0, 0], "at": [1, 1, 1]})");
    CHECK(bad(many, "at most 4096"));
}

TEST(stagemap_copy_write_roundtrip)
{
    sm::Map m;
    m.name = "COPIES";
    m.base = "METRO";
    m.baseHash = 0x1122334455667788ull;
    sm::Copy c1, c2;
    c1.node = 0x2d85a0, c1.at[0] = 768.1f, c1.at[1] = -0.f, c1.at[2] = 1e-5f, c1.turn = 22.5f;
    c2.node = 0x404, c2.at[0] = 0.1f, c2.at[1] = -1234.567f, c2.at[2] = 3.f;
    // added before the step 1 edits: write() still puts them in its own order
    m.addPowerUps.push_back(powerUp(2, {1.f, 2.f, 3.f}, {-0.f, 600.7f, 1e-5f}));
    m.addPowerUps.push_back(powerUp(2, {1.f, 2.f, 3.f}, {4.f, 5.f, 6.f}));
    m.copies.push_back(c1);
    m.copies.push_back(c2);
    m.powerups.push_back(powerUp(2, {1.f, 2.f, 3.f}, {7.f, 8.f, 9.f}));
    m.edits.push_back(moveTo(0x61e340, 0.1f, -1234.567f, 1e-5f, 22.5f));
    m.edits.push_back(removal(0x62a1c0));
    const std::string text = sm::write(m);
    sm::Map back;
    std::string why;
    CHECK(sm::parse(text, back, why));
    CHECK(sm::write(back) == text);   // byte stable
    CHECK(back.edits.size() == 2 && back.copies.size() == 2 && back.powerups.size() == 1 && back.addPowerUps.size() == 2);
    bool same = back.copies.size() == m.copies.size();
    for (size_t i = 0; same && i < m.copies.size(); ++i)
    {
        const sm::Copy &a = m.copies[i], &b = back.copies[i];
        same = a.node == b.node && bitsOf(a.turn) == bitsOf(b.turn);
        for (int k = 0; k < 3; ++k)
            same = same && bitsOf(a.at[k]) == bitsOf(b.at[k]);
    }
    CHECK(same);
    bool samePups = back.addPowerUps.size() == m.addPowerUps.size() && back.powerups.size() == m.powerups.size() &&
                    back.powerups[0].at[2] == 9.f;
    for (size_t i = 0; samePups && i < m.addPowerUps.size(); ++i)
    {
        const sm::PowerUpEdit &a = m.addPowerUps[i], &b = back.addPowerUps[i];
        samePups = a.index == b.index;
        for (int k = 0; k < 3; ++k)
            samePups = samePups && bitsOf(a.from[k]) == bitsOf(b.from[k]) && bitsOf(a.at[k]) == bitsOf(b.at[k]);
    }
    CHECK(samePups);
    CHECK(back.edits.size() == 2 && back.edits[0].node == 0x61e340 && back.edits[1].remove);
    // order in the text: node edits, copies, power-up moves, power-up additions
    const size_t lastNode = text.rfind("\"node\""), firstCopy = text.find("\"copy\""), lastCopy = text.rfind("\"copy\""),
                 firstPup = text.find("\"powerup\""), lastPup = text.rfind("\"powerup\""), firstAdd = text.find("\"addpowerup\"");
    CHECK(lastNode != std::string::npos && firstCopy != std::string::npos && firstPup != std::string::npos &&
          firstAdd != std::string::npos);
    CHECK(lastNode < firstCopy && lastCopy < firstPup && lastPup < firstAdd);
    CHECK(text.find("e-") == std::string::npos);   // plain decimals
    // turn 0 is left out
    CHECK(text.find("\"turn\": 22.5") != std::string::npos);
    size_t turns = 0;
    for (size_t p = text.find("\"turn\""); p != std::string::npos; p = text.find("\"turn\"", p + 1))
        ++turns;
    CHECK(turns == 2);   // the move's and c1's
}

// Power-up places on a fake PowerUpTool points array (64-byte points, position at +0, type and arg at +0x2C/+0x2D)
TEST(stagemap_powerups_apply)
{
    constexpr uint32_t kPoints = 0x2AF0C0;   // PowerUpTool::instance (points at +0)
    std::vector<uint8_t> buf(sm::kMaxPoints * sm::kPointSize);
    sm::Mem mem;
    mem.r.push_back({kPoints, buf.data(), uint32_t(buf.size())});
    auto put = [&](uint32_t i, float x, float y, float z) {
        mem.wf32(kPoints + sm::kPointSize * i, x), mem.wf32(kPoints + sm::kPointSize * i + 4, y);
        mem.wf32(kPoints + sm::kPointSize * i + 8, z);
        buf[sm::kPointSize * i + 0x2C] = uint8_t(i), buf[sm::kPointSize * i + 0x2D] = 7;
    };
    auto pos = [&](uint32_t i) {
        const uint32_t a = kPoints + sm::kPointSize * i;
        return std::array<float, 3>{mem.f32(a), mem.f32(a + 4), mem.f32(a + 8)};
    };
    for (uint32_t i = 0; i < 4; ++i)
        put(i, 100.f * float(i), -50.f, 40.f + float(i));
    const std::vector<sm::PowerUpEdit> edits = {
        powerUp(1, {100.f, -50.f, 41.f}, {500.f, 600.f, 70.f}),
        powerUp(2, {200.005f, -50.f, 42.f}, {-1.5f, 2.5f, 3.5f}),   // within 0.01 of the disc place
        powerUp(3, {300.f, -50.f, 44.f}, {9.f, 9.f, 9.f}),          // z off by 1: refused
        powerUp(4, {0.f, 0.f, 0.f}, {9.f, 9.f, 9.f}),               // past numPoints (4): refused
        powerUp(1, {100.f, -50.f, 41.f}, {7.f, 7.f, 7.f}),          // index 1 again: refused
    };
    const std::vector<uint8_t> before = buf;
    sm::PowerUpResult r = sm::applyPowerUps(mem, kPoints, 4, edits);
    for (const std::string &l : r.log)
        std::printf("  %s\n", l.c_str());
    CHECK(r.moved == 2 && r.refused == 3 && r.log.size() == 3);
    CHECK((pos(1) == std::array<float, 3>{500.f, 600.f, 70.f}) && (pos(2) == std::array<float, 3>{-1.5f, 2.5f, 3.5f}));
    CHECK((pos(0) == std::array<float, 3>{0.f, -50.f, 40.f}) && (pos(3) == std::array<float, 3>{300.f, -50.f, 43.f}));
    CHECK(buf[sm::kPointSize * 1 + 0x2C] == 1 && buf[sm::kPointSize * 1 + 0x2D] == 7);   // type and arg kept
    bool onlyPositions = true;
    for (size_t i = 0; i < buf.size(); ++i)
        onlyPositions = onlyPositions && (buf[i] == before[i] || ((i / sm::kPointSize == 1 || i / sm::kPointSize == 2) && i % sm::kPointSize < 12));
    CHECK(onlyPositions);
    if (r.log.size() == 3)
        CHECK(r.log[0].find("power-up 3: is at 300.0 -50.0 43.0") == 0 && r.log[1].find("power-up 4: the stage has 4") == 0 &&
              r.log[2].find("power-up 1: edited twice") == 0);
    // a second call on the same points (exportPoints again without a new loadPoints): the moved points are at "at", same result
    const std::vector<uint8_t> once = buf;
    r = sm::applyPowerUps(mem, kPoints, 4, edits);
    CHECK(r.moved == 2 && r.refused == 3 && buf == once);
    // after a fresh loadPoints (disc places again) the same edits give the same bytes
    put(1, 100.f, -50.f, 41.f), put(2, 200.f, -50.f, 42.f);
    r = sm::applyPowerUps(mem, kPoints, 4, edits);
    CHECK(r.moved == 2 && buf == once);
    // no edits, or no points: nothing written
    CHECK(sm::applyPowerUps(mem, kPoints, 4, {}).moved == 0 && buf == once);
    r = sm::applyPowerUps(mem, kPoints, 0, edits);
    CHECK(r.moved == 0 && r.refused == 5 && buf == once);
    // a count past PointToolKit::points[256] (a broken numPoints): everything refused, nothing written past the array
    r = sm::applyPowerUps(mem, kPoints, sm::kMaxPoints + 1, {powerUp(1, {500.f, 600.f, 70.f}, {1.f, 2.f, 3.f}),
                                                             powerUp(255, {0.f, 0.f, 0.f}, {1.f, 2.f, 3.f})});
    CHECK(r.moved == 0 && r.refused == 2 && buf == once);
    if (!r.log.empty())
        CHECK(r.log[0].find("the point count 257 is past the 256 points") != std::string::npos);
}

// Power-up places added by a map: copies of stock points appended after them, the whole 64-byte point with its position set
TEST(stagemap_powerups_append)
{
    constexpr uint32_t kPoints = 0x2AF0C0;
    std::vector<uint8_t> buf(sm::kMaxPoints * sm::kPointSize);
    sm::Mem mem;
    mem.r.push_back({kPoints, buf.data(), uint32_t(buf.size())});
    auto put = [&](uint32_t i, uint8_t type) {
        const uint32_t a = kPoints + sm::kPointSize * i;
        mem.wf32(a, 100.f * float(i)), mem.wf32(a + 4, -50.f), mem.wf32(a + 8, 40.f + float(i));
        for (uint32_t k = 12; k < sm::kPointSize; ++k)
            buf[a - kPoints + k] = uint8_t(i * 7 + k);
        buf[a - kPoints + 0x2C] = type, buf[a - kPoints + 0x2D] = uint8_t(type + 10);
    };
    auto pos = [&](uint32_t i) {
        const uint32_t a = kPoints + sm::kPointSize * i;
        return std::array<float, 3>{mem.f32(a), mem.f32(a + 4), mem.f32(a + 8)};
    };
    auto tailOf = [&](uint32_t i, uint32_t src) {   // bytes 12..63 of point i equal those of point src
        return std::equal(buf.begin() + sm::kPointSize * i + 12, buf.begin() + sm::kPointSize * (i + 1),
                          buf.begin() + sm::kPointSize * src + 12);
    };
    auto from = [](uint32_t i) { return std::array<float, 3>{100.f * float(i), -50.f, 40.f + float(i)}; };
    {
        for (uint32_t i = 0; i < 3; ++i)
            put(i, uint8_t(i + 1));
        const std::vector<uint8_t> stock(buf.begin(), buf.begin() + 3 * sm::kPointSize);
        const std::vector<sm::PowerUpEdit> adds = {
            powerUp(1, from(1), {500.f, 600.f, 70.f}),
            powerUp(1, {100.005f, -50.f, 41.f}, {-1.5f, 2.5f, 3.5f}),   // the same index again, within 0.01 of the disc place
            powerUp(0, {0.f, -50.f, 41.f}, {9.f, 9.f, 9.f}),            // z off by 1: refused
            powerUp(2, from(2), {7.f, 8.f, 9.f}),
            powerUp(3, from(3), {9.f, 9.f, 9.f}),                       // past the stock points: refused
        };
        uint32_t count = 0;
        const sm::PowerUpResult r = sm::appendPowerUps(mem, kPoints, 3, adds, count);
        for (const std::string &l : r.log)
            std::printf("  %s\n", l.c_str());
        CHECK(count == 6 && r.moved == 3 && r.refused == 2 && r.log.size() == 2);
        CHECK(tailOf(3, 1) && tailOf(4, 1) && tailOf(5, 2) && buf[sm::kPointSize * 5 + 0x2C] == 3);
        CHECK((pos(3) == std::array<float, 3>{500.f, 600.f, 70.f}) && (pos(4) == std::array<float, 3>{-1.5f, 2.5f, 3.5f}) &&
              (pos(5) == std::array<float, 3>{7.f, 8.f, 9.f}));
        CHECK(std::equal(stock.begin(), stock.end(), buf.begin()));   // the stock points are untouched
        CHECK(std::all_of(buf.begin() + 6 * sm::kPointSize, buf.end(), [](uint8_t b) { return b == 0; }));
        if (r.log.size() == 2)
            CHECK(r.log[0].find("added power-up 0: is at 0.0 -50.0 40.0") == 0 &&
                  r.log[1].find("added power-up 3: the stage has 3 power-up places") == 0);
    }
    {
        std::fill(buf.begin(), buf.end(), uint8_t(0));
        for (uint32_t i = 0; i < 255; ++i)
            put(i, 1);
        uint32_t count = 0;
        const sm::PowerUpResult r = sm::appendPowerUps(mem, kPoints, 255, {powerUp(4, from(4), {1.f, 2.f, 3.f}),
                                                                            powerUp(5, from(5), {1.f, 2.f, 3.f})}, count);
        CHECK(count == 256 && r.moved == 1 && r.refused == 1 && tailOf(255, 4));
        CHECK(r.log.size() == 2 && r.log[0].find("added power-up 5: ") == 0 && r.log[0].find("256 points") != std::string::npos);
        CHECK(r.log.size() == 2 && r.log[1].find("the game creates 100 at most") != std::string::npos);
    }
    {
        std::fill(buf.begin(), buf.end(), uint8_t(0));
        for (uint32_t i = 0; i < 98; ++i)
            put(i, 2);
        uint32_t count = 0;
        const sm::PowerUpResult r = sm::appendPowerUps(mem, kPoints, 98, {powerUp(0, from(0), {1.f, 2.f, 3.f}),
                                                                           powerUp(1, from(1), {1.f, 2.f, 3.f}),
                                                                           powerUp(2, from(2), {1.f, 2.f, 3.f})}, count);
        CHECK(count == 101 && r.moved == 3 && r.refused == 0);
        CHECK(r.log.size() == 1 && r.log[0] == "added 3 power-ups (101 places: the game creates 100 at most)");
        // two short of 100 and one added: no note
        uint32_t c2 = 0;
        CHECK(sm::appendPowerUps(mem, kPoints, 98, {powerUp(0, from(0), {1.f, 2.f, 3.f})}, c2).log.empty() && c2 == 99);
    }
    {
        // no additions: nothing written, the count stays; a broken count past 256: everything refused, nothing written
        const std::vector<uint8_t> before = buf;
        uint32_t count = 0;
        CHECK(sm::appendPowerUps(mem, kPoints, 98, {}, count).moved == 0 && count == 98 && buf == before);
        const sm::PowerUpResult r = sm::appendPowerUps(mem, kPoints, sm::kMaxPoints + 1, {powerUp(0, from(0), {1.f, 2.f, 3.f})}, count);
        CHECK(r.moved == 0 && r.refused == 1 && count == sm::kMaxPoints + 1 && buf == before);
    }
}

// The coverage rule on the same cases as tools/stage_map.py selftest: a box past both X edges, a negative start, one fully
// outside, and one ending exactly on a cell line (floor-inclusive takes the next cell).
TEST(stagemap_covered)
{
    const sm::GridDims g{4, 2, 100.f, -100.f, 0.f, 300.f, 200.f};
    auto cells = [&](float x0, float y0, float x1, float y1) {
        const float lo[3] = {x0, y0, 0.f}, hi[3] = {x1, y1, 9.f};
        return sm::covered(g, lo, hi);
    };
    CHECK(cells(-160.f, 110.f, 450.f, 150.f) == std::vector<int>({4, 5, 6, 7}));
    CHECK(cells(-50.f, 10.f, 20.f, 50.f) == std::vector<int>({0, 1}));
    CHECK(cells(-500.f, 10.f, -300.f, 20.f).empty());
    CHECK(cells(0.f, 0.f, 100.f, 100.f) == std::vector<int>({1, 2, 5, 6}));
}

TEST(stagemap_reserve_alloc)
{
    sm::Reserve r{0x1008, 0x1040};
    CHECK(r.alloc(8) == 0x1010 && r.next == 0x1020);
    CHECK(r.alloc(0x11) == 0x1020 && r.next == 0x1040);
    CHECK(r.alloc(1) == 0 && r.next == 0x1040);
}

TEST(stagemap_reserve_layout)
{
    CHECK(sm::kReserve == 0x3BFF000u && sm::kReserveSize == 0x400000u);
    CHECK(sm::kReserve + sm::kReserveSize == 0x4000000u - 0x1000u);   // right under the top-of-RAM slots (wotm_split4.inc: -0x1000)
}

TEST(stagemap_walk)
{
    Img im;
    const uint32_t o1 = im.object(), pc1 = im.polycoll(), shared = im.object();
    const uint32_t g1 = im.group({o1, pc1, shared}), t1 = im.translate({g1}, 50, 50, 0);
    const uint32_t g2 = im.group({im.object(), shared}), t2 = im.translate({g2}, 150, 50, 0);
    const uint32_t ts = im.translate({im.object()}, 1, 1, 1), sc = im.holder(30, 8, 0x2C, {ts});    // under a SCALE
    const uint32_t ta = im.translate({im.object()}, 2, 2, 2), ax = im.holder(17, 4, 0xCC, {ta});    // under an ANIM_XFORM
    const uint32_t tb = im.translate({im.object()}, 3, 3, 3), sb = im.holder(35, 4, 0xC, {tb});     // under a SKEL_BONE
    const uint32_t tc = im.translate({im.object()}, 4, 4, 4), as = im.holder(38, 4, 0x60, {tc});    // under an ANIM_SCALE
    const uint32_t te = im.translate({}, 5, 5, 5);                                                   // no geometry
    const uint32_t world = im.group({t1, t2, sc, ax, sb, as, te}, 0, 0, 0, 1000, 1);
    const uint32_t pedIn = im.translate({im.object()}, 0, 0, 0), ped = im.translate({pedIn}, 9, 9, 9, 3000);
    const uint32_t tt = im.translate({im.object(18)}, 0, 35, 0), templ = im.group({tt});
    const uint32_t pick = im.translate({im.group({im.object(5001)})}, 50, 60, 70), pickups = im.group({pick});
    im.roots({world, ped, templ, pickups});
    const sm::Mem m = im.mem();
    const std::map<uint32_t, uint32_t> par = sm::parents(m);
    for (uint32_t n : {world, t1, g1, o1, pc1, shared, t2, g2, ts, sc, ta, ax, tb, sb, tc, as, te, ped, pedIn, tt, templ, pick, pickups})
        CHECK(par.count(n) == 1);
    auto parent = [&](uint32_t n) { const auto it = par.find(n); return it == par.end() ? 0xFFFFFFFFu : it->second; };
    CHECK(parent(world) == 0 && parent(ped) == 0 && parent(t1) == world && parent(g1) == t1);
    CHECK(parent(shared) == g1);   // reached twice: keeps its first parent
    CHECK(parent(ta) == ax && parent(tb) == sb && parent(tc) == as && parent(ts) == sc && parent(pedIn) == ped);
    CHECK(sm::children(m, ax) == std::vector<uint32_t>({ta}) && sm::children(m, as) == std::vector<uint32_t>({tc}));
    CHECK(sm::isPiece(m, par, t1) && sm::isPiece(m, par, t2) && sm::isPiece(m, par, tb) && sm::isPiece(m, par, pick));
    for (uint32_t n : {ts, ta, tc, te, ped, pedIn, tt, world, g1, o1, templ, pickups})
        CHECK(!sm::isPiece(m, par, n));
    CHECK(!sm::isPiece(m, par, sm::kBase + 0x7F00));   // not reached
}

TEST(stagemap_remove)
{
    {
        Stage s;
        const std::vector<uint8_t> before = s.im.img;
        const sm::Result r = s.run({removal(s.off(s.tA))});
        CHECK(r.applied == 1 && r.refused == 0);
        uint32_t w0;
        std::memcpy(&w0, &before[s.off(s.tA)], 4);
        CHECK(s.im.r32(s.tA) == ((w0 & ~63u) | 9));
        CHECK(s.im.cellsOf(s.g, s.pA).empty());   // its path left the grid (traversal stops at the NULL anyway)
        CHECK(s.im.cellsOf(s.g, s.pB) == std::vector<int>({2}) && s.im.cellsOf(s.g, s.pG).size() == 16 && s.im.gridProblems(s.g) == 0);
        CHECK(s.im.rf(s.pA) == 90.f && s.im.r32(s.pA + 0x24 + 8) == s.tA);   // the path itself is left as it was
    }
    {
        Stage s;
        const std::vector<uint8_t> before = s.im.img, rbefore = s.im.res;
        const sm::Result r = s.run({removal(s.off(s.gA)), removal(0x100000), removal(s.off(s.tA) + 4)});
        CHECK(r.applied == 0 && r.refused == 3 && r.log.size() == 3);
        CHECK(logHas(r, hexOff(s.off(s.gA))) && logHas(r, "0x100000") && logHas(r, "not a piece") && logHas(r, "outside"));
        CHECK(s.im.img == before && s.im.res == rbefore);
    }
    {
        Stage s;
        const sm::Result r = s.run({removal(s.off(s.tA)), moveTo(s.off(s.tA), 1, 2, 3)});
        CHECK(r.applied == 1 && r.refused == 1 && logHas(r, "twice") && logHas(r, hexOff(s.off(s.tA))));
        CHECK(s.im.rf(s.tA + 0x10) == 100.f);
    }
}

TEST(stagemap_move)
{
    {
        Stage s;   // +250 in X: from cell 0 to cell 1, the path's own list node moves
        const sm::Result r = s.run({moveTo(s.off(s.tA), 350, 100, 0)});
        CHECK(r.applied == 1 && r.refused == 0 && r.relinked == 1 && r.fromReserve == 0);
        CHECK(s.im.rf(s.tA + 0x10) == 350.f && s.im.rf(s.tA + 0x14) == 100.f && s.im.rf(s.tA + 0x18) == 0.f);
        CHECK(near(s.im.rf(s.pA), 340.f) && near(s.im.rf(s.pA + 4), 95.f) && near(s.im.rf(s.pA + 0xC), 380.f) &&
              near(s.im.rf(s.pA + 0x10), 105.f) && s.im.rf(s.pA + 0x8) == 0.f && s.im.rf(s.pA + 0x14) == 30.f);
        CHECK(s.im.cellsOf(s.g, s.pA) == std::vector<int>({1}));
        CHECK(s.im.cellsOf(s.g, s.pG).size() == 16 && s.im.cellsOf(s.g, s.pB) == std::vector<int>({2}));
        CHECK(s.im.gridProblems(s.g) == 0);
    }
    {
        Stage s;   // across a cell line: one more list node, from the reserve
        sm::Reserve res = s.im.reserve();
        const uint32_t start = res.next;
        const sm::Result r = s.run({moveTo(s.off(s.tA), 180, 100, 0)}, res);
        CHECK(r.applied == 1 && r.relinked == 2 && r.fromReserve == 1);
        CHECK(s.im.cellsOf(s.g, s.pA) == std::vector<int>({0, 1}) && s.im.gridProblems(s.g) == 0);
        int inReserve = 0;
        for (const auto &c : s.im.listing(s.g, s.pA))
            inReserve += c.second >= start && c.second < res.end;
        CHECK(inReserve == 1 && res.next > start);
    }
    {
        Stage s;   // the same with no room in the reserve: refused, nothing written
        const std::vector<uint8_t> before = s.im.img;
        sm::Reserve none{sm::kReserve, sm::kReserve};
        const sm::Result r = s.run({moveTo(s.off(s.tA), 180, 100, 0)}, none);
        CHECK(r.applied == 0 && r.refused == 1 && logHas(r, "reserve") && logHas(r, hexOff(s.off(s.tA))));
        CHECK(s.im.img == before);
    }
    {
        Stage s;   // past the grid's edges: clamped to the stored min and max
        const sm::Result r = s.run({moveTo(s.off(s.tA), -50, 100, 0), moveTo(s.off(s.rB), 900, 900, 0)});
        CHECK(r.applied == 2 && logHas(r, "edge"));
        CHECK(near(s.im.rf(s.tA + 0x10), 100.f - 90.f) && near(s.im.rf(s.pA), 0.f));                 // box min x was -90 - 50 + 100
        CHECK(near(s.im.rf(s.rB + 0x40), 900.f - 140.f) && near(s.im.rf(s.rB + 0x44), 790.f));       // box max x 940 -> 800, y 910 -> 800
        CHECK(near(s.im.rf(s.pB + 0xC), 800.f) && near(s.im.rf(s.pB + 0x10), 800.f));
        CHECK(s.im.gridProblems(s.g) == 0 && s.im.cellsOf(s.g, s.pB) == std::vector<int>({15}));
    }
    {
        Stage s;   // a ROTATE piece moves through its fourth row; its 3x3 stays
        const float m00 = s.im.rf(s.rB + 0x10), m01 = s.im.rf(s.rB + 0x14);
        const sm::Result r = s.run({moveTo(s.off(s.rB), 500, 300, 5)});
        CHECK(r.applied == 1 && s.im.rf(s.rB + 0x40) == 500.f && s.im.rf(s.rB + 0x44) == 300.f && s.im.rf(s.rB + 0x48) == 5.f);
        CHECK(s.im.rf(s.rB + 0x4C) == 1.f && s.im.rf(s.rB + 0x10) == m00 && s.im.rf(s.rB + 0x14) == m01);
        CHECK(near(s.im.rf(s.pB), 480.f) && near(s.im.rf(s.pB + 4), 290.f) && near(s.im.rf(s.pB + 8), 5.f) && near(s.im.rf(s.pB + 0x14), 15.f));
        CHECK(s.im.cellsOf(s.g, s.pB) == std::vector<int>({6}) && s.im.gridProblems(s.g) == 0);
    }
}

TEST(stagemap_spheres)
{
    {
        Stage s;   // out of `mid`'s sphere: mid grows, center kept; the world root's sphere already covers it
        const sm::Result r = s.run({moveTo(s.off(s.tA), 700, 700, 0)});
        CHECK(r.applied == 1);
        const double want = std::sqrt(600.0 * 600.0 * 2.0) + 20.0;   // |(700,700) - (100,100)| + gA's radius
        const float rad = s.im.rf(s.mid + 0x1C);
        CHECK(double(rad) >= want && near(rad, float(want)) && s.im.rf(s.mid + 0xC) == rad * rad);
        CHECK(s.im.rf(s.mid + 0x10) == 100.f && s.im.rf(s.mid + 0x14) == 100.f && s.im.rf(s.mid + 0x18) == 0.f);
        CHECK(s.im.rf(s.world + 0x1C) == 600.f && s.im.rf(s.world + 0xC) == 360000.f);
    }
    {
        Stage s;   // a move inside the sphere changes nothing
        s.run({moveTo(s.off(s.tA), 110, 100, 0)});
        CHECK(s.im.rf(s.mid + 0x1C) == 50.f && s.im.rf(s.mid + 0xC) == 2500.f);
    }
    {
        Img im;   // a LOD above the piece: its sphere drives LOD switching, so it is logged and left alone
        const uint32_t t = im.translate({im.object()}, 10, 10, 0), l = im.lod(t, 10, 10, 0, 30);
        const uint32_t world = im.group({l}, 0, 0, 0, 5000, 1), g = im.grid(2, 2, 200, 0, 0, 400, 400);
        im.roots({world, g});
        sm::Map m;
        m.edits.push_back(moveTo(t - sm::kBase, 300, 300, 0));
        sm::Reserve res = im.reserve();
        const sm::Result r = sm::apply(im.mem(), uint32_t(im.img.size()), m, res);
        CHECK(r.applied == 1 && im.rf(t + 0x10) == 300.f && im.rf(l + 0x1C) == 30.f && im.rf(l + 0x10) == 10.f);
        CHECK(logHas(r, "LOD") && logHas(r, hexOff(l - sm::kBase)));
    }
}

TEST(stagemap_turn_rotate_piece)
{
    {
        Stage s;   // 30 + 90 degrees, moved: the 3x3 is multiplied by rotZ(90), the fourth row is the new place
        const sm::Result r = s.run({moveTo(s.off(s.rB), 500, 300, 0, 90)});
        CHECK(r.applied == 1);
        const M4 want = rotZ(120.f);
        bool ok = true;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 4; ++j)
                ok = ok && near(s.im.rf(s.rB + 0x10 + 16 * i + 4 * j), want.m[i][j], 1e-5f);
        CHECK(ok);
        CHECK(s.im.rf(s.rB + 0x40) == 500.f && s.im.rf(s.rB + 0x44) == 300.f && s.im.rf(s.rB + 0x48) == 0.f && s.im.rf(s.rB + 0x4C) == 1.f);
        // box relative to the old place (-20..40, -10..10) turned 90: (-10..10, -20..40)
        CHECK(near(s.im.rf(s.pB), 490.f) && near(s.im.rf(s.pB + 4), 280.f) && near(s.im.rf(s.pB + 0xC), 510.f) && near(s.im.rf(s.pB + 0x10), 340.f));
        CHECK(s.im.cellsOf(s.g, s.pB) == std::vector<int>({6}) && s.im.gridProblems(s.g) == 0);
    }
    {
        Stage s;   // a turn alone keeps the place; 45 degrees is not a right angle
        const sm::Result r = s.run({turnBy(s.off(s.rB), 45)});
        CHECK(r.applied == 1 && s.im.rf(s.rB + 0x40) == 500.f && s.im.rf(s.rB + 0x44) == 100.f);
        CHECK(near(s.im.rf(s.rB + 0x10), std::cos(75.f * 3.14159265f / 180.f), 1e-5f) &&
              near(s.im.rf(s.rB + 0x14), std::sin(75.f * 3.14159265f / 180.f), 1e-5f));
        CHECK(s.im.gridProblems(s.g) == 0);
    }
}

TEST(stagemap_turn_translate_piece)
{
    {
        Stage s;
        s.im.w32(s.tA + 4, 0x1234);   // a GS ctx: the ROTATE keeps it (the renderer swaps ctx at TRANSLATE and ROTATE alike)
        const uint32_t word = s.im.r32(s.tA);
        sm::Reserve res = s.im.reserve();
        const uint32_t start = res.next;
        const sm::Result r = s.run({turnBy(s.off(s.tA), 90)}, res);
        CHECK(r.applied == 1 && r.refused == 0);
        const uint32_t rot = s.im.r32(s.mid + 0x20);   // mid's child slot now holds the ROTATE
        CHECK(rot != s.tA && rot >= start && rot < res.end && rot % 16 == 0);
        CHECK(s.im.r32(rot) == ((word & ~63u) | 4) && s.im.r32(rot + 4) == 0x1234 && s.im.r32(rot + 8) == 1 && s.im.r32(rot + 0xC) == 0);
        CHECK(s.im.r32(rot + 0x50) == s.gA);
        const float want[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 100, 100, 0, 1};
        bool ok = true;
        for (int k = 0; k < 16; ++k)
            ok = ok && s.im.rf(rot + 0x10 + 4 * uint32_t(k)) == want[k];
        CHECK(ok);
        CHECK(s.im.r32(s.pA + 0x24 + 8) == rot && s.im.r32(s.pA + 0x24 + 4) == s.mid && s.im.r32(s.pA + 0x24 + 12) == s.gA);
        CHECK((s.im.r32(s.tA) & 63) == 3 && s.im.rf(s.tA + 0x10) == 100.f);   // the TRANSLATE itself is left as it was
        // box (-10..30, -5..5) around (100,100) turned 90: (-5..5, -10..30)
        CHECK(near(s.im.rf(s.pA), 95.f) && near(s.im.rf(s.pA + 4), 90.f) && near(s.im.rf(s.pA + 0xC), 105.f) && near(s.im.rf(s.pA + 0x10), 130.f));
        CHECK(s.im.gridProblems(s.g) == 0);
    }
    {
        Stage s;   // turned and moved
        const sm::Result r = s.run({moveTo(s.off(s.tA), 300, 300, 0, 90)});
        const uint32_t rot = s.im.r32(s.mid + 0x20);
        CHECK(r.applied == 1 && s.im.rf(rot + 0x40) == 300.f && s.im.rf(rot + 0x44) == 300.f);
        CHECK(near(s.im.rf(s.pA), 295.f) && near(s.im.rf(s.pA + 4), 290.f) && near(s.im.rf(s.pA + 0xC), 305.f) && near(s.im.rf(s.pA + 0x10), 330.f));
        CHECK(s.im.cellsOf(s.g, s.pA) == std::vector<int>({5}) && s.im.gridProblems(s.g) == 0);
    }
    {
        Stage s;   // no room for the ROTATE: refused, nothing written
        const std::vector<uint8_t> before = s.im.img;
        sm::Reserve none{sm::kReserve, sm::kReserve};
        const sm::Result r = s.run({turnBy(s.off(s.tA), 90)}, none);
        CHECK(r.applied == 0 && r.refused == 1 && logHas(r, "reserve") && s.im.img == before);
    }
    {
        Img im;   // the parent is a CONTROL and the piece sits in its second slot
        const uint32_t ghost = im.object(), t = im.translate({im.object()}, 50, 50, 0), c = im.control(ghost, t);
        const uint32_t world = im.group({c}, 0, 0, 0, 5000, 1), g = im.grid(2, 2, 200, 0, 0, 400, 400);
        im.roots({world, g});
        sm::Map m;
        m.edits.push_back(turnBy(t - sm::kBase, 180));
        sm::Reserve res = im.reserve();
        const sm::Result r = sm::apply(im.mem(), uint32_t(im.img.size()), m, res);
        const uint32_t rot = im.r32(c + 8);
        CHECK(r.applied == 1 && im.r32(c + 4) == ghost && rot >= sm::kReserve && (im.r32(rot) & 63) == 4);
        CHECK(logHas(r, "CONTROL") && logHas(r, hexOff(c - sm::kBase)));
        CHECK(im.rf(rot + 0x10) == -1.f && im.rf(rot + 0x14) == 0.f && im.rf(rot + 0x24) == -1.f && im.rf(rot + 0x40) == 50.f);
    }
}

TEST(stagemap_remove_frees_grid_nodes)
{
    for (int order = 0; order < 2; ++order)
    {
        Stage s;   // a remove's list node goes to the pool, so a move needing one more cell takes no reserve, whatever the file order
        sm::Reserve res = s.im.reserve();
        const uint32_t start = res.next;
        const sm::Edit rm = removal(s.off(s.rB)), mv = moveTo(s.off(s.tA), 180, 100, 0);
        const sm::Result r = order ? s.run({mv, rm}, res) : s.run({rm, mv}, res);
        CHECK(r.applied == 2 && r.refused == 0 && r.relinked == 2 && r.fromReserve == 0 && res.next == start);
        CHECK(s.im.cellsOf(s.g, s.pA) == std::vector<int>({0, 1}) && s.im.cellsOf(s.g, s.pB).empty() && s.im.gridProblems(s.g) == 0);
    }
    {
        Stage s;   // a move that leaves a cell frees a node for the next move
        sm::Reserve res = s.im.reserve();
        const uint32_t start = res.next;
        const sm::Result r = s.run({moveTo(s.off(s.tA), 180, 100, 0)}, res);
        CHECK(r.applied == 1 && r.fromReserve == 1 && res.next > start);
        const uint32_t after = res.next;
        const sm::Result r2 = s.run({moveTo(s.off(s.tA), 100, 100, 0), moveTo(s.off(s.tC), 590, 700, 0)}, res);
        CHECK(r2.applied == 2 && r2.fromReserve == 0 && res.next == after);   // C now needs cells 14 and 15: A's spare node
        CHECK(s.im.cellsOf(s.g, s.pC) == std::vector<int>({14, 15}) && s.im.gridProblems(s.g) == 0);
    }
}

TEST(stagemap_refusals_write_nothing)
{
    {
        Stage s;   // a TRANSLATE with action data cannot become a ROTATE
        s.im.w32(s.tA + 0xC, 0x1000);
        const std::vector<uint8_t> before = s.im.img, rbefore = s.im.res;
        const sm::Result r = s.run({turnBy(s.off(s.tA), 90)});
        CHECK(r.applied == 0 && r.refused == 1 && logHas(r, "action data") && s.im.img == before && s.im.res == rbefore);
    }
    {
        Stage s;   // room for the ROTATE but not for the extra grid node: nothing written, reserve untouched
        const std::vector<uint8_t> before = s.im.img, rbefore = s.im.res;
        sm::Reserve res{sm::kReserve, sm::kReserve + 0x60};
        const sm::Result r = s.run({moveTo(s.off(s.tA), 198, 100, 0, 90)}, res);
        CHECK(r.applied == 0 && r.refused == 1 && logHas(r, "reserve"));
        CHECK(s.im.img == before && s.im.res == rbefore && res.next == sm::kReserve);
    }
    {
        Img im;   // a piece that also hangs from another node is refused
        const uint32_t t = im.translate({im.object()}, 10, 10, 0), other = im.group({t});
        const uint32_t world = im.group({t, other}, 0, 0, 0, 5000, 1), g = im.grid(2, 2, 200, 0, 0, 400, 400);
        im.roots({world, g});
        const std::vector<uint8_t> before = im.img;
        sm::Map m;
        m.edits.push_back(moveTo(t - sm::kBase, 50, 50, 0));
        sm::Reserve res = im.reserve();
        const sm::Result r = sm::apply(im.mem(), uint32_t(im.img.size()), m, res);
        CHECK(r.refused == 1 && logHas(r, hexOff(other - sm::kBase)) && im.img == before);
    }
}

TEST(stagemap_spheres_of_turned_branches)
{
    Img im;   // a ROTATE piece with two branches off its origin, turned 90 and moved: the GROUP above covers both
    const uint32_t a = im.group({im.object()}, 30, 0, 0, 5), b = im.group({im.object()}, 0, -20, 0, 8);
    const uint32_t r = im.rotate({a, b}, 0, 100, 100, 0), up = im.group({r}, 100, 100, 0, 40);
    const uint32_t world = im.group({up}, 0, 0, 0, 5000, 1), g = im.grid(2, 2, 200, 0, 0, 400, 400);
    im.roots({world, g});
    sm::Map m;
    m.edits.push_back(moveTo(r - sm::kBase, 150, 100, 0, 90));
    sm::Reserve res = im.reserve();
    const sm::Result out = sm::apply(im.mem(), uint32_t(im.img.size()), m, res);
    // a's center (30,0) turns to (0,30), b's (0,-20) to (20,0); at (150,100) they are 58.31 + 5 and 70 + 8 from (100,100)
    const double want = std::max(std::sqrt(50.0 * 50.0 + 30.0 * 30.0) + 5.0, 70.0 + 8.0);
    CHECK(out.applied == 1 && double(im.rf(up + 0x1C)) >= want && near(im.rf(up + 0x1C), float(want)));
    CHECK(im.rf(up + 0x10) == 100.f && im.rf(up + 0xC) == im.rf(up + 0x1C) * im.rf(up + 0x1C));
}

TEST(stagemap_turn_turns_hat_offsets)
{
    // a destructible piece whose states drop rooftop pickups: hat actions (ACT_HATPICKUP, offset at +0xC) reached through an
    // ACT_GROUP, an ACT_TRANSFORM and from two states; a third hat is shared with a destructible outside the piece
    struct Hats
    {
        Img im;
        uint32_t t, t2, g, h1, h2, h3;
        uint32_t act(uint16_t type, uint32_t size) { return im.act(type, size); }
        uint32_t hat(float r, float x, float y, float z) { return im.hat(r, x, y, z); }
        uint32_t destructible(const std::vector<std::pair<uint32_t, uint32_t>> &states) { return im.destructible(states); }
        Hats()
        {
            h1 = hat(400, 10, 0, 5), h2 = hat(300, 3, 4, 0), h3 = hat(200, 7, 0, 0);
            const uint32_t xf = act(sm::kActTransform, 0x50);
            im.w32(xf + 8, h1);
            const uint32_t grp = act(sm::kActGroup, 0x18);
            im.w32(grp + 8, 3), im.w32(grp + 0xC, xf), im.w32(grp + 0x10, h2), im.w32(grp + 0x14, h3);
            const uint32_t d = destructible({{im.object(), grp}, {im.object(), h1}});
            t = im.translate({d, im.polycoll()}, 100, 100, 0);
            t2 = im.translate({destructible({{im.object(), h3}})}, 300, 300, 0);
            const uint32_t world = im.group({t, t2}, 200, 200, 0, 5000, 1);
            g = im.grid(2, 2, 200, 0, 0, 400, 400);
            im.roots({world, g});
        }
        sm::Result run(const sm::Edit &e, sm::Reserve res)
        {
            sm::Map m;
            m.edits.push_back(e);
            return sm::apply(im.mem(), uint32_t(im.img.size()), m, res);
        }
    };
    {
        Hats h;
        CHECK(sm::hatActions(h.im.mem(), h.t) == std::vector<uint32_t>());   // a TRANSLATE is not a destructible
        const sm::Result r = h.run(turnBy(h.t - sm::kBase, 90), h.im.reserve());
        CHECK(r.applied == 1);
        CHECK(h.im.rf(h.h1 + 0xC) == 0.f && h.im.rf(h.h1 + 0x10) == 10.f && h.im.rf(h.h1 + 0x14) == 5.f && h.im.rf(h.h1 + 8) == 400.f);
        CHECK(h.im.rf(h.h2 + 0xC) == -4.f && h.im.rf(h.h2 + 0x10) == 3.f);
        CHECK(h.im.rf(h.h3 + 0xC) == 7.f && h.im.rf(h.h3 + 0x10) == 0.f && logHas(r, hexOff(h.h3 - sm::kBase)));   // shared: as is
    }
    {
        Hats h;   // 45 degrees, moved
        const sm::Result r = h.run(moveTo(h.t - sm::kBase, 150, 120, 0, 45), h.im.reserve());
        CHECK(r.applied == 1 && near(h.im.rf(h.h1 + 0xC), 7.0710678f) && near(h.im.rf(h.h1 + 0x10), 7.0710678f));
    }
    {
        Hats h;   // a move alone leaves the offsets (the destructible's place moves with the piece)
        const sm::Result r = h.run(moveTo(h.t - sm::kBase, 150, 120, 0), h.im.reserve());
        CHECK(r.applied == 1 && h.im.rf(h.h1 + 0xC) == 10.f && h.im.rf(h.h1 + 0x10) == 0.f && h.im.rf(h.h2 + 0xC) == 3.f);
    }
    {
        Hats h;   // a cycle in the action tree (a GROUP holding a TRANSFORM that points back at it) ends: each action once
        const uint32_t grp = h.act(sm::kActGroup, 0x18), xf = h.act(sm::kActTransform, 0x50), hat = h.hat(100, 1, 2, 3);
        h.im.w32(grp + 8, 3), h.im.w32(grp + 0xC, xf), h.im.w32(grp + 0x10, hat), h.im.w32(grp + 0x14, grp);
        h.im.w32(xf + 8, grp);
        const uint32_t d = h.destructible({{h.im.object(), grp}, {h.im.object(), xf}});
        CHECK(sm::hatActions(h.im.mem(), d) == std::vector<uint32_t>({hat}));
    }
    {
        Hats h;   // a refused turn (no room for the ROTATE) writes no offset either
        const std::vector<uint8_t> before = h.im.img;
        const sm::Result r = h.run(turnBy(h.t - sm::kBase, 90), sm::Reserve{sm::kReserve, sm::kReserve});
        CHECK(r.refused == 1 && h.im.img == before);
    }
}

// The edits' writes must give the same bits on every compiler and CPU (they reach guest RAM in a network match): a fixed set of
// edits on a fixed image, hashed. Measured on Windows (MSVC, x64); the Mac (clang, arm64) must print the same.
TEST(stagemap_same_bits_on_every_machine)
{
    Stage s;
    sm::Reserve res = s.im.reserve();
    const sm::Result r = s.run({moveTo(s.off(s.rB), 590.5f, 410.25f, 3.f, 45.f), moveTo(s.off(s.tA), 395.5f, 610.25f, 0.f, 15.f),
                                removal(s.off(s.tC))}, res);
    uint64_t h = sm::fnv1a(s.im.img.data(), s.im.img.size());
    for (uint8_t byte : s.im.res)
        h = (h ^ byte) * 0x100000001B3ull;
    std::printf("  stagemap digest %016llx\n", static_cast<unsigned long long>(h));
    CHECK(r.applied == 3 && s.im.gridProblems(s.g) == 0);
    // 0x035d31e0eff6d986 with the 1 MB reserve at 0x3EFF000; the 4 MB reserve (0x3BFF000) changes only the 5 reserve pointers the
    // edits write (each 0x300000 lower), no float bit
    CHECK(h == 0x7ebbfd393eca9736ull);
}

// The same for copies: a step 1 move, turned and plain copies of a world piece (one past the grid's edge, so the clamp's bits are
// in it too) and a turned pickup copy, image and reserve hashed
TEST(stagemap_copy_same_bits_on_every_machine)
{
    CopyStage s;
    sm::Map map;
    map.edits = {moveTo(s.off(s.tE), 650.5f, 150.25f, 0.f, 30.f)};
    map.copies = {copyAt(s.off(s.tD), 590.5f, 410.25f, 3.f, 45.f), copyAt(s.off(s.tD), 395.5f, 610.25f, 0.f, 15.f),
                  copyAt(s.off(s.tD), 100.f, 700.f, 0.f, 0.f), copyAt(s.off(s.tP), 650.f, 120.f, 10.f, 90.f),
                  copyAt(s.off(s.tD), 797.25f, 795.5f, 0.f, 30.f)};
    sm::Reserve res = s.im.reserve();
    const sm::Result r = s.run(map, res);
    uint64_t h = sm::fnv1a(s.im.img.data(), s.im.img.size());
    for (uint8_t byte : s.im.res)
        h = (h ^ byte) * 0x100000001B3ull;
    std::printf("  stagemap copy digest %016llx\n", static_cast<unsigned long long>(h));
    for (const std::string &l : r.log)
        std::printf("  %s\n", l.c_str());
    CHECK(r.applied == 1 && r.copied == 5 && r.copyRefused == 0 && s.im.gridProblems(s.g) == 0);
    CHECK(logHas(r, "copy " + hexOff(s.off(s.tD)) + " (edit 6): kept inside the collision grid's edge, at "));
    // measured on Windows, MSVC x64; the Mac (clang, arm64) must print the same
    CHECK(h == 0x946f58d8ff7f329eull);
}

TEST(stagemap_copy_set)
{
    CopyStage s;
    const sm::CopySet c = s.copySet(s.tD);
    CHECK(c.why == "");
        // preorder; the OBJECTs and the POLY_COLL are shared
    CHECK(c.nodes == std::vector<uint32_t>({s.tD, s.i1, s.d1, s.g0, s.sw, s.em, s.i2, s.d2}));
    CHECK(c.bytes == aligned(0x24) + aligned(0x10) + aligned(0x24) + aligned(0x2C) + aligned(0x14) + aligned(0xB0) + aligned(0x10)
                         + aligned(0x18));
    CHECK(c.interactives == 2 && c.destructibles == 2 && c.pickups == 0 && c.optionGroup == 0);
    const sm::CopySet p = s.copySet(s.tP);
    CHECK(p.why == "" && p.nodes == std::vector<uint32_t>({s.tP, s.gP, s.swP}));
    CHECK(p.interactives == 1 && p.destructibles == 0 && p.pickups == 1);
    CHECK(has(s.copySet(s.g0).why, "not a piece"));

    const sm::Mem m = s.im.mem();
    CHECK(sm::nodeSize(m, s.tD) == 0x24 && sm::nodeSize(m, s.d1) == 0x24 && sm::nodeSize(m, s.sw) == 0x14);
    CHECK(sm::nodeSize(m, s.em) == 0xB0 && sm::nodeSize(m, s.i1) == 0x10 && sm::nodeSize(m, s.g0) == 0x2C);
    CHECK(sm::nodeSize(m, s.obj0) == 0 && sm::nodeSize(m, s.pc) == 0);   // shared kinds are never copied
    const uint32_t ad = s.im.node(sm::kOpActionData, 0x10);
    CHECK(sm::nodeSize(m, ad) == 0);
    CHECK(sm::nodeSize(m, s.im.node(sm::kOpScriptAction, 0x60)) == 0x60);
    const uint32_t lod = s.im.lod(s.obj0, 0, 0, 0, 10), ctl = s.im.node(sm::kOpControl, 0x28);
    *s.im.at(ctl + 0xC) = 2;   // numVolumes 2: bspNodes[] +0x20
    CHECK(sm::nodeSize(m, lod) == 0x30 && sm::nodeSize(m, ctl) == 0x28);
    s.im.w32(lod + 4, 16);     // a LOD of 16 levels is past what slots() reads
    CHECK(sm::nodeSize(m, lod) == 0);
    CHECK(sm::dominoActions(m, s.d1) == std::vector<uint32_t>({s.dom, s.domOut}));
    CHECK(sm::dominoActions(m, s.tD).empty());
}

TEST(stagemap_copy_limits_refuse)
{
    // a child count past what slots() reads (4096) makes nodeSize 0 (the piece is refused), not a truncated copy
    {
        Img im(0x20000);
        const sm::Mem m = im.mem();
        const uint32_t t = im.node(sm::kOpTranslate, 0x1C + 4 * 4097), r = im.node(sm::kOpRotate, 0x50 + 4 * 4097),
                       sc = im.node(sm::kOpScale, 0x2C + 4 * 4097), gr = im.node(sm::kOpGroup, 0x20 + 4 * 4097),
                       d = im.node(sm::kOpDestructible, 0xC + 12 * 4097);
        im.w32(t + 8, 4097), im.w32(r + 8, 4097), im.w32(sc + 8, 4097), im.w16(gr + 8, 4097), im.w16(d + 0xA, 4097);
        CHECK(sm::nodeSize(m, t) == 0 && sm::nodeSize(m, r) == 0 && sm::nodeSize(m, sc) == 0);
        CHECK(sm::nodeSize(m, gr) == 0 && sm::nodeSize(m, d) == 0);
        im.w32(t + 8, 4096), im.w16(gr + 8, 4096);
        CHECK(sm::nodeSize(m, t) == 0x1C + 4 * 4096 && sm::nodeSize(m, gr) == 0x20 + 4 * 4096);
    }
    // action trees past 20 levels or 256 actions in a group refuse the piece instead of being read in part
    {
        CopyStage s;   // a chain of ACT_TRANSFORMs 22 deep under a1
        uint32_t next = s.hat1;
        for (int i = 0; i < 21; ++i)
        {
            const uint32_t xf = s.im.act(sm::kActTransform, 0x50);
            s.im.w32(xf + 8, next);
            next = xf;
        }
        s.im.w32(s.a1 + 0xC, next);
        CHECK(has(s.copySet(s.tD).why, "action tree"));
    }
    {
        CopyStage s;   // 20 levels are read whole: fine
        uint32_t next = s.hat1;
        for (int i = 0; i < 19; ++i)
        {
            const uint32_t xf = s.im.act(sm::kActTransform, 0x50);
            s.im.w32(xf + 8, next);
            next = xf;
        }
        s.im.w32(s.a1 + 0xC, next);
        CHECK(s.copySet(s.tD).why == "");
    }
    {
        CopyStage s;   // an ACT_GROUP of 257 actions
        const uint32_t big = s.im.act(sm::kActGroup, 0xC + 4 * 257);
        s.im.w32(big + 8, 257);
        s.im.w32(s.a1 + 0xC, big);
        CHECK(has(s.copySet(s.tD).why, "action tree"));
        s.im.w32(big + 8, 256);
        CHECK(s.copySet(s.tD).why == "");
    }
}

TEST(stagemap_copy_refusals)
{
    auto why = [](const std::function<void(CopyStage &)> &change) {
        CopyStage s;
        change(s);
        return s.copySet(s.tD).why;
    };
    auto setId = [](CopyStage &s, uint32_t n, uint32_t id1) { s.im.w32(n, (s.im.r32(n) & 0x3FFFFu) | id1 << 18); };
    // special ids on a node traversal hands to dbProcInteractive
    CHECK(has(why([&](CopyStage &s) { setId(s, s.g0, 0x2134); }), "special id 0x2134"));
    CHECK(has(why([&](CopyStage &s) { setId(s, s.g0, 0x20); }), "special id 0x20"));
    // INTERACTIVE_STATE and DESTRUCTIBLE: only the fixed id1 tables
    CHECK(has(why([&](CopyStage &s) { setId(s, s.i1, 0x27AE); }), "fixed id1 table"));
    CHECK(has(why([&](CopyStage &s) { setId(s, s.i2, 0x2715); }), "fixed id1 table"));
    CHECK(why([&](CopyStage &s) { setId(s, s.i1, 0x2134); }) == "");   // any other id on a 39 goes to AddDestruct only
    {
        CopyStage s;   // a refusal carries no partial copy set or counts
        setId(s, s.i2, 0x2715);
        const sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why != "" && c.nodes.empty() && c.bytes == 0 && c.interactives == 0 && c.destructibles == 0);
    }
    CHECK(has(why([&](CopyStage &s) { setId(s, s.em, 0x1D4C); }), "locator"));
    // animated
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.g0 + 0x24, s.im.holder(sm::kOpAnimXform, 4, 0xCC, {s.pc})); }), "animated"));
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.a1 + 0xC, s.im.act(sm::kActAnim, 0x10)); }), "animated"));
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.a1 + 0xC, s.im.act(sm::kActRunScript, 0x10)); }), "script action"));
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.a1 + 0xC, s.im.act(sm::kActCinema, 0x10)); }), "script action"));
    // a CHAR_INSTANCE above (amendment A1): refused only when an OBJECT of the piece reads an animation channel
    auto underChar = [&](CopyStage &s) { s.im.w32(s.mid + 0x20, s.im.holder(sm::kOpCharInstance, 0x28, 0x2C, {s.tD})); };
    CHECK(why(underChar) == "");
    CHECK(has(why([&](CopyStage &s) { underChar(s), s.im.w16(s.obj0 + 0x30, 0); }), "animated"));
    CHECK(has(why([&](CopyStage &s) { underChar(s), s.im.w16(s.obj2 + 0x3C, 3); }), "animated"));
        // a channel without a CHAR_INSTANCE above: the copy is no worse than the original
    CHECK(why([&](CopyStage &s) { s.im.w16(s.obj0 + 0x30, 0); }) == "");
    // an ACTION_DATA between the root and a per-instance node would be in the copy set
    CHECK(has(why([&](CopyStage &s) {
              const uint32_t ad = s.im.node(sm::kOpActionData, 0x10);
              s.im.w32(ad + 4, s.i1);
              s.im.w32(s.tD + 0x1C, ad);
          }), "ACTION_DATA"));
    // a GROUP over a SWITCH held twice inside the piece: first by g0 (so it is copied), then by `plain` (not copied). The copy's
    // `plain` would still reach the original SWITCH and share its state: refused.
    CHECK(has(why([&](CopyStage &s) {
              const uint32_t gs = s.im.group({s.im.switchNode({s.im.stillObject()})});
              s.im.w32(s.g0 + 0x24, gs);                                        // g0 = {sw, gs, em}
              s.im.w32(s.d1 + 0x18, s.im.group({gs, s.im.stillObject()}));      // state 1 = plain {gs, OBJECT}
          }), "shared node inside"));
    CHECK(why([&](CopyStage &s) {   // `plain` reached first: then both holders (plain, d1) are copied and map to one gs'
              const uint32_t gs = s.im.group({s.im.switchNode({s.im.stillObject()})});
              s.im.w32(s.g0 + 0x24, s.im.group({gs, s.im.stillObject()}));      // g0 = {sw, plain, em}
              s.im.w32(s.d1 + 0x18, gs);                                        // state 1 = gs
          }) == "");
    CHECK(why([&](CopyStage &s) { s.im.w32(s.d1 + 0x18, s.obj0); }) == "");   // a shared OBJECT held twice is fine
    // the other animation kinds: SKEL_BONE and ANIM_SCALE in the piece, a SKEL_BONE above it; an ANIM_SCALE above transforms
    // it, so the source is not a piece at all
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.g0 + 0x24, s.im.holder(sm::kOpSkelBone, 4, 0xC, {s.pc})); }), "animated"));
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.g0 + 0x24, s.im.holder(sm::kOpAnimScale, 4, 0x60, {s.pc})); }), "animated"));
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.mid + 0x20, s.im.holder(sm::kOpSkelBone, 4, 0xC, {s.tD})); }), "animated"));
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.mid + 0x20, s.im.holder(sm::kOpAnimScale, 4, 0x60, {s.tD})); }), "not a piece"));
    // lost context (amendment A3): part of a destructible's state, mode-masked GROUP above
    CHECK(has(why([&](CopyStage &s) { s.im.w32(s.mid + 0x20, s.im.istate(s.im.destructible({{s.tD, 0}}))); }),
              "in a destructible's state"));
    CHECK(has(why([&](CopyStage &s) { s.im.w16(s.mid + 0xA, 0x21); }), "mode-masked"));
    CHECK(has(why([&](CopyStage &s) { s.im.w16(s.mid + 0xA, 0x81); }), "mode-masked"));
    CHECK(has(why([&](CopyStage &s) { s.im.w16(s.mid + 0xA, 0x11); }), "mode-masked"));   // 0x10: modes 2, 4, 5
    CHECK(why([&](CopyStage &s) { s.im.w16(s.mid + 0xA, 0x4F); }) == "");                 // other low bits are not masks
    {
        CopyStage s;   // a GROUP above with collision-option flags: copyable, named for the log
        s.im.w16(s.mid + 0xA, 0x0102);
        const sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why == "" && c.optionGroup == s.mid);
    }
    // fine: a destructible-range id1 on an OBJECT, a pickup id on a GROUP
    {
        CopyStage s;
        setId(s, s.obj0, 0x1500);
        const sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why == "" && c.destructibles == 3 && c.interactives == 2);
    }
    {
        CopyStage s;
        setId(s, s.g0, 0x12C5);
        const sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why == "" && c.interactives == 3 && c.pickups == 1);
    }
    // a pickup id1 on a node that is not a GROUP: still per-instance (Pickup::init writes into it). A SWITCH is copied (CANYON1
    // has two); an OBJECT cannot be, so the piece is refused rather than two pickups sharing one node
    {
        CopyStage s;
        setId(s, s.sw, 0x12C3);
        const sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why == "" && c.interactives == 3 && c.pickups == 1);
        CHECK(std::find(c.nodes.begin(), c.nodes.end(), s.sw) != c.nodes.end());
    }
    {
        CopyStage s;
        setId(s, s.obj2, 0x12C3);
        CHECK(has(s.copySet(s.tD).why, "kind 0 (OBJECT)"));
    }
}

TEST(stagemap_stage_counts)
{
    CopyStage s;
    {
        const sm::Mem m = s.im.mem();
        const sm::StageCounts c = sm::stageCounts(m, sm::parents(m));
        CHECK(c.interactives == 4 && c.destructibles == 3 && c.pickups == 1);   // i1, i2, iE + gP; d1, d2, dE; gP
    }
    s.im.w32(s.tE, (s.im.r32(s.tE) & ~63u) | sm::kOpNull);   // a NULL ends its branch
    const sm::Mem m = s.im.mem();
    const sm::StageCounts c = sm::stageCounts(m, sm::parents(m));
    CHECK(c.interactives == 3 && c.destructibles == 2 && c.pickups == 1);
}

TEST(stagemap_counts_follow_paths)
{
    // the game registers a node once per path dbsTraverse takes to it, so a node held twice counts twice
    {
        CopyStage s;
        s.im.w32(s.oldLast + 0x20, s.iE);   // iE (and dE under it) held by tE and by oldLast
        const sm::Mem m = s.im.mem();
        const sm::StageCounts c = sm::stageCounts(m, sm::parents(m));
        CHECK(c.interactives == 5 && c.destructibles == 4 && c.pickups == 1);
    }
    {
        CopyStage s;   // inside a piece: an OBJECT with a destructible id1 under sw and under d1's state 1
        s.im.w32(s.obj0, 0x1500u << 18);
        s.im.w32(s.d1 + 0x18, s.obj0);
        const sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why == "" && c.destructibles == 4 && c.interactives == 2);
        const sm::Mem m = s.im.mem();
        CHECK(sm::stageCounts(m, sm::parents(m)).destructibles == 5);
    }
}

TEST(stagemap_copy_build)
{
    {
        CopyStage s;   // one copy of tD at (500, 500, 0): a wrapper G in the world root's last slot = {oldLast, tD'}
        s.im.w32(s.world + 4, 0x1234);   // a GS ctx: the wrapper hands the root's on
        const std::vector<uint8_t> before = s.im.img;
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tD, s.copySet(s.tD).nodes}};
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        sm::Reserve res = s.im.reserve();
        const sm::Result r = s.run(map, res);
        CHECK(r.copied == 1 && r.copyRefused == 0 && r.applied == 0 && r.refused == 0);
        const uint32_t G = s.im.r32(s.world + 0x28);
        CHECK(s.inReserve(G) && G < res.next);
        CHECK(s.im.r32(G) == sm::kOpGroup && s.im.r32(G + 4) == 0x1234 && s.im.r16(G + 8) == 2 && s.im.r16(G + 0xA) == 3);
        CHECK(s.im.r32(G + 0x20) == s.oldLast);
        const uint32_t root = s.im.r32(G + 0x24);
        CHECK(s.inReserve(root) && s.im.r32(root) == s.im.r32(s.tD));
        CHECK(s.im.rf(root + 0x10) == 500.f && s.im.rf(root + 0x14) == 500.f && s.im.rf(root + 0x18) == 0.f);
        // follow the copy: its per-instance nodes are in the reserve, its geometry is the image's
        const uint32_t i1c = s.im.r32(root + 0x1C), i2c = s.im.r32(root + 0x20), d1c = s.im.r32(i1c + 0xC),
                       g0c = s.im.r32(d1c + 0xC), swc = s.im.r32(g0c + 0x20), emc = s.im.r32(g0c + 0x28),
                       d2c = s.im.r32(i2c + 0xC);
        const std::vector<uint32_t> copies{root, i1c, d1c, g0c, swc, emc, i2c, d2c};
        bool inRes = true;
        for (uint32_t c : copies)
            inRes = inRes && s.inReserve(c);
        CHECK(inRes && std::set<uint32_t>(copies.begin(), copies.end()).size() == 8);
        CHECK(s.im.r32(g0c + 0x24) == s.pc && s.im.r32(swc + 0xC) == s.obj0 && s.im.r32(swc + 0x10) == s.obj1 &&
              s.im.r32(d1c + 0x18) == s.obj2 && s.im.r32(d2c + 0xC) == s.im.r32(s.d2 + 0xC));
        const std::map<uint32_t, uint32_t> to{{s.tD, root}, {s.i1, i1c}, {s.d1, d1c}, {s.g0, g0c}, {s.sw, swc}, {s.em, emc},
                                              {s.i2, i2c}, {s.d2, d2c}};
        CHECK(copiedBytes(s.im, s.tD, root, 0x10, to) && s.im.r32(root + 0x1C) == i1c && s.im.r32(root + 0x20) == i2c);
        // d1's state 0 actions (+0x10) hold an in-piece domino: the copy has its own tree there (stagemap_copy_actions)
        CHECK(copiedBytes(s.im, s.d1, d1c, 0x10, to) && s.inReserve(s.im.r32(d1c + 0x10)) &&
              std::memcmp(s.im.at(d1c + 0x14), s.im.at(s.d1 + 0x14), 0x10) == 0);
        CHECK(copiedBytes(s.im, s.i1, i1c, 0x10, to) &&
              copiedBytes(s.im, s.g0, g0c, 0x2C, to) && copiedBytes(s.im, s.sw, swc, 0x14, to) &&
              copiedBytes(s.im, s.em, emc, 0xB0, to) && copiedBytes(s.im, s.i2, i2c, 0x10, to) && copiedBytes(s.im, s.d2, d2c, 0x18, to));
        CHECK(copiesStandAlone(s.im, r, sets));
        // the image: only the world root's last slot, its sphere fields and the grid's cell heads changed (the copy's list
        // node is in the reserve: no step 1 remove left any in the pool)
        std::vector<uint8_t> after = s.im.img;
        const uint32_t w = s.world - sm::kBase, gh = s.g + 0x20 - sm::kBase;
        std::copy(before.begin() + w + 0xC, before.begin() + w + 0x20, after.begin() + w + 0xC);
        std::copy(before.begin() + w + 0x28, before.begin() + w + 0x2C, after.begin() + w + 0x28);
        std::copy(before.begin() + gh, before.begin() + gh + 4 * 16, after.begin() + gh);
        CHECK(after == before);
        CHECK(r.copies.size() == 1);
        if (r.copies.size() == 1)
        {
            const sm::CopyInfo &c = r.copies[0];
            CHECK(c.source == s.tD && c.root == root && c.istate == i1c && c.destructible == d1c && c.sourceIstate == s.i1);
        }
        // G's centre is the world root's (its radius: the next block); the world root's 600 already covers the copy
        CHECK(s.im.rf(G + 0x10) == 400.f && s.im.rf(G + 0x14) == 400.f && s.im.rf(G + 0x18) == 0.f);
        CHECK(s.im.rf(s.world + 0x1C) == 600.f && s.im.rf(s.world + 0xC) == 360000.f);
    }
    // G's radius covers each child, whichever is farther: oldLast (a GROUP at (0, 0, 0), 565.7 from the centre) with radius
    // 200 or 5, and the copy (g0's sphere is local to tD: g0' at (800, 800) r20, 565.7 + 20 from the centre)
    for (const float oldR : {200.f, 5.f})
    {
        CopyStage s;
        s.im.sphere(s.oldLast, 0, 0, 0, oldR);
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        const uint32_t G = s.im.r32(s.world + 0x28);
        const double d = std::sqrt(400.0 * 400.0 * 2.0), oldNeed = d + oldR, copyNeed = d + 20.0;
        const float rad = s.im.rf(G + 0x1C);
        CHECK(r.copied == 1 && double(rad) >= oldNeed && double(rad) >= copyNeed);
        CHECK(near(rad, float(std::max(oldNeed, copyNeed))) && s.im.rf(G + 0xC) == rad * rad);
    }
    {
        CopyStage s;   // two copies: one wrapper per apply, numKids 3, each copy its own nodes
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tD, s.copySet(s.tD).nodes}};
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0)};
        const sm::Result r = s.run(map);
        const uint32_t G = s.im.r32(s.world + 0x28);
        CHECK(r.copied == 2 && s.im.r16(G + 8) == 3 && s.im.r32(G + 0x20) == s.oldLast);
        const uint32_t c1 = s.im.r32(G + 0x24), c2 = s.im.r32(G + 0x28);
        CHECK(s.inReserve(c1) && s.inReserve(c2) && c1 != c2 && s.im.r32(c1 + 0x1C) != s.im.r32(c2 + 0x1C));
        CHECK(s.im.rf(c2 + 0x10) == 100.f && s.im.rf(c2 + 0x14) == 500.f);
        CHECK(r.copies.size() == 2 && copiesStandAlone(s.im, r, sets));
    }
    {
        CopyStage s;   // turned 90: the TRANSLATE's copy is a ROTATE
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 90));
        const sm::Result r = s.run(map);
        const uint32_t root = s.im.r32(s.im.r32(s.world + 0x28) + 0x24);
        CHECK(r.copied == 1 && s.inReserve(root));
        CHECK(s.im.r32(root) == ((s.im.r32(s.tD) & ~63u) | sm::kOpRotate) && s.im.r32(root + 8) == 2 && s.im.r32(root + 0xC) == 0);
        const float want[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 500, 500, 0, 1};
        bool ok = true;
        for (uint32_t k = 0; k < 16; ++k)
            ok = ok && s.im.rf(root + 0x10 + 4 * k) == want[k];
        CHECK(ok);
        const uint32_t i1c = s.im.r32(root + 0x50), i2c = s.im.r32(root + 0x54);
        CHECK(s.inReserve(i1c) && s.inReserve(i2c) && s.im.r32(i1c) == s.im.r32(s.i1) && s.inReserve(s.im.r32(i1c + 0xC)));
    }
    {
        CopyStage s;   // a TRANSLATE with action data cannot be turned into a ROTATE (step 1's rule)
        s.im.w32(s.tD + 0xC, s.a1);
        const std::vector<uint8_t> before = s.im.img;
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 90));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 0 && r.copyRefused == 1 && logHas(r, "action data") && s.im.img == before);
    }
    {
        CopyStage s;   // a ROTATE source at 30 degrees: unturned it keeps its rows, turned 90 they are 120 degrees
        const uint32_t rB = s.im.rotate({s.im.stillObject()}, 30, 200, 600, 0);
        s.im.w32(s.oldLast + 0x20, rB);
        sm::Map map;
        map.copies = {copyAt(s.off(rB), 100, 700, 0), copyAt(s.off(rB), 100, 700, 0, 90)};
        const sm::Result r = s.run(map);
        const uint32_t G = s.im.r32(s.world + 0x28), c0 = s.im.r32(G + 0x24), c1 = s.im.r32(G + 0x28);
        CHECK(r.copied == 2 && s.im.r32(G + 0x20) == s.oldLast && (s.im.r32(c0) & 63) == sm::kOpRotate);
        bool same = true;
        for (uint32_t k = 0; k < 12; ++k)
            same = same && s.im.r32(c0 + 0x10 + 4 * k) == s.im.r32(rB + 0x10 + 4 * k);
        CHECK(same && s.im.rf(c0 + 0x40) == 100.f && s.im.rf(c0 + 0x44) == 700.f && s.im.rf(c0 + 0x4C) == 1.f);
        CHECK(s.im.r32(c0 + 0x50) == s.im.r32(rB + 0x50));   // its only child is a shared OBJECT
        const M4 want = rotZ(120.f);
        bool ok = true;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 4; ++j)
                ok = ok && near(s.im.rf(c1 + 0x10 + 16 * uint32_t(i) + 4 * uint32_t(j)), want.m[i][j], 1e-5f);
        CHECK(ok && s.im.rf(c1 + 0x40) == 100.f && s.im.rf(c1 + 0x44) == 700.f);
    }
    {
        CopyStage s;   // a level pickup: its wrapper goes in the pickup root's last slot, the world root is untouched
        s.im.w32(s.proot + 4, 0x55);
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tP, s.copySet(s.tP).nodes}, {s.tD, s.copySet(s.tD).nodes}};
        sm::Map map;
        map.copies = {copyAt(s.off(s.tP), 600, 600, 10), copyAt(s.off(s.tD), 500, 500, 0)};
        const sm::Result r = s.run(map);
        CHECK(r.copied == 2);
        const uint32_t W = s.im.r32(s.proot + 0x20);
        CHECK(s.inReserve(W) && s.im.r32(W) == sm::kOpGroup && s.im.r32(W + 4) == 0x55 && s.im.r16(W + 8) == 2 &&
              s.im.r16(W + 0xA) == s.im.r16(s.proot + 0xA));
        CHECK(s.im.r32(W + 0x20) == s.holder);
        const uint32_t tPc = s.im.r32(W + 0x24), gPc = s.im.r32(tPc + 0x1C), swPc = s.im.r32(gPc + 0x20);
        CHECK(s.inReserve(tPc) && s.inReserve(gPc) && s.inReserve(swPc) && s.im.r32(gPc) == s.im.r32(s.gP));
        CHECK(s.im.rf(tPc + 0x10) == 600.f && s.im.rf(tPc + 0x14) == 600.f && s.im.rf(tPc + 0x18) == 10.f);
        CHECK(s.im.r32(swPc + 0xC) == s.im.r32(s.swP + 0xC));
        const uint32_t G = s.im.r32(s.world + 0x28);   // the world copy has its own wrapper
        CHECK(G != W && s.inReserve(G) && s.im.r16(G + 8) == 2 && s.im.r32(G + 0x20) == s.oldLast);
        CHECK(copiesStandAlone(s.im, r, sets));
        CHECK(r.copies.size() == 2 && r.copies[0].istate == 0 && r.copies[0].destructible == 0 && r.copies[0].root == tPc);
    }
    {
        CopyStage s;   // a copy outside the world root's sphere widens it, centre kept
        s.im.sphere(s.world, 400, 400, 0, 100);
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        // g0's sphere (300, 300, 0) is local to tD, so g0' is at (800, 800) r20 in the world
        const double want = std::sqrt(400.0 * 400.0 * 2.0) + 20.0;
        const float rad = s.im.rf(s.world + 0x1C);
        CHECK(r.copied == 1 && double(rad) >= want && near(rad, float(want)) && s.im.rf(s.world + 0xC) == rad * rad);
        CHECK(s.im.rf(s.world + 0x10) == 400.f && s.im.rf(s.world + 0x14) == 400.f && s.im.rf(s.world + 0x18) == 0.f);
    }
    {
        CopyStage s;   // a source that step 1 removes: its copy is built from the disc state
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tD, s.copySet(s.tD).nodes}};
        sm::Map map;
        map.edits.push_back(removal(s.off(s.tD)));
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        const uint32_t root = s.im.r32(s.im.r32(s.world + 0x28) + 0x24);
        CHECK(r.applied == 1 && r.copied == 1 && (s.im.r32(s.tD) & 63) == sm::kOpNull);
        CHECK((s.im.r32(root) & 63) == sm::kOpTranslate && s.im.rf(root + 0x10) == 500.f && s.im.r32(root + 8) == 2);
        CHECK(s.inReserve(s.im.r32(root + 0x1C)) && s.inReserve(s.im.r32(root + 0x20)) && copiesStandAlone(s.im, r, sets));
    }
    {
        CopyStage s;   // a source that step 1 moves and turns: the copy is the disc TRANSLATE, at the copy's place
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tD, s.copySet(s.tD).nodes}};
        sm::Map map;
        map.edits.push_back(moveTo(s.off(s.tD), 100, 500, 0, 90));
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        const uint32_t root = s.im.r32(s.im.r32(s.world + 0x28) + 0x24), moved = s.im.r32(s.mid + 0x20);
        CHECK(r.applied == 1 && r.copied == 1 && (s.im.r32(moved) & 63) == sm::kOpRotate);
        CHECK((s.im.r32(root) & 63) == sm::kOpTranslate && s.im.rf(root + 0x10) == 500.f && s.im.rf(root + 0x14) == 500.f);
        CHECK(s.im.r32(root + 8) == 2 && s.inReserve(s.im.r32(root + 0x1C)) && s.im.r32(root + 0x1C) != s.i1);
        CHECK(copiesStandAlone(s.im, r, sets));
    }
    {
        CopyStage s;   // a node with two copied holders (copySet allows it): both copies of the holders hold one copy of it
        const uint32_t gs = s.im.group({s.im.switchNode({s.im.stillObject()})});
        const uint32_t plain = s.im.group({gs, s.im.stillObject()});
        s.im.w32(s.g0 + 0x24, plain);   // g0 = {sw, plain, em}
        s.im.w32(s.d1 + 0x18, gs);      // state 1 = gs
        const sm::CopySet cs = s.copySet(s.tD);
        CHECK(cs.why == "");
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tD, cs.nodes}};
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        const uint32_t root = s.im.r32(s.im.r32(s.world + 0x28) + 0x24), d1c = s.im.r32(s.im.r32(root + 0x1C) + 0xC);
        const uint32_t g0c = s.im.r32(d1c + 0xC), plainc = s.im.r32(g0c + 0x24), gsc = s.im.r32(d1c + 0x18);
        CHECK(r.copied == 1 && s.inReserve(plainc) && s.inReserve(gsc) && gsc != gs && s.im.r32(plainc + 0x20) == gsc);
        CHECK(copiesStandAlone(s.im, r, sets));
    }
    // amendment A2: the wrapper takes the root's last slot as it is after step 1. Here the pickup root's last (only) child is
    // the pickup piece tP itself; step 1 turns it (its slot gets a reserve ROTATE) or removes it (NULL)
    for (int turnIt = 0; turnIt < 2; ++turnIt)
    {
        CopyStage s;
        s.im.w32(s.proot + 0x20, s.tP);   // proot = {tP}
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tP, s.copySet(s.tP).nodes}};
        sm::Map map;
        map.edits.push_back(turnIt ? turnBy(s.off(s.tP), 90) : removal(s.off(s.tP)));
        map.copies.push_back(copyAt(s.off(s.tP), 600, 600, 10));
        const sm::Result r = s.run(map);
        const uint32_t W = s.im.r32(s.proot + 0x20), live = s.im.r32(W + 0x20), copy = s.im.r32(W + 0x24);
        CHECK(r.applied == 1 && r.copied == 1 && s.inReserve(W) && s.im.r16(W + 8) == 2);
        if (turnIt)
            CHECK(live != s.tP && s.inReserve(live) && (s.im.r32(live) & 63) == sm::kOpRotate && s.im.r32(live + 0x50) == s.gP);
        else
            CHECK(live == s.tP && (s.im.r32(s.tP) & 63) == sm::kOpNull);
        CHECK(copy != live && (s.im.r32(copy) & 63) == sm::kOpTranslate && s.im.rf(copy + 0x10) == 600.f && s.inReserve(s.im.r32(copy + 0x1C)));
        CHECK(copiesStandAlone(s.im, r, sets));
    }
    {
        CopyStage s;   // CopyInfo pairs the first DESTRUCTIBLE with its INTERACTIVE_STATE, not the first one in preorder
        const uint32_t iX = s.im.istate(s.im.stillObject());
        s.im.w32(s.tD + 0x1C, s.im.group({iX, s.i1}));   // tD = {GROUP {iX, i1}, i2}: iX comes first and holds no DESTRUCTIBLE
        CHECK(s.copySet(s.tD).why == "");
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        CHECK(r.copies.size() == 1);
        if (r.copies.size() == 1)
        {
            const sm::CopyInfo &c = r.copies[0];
            CHECK(c.sourceIstate == s.i1 && s.inReserve(c.istate) && s.im.r32(c.istate + 0xC) == c.destructible && s.inReserve(c.destructible));
        }
    }
}

TEST(stagemap_copy_actions)
{
    // d1's states: 0 -> a1 = {hat1, dom (-> i2, state 1), domOut (-> iE)}, 1 -> hat1. The copy's destructible d1' (d1 + 0xC is
    // its state 0 child g0, + 0x10 its actions, + 0x1C state 1's actions)
    auto rootOf = [](CopyStage &s, size_t k) { return s.im.r32(s.im.r32(s.world + 0x28) + 0x24 + 4 * uint32_t(k)); };
    auto destOf = [](CopyStage &s, uint32_t root) {
        const uint32_t first = (s.im.r32(root) & 63) == sm::kOpRotate ? 0x50 : 0x1C;
        return s.im.r32(s.im.r32(root + first) + 0xC);   // root -> i1' -> d1'
    };
    {
        CopyStage s;   // unturned: the group holding the in-piece domino is private, the hat and the outside domino shared
        const std::vector<uint8_t> a1Before(s.im.img.begin() + (s.a1 - sm::kBase), s.im.img.begin() + (s.a1 - sm::kBase) + 0x18);
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 1);
        const uint32_t root = rootOf(s, 0), d1c = destOf(s, root), i2c = s.im.r32(root + 0x20);
        CHECK(s.inReserve(d1c) && d1c != s.d1 && s.inReserve(i2c));
        const uint32_t a1c = s.im.r32(d1c + 0x10);
        CHECK(s.inReserve(a1c) && a1c != s.a1 && s.im.r16(a1c) == sm::kActGroup && s.im.r32(a1c + 8) == 3);
        const uint32_t domc = s.im.r32(a1c + 0x10);
        CHECK(s.im.r32(a1c + 0xC) == s.hat1 && s.im.r32(a1c + 0x14) == s.domOut);
        CHECK(s.inReserve(domc) && s.im.r16(domc) == sm::kActDomino && s.im.r32(domc + 8) == 1 && s.im.r32(domc + 0xC) == i2c);
        CHECK(s.im.r32(d1c + 0x1C) == s.hat1);   // no domino under state 1: shared
        CHECK(logHas(r, "copy " + hexOff(s.off(s.tD)) + ": 1 domino actions point outside it"));
        CHECK(std::equal(a1Before.begin(), a1Before.end(), s.im.img.begin() + (s.a1 - sm::kBase)));
        CHECK(s.im.r32(s.dom + 0xC) == s.i2 && s.im.r32(s.d1 + 0x10) == s.a1);
    }
    {
        CopyStage s;   // turned 90, twice: each copy has its own turned hat, reached from both states (one memo per copy)
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0, 90), copyAt(s.off(s.tD), 100, 500, 0, 90)};
        const sm::Result r = s.run(map);
        CHECK(r.copied == 2);
        std::vector<uint32_t> hats;
        for (size_t k = 0; k < 2; ++k)
        {
            const uint32_t root = rootOf(s, k), d1c = destOf(s, root), i2c = s.im.r32(root + 0x54);
            const uint32_t a1c = s.im.r32(d1c + 0x10), hatc = s.im.r32(a1c + 0xC), domc = s.im.r32(a1c + 0x10);
            CHECK(s.inReserve(a1c) && s.inReserve(hatc) && hatc != s.hat1 && s.im.r32(d1c + 0x1C) == hatc);
            CHECK(s.im.r16(hatc) == sm::kActHatPickup && s.im.rf(hatc + 8) == 300.f);
            CHECK(s.im.rf(hatc + 0xC) == 0.f && s.im.rf(hatc + 0x10) == 10.f && s.im.rf(hatc + 0x14) == 5.f);
            CHECK(s.inReserve(domc) && s.im.r32(domc + 0xC) == i2c && s.im.r32(a1c + 0x14) == s.domOut);
            hats.push_back(hatc);
        }
        CHECK(hats.size() == 2 && hats[0] != hats[1]);
        CHECK(s.im.rf(s.hat1 + 0xC) == 10.f && s.im.rf(s.hat1 + 0x10) == 0.f && s.im.rf(s.hat1 + 0x14) == 5.f);
        int noted = 0;
        for (const std::string &l : r.log)
            noted += has(l, "domino actions point outside it");
        CHECK(noted == 1);   // once per source
    }
    {
        CopyStage s;   // the hat offset turns with the same bits as a step 1 turn (45 degrees)
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 45));
        const sm::Result r = s.run(map);
        CopyStage t;
        t.run([&] { sm::Map m2; m2.edits.push_back(turnBy(t.off(t.tD), 45)); return m2; }());
        const uint32_t hatc = s.im.r32(s.im.r32(destOf(s, rootOf(s, 0)) + 0x10) + 0xC);
        CHECK(r.copied == 1 && s.im.r32(hatc + 0xC) == t.im.r32(t.hat1 + 0xC) && s.im.r32(hatc + 0x10) == t.im.r32(t.hat1 + 0x10));
    }
    {
        CopyStage s;   // a cycle (an ACT_GROUP holding an ACT_TRANSFORM that points back at it) ends, each action copied once
        const uint32_t grp = s.im.act(sm::kActGroup, 0x18), xf = s.im.act(sm::kActTransform, 0x50), hat = s.im.hat(100, 1, 2, 3);
        s.im.w32(grp + 8, 3), s.im.w32(grp + 0xC, xf), s.im.w32(grp + 0x10, hat), s.im.w32(grp + 0x14, grp);
        s.im.w32(xf + 8, grp);
        s.im.w32(s.d1 + 0x1C, xf);
        CHECK(s.copySet(s.tD).why == "");
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 90));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 1);
        const uint32_t d1c = destOf(s, rootOf(s, 0)), xfc = s.im.r32(d1c + 0x1C), grpc = s.im.r32(xfc + 8);
        CHECK(s.inReserve(xfc) && s.inReserve(grpc) && s.im.r16(xfc) == sm::kActTransform && s.im.r16(grpc) == sm::kActGroup);
        CHECK(s.im.r32(grpc + 0xC) == xfc && s.im.r32(grpc + 0x14) == grpc);
        const uint32_t hatc = s.im.r32(grpc + 0x10);
        CHECK(s.inReserve(hatc) && s.im.rf(hatc + 0xC) == -2.f && s.im.rf(hatc + 0x10) == 1.f && s.im.rf(hatc + 0x14) == 3.f);
        CHECK(std::memcmp(s.im.at(xfc + 0xC), s.im.at(xf + 0xC), 0x50 - 0xC) == 0);   // the TRANSFORM's matrix copied whole
        CHECK(s.im.r32(grp + 0xC) == xf && s.im.r32(xf + 8) == grp && s.im.rf(hat + 0xC) == 1.f);
    }
    {
        CopyStage s;   // a domino aimed at a node inside the piece that is not copied (an OBJECT with id2 set): kept, logged
        s.im.w32(s.obj0, s.im.r32(s.obj0) | 5u << 7);
        const uint32_t domIn = s.im.domino(s.obj0, 0), grp = s.im.actGroup({s.hat1, domIn});
        s.im.w32(s.d1 + 0x1C, grp);
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 90));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 1);
        const uint32_t grpc = s.im.r32(destOf(s, rootOf(s, 0)) + 0x1C);
        CHECK(s.inReserve(grpc) && s.inReserve(s.im.r32(grpc + 0xC)) && s.im.r32(grpc + 0x10) == domIn);
        CHECK(logHas(r, "copy " + hexOff(s.off(s.tD)) + ": domino action " + hexOff(s.off(domIn)) + " targets " +
                            hexOff(s.off(s.obj0)) + " inside it, not copied"));
    }
    {
        CopyStage s;   // atomic: no room for the last action of the copy: refused, nothing written, its reserve given back
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 90));
        uint32_t actionsEnd = 0;   // where the copy's actions (a1', hat1', dom') end in a reserve with room for everything
        {
            CopyStage t;
            sm::Reserve full = t.im.reserve();
            CHECK(t.run(map, full).copied == 1);
            const uint32_t a1c = t.im.r32(destOf(t, rootOf(t, 0)) + 0x10);
            const uint32_t hatc = t.im.r32(a1c + 0xC), domc = t.im.r32(a1c + 0x10);
            actionsEnd = std::max({a1c + aligned(0x18), hatc + aligned(0x18), domc + aligned(0x10)});
            CHECK(t.inReserve(a1c) && t.inReserve(hatc) && t.inReserve(domc) && actionsEnd <= full.next);
        }
        const std::vector<uint8_t> img = s.im.img, resBytes = s.im.res;
        sm::Reserve res{sm::kReserve, actionsEnd - 16};
        const sm::Result r = s.run(map, res);
        CHECK(r.copied == 0 && r.copyRefused == 1 && logHas(r, "the reserve is full (no room for its actions)"));
        CHECK(s.im.img == img && s.im.res == resBytes && res.next == sm::kReserve + aligned(0x20 + 4 * 2));
    }
    {
        CopyStage s;   // step 1 turns tD (its hat in the live image turns); an unturned copy keeps the disc's hat bits
        const std::vector<uint8_t> hatDisc(s.im.at(s.hat1), s.im.at(s.hat1) + 0x18);
        sm::Map map;
        map.edits.push_back(turnBy(s.off(s.tD), 90));
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        CHECK(r.applied == 1 && r.copied == 1);
        CHECK(s.im.rf(s.hat1 + 0xC) == 0.f && s.im.rf(s.hat1 + 0x10) == 10.f);   // step 1's turn
        const uint32_t d1c = destOf(s, rootOf(s, 0)), hatc = s.im.r32(s.im.r32(d1c + 0x10) + 0xC);
        CHECK(s.inReserve(hatc) && s.im.r32(d1c + 0x1C) == hatc && std::memcmp(s.im.at(hatc), hatDisc.data(), 0x18) == 0);
    }
    {
        CopyStage s;   // step 1 turns tD 90; a copy turned 45 turns the disc offset (10, 0) by 45, not the live (0, 10)
        sm::Map map;
        map.edits.push_back(turnBy(s.off(s.tD), 90));
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 45));
        const sm::Result r = s.run(map);
        CopyStage t;
        t.run([&] { sm::Map m2; m2.edits.push_back(turnBy(t.off(t.tD), 45)); return m2; }());
        const uint32_t hatc = s.im.r32(s.im.r32(destOf(s, rootOf(s, 0)) + 0x10) + 0xC);
        CHECK(r.copied == 1 && s.im.r32(hatc + 0xC) == t.im.r32(t.hat1 + 0xC) && s.im.r32(hatc + 0x10) == t.im.r32(t.hat1 + 0x10));
    }
    {
        CopyStage s;   // one subtree holding the in-piece domino, shared by two states of d1 and by d2: copied once
        const uint32_t grp = s.im.actGroup({s.dom});
        s.im.w32(s.d1 + 0x1C, grp);    // d1 state 1
        s.im.w32(s.d2 + 0x10, grp);    // d2 state 0
        CHECK(s.copySet(s.tD).why == "");
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 1);
        const uint32_t root = rootOf(s, 0), d1c = destOf(s, root), i2c = s.im.r32(root + 0x20), d2c = s.im.r32(i2c + 0xC);
        const uint32_t a1c = s.im.r32(d1c + 0x10), grpc = s.im.r32(d1c + 0x1C), domc = s.im.r32(a1c + 0x10);
        CHECK(s.inReserve(grpc) && grpc != grp && s.im.r32(d2c + 0x10) == grpc && s.im.r32(grpc + 8) == 1);
        CHECK(s.inReserve(domc) && s.im.r32(grpc + 0xC) == domc && s.im.r32(domc + 0xC) == i2c);   // one dom' for all
        CHECK(s.im.r32(a1c + 0xC) == s.hat1 && s.im.r32(s.d2 + 0x10) == grp && s.im.r32(grp + 0xC) == s.dom);
    }
    {
        CopyStage s;   // an ACT_TRANSFORM on the way to the in-piece domino (no cycle): private, its matrix copied whole
        const uint32_t xf = s.im.act(sm::kActTransform, 0x50);
        for (uint32_t k = 0xC; k < 0x50; k += 4)
            s.im.wf(xf + k, float(k));
        s.im.w32(xf + 8, s.dom);
        s.im.w32(s.d1 + 0x1C, xf);
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 1);
        const uint32_t root = rootOf(s, 0), d1c = destOf(s, root), i2c = s.im.r32(root + 0x20);
        const uint32_t xfc = s.im.r32(d1c + 0x1C), domc = s.im.r32(s.im.r32(d1c + 0x10) + 0x10);
        CHECK(s.inReserve(xfc) && s.im.r16(xfc) == sm::kActTransform && s.im.r32(xfc + 8) == domc && s.im.r32(domc + 0xC) == i2c);
        CHECK(std::memcmp(s.im.at(xfc + 0xC), s.im.at(xf + 0xC), 0x50 - 0xC) == 0 && s.im.r32(xf + 8) == s.dom);
    }
}

namespace
{
    // The paths listed in grid g other than `known`, in the order the cells first list them
    std::vector<uint32_t> otherPaths(Img &im, uint32_t g, const std::set<uint32_t> &known)
    {
        std::vector<uint32_t> out;
        for (int i = 0; i < im.cols(g) * im.rows(g); ++i)
            for (uint32_t ln = im.r32(g + 0x20 + 4 * uint32_t(i)); ln; ln = im.r32(ln))
            {
                const uint32_t p = im.r32(ln + 4);
                if (!known.count(p) && std::find(out.begin(), out.end(), p) == out.end())
                    out.push_back(p);
            }
        return out;
    }
    std::vector<uint32_t> pathList(Img &im, uint32_t p)
    {
        std::vector<uint32_t> out;
        for (uint32_t i = 0; i < im.r32(p + 0x1C) && i < 64; ++i)
            out.push_back(im.r32(p + 0x24 + 4 * i));
        return out;
    }
    // index of the first log line that starts with `what` (-1: none)
    int logLine(const sm::Result &r, const std::string &what)
    {
        for (size_t i = 0; i < r.log.size(); ++i)
            if (r.log[i].rfind(what, 0) == 0)
                return int(i);
        return -1;
    }
    std::string kb(uint32_t bytes) { return std::to_string((bytes + 512) / 1024); }
    sm::StageCounts stockCounts(CopyStage &s)
    {
        const sm::Mem m = s.im.mem();
        return sm::stageCounts(m, sm::parents(m));
    }
}

TEST(stagemap_debris_pickup_bytes)
{
    // Pickup.cpp:1098-1160 (object, then the 0xC list node), each 16-aligned
    CHECK(sm::debrisPickupBytes(0) == 0 && sm::debrisPickupBytes(0x138D - 0x1388) == 0);
    CHECK(sm::debrisPickupBytes(1) == 0x180);                  // a plain Pickup
    CHECK(sm::debrisPickupBytes(0x1395 - 0x1388) == 0x4D0);   // rigid
    CHECK(sm::debrisPickupBytes(0x139E - 0x1388) == 0x4D0);
    CHECK(sm::debrisPickupBytes(0x13A2 - 0x1388) == 0x280);   // car
    CHECK(sm::debrisPickupBytes(0x1393 - 0x1388) == 0x310);   // helicopter
    CHECK(sm::debrisPickupBytes(0x13A3 - 0x1388) == 0x260);   // subway
    CHECK(sm::debrisPickupBytes(0x13A0 - 0x1388) == 0x1E0);   // turret
    CHECK(sm::debrisPickupBytes(31) == 0x180);
    for (uint32_t t = 1; t < 32; ++t)
        CHECK(sm::debrisPickupBytes(t) <= sm::kPickupBytes && sm::debrisPickupBytes(t) % 16 == 0);
}

// Amendment A4: debris chunks with a pickup type count against the Interactives cap and the pickup memory, in the copy set and in
// the stage's own counts
TEST(stagemap_copy_debris_pickups)
{
    // d1's state 0 actions a1 get a debris action with three chunks: a rigid pickup, none, a plain pickup
    auto withDebris = [](CopyStage &s) {
        const uint32_t deb = s.im.debris({0x1395 - 0x1388, 0, 1});
        s.im.w32(s.a1 + 0x14, deb);   // a1 = {hat1, dom, deb}; the outside domino is dropped
        return deb;
    };
    const uint64_t perCopy = 0x4D0 + 0x180;
    {
        CopyStage s;
        const uint32_t deb = withDebris(s);
        sm::CopySet c = s.copySet(s.tD);
        CHECK(c.why == "" && c.debrisPickups == 2 && c.debrisBytes == perCopy && c.interactives == 2 && c.pickups == 0);
        sm::StageCounts st = stockCounts(s);
        CHECK(st.debrisPickups == 2 && st.debrisBytes == perCopy && st.interactives == 4 && st.pickups == 1);
        // reached from state 1 too (it fires again when d1 enters state 1): counted per state
        s.im.w32(s.d1 + 0x1C, s.im.actGroup({s.hat1, deb}));
        c = s.copySet(s.tD);
        CHECK(c.debrisPickups == 4 && c.debrisBytes == 2 * perCopy);
        // dE (outside tD) gets the same action: the stage counts it, tD's copy set does not
        s.im.w32(s.dE + 0x10, deb);
        CHECK(s.copySet(s.tD).debrisPickups == 4 && stockCounts(s).debrisPickups == 6);
        // more than 32 chunks: Debris::Activate uses the first 32
        CopyStage t;
        std::vector<uint32_t> many(40, 1);
        t.im.w32(t.d2 + 0x10, t.im.debris(many));
        CHECK(t.copySet(t.tD).debrisPickups == 32 && stockCounts(t).debrisPickups == 32);
    }
    {
        // Interactives: 1 + 8 + 4 + the stage's 2 debris pickups = 15; each copy of tD adds 2 + 2
        CopyStage s;
        withDebris(s);
        sm::Limits lim;
        lim.interactives = 15 + 4 + 3;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), 0x8000, map, res, lim);
        CHECK(r.copied == 1 && r.copyRefused == 1 && r.interactives == 15 + 4);
        CHECK(logLine(r, "copy " + hexOff(s.off(s.tD)) + " x1 refused: the Interactives table would pass its cap (22; each copy "
                         "adds 4, 2 of them debris pickups)") >= 0);
        CHECK(r.pickupBytes == sm::kPickupBytes + 2 * perCopy);
    }
    {
        // pickup memory: the stage's level pickup and debris, plus one copy's debris
        CopyStage s;
        withDebris(s);
        sm::Limits lim;
        lim.pickupRoom = uint32_t(sm::kPickupBytes + 2 * perCopy);
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), 0x8000, map, res, lim);
        CHECK(r.copied == 1 && r.copyRefused == 1 && r.pickupBytes == lim.pickupRoom);
        CHECK(logLine(r, "copy " + hexOff(s.off(s.tD)) + " x1 refused: not enough pickup memory (") >= 0);
    }
}

TEST(stagemap_copy_paths)
{
    {
        CopyStage s;   // decision 2: the copy's path is [world, G, tD', i1', d1', g0', pc]; its box is pD's moved by (200, 200, 0)
        s.im.w32(s.pD + 0x18, 0x77), s.im.w32(s.pD + 0x20, 0x1234);   // traceId and flags: copied
        const std::vector<std::pair<int, uint32_t>> pDBefore = s.im.listing(s.g, s.pD);
        const std::map<uint32_t, std::vector<uint32_t>> sets{{s.tD, s.copySet(s.tD).nodes}};
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        sm::Reserve res = s.im.reserve();
        const sm::Result r = s.run(map, res);
        CHECK(r.copied == 1 && r.relinked == 0 && r.fromReserve == 0);
        const std::vector<uint32_t> np = otherPaths(s.im, s.g, {s.pD, s.pE});
        CHECK(np.size() == 1);
        if (np.size() == 1)
        {
            const uint32_t p = np[0], G = s.im.r32(s.world + 0x28), root = s.im.r32(G + 0x24), i1c = s.im.r32(root + 0x1C),
                           d1c = s.im.r32(i1c + 0xC), g0c = s.im.r32(d1c + 0xC);
            CHECK(s.inReserve(p) && p + aligned(0x24 + 4 * 7) <= res.next);
            CHECK(pathList(s.im, p) == std::vector<uint32_t>({s.world, G, root, i1c, d1c, g0c, s.pc}));
            CHECK(s.im.rf(p) == 490.f && s.im.rf(p + 4) == 490.f && s.im.rf(p + 8) == 0.f);
            CHECK(s.im.rf(p + 0xC) == 520.f && s.im.rf(p + 0x10) == 510.f && s.im.rf(p + 0x14) == 40.f);
            CHECK(s.im.r32(p + 0x18) == 0x77 && s.im.r32(p + 0x20) == 0x1234);
            CHECK(s.im.cellsOf(s.g, p) == std::vector<int>({10}) && s.inReserve(s.im.listing(s.g, p)[0].second));
        }
        CHECK(s.im.gridProblems(s.g) == 0 && s.im.listing(s.g, s.pD) == pDBefore);
        CHECK(copiesStandAlone(s.im, r, sets));
    }
    {
        CopyStage s;   // turned 90: the box is the bounds of the turned corners; the path goes through the ROTATE
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0, 90));
        const sm::Result r = s.run(map);
        const std::vector<uint32_t> np = otherPaths(s.im, s.g, {s.pD, s.pE});
        CHECK(r.copied == 1 && np.size() == 1);
        if (np.size() == 1)
        {
            const uint32_t p = np[0], G = s.im.r32(s.world + 0x28), root = s.im.r32(G + 0x24);
            CHECK((s.im.r32(root) & 63) == sm::kOpRotate && pathList(s.im, p).size() == 7 && pathList(s.im, p)[2] == root &&
                  pathList(s.im, p)[3] == s.im.r32(root + 0x50));
            // pD relative to tD: (-10, -10, 0)..(20, 10, 40); turned 90: x in -10..10, y in -10..20
            CHECK(s.im.rf(p) == 490.f && s.im.rf(p + 4) == 490.f && s.im.rf(p + 8) == 0.f);
            CHECK(s.im.rf(p + 0xC) == 510.f && s.im.rf(p + 0x10) == 520.f && s.im.rf(p + 0x14) == 40.f);
        }
        CHECK(s.im.gridProblems(s.g) == 0);
    }
    {
        CopyStage s;   // a source with no paths (the pickup tP) adds none
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tP), 600, 600, 10));
        const sm::Result r = s.run(map);
        CHECK(r.copied == 1 && otherPaths(s.im, s.g, {s.pD, s.pE}).empty() && s.im.gridProblems(s.g) == 0);
    }
    {
        CopyStage s;   // decision 10: after a step 1 remove of tE, the copy takes tE's freed list node first
        const std::vector<std::pair<int, uint32_t>> freed = s.im.listing(s.g, s.pE);
        sm::Map map;
        map.edits.push_back(removal(s.off(s.tE)));
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map);
        const std::vector<uint32_t> np = otherPaths(s.im, s.g, {s.pD});
        CHECK(r.applied == 1 && r.copied == 1 && freed.size() == 1 && np.size() == 1);
        if (np.size() == 1 && freed.size() == 1)
            CHECK(s.im.listing(s.g, np[0]) == decltype(freed)({{10, freed[0].second}}));
        CHECK(s.im.listing(s.g, s.pE).empty() && s.im.gridProblems(s.g) == 0);
    }
    {
        CopyStage s;   // two copies across two cells each: every one listed where its box is
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 600, 500, 0), copyAt(s.off(s.tD), 390, 610, 0, 45)};
        const sm::Result r = s.run(map);
        CHECK(r.copied == 2 && otherPaths(s.im, s.g, {s.pD, s.pE}).size() == 2 && s.im.gridProblems(s.g) == 0);
    }
}

TEST(stagemap_copy_caps)
{
    const uint32_t size = 0x8000;
    {
        CopyStage s;   // Interactives: 1 + 8 + the stage's (13 here) + 2 per copy of tD; three copies under stock + 3
        const sm::StageCounts st = stockCounts(s);
        const int base = 1 + sm::kMonsterSlots + st.interactives;
        CHECK(base == 13);
        sm::Limits lim;
        lim.interactives = base + 3;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0), copyAt(s.off(s.tD), 500, 100, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), size, map, res, lim);
        CHECK(r.copied == 1 && r.copyRefused == 2);
        CHECK(logLine(r, "copy " + hexOff(s.off(s.tD)) + " x2 refused: the Interactives table would pass its cap (") >= 0);
        CHECK(r.interactives == base + 2 && r.destructibles == st.destructibles + 2 && r.pickupBytes == sm::kPickupBytes * uint32_t(st.pickups));
    }
    {
        CopyStage s;   // reaching the cap exactly is fine
        sm::Limits lim;
        lim.interactives = 13 + 4;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), size, map, res, lim);
        CHECK(r.copied == 2 && r.copyRefused == 0 && r.interactives == 17);
    }
    {
        CopyStage s;   // Destructibles: the stage's 3 + 2 per copy
        const sm::StageCounts st = stockCounts(s);
        sm::Limits lim;
        lim.destructibles = st.destructibles + 3;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0), copyAt(s.off(s.tD), 500, 100, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), size, map, res, lim);
        CHECK(r.copied == 1 && r.copyRefused == 2);
        CHECK(logLine(r, "copy " + hexOff(s.off(s.tD)) + " x2 refused: the Destructibles table would pass its cap (") >= 0);
        CHECK(r.destructibles == st.destructibles + 2 && r.interactives == 13 + 2);
    }
    {
        CopyStage s;   // pickup memory: room for the stage's pickup and one more; a copy with no pickup is not held back
        const sm::StageCounts st = stockCounts(s);
        sm::Limits lim;
        lim.pickupRoom = sm::kPickupBytes * uint32_t(st.pickups + 1);
        sm::Map map;
        map.copies = {copyAt(s.off(s.tP), 600, 600, 10), copyAt(s.off(s.tP), 100, 600, 10), copyAt(s.off(s.tD), 500, 500, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), size, map, res, lim);
        CHECK(r.copied == 2 && r.copyRefused == 1);
        CHECK(logLine(r, "copy " + hexOff(s.off(s.tP)) + " x1 refused: ") >= 0 && logHas(r, "pickup memory"));
        CHECK(r.pickupBytes == sm::kPickupBytes * uint32_t(st.pickups + 1) && r.interactives == 13 + 1 + 2);
    }
    {
        CopyStage s;   // a stage already past a cap still takes copies that add nothing to it
        sm::Limits lim;
        lim.pickupRoom = 0;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0)};
        sm::Reserve res = s.im.reserve();
        CHECK(sm::apply(s.im.mem(), size, map, res, lim).copied == 1);
    }
}

TEST(stagemap_copy_atomic)
{
    {
        CopyStage s;   // room for the wrapper and half a copy: refused, nothing written, the reserve back where the wrapper left it
        const uint32_t wrapper = aligned(0x20 + 4 * 2);
        const std::vector<uint8_t> img = s.im.img, resBytes = s.im.res;
        sm::Reserve res{sm::kReserve, sm::kReserve + wrapper + aligned(s.copySet(s.tD).bytes / 2)};
        sm::Map map;
        map.copies.push_back(copyAt(s.off(s.tD), 500, 500, 0));
        const sm::Result r = s.run(map, res);
        CHECK(r.copied == 0 && r.copyRefused == 1 && logHas(r, "the reserve is full"));
        CHECK(res.next == sm::kReserve + wrapper && s.im.img == img && s.im.res == resBytes);
    }
    {
        // after a step 1 remove of tE: a copy over two cells takes tE's freed list node for the first and a reserve node for the
        // second. With no room for that one it is refused, and the freed node stays unused (the image is the remove's alone)
        sm::Map map;
        map.edits.push_back(removal(0));
        map.copies.push_back(copyAt(0, 600, 500, 0));
        uint32_t pathEnd = 0, freedNode = 0;
        {
            CopyStage t;
            map.edits[0].node = t.off(t.tE), map.copies[0].node = t.off(t.tD);
            freedNode = t.im.listing(t.g, t.pE)[0].second;
            sm::Reserve full = t.im.reserve();
            CHECK(t.run(map, full).copied == 1);
            const std::vector<uint32_t> np = otherPaths(t.im, t.g, {t.pD});
            CHECK(np.size() == 1);
            if (np.size() == 1)
            {
                const auto ls = t.im.listing(t.g, np[0]);
                CHECK(ls.size() == 2 && ls[0] == std::make_pair(10, freedNode) && ls[1].first == 11 && t.inReserve(ls[1].second));
                pathEnd = np[0] + aligned(0x24 + 4 * 7);
                CHECK(ls.size() == 2 && ls[1].second == pathEnd);   // the reserve node comes right after the path
            }
        }
        CopyStage removed;   // the remove alone
        {
            sm::Map only;
            only.edits.push_back(removal(removed.off(removed.tE)));
            CHECK(removed.run(only).applied == 1);
        }
        CopyStage s;
        sm::Reserve res{sm::kReserve, pathEnd};
        const sm::Result r = s.run(map, res);
        CHECK(r.applied == 1 && r.copied == 0 && r.copyRefused == 1 && logHas(r, "the reserve is full (no room for its collision grid nodes)"));
        CHECK(s.im.img == removed.im.img && s.im.res == removed.im.res && res.next == sm::kReserve + aligned(0x20 + 4 * 2));
        CHECK(s.im.gridProblems(s.g) == 0);
    }
    {
        // the grid's lists and the pool are put back after a refusal that had already linked a list node: a copy of tD over
        // cells 2 and 3 takes tE's freed node for cell 2, then finds no room for cell 3's. The next copy (tE at its own place,
        // cell 3) gets that freed node, and the bytes are those of a map without the refused copy
        auto mapWith = [](CopyStage &s, bool refusedFirst) {
            sm::Map map;
            map.edits.push_back(removal(s.off(s.tE)));
            if (refusedFirst)
                map.copies.push_back(copyAt(s.off(s.tD), 600, 100, 0));
            map.copies.push_back(copyAt(s.off(s.tE), 700, 100, 0));
            return map;
        };
        uint32_t pathEnd = 0;
        {
            CopyStage t;
            sm::Map map;
            map.edits.push_back(removal(t.off(t.tE)));
            map.copies.push_back(copyAt(t.off(t.tD), 600, 100, 0));
            sm::Reserve full = t.im.reserve();
            CHECK(t.run(map, full).copied == 1);
            const std::vector<uint32_t> np = otherPaths(t.im, t.g, {t.pD});
            CHECK(np.size() == 1 && t.im.cellsOf(t.g, np[0]) == std::vector<int>({2, 3}));
            if (np.size() == 1)
                pathEnd = np[0] + aligned(0x24 + 4 * 7);
        }
        CopyStage s, t;
        const uint32_t freedNode = s.im.listing(s.g, s.pE)[0].second;
        sm::Reserve rs{sm::kReserve, pathEnd}, rt{sm::kReserve, pathEnd};
        const sm::Result a = s.run(mapWith(s, true), rs), b = t.run(mapWith(t, false), rt);
        CHECK(a.copied == 1 && a.copyRefused == 1 && logHas(a, "no room for its collision grid nodes") && b.copied == 1);
        const std::vector<uint32_t> np = otherPaths(s.im, s.g, {s.pD});
        CHECK(np.size() == 1 && s.im.listing(s.g, np[0]) == decltype(s.im.listing(s.g, 0))({{3, freedNode}}));
        CHECK(s.im.gridProblems(s.g) == 0 && s.im.img == t.im.img && s.im.res == t.im.res && rs.next == rt.next);
    }
    {
        // a copy refused half-way (the reserve runs out in its nodes) between two that apply leaves the same bytes as one refused
        // before it took anything (a turned TRANSLATE with action data): the last copy gets the reserve the first left
        sm::Map late, early;
        CopyStage s, t;
        s.im.w32(s.tD + 0xC, s.a1), t.im.w32(t.tD + 0xC, t.a1);
        late.copies = {copyAt(s.off(s.tP), 600, 600, 10), copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tP), 100, 600, 10)};
        early.copies = late.copies;
        early.copies[1].turn = 90;
        const uint32_t pickupCopy = aligned(0x20) + aligned(0x24) + aligned(0x10);   // tP, gP, swP
        const uint32_t end = sm::kReserve + 2 * aligned(0x20 + 4 * 3) + 2 * pickupCopy;   // two wrappers (room for 2 and 1)
        sm::Reserve rs{sm::kReserve, end}, rt{sm::kReserve, end};
        const sm::Result a = s.run(late, rs), b = t.run(early, rt);
        CHECK(a.copied == 2 && a.copyRefused == 1 && logHas(a, "the reserve is full (no room for its nodes)"));
        CHECK(b.copied == 2 && b.copyRefused == 1 && logHas(b, "action data"));
        CHECK(s.im.img == t.im.img && s.im.res == t.im.res && rs.next == rt.next && rs.next == end);
    }
}

TEST(stagemap_copy_logs)
{
    {
        // three copies of tD: the second past the grid's edge (clamped), the third refused by the Interactives cap
        CopyStage s;
        sm::Limits lim;
        lim.interactives = 13 + 4;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 790, 500, 0), copyAt(s.off(s.tD), 100, 500, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), 0x8000, map, res, lim);
        CHECK(r.copied == 2 && r.copyRefused == 1);
        const std::string src = hexOff(s.off(s.tD));
        const uint32_t copyBytes = res.next - (sm::kReserve + aligned(0x20 + 4 * 3));   // after the wrapper (room for 3)
        const int clamp = logLine(r, "copy " + src + " (edit 2): kept inside the collision grid's edge, at 780.0 500.0 0.0");
        const int copied = logLine(r, "copied " + src + " x2 (" + kb(copyBytes) + " KB, 2 grid nodes)");
        const int refused = logLine(r, "copy " + src + " x1 refused: the Interactives table would pass its cap (");
        CHECK(clamp >= 0 && copied > clamp && refused > copied);
        CHECK(int(r.log.size()) == refused + 1);   // the refusals come last
        CHECK(r.copySummary == "copies: 2 applied, 1 refused; reserve " + kb(res.next - sm::kReserve) +
                                   " of 16 KB; interactives 17 of 17, destructibles 7 of 1400, pickup room 1 KB (no limit)");
        for (const std::string &l : r.log)
            std::printf("  %s\n", l.c_str());
        std::printf("  %s\n", r.copySummary.c_str());
    }
    {
        CopyStage s;   // the edit number counts the piece edits written before the copies (write()'s order)
        sm::Map map;
        map.edits.push_back(moveTo(s.off(s.tE), 600, 100, 0));
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 790, 500, 0)};
        const sm::Result r = s.run(map);
        CHECK(r.copied == 2 && logLine(r, "copy " + hexOff(s.off(s.tD)) + " (edit 3): kept inside") >= 0);
        CHECK(r.applied == 1);   // copies are not counted as edits
    }
    {
        CopyStage s;   // pickup room with a limit; refusals grouped per source and reason, in first-appearance order
        sm::Limits lim;
        lim.pickupRoom = sm::kPickupBytes * 2;
        sm::Map map;
        map.copies = {copyAt(s.off(s.tP), 600, 600, 10), copyAt(s.off(s.mid), 0, 0, 0), copyAt(s.off(s.tP), 100, 600, 10),
                      copyAt(s.off(s.tP), 200, 600, 10), copyAt(s.off(s.mid), 0, 0, 0)};
        sm::Reserve res = s.im.reserve();
        const sm::Result r = sm::apply(s.im.mem(), 0x8000, map, res, lim);
        CHECK(r.copied == 1 && r.copyRefused == 4);
        const int notPiece = logLine(r, "copy " + hexOff(s.off(s.mid)) + " x2 refused: not a piece");
        const int room = logLine(r, "copy " + hexOff(s.off(s.tP)) + " x2 refused: ");
        CHECK(logLine(r, "copied " + hexOff(s.off(s.tP)) + " x1 (") >= 0 && notPiece >= 0 && room > notPiece);
        CHECK(has(r.copySummary, "pickup room 3 of 3 KB"));   // 2 x 0x500 bytes
    }
    {
        // a source under a CONTROL, a LOD, a SWITCH and a GROUP with collision-option flags: one line each, however many copies;
        // the copies' paths drop them (decision 2)
        CopyStage s;
        const uint32_t sw2 = s.im.switchNode({s.mid}), l = s.im.lod(sw2, 300, 300, 0, 100), ctl = s.im.control(l, 0);
        s.im.w32(s.world + 0x20, ctl);
        s.im.w16(s.mid + 0xA, 0x0101);
        const uint32_t pD2 = s.im.path(290, 290, 0, 320, 310, 40, {s.world, ctl, l, sw2, s.mid, s.tD, s.i1, s.d1, s.g0, s.pc});
        for (const auto &c : s.im.listing(s.g, s.pD))
            s.im.w32(c.second + 4, pD2);
        CHECK(s.copySet(s.tD).why == "" && s.copySet(s.tD).optionGroup == s.mid);
        sm::Map map;
        map.copies = {copyAt(s.off(s.tD), 500, 500, 0), copyAt(s.off(s.tD), 100, 500, 0), copyAt(s.off(s.tD), 500, 100, 0, 90)};
        const sm::Result r = s.run(map);
        CHECK(r.copied == 3);
        const std::string src = "copy " + hexOff(s.off(s.tD)) + ": ";
        int lines[4] = {0, 0, 0, 0};
        for (const std::string &line : r.log)
        {
            lines[0] += line == src + "CONTROL " + hexOff(s.off(ctl)) + " above it does not hang over its copies";
            lines[1] += line == src + "LOD " + hexOff(s.off(l)) + " above it does not hang over its copies";
            lines[2] += line == src + "SWITCH " + hexOff(s.off(sw2)) + " above it does not hang over its copies";
            lines[3] += line.rfind(src + "GROUP " + hexOff(s.off(s.mid)) + " above it does not hang over its copies", 0) == 0;
        }
        CHECK(lines[0] == 1 && lines[1] == 1 && lines[2] == 1 && lines[3] == 1);
        const std::vector<uint32_t> np = otherPaths(s.im, s.g, {pD2, s.pE});
        CHECK(np.size() == 3 && s.im.gridProblems(s.g) == 0);
        for (uint32_t p : np)
        {
            const std::vector<uint32_t> nodes = pathList(s.im, p);
            CHECK(nodes.size() == 7 && nodes[0] == s.world && nodes[1] == s.im.r32(s.world + 0x28) && nodes.back() == s.pc);
        }
    }
}

namespace
{
    // ';'-separated list from the environment (empty when unset)
    std::vector<std::string> envList(const char *name)
    {
        std::vector<std::string> out;
        const char *v = std::getenv(name);
        std::string cur;
        for (const char *c = v ? v : ""; ; ++c)
        {
            if (*c == ';' || !*c)
            {
                if (!cur.empty())
                    out.push_back(cur);
                cur.clear();
                if (!*c)
                    break;
            }
            else
                cur += *c;
        }
        return out;
    }
    bool readFile(const std::string &path, std::vector<uint8_t> &out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return false;
        out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }
    bool writeFile(const std::string &path, const std::vector<uint8_t> &data)
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
        return bool(f);
    }

    // The grid rule on a real stage (thousands of paths, so sets instead of Img::gridProblems): every listed path in exactly
    // the cells sm::covered() gives for its box, once per cell, no list node in two cells, no broken list. `listed` gets each
    // path's cells.
    struct GridScan { int problems = 0, nodes = 0; std::map<uint32_t, std::vector<int>> listed; };
    GridScan scanGrid(const sm::Mem &m, uint32_t g)
    {
        GridScan s;
        const sm::GridDims d{m.s16(g + 4), m.s16(g + 6), m.f32(g + 8), m.f32(g + 0xC), m.f32(g + 0x10), m.f32(g + 0x14), m.f32(g + 0x18)};
        std::set<uint32_t> seen;
        auto problem = [&](const std::string &what) {
            if (s.problems++ < 10)
                std::printf("  grid: %s\n", what.c_str());
        };
        for (int i = 0; i < d.nCols * d.nRows; ++i)
        {
            std::set<uint32_t> here;
            for (uint32_t ln = m.u32(g + 0x20 + 4 * uint32_t(i)); ln; ln = m.u32(ln))
            {
                if (!m.ok(ln, 8) || !seen.insert(ln).second)
                {
                    problem("cell " + std::to_string(i) + ": list node " + hexOff(ln) + " unmapped or in two cells");
                    break;
                }
                ++s.nodes;
                const uint32_t p = m.u32(ln + 4);
                if (!here.insert(p).second)
                    problem("path " + hexOff(p) + " twice in cell " + std::to_string(i));
                s.listed[p].push_back(i);
            }
        }
        for (auto &kv : s.listed)
        {
            const uint32_t p = kv.first;
            kv.second.erase(std::unique(kv.second.begin(), kv.second.end()), kv.second.end());
            const float lo[3] = {m.f32(p), m.f32(p + 4), m.f32(p + 8)}, hi[3] = {m.f32(p + 0xC), m.f32(p + 0x10), m.f32(p + 0x14)};
            if (!m.ok(p, 0x24) || sm::covered(d, lo, hi) != kv.second)
                problem("path " + hexOff(p) + ": listed cells differ from the cells its box covers");
        }
        return s;
    }
}

// Cross-check against a stock stage (spec "PR 1 checks", 1f and 1i). Needs the disc, so it runs only with these set:
//   WOTM_TEST_STAGE        raw inflated images, ';'-separated, e.g. from
//                          python tools/stage_map.py inflate disc/LVL/CENTRAL.NGP %TEMP%\CENTRAL.bin
//   WOTM_TEST_PIECES       optional, one file per image (same order): python tools/stage_map.py pieces disc/LVL/CENTRAL.NGP > %TEMP%\CENTRAL.pieces
//   WOTM_TEST_STAGE_DUMP   optional folder: the edited image and reserve go to <folder>/<stage>.edited.bin(.reserve), for
//                          python tools/stage_map.py check <folder>/<stage>.edited.bin   (must print OK)
// Per stage: the C++ coverage rule gives exactly the stock cells, the C++ pieces equal stage_map.py's, and a realistic edit
// set (a destructible removed, a piece moved across cells, a TRANSLATE turned 90, a ROTATE turned 15) all applies and leaves
// the grid rule holding.
TEST(stagemap_stock_grid_rule)
{
    const std::vector<std::string> stages = envList("WOTM_TEST_STAGE"), pieceFiles = envList("WOTM_TEST_PIECES");
    const char *dumpDir = std::getenv("WOTM_TEST_STAGE_DUMP");
    if (stages.empty())
    {
        std::printf("  skip (WOTM_TEST_STAGE not set)\n");
        return;
    }
    for (size_t si = 0; si < stages.size(); ++si)
    {
        const std::string stem = std::filesystem::path(stages[si]).stem().string();
        std::vector<uint8_t> file;
        CHECK(readFile(stages[si], file) && file.size() > 8);
        if (file.size() <= 8)
            continue;
        Img im(uint32_t(file.size()), sm::kReserveSize);
        im.img = file;
        const sm::Mem m = im.mem();
        const std::map<uint32_t, uint32_t> par = sm::parents(m);
        uint32_t g = 0;
        int grids = 0;
        for (const auto &kv : par)
            if ((m.u32(kv.first) & 63) == sm::kOpGrid)
                g = kv.first, ++grids;
        CHECK(grids == 1);
        if (grids != 1)
            continue;

        // 1. the stock grid follows the C++ rule
        const GridScan stock = scanGrid(m, g);
        CHECK(stock.problems == 0 && !stock.listed.empty());

        // the C++ pieces, the same set as stage_map.py's
        std::set<uint32_t> pieces;
        for (const auto &kv : par)
            if (sm::isPiece(m, par, kv.first))
                pieces.insert(kv.first - sm::kBase);
        size_t pyCount = 0;
        if (si < pieceFiles.size())
        {
            std::ifstream f(pieceFiles[si]);
            CHECK(bool(f));
            std::set<uint32_t> py;
            std::string line;
            while (std::getline(f, line))
                if (line.rfind("0x", 0) == 0)
                    py.insert(uint32_t(std::strtoul(line.c_str(), nullptr, 16))), ++pyCount;
            CHECK(py.size() == pyCount && py == pieces);
            for (uint32_t n : pieces)
                if (!py.count(n))
                    std::printf("  %s: C++ piece %s not in stage_map.py's\n", stem.c_str(), hexOff(n).c_str());
            for (uint32_t n : py)
                if (!pieces.count(n))
                    std::printf("  %s: stage_map.py piece %s not in C++'s\n", stem.c_str(), hexOff(n).c_str());
        }

        // 2. a realistic edit set, picked from the pieces
        std::map<uint32_t, std::vector<uint32_t>> through;   // node -> paths through it
        for (const auto &kv : stock.listed)
        {
            std::set<uint32_t> nodes;
            const uint32_t k = std::min<uint32_t>(m.u32(kv.first + 0x1C), 256);
            for (uint32_t i = 0; i < k; ++i)
                nodes.insert(m.u32(kv.first + 0x24 + 4 * i));
            for (uint32_t n : nodes)
                through[n].push_back(kv.first);
        }
        std::map<uint32_t, int> holders;   // pointers to each node from child slots
        for (const auto &kv : par)
            for (uint32_t sl : sm::slots(m, kv.first))
                if (const uint32_t k = m.u32(sl))
                    ++holders[k];
        auto above = [&](uint32_t n, uint32_t op) {   // a node of this opcode on the first-parent chain
            for (uint32_t a = par.at(n); a; a = par.at(a))
                if ((m.u32(a) & 63) == op)
                    return true;
            return false;
        };
        auto destructible = [&](uint32_t n) {
            for (uint32_t k : sm::subtree(m, n))
                if ((m.u32(k) & 63) == sm::kOpDestructible)
                    return true;
            return false;
        };
        std::set<uint32_t> used;
        // first piece (address order) that passes `want`, preferring one with a CONTROL or LOD above it
        auto pick = [&](const std::function<bool(uint32_t)> &want) {
            uint32_t best = 0;
            for (uint32_t off : pieces)
            {
                const uint32_t n = off + sm::kBase;
                if (used.count(n) || holders[n] != 1 || !through.count(n) || !want(n))
                    continue;
                if (above(n, sm::kOpControl) || above(n, sm::kOpLod))
                {
                    best = n;
                    break;
                }
                if (!best)
                    best = n;
            }
            if (best)
                used.insert(best);
            return best;
        };
        auto opOf = [&](uint32_t n) { return m.u32(n) & 63; };
        const uint32_t rm = pick([&](uint32_t n) { return destructible(n); });
        const uint32_t mv = pick([&](uint32_t n) { return opOf(n) == sm::kOpTranslate; });
        const uint32_t t90 = pick([&](uint32_t n) { return opOf(n) == sm::kOpTranslate && m.u32(n + 0xC) == 0; });
        uint32_t t15 = pick([&](uint32_t n) { return opOf(n) == sm::kOpRotate; });
        const char *t15kind = "ROTATE";
        if (!t15)   // CENTRAL has no ROTATE piece with collision paths: turn a second TRANSLATE by 15 instead
            t15 = pick([&](uint32_t n) { return opOf(n) == sm::kOpTranslate && m.u32(n + 0xC) == 0; }), t15kind = "TRANSLATE";
        if (!(rm && mv && t90 && t15))
        {   // BIGSHOT, DBALL and TRAINING have 0 to 2 pieces with collision paths: the grid rule and the piece set above still ran
            std::printf("  %s: edit set skipped: not enough pieces with collision paths (remove %s, move %s, turn 90 %s, turn 15 %s)\n",
                        stem.c_str(), rm ? "found" : "none", mv ? "found" : "none", t90 ? "found" : "none", t15 ? "found" : "none");
            continue;
        }
        // the move: three cells in X and two in Y, toward the middle of the grid
        const float cell = m.f32(g + 8), minX = m.f32(g + 0xC), minY = m.f32(g + 0x10), maxX = m.f32(g + 0x14), maxY = m.f32(g + 0x18);
        const float px = m.f32(mv + 0x10), py = m.f32(mv + 0x14), pz = m.f32(mv + 0x18);
        const float nx = px + (px < (minX + maxX) / 2 ? 3 : -3) * cell, ny = py + (py < (minY + maxY) / 2 ? 2 : -2) * cell;
        sm::Map map;
        map.edits = {removal(rm - sm::kBase), moveTo(mv - sm::kBase, nx, ny, pz), turnBy(t90 - sm::kBase, 90), turnBy(t15 - sm::kBase, 15)};
        const uint32_t t90parent = par.at(t90);
        std::vector<std::pair<uint32_t, std::pair<float, float>>> hats;   // the 90-degree piece's hat offsets, before
        for (uint32_t k : sm::subtree(m, t90))
            for (uint32_t h : sm::hatActions(m, k))
                if (std::none_of(hats.begin(), hats.end(), [&](const auto &x) { return x.first == h; }))
                    hats.push_back({h, {m.f32(h + 0xC), m.f32(h + 0x10)}});
        sm::Reserve res = im.reserve();
        const sm::Result r = sm::apply(m, uint32_t(im.img.size()), map, res);
        CHECK(r.applied == 4 && r.refused == 0);
        for (const std::string &l : r.log)
            std::printf("  %s: %s\n", stem.c_str(), l.c_str());

        const GridScan after = scanGrid(m, g);
        CHECK(after.problems == 0);
        for (uint32_t p : through[rm])
            CHECK(!after.listed.count(p));   // the removed piece's paths left the grid
        for (uint32_t p : through[mv])
            CHECK(after.listed.count(p) && after.listed.at(p) != stock.listed.at(p));   // the moved piece's paths changed cells
        CHECK(after.listed.size() + through[rm].size() == stock.listed.size());
        CHECK((m.u32(rm) & 63) == sm::kOpNull && m.f32(mv + 0x10) == nx && m.f32(mv + 0x14) == ny);
        bool slotTurned = false;   // the 90-degree TRANSLATE is now a ROTATE in the reserve, in its parent's slot
        for (uint32_t sl : sm::slots(m, t90parent))
        {
            const uint32_t k = m.u32(sl);
            slotTurned = slotTurned || (k >= sm::kReserve && (m.u32(k) & 63) == sm::kOpRotate && m.u32(k + 0x50) == m.u32(t90 + 0x1C));
        }
        CHECK(slotTurned);
        bool hatsTurned = true;   // (x, y) -> (-y, x)
        for (const auto &h : hats)
            hatsTurned = hatsTurned && m.f32(h.first + 0xC) == -h.second.second && m.f32(h.first + 0x10) == h.second.first;
        CHECK(hatsTurned);
        if (stem == "CENTRAL")
            CHECK(!hats.empty());   // CENTRAL's turned piece (0x165a70) has 2: the check above is not vacuous there
        std::printf("  %s: %zu hat offsets of %s turned\n", stem.c_str(), hats.size(), hexOff(t90 - sm::kBase).c_str());
        std::printf("  %s: %zu pieces (stage_map.py %zu); removed %s, moved %s, turned %s by 90 and %s %s by 15; applied %d, "
                    "relinked %d (%d from reserve); grid %zu paths, %d list nodes, %d problems\n",
                    stem.c_str(), pieces.size(), pyCount, hexOff(rm - sm::kBase).c_str(), hexOff(mv - sm::kBase).c_str(),
                    hexOff(t90 - sm::kBase).c_str(), t15kind, hexOff(t15 - sm::kBase).c_str(), r.applied, r.relinked, r.fromReserve,
                    after.listed.size(), after.nodes, after.problems);
        if (dumpDir && *dumpDir)
        {
            const std::string out = (std::filesystem::path(dumpDir) / (stem + ".edited.bin")).string();
            CHECK(writeFile(out, im.img) && writeFile(out + ".reserve", im.res));
            std::printf("  %s: wrote %s(.reserve)\n", stem.c_str(), out.c_str());
        }
    }
}

// Review focus 1, 2 and 4 on every stock stage (same WOTM_TEST_STAGE images; WOTM_TEST_STAGE_DUMP writes <stem>.copies.bin and
// .reserve for python tools/stage_map.py check): a piece removed (its list nodes go to the pool), then up to three copyable
// pieces with collision paths (a destructible, a TRANSLATE, a ROTATE: address order) each copied twice, three cells over and
// turned 90 two cells the other way. Every copy applies, the grid rule holds, the listed paths are the stock ones minus the
// removed piece's plus one per copied path, every per-instance node keeps one holder, the Interactives estimate is the
// stage's plus the copies' (debris pickups included, amendment A4), the copies take the pool's list nodes before the
// reserve's, and each copy path is [header root, wrapper, copy root] then a chain of children (to a disc NULL at most). Prints
// each stage's own counts, debris pickups included.
TEST(stagemap_stock_copies_grid_rule)
{
    const std::vector<std::string> stages = envList("WOTM_TEST_STAGE");
    const char *dumpDir = std::getenv("WOTM_TEST_STAGE_DUMP");
    if (stages.empty())
    {
        std::printf("  skip (WOTM_TEST_STAGE not set)\n");
        return;
    }
    for (const std::string &file : stages)
    {
        const std::string stem = std::filesystem::path(file).stem().string();
        std::vector<uint8_t> bytes;
        CHECK(readFile(file, bytes) && bytes.size() > 8);
        if (bytes.size() <= 8)
            continue;
        Img im(uint32_t(bytes.size()), sm::kReserveSize);
        im.img = bytes;
        const sm::Mem m = im.mem();
        const std::map<uint32_t, uint32_t> par = sm::parents(m);
        uint32_t g = 0;
        for (const auto &kv : par)
            if ((m.u32(kv.first) & 63) == sm::kOpGrid)
                g = kv.first;
        {
            const sm::StageCounts sc = sm::stageCounts(m, par);   // amendment A4: the stage's own debris pickups
            std::printf("  %s: stock interactives %d, destructibles %d, level pickups %d, debris pickups %d (%llu KB)\n", stem.c_str(),
                        sc.interactives, sc.destructibles, sc.pickups, sc.debrisPickups,
                        (unsigned long long)((sc.debrisBytes + 512) / 1024));
        }
        if (!g)
            continue;
        const GridScan stock = scanGrid(m, g);
        std::map<uint32_t, std::set<uint32_t>> through;
        for (const auto &kv : stock.listed)
        {
            const uint32_t k = std::min<uint32_t>(m.u32(kv.first + 0x1C), 256);
            for (uint32_t i = 0; i < k; ++i)
                through[m.u32(kv.first + 0x24 + 4 * i)].insert(kv.first);
        }
        std::map<uint32_t, int> holders;
        for (const auto &kv : par)
            for (uint32_t sl : sm::slots(m, kv.first))
                if (const uint32_t k = m.u32(sl))
                    ++holders[k];
        auto destructible = [&](uint32_t n) {
            for (uint32_t k : sm::subtree(m, n))
                if ((m.u32(k) & 63) == sm::kOpDestructible)
                    return true;
            return false;
        };
        // sources: copyable pieces with paths, one held by its parent only; the removed piece: any other piece with paths
        std::map<uint32_t, sm::CopySet> sets;
        std::vector<uint32_t> sources;
        uint32_t removed = 0;
        const std::function<bool(uint32_t)> wants[3] = {
            [&](uint32_t n) { return destructible(n); },
            [&](uint32_t n) { return (m.u32(n) & 63) == sm::kOpTranslate; },
            [&](uint32_t n) { return (m.u32(n) & 63) == sm::kOpRotate; }};
        for (const auto &want : wants)
            for (const auto &kv : par)
            {
                const uint32_t n = kv.first;
                if (sets.count(n) || holders[n] != 1 || !through.count(n) || !sm::isPiece(m, par, n) || !want(n))
                    continue;
                sm::CopySet cs = sm::copySet(m, par, n);
                if (!cs.why.empty())
                    continue;
                // a TRANSLATE with action data cannot be turned
                if ((m.u32(n) & 63) == sm::kOpTranslate && m.u32(n + 0xC) != 0)
                    continue;
                sets[n] = std::move(cs);
                sources.push_back(n);
                break;
            }
        for (const auto &kv : par)
            if (!sets.count(kv.first) && holders[kv.first] == 1 && through.count(kv.first) && sm::isPiece(m, par, kv.first))
            {
                bool feeds = false;   // a source's paths must not be the removed piece's
                for (uint32_t s : sources)
                    for (uint32_t p : through[s])
                        feeds = feeds || through[kv.first].count(p);
                if (!feeds)
                {
                    removed = kv.first;
                    break;
                }
            }
        if (sources.empty() || !removed)
        {
            std::printf("  %s: copies skipped (%zu copyable pieces with paths)\n", stem.c_str(), sources.size());
            continue;
        }
        const float cell = m.f32(g + 8), minX = m.f32(g + 0xC), minY = m.f32(g + 0x10), maxX = m.f32(g + 0x14), maxY = m.f32(g + 0x18);
        sm::Map map;
        map.edits.push_back(removal(removed - sm::kBase));
        std::map<uint32_t, std::vector<uint32_t>> setNodes;
        int addInteractives = 0;
        size_t addPaths = 0;
        for (uint32_t n : sources)
        {
            const uint32_t at = (m.u32(n) & 63) == sm::kOpRotate ? 0x40 : 0x10;
            const float px = m.f32(n + at), py = m.f32(n + at + 4), pz = m.f32(n + at + 8);
            const float dx = (px < (minX + maxX) / 2 ? 3 : -3) * cell, dy = (py < (minY + maxY) / 2 ? 2 : -2) * cell;
            map.copies.push_back(copyAt(n - sm::kBase, px + dx, py + dy, pz));
            map.copies.push_back(copyAt(n - sm::kBase, px - dx / 3 * 2, py - dy, pz, 90));
            setNodes[n] = sets[n].nodes;
            addInteractives += 2 * (sets[n].interactives + sets[n].debrisPickups);   // amendment A4: debris pickups too
            addPaths += 2 * through[n].size();
        }
        sm::StageCounts after;   // the stage's counts after the remove alone
        {
            Img only(uint32_t(bytes.size()), 0x10);
            only.img = bytes;
            sm::Map rm;
            rm.edits = map.edits;
            sm::Reserve r0 = only.reserve();
            sm::apply(only.mem(), uint32_t(bytes.size()), rm, r0);
            const sm::Mem om = only.mem();
            after = sm::stageCounts(om, sm::parents(om));
        }
        sm::Reserve res = im.reserve();
        const sm::Result r = sm::apply(m, uint32_t(bytes.size()), map, res);
        for (const std::string &l : r.log)
            std::printf("  %s: %s\n", stem.c_str(), l.c_str());
        std::printf("  %s: %s\n", stem.c_str(), r.copySummary.c_str());
        CHECK(r.applied == 1 && r.copied == int(map.copies.size()) && r.copyRefused == 0);
        const GridScan scan = scanGrid(m, g);
        CHECK(scan.problems == 0);
        CHECK(scan.listed.size() == stock.listed.size() - through[removed].size() + addPaths);
        CHECK(copiesStandAlone(im, r, setNodes));
        // each copy's path (the paths in the reserve): a header root, its wrapper (in the root's last slot, ids 0), a copy's root,
        // then a chain of children down to the hull
        {
            const std::vector<uint32_t> rs = sm::roots(m);
            std::set<uint32_t> copyRoots;
            for (const sm::CopyInfo &c : r.copies)
                copyRoots.insert(c.root);
            size_t copyPaths = 0;
            int bad = 0, throughNull = 0;
            for (const auto &kv : scan.listed)
            {
                const uint32_t p = kv.first;
                if (p < sm::kReserve)
                    continue;
                ++copyPaths;
                const uint32_t len = m.u32(p + 0x1C);
                std::vector<uint32_t> nodes;
                for (uint32_t i = 0; i < len && i < 256; ++i)
                    nodes.push_back(m.u32(p + 0x24 + 4 * i));
                bool ok = nodes.size() >= 4 && std::find(rs.begin(), rs.end(), nodes[0]) != rs.end();
                if (ok)
                {
                    const std::vector<uint32_t> sl = sm::slots(m, nodes[0]);
                    ok = !sl.empty() && m.u32(sl.back()) == nodes[1] && nodes[1] >= sm::kReserve &&
                         (m.u32(nodes[1]) & 63) == sm::kOpGroup && (m.u32(nodes[1]) >> 7) == 0 && copyRoots.count(nodes[2]);
                }
                // a NULL on the way (as on the disc: AIRPORT, ISLAND and VEGAS have such paths) has no children to check: the
                // game stops there (hdTraverse.cpp:179), for the source and its copies alike
                for (size_t j = 1; ok && j + 1 < nodes.size(); ++j)
                {
                    if ((m.u32(nodes[j]) & 63) == sm::kOpNull)
                    {
                        ok = nodes[j] < sm::kReserve && (bytes[nodes[j] - sm::kBase] & 63) == sm::kOpNull;   // NULL on the disc
                        ++throughNull;
                        break;
                    }
                    const std::vector<uint32_t> ks = sm::children(m, nodes[j]);
                    ok = std::find(ks.begin(), ks.end(), nodes[j + 1]) != ks.end();
                }
                if (!ok && bad++ < 5)
                {
                    std::printf("  %s: copy path %s is not [root, wrapper, copy root, children...]:", stem.c_str(), hexOff(p).c_str());
                    for (uint32_t n : nodes)
                        std::printf(" %s/%u", hexOff(n).c_str(), m.u32(n) & 63);
                    std::printf("\n");
                }
            }
            CHECK(bad == 0 && copyPaths == addPaths);
            std::printf("  %s: %zu copy paths [root, wrapper, copy root, children...] (%d through a disc NULL)\n", stem.c_str(),
                        copyPaths, throughNull);
        }
        CHECK(r.interactives == 1 + sm::kMonsterSlots + after.interactives + after.debrisPickups + addInteractives);
        // the pool first: the copies' list nodes include every node the remove freed, or all of theirs are freed ones
        const sm::Mem disc{{{sm::kBase, bytes.data(), uint32_t(bytes.size())}}};
        const int cells = m.s16(g + 4) * m.s16(g + 6);
        std::set<uint32_t> nowListed;
        for (int i = 0; i < cells; ++i)
            for (uint32_t ln = m.u32(g + 0x20 + 4 * uint32_t(i)); ln; ln = m.u32(ln))
                nowListed.insert(ln);
        int freed = 0, reused = 0;
        for (int i = 0; i < cells; ++i)
            for (uint32_t ln = disc.u32(g + 0x20 + 4 * uint32_t(i)); ln; ln = disc.u32(ln))
                if (through[removed].count(disc.u32(ln + 4)))
                    ++freed, reused += int(nowListed.count(ln));
        const int copyNodes = scan.nodes - (stock.nodes - freed);
        CHECK(reused == std::min(freed, copyNodes));
        std::printf("  %s: removed %s, copied %zu sources x2; %d of %d freed list nodes reused; grid %zu paths, %d list nodes\n",
                    stem.c_str(), hexOff(removed - sm::kBase).c_str(), sources.size(), reused, freed, scan.listed.size(), scan.nodes);
        if (dumpDir && *dumpDir)
        {
            const std::string out = (std::filesystem::path(dumpDir) / (stem + ".copies.bin")).string();
            CHECK(writeFile(out, im.img) && writeFile(out + ".reserve", im.res));
        }
    }
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
