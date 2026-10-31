// wotm_mouse.hpp: mouse for the modern camera (step 7 of the orbit camera, 75f5be1:docs/investigacoes/2026-10-22/camera_orbital_plano.md). Two sides:
//  - window (wotm_live.inc, on every present): says which player the keyboard feeds, captures the cursor while that player's orbit
//    is steering (and the window has focus) and accumulates the mouse movement;
//  - EE thread (wotm_orbit.inc, after Cameras::Update): feeds the accumulated movement into the keyboard player's orbit and signals,
//    on every frame where the orbit steers outside the pause, that the cursor may stay captured. With no signal for ~150 ms (menus,
//    pause, photo mode, F7), the window releases the cursor.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

namespace wotm::mouse
{
inline std::atomic<int> g_kbPlayer{0};        // player (0..3) the keyboard feeds; -1 = none (window writes)
inline std::atomic<int64_t> g_wantUntil{0};   // monotonic-clock ms until which the cursor may stay captured (EE writes)
inline std::atomic<bool> g_captured{false};   // cursor captured right now (window writes)
inline std::atomic<int> g_wheel{0};          // wheel steps not used yet (+ up, - down; window adds, keyboard consumes)
// Target switch requested from the keyboard (step 8: Tab, R, wheel), per player: +1 next, -1 previous. The keyboard (wotm_pad.inc) adds
// on the key's edge, the interpretInputs hook (wotm_overrides.cpp) consumes it while the target is locked, whatever the lock-on
inline std::atomic<int> g_targetReq[4] = {};
inline std::mutex g_m;
inline float g_dx = 0.f, g_dy = 0.f;          // pixels accumulated since the last take (guarded by g_m)

inline int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// EE: the keyboard player's orbit is steering this frame
inline void want() { g_wantUntil.store(nowMs() + 150, std::memory_order_relaxed); }
inline bool wanted() { return nowMs() < g_wantUntil.load(std::memory_order_relaxed); }
// Window: this frame's movement (only while the cursor is captured)
inline void add(float dx, float dy)
{
    std::lock_guard<std::mutex> lk(g_m);
    g_dx += dx;
    g_dy += dy;
}
// EE: the accumulated movement (resets it)
inline void take(float &dx, float &dy)
{
    std::lock_guard<std::mutex> lk(g_m);
    dx = g_dx;
    dy = g_dy;
    g_dx = g_dy = 0.f;
}
inline void clear()
{
    std::lock_guard<std::mutex> lk(g_m);
    g_dx = g_dy = 0.f;
}
} // namespace wotm::mouse

// Mouse in menus (step 9): with the cursor released, the window (wotm_pad.inc, pointerFrame) records where it is in the native frame and
// counts the clicks; whoever draws a native menu on the big screen (wotm_mainmenu.inc) finds the item under it, and the screen's hook, on
// the EE thread, consumes the clicks. A native menu with items under the mouse signals (claim) that the left click is its own; without
// that, the left click acts as X (confirms the selected item) on the game's screens.
namespace wotm::mouse
{
struct Pointer
{
    float x = -1.f, y = -1.f;   // in the native frame (RW x RH), only with the cursor released and inside it
    int rw = 0, rh = 0;
    bool in = false;
};
inline Pointer g_ptr;                          // window thread only
inline std::atomic<uint32_t> g_moveSeq{0};     // goes up on every frame the released cursor moved (the EE only selects by mouse after that)
inline std::atomic<int64_t> g_clickL{0}, g_clickR{0};   // time (ms) of the last unconsumed click (released cursor), 0 = none
inline std::atomic<int64_t> g_claimUntil{0};
inline void claim() { g_claimUntil.store(nowMs() + 150, std::memory_order_relaxed); }
inline bool claimed() { return nowMs() < g_claimUntil.load(std::memory_order_relaxed); }
inline void click(std::atomic<int64_t> &c) { c.store(nowMs(), std::memory_order_relaxed); }
// A click from at most 300 ms ago (an old one, from outside the menus, doesn't count)
inline bool takeClick(std::atomic<int64_t> &c)
{
    const int64_t t = c.exchange(0, std::memory_order_relaxed);
    return t != 0 && nowMs() - t < 300;
}
} // namespace wotm::mouse

