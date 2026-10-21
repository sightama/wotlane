// Visualizador de monstro direto do disco (native/wotm_model.hpp): pose de uma animacao, textura do .TEX/.RTX, sem o jogo.
//
//   monview <disco> MONKEY0 [--anim 90] [--frame 0] [--yaw 35] [--pitch 8] [--size 512] [--shot saida.png]
// Com --shot grava um PNG (fundo transparente) e sai; sem, abre uma janela com a animacao em laco (setas giram, +/- zoom,
// PageUp/PageDown trocam a animacao, Espaco pausa).
#include <cstdlib>
#include <string>

#include "wotm_model.hpp"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "uso: monview <disco> MONKEY0 [--anim 90] [--frame 0] [--yaw 35] [--pitch 8] [--size 512] [--shot saida.png]\n");
        return 1;
    }
    const std::string disc = argv[1], name = argv[2];
    int anim = wotm::model::kIdle, size = 512;
    float frame = 0.f, yaw = 35.f, pitch = 8.f, zoom = 1.f;
    const char *shot = nullptr;
    for (int i = 3; i + 1 < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--anim") anim = std::atoi(argv[++i]);
        else if (a == "--frame") frame = float(std::atof(argv[++i]));
        else if (a == "--yaw") yaw = float(std::atof(argv[++i]));
        else if (a == "--pitch") pitch = float(std::atof(argv[++i]));
        else if (a == "--size") size = std::atoi(argv[++i]);
        else if (a == "--zoom") zoom = float(std::atof(argv[++i]));
        else if (a == "--shot") shot = argv[++i];
    }
    wotm::model::Model m;
    if (!m.load(disc, {}, name)) {
        std::fprintf(stderr, "nao consegui ler MON/%s.NGP em %s\n", name.c_str(), disc.c_str());
        return 1;
    }
    if (!m.pose(anim, frame)) {
        std::fprintf(stderr, "%s: sem animacao %d; pose de base\n", name.c_str(), anim);
        m.pose(-1, 0.f);
    }
    std::fprintf(stderr, "%s: %zu triangulos, %zu texturas, animacao %d com %.0f quadros\n", name.c_str(), m.verts().size() / 3,
                 m.textures().size(), anim, m.animLength(anim));
    SetTraceLogLevel(LOG_WARNING);
    if (shot) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(shot ? 64 : size, shot ? 64 : size, "monview");
    wotm::model::Renderer r;
    m.computeFraming(anim);
    m.pose(anim, frame);
    const auto bg = [] { DrawRectangle(0, 0, 4096, 4096, Color{40, 44, 52, 255}); };
    if (shot) {
        const RenderTexture2D &rt = r.render(m, size, size, yaw, pitch, zoom, bg);
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        const bool ok = ExportImage(img, shot);
        UnloadImage(img);
        r.release();
        CloseWindow();
        return ok ? 0 : 1;
    }
    SetTargetFPS(60);
    bool paused = false;
    double t0 = GetTime(), tp = 0.0;
    while (!WindowShouldClose()) {
        if (IsKeyDown(KEY_LEFT)) yaw -= 2.f;
        if (IsKeyDown(KEY_RIGHT)) yaw += 2.f;
        if (IsKeyDown(KEY_UP)) pitch += 1.f;
        if (IsKeyDown(KEY_DOWN)) pitch -= 1.f;
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) zoom *= 1.02f;
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) zoom /= 1.02f;
        if (IsKeyPressed(KEY_SPACE)) { paused = !paused; if (!paused) t0 = GetTime() - tp; }
        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressed(KEY_PAGE_DOWN)) {
            const int step = IsKeyPressed(KEY_PAGE_UP) ? 1 : -1;
            for (int k = 0; k < 300; ++k) {
                anim = (anim + step + 300) % 300;
                if (m.animLength(anim) > 0.f) break;
            }
            t0 = GetTime();
            m.computeFraming(anim);
            std::fprintf(stderr, "animacao %d (%.0f quadros)\n", anim, m.animLength(anim));
        }
        if (!paused) tp = GetTime() - t0;
        m.pose(anim, frame + float(tp * 60.0));
        const RenderTexture2D &rt = r.render(m, size, size, yaw, pitch, zoom, bg);
        BeginDrawing();
        ClearBackground(Color{40, 44, 52, 255});
        DrawTexturePro(rt.texture, Rectangle{0, 0, float(size), -float(size)}, Rectangle{0, 0, float(GetScreenWidth()), float(GetScreenHeight())},
                       Vector2{0, 0}, 0.f, WHITE);
        DrawText(TextFormat("%s  anim %d  yaw %.0f", name.c_str(), anim, yaw), 8, 8, 20, RAYWHITE);
        EndDrawing();
    }
    r.release();
    CloseWindow();
    return 0;
}
