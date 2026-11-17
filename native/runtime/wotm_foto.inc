// wotm_foto.inc: pause, frame advance and snapshots with a note (included by wotm_scene.inc, after wotm_live.inc and before
// wotm_vu1check.inc; global scope of ps2_runtime.cpp).
//
// Window keys (presentation thread, wotmFotoKeys):
//  - F7 pauses and resumes. The pause holds the EE thread at the capture point (wotmPauseBlock, called from wotm_overrides.cpp on
//    entry to timerWaitForMinUpdateRate, right after wotmCaptureFrame): the frame on screen is the last finished one and the next
//    has not started. Movies (FMV) and sound keep going on their own;
//  - F6, while paused, advances one game frame;
//  - F8, with WOTM_FOTOS, takes a snapshot: the game runs until the VU1 vs native check records one whole frame (from hier(0, 0) to
//    the next, wotm_vu1check.inc), stops paused on it (wotmFotoDone) and opens a text box for the note. The snapshot is the frame
//    after the one on screen when F8 was pressed; the screen shows the captured frame while the note is typed. Enter saves (empty =
//    no note; Esc would close the window, it is raylib's exit key). Without WOTM_FOTOS, F8 only triggers the check (WOTM_VU1CHECK).
// Each snapshot goes to <folder>/<session's YYYY-MM-DD_HHMMSS>/: foto_NNN.vu1snap (the snapshot: tools\vu1cmp.ps1 redoes the check
// and the ref/nat/diff images), foto_NNN_tela.png (the native frame as it looked in the window) and a line in notas.txt (number,
// g_frame, currScreen, loaded resources, note). WOTM_FOTOS=1 uses <cwd>/../fotos (run_live runs in discroot: <runtime>/fotos).
#include <condition_variable>
#include "wotm_netstatus.h"   // network match: no pause and no snapshots
#include "wotm_syms.h"   // wotm::sym: ELF addresses by name
#include <ctime>
#include <filesystem>
#include <fstream>

namespace wotm_foto
{
std::mutex m;
std::condition_variable cv;
bool paused = false;        // F7 / end of snapshot
int steps = 0;              // F6: frames to let run before holding again
bool photoRun = false;      // F8: the game runs until the snapshot finishes, even while paused
std::atomic<bool> blocked{false};        // the EE thread is stopped here (watchdog, overlay)
std::atomic<bool> tookFrame{false};      // the frame in progress had a stop (wotmPauseTookFrame)
std::string dir;            // session folder ("" = snapshots off); created on the first snapshot (makeDir)
unsigned next = 1;          // number of the next snapshot

// Snapshot ready (EE thread -> presentation)
struct Done { bool ready = false; unsigned n = 0; uint32_t frame = 0, screen = 0; std::string file; };
Done done;
// Presentation state
bool exported = false, noteOpen = false;
std::string note, msg;
double msgUntil = 0.0, photoAsked = 0.0;

Font font()
{
    // Consolas with Latin-1 (accented characters in the note); without the file, raylib's default font
    static Font f = [] {
        std::vector<int> cps;
        for (int c = 32; c < 256; ++c) cps.push_back(c);
        for (int c : {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026}) cps.push_back(c);
        Font r = LoadFontEx("C:/Windows/Fonts/consola.ttf", 32, cps.data(), int(cps.size()));
        if (r.texture.id == 0) return GetFontDefault();
        SetTextureFilter(r.texture, TEXTURE_FILTER_BILINEAR);
        return r;
    }();
    return f;
}

void say(const std::string &s, double secs = 3.0)
{
    msg = s;
    msgUntil = GetTime() + secs;
}

bool makeDir()
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) std::fprintf(stderr, "[snap] could not create %s: %s\n", dir.c_str(), ec.message().c_str());
    return !ec;
}

std::string name(unsigned n, const char *suffix)
{
    char b[64];
    std::snprintf(b, sizeof b, "foto_%03u%s", n, suffix);
    return (std::filesystem::path(dir) / b).string();
}
} // namespace wotm_foto

// Turns snapshots on (wotmVu1CheckInstall, with WOTM_FOTOS): picks the session folder (created only on the first snapshot).
static bool wotmFotoInit(const char *base)
{
    namespace fs = std::filesystem;
    const fs::path root = (std::strcmp(base, "1") == 0) ? fs::path("..") / "fotos" : fs::path(base);
    const std::time_t t = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d_%H%M%S", std::localtime(&t));
    std::error_code ec;
    wotm_foto::dir = fs::absolute(root / stamp, ec).string();
    if (ec) return false;
    std::fprintf(stderr, "[snap] F8 takes a snapshot into %s (F7 pauses, F6 advances one frame)\n", wotm_foto::dir.c_str());
    return true;
}
static bool wotmFotosOn() { return !wotm_foto::dir.empty(); }
// Path of the snapshot file for the snapshot in progress (wotm_vu1check.inc, snap::begin)
static std::string wotmFotoSnapPath()
{
    wotm_foto::makeDir();
    return wotm_foto::name(wotm_foto::next, ".vu1snap");
}

