// ABOUTME: Map editor, window side (included by wotm_live.inc): F10 opens it on the free camera, the keys become editor commands for the
// ABOUTME: EE thread (runtime/wotm_mapedit.inc), and the pieces' markers, the crosshair and the panel are drawn over the scene.
//
// Keys while it's open (the free camera's WASD, Q/E, arrows and mouse still fly and look):
//   Enter or left click: pick the piece under the crosshair    I/K/J/L: slide it (camera relative, 10 units; Shift x10)
//   U/O: up/down    Z/X: turn 15 degrees    C: copy it (the copy is picked)    Delete: remove    Backspace: undo
//   R: back to its disc place    F4: save to mods/Creator/stages/<id>/    F10: close (the free camera stays: F5 turns it off)
#include "wotm_mapedit_live.h"

namespace wotm_mapedit_view
{
namespace me = wotm::mapedit;

static bool g_shown = false;   // what update() decided this frame (draw3d and draw2d follow it)

static void send(const me::Command &c)
{
    std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
    g_wotmMapEdit.cmds.push_back(c);
}

static bool pressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

// every drawn frame, before the free camera reads its keys
static void update(const wotm::GameCamera &gc)
{
    bool ready, on;
    {
        std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
        ready = g_wotmMapEdit.enabled && g_wotmMapEdit.ready && !wotm_netstatus::g_active;
        if (ready && IsKeyPressed(KEY_F10) && !wotmPhotoActive())
        {
            g_wotmMapEdit.on = !g_wotmMapEdit.on;
            if (!g_wotmMapEdit.on) std::fprintf(stderr, "[mapedit] editor closed (F10 opens it)\n");
        }
        if (!ready)
            g_wotmMapEdit.on = false;
        on = g_wotmMapEdit.on;
    }
    if (on && !g_shown)
        std::fprintf(stderr, "[mapedit] editor open on the free camera (F10 closes it)\n");
    g_shown = on;
    if (!on)
        return;
    if (!g_wotmFreeCam.load(std::memory_order_relaxed))
    {   // the editor flies the free camera: it starts where the game camera is, as F5 does
        std::lock_guard<std::mutex> lk(g_wotmFreeCamM);
        WotmFreeCamPose &p = g_wotmFreeCamPose;
        for (int i = 0; i < 3; ++i) p.pos[i] = gc.pos[i];
        p.heading = std::atan2(gc.fwd[0], gc.fwd[1]);
        p.pitch = std::atan2(gc.fwd[2], std::sqrt(gc.fwd[0] * gc.fwd[0] + gc.fwd[1] * gc.fwd[1]));
        g_wotmFreeCamPad = false;
        g_wotmFreeCam = true;
    }
    WotmFreeCamPose pose;
    {
        std::lock_guard<std::mutex> lk(g_wotmFreeCamM);
        pose = g_wotmFreeCamPose;
    }
    const float step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 100.f : 10.f;
    // the free camera's heading is from +Y towards +X; the session's slide yaw is from +X towards +Y
    const float yawDeg = 90.f - pose.heading * 57.29578f;
    me::Command c;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        float r[3], f[3], u[3];
        wotmFreeCamAxes(pose, r, f, u);
        c.kind = me::Command::Pick;
        for (int k = 0; k < 3; ++k) c.a[k] = pose.pos[k], c.a[3 + k] = f[k];
        send(c);
    }
    const float fwd = float(pressed(KEY_I)) - float(pressed(KEY_K)), right = float(pressed(KEY_L)) - float(pressed(KEY_J));
    if (fwd != 0.f || right != 0.f)
    {
        c.kind = me::Command::Slide;
        c.a[0] = fwd * step, c.a[1] = right * step, c.a[2] = yawDeg;
        send(c);
    }
    const float up = float(pressed(KEY_U)) - float(pressed(KEY_O));
    if (up != 0.f)
    {
        c = me::Command{};
        c.kind = me::Command::Move;
        c.a[2] = up * step;
        send(c);
    }
    const float turn = float(pressed(KEY_X)) - float(pressed(KEY_Z));
    if (turn != 0.f)
    {
        c = me::Command{};
        c.kind = me::Command::Turn;
        c.a[0] = turn * 15.f;
        send(c);
    }
    auto simple = [](me::Command::Kind k) { me::Command s; s.kind = k; send(s); };
    if (IsKeyPressed(KEY_C))
    {
        c = me::Command{};
        c.kind = me::Command::Copy;
        c.a[0] = 0.f, c.a[1] = 0.f;   // in place: slide it out with I/K/J/L
        send(c);
    }
    if (IsKeyPressed(KEY_DELETE)) simple(me::Command::Remove);
    if (pressed(KEY_BACKSPACE)) simple(me::Command::Undo);
    if (IsKeyPressed(KEY_R)) simple(me::Command::Reset);
    if (IsKeyPressed(KEY_F4)) simple(me::Command::Save);
}

