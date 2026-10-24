// Modo treino, lado do desenho (incluido pelo wotm_live.inc): volumes do corpo lidos na thread do EE (capture, chamada pelo
// wotmCaptureFrame), esferas dos golpes e dano vindos dos ganchos (native/runtime/wotm_training.inc), desenho por cima da cena (sem
// teste de profundidade), painel de dano e menu F2. Estado compartilhado em native/wotm_training.h.

#include "wotm_training.h"

namespace wotm_trainview
{
using namespace wotm_training;
constexpr uint32_t kSlots = 0x120380u;

// ---- cascos de colisao do corpo (nos op 10 do hier do monstro): planos {n, d} no espaco do no, dentro = n.p <= d

void hullSegments(const wotm::Ram &ram, uint32_t node, const wotm::M4 &m, std::vector<float> &seg)
{
    const int np = int16_t(ram.u16(node + 4));
    if (np < 4 || np > 24 || !ram.ok(node + 0x20, 16u * uint32_t(np)))
        return;
    float n[24][3], d[24];
    for (int i = 0; i < np; ++i)
    {
        for (int k = 0; k < 3; ++k)
            n[i][k] = ram.f32(node + 0x20 + 16 * i + 4 * k);
        d[i] = ram.f32(node + 0x20 + 16 * i + 12);
    }
    for (int sign = 1; sign >= -1; sign -= 2)   // se nenhum vertice fica dentro com n.p <= d, tenta a outra convencao
    {
        struct V { float p[3]; uint32_t tight; };
        std::vector<V> vs;
        for (int i = 0; i < np; ++i)
            for (int j = i + 1; j < np; ++j)
                for (int k = j + 1; k < np; ++k)
                {
                    const float *a = n[i], *b = n[j], *c = n[k];
                    const float bc[3] = {b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0]};
                    const float det = a[0] * bc[0] + a[1] * bc[1] + a[2] * bc[2];
                    if (std::fabs(det) < 1e-6f)
                        continue;
                    const float ca[3] = {c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0]};
                    const float ab[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
                    V v{};
                    for (int q = 0; q < 3; ++q)
                        v.p[q] = (d[i] * bc[q] + d[j] * ca[q] + d[k] * ab[q]) / det;
                    bool in = std::isfinite(v.p[0] + v.p[1] + v.p[2]);
                    for (int t = 0; t < np && in; ++t)
                    {
                        const float s = n[t][0] * v.p[0] + n[t][1] * v.p[1] + n[t][2] * v.p[2] - d[t];
                        if (sign * s > 0.05f)
                            in = false;
                        else if (std::fabs(s) <= 0.05f)
                            v.tight |= 1u << t;
                    }
                    if (!in)
                        continue;
                    bool dup = false;
                    for (V &o : vs)
                        if (std::fabs(o.p[0] - v.p[0]) + std::fabs(o.p[1] - v.p[1]) + std::fabs(o.p[2] - v.p[2]) < 0.05f)
                        {
                            o.tight |= v.tight;
                            dup = true;
                            break;
                        }
                    if (!dup && vs.size() < 64)
                        vs.push_back(v);
                }
        if (vs.size() < 4)
            continue;
        for (size_t i = 0; i < vs.size(); ++i)
            for (size_t j = i + 1; j < vs.size(); ++j)
            {
                const uint32_t both = vs[i].tight & vs[j].tight;
                if ((both & (both - 1)) == 0)   // menos de dois planos em comum: nao e aresta
                    continue;
                float a[3], b[3];
                wotm::xform(m, vs[i].p[0], vs[i].p[1], vs[i].p[2], a);
                wotm::xform(m, vs[j].p[0], vs[j].p[1], vs[j].p[2], b);
                seg.insert(seg.end(), {a[0], a[1], a[2], b[0], b[1], b[2]});
            }
        return;
    }
}