// EE thread: holds while paused (with no step or snapshot in progress). Returns whether it stopped.
bool wotmPauseBlock(PS2Runtime *rt)
{
    using namespace wotm_foto;
    std::unique_lock<std::mutex> lk(m);
    if (paused && steps > 0 && !photoRun) {
        --steps;
        return false;
    }
    if (!paused || photoRun) return false;
    blocked.store(true, std::memory_order_relaxed);
    const auto t0 = std::chrono::steady_clock::now();
    while (paused && steps == 0 && !photoRun && !(rt && rt->isStopRequested()))
        cv.wait_for(lk, std::chrono::milliseconds(50));
    if (steps > 0 && !photoRun) --steps;
    lk.unlock();
    // The vblanks counted during the pause do not all fall due on resume (shiftHostDeadlines clears vblankDue). The next vblank's
    // deadline is NOT moved: the vblank clock (EeSchedulerGame.cpp) keeps running during the pause and keeps it close to now; adding
    // the pause duration would leave the game stopped for that same time after release (a minute of photo mode = a minute "frozen")
    (void)t0;
    if (rt) rt->eeScheduler().shiftHostDeadlines(std::chrono::steady_clock::duration::zero());
    blocked.store(false, std::memory_order_relaxed);
    tookFrame.store(true, std::memory_order_relaxed);
    return true;
}
// Presentation: holds the EE thread at the frame boundary (like F7) until hold(false); returns whether it stopped within 2 s. For
// code that needs the RAM still for a moment (interpolation self-test: drawing reads parts of the RAM live).
bool wotmHoldGame(bool hold)
{
    using namespace wotm_foto;
    {
        std::lock_guard<std::mutex> lk(m);
        paused = hold;
        steps = 0;
    }
    cv.notify_all();
    if (!hold)
        return true;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!blocked.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return blocked.load(std::memory_order_relaxed);
}
// wotm_overrides.cpp, on entry to timerWaitForMinUpdateRate: the frame that just ended had a stop (its time does not count)
bool wotmPauseTookFrame() { return wotm_foto::tookFrame.exchange(false, std::memory_order_relaxed); }
// Watchdog (ps2_runtime.cpp): g_frame stopped on purpose
bool wotmPaused() { return wotm_foto::blocked.load(std::memory_order_relaxed) || wotm_netstatus::g_waiting.load(std::memory_order_relaxed); }

// EE thread (wotm_vu1check.inc, entry to the hier(0, 0) after the captured frame): the snapshot is saved; stays paused here.
static void wotmFotoDone(const uint8_t *rdram, uint32_t frame, const std::string &file)
{
    using namespace wotm_foto;
    {
        std::lock_guard<std::mutex> lk(m);
        done.ready = true;
        done.n = next++;
        done.frame = frame;
        std::memcpy(&done.screen, rdram + wotm::sym::currScreen, 4);
        done.file = file;
        photoRun = false;
        paused = true;
        steps = 0;
    }
    std::fprintf(stderr, "[snap] %u: g_frame %u saved to %s\n", done.n, frame, file.c_str());
}

// Photo mode (wotm_photo.inc): turns the same pause as F7 on and off
void wotmPhotoPause(bool on)
{
    using namespace wotm_foto;
    std::lock_guard<std::mutex> lk(m);
    paused = on;
    steps = 0;
    cv.notify_all();
}

// Presentation, before drawing: F6/F7/F8 (an open note owns the keyboard; photo mode handles the pause on its own)
static void wotmFotoKeys()
{
    using namespace wotm_foto;
    wotmPhotoWatchdog();
    if (noteOpen || wotmPhotoActive() || wotm_netstatus::g_active) return;   // online, the pause would stop only this PC (and the lockstep, everyone)
    std::lock_guard<std::mutex> lk(m);
    if (IsKeyPressed(KEY_F7)) {
        paused = !paused;
        steps = 0;
        say(paused ? "PAUSED" : "resuming", paused ? 0.0 : 1.0);
    }
    if (IsKeyPressed(KEY_F6) && paused && !photoRun) ++steps;
    if (IsKeyPressed(KEY_F8)) {
        if (!wotmFotosOn()) {
            g_wotmShotKey = true;   // just the check (WOTM_VU1CHECK), as before snapshots existed
        } else if (!photoRun) {
            g_wotmShotKey = true;
            photoRun = true;
            paused = true;
            steps = 0;
            photoAsked = GetTime();
            say("taking photo...", 5.0);
        }
    }
    // Without hier(0, 0) (movie, loading) the check never fires: give up and pause on the next frame
    if (photoRun && GetTime() - photoAsked > 3.0) {
        photoRun = false;
        g_wotmShotKey = false;
        say("can't take a photo on this screen (no game frame)", 4.0);
    }
    cv.notify_all();
}

