// wotm_photo.inc: photo mode (included by wotm_live.inc after wotm_legend.inc; global scope of ps2_runtime.cpp).
//
// F9 on the keyboard or L3 + R3 together on a gamepad: the game pauses (the same pause as F7, wotmPhotoPause in wotm_foto.inc) and
// the camera is freed, with zoom. The controls show in the bottom legend (wotm_legend.inc, icons in the style of the last device
// used), on a translucent strip. With the EE thread stopped the game recomputes nothing that depends on the camera, so drawing
// carries these along every frame:
//  - the hier HUD (Scene::hud, hp, overlay, hpOverlay): 3D objects Hud::update placed in front of the game camera. Each matrix goes
//    from the game camera's eye space to the free camera's (with the zoom compensated on the screen axes), so the HUD stays put;
//  - lens flare: the list is rebuilt every frame from the free camera, the way the game builds it (hierLightNode and
//    particleAddLensFlareToList) and projects it (particleLensFlareTestZbuffer): hier lights with lens flare that the scene walk
//    found (Scene::worldLights, including ones that were off screen at pause time, like Robo-47's eye seen from the front) and
//    particle systems with light; drawFlares' occlusion test runs against the new scene. With neither source, the paused frame's
//    list is mapped back to the world and reprojected;
//  - Agamo's head fire (preset 141): the emitter only spawns particles when the game's traversal passes it. With Agamo off screen
//    at pause time the fire had already died out; here it is simulated (wotm_flame.hpp, warmed up and frozen like a photo) on every
//    traversed emitter with no live particles nearby (Particles::synth);
//  - bars over enemies and the game's health icon: the Hud::drawHealthIcon hook records the world point (Snapshot::anchors); here
//    the point is projected through both cameras and the difference moves the bars (ScreenRect::anchor) and the icon (ScreenPolys
//    with the same center). In split screen the bars are dropped (the free camera shows only view 0, full screen).
// The game's text (timer, names) and the damage numbers (projected by the native layer) already follow the screen/camera. Hiding
// the HUD removes all of that except the fades. Saving writes the native frame without the legend to
// <settings.ini folder>/screenshots/WotM_<date>_<time>.png.
// On exit, the game camera comes back at once and the game resumes when the button used to exit is released (at most 1 s; the
// button never reaches the monster). Sound keeps playing, as with F7.
#include <array>
#include <ctime>
#include <unordered_map>
#include "wotm_flame.hpp"

void wotmPhotoPause(bool on);   // wotm_foto.inc: the F7 pause

namespace wotm_photo
{
namespace st = wotm::settings;
using wotm_legend::Icon;

enum class Phase { Off, Entering, On, Leaving };
static Phase g_phase = Phase::Off;
static double g_enterT = 0.0;
static double g_leaveT = 0.0;
static bool g_exitHeld[st::kPadButtons] = {};   // pad buttons held at exit: the game waits for them to be released
static std::unordered_map<uint32_t, wotm::flame::Flame> g_flames;   // simulated fire per emitter node (rebuilt every session)
static bool g_logged = false;    // summary of lights and emitters, once per session
static bool g_autoPivotOk = false;   // automatic test: the half turn orbits the nearest light (the player's monster's eye)
static float g_autoPivot[3] = {0, 0, 0}, g_autoBeam[3] = {0, 1, 0};
static bool g_autoFireOk = false;   // automatic test: stage 3 (no HUD) looks at Agamo's fire up close
static float g_autoFire[3] = {0, 0, 0};
static WotmFreeCamPose g_pose, g_start;
static float g_zoom = 1.f;
static float g_speed = 220.f;   // world units per second
static bool g_hide = false;
static bool g_saveReq = false;
static bool g_legendShot = false;   // automatic test: saves the frame with the legend in the current folder
static std::string g_msg;
static double g_msgUntil = 0.0;
static uint64_t g_fontTex0 = 0;   // game font atlas from the last frame with text (the legend writes with it)

static bool padDown(int b)
{
    for (int p = 0; p < 4; ++p)
        if (IsGamepadAvailable(p) && IsGamepadButtonDown(p, b)) return true;
    return false;
}
static bool padPressed(int b)
{
    for (int p = 0; p < 4; ++p)
        if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, b)) return true;
    return false;
}
// axis with a dead zone, the largest magnitude across gamepads
static float padAxis(int a)
{
    float best = 0.f;
    for (int p = 0; p < 4; ++p)
        if (IsGamepadAvailable(p))
        {
            const float v = GetGamepadAxisMovement(p, a);
            if (std::fabs(v) > std::fabs(best)) best = v;
        }
    constexpr float kDead = 0.18f;
    if (std::fabs(best) < kDead) return 0.f;
    const float s = (std::fabs(best) - kDead) / (1.f - kDead);
    return std::copysign(s * s, best);   // curve: slow near the center
}
static bool anyPadButton()
{
    for (int b = 1; b < st::kPadButtons; ++b)
        if (padDown(b)) return true;
    return false;
}
// Exit: the view returns to the game's right away; the game resumes once the buttons held now are released (update)
static void startLeaving()
{
    for (int b = 1; b < st::kPadButtons; ++b) g_exitHeld[b] = padDown(b);
    g_phase = Phase::Leaving;
    g_leaveT = GetTime();
}
// L3 + R3: both held, one of them pressed just now
static bool chord()
{
    return padDown(GAMEPAD_BUTTON_LEFT_THUMB) && padDown(GAMEPAD_BUTTON_RIGHT_THUMB) &&
           (padPressed(GAMEPAD_BUTTON_LEFT_THUMB) || padPressed(GAMEPAD_BUTTON_RIGHT_THUMB));
}

