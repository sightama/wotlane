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
    put(root / "B" / "stages" / "m1" / "stage.json", R"({"name": "LOSER", "base": "CENTRAL", "baseHash": "0123456789abcdef", "edits": []})");
    put(root / "A" / "MON" / "LIZARD1.TEX", "tex");   // a real override, so the file list is not empty
    wotm::mods::Registry reg;
    reg.scan(root);
    const std::vector<wotm::mods::Stage> st = reg.stages();
    CHECK(st.size() == 1);
    if (st.size() == 1)
    {
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
    CHECK(logged("map m1: A beats B", ""));
    CHECK(logged("[stages] map m1 (\"DOWNTOWN MESS\", A): base CENTRAL, 2 edits", ""));
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
        uint32_t polycoll() { return node(42, 0x10); }
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
    sm::Map back;
    std::string why;
    CHECK(sm::parse(sm::write(m), back, why));
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
    CHECK(h == 0x035d31e0eff6d986ull);
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
//                          python -c "import sys;sys.path.insert(0,'tools');import ngp;open(sys.argv[2],'wb').write(ngp.inflate_ngp(sys.argv[1]))" disc/LVL/CENTRAL.NGP %TEMP%\CENTRAL.bin
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
        CHECK(rm && mv && t90 && t15);
        if (!(rm && mv && t90 && t15))
            continue;
        // the move: three cells in X and two in Y, toward the middle of the grid
        const float cell = m.f32(g + 8), minX = m.f32(g + 0xC), minY = m.f32(g + 0x10), maxX = m.f32(g + 0x14), maxY = m.f32(g + 0x18);
        const float px = m.f32(mv + 0x10), py = m.f32(mv + 0x14), pz = m.f32(mv + 0x18);
        const float nx = px + (px < (minX + maxX) / 2 ? 3 : -3) * cell, ny = py + (py < (minY + maxY) / 2 ? 2 : -2) * cell;
        sm::Map map;
        map.edits = {removal(rm - sm::kBase), moveTo(mv - sm::kBase, nx, ny, pz), turnBy(t90 - sm::kBase, 90), turnBy(t15 - sm::kBase, 15)};
        const uint32_t t90parent = par.at(t90);
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