// Percurso so do que leva aos cascos (o mesmo caminho de matrizes do Scene::walk, sem LOD por distancia: o filho 0)
void hullWalk(const wotm::Ram &ram, uint32_t node, const wotm::M4 &m, uint32_t atMat, int depth, int &budget, std::vector<float> &seg)
{
    using namespace wotm;
    if (depth > 48 || --budget < 0 || !ram.ok(node, 0x10))
        return;
    auto kids = [&](uint32_t arr, uint32_t n, const M4 &mm, uint32_t at) {
        n = std::min<uint32_t>(n, 256);
        for (uint32_t i = 0; i < n; ++i)
            if (const uint32_t c = ram.u32(arr + 4 * i); ram.ok(c))
                hullWalk(ram, c, mm, at, depth + 1, budget, seg);
    };
    switch (ram.u32(node) & 0x3F)
    {
    case 10:   // COLLISION_VOLUME
        hullSegments(ram, node, m, seg);
        break;
    case 1:    // GROUP: visual (bit 0) e colisao (bit 1)
        if (ram.u16(node + 0xA) & 3)
            kids(node + 0x20, ram.u16(node + 8), m, atMat);
        break;
    case 2:    // LOD: o mais detalhado
        if (ram.u32(node + 4) > 0 && ram.u32(node + 4) < 16)
            if (const uint32_t c = ram.u32(node + 0x28); ram.ok(c))
                hullWalk(ram, c, m, atMat, depth + 1, budget, seg);
        break;
    case 3:
        kids(node + 0x1C, ram.u32(node + 8), mul(translateM(ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18)), m), atMat);
        break;
    case 4:
        kids(node + 0x50, ram.u32(node + 8), mul(readM4(ram, node + 0x10), m), atMat);
        break;
    case 30:
        kids(node + 0x2C, ram.u32(node + 8),
             mul(mul(scaleM(ram.f32(node + 0x20), ram.f32(node + 0x24), ram.f32(node + 0x28)),
                     translateM(ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18))), m), atMat);
        break;
    case 6:
        if (const uint32_t w = ram.u8(node + 8); w < ram.u8(node + 0xB))
            if (const uint32_t c = ram.u32(node + 0xC + 4 * w); ram.ok(c))
                hullWalk(ram, c, m, atMat, depth + 1, budget, seg);
        break;
    case 23:
        if (const uint32_t c = ram.u32(node + 4); ram.ok(c))
            hullWalk(ram, c, m, atMat, depth + 1, budget, seg);
        break;
    case 35:
        kids(node + 0xC, ram.u32(node + 4), m, atMat);
        break;
    case 25:   // CHAR_INSTANCE: matrizes da animacao
        kids(node + 0x2C, ram.u32(node + 0x28), m, ram.u32(node + 0xC));
        break;
    case 17: { // ANIM_XFORM
        if (ram.f32(node + 0xC) == 0.f)
            break;
        M4 local = ident();
        const uint32_t idx = ram.u32(node + 0xC4);
        if (atMat && idx < 4096 && ram.ok(atMat + idx * 64, 64))
            local = readM4(ram, atMat + idx * 64);
        local.m[0][3] = local.m[1][3] = local.m[2][3] = 0.f;
        local.m[3][3] = 1.f;
        kids(node + 0xCC, ram.u32(node + 4), mul(local, m), atMat);
        break;
    }
    case 38: { // ANIM_SCALE
        const float sx = ram.f32(node + 0x20), sy = ram.f32(node + 0x24), sz = ram.f32(node + 0x28);
        if (sx == 0.f && sy == 0.f && sz == 0.f)
            break;
        const M4 pre = translateM(ram.f32(node + 0x40), ram.f32(node + 0x44), ram.f32(node + 0x48));
        const M4 post = translateM(ram.f32(node + 0x30) + ram.f32(node + 0x50), ram.f32(node + 0x34) + ram.f32(node + 0x54),
                                   ram.f32(node + 0x38) + ram.f32(node + 0x58));
        kids(node + 0x60, ram.u32(node + 4), mul(mul(mul(pre, scaleM(sx, sy, sz)), post), m), atMat);
        break;
    }
    default:
        break;
    }
}