static void say(const char *s, double secs = 2.0)
{
    g_msg = s;
    g_msgUntil = GetTime() + secs;
}

static void postLegend()
{
    using wotm_legend::kStickL;
    using wotm_legend::kStickR;
    const char *hud = g_hide ? "SHOW HUD" : "HIDE HUD";
    if (wotm_prompts::currentStyle() == st::PromptKeyboard)
        wotm_legend::post({{Icon{0, "WASD"}, {}, "MOVE"},
                           {Icon{0, "Q"}, Icon{0, "E"}, "HEIGHT"},
                           {Icon{0, "MOUSE_RIGHT"}, {}, "LOOK"},
                           {Icon{0, "LSHIFT"}, {}, "FAST"},
                           {Icon{0, "MOUSE_SCROLL"}, {}, "ZOOM"},
                           {Icon{0, "H"}, {}, hud},
                           {Icon{0, "ENTER"}, {}, "SAVE PHOTO"},
                           {Icon{0, "R"}, {}, "RESET"},
                           {Icon{0, "F9"}, {}, "EXIT"}},
                          1.f, 0.8f);
    else
        wotm_legend::post({{Icon{kStickL, nullptr}, {}, "MOVE"},
                           {Icon{kStickR, nullptr}, {}, "LOOK"},
                           {Icon{GAMEPAD_BUTTON_LEFT_TRIGGER_2, nullptr}, Icon{GAMEPAD_BUTTON_RIGHT_TRIGGER_2, nullptr}, "HEIGHT"},
                           {Icon{GAMEPAD_BUTTON_LEFT_TRIGGER_1, nullptr}, Icon{GAMEPAD_BUTTON_RIGHT_TRIGGER_1, nullptr}, "ZOOM"},
                           {Icon{GAMEPAD_BUTTON_RIGHT_FACE_DOWN, nullptr}, {}, hud},
                           {Icon{GAMEPAD_BUTTON_RIGHT_FACE_LEFT, nullptr}, {}, "SAVE PHOTO"},
                           {Icon{GAMEPAD_BUTTON_RIGHT_FACE_UP, nullptr}, {}, "RESET"},
                           {Icon{GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, nullptr}, {}, "EXIT"}},
                          1.f, 0.8f);
}

static void move(float dt)
{
    WotmFreeCamPose &p = g_pose;
    // look: mouse with the right button, arrow keys, right stick (slower when zoomed)
    const float lookK = std::min(1.f, g_zoom);
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        const Vector2 d = GetMouseDelta();
        p.heading += d.x * 0.003f * lookK;
        p.pitch -= d.y * 0.003f * lookK;
    }
    const float turn = 1.6f * dt * lookK;
    p.heading += turn * (float(IsKeyDown(KEY_RIGHT)) - float(IsKeyDown(KEY_LEFT)) + padAxis(GAMEPAD_AXIS_RIGHT_X) * 1.4f);
    p.pitch += turn * (float(IsKeyDown(KEY_UP)) - float(IsKeyDown(KEY_DOWN)) - padAxis(GAMEPAD_AXIS_RIGHT_Y) * 1.4f);
    p.pitch = std::clamp(p.pitch, -1.55f, 1.55f);
    p.heading = std::remainder(p.heading, 6.2831853f);
    // move: WASD + Q/E, left stick + L2/R2; Shift x4, Ctrl x0.25
    float k = g_speed * dt;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) k *= 4.f;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) k *= 0.25f;
    const float mf = float(IsKeyDown(KEY_W)) - float(IsKeyDown(KEY_S)) - padAxis(GAMEPAD_AXIS_LEFT_Y);
    const float mr = float(IsKeyDown(KEY_D)) - float(IsKeyDown(KEY_A)) + padAxis(GAMEPAD_AXIS_LEFT_X);
    const float mu = float(IsKeyDown(KEY_E)) - float(IsKeyDown(KEY_Q)) + float(padDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) -
                     float(padDown(GAMEPAD_BUTTON_LEFT_TRIGGER_2));
    float r[3], f[3], u[3];
    wotmFreeCamAxes(p, r, f, u);
    // forward on the ground plane: moving doesn't climb or sink with the camera tilted (height is Q/E, L2/R2)
    float fh[3] = {f[0], f[1], 0.f};
    const float n = std::sqrt(fh[0] * fh[0] + fh[1] * fh[1]);
    if (n > 1e-4f) { fh[0] /= n; fh[1] /= n; }
    else { fh[0] = -u[0]; fh[1] = -u[1]; }   // looking straight down/up
    for (int i = 0; i < 2; ++i) p.pos[i] += k * (mf * fh[i] + mr * r[i]);
    p.pos[2] += k * mu;
    // zoom: mouse wheel, L1/R1
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.f) g_zoom *= std::pow(0.88f, wheel);
    g_zoom *= std::exp(1.2f * dt * (float(padDown(GAMEPAD_BUTTON_LEFT_TRIGGER_1)) - float(padDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_1))));
    g_zoom = std::clamp(g_zoom, 0.15f, 1.6f);
}

