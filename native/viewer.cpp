// Visualizador standalone da camada nativa: carrega um dump da RAM do EE (`ram_<quadro>.bin`, gerado pelo runtime com
// PS2X_DUMP_RAM) e desenha a cena com raylib, com camera livre. Serve para iterar sem o emulador.
//
//   viewer ram.bin [--vram vram.bin] [--shot saida.png] [--cam x y z yaw pitch] [--lod n] [--wire]
// Recursos: por padrao le LVL/MON de `disc/` (--disc <dir>) conforme fileStatus; --rtx <arquivo> forca um RTX de nivel.
// Sem --vram, procura vram_<n>.bin ao lado de ram_<n>.bin.
// Teclas: botao direito + mouse = olhar; WASD = mover (Shift = rapido); Q/E = descer/subir; Tab = arame; L = LOD; F12 = captura.
#include <cstdlib>
#include <unordered_map>
#include <string>

#include "wotm_native.hpp"

static std::vector<uint8_t> readFile(const char *path) {
    std::vector<uint8_t> v;
    if (FILE *f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        v.resize(size_t(n));
        if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
        std::fclose(f);
    }
    return v;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "uso: viewer ram.bin [--shot saida.png] [--cam x y z yaw pitch] [--lod n] [--wire]\n");
        return 1;
    }
    std::vector<uint8_t> data = readFile(argv[1]);
    if (data.size() < 0x1000000) {
        std::fprintf(stderr, "dump invalido: %s (%zu bytes)\n", argv[1], data.size());
        return 1;
    }
    const char *shot = nullptr;
    std::string vramPath;
    const char *texdump = nullptr;
    std::string rtxPath, discRoot = "disc";
    bool wire = false, haveCam = false;
    int lod = -1;   // -1 = automatico por distancia
    bool useFree = false;
    wotm::Camera cam;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--wire") wire = true;
        else if (a == "--free") useFree = true;
        else if (a == "--rtx" && i + 1 < argc) rtxPath = argv[++i];
        else if (a == "--disc" && i + 1 < argc) discRoot = argv[++i];
        else if (a == "--texdump" && i + 1 < argc) texdump = argv[++i];
        else if (a == "--vram" && i + 1 < argc) vramPath = argv[++i];
        else if (a == "--lod" && i + 1 < argc) lod = std::atoi(argv[++i]);
        else if (a == "--cam" && i + 5 < argc) {
            for (int k = 0; k < 3; ++k) cam.pos[k] = float(std::atof(argv[++i]));
            cam.yaw = float(std::atof(argv[++i])) * 3.14159265f / 180.f;
            cam.pitch = float(std::atof(argv[++i])) * 3.14159265f / 180.f;
            haveCam = true;
        }
    }
    if (vramPath.empty()) {   // ram_123.bin -> vram_123.bin
        std::string r = argv[1];
        const size_t k = r.rfind("ram_");
        if (k != std::string::npos) vramPath = r.substr(0, k) + "v" + r.substr(k);
    }
    std::vector<uint8_t> vram = vramPath.empty() ? std::vector<uint8_t>() : readFile(vramPath.c_str());
    std::fprintf(stderr, "vram: %s (%zu bytes)\n", vramPath.c_str(), vram.size());
    wotm::Ram ram{data.data(), data.size()};
    wotm::Scene scene;
    scene.vram = {vram.data(), vram.size()};
    if (rtxPath.empty()) {   // padrao: nivel + monstros do jogo, achados por fileStatus (nomes e bases das paletas) no disco extraido
        std::string log;
        const bool ok = scene.loadResources(ram, discRoot, &log);
        std::fprintf(stderr, "recursos (%s): %s -> %s (%zu paletas)\n", discRoot.c_str(), log.c_str(), ok ? "ok" : "falhou", scene.rtxPal.size());
    } else {
        const bool ok = scene.loadRtx(rtxPath.c_str(), ram.u32(wotm::addr::tempVramTexAddr) >> 6);
        std::fprintf(stderr, "rtx %s: %s (%zu paletas)\n", rtxPath.c_str(), ok ? "ok" : "falhou", scene.rtxPal.size());
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1024, 768, "WotM nativo");
    SetTargetFPS(60);

    scene.collect(ram, lod);
    std::fprintf(stderr, "itens=%zu visitados=%u\n", scene.items.size(), scene.visited);
    if (std::getenv("WOTM_UVSTAT")) scene.dumpUv(ram);
    if (std::getenv("WOTM_UVISO")) scene.dumpUvIso(ram);
    if (std::getenv("WOTM_SKINSAN")) scene.dumpSkinSanity(ram);
    if (const char *cv = std::getenv("WOTM_COVER")) {   // "ref,ref,..:teste,teste,.."
        std::vector<uint32_t> refs, tests; std::vector<uint32_t> *cur = &refs; unsigned v = 0; bool has = false;
        for (const char *q = cv;; ++q) {
            const char ch = *q;
            if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')) { v = v * 16 + unsigned(ch <= '9' ? ch - '0' : ch - 'a' + 10); has = true; }
            else { if (has) cur->push_back(v); v = 0; has = false; if (ch == ':') cur = &tests; if (!ch) break; }
        }
        scene.coverage(ram, refs, tests, std::getenv("WOTM_COVERTOL") ? float(std::atof(std::getenv("WOTM_COVERTOL"))) : 6.f);
    }
    if (const char *bc = std::getenv("WOTM_BONECODES")) { unsigned n = 0; if (std::sscanf(bc, "%x", &n) == 1) scene.boneCodes(ram, n); }
    if (const char *sf = std::getenv("WOTM_SKINFIT")) { unsigned a = 0, b = 0, c = 0; if (std::sscanf(sf, "%x,%x,%x", &a, &b, &c) == 3) scene.fitSkin(ram, a, b, c); }
    scene.dumpSkinned(ram);
    if (!haveCam) {   // enquadra a cena (ignora o domo do ceu)
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (const auto &it : scene.items) {
            const float r = ram.f32(it.node + 8);
            if (!(r < 2000.f * 2000.f)) continue;
            for (int c = 0; c < 3; ++c) {
                lo[c] = std::min(lo[c], it.m.m[3][c]);
                hi[c] = std::max(hi[c], it.m.m[3][c]);
            }
        }
        if (lo[0] < hi[0]) {
            cam.pos[0] = (lo[0] + hi[0]) * 0.5f;
            cam.pos[1] = lo[1] - (hi[1] - lo[1]) * 0.6f;
            cam.pos[2] = hi[2] + (hi[1] - lo[1]) * 0.5f;
            cam.yaw = 3.14159265f * 0.5f;
            cam.pitch = -0.6f;
        }
    }

    if (texdump) {   // depuracao: grava as texturas decodificadas (TEX0 distintos) em <texdump>_<n>.png
        std::unordered_map<uint64_t, int> done;
        for (const auto &it : scene.items) for (uint32_t slot = 0; slot < std::min<uint32_t>(std::max<uint32_t>(1, ram.u16(it.node + 0x5A)), 5); ++slot) {
            const uint64_t key = wotm::Scene::texKey(ram, it.node, slot);
            if (!key || done.count(key) || done.size() >= 400) continue;
            std::vector<uint32_t> px;
            uint32_t w = 0, h = 0;
            const bool ok = scene.decodeTex(ram, it.node, px, w, h, slot);
            const auto t = wotm::gs::Tex0::decode(wotm::Scene::objectTex0(ram, it.node, slot));
            std::fprintf(stderr, "texId=%u psm=%02x %ux%u tw=%u th=%u tbp0=%u tbw=%u cbp=%u cpsm=%u csa=%u -> %s\n", unsigned(key >> 40) & 0xFFFF, t.psm, w, h, 1u << t.tw, 1u << t.th, t.tbp0, t.tbw, t.cbp, t.cpsm, t.csa, ok ? "ok" : "falhou");
            if (ok) {
                Image img{px.data(), int(w), int(h), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                ExportImage(img, TextFormat("%s_%02d.png", texdump, int(done.size())));
            }
            done[key] = 1;
        }
    }
    if (const char *wt = std::getenv("WOTM_NODETEX")) {   // depuracao: lista os objetos cujo texId[0] e <wt> (texIds, numTextures e TEX0 de cada contexto)
        const uint32_t want = uint32_t(std::atoi(wt));
        int shown = 0;
        for (const auto &it : scene.items) {
            if (ram.u16(it.node + 0x50) != want || shown++ >= 8) continue;
            std::fprintf(stderr, "pos(%.0f %.0f %.0f) ", it.m.m[3][0], it.m.m[3][1], it.m.m[3][2]);
            std::fprintf(stderr, "bits=%08x envW=%.2f ", ram.u32(it.node + 0xC), ram.f32(it.node + 0x5C));
            std::fprintf(stderr, "no %06x texId=[%u %u %u %u %u] numTex=%u tex0:", it.node, ram.u16(it.node + 0x50), ram.u16(it.node + 0x52), ram.u16(it.node + 0x54), ram.u16(it.node + 0x56), ram.u16(it.node + 0x58), ram.u16(it.node + 0x5A));
            for (uint32_t k = 0; k < std::min<uint32_t>(ram.u16(it.node + 0x5A), 5); ++k) {
                const uint64_t v = wotm::Scene::objectTex0(ram, it.node, k);
                const auto t = wotm::gs::Tex0::decode(v);
                std::fprintf(stderr, " [psm=%02x tbp0=%u cbp=%u tw=%u th=%u]", t.psm, t.tbp0, t.cbp, 1u << t.tw, 1u << t.th);
            }
            if (const auto *md = scene.mesh(ram, it.node)) {
                uint32_t per[8] = {};
                for (const auto &v : md->tris) ++per[v.slot & 7];
                std::fprintf(stderr, "  slotMask=%x verts/slot: %u %u %u %u %u", md->slotMask, per[0], per[1], per[2], per[3], per[4]);
                for (uint32_t sl = 0; sl < 2; ++sl) {
                    float u0 = 1e9f, u1 = -1e9f, v0 = 1e9f, v1 = -1e9f;
                    for (const auto &v : md->tris) if (v.slot == sl) { u0 = std::min(u0, v.u); u1 = std::max(u1, v.u); v0 = std::min(v0, v.v); v1 = std::max(v1, v.v); }
                    std::fprintf(stderr, " | slot%u uv u[%.2f %.2f] v[%.2f %.2f]", sl, u0, u1, v0, v1);
                }
            }
            std::fprintf(stderr, "\n");
        }
    }
    if (std::getenv("WOTM_T4SMOOTH")) {   // depuracao: para cada pacote T4 de texInfo, suavidade (diferenca media entre vizinhos) linear x como memoria do GS
        std::vector<uint32_t> swz;
        uint32_t nlin = 0;
        for (uint32_t id = 0; id < 400; ++id) {
            const uint32_t ptr = ram.u32(wotm::addr::texInfo + 16 * id) & 0x0FFFFFFF;
            const wotm::UploadHdr hd = wotm::UploadHdr::read(ram, ptr);
            if (!ram.ok(ptr + 0x80) || hd.psm != wotm::gs::T4) continue;
            const uint32_t w = hd.w, h = hd.h, bw = std::max<uint32_t>(1, hd.bw);
            if (w < 16 || h < 16 || w > 256 || h > 256 || !ram.ok(ptr + 0x80 + w * h / 2)) continue;
            double lin = 0, sw = 0;
            uint32_t n = 0;
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x + 1 < w; ++x) {
                    auto nib = [&](uint32_t idx) { const uint8_t b = ram.u8(ptr + 0x80 + idx / 2); return int((idx & 1) ? (b >> 4) : (b & 15)); };
                    lin += std::abs(nib(y * w + x) - nib(y * w + x + 1));
                    sw += std::abs(nib(wotm::gs::nibT4(0, bw, x, y)) - nib(wotm::gs::nibT4(0, bw, x + 1, y)));
                    ++n;
                }
            lin /= n; sw /= n;
            if (sw < lin * 0.6) swz.push_back(id); else ++nlin;
        }
        std::fprintf(stderr, "T4: %u linear(es); %zu mais suaves como memoria do GS:", nlin, swz.size());
        for (uint32_t id : swz) std::fprintf(stderr, " %u", id);
        std::fprintf(stderr, "\n");
    }
    if (const char *pc = std::getenv("WOTM_PALCMP")) {   // depuracao: paleta do CBP <pc> no .RTX carregado x na VRAM do dump (16 primeiras cores, RGBA)
        const uint32_t cbp = uint32_t(std::atoi(pc));
        const auto it = scene.rtxPal.find(cbp);
        std::fprintf(stderr, "paleta CBP %u: RTX %s\n", cbp, it == scene.rtxPal.end() ? "ausente" : "presente");
        if (it != scene.rtxPal.end()) {
            std::fprintf(stderr, "  RTX (%zu entradas):", it->second.size());
            for (size_t k = 0; k < std::min<size_t>(16, it->second.size()); ++k) std::fprintf(stderr, " %08x", it->second[k]);
            std::fprintf(stderr, "\n");
        }
        if (scene.vram.ok()) {
            wotm::gs::Tex0 t{};
            t.psm = wotm::gs::T4; t.cbp = cbp; t.cpsm = 0; t.csm = 0; t.csa = 0;
            uint32_t pv[256] = {};
            wotm::gs::readPalette(scene.vram, t, pv);
            std::fprintf(stderr, "  VRAM:");
            for (int k = 0; k < 16; ++k) std::fprintf(stderr, " %08x", pv[k]);
            std::fprintf(stderr, "\n");
        }
    }
    if (const char *fxd = std::getenv("WOTM_FXDUMP")) {   // depuracao: texturas dos efeitos (fx[] em 0x5BA280, 98 x 12000 bytes) decodificadas da VRAM do dump
        std::unordered_map<uint64_t, int> done;
        for (uint32_t i = 1; i < wotm::fxaddr::count; ++i) {
            const uint32_t b = 0x005BA280u + i * 12000u;
            const uint32_t np = ram.u32(b + 0x2ED0);
            const uint32_t t0p = ram.u32(b + 0x2EBC);
            if (!np || np > 4096 || !ram.ok(t0p, 8)) continue;
            const uint64_t t0 = uint64_t(ram.u32(t0p)) | (uint64_t(ram.u32(t0p + 4)) << 32);
            const auto t = wotm::gs::Tex0::decode(t0);
            std::vector<uint32_t> px;
            uint32_t w = 0, h = 0;
            const uint32_t texId = ram.u16(b + 0x2C40);   // texLoad[0]: indice em texInfo
            const bool ok = scene.decodeFxTex(ram, texId, t0, px, w, h);
            std::fprintf(stderr, "fx%u vis=%u fxTex=%u texInfo=%u n=%u psm=%02x %ux%u tbp0=%u cbp=%u -> %s\n", i, ram.u32(b + 0x2E90) & 0xFF, ram.u32(b + 0x2E98), texId, np, t.psm, w, h, t.tbp0, t.cbp, ok ? "ok" : "falhou");
            if (ok && !done.count(t0)) {
                done[t0] = 1;
                Image img{px.data(), int(w), int(h), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                ExportImage(img, TextFormat("%s_fx%02u.png", fxd, i));
            }
        }
    }
    const wotm::GameCamera gcam = wotm::readGameCamera(ram, 0);
    bool useGame = gcam.ok && !useFree && !haveCam;
    std::fprintf(stderr, "camera do jogo: %s pos %.1f %.1f %.1f fovy %.1f\n", gcam.ok ? "ok" : "ausente", gcam.pos[0], gcam.pos[1], gcam.pos[2], gcam.fovy);
    int frame = 0;
    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            const Vector2 d = GetMouseDelta();
            cam.yaw -= d.x * 0.003f;
            cam.pitch = std::max(-1.5f, std::min(1.5f, cam.pitch - d.y * 0.003f));
        }
        const float fx = std::cos(cam.pitch) * std::cos(cam.yaw), fy = std::cos(cam.pitch) * std::sin(cam.yaw), fz = std::sin(cam.pitch);
        const float rx = std::sin(cam.yaw), ry = -std::cos(cam.yaw);
        const float sp = (IsKeyDown(KEY_LEFT_SHIFT) ? 600.f : 80.f) * dt;
        if (IsKeyDown(KEY_W)) { cam.pos[0] += fx * sp; cam.pos[1] += fy * sp; cam.pos[2] += fz * sp; }
        if (IsKeyDown(KEY_S)) { cam.pos[0] -= fx * sp; cam.pos[1] -= fy * sp; cam.pos[2] -= fz * sp; }
        if (IsKeyDown(KEY_D)) { cam.pos[0] += rx * sp; cam.pos[1] += ry * sp; }
        if (IsKeyDown(KEY_A)) { cam.pos[0] -= rx * sp; cam.pos[1] -= ry * sp; }
        if (IsKeyDown(KEY_E)) cam.pos[2] += sp;
        if (IsKeyDown(KEY_Q)) cam.pos[2] -= sp;
        if (IsKeyPressed(KEY_TAB)) wire = !wire;
        if (IsKeyPressed(KEY_L)) { lod = lod >= 3 ? -1 : lod + 1; scene.collect(ram, lod); }

        if (IsKeyPressed(KEY_C)) useGame = !useGame;
        Camera3D c3{};
        if (useGame && gcam.ok) {
            c3.position = {gcam.pos[0], gcam.pos[1], gcam.pos[2]};
            c3.target = {gcam.pos[0] + gcam.fwd[0], gcam.pos[1] + gcam.fwd[1], gcam.pos[2] + gcam.fwd[2]};
            c3.up = {gcam.up[0], gcam.up[1], gcam.up[2]};
            c3.fovy = gcam.fovy;
        } else {
            c3.position = {cam.pos[0], cam.pos[1], cam.pos[2]};
            c3.target = {cam.pos[0] + fx, cam.pos[1] + fy, cam.pos[2] + fz};
            c3.up = {0, 0, 1};
            c3.fovy = 60.f;
        }
        c3.projection = CAMERA_PERSPECTIVE;

        BeginDrawing();
        ClearBackground(Color{40, 60, 90, 255});
        rlSetClipPlanes(0.5, 30000.0);
        BeginMode3D(c3);
        scene.draw(ram, wire);
        EndMode3D();
        scene.updateText(ram);
        scene.drawText(ram, float(GetScreenWidth()), float(GetScreenHeight()));
        DrawText(TextFormat("itens %zu  lod %d  %s  cam %.0f %.0f %.0f", scene.items.size(), lod, wire ? "arame" : "solido", cam.pos[0], cam.pos[1], cam.pos[2]), 10, 10, 18, WHITE);
        EndDrawing();
        ++frame;
        if (IsKeyPressed(KEY_F12)) TakeScreenshot("viewer_shot.png");
        if (shot && frame == 3) {
            TakeScreenshot(shot);
            break;
        }
    }
    CloseWindow();
    return 0;
}