// Na thread do EE (wotmCaptureFrame): cascos, capsulas e vida de cada monstro deste quadro
void capture(const wotm::Ram &ram)
{
    if (!g_on.load(std::memory_order_relaxed))
        return;
    const uint32_t opts = g_opts.load(std::memory_order_relaxed);
    std::vector<Hull> hulls;
    std::vector<Capsule> caps;
    float health[16] = {}, maxHealth[16] = {}, stamina[16] = {}, staminaMax[16] = {};
    for (uint32_t i = 0; i < 16; ++i)
    {
        const uint32_t mon = ram.u32(wotm::sym::shellGame + kSlots + 4 * i) & 0x1FFFFFFFu;
        if (!mon || !ram.ok(mon, 0x450))
            continue;
        health[i] = ram.f32(mon + 0x44C);
        maxHealth[i] = ram.f32(mon + 0x448);
        stamina[i] = ram.f32(mon + 0x470);      // StaminaMeter (+0x460): cur
        staminaMax[i] = ram.f32(mon + 0x460);   // max (a furia passa dele ate o maxLevel, +0x468)
        const uint32_t cs = ram.u32(mon + 0xC) & 0x1FFFFFFFu;
        if (!ram.ok(cs, 0xB0) || !ram.u8(cs + 0xC))   // drawMe: monstro escondido (fora da luta, desligado)
            continue;
        wotm::M4 m = wotm::readM4(ram, cs + 0x20);
        m.m[3][0] = ram.f32(cs + 0x10);
        m.m[3][1] = ram.f32(cs + 0x14);
        m.m[3][2] = ram.f32(cs + 0x18);
        m.m[0][3] = m.m[1][3] = m.m[2][3] = 0.f;
        m.m[3][3] = 1.f;
        if (opts & kShowCapsule)
        {
            // capsula do collisResolveCsToCsCollisions: ponto +0x3F0 no espaco do cs, eixo = z do cs, comprimento +0x400, raio +0x404
            Capsule c{};
            c.owner = int8_t(i);
            wotm::xform(m, ram.f32(mon + 0x3F0), ram.f32(mon + 0x3F4), ram.f32(mon + 0x3F8), c.a);
            const float len = ram.f32(mon + 0x400);
            for (int k = 0; k < 3; ++k)
                c.b[k] = c.a[k] + m.m[2][k] * len;
            c.r = ram.f32(mon + 0x404);
            if (std::isfinite(c.r + len) && c.r > 0.f && c.r < 2000.f)
                caps.push_back(c);
        }
        if (opts & kShowHulls)
        {
            if (ram.u8(cs + 0xF))   // scaleMe
                m = wotm::mul(wotm::scaleM(ram.f32(cs + 0x80), ram.f32(cs + 0x84), ram.f32(cs + 0x88)), m);
            Hull h{};
            h.owner = int8_t(i);
            int budget = 4000;
            const uint32_t ep = ram.u32(cs) & 0x1FFFFFFFu;
            if (ram.ok(ep))
                hullWalk(ram, ep, m, 0, 0, budget, h.seg);
            if (!h.seg.empty())
                hulls.push_back(std::move(h));
        }
    }
    int32_t players = static_cast<int32_t>(ram.u32(wotm::sym::shellInfo + 0x2BB4));
    if (players < 1 || players > 4)
        players = 1;
    Shared &sh = shared();
    std::lock_guard<std::mutex> lk(sh.m);
    sh.hulls = std::move(hulls);
    sh.caps = std::move(caps);
    sh.frame = ram.u32(wotm::sym::g_frame);
    std::memcpy(sh.health, health, sizeof health);
    std::memcpy(sh.maxHealth, maxHealth, sizeof maxHealth);
    std::memcpy(sh.stamina, stamina, sizeof stamina);
    std::memcpy(sh.staminaMax, staminaMax, sizeof staminaMax);
    sh.players = int8_t(players);
}

// ---- janela