// Presentation, every frame, before drawing. gc = the game camera from the snapshot. Returns the view for drawing (on = false:
// the F5 free camera decides).
static double g_lastUpdate = 0.0;
static wotm_freecam::View update(const wotm::GameCamera &gc)
{
    const double now = GetTime();
    g_lastUpdate = now;
    bool toggle = IsKeyPressed(KEY_F9) || chord();
    if (toggle && g_phase == Phase::Off && wotm_pad::inMenus())   // fights only: it doesn't turn on in the game's menus (shell screens)
    {
        toggle = false;
        std::fprintf(stderr, "[photo] ignored in menus\n");
    }
    // network match: photo mode's pause would stop only this PC (the others would wait for it)
    static const bool s_net = [] { const char *e = std::getenv("WOTM_NET"); return e && *e; }();
    const bool autoDue = std::getenv("WOTM_PHOTO") != nullptr;
    if (s_net && (toggle || autoDue))
    {
        static bool said = false;
        if (!said && (toggle || gc.ok))
        {
            said = true;
            std::fprintf(stderr, "[photo] off in network matches\n");
        }
        return {};
    }
    // Keyboardless test (WOTM_PHOTO=<s>): s seconds after the first scene it turns on, turns and strafes, saves (HUD, no HUD, zoomed)
    // and exits; the photos go to the screenshots folder
    static const double autoAt = std::getenv("WOTM_PHOTO") ? std::atof(std::getenv("WOTM_PHOTO")) : -1.0;
    static double firstT = -1.0;
    static int autoStage = 0;
    if (autoAt >= 0.0 && gc.ok && firstT < 0.0) firstT = now;
    const double autoT = autoAt >= 0.0 && firstT >= 0.0 ? now - firstT - autoAt : -1.0;
    if (autoT >= 0.0 && autoStage == 0)
    {
        toggle = g_phase == Phase::Off;
        autoStage = 1;
        std::fprintf(stderr, "[photo] automatic test: on\n");
    }
    if (autoStage >= 1 && autoStage < 7 && g_phase == Phase::On)
    {
        const float dt = std::min(GetFrameTime(), 0.1f);
        if (autoStage == 2)
        {   // the camera moves in front of the nearest light's beam (the player's monster's eye), 70 away, looking at it: the flare
            // must light up even though it was off screen at pause time. No light: a half turn around a point 170 ahead
            (void)dt;
            const float a = std::clamp(float(autoT - 0.4) / 2.f, 0.f, 1.f);
            const float h = g_start.heading;
            float target[3] = {g_start.pos[0] + 170.f * std::sin(h + 3.14159265f * a), g_start.pos[1] + 170.f * std::cos(h + 3.14159265f * a), g_start.pos[2]};
            float look[3] = {g_start.pos[0] + 170.f * std::sin(h), g_start.pos[1] + 170.f * std::cos(h), g_start.pos[2]};
            if (g_autoPivotOk)
                for (int k = 0; k < 3; ++k) { target[k] = g_autoPivot[k] + 70.f * g_autoBeam[k]; look[k] = g_autoPivot[k]; }
            for (int k = 0; k < 3; ++k) g_pose.pos[k] = g_start.pos[k] + (target[k] - g_start.pos[k]) * a;
            const float d[3] = {look[0] - g_pose.pos[0], look[1] - g_pose.pos[1], look[2] - g_pose.pos[2]};
            if (std::fabs(d[0]) + std::fabs(d[1]) > 1e-3f)
            {
                g_pose.heading = std::atan2(d[0], d[1]);
                g_pose.pitch = std::atan2(d[2], std::hypot(d[0], d[1]));
            }
        }
        if (autoStage == 3 && g_autoFireOk)
        {   // beside the fire emitter, a bit above, looking at it
            g_pose.pos[0] = g_autoFire[0] + 110.f;
            g_pose.pos[1] = g_autoFire[1] - 40.f;
            g_pose.pos[2] = g_autoFire[2] + 30.f;
            const float d[3] = {g_autoFire[0] - g_pose.pos[0], g_autoFire[1] - g_pose.pos[1], g_autoFire[2] + 25.f - g_pose.pos[2]};
            g_pose.heading = std::atan2(d[0], d[1]);
            g_pose.pitch = std::atan2(d[2], std::hypot(d[0], d[1]));
        }
        static const double stageAt[6] = {0.4, 2.5, 3.5, 4.5, 5.5, 6.5};
        if (autoT >= stageAt[autoStage - 1])
        {
            switch (autoStage)
            {
            case 1: g_saveReq = true; break;                                   // game camera (reference)
            case 2: g_saveReq = true; break;                                   // after moving: HUD following the camera
            case 3: g_hide = true; g_saveReq = true; break;                    // no HUD
            case 4: g_hide = false; g_zoom = 0.5f; g_saveReq = true; break;    // zoom
            case 5: g_legendShot = true; break;                                // frame with the legend (photo_test_legend.png)
            case 6: startLeaving(); break;
            }
            std::fprintf(stderr, "[photo] automatic test: stage %d\n", autoStage);
            ++autoStage;
        }
    }
    switch (g_phase)
    {
    case Phase::Off:
        if (toggle && gc.ok)
        {
            g_pose.pos[0] = gc.pos[0];
            g_pose.pos[1] = gc.pos[1];
            g_pose.pos[2] = gc.pos[2];
            g_pose.heading = std::atan2(gc.fwd[0], gc.fwd[1]);   // like F5 (mathfRotationFromVector)
            g_pose.pitch = std::atan2(gc.fwd[2], std::sqrt(gc.fwd[0] * gc.fwd[0] + gc.fwd[1] * gc.fwd[1]));
            g_start = g_pose;
            g_zoom = 1.f;
            g_hide = false;
            g_saveReq = false;
            g_flames.clear();
            g_logged = false;
            g_autoPivotOk = false;
            g_autoFireOk = false;
            g_wotmFreeCam = false;   // the debug free camera turns off: photo mode doesn't write to the game's _cs
            wotmPhotoPause(true);
            g_phase = Phase::Entering;
            g_enterT = now;
            std::fprintf(stderr, "[photo] on\n");
        }
        break;
    case Phase::Entering:
        if (wotmPaused())
            g_phase = Phase::On;
        else if (toggle || now - g_enterT > 1.5)   // screen with no game frame (movie, loading): the pause never arrives
        {
            wotmPhotoPause(false);
            g_phase = Phase::Off;
            std::fprintf(stderr, "[photo] canceled: the pause never arrived (screen with no game frame)\n");
        }
        break;
    case Phase::On:
        if (toggle || IsKeyPressed(KEY_ESCAPE) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            startLeaving();
        break;
    case Phase::Leaving:
        break;
    }
    if (g_phase == Phase::Leaving)
    {
        bool held = IsKeyDown(KEY_F9) || IsKeyDown(KEY_ESCAPE);
        for (int b = 1; b < st::kPadButtons && !held; ++b)
            if (g_exitHeld[b] && padDown(b)) held = true;
            else g_exitHeld[b] = false;   // released once, it no longer counts
        // a button raylib always sees as held used to leave the game paused forever: after 1 s it resumes regardless
        if (held && now - g_leaveT < 1.0) return wotm_freecam::View{};
    }
    if (g_phase == Phase::Leaving)
    {
        wotmPhotoPause(false);
        g_phase = Phase::Off;
        std::fprintf(stderr, "[photo] off\n");
    }
    wotm_freecam::View v;
    if (g_phase == Phase::Off || g_phase == Phase::Entering)
        return v;
    if (g_phase == Phase::On)
    {
        move(std::min(GetFrameTime(), 0.1f));
        if (IsKeyPressed(KEY_H) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) g_hide = !g_hide;
        if (IsKeyPressed(KEY_R) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_UP))
        {
            g_pose = g_start;
            g_zoom = 1.f;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) g_saveReq = true;
        // the photo frame comes out without the legend or the notice (afterRender saves it)
        if (!g_saveReq)
        {
            postLegend();
            if (now < g_msgUntil) wotm_legend::hint(g_msg.c_str(), std::min(1.f, float(g_msgUntil - now) * 3.f));
        }
    }
    v.on = true;
    v.override = wotmPaused();
    v.pose = g_pose;
    v.zoom = g_zoom;
    v.photo = true;
    v.hideHud = g_hide;
    return v;
}