// over the scene, no depth test: the selection in yellow, copies in cyan, removed pieces in red where the disc has them
static void draw3d()
{
    if (!g_shown)
        return;
    std::vector<me::ItemView> items;
    int sel;
    {
        std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
        items = g_wotmMapEdit.items;
        sel = g_wotmMapEdit.selected;
    }
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const me::ItemView &it = items[i];
        if (it.radius <= 0.f || (!it.copy && !it.removed && int(i) != sel))
            continue;
        const Color col = int(i) == sel ? Color{255, 230, 40, 230} : it.removed ? Color{255, 60, 60, 160} : Color{60, 220, 255, 200};
        if (it.removed && it.copy)
            continue;
        DrawSphereWires(Vector3{it.centre[0], it.centre[1], it.centre[2]}, it.radius, 6, 12, col);
        // the facing a turn gives it: a line from its place along its turned +Y
        const float a = it.turn * 0.017453293f, len = it.radius * 0.8f;
        DrawLine3D(Vector3{it.pos[0], it.pos[1], it.pos[2]}, Vector3{it.pos[0] - std::sin(a) * len, it.pos[1] + std::cos(a) * len, it.pos[2]}, col);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

// the crosshair and the panel, on the window
static void draw2d()
{
    if (!g_shown)
        return;
    const int sw = GetScreenWidth(), sh = GetScreenHeight(), fs = 18;
    DrawLine(sw / 2 - 10, sh / 2, sw / 2 + 10, sh / 2, YELLOW);
    DrawLine(sw / 2, sh / 2 - 10, sw / 2, sh / 2 + 10, YELLOW);
    std::string id, stage, msg, picked = "NOTHING PICKED (ENTER OR CLICK ON A PIECE)";
    int edits, copies;
    {
        std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
        id = g_wotmMapEdit.id, stage = g_wotmMapEdit.stage, msg = g_wotmMapEdit.msg;
        edits = g_wotmMapEdit.edits, copies = g_wotmMapEdit.copies;
        const int s = g_wotmMapEdit.selected;
        if (s >= 0 && s < int(g_wotmMapEdit.items.size()))
        {
            const me::ItemView &it = g_wotmMapEdit.items[size_t(s)];
            char b[96];
            std::snprintf(b, sizeof b, "%s 0x%X AT %.0f %.0f %.0f TURN %.0f", it.copy ? "COPY OF" : "PIECE", it.node - wotm::stagemap::kBase,
                          it.pos[0], it.pos[1], it.pos[2], it.turn);
            picked = b;
        }
    }
    char lines[8][112];
    int nl = 0;
    std::snprintf(lines[nl++], 112, "MAP EDITOR: %s ON %s   (%d EDITS, %d COPIES)", id.c_str(), stage.c_str(), edits, copies);
    std::snprintf(lines[nl++], 112, "%s", picked.c_str());
    std::snprintf(lines[nl++], 112, "ENTER PICK  IJKL SLIDE  U/O UP/DOWN  Z/X TURN  C COPY  DEL REMOVE");
    std::snprintf(lines[nl++], 112, "BACKSPACE UNDO  R RESET  F4 SAVE  F10 CLOSE  (WASD Q/E ARROWS FLY)");
    std::snprintf(lines[nl++], 112, "MOVES SHOW AT ONCE; TURNS, COPIES AND COLLISION WHEN THE MAP IS PLAYED");
    if (!msg.empty())
        std::snprintf(lines[nl++], 112, "%s", msg.c_str());
    int w = 0;
    for (int i = 0; i < nl; ++i)
        w = std::max(w, MeasureText(lines[i], fs));
    const int x = 12, top = sh - nl * (fs + 4) - sh / 8;   // above the free camera's legend strip
    DrawRectangle(x - 6, top - 6, w + 12, nl * (fs + 4) + 8, Color{0, 0, 0, 170});
    for (int i = 0; i < nl; ++i)
        DrawText(lines[i], x, top + i * (fs + 4), fs, i == 0 ? YELLOW : RAYWHITE);
}

} // namespace wotm_mapedit_view