struct View
{
    std::vector<Sweep> sweeps;
    std::vector<Hit> hits;
    std::vector<Hull> hulls;
    std::vector<Capsule> caps;
    std::vector<MoveRec> moves;
    std::vector<StunRec> stuns;
    uint32_t frame = 0;
    float health[16] = {}, maxHealth[16] = {}, stamina[16] = {}, staminaMax[16] = {};
    int players = 1;
    Combo combo;
    float bestCombo = 0.f;
    int bestHits = 0;
    std::vector<PadRec> pad[2];
};

// copia o estado compartilhado (descartando as esferas velhas e guardando os ultimos danos)
View snapshot()
{
    constexpr uint32_t kSweepLife = 10, kHitsKept = 64;
    Shared &sh = shared();
    View v;
    std::lock_guard<std::mutex> lk(sh.m);
    const uint32_t f = sh.frame;
    if (g_resetStats.exchange(false))
    {
        sh.hits.clear();
        sh.moves.clear();
        sh.stuns.clear();
        sh.combo = Combo{};
        sh.bestCombo = 0.f;
        sh.bestHits = 0;
    }
    v.moves = sh.moves;
    v.stuns = sh.stuns;
    sh.sweeps.erase(std::remove_if(sh.sweeps.begin(), sh.sweeps.end(),
                                   [&](const Sweep &s) { return s.frame + kSweepLife < f || s.frame > f + 600; }),
                    sh.sweeps.end());
    if (sh.hits.size() > kHitsKept)
        sh.hits.erase(sh.hits.begin(), sh.hits.end() - kHitsKept);
    sh.hits.erase(std::remove_if(sh.hits.begin(), sh.hits.end(), [&](const Hit &h) { return h.frame > f + 600; }), sh.hits.end());
    v.sweeps = sh.sweeps;
    v.hits = sh.hits;
    v.hulls = sh.hulls;
    v.caps = sh.caps;
    v.frame = f;
    std::memcpy(v.health, sh.health, sizeof v.health);
    std::memcpy(v.maxHealth, sh.maxHealth, sizeof v.maxHealth);
    std::memcpy(v.stamina, sh.stamina, sizeof v.stamina);
    std::memcpy(v.staminaMax, sh.staminaMax, sizeof v.staminaMax);
    v.players = sh.players;
    v.combo = sh.combo;
    v.bestCombo = sh.bestCombo;
    v.bestHits = sh.bestHits;
    v.pad[0] = sh.pad[0];
    v.pad[1] = sh.pad[1];
    return v;
}

Color ownerColor(int owner, int players, unsigned char a)
{
    if (owner >= 0 && owner < players)
        return Color{80, 200, 255, a};   // jogador: azul claro
    if (owner >= 0)
        return Color{255, 150, 40, a};   // IA: laranja
    return Color{220, 220, 220, a};
}

// Dentro do BeginMode3D da vista cheia, depois da cena
void draw3d()
{
    if (!g_on.load(std::memory_order_relaxed))
        return;
    const uint32_t opts = g_opts.load(std::memory_order_relaxed);
    const View v = snapshot();
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    if (opts & kShowHulls)
        for (const Hull &h : v.hulls)
        {
            const Color c = h.owner < v.players ? Color{60, 255, 120, 230} : Color{255, 60, 220, 230};
            for (size_t i = 0; i + 5 < h.seg.size(); i += 6)
                DrawLine3D(Vector3{h.seg[i], h.seg[i + 1], h.seg[i + 2]}, Vector3{h.seg[i + 3], h.seg[i + 4], h.seg[i + 5]}, c);
        }
    if (opts & kShowCapsule)
        for (const Capsule &c : v.caps)
            DrawCapsuleWires(Vector3{c.a[0], c.a[1], c.a[2]}, Vector3{c.b[0], c.b[1], c.b[2]}, c.r, 10, 3, Color{200, 200, 200, 120});
    if (opts & kShowAttacks)
        for (const Sweep &s : v.sweeps)
        {
            const uint32_t age = v.frame >= s.frame ? v.frame - s.frame : 0;
            const unsigned char a = static_cast<unsigned char>(255 - std::min<uint32_t>(age, 10) * 20);
            Color c = s.hit == 2 ? Color{255, 40, 40, a} : s.hit == 1 ? Color{255, 230, 40, a} : ownerColor(s.owner, v.players, a);
            const Vector3 pa{s.a[0], s.a[1], s.a[2]}, pb{s.b[0], s.b[1], s.b[2]};
            const float r = std::max(s.r, 2.f);
            if (s.a[0] == s.b[0] && s.a[1] == s.b[1] && s.a[2] == s.b[2])
                DrawSphereWires(pa, r, 6, 10, c);
            else
                DrawCapsuleWires(pa, pb, r, 10, 3, c);
        }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
}