// ------------------------------------------------------------------ game camera -> free camera
struct Eye { float o[3], r[3], f[3], u[3]; };
static Eye eyeOf(const wotm::GameCamera &c)
{
    Eye e;
    for (int i = 0; i < 3; ++i) { e.o[i] = c.pos[i]; e.f[i] = c.fwd[i]; e.u[i] = c.up[i]; }
    // right = forward x up (the same as wotmFreeCamAxes and row 0 of worldToScreenMat)
    e.r[0] = e.f[1] * e.u[2] - e.f[2] * e.u[1];
    e.r[1] = e.f[2] * e.u[0] - e.f[0] * e.u[2];
    e.r[2] = e.f[0] * e.u[1] - e.f[1] * e.u[0];
    const float n = std::sqrt(e.r[0] * e.r[0] + e.r[1] * e.r[1] + e.r[2] * e.r[2]);
    if (n > 1e-6f) for (float &x : e.r) x /= n;
    return e;
}
static float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
// world -> eye (right, forward, up); point = include the translation
static void toEye(const Eye &e, const float p[3], bool point, float q[3])
{
    const float d[3] = {p[0] - (point ? e.o[0] : 0.f), p[1] - (point ? e.o[1] : 0.f), p[2] - (point ? e.o[2] : 0.f)};
    q[0] = dot3(d, e.r);
    q[1] = dot3(d, e.f);
    q[2] = dot3(d, e.u);
}
static void fromEye(const Eye &e, const float q[3], bool point, float p[3])
{
    for (int i = 0; i < 3; ++i) p[i] = (point ? e.o[i] : 0.f) + q[0] * e.r[i] + q[1] * e.f[i] + q[2] * e.u[i];
}

