// wotm_photo.inc: modo foto (incluido por wotm_live.inc depois do wotm_legend.inc; escopo global de ps2_runtime.cpp).
//
// F9 no teclado ou L3 + R3 juntos no gamepad: o jogo pausa (a mesma pausa do F7, wotmPhotoPause no wotm_foto.inc) e a camera solta,
// com zoom. Os controles aparecem na legenda de baixo (wotm_legend.inc, icones do estilo do ultimo dispositivo usado), numa faixa
// translucida. Com a thread do EE parada o jogo nao refaz nada que dependa da camera, entao o desenho leva junto, a cada quadro:
//  - HUD do hier (Scene::hud, hp, overlay, hpOverlay): objetos 3D que o Hud::update pos na frente da camera do jogo. Cada matriz vai
//    do espaco do olho do jogo para o da camera livre (com o zoom compensado nos eixos da tela), entao o HUD fica parado na tela;
//  - lens flare: a lista do quadro ja vem projetada na tela do jogo (x, y do GS e W). Volta ao mundo pela camera do jogo e e
//    projetada de novo pela livre (luz no infinito: so a direcao); o teste de oclusao do drawFlares roda contra a cena nova. So as
//    luzes que estavam na tela na hora da pausa existem (o jogo compactou a lista);
//  - barras sobre os inimigos e o icone de vida do jogo: o gancho do Hud::drawHealthIcon anota o ponto do mundo (Snapshot::anchors);
//    aqui o ponto e projetado pelas duas cameras e a diferenca desloca as barras (ScreenRect::anchor) e o icone (ScreenPolys no mesmo
//    centro). Na tela dividida as barras saem (a camera livre mostra so a vista 0 em tela cheia).
// O texto do jogo (cronometro, nomes) e os numeros de dano (projetados pelo nativo) ja seguem a tela/a camera. Esconder o HUD tira tudo
// isso, menos os fades. Salvar grava o quadro nativo sem a legenda em <pasta do settings.ini>/screenshots/WotM_<data>_<hora>.png.
// Ao sair, o jogo so volta quando os botoes forem soltos (o botao de sair nao chega ao monstro). O som segue tocando, como no F7.
#include <array>
#include <ctime>

void wotmPhotoPause(bool on);   // wotm_foto.inc: a pausa do F7

namespace wotm_photo
{
namespace st = wotm::settings;
using wotm_legend::Icon;

enum class Phase { Off, Entering, On, Leaving };
static Phase g_phase = Phase::Off;
static double g_enterT = 0.0;
static WotmFreeCamPose g_pose, g_start;
static float g_zoom = 1.f;
static float g_speed = 220.f;   // unidades do mundo por segundo
static bool g_hide = false;
static bool g_saveReq = false;
static bool g_legendShot = false;   // teste automatico: grava o quadro com a legenda na pasta atual
static std::string g_msg;
static double g_msgUntil = 0.0;
static uint64_t g_fontTex0 = 0;   // atlas da fonte do jogo do ultimo quadro com texto (a legenda escreve com ele)

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
// eixo com zona morta, o de maior modulo entre os gamepads
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
    return std::copysign(s * s, best);   // curva: devagar perto do centro
}
static bool anyPadButton()
{
    for (int b = 1; b < st::kPadButtons; ++b)
        if (padDown(b)) return true;
    return false;
}
// L3 + R3: os dois apertados, um deles agora
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
    // olhar: mouse com o botao direito, setas, analogico direito (mais devagar com zoom)
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
    // andar: WASD + Q/E, analogico esquerdo + L2/R2; Shift x4, Ctrl x0,25
    float k = g_speed * dt;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) k *= 4.f;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) k *= 0.25f;
    const float mf = float(IsKeyDown(KEY_W)) - float(IsKeyDown(KEY_S)) - padAxis(GAMEPAD_AXIS_LEFT_Y);
    const float mr = float(IsKeyDown(KEY_D)) - float(IsKeyDown(KEY_A)) + padAxis(GAMEPAD_AXIS_LEFT_X);
    const float mu = float(IsKeyDown(KEY_E)) - float(IsKeyDown(KEY_Q)) + float(padDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) -
                     float(padDown(GAMEPAD_BUTTON_LEFT_TRIGGER_2));
    float r[3], f[3], u[3];
    wotmFreeCamAxes(p, r, f, u);
    // frente no plano do chao: andar nao sobe nem desce com a camera inclinada (a altura e Q/E, L2/R2)
    float fh[3] = {f[0], f[1], 0.f};
    const float n = std::sqrt(fh[0] * fh[0] + fh[1] * fh[1]);
    if (n > 1e-4f) { fh[0] /= n; fh[1] /= n; }
    else { fh[0] = -u[0]; fh[1] = -u[1]; }   // olhando reto para baixo/cima
    for (int i = 0; i < 2; ++i) p.pos[i] += k * (mf * fh[i] + mr * r[i]);
    p.pos[2] += k * mu;
    // zoom: roda do mouse, L1/R1
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.f) g_zoom *= std::pow(0.88f, wheel);
    g_zoom *= std::exp(1.2f * dt * (float(padDown(GAMEPAD_BUTTON_LEFT_TRIGGER_1)) - float(padDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_1))));
    g_zoom = std::clamp(g_zoom, 0.15f, 1.6f);
}