const char *sourceName(int s)
{
    switch (s)
    {
    // HitEvent::source (s_takeHitInfo +0x10), os vistos no treino e nas investigacoes
    case -1: return "over time (fire, shock, fall)";
    case 2: case 11: return "melee";
    case 3: return "object swing";
    case 1: return "projectile";
    case 7: return "butt slam";
    case 8: case 13: return "throw";
    case 5: case 6: case 12: case 14: case 20: return "special";
    default: return "other";
    }
}

std::string slotName(int s, int players)
{
    if (s < 0)
        return "?";
    char b[16];
    if (s < players)
        std::snprintf(b, sizeof b, "P%d", s + 1);
    else if (s >= 4)   // as IAs ficam em m_monsters[4 + i]
        std::snprintf(b, sizeof b, "CPU%d", s - 3);
    else
        std::snprintf(b, sizeof b, "#%d", s);
    return b;
}

// Menu F2 e painel de dano (na janela, por cima do quadro). `f1Open` fecha o menu F1 quando o F2 abre (as teclas 1..8 sao dos dois).
void draw2d(bool &f1Open)
{
    static bool open = false;
    if (IsKeyPressed(KEY_F2))
    {
        if (!g_on.load())
        {
            g_on = true;
            open = true;
            std::fprintf(stderr, "[treino] modo treino ligado (F2)\n");
        }
        else
            open = !open;
        if (open)
            f1Open = false;
    }
    if (f1Open)
        open = false;
    const int fs = 20, pad = 6;
    if (open)
    {
        uint32_t o = g_opts.load();
        const uint32_t o0 = o;
        if (IsKeyPressed(KEY_ONE)) { g_on = !g_on.load(); std::fprintf(stderr, "[treino] modo treino %s\n", g_on.load() ? "ligado" : "desligado"); }
        if (IsKeyPressed(KEY_TWO)) o ^= kShowAttacks;
        if (IsKeyPressed(KEY_THREE)) o ^= kShowHulls;
        if (IsKeyPressed(KEY_FOUR)) o ^= kShowCapsule;
        if (IsKeyPressed(KEY_FIVE)) g_dummy = (g_dummy.load() + 1) % DCount;
        if (IsKeyPressed(KEY_SIX))
        {   // recarga: desligada, 100, 75, 50, 25, 10 % (a vida do boneco; o jogador volta sempre cheio)
            static constexpr int kPct[] = {100, 75, 50, 25, 10};
            if (!(o & kRefill)) { o |= kRefill; g_dummyHealth = 100; }
            else
            {
                int i = 0;
                while (i < 5 && kPct[i] != g_dummyHealth.load()) ++i;
                if (i >= 4) o &= ~kRefill;
                else g_dummyHealth = kPct[i + 1];
            }
        }
        if (IsKeyPressed(KEY_SEVEN)) o ^= kMeters;
        if (IsKeyPressed(KEY_EIGHT)) o ^= kShowInputs;
        if (IsKeyPressed(KEY_NINE)) o ^= kPanel;
        if (IsKeyPressed(KEY_ZERO)) g_resetStats = true;
        if (o != o0)
            g_opts = o;
        const bool on = g_on.load();
        static const char *const kDummy[DCount] = {"STAND STILL", "CPU FIGHTS", "ALWAYS BLOCK", "BLOCK AFTER 1ST HIT"};
        char hp[16];
        if (o & kRefill) std::snprintf(hp, sizeof hp, "%d%%", g_dummyHealth.load());
        else std::snprintf(hp, sizeof hp, "OFF");
        char lines[12][72];
        int nl = 0;
        std::snprintf(lines[nl++], 72, "TRAINING (F2 closes)");
        std::snprintf(lines[nl++], 72, "1  Training mode     %s", on ? "ON" : "-");
        std::snprintf(lines[nl++], 72, "2  Attack spheres    %s", (o & kShowAttacks) ? "ON" : "-");
        std::snprintf(lines[nl++], 72, "3  Body boxes        %s", (o & kShowHulls) ? "ON" : "-");
        std::snprintf(lines[nl++], 72, "4  Push capsules     %s", (o & kShowCapsule) ? "ON" : "-");
        std::snprintf(lines[nl++], 72, "5  Dummy             %s", kDummy[std::clamp(g_dummy.load(), 0, DCount - 1)]);
        std::snprintf(lines[nl++], 72, "6  Health refill     %s", hp);
        std::snprintf(lines[nl++], 72, "7  Stamina, special  %s", (o & kMeters) ? "ALWAYS FULL" : "-");
        std::snprintf(lines[nl++], 72, "8  Input display     %s", (o & kShowInputs) ? "ON" : "-");
        std::snprintf(lines[nl++], 72, "9  Damage panel      %s", (o & kPanel) ? "ON" : "-");
        std::snprintf(lines[nl++], 72, "0  Reset damage log");
        int w = 0;
        for (int i = 0; i < nl; ++i)
            w = std::max(w, MeasureText(lines[i], fs));
        const int x = 10, top = 40;
        DrawRectangle(x - pad, top - pad, w + 2 * pad, nl * (fs + 4) + 2 * pad, Color{0, 0, 0, 170});
        for (int i = 0; i < nl; ++i)
            DrawText(lines[i], x, top + i * (fs + 4), fs, i == 0 ? Color{120, 255, 160, 255} : RAYWHITE);
    }
}