// Game projection: horizontal (GS px) and vertical (lines) focal lengths from view 0's worldToScreenMat (row 0 = fh*right +
// c*forward, row 1 = -fv*up + c*forward, row 3 = forward). With the RAM still (paused) the matrix is the paused frame's.
struct Focus { float fh = 384.f, fv = 180.48f; };
static Focus gameFocus(const wotm::Ram &ram, const wotm::GameCamera &gc)
{
    Focus f;
    const uint32_t wm = wotm::addr::worldToScreenMat;
    float r0[3], r1[3], r3[3];
    for (int i = 0; i < 3; ++i) { r0[i] = ram.f32(wm + 4 * i); r1[i] = ram.f32(wm + 0x10 + 4 * i); r3[i] = ram.f32(wm + 0x30 + 4 * i); }
    const float n3 = dot3(r3, r3);
    auto perp = [&](const float r[3]) {
        const float k = dot3(r, r3) / n3;
        float s = 0.f;
        for (int i = 0; i < 3; ++i) s += (r[i] - k * r3[i]) * (r[i] - k * r3[i]);
        return std::sqrt(s);
    };
    if (n3 > 0.5f && std::isfinite(n3))
    {
        const float fh = perp(r0), fv = perp(r1);
        if (fh > 1.f && fv > 1.f && std::isfinite(fh) && std::isfinite(fv)) { f.fh = fh; f.fv = fv; return f; }
    }
    f.fv = 112.f / std::tan(gc.fovy * 3.14159265f / 360.f);
    f.fh = f.fv * (384.f / 180.48f);
    return f;
}
// World point -> offset from the game screen's center (GS px in x, lines in y); false when behind the camera
static bool project(const Eye &e, const Focus &fc, const float w[3], float &x, float &y, float *depth = nullptr)
{
    float q[3];
    toEye(e, w, true, q);
    if (q[1] < 1.f) return false;
    x = fc.fh * q[0] / q[1];
    y = -fc.fv * q[2] / q[1];
    if (depth) *depth = q[1];
    return true;
}

// Matrix (rows = axes, row 3 = position) attached to the game camera -> attached to the free one; the screen axes shrink with the
// zoom (the HUD keeps the same size on screen)
static void rebase(wotm::M4 &m, const Eye &g, const Eye &f, float zoom)
{
    for (int row = 0; row < 4; ++row)
    {
        const bool point = row == 3;
        float q[3];
        toEye(g, m.m[row], point, q);
        q[0] *= zoom;
        q[2] *= zoom;
        fromEye(f, q, point, m.m[row]);
    }
}

// Lens flare from the game camera's screen to the free camera's (FlareFrame in GS px, center cx, cy)
static void reprojectFlares(wotm::Particles::FlareFrame &fr, const Eye &g, const Eye &f, const Focus &fg, const Focus &ff)
{
    std::vector<wotm::Particles::Flare> out;
    const float x0 = -0.5f * fr.vw - 3.f, x1 = 0.5f * fr.vw + 3.f, y0 = -0.5f * fr.vh - 3.f, y1 = 0.5f * fr.vh + 3.f;
    for (wotm::Particles::Flare fl : fr.e)
    {
        const bool inf = (fl.flags & 0x80) != 0;
        const float dx = (fl.x - fr.cx) / fg.fh, dy = (fl.y - fr.cy) / fg.fv;
        const float qg[3] = {dx, 1.f, -dy};
        float w[3], q[3];
        if (inf)
        {
            fromEye(g, qg, false, w);   // direction only
            toEye(f, w, false, q);
            if (q[1] <= 1e-4f) continue;
        }
        else
        {
            const float qp[3] = {qg[0] * fl.w, fl.w, qg[2] * fl.w};
            fromEye(g, qp, true, w);
            toEye(f, w, true, q);
            if (q[1] < 1.f) continue;
            fl.w = q[1];
            if (fl.w > 0.f) fl.z = fr.zScale + fr.zOff / fl.w;
        }
        const float sx = ff.fh * q[0] / q[1], sy = -ff.fv * q[2] / q[1];
        if (sx < x0 || sx > x1 || sy < y0 || sy > y1) continue;
        fl.x = fr.cx + sx;
        fl.y = fr.cy + sy;
        out.push_back(fl);
    }
    fr.e.swap(out);
}

