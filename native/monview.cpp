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
    const char *shot = nullptr, *dumpTex = nullptr;
    bool noAbe = false;
    for (int i = 3; i + 1 < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--anim") anim = std::atoi(argv[++i]);
        else if (a == "--frame") frame = float(std::atof(argv[++i]));
        else if (a == "--yaw") yaw = float(std::atof(argv[++i]));
        else if (a == "--pitch") pitch = float(std::atof(argv[++i]));
        else if (a == "--size") size = std::atoi(argv[++i]);
        else if (a == "--zoom") zoom = float(std::atof(argv[++i]));
        else if (a == "--shot") shot = argv[++i];
        else if (a == "--dumptex") dumpTex = argv[++i];
        else if (a == "--noabe") noAbe = true;
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
    if (dumpTex) {   // texturas decodificadas (PNG) e o alfa de cada uma, e os estados de desenho: para conferir trajes
        std::map<std::string, int> st;
        const auto &vs = m.verts();
        for (size_t k = 0; k + 2 < vs.size(); k += 3)
            ++st[TextFormat("tex %d abe %d blend %02x aref %d zmsk %d cull %d ztst %d", vs[k].tex, vs[k].abe, vs[k].blend, vs[k].aref, vs[k].zmsk, vs[k].cull, vs[k].ztst)];
        for (const auto &e : st) std::fprintf(stderr, "  %s: %d tris\n", e.first.c_str(), e.second);
        float blo[2][3] = {{1e30f, 1e30f, 1e30f}, {1e30f, 1e30f, 1e30f}}, bhi[2][3] = {{-1e30f, -1e30f, -1e30f}, {-1e30f, -1e30f, -1e30f}};
        int na[2] = {0, 0}, nalpha0 = 0;
        for (const auto &v : vs) {
            const int g = v.abe ? 1 : 0;
            const float q[3] = {v.x, v.y, v.z};
            for (int c = 0; c < 3; ++c) { blo[g][c] = std::min(blo[g][c], q[c]); bhi[g][c] = std::max(bhi[g][c], q[c]); }
            ++na[g];
            if (g && v.a == 0) ++nalpha0;
        }
        for (int g = 0; g < 2; ++g)
            std::fprintf(stderr, "  caixa %s: (%.0f %.0f %.0f)-(%.0f %.0f %.0f), %d vertices\n", g ? "com ABE" : "opacos", blo[g][0], blo[g][1],
                         blo[g][2], bhi[g][0], bhi[g][1], bhi[g][2], na[g]);
        std::fprintf(stderr, "  vertices com ABE e alfa 0: %d\n", nalpha0);
        for (size_t t = 0; t < m.textures().size(); ++t) {
            const auto &ti = m.textures()[t];
            int hist[3] = {0, 0, 0};
            for (uint32_t c : ti.rgba) ++hist[(c >> 24) == 0 ? 0 : (c >> 24) == 255 ? 2 : 1];
            std::fprintf(stderr, "tex %zu %dx%d alfa 0: %d, parcial: %d, 255: %d\n", t, ti.w, ti.h, hist[0], hist[1], hist[2]);
            Image img{const_cast<uint32_t *>(ti.rgba.data()), ti.w, ti.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            ExportImage(img, TextFormat("%s/%s_tex%zu.png", dumpTex, name.c_str(), t));
        }
    }
    SetTraceLogLevel(LOG_WARNING);
    if (shot) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(shot ? 64 : size, shot ? 64 : size, "monview");
    wotm::model::Renderer r;
    r.skipAbe = noAbe;
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