// Seta da direcao (notacao do teclado numerico; 5 = neutro, um ponto) num quadrado de lado `sz` centrado em (cx, cy)
void drawDir(int dir, float cx, float cy, float sz, Color c)
{
    const float dx = float((dir - 1) % 3 - 1), dy = -float((dir - 1) / 3 - 1);   // tela: y para baixo
    if (dx == 0.f && dy == 0.f)
    {
        DrawCircleV(Vector2{cx, cy}, sz * 0.12f, Color{c.r, c.g, c.b, 140});
        return;
    }
    const float l = std::sqrt(dx * dx + dy * dy), ux = dx / l, uy = dy / l, h = sz * 0.42f;
    const Vector2 tail{cx - ux * h, cy - uy * h}, tip{cx + ux * h, cy + uy * h};
    const Vector2 base{tip.x - ux * sz * 0.35f, tip.y - uy * sz * 0.35f};
    DrawLineEx(tail, base, sz * 0.14f, c);
    const Vector2 p1{base.x - uy * sz * 0.28f, base.y + ux * sz * 0.28f}, p2{base.x + uy * sz * 0.28f, base.y - ux * sz * 0.28f};
    // o raylib so desenha um dos sentidos (o outro e descartado): manda os dois
    DrawTriangle(tip, p1, p2, c);
    DrawTriangle(tip, p2, p1, c);
}