// Lens flare list rebuilt from the free camera: what hierLightNode (hier lights) and particleAddLensFlareToList (systems with
// vis.lightColor & 0x1000000) put on the list, projected like particleLensFlareTestZbuffer (drops W < 1 and off screen; distance
// = W, or 100 for a light at infinity; GS Z = a + b / W). No ID (drawFlares doesn't average with the previous frame: the camera
// changes every frame). False = no source at all (the caller reprojects the paused frame's list).
static bool rebuildFlares(wotm::Particles::FlareFrame &fr, const wotm::Ram &ram, const std::vector<wotm::WorldLight> &lights,
                          const std::vector<wotm::FxSystem> &systems, const Eye &f, const Focus &ff)
{
    using wotm::Particles;
    std::vector<uint32_t> lit;   // particle systems with light
    for (const wotm::FxSystem &sy : systems)
        if (ram.u32(sy.base + 0x2EAC) & 0x1000000u) lit.push_back(sy.base);   // vis.lightColor
    if (lights.empty() && lit.empty()) return false;
    if (fr.nspr == 0)   // the pause came from a frame with no flare: the sprite table (lensFlarePacketInit) and frame come from the still RAM
    {
        Particles::FlareFrame t;
        Particles::readFlares(ram, t);
        t.e.clear();
        fr = std::move(t);
        if (fr.nspr == 0) return false;
    }
    fr.e.clear();
    const float x0 = -0.5f * fr.vw, x1 = 0.5f * fr.vw, y0 = -0.5f * fr.vh, y1 = 0.5f * fr.vh;
    auto add = [&](const float p[3], Particles::Flare fl) {
        float q[3];
        toEye(f, p, true, q);
        if (q[1] < 1.f || fr.e.size() >= 74) return;
        const float sx = ff.fh * q[0] / q[1], sy = -ff.fv * q[2] / q[1];
        if (sx < x0 || sx > x1 || sy < y0 || sy > y1) return;
        const bool inf = (fl.flags & 0x80) != 0;
        // particleLensFlareRender adds the centroid of the visible samples; drawFlares undoes it assuming all are visible
        const float shift = 1.5f - (inf ? 2.f : 1.f);
        fl.x = fr.cx + sx + shift;
        fl.y = fr.cy + sy + shift;
        fl.w = inf ? 100.f : q[1];
        fl.z = fr.zScale + fr.zOff / q[1];
        fl.id = 0;
        fr.e.push_back(fl);
    };
    for (const wotm::WorldLight &l : lights)
    {
        Particles::Flare fl;
        fl.flags = l.flags;   // the flags byte sits where Zfudge goes: a denormal, so it counts as the minimum (1.5)
        fl.intensity = l.intensity;
        for (int k = 0; k < 3; ++k) fl.rgb[k] = l.rgb[k];
        fl.dot = 1.f;
        fl.bw = 0.f;
        if ((l.flags & 0x80) == 0 && (l.flags & 8))
        {   // dotProd = -(eye -> light) . beam
            const float d[3] = {l.p[0] - f.o[0], l.p[1] - f.o[1], l.p[2] - f.o[2]};
            const float n = std::sqrt(dot3(d, d));
            fl.dot = n > 1e-6f ? -dot3(d, l.beam) / n : 1.f;
            fl.bw = l.bw;
        }
        else if (l.flags & 0x80)
            fl.bw = 1.f;   // at infinity the game doesn't write dot/bw (the previous entry's remain); here, full beam
        add(l.p, fl);
    }
    for (uint32_t b : lit)
    {
        Particles::Flare fl;
        const uint32_t c = ram.u32(b + 0x2EAC);
        const uint32_t src = (c & 0x8000000u) ? 0x2CA0u : 0x2C80u;   // emit.ParticleToEmitVec or FOVsphere
        const float p[3] = {ram.f32(b + src), ram.f32(b + src + 4), ram.f32(b + src + 8)};
        // Zfudge (info + 0x1C) goes in word +0x10 and its low byte becomes the flags (3)
        const uint32_t word = (ram.u32(b + 0x2E4C) & ~0xFFu) | 3u;
        fl.flags = word;
        std::memcpy(&fl.zfudge, &word, 4);
        for (int k = 0; k < 3; ++k) fl.rgb[k] = float((c >> (8 * k)) & 0xFF) * 0.0039215684f;
        const float li = ram.f32(b + 0x2E5C);   // info.lightIntensity
        fl.intensity = li * li;
        add(p, fl);
    }
    return true;
}

