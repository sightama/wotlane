// wotm_freecam.inc: debug free camera, window side (included by wotm_live.inc; global scope of ps2_runtime.cpp).
// Shared state and the pose convention live in native/wotm_freecam.h; the game side (pose in view 0's _cs, idle pad) lives
// in wotm_overrides.cpp.
//
// Keys (presentation thread, on every draw, in real time):
//   F5 on/off (starts where the game camera is; when turned off, the game camera returns to the monster by itself)
//   W/S forward/back (where it looks), A/D strafe, Q/E down/up; Shift x5, Ctrl x0.2; mouse wheel changes the base speed
//   right mouse button drag or arrow keys: look
//   Tab: hands the pad back to the game (the monster walks, relative to the free camera) and stops keyboard camera control; the mouse keeps looking
// With the camera on and Tab off, the game gets an idle pad (the pad keys are also WASD/Q/Shift/arrows).
// Paused (F7) the EE thread doesn't run and the game doesn't rebuild the view: native drawing uses the pose directly, without the hier HUD (which the
// game places in front of its own camera) and with the sky carried along with the eye. The rest of the snapshot stays as the game built it: LOD,
// culled particles and "ghost" buildings follow the camera of the paused frame.
#include "wotm_freecam.h"

bool wotmPaused();   // wotm_foto.inc: the EE thread is held by the pause (F7)
bool wotmPhotoActive();   // wotm_photo.inc: photo mode on (F5 has no effect)

namespace wotm_freecam
{
float speed = 150.f;   // world units per second (mouse wheel)

struct View {
    bool on = false;
    bool override = false;   // drawing uses the pose (game paused); outside the pause the game already draws with it
    WotmFreeCamPose pose;
    float zoom = 1.f;        // photo mode: tan(fov/2) of the free camera over the game's (< 1 zooms in)
    bool photo = false;      // photo mode view (wotm_photo.inc): the HUD follows the camera instead of disappearing
    bool hideHud = false;    // photo mode with the HUD hidden
};

static View update(const wotm::GameCamera &gc)
{
    View v;
    bool on = g_wotmFreeCam.load(std::memory_order_relaxed);
    // Keyboardless test (WOTM_FREECAM=<s>): turns on s seconds after the first stage is on screen, rises and turns by itself for 10 s, then turns off
    static const double autoAt = std::getenv("WOTM_FREECAM") ? std::atof(std::getenv("WOTM_FREECAM")) : -1.0;
    static double firstT = -1.0;
    if (firstT < 0.0) firstT = GetTime();
    const double autoT = autoAt >= 0.0 ? GetTime() - firstT - autoAt : -1.0;
    static int autoStage = 0;   // 0 waiting, 1 moving, 2 done
    bool toggle = IsKeyPressed(KEY_F5) && !wotmPhotoActive() && !wotm_netstatus::g_active;   // the free camera writes to game RAM: not during a network match
    if (autoAt >= 0.0 && ((autoStage == 0 && autoT >= 0.0) || (autoStage == 1 && autoT >= 10.0)))
    {
        toggle = on == (autoStage == 1);
        std::fprintf(stderr, "[freecam] automatic test: %s (t=%.1f s)\n", autoStage == 0 ? "on" : "off", GetTime() - firstT);
        ++autoStage;
    }
    if (toggle)
    {
        on = !on;
        if (on)
        {
            std::lock_guard<std::mutex> lk(g_wotmFreeCamM);
            WotmFreeCamPose &p = g_wotmFreeCamPose;
            for (int i = 0; i < 3; ++i) p.pos[i] = gc.pos[i];
            p.heading = std::atan2(gc.fwd[0], gc.fwd[1]);   // mathfRotationFromVector
            p.pitch = std::atan2(gc.fwd[2], std::sqrt(gc.fwd[0] * gc.fwd[0] + gc.fwd[1] * gc.fwd[1]));
            g_wotmFreeCamPad = false;
        }
        g_wotmFreeCam = on;
        std::fprintf(stderr, "[freecam] %s\n", on ? "on" : "off");
    }
    if (!on)
        return v;
    if (IsKeyPressed(KEY_TAB))
    {
        g_wotmFreeCamPad = !g_wotmFreeCamPad.load(std::memory_order_relaxed);
        std::fprintf(stderr, "[freecam] pad %s\n", g_wotmFreeCamPad ? "with the game" : "locked");
    }
    const bool pad = g_wotmFreeCamPad.load(std::memory_order_relaxed);
    const float dt = std::min(GetFrameTime(), 0.1f);
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
        speed = std::clamp(speed * std::pow(1.25f, wheel), 5.f, 20000.f);

    std::lock_guard<std::mutex> lk(g_wotmFreeCamM);
    WotmFreeCamPose &p = g_wotmFreeCamPose;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        const Vector2 d = GetMouseDelta();
        p.heading += d.x * 0.003f;   // positive heading turns toward +X (to the right, seen from above)
        p.pitch -= d.y * 0.003f;
    }
    if (!pad)
    {
        const float turn = 1.5f * dt;
        if (IsKeyDown(KEY_LEFT)) p.heading -= turn;
        if (IsKeyDown(KEY_RIGHT)) p.heading += turn;
        if (IsKeyDown(KEY_UP)) p.pitch += turn;
        if (IsKeyDown(KEY_DOWN)) p.pitch -= turn;
    }
    p.pitch = std::clamp(p.pitch, -1.55f, 1.55f);
    p.heading = std::remainder(p.heading, 6.2831853f);
    if (!pad)
    {
        float r[3], f[3], u[3];
        wotmFreeCamAxes(p, r, f, u);
        float k = speed * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) k *= 5.f;
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) k *= 0.2f;
        const float mf = float(IsKeyDown(KEY_W)) - float(IsKeyDown(KEY_S));
        const float mr = float(IsKeyDown(KEY_D)) - float(IsKeyDown(KEY_A));
        const float mu = float(IsKeyDown(KEY_E)) - float(IsKeyDown(KEY_Q));
        for (int i = 0; i < 3; ++i)
            p.pos[i] += k * (mf * f[i] + mr * r[i]);
        p.pos[2] += k * mu;   // world Z
    }
    if (autoStage == 1)
    {
        p.pos[2] += 30.f * dt;
        p.heading += 0.25f * dt;
    }
    v.on = true;
    v.pose = p;
    v.override = wotmPaused();
    return v;
}

// Paused: removes from the scene what the game pins in front of its own camera. The sky stays where it is: the game draws it at skyCs->trans, fixed
// in the world, not at the eye.
static void applyToScene(wotm::Scene &scene, const wotm::GameCamera &gc, const WotmFreeCamPose &p)
{
    (void)gc;
    (void)p;
    scene.hud.clear();
    scene.hp.clear();
    scene.hpOverlay.clear();
    scene.overlay.clear();
}

static wotm::GameCamera camera(const wotm::GameCamera &gc, const WotmFreeCamPose &p, float zoom = 1.f)
{
    wotm::GameCamera c = gc;
    float r[3];
    wotmFreeCamAxes(p, r, c.fwd, c.up);
    for (int i = 0; i < 3; ++i) c.pos[i] = p.pos[i];
    if (zoom != 1.f)
        c.fovy = 2.f * std::atan(zoom * std::tan(gc.fovy * 3.14159265f / 360.f)) * 180.f / 3.14159265f;
    return c;
}

} // namespace wotm_freecam
// The controls appear in the icon legend at the bottom (wotmFreeCamLegend, wotm_photo.inc)