// Historico dos comandos (P1 e o primeiro boneco), no lado esquerdo da faixa 4:3, de cima para baixo do mais novo para o mais velho:
// quadros segurados, direcao (relativa ao monstro: 8 frente) e botoes, nos icones do dispositivo do jogador (wotm_prompts.inc)
void drawInputs(int rw, int rh, const View &v)
{
    namespace st = wotm::settings;
    const int fs = std::max(14, rh / 55);
    const float isz = float(fs) * 1.15f, rowH = isz + 4.f;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    struct B { uint16_t bit; st::Bind bind; const char *txt; };
    static constexpr B kB[] = {{kBtnSquare, st::BSquare, "SQ"}, {kBtnTriangle, st::BTriangle, "TRI"}, {kBtnX, st::BCross, "X"},
                               {kBtnCircle, st::BCircle, "O"}, {kBtnL1, st::BL1, "L1"}, {kBtnR1, st::BR1, "R1"},
                               {kBtnL2, st::BL2, "L2"}, {kBtnR2, st::BR2, "R2"}};
    const int rows = 12;
    float x = gr.x0 + float(fs);
    const float top = float(rh) / 6.f;
    for (int k = 0; k < 2; ++k)
    {
        const std::vector<PadRec> &h = v.pad[k];
        if (k == 1 && (h.empty() || g_dummy.load() == DStand))
            continue;
        const float w = float(MeasureText("999", fs)) + isz * 6.f;
        const int n = std::min<int>(rows, int(h.size()));
        DrawRectangle(int(x) - 4, int(top) - 4, int(w) + 8, int(rowH * float(n + 1)) + 8, Color{0, 0, 0, 130});
        DrawText(k == 0 ? "P1 inputs" : "Dummy inputs", int(x), int(top), fs, Color{120, 255, 160, 255});
        for (int i = 0; i < n; ++i)
        {
            const PadRec &r = h[h.size() - 1 - size_t(i)];
            const float y = top + rowH * float(i + 1);
            const unsigned char a = static_cast<unsigned char>(255 - i * 12);
            char fr[8];
            std::snprintf(fr, sizeof fr, "%u", r.frames);
            DrawText(fr, int(x), int(y + (isz - float(fs)) * 0.5f), fs, Color{200, 200, 200, a});
            float bx = x + float(MeasureText("999", fs)) + 6.f;
            drawDir(r.dir, bx + isz * 0.5f, y + isz * 0.5f, isz, Color{255, 255, 255, a});
            bx += isz + 4.f;
            for (const B &b : kB)
                if (r.btn & b.bit)
                {
                    if (!wotm_prompts::drawCell2d(wotm_prompts::cellFor(b.bind), bx + isz * 0.5f, y + isz * 0.5f, isz, float(a) / 255.f))
                        DrawText(b.txt, int(bx), int(y), fs, Color{255, 230, 120, a});
                    bx += isz + 2.f;
                }
        }
        x += w + float(fs);
    }
}