// Agamo's head fire (preset 141, IdolHeadFlame: Fireball texture, texInfo 8; additive blend 0x48; 75f5be1:docs/investigacoes/2026-10-21/fogo_agamo.md) on
// traversed emitters with no system with live particles nearby: simulated for 30 fields with the emitter still (starts warmed up,
// as in a game already running) and frozen for the session
static void synthFire(wotm::Scene &scene, const wotm::Ram &ram)
{
    wotm::Particles::SynthSystem sys;
    sys.texInfo = 8;
    sys.alphaContext = 0x48;
    for (const wotm::WorldEmitter &em : scene.emitters)
    {
        if (em.preset != 141) continue;
        bool live = false;
        for (const wotm::FxSystem &sy : scene.fx.systems)
        {
            if (sy.n <= 0) continue;
            const float d[3] = {ram.f32(sy.base + 0x2CA0) - em.e[0], ram.f32(sy.base + 0x2CA4) - em.e[1], ram.f32(sy.base + 0x2CA8) - em.e[2]};
            if (dot3(d, d) < 20.f * 20.f) { live = true; break; }
        }
        if (!g_logged)
        {
            std::fprintf(stderr, "[photo] Agamo fire at emitter %06x (%.0f %.0f %.0f): %s\n", em.node, em.e[0], em.e[1], em.e[2],
                         live ? "game particles" : "simulated");
            for (int k = 0; k < 3; ++k) g_autoFire[k] = em.e[k];
            g_autoFireOk = true;
        }
        if (live) continue;
        auto it = g_flames.find(em.node);
        if (it == g_flames.end())
        {
            wotm::flame::Flame fl;
            uint32_t rng = 0x2545F491u ^ em.node;
            for (int k = 0; k < 30; ++k) wotm::flame::step(fl, em.e, em.r0, em.r2, true, rng);
            it = g_flames.emplace(em.node, std::move(fl)).first;
            std::fprintf(stderr, "[photo] simulated fire at emitter %06x (%.0f %.0f %.0f): %zu particles\n", em.node, em.e[0], em.e[1], em.e[2],
                         it->second.parts.size());
        }
        for (const wotm::flame::Part &q : it->second.parts)
            sys.sprites.push_back({{q.p[0], q.p[1], q.p[2]}, q.size, wotm::flame::color(q.age), q.sel});
    }
    if (!sys.sprites.empty()) scene.fx.synth.push_back(std::move(sys));
}

// Paused in photo mode, after show: the HUD and everything else the game attached to its camera move to the free one (or vanish
// with the HUD hidden). split = the paused frame was split screen.
static void applyToScene(wotm::Scene &scene, const wotm::Ram &ram, const wotm::GameCamera &gameCam, const wotm::GameCamera &freeCam,
                         const wotm_freecam::View &v, const std::vector<wotm::HudAnchor> &anchors, bool split)
{
    const Eye g = eyeOf(gameCam), f = eyeOf(freeCam);
    const Focus fg = gameFocus(ram, gameCam);
    const Focus ff{fg.fh / v.zoom, fg.fv / v.zoom};
    {
        wotm::ViewScope vs(scene.vctx);
        if (!rebuildFlares(scene.fx.flares, ram, scene.worldLights, scene.fx.systems, f, ff))
            reprojectFlares(scene.fx.flares, g, f, fg, ff);
    }
    scene.fx.resetFlareDebounce();   // the visible fraction is the new camera's, not averaged with the old one's
    synthFire(scene, ram);
    if (!g_logged)
    {
        g_logged = true;
        int n141 = 0;
        for (const wotm::WorldEmitter &em : scene.emitters) n141 += em.preset == 141;
        std::fprintf(stderr, "[photo] scene walk: %zu lights with lens flare, %zu emitters (%d of fire 141), %zu particle systems\n",
                     scene.worldLights.size(), scene.emitters.size(), n141, scene.fx.systems.size());
        float best = 1e30f;
        for (const wotm::WorldLight &l : scene.worldLights)
        {
            const float d[3] = {l.p[0] - g.o[0], l.p[1] - g.o[1], l.p[2] - g.o[2]};
            std::fprintf(stderr, "[photo]   light (%.0f %.0f %.0f) flags %02x I %.0f at %.0f from the eye (node %06x, cs %06x)\n", l.p[0], l.p[1], l.p[2], l.flags,
                         l.intensity, std::sqrt(dot3(d, d)), l.node, l.cs);
            const float bl = std::sqrt(dot3(l.beam, l.beam));
            if (dot3(d, d) < best && bl > 1e-6f)
            {
                best = dot3(d, d);
                for (int k = 0; k < 3; ++k) { g_autoPivot[k] = l.p[k]; g_autoBeam[k] = l.beam[k] / bl; }
                g_autoPivotOk = true;
            }
        }
    }
    auto isBar = [](const wotm::ScreenRect &r) { return r.barView >= 0; };
    if (v.hideHud)
    {
        scene.hud.clear();
        scene.hp.clear();
        scene.hpOverlay.clear();
        scene.overlay.clear();
        scene.screenPolys.clear();
        scene.screenRects.erase(std::remove_if(scene.screenRects.begin(), scene.screenRects.end(), isBar), scene.screenRects.end());
        return;
    }
    for (auto *list : {&scene.hud, &scene.hp, &scene.hpOverlay, &scene.overlay})
        for (wotm::DrawItem &it : *list) rebase(it.m, g, f, v.zoom);
    // offset (game screen, 640 x 224) of each health icon in view 0; NAN = behind the free camera
    std::vector<std::array<float, 2>> shift(anchors.size(), {NAN, NAN});
    for (size_t i = 0; i < anchors.size(); ++i)
    {
        const wotm::HudAnchor &a = anchors[i];
        float xg, yg, xf, yf;
        if (a.view == 0 && !split && project(g, fg, a.w, xg, yg) && project(f, ff, a.w, xf, yf))
            shift[i] = {xf - xg, yf - yg};
    }
    auto &rects = scene.screenRects;
    rects.erase(std::remove_if(rects.begin(), rects.end(),
                               [&](wotm::ScreenRect &r) {
                                   if (!isBar(r)) return false;
                                   if (r.anchor < 0 || size_t(r.anchor) >= shift.size() || std::isnan(shift[r.anchor][0])) return true;
                                   const auto &s = shift[r.anchor];
                                   r.x0 += s[0]; r.x1 += s[0]; r.y0 += s[1]; r.y1 += s[1];
                                   return false;
                               }),
                rects.end());
    // the game's health icon (not the bars): the two quads centered on the recorded point
    auto &polys = scene.screenPolys;
    polys.erase(std::remove_if(polys.begin(), polys.end(),
                               [&](wotm::TextQuad &t) {
                                   const float cx = 0.5f * (t.x0 + t.x1), cy = 0.5f * (t.y0 + t.y1);
                                   for (size_t i = 0; i < anchors.size(); ++i)
                                       if (std::fabs(anchors[i].x - cx) < 1.5f && std::fabs(anchors[i].y - cy) < 1.5f)
                                       {
                                           if (std::isnan(shift[i][0])) return true;
                                           t.x0 += shift[i][0]; t.x1 += shift[i][0]; t.y0 += shift[i][1]; t.y1 += shift[i][1];
                                           return false;
                                       }
                                   return false;
                               }),
                polys.end());
}