// The pointer for the EE thread (the game's screens know where they write their text, in PS2 screen units): x, y in the native frame
// (packed float bits) and the frame's size; 0 = outside the frame or cursor captured
namespace wotm::mouse
{
inline std::atomic<uint64_t> g_ptrXY{0};
inline std::atomic<uint32_t> g_ptrWH{0};   // rw << 16 | rh
inline void publishPointer(bool in, float x, float y, int rw, int rh)
{
    uint32_t a, b;
    std::memcpy(&a, &x, 4);
    std::memcpy(&b, &y, 4);
    g_ptrXY.store(in ? (uint64_t(a) << 32 | b) : 0, std::memory_order_relaxed);
    g_ptrWH.store(uint32_t(rw) << 16 | uint32_t(rh & 0xFFFF), std::memory_order_relaxed);
}
inline bool pointer(float &x, float &y, int &rw, int &rh)
{
    const uint64_t v = g_ptrXY.load(std::memory_order_relaxed);
    const uint32_t wh = g_ptrWH.load(std::memory_order_relaxed);
    if (!v || !wh) return false;
    const uint32_t a = uint32_t(v >> 32), b = uint32_t(v);
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    rw = int(wh >> 16);
    rh = int(wh & 0xFFFF);
    return rw > 0 && rh > 0;
}
// Game menu with items under the mouse (pause and other game screens): the EE asks for an X for a few pad reads (a click on an item)
inline std::atomic<int> g_crossReq{0};
// The request only lasts a short time: the game usually acts on the first read already (the pause's QUIT leaves the stage), and the
// second one, with no mouse menu during the load, used to hang around until the main menu, where it became an X on the item under the cursor
inline std::atomic<int64_t> g_crossReqUntil{0};
inline void requestCross(int reads)
{
    g_crossReqUntil.store(nowMs() + 200, std::memory_order_relaxed);
    g_crossReq.store(reads, std::memory_order_relaxed);
}
inline std::atomic<int64_t> g_gameMenuUntil{0};   // until when (ms) the game screen with mouse is open (the pause is not a shell menu)
inline void gameMenu() { g_gameMenuUntil.store(nowMs() + 150, std::memory_order_relaxed); }
inline bool inGameMenu() { return nowMs() < g_gameMenuUntil.load(std::memory_order_relaxed); }

// Game screens that move sideways (stage, number of CPUs, minigame, unlocks): the EE signals and the keyboard (wotm_pad.inc) sends Q/E as
// left/right; with `wheel`, the wheel too (otherwise it stays up/down)
inline std::atomic<int64_t> g_sideUntil{0};
inline std::atomic<bool> g_sideWheel{false};
inline void sideScreen(bool wheel)
{
    g_sideWheel.store(wheel, std::memory_order_relaxed);
    g_sideUntil.store(nowMs() + 150, std::memory_order_relaxed);
}
inline bool sideActive() { return nowMs() < g_sideUntil.load(std::memory_order_relaxed); }
// Save list open (EE, wotm_save.inc): 1 load, 2 save, 3 confirm overwrite; the window puts up the caption at the bottom
inline std::atomic<int64_t> g_saveListUntil{0};
inline std::atomic<int> g_saveListMode{0};
inline void saveList(int mode)
{
    g_saveListMode.store(mode, std::memory_order_relaxed);
    g_saveListUntil.store(nowMs() + 150, std::memory_order_relaxed);
}
inline int saveListMode() { return nowMs() < g_saveListUntil.load(std::memory_order_relaxed) ? g_saveListMode.load(std::memory_order_relaxed) : 0; }
// Name of the confirm button on the current device ("ENTER", "A", "CROSS"...), for the game's text (the window writes it)
inline std::mutex g_lblM;
inline std::string g_confirmLabel = "X";
inline std::string confirmLabel()
{
    std::lock_guard<std::mutex> lk(g_lblM);
    return g_confirmLabel;
}
} // namespace wotm::mouse

// The pointer in PS2 screen units (640 x 224, the units of the game's text), published by the window along with the present
namespace wotm::mouse
{
inline std::atomic<uint64_t> g_gsXY{0};
inline std::atomic<bool> g_gsIn{false};
inline void publishGs(bool in, float gx, float gy)
{
    uint32_t a, b;
    std::memcpy(&a, &gx, 4);
    std::memcpy(&b, &gy, 4);
    g_gsXY.store(uint64_t(a) << 32 | b, std::memory_order_relaxed);
    g_gsIn.store(in, std::memory_order_relaxed);
}
inline bool gsPointer(float &gx, float &gy)
{
    if (!g_gsIn.load(std::memory_order_relaxed)) return false;
    const uint64_t v = g_gsXY.load(std::memory_order_relaxed);
    const uint32_t a = uint32_t(v >> 32), b = uint32_t(v);
    std::memcpy(&gx, &a, 4);
    std::memcpy(&gy, &b, 4);
    return true;
}
} // namespace wotm::mouse