// Presentation, after native drawing (drewNative: the native layer drew the frame): saves the snapshot's PNG, draws the note box
// and the pause notice.
static void wotmFotoOverlay(bool drewNative, const uint8_t *rdram)
{
    using namespace wotm_foto;
    Done d;
    {
        std::lock_guard<std::mutex> lk(m);
        d = done;
    }
    if (d.ready && !exported) {
        exported = true;
        const std::string png = name(d.n, "_tela.png");
        if (!(drewNative && wotm_live::exportFrame(png.c_str()))) {
            Image img = LoadImageFromScreen();
            ExportImage(img, png.c_str());
            UnloadImage(img);
        }
        noteOpen = true;
        note.clear();
        while (GetCharPressed()) {}   // anything typed before the box opened is dropped
    }
    const float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    const Font f = font();
    if (noteOpen) {
        for (int c = GetCharPressed(); c; c = GetCharPressed())
            if (c >= 32 && note.size() < 400) {
                int n = 0;
                note += CodepointToUTF8(c, &n);
            }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !note.empty()) {
            size_t i = note.size() - 1;
            while (i > 0 && (uint8_t(note[i]) & 0xC0) == 0x80) --i;   // back up to the start of the UTF-8 character
            note.erase(i);
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            const std::string recursos = wotm_live::resources();
            std::ofstream o(std::filesystem::path(dir) / "notas.txt", std::ios::app | std::ios::binary);
            char head[160];
            std::snprintf(head, sizeof head, "foto_%03u  g_frame %u  currScreen %u  ", d.n, d.frame, d.screen);
            o << head << "resources: " << (recursos.empty() ? "-" : recursos) << "\n    " << (note.empty() ? "(no note)" : note) << "\n";
            std::fprintf(stderr, "[snap] %u: note \"%s\"\n", d.n, note.c_str());
            noteOpen = false;
            exported = false;
            {
                std::lock_guard<std::mutex> lk(m);
                done.ready = false;
            }
            say("photo " + std::to_string(d.n) + " saved (F7 resumes, F6 steps)", 3.0);
        }
    }
    if (noteOpen) {
        const float h = 96.f, y = sh - h - 16.f;
        DrawRectangle(16, int(y), int(sw) - 32, int(h), Fade(BLACK, 0.85f));
        DrawRectangleLines(16, int(y), int(sw) - 32, int(h), YELLOW);
        DrawTextEx(f, TextFormat("Photo %03u (g_frame %u, screen %u): what is wrong? Enter saves", d.n, d.frame, d.screen),
                   Vector2{28.f, y + 10.f}, 18.f, 0.f, YELLOW);
        const bool blink = std::fmod(GetTime(), 1.0) < 0.5;
        DrawTextEx(f, (note + (blink ? "_" : " ")).c_str(), Vector2{28.f, y + 44.f}, 22.f, 0.f, RAYWHITE);
        return;
    }
    if (wotmPhotoActive()) return;   // photo mode has its own caption
    bool isPaused, run;
    {
        std::lock_guard<std::mutex> lk(m);
        isPaused = paused;
        run = photoRun;
    }
    std::string line = GetTime() < msgUntil ? msg : "";
    if (isPaused && !run && line.empty()) line = wotmFotosOn() ? "PAUSED  F7 resumes  F6 steps one frame  F8 photo" : "PAUSED  F7 resumes  F6 steps one frame";
    if (!line.empty()) {
        if (isPaused && !run && rdram) {
            uint32_t g = 0;
            std::memcpy(&g, rdram + wotm::sym::g_frame, 4);
            line += "   (g_frame " + std::to_string(g) + ")";
        }
        const Vector2 sz = MeasureTextEx(f, line.c_str(), 20.f, 0.f);
        const float x = (sw - sz.x) * 0.5f, y = sh - sz.y - 24.f;   // footer: the HUD is at the top
        DrawRectangle(int(x - 10.f), int(y - 6.f), int(sz.x + 20.f), int(sz.y + 12.f), Fade(BLACK, 0.7f));
        DrawTextEx(f, line.c_str(), Vector2{x, y}, 20.f, 0.f, isPaused ? YELLOW : RAYWHITE);
    }
}