// Presentation, after the frame's EndTextureMode: saves the requested photo (the frame was just drawn without the legend)
static void afterRender()
{
    if (g_legendShot)
    {
        g_legendShot = false;
        std::fprintf(stderr, "[photo] automatic test: frame with the legend %s\n",
                     wotm_live::exportFrame("photo_test_legend.png") ? "in photo_test_legend.png" : "not saved");
    }
    if (!g_saveReq) return;
    g_saveReq = false;
    namespace fs = std::filesystem;
    const fs::path base = st::g_path.empty() ? fs::current_path() : st::g_path.parent_path();
    const fs::path dir = base / "screenshots";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::time_t t = std::time(nullptr);
    char stamp[48];
    std::strftime(stamp, sizeof stamp, "WotM_%Y-%m-%d_%H%M%S", std::localtime(&t));
    fs::path file = dir / (std::string(stamp) + ".png");
    for (int n = 2; fs::exists(file, ec) && n < 100; ++n) file = dir / (std::string(stamp) + "_" + std::to_string(n) + ".png");
    if (wotm_live::exportFrame(file.string().c_str()))
    {
        std::fprintf(stderr, "[photo] photo saved to %s\n", file.string().c_str());
        say("PHOTO SAVED");
    }
    else
    {
        std::fprintf(stderr, "[photo] could not save %s\n", file.string().c_str());
        say("COULD NOT SAVE THE PHOTO", 3.0);
    }
}

// Game font atlas for the legend: the one from the frame's text, or the last one seen (HUD hidden, frame without text)
static uint64_t fontTex0(const std::vector<wotm::TextQuad> &text)
{
    if (!text.empty() && text.front().tex0) g_fontTex0 = text.front().tex0;
    return g_fontTex0;
}
} // namespace wotm_photo

bool wotmPhotoActive() { return wotm_photo::g_phase != wotm_photo::Phase::Off; }
// Debug free camera (F5, wotm_freecam.inc): the same icon legend instead of the text line
static void wotmFreeCamLegend(const wotm_freecam::View &v)
{
    using wotm_legend::Icon;
    if (!v.on || v.photo) return;
    const bool pad = g_wotmFreeCamPad.load(std::memory_order_relaxed);
    wotm_legend::post({{Icon{0, "WASD"}, {}, "MOVE"},
                       {Icon{0, "Q"}, Icon{0, "E"}, "HEIGHT"},
                       {Icon{0, "MOUSE_RIGHT"}, {}, "LOOK"},
                       {Icon{0, "LSHIFT"}, Icon{0, "LCTRL"}, "FAST SLOW"},
                       {Icon{0, "MOUSE_SCROLL"}, {}, "SPEED"},
                       {Icon{0, "TAB"}, {}, pad ? "PAD TO CAMERA" : "PAD TO GAME"},
                       {Icon{0, "F5"}, {}, "EXIT"}},
                      1.f, 0.8f);
}
// Presentation, every frame (wotmFotoKeys): if native drawing stopped calling update (a screen with no native scene), photo mode
// exits and the game resumes, instead of staying paused with no legend
void wotmPhotoWatchdog()
{
    using namespace wotm_photo;
    if (g_phase != Phase::Off && GetTime() - g_lastUpdate > 3.0)
    {
        wotmPhotoPause(false);
        g_phase = Phase::Off;
        std::fprintf(stderr, "[photo] off: the screen no longer has a native scene\n");
    }
}
