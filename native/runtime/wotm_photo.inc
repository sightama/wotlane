// wotm_photo.inc: modo foto (incluido por wotm_live.inc depois do wotm_legend.inc; escopo global de ps2_runtime.cpp).
//
// F9 no teclado ou L3 + R3 juntos no gamepad: o jogo pausa (a mesma pausa do F7, wotmPhotoPause no wotm_foto.inc) e a camera solta,
// com zoom. Os controles aparecem na legenda de baixo (wotm_legend.inc, icones do estilo do ultimo dispositivo usado), numa faixa
// translucida. Com a thread do EE parada o jogo nao refaz nada que dependa da camera, entao o desenho leva junto, a cada quadro:
//  - HUD do hier (Scene::hud, hp, overlay, hpOverlay): objetos 3D que o Hud::update pos na frente da camera do jogo. Cada matriz vai
//    do espaco do olho do jogo para o da camera livre (com o zoom compensado nos eixos da tela), entao o HUD fica parado na tela;
//  - lens flare: a lista e refeita a cada quadro pela camera livre, como o jogo a monta (hierLightNode e particleAddLensFlareToList) e
//    projeta (particleLensFlareTestZbuffer): luzes do hier com lens flare que a coleta achou (Scene::worldLights, inclusive as que
//    estavam fora da tela na pausa, como o olho do Robo-47 visto de frente) e sistemas de particula com luz; o teste de oclusao do
//    drawFlares roda contra a cena nova. Sem nenhuma das duas fontes, a lista do quadro pausado volta ao mundo e e reprojetada;
//  - fogo da cabeca do Agamo (preset 141): o emissor so solta particulas quando a travessia do jogo passa por ele. Com o Agamo fora
//    da tela na pausa o fogo ja tinha acabado; aqui ele e simulado (wotm_flame.hpp, aquecido e parado como numa foto) em cada emissor
//    percorrido sem particulas vivas perto (Particles::synth);
//  - barras sobre os inimigos e o icone de vida do jogo: o gancho do Hud::drawHealthIcon anota o ponto do mundo (Snapshot::anchors);
//    aqui o ponto e projetado pelas duas cameras e a diferenca desloca as barras (ScreenRect::anchor) e o icone (ScreenPolys no mesmo
//    centro). Na tela dividida as barras saem (a camera livre mostra so a vista 0 em tela cheia).
// O texto do jogo (cronometro, nomes) e os numeros de dano (projetados pelo nativo) ja seguem a tela/a camera. Esconder o HUD tira tudo
// isso, menos os fades. Salvar grava o quadro nativo sem a legenda em <pasta do settings.ini>/screenshots/WotM_<data>_<hora>.png.
// Ao sair, a camera do jogo volta na hora e o jogo continua quando o botao usado para sair for solto (no maximo 1 s; o botao nao chega
// ao monstro). O som segue tocando, como no F7.
#include <array>
#include <ctime>
#include <unordered_map>
#include "wotm_flame.hpp"

void wotmPhotoPause(bool on);   // wotm_foto.inc: a pausa do F7