// Apresentacao, a cada quadro, antes do desenho. gc = a camera do jogo do instantaneo. Devolve a vista para o desenho (on = falso:
// a camera livre do F5 decide).
static double g_lastUpdate = 0.0;
static wotm_freecam::View update(const wotm::GameCamera &gc)
{
    const double now = GetTime();
    g_lastUpdate = now;
    bool toggle = IsKeyPressed(KEY_F9) || chord();
    // Teste sem teclado (WOTM_PHOTO=<s>): s segundos depois da primeira cena liga, gira e anda de lado, salva (HUD, sem HUD, com zoom)
    // e sai; as fotos vao para a pasta screenshots
    static const double autoAt = std::getenv("WOTM_PHOTO") ? std::atof(std::getenv("WOTM_PHOTO")) : -1.0;
    static double firstT = -1.0;
    static int autoStage = 0;
    if (autoAt >= 0.0 && gc.ok && firstT < 0.0) firstT = now;
    const double autoT = autoAt >= 0.0 && firstT >= 0.0 ? now - firstT - autoAt : -1.0;
    if (autoT >= 0.0 && autoStage == 0)
    {
        toggle = g_phase == Phase::Off;
        autoStage = 1;
        std::fprintf(stderr, "[photo] teste automatico: liga\n");
    }
    if (autoStage >= 1 && autoStage < 7 && g_phase == Phase::On)
    {
        const float dt = std::min(GetFrameTime(), 0.1f);
        if (autoStage == 2)
        {
            float r[3], f[3], u[3];
            wotmFreeCamAxes(g_pose, r, f, u);
            g_pose.heading -= 0.35f * dt;
            for (int i = 0; i < 3; ++i) g_pose.pos[i] += 90.f * dt * r[i];
            g_pose.pos[2] += 25.f * dt;
        }
        static const double stageAt[6] = {0.4, 2.5, 3.5, 4.5, 5.5, 6.5};
        if (autoT >= stageAt[autoStage - 1])
        {
            switch (autoStage)
            {
            case 1: g_saveReq = true; break;                                   // camera do jogo (referencia)
            case 2: g_saveReq = true; break;                                   // depois de andar: HUD seguindo a camera
            case 3: g_hide = true; g_saveReq = true; break;                    // sem HUD
            case 4: g_hide = false; g_zoom = 0.5f; g_saveReq = true; break;    // zoom
            case 5: g_legendShot = true; break;                                // quadro com a legenda (photo_test_legend.png)
            case 6: g_phase = Phase::Leaving; break;
            }
            std::fprintf(stderr, "[photo] teste automatico: etapa %d\n", autoStage);
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
            g_pose.heading = std::atan2(gc.fwd[0], gc.fwd[1]);   // como o F5 (mathfRotationFromVector)
            g_pose.pitch = std::atan2(gc.fwd[2], std::sqrt(gc.fwd[0] * gc.fwd[0] + gc.fwd[1] * gc.fwd[1]));
            g_start = g_pose;
            g_zoom = 1.f;
            g_hide = false;
            g_saveReq = false;
            g_wotmFreeCam = false;   // a camera livre de debug sai: o modo foto nao escreve no _cs do jogo
            wotmPhotoPause(true);
            g_phase = Phase::Entering;
            g_enterT = now;
            std::fprintf(stderr, "[photo] ligado\n");
        }
        break;
    case Phase::Entering:
        if (wotmPaused())
            g_phase = Phase::On;
        else if (toggle || now - g_enterT > 1.5)   // tela sem quadro do jogo (filme, carga): a pausa nao chega
        {
            wotmPhotoPause(false);
            g_phase = Phase::Off;
            std::fprintf(stderr, "[photo] cancelado: a pausa nao chegou (tela sem quadro do jogo)\n");
        }
        break;
    case Phase::On:
        if (toggle || IsKeyPressed(KEY_ESCAPE) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            g_phase = Phase::Leaving;
        break;
    case Phase::Leaving:
        break;
    }
    if (g_phase == Phase::Leaving && !anyPadButton() && !IsKeyDown(KEY_F9) && !IsKeyDown(KEY_ESCAPE))
    {
        wotmPhotoPause(false);
        g_phase = Phase::Off;
        std::fprintf(stderr, "[photo] desligado\n");
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
        // o quadro da foto sai sem a legenda nem o aviso (afterRender o grava)
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

// ------------------------------------------------------------------ camera do jogo -> camera livre
struct Eye { float o[3], r[3], f[3], u[3]; };
static Eye eyeOf(const wotm::GameCamera &c)
{
    Eye e;
    for (int i = 0; i < 3; ++i) { e.o[i] = c.pos[i]; e.f[i] = c.fwd[i]; e.u[i] = c.up[i]; }
    // direita = frente x cima (a mesma do wotmFreeCamAxes e da linha 0 da worldToScreenMat)
    e.r[0] = e.f[1] * e.u[2] - e.f[2] * e.u[1];
    e.r[1] = e.f[2] * e.u[0] - e.f[0] * e.u[2];
    e.r[2] = e.f[0] * e.u[1] - e.f[1] * e.u[0];
    const float n = std::sqrt(e.r[0] * e.r[0] + e.r[1] * e.r[1] + e.r[2] * e.r[2]);
    if (n > 1e-6f) for (float &x : e.r) x /= n;
    return e;
}
static float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
// mundo -> (direita, frente, cima) do olho; point = com a translacao
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

// Projecao do jogo: focos horizontal (px do GS) e vertical (linhas) da worldToScreenMat da vista 0 (linha 0 = fh*direita + c*frente,
// linha 1 = -fv*cima + c*frente, linha 3 = frente). Com a RAM parada (pausa) a matriz e a do quadro pausado.
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
// Ponto do mundo -> deslocamento do centro da tela do jogo (px do GS em x, linhas em y); falso atras da camera
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

// Matriz (linhas = eixos, linha 3 = posicao) presa a camera do jogo -> presa a livre; os eixos da tela encolhem com o zoom (o HUD
// fica do mesmo tamanho na tela)
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

// Lens flare da tela do jogo para a da camera livre (FlareFrame em px do GS, centro cx, cy)
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
            fromEye(g, qg, false, w);   // so a direcao
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

// Pausado no modo foto, depois do show: o HUD e o resto que o jogo prendeu a camera dele passam para a livre (ou somem com o HUD
// escondido). split = o quadro pausado era de tela dividida.
static void applyToScene(wotm::Scene &scene, const wotm::Ram &ram, const wotm::GameCamera &gameCam, const wotm::GameCamera &freeCam,
                         const wotm_freecam::View &v, const std::vector<wotm::HudAnchor> &anchors, bool split)
{
    const Eye g = eyeOf(gameCam), f = eyeOf(freeCam);
    const Focus fg = gameFocus(ram, gameCam);
    const Focus ff{fg.fh / v.zoom, fg.fv / v.zoom};
    reprojectFlares(scene.fx.flares, g, f, fg, ff);
    scene.fx.resetFlareDebounce();   // a fracao visivel e a da camera nova, sem media com a da antiga
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
    // deslocamento (tela do jogo, 640 x 224) de cada icone de vida da vista 0; NAN = atras da camera livre
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
    // icone de vida do jogo (sem as barras): os dois quadrados centrados no ponto anotado
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

// Apresentacao, depois do EndTextureMode do quadro: grava a foto pedida (o quadro acabou de sair sem a legenda)
static void afterRender()
{
    if (g_legendShot)
    {
        g_legendShot = false;
        std::fprintf(stderr, "[photo] teste automatico: quadro com a legenda %s\n",
                     wotm_live::exportFrame("photo_test_legend.png") ? "em photo_test_legend.png" : "nao gravado");
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
        std::fprintf(stderr, "[photo] foto salva em %s\n", file.string().c_str());
        say("PHOTO SAVED");
    }
    else
    {
        std::fprintf(stderr, "[photo] nao consegui gravar %s\n", file.string().c_str());
        say("COULD NOT SAVE THE PHOTO", 3.0);
    }
}

// Atlas da fonte do jogo para a legenda: o do texto do quadro, ou o ultimo visto (HUD escondido, quadro sem texto)
static uint64_t fontTex0(const std::vector<wotm::TextQuad> &text)
{
    if (!text.empty() && text.front().tex0) g_fontTex0 = text.front().tex0;
    return g_fontTex0;
}
} // namespace wotm_photo

bool wotmPhotoActive() { return wotm_photo::g_phase != wotm_photo::Phase::Off; }
// Apresentacao, todo quadro (wotmFotoKeys): se o desenho nativo parou de chamar o update (tela sem cena nativa), o modo foto sai e o
// jogo volta, em vez de ficar pausado sem legenda
void wotmPhotoWatchdog()
{
    using namespace wotm_photo;
    if (g_phase != Phase::Off && GetTime() - g_lastUpdate > 1.0)
    {
        wotmPhotoPause(false);
        g_phase = Phase::Off;
        std::fprintf(stderr, "[photo] desligado: a tela deixou de ter cena nativa\n");
    }
}
