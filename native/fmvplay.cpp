// Standalone player for the movies (.PSS) using the same code as the runtime (wotm_fmv.hpp). For checking picture, sound and aspect
// ratio without the game:
//   fmvplay disc/MOV/INTRO_M.PSS                     plays in the window (Space pauses, Esc quits)
//   fmvplay file.pss --shot q.png --at 5             fixed 60 Hz clock (not real time): writes the frame at 5 s and exits
//   fmvplay file.pss --wav sound.wav --fast          decodes everything as fast as possible and writes the audio to WAV
//   fmvplay mov/intro_m.pss --iso game.iso           reads the movie from inside the ISO (like the runtime with PS2X_CD_IMAGE)
//   fmvplay file.pss --frame 150 --shot q.png        writes the decoded pixels of frame 150
// Prints to stderr: size, display aspect ratio, frames shown and audio samples.
#include <cstdlib>
#include <string>

#include "wotm_fmv.hpp"

static void writeWav(const std::string &path, const std::vector<int16_t> &s, uint32_t rate) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    const uint32_t data = uint32_t(s.size() * 2), riff = 36 + data, fmt = 16, br = rate * 4;
    const uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f);
    std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f); std::fwrite(s.data(), 2, s.size(), f);
    std::fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: fmvplay file.pss [--shot out.png --at seconds] [--wav out.wav] [--fast] [--mute]\n");
        return 1;
    }
    std::string shot, wav, iso;   // --iso image.iso: the first argument is the path inside it (mov/intro_m.pss)
    double at = 1.0;
    bool fast = false, mute = false;
    long frameN = -1;   // --frame N: writes the decoded pixels of frame N (counting from 0) to --shot, bypassing the window
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--at" && i + 1 < argc) at = std::atof(argv[++i]);
        else if (a == "--wav" && i + 1 < argc) wav = argv[++i];
        else if (a == "--fast") fast = true;
        else if (a == "--mute") mute = true;
        else if (a == "--iso" && i + 1 < argc) iso = argv[++i];
        else if (a == "--frame" && i + 1 < argc) frameN = std::atol(argv[++i]);
    }
    if (frameN >= 0) fast = true;
    const bool fixedClock = !shot.empty() || fast;
    if (fixedClock) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(960, 540, "fmvplay");
    if (!fixedClock && !mute) InitAudioDevice();
    if (!fixedClock) SetTargetFPS(60);

    wotm::fmv::Movie m;
    std::vector<int16_t> tap;
    if (!wav.empty()) m.audioTap = &tap;
    const wotm::fmv::Source src = iso.empty() ? wotm::fmv::Source{argv[1]} : wotm::fmv::isoFind(iso, wotm::fmv::discRelative(argv[1]));
    if (!src.ok() || !m.open(src, true)) {
        std::fprintf(stderr, "failed to open %s\n", argv[1]);
        CloseWindow();
        return 1;
    }
    long draws = 0;
    double clock = 0.0, pausedAt = -1.0, pauseTotal = 0.0;
    bool ok = true, shotDone = false;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE) && !fixedClock) {
            if (pausedAt < 0) pausedAt = GetTime();
            else { pauseTotal += GetTime() - pausedAt; pausedAt = -1.0; }
        }
        if (fixedClock) clock = fast ? clock + 1.0 / 29.97 : clock + 1.0 / 60.0;
        else if (pausedAt < 0) clock = GetTime() - pauseTotal;
        if (pausedAt < 0 || fixedClock) ok = m.update(clock);
        BeginDrawing();
        ClearBackground(BLACK);
        if (m.hasFrame) {
            const float sw = float(GetScreenWidth()), sh = float(GetScreenHeight()), a = m.aspect;
            const float k = std::min(sw / a, sh);
            DrawTexturePro(m.tex, Rectangle{0, 0, float(m.w), float(m.h)}, Rectangle{(sw - k * a) * 0.5f, (sh - k) * 0.5f, k * a, k}, Vector2{0, 0}, 0.f, WHITE);
        }
        if (!fixedClock) DrawText(TextFormat("%.1f s  %dx%d  aspect %.3f", clock, m.w, m.h, m.aspect), 10, 10, 20, YELLOW);
        EndDrawing();
        ++draws;
        // writes the decoded pixels (TakeScreenshot on a hidden window can read the wrong buffer after the swap)
        const bool hit = frameN >= 0 ? m.shownFrames() == frameN + 1 : clock >= at;
        if (!shot.empty() && !shotDone && hit && m.hasFrame) {
            const wotm::fmv::Frame &f = m.current();
            Image img{const_cast<uint32_t *>(f.rgba.data()), f.w, f.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            ExportImage(img, shot.c_str());
            shotDone = true;
            if (wav.empty()) break;
        }
        if (!ok) break;
    }
    std::fprintf(stderr, "%s: %dx%d aspect %.3f, %.2f s, %ld draws, audio %zu samples (%.2f s)%s\n", argv[1], m.w, m.h, m.aspect, clock, draws,
                 tap.size() / 2, tap.size() / 2 / 48000.0, ok ? "" : ", end of movie");
    if (!wav.empty()) writeWav(wav, tap, 48000);
    m.close();
    if (!fixedClock && !mute) CloseAudioDevice();
    CloseWindow();
    return 0;
}