namespace wotm_photo
{
namespace st = wotm::settings;
using wotm_legend::Icon;

enum class Phase { Off, Entering, On, Leaving };
static Phase g_phase = Phase::Off;
static double g_enterT = 0.0;
static double g_leaveT = 0.0;
static bool g_exitHeld[st::kPadButtons] = {};   // botoes do pad apertados ao sair: o jogo espera solta-los
static std::unordered_map<uint32_t, wotm::flame::Flame> g_flames;   // fogo simulado por no emissor (refeito a cada sessao)
static bool g_logged = false;    // resumo das luzes e emissores, uma vez por sessao
static bool g_autoPivotOk = false;   // teste automatico: a meia volta gira em torno da luz mais perto (olho do monstro do jogador)
static float g_autoPivot[3] = {0, 0, 0}, g_autoBeam[3] = {0, 1, 0};
static bool g_autoFireOk = false;   // teste automatico: a etapa 3 (sem HUD) olha o fogo do Agamo de perto
static float g_autoFire[3] = {0, 0, 0};
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
// Sai: a vista volta a do jogo ja; o jogo continua quando os botoes apertados agora forem soltos (update)
static void startLeaving()
{
    for (int b = 1; b < st::kPadButtons; ++b) g_exitHeld[b] = padDown(b);
    g_phase = Phase::Leaving;
    g_leaveT = GetTime();
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
    if (toggle && g_phase == Phase::Off && wotm_pad::inMenus())   // so na luta: nos menus do jogo (telas do shell) nao liga
    {
        toggle = false;
        std::fprintf(stderr, "[photo] ignorado nos menus\n");
    }
    // partida em rede: a pausa do modo foto pararia so este PC (os outros esperariam por ele)
    static const bool s_net = [] { const char *e = std::getenv("WOTM_NET"); return e && *e; }();
    const bool autoDue = std::getenv("WOTM_PHOTO") != nullptr;
    if (s_net && (toggle || autoDue))
    {
        static bool said = false;
        if (!said && (toggle || gc.ok))
        {
            said = true;
            std::fprintf(stderr, "[photo] desligado na partida em rede\n");
        }
        return {};
    }
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
        {   // a camera vai ate a frente do feixe da luz mais perto (olho do monstro do jogador), a 70, olhando para ela: o flare tem que
            // acender mesmo sem estar na tela na pausa. Sem luz: meia volta em torno de um ponto 170 a frente
            (void)dt;
            const float a = std::clamp(float(autoT - 0.4) / 2.f, 0.f, 1.f);
            const float h = g_start.heading;
            float target[3] = {g_start.pos[0] + 170.f * std::sin(h + 3.14159265f * a), g_start.pos[1] + 170.f * std::cos(h + 3.14159265f * a), g_start.pos[2]};
            float look[3] = {g_start.pos[0] + 170.f * std::sin(h), g_start.pos[1] + 170.f * std::cos(h), g_start.pos[2]};
            if (g_autoPivotOk)
                for (int k = 0; k < 3; ++k) { target[k] = g_autoPivot[k] + 70.f * g_autoBeam[k]; look[k] = g_autoPivot[k]; }
            for (int k = 0; k < 3; ++k) g_pose.pos[k] = g_start.pos[k] + (target[k] - g_start.pos[k]) * a;
            const float d[3] = {look[0] - g_pose.pos[0], look[1] - g_pose.pos[1], look[2] - g_pose.pos[2]};
            if (std::fabs(d[0]) + std::fabs(d[1]) > 1e-3f)
            {
                g_pose.heading = std::atan2(d[0], d[1]);
                g_pose.pitch = std::atan2(d[2], std::hypot(d[0], d[1]));
            }
        }
        if (autoStage == 3 && g_autoFireOk)
        {   // ao lado do emissor do fogo, um pouco acima, olhando para ele
            g_pose.pos[0] = g_autoFire[0] + 110.f;
            g_pose.pos[1] = g_autoFire[1] - 40.f;
            g_pose.pos[2] = g_autoFire[2] + 30.f;
            const float d[3] = {g_autoFire[0] - g_pose.pos[0], g_autoFire[1] - g_pose.pos[1], g_autoFire[2] + 25.f - g_pose.pos[2]};
            g_pose.heading = std::atan2(d[0], d[1]);
            g_pose.pitch = std::atan2(d[2], std::hypot(d[0], d[1]));
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
            case 6: startLeaving(); break;
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
            g_flames.clear();
            g_logged = false;
            g_autoPivotOk = false;
            g_autoFireOk = false;
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
            startLeaving();
        break;
    case Phase::Leaving:
        break;
    }
    if (g_phase == Phase::Leaving)
    {
        bool held = IsKeyDown(KEY_F9) || IsKeyDown(KEY_ESCAPE);
        for (int b = 1; b < st::kPadButtons && !held; ++b)
            if (g_exitHeld[b] && padDown(b)) held = true;
            else g_exitHeld[b] = false;   // solto uma vez, nao conta mais
        // um botao que o raylib ve sempre apertado deixava o jogo parado para sempre: depois de 1 s ele continua de qualquer jeito
        if (held && now - g_leaveT < 1.0) return wotm_freecam::View{};
    }
    if (g_phase == Phase::Leaving)
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

// Lista do lens flare refeita pela camera livre: o que o hierLightNode (luzes do hier) e o particleAddLensFlareToList (sistemas com
// vis.lightColor & 0x1000000) poem na lista, projetado como o particleLensFlareTestZbuffer (descarta W < 1 e fora da tela; distance
// = W, ou 100 com a luz no infinito; Z do GS = a + b / W). Sem ID (o drawFlares nao faz a media com o quadro anterior: a camera muda a
// cada quadro). Falso = sem fonte nenhuma (o chamador reprojeta a lista do quadro pausado).
static bool rebuildFlares(wotm::Particles::FlareFrame &fr, const wotm::Ram &ram, const std::vector<wotm::WorldLight> &lights,
                          const std::vector<wotm::FxSystem> &systems, const Eye &f, const Focus &ff)
{
    using wotm::Particles;
    std::vector<uint32_t> lit;   // sistemas de particula com luz
    for (const wotm::FxSystem &sy : systems)
        if (ram.u32(sy.base + 0x2EAC) & 0x1000000u) lit.push_back(sy.base);   // vis.lightColor
    if (lights.empty() && lit.empty()) return false;
    if (fr.nspr == 0)   // a pausa veio de um quadro sem flare: a tabela dos sprites (lensFlarePacketInit) e a moldura vem da RAM parada
    {
        Particles::FlareFrame t;
        Particles::readFlares(ram, t);
        t.e.clear();
        fr = std::move(t);
        if (fr.nspr == 0) return false;
    }
    fr.e.clear();
    const float x0 = -0.5f * fr.vw, x1 = 0.5f * fr.vw, y0 = -0.5f * fr.vh, y1 = 0.5f * fr.vh;
    auto add = [&](const float p[3], Particles::Flare fl) {
        float q[3];
        toEye(f, p, true, q);
        if (q[1] < 1.f || fr.e.size() >= 74) return;
        const float sx = ff.fh * q[0] / q[1], sy = -ff.fv * q[2] / q[1];
        if (sx < x0 || sx > x1 || sy < y0 || sy > y1) return;
        const bool inf = (fl.flags & 0x80) != 0;
        // o particleLensFlareRender soma o centroide das amostras visiveis; o drawFlares o desfaz supondo todas visiveis
        const float shift = 1.5f - (inf ? 2.f : 1.f);
        fl.x = fr.cx + sx + shift;
        fl.y = fr.cy + sy + shift;
        fl.w = inf ? 100.f : q[1];
        fl.z = fr.zScale + fr.zOff / q[1];
        fl.id = 0;
        fr.e.push_back(fl);
    };
    for (const wotm::WorldLight &l : lights)
    {
        Particles::Flare fl;
        fl.flags = l.flags;   // o byte das flags vai no lugar do Zfudge: denormal, vale o minimo (1,5)
        fl.intensity = l.intensity;
        for (int k = 0; k < 3; ++k) fl.rgb[k] = l.rgb[k];
        fl.dot = 1.f;
        fl.bw = 0.f;
        if ((l.flags & 0x80) == 0 && (l.flags & 8))
        {   // dotProd = -(olho -> luz) . feixe
            const float d[3] = {l.p[0] - f.o[0], l.p[1] - f.o[1], l.p[2] - f.o[2]};
            const float n = std::sqrt(dot3(d, d));
            fl.dot = n > 1e-6f ? -dot3(d, l.beam) / n : 1.f;
            fl.bw = l.bw;
        }
        else if (l.flags & 0x80)
            fl.bw = 1.f;   // no infinito o jogo nao grava dot/bw (ficam os da entrada anterior); aqui feixe inteiro
        add(l.p, fl);
    }
    for (uint32_t b : lit)
    {
        Particles::Flare fl;
        const uint32_t c = ram.u32(b + 0x2EAC);
        const uint32_t src = (c & 0x8000000u) ? 0x2CA0u : 0x2C80u;   // emit.ParticleToEmitVec ou FOVsphere
        const float p[3] = {ram.f32(b + src), ram.f32(b + src + 4), ram.f32(b + src + 8)};
        // o Zfudge (info + 0x1C) vai na palavra +0x10 e o byte baixo dela vira as flags (3)
        const uint32_t word = (ram.u32(b + 0x2E4C) & ~0xFFu) | 3u;
        fl.flags = word;
        std::memcpy(&fl.zfudge, &word, 4);
        for (int k = 0; k < 3; ++k) fl.rgb[k] = float((c >> (8 * k)) & 0xFF) * 0.0039215684f;
        const float li = ram.f32(b + 0x2E5C);   // info.lightIntensity
        fl.intensity = li * li;
        add(p, fl);
    }
    return true;
}

// Fogo da cabeca do Agamo (preset 141, IdolHeadFlame: textura Fireball, texInfo 8; mistura aditiva 0x48; 75f5be1:docs/investigacoes/2026-10-21/fogo_agamo.md) nos emissores
// percorridos que nao tem um sistema com particulas vivas perto: simulado 30 fields com o emissor parado (comeca aquecido, como no jogo
// ja rodando) e congelado durante a sessao
static void synthFire(wotm::Scene &scene, const wotm::Ram &ram)
{
    wotm::Particles::SynthSystem sys;
    sys.texInfo = 8;
    sys.alphaContext = 0x48;
    for (const wotm::WorldEmitter &em : scene.emitters)
    {
        if (em.preset != 141) continue;
        bool live = false;
        for (const wotm::FxSystem &sy : scene.fx.systems)
        {
            if (sy.n <= 0) continue;
            const float d[3] = {ram.f32(sy.base + 0x2CA0) - em.e[0], ram.f32(sy.base + 0x2CA4) - em.e[1], ram.f32(sy.base + 0x2CA8) - em.e[2]};
            if (dot3(d, d) < 20.f * 20.f) { live = true; break; }
        }
        if (!g_logged)
        {
            std::fprintf(stderr, "[photo] fogo do Agamo no emissor %06x (%.0f %.0f %.0f): %s\n", em.node, em.e[0], em.e[1], em.e[2],
                         live ? "particulas do jogo" : "simulado");
            for (int k = 0; k < 3; ++k) g_autoFire[k] = em.e[k];
            g_autoFireOk = true;
        }
        if (live) continue;
        auto it = g_flames.find(em.node);
        if (it == g_flames.end())
        {
            wotm::flame::Flame fl;
            uint32_t rng = 0x2545F491u ^ em.node;
            for (int k = 0; k < 30; ++k) wotm::flame::step(fl, em.e, em.r0, em.r2, true, rng);
            it = g_flames.emplace(em.node, std::move(fl)).first;
            std::fprintf(stderr, "[photo] fogo simulado no emissor %06x (%.0f %.0f %.0f): %zu particulas\n", em.node, em.e[0], em.e[1], em.e[2],
                         it->second.parts.size());
        }
        for (const wotm::flame::Part &q : it->second.parts)
            sys.sprites.push_back({{q.p[0], q.p[1], q.p[2]}, q.size, wotm::flame::color(q.age), q.sel});
    }
    if (!sys.sprites.empty()) scene.fx.synth.push_back(std::move(sys));
}

// Pausado no modo foto, depois do show: o HUD e o resto que o jogo prendeu a camera dele passam para a livre (ou somem com o HUD
// escondido). split = o quadro pausado era de tela dividida.
static void applyToScene(wotm::Scene &scene, const wotm::Ram &ram, const wotm::GameCamera &gameCam, const wotm::GameCamera &freeCam,
                         const wotm_freecam::View &v, const std::vector<wotm::HudAnchor> &anchors, bool split)
{
    const Eye g = eyeOf(gameCam), f = eyeOf(freeCam);
    const Focus fg = gameFocus(ram, gameCam);
    const Focus ff{fg.fh / v.zoom, fg.fv / v.zoom};
    {
        wotm::ViewScope vs(scene.vctx);
        if (!rebuildFlares(scene.fx.flares, ram, scene.worldLights, scene.fx.systems, f, ff))
            reprojectFlares(scene.fx.flares, g, f, fg, ff);
    }
    scene.fx.resetFlareDebounce();   // a fracao visivel e a da camera nova, sem media com a da antiga
    synthFire(scene, ram);
    if (!g_logged)
    {
        g_logged = true;
        int n141 = 0;
        for (const wotm::WorldEmitter &em : scene.emitters) n141 += em.preset == 141;
        std::fprintf(stderr, "[photo] coleta: %zu luzes com lens flare, %zu emissores (%d do fogo 141), %zu sistemas de particula\n",
                     scene.worldLights.size(), scene.emitters.size(), n141, scene.fx.systems.size());
        float best = 1e30f;
        for (const wotm::WorldLight &l : scene.worldLights)
        {
            const float d[3] = {l.p[0] - g.o[0], l.p[1] - g.o[1], l.p[2] - g.o[2]};
            std::fprintf(stderr, "[photo]   luz (%.0f %.0f %.0f) flags %02x I %.0f a %.0f do olho (no %06x, cs %06x)\n", l.p[0], l.p[1], l.p[2], l.flags,
                         l.intensity, std::sqrt(dot3(d, d)), l.node, l.cs);
            const float bl = std::sqrt(dot3(l.beam, l.beam));
            if (dot3(d, d) < best && bl > 1e-6f)
            {
                best = dot3(d, d);
                for (int k = 0; k < 3; ++k) { g_autoPivot[k] = l.p[k]; g_autoBeam[k] = l.beam[k] / bl; }
                g_autoPivotOk = true;
            }
        }
    }
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
// Camera livre de debug (F5, wotm_freecam.inc): a mesma legenda de icones no lugar da linha de texto
static void wotmFreeCamLegend(const wotm_freecam::View &v)
{
    using wotm_legend::Icon;
    if (!v.on || v.photo) return;
    const bool pad = g_wotmFreeCamPad.load(std::memory_order_relaxed);
    wotm_legend::post({{Icon{0, "WASD"}, {}, "MOVE"},
                       {Icon{0, "Q"}, Icon{0, "E"}, "HEIGHT"},
                       {Icon{0, "MOUSE_RIGHT"}, {}, "LOOK"},
                       {Icon{0, "LSHIFT"}, Icon{0, "LCTRL"}, "FAST SLOW"},
                       {Icon{0, "MOUSE_SCROLL"}, {}, "SPEED"},
                       {Icon{0, "TAB"}, {}, pad ? "PAD TO CAMERA" : "PAD TO GAME"},
                       {Icon{0, "F5"}, {}, "EXIT"}},
                      1.f, 0.8f);
}
// Apresentacao, todo quadro (wotmFotoKeys): se o desenho nativo parou de chamar o update (tela sem cena nativa), o modo foto sai e o
// jogo volta, em vez de ficar pausado sem legenda
void wotmPhotoWatchdog()
{
    using namespace wotm_photo;
    if (g_phase != Phase::Off && GetTime() - g_lastUpdate > 3.0)
    {
        wotmPhotoPause(false);
        g_phase = Phase::Off;
        std::fprintf(stderr, "[photo] desligado: a tela deixou de ter cena nativa\n");
    }
}