// Painel de dano, dentro do quadro nativo (rw x rh; sai nas fotos do WOTM_SHOTS, some no modo foto); tambem os comandos
void drawPanel(int rw, int rh)
{
    if (!g_on.load())
        return;
    const View v = snapshot();
    if (g_opts.load() & kShowInputs)
        drawInputs(rw, rh, v);
    if (!(g_opts.load() & kPanel))
        return;
    const int fs = std::max(14, rh / 55), pad = fs / 3;
    // combo: o da thread do EE (golpes que a vitima levou sem se recuperar, wotm_training.inc, takeDamage)
    std::vector<std::string> lines;
    lines.push_back("TRAINING  (F2 options)");
    if (!v.hits.empty())
    {
        const Hit &last = v.hits.back();
        char b[128];
        if (last.base >= 0.f)
            std::snprintf(b, sizeof b, "Last: %.1f  (asked %.1f, %s)", last.applied, last.base, sourceName(last.source));
        else
            std::snprintf(b, sizeof b, "Last: %.1f  (%s)", last.applied, sourceName(last.source));
        lines.push_back(b);
        // terminou: a reacao que a vitima teve depois do ultimo golpe ja fechou (o StunRec so entra no fim dela), ou 3 s sem golpe
        const Combo &c = v.combo;
        bool ended = v.frame - c.last > 180u;
        for (const StunRec &s : v.stuns)
            if (s.victim == c.victim && s.end > c.last + 1)
                ended = true;
        const bool open = !ended;
        std::snprintf(b, sizeof b, "Combo: %d hit%s, %.1f damage%s", c.hits, c.hits == 1 ? "" : "s", c.damage, open ? "" : "  (ended)");
        lines.push_back(b);
        std::snprintf(b, sizeof b, "Best combo: %.1f damage (%d hit%s)", v.bestCombo, v.bestHits, v.bestHits == 1 ? "" : "s");
        lines.push_back(b);
        lines.push_back("");
        int shown = 0;
        for (auto it = v.hits.rbegin(); it != v.hits.rend() && shown < 6; ++it, ++shown)
        {
            std::snprintf(b, sizeof b, "%5.1f  %s -> %s  %s", it->applied, slotName(it->attacker, v.players).c_str(),
                          slotName(it->victim, v.players).c_str(), sourceName(it->source));
            lines.push_back(b);
        }
    }
    else
        lines.push_back("Hit someone: the damage shows here");
    // dados de quadro: os ultimos golpes (inicio = primeiro quadro com teste de golpe; ativo = quadros com teste; total = o estado inteiro)
    // e, no que acertou, quanto a vitima ficou sem agir (reacoes seguidas somadas) e a vantagem (fim da reacao - fim do golpe)
    if (!v.moves.empty())
    {
        lines.push_back("");
        lines.push_back("Frame data (frames, 60 per second)");
        int shown = 0;
        for (auto it = v.moves.rbegin(); it != v.moves.rend() && shown < 4; ++it, ++shown)
        {
            const MoveRec &mv = *it;
            char nm[40], b[160];
            if (mv.chain >= 0)
                std::snprintf(nm, sizeof nm, "%s %s c%d.%d", slotName(mv.owner, v.players).c_str(), mv.name, mv.chain, mv.step);
            else
                std::snprintf(nm, sizeof nm, "%s %s", slotName(mv.owner, v.players).c_str(), mv.name);
            char st[8];
            if (mv.startup > 0)
                std::snprintf(st, sizeof st, "%d", mv.startup);
            else
                std::snprintf(st, sizeof st, "-");
            std::snprintf(b, sizeof b, "%s: startup %s, active %d, total %u", nm, st, mv.active, mv.end - mv.start);
            lines.push_back(b);
            if (mv.hitVictim >= 0)
            {
                uint32_t s0 = 0, s1 = 0;
                const char *sname = nullptr;
                for (const StunRec &s : v.stuns)
                {
                    if (s.victim != mv.hitVictim)
                        continue;
                    if (!sname && s.start >= mv.start && s.start <= mv.end + 1)
                    {
                        s0 = s.start;
                        s1 = s.end;
                        sname = s.name;
                    }
                    else if (sname && s.start == s1)   // reacao emendada (recuo -> derrubado)
                        s1 = s.end;
                }
                if (sname)
                {
                    const int adv = int(s1) - int(mv.end);
                    std::snprintf(b, sizeof b, "   hit %s: %s for %u, advantage %+d", slotName(mv.hitVictim, v.players).c_str(), sname, s1 - s0, adv);
                }
                else
                    std::snprintf(b, sizeof b, "   hit %s (no reaction yet)", slotName(mv.hitVictim, v.players).c_str());
                lines.push_back(b);
            }
        }
    }
    lines.push_back("");
    for (int i = 0; i < 16; ++i)
        if (v.maxHealth[i] > 0.f)
        {
            char b[96];
            std::snprintf(b, sizeof b, "%s health %.1f / %.0f, stamina %.0f / %.0f", slotName(i, v.players).c_str(), v.health[i], v.maxHealth[i],
                          v.stamina[i], v.staminaMax[i]);
            lines.push_back(b);
        }
    int w = 0;
    for (const std::string &s : lines)
        w = std::max(w, MeasureText(s.c_str(), fs));
    // canto de cima a direita, dentro da faixa 4:3 do jogo (abaixo da barra de vida do inimigo, que fica no alto)
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const int x = int(gr.x0 + gr.w) - w - 2 * pad, top = rh / 6;
    DrawRectangle(x - pad, top - pad, w + 2 * pad, int(lines.size()) * (fs + 4) + 2 * pad, Color{0, 0, 0, 150});
    for (size_t i = 0; i < lines.size(); ++i)
        DrawText(lines[i].c_str(), x, top + int(i) * (fs + 4), fs, i == 0 ? Color{120, 255, 160, 255} : RAYWHITE);
}
}
