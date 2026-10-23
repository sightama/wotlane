// wotm_padsel.inc: menu multijogador no telao (incluido por wotm_live.inc, com o desenho nativo).
//
// Menu principal (tela 1): o item "2 player" vira "MULTIPLAYER". Os objetos do switch dele (shell->m_menuItemSw[1][2]: o texto
// apagado e o aceso) ficam com alfa 0 e o rotulo nativo e desenhado no mesmo retangulo do telao, aceso quando o cursor esta nele.
// O jogo continua tratando o item como o 2 player: X leva a tela 5 (screenGameModes2P).
//
// Escolha de controles (tela 5 vinda do menu principal): o telao mostra os controles conectados e o teclado. X (Espaco no teclado)
// entra no jogo como o menor jogador livre (1 a 4); com o direcional esquerda/direita o dispositivo troca de jogador (dois
// dispositivos no mesmo numero sao o mesmo jogador, como o teclado junto do controle 1); triangulo (X/Backspace no teclado) sai, e
// de quem esta fora volta ao menu principal (o screenGetInput devolve o codigo 5). Start (Enter) com 2 ou mais jogadores confirma;
// os numeros escolhidos sao renumerados em ordem (1, 3, 4 viram 1, 2, 3). Depois disso o mapa porta -> dispositivos do
// wotm_pad.inc fica manual (wotm_pad::g_manual, g_manualDevs[jogador]) ate o jogo voltar ao menu principal ou ao "press start".
//   - 2 jogadores: como no retail, os itens "free-for-all / elimination / minigames" aparecem (fade dos objetos da tela).
//   - 3 e 4 (g_multi): so o free for all (modo 3) existe, sem IA: o gancho faz o que o item free-for-all faz (m_mode e
//     m_battleMode 3, g_levelList = PO_MPFreeForAll, initSelectSwitches) e vai direto a tela 15 ("first to"), pulando a 13 (CPUs).
//     A volta da 15 (que iria a 13) vem para a escolha de controles, e a volta desta vai ao menu principal.
//
// Selecao de monstros de 3 e 4 (tela 6, screenCharSelect2P, com g_multi): o screenCharSelect2P roda sem entrada (o filme do telao
// segue) e com os objetos da tela 6 e os paineis de traje escondidos; o telao mostra um quadro por jogador. Cada jogador, pelos
// dispositivos dele: esquerda/direita troca de monstro (as colunas destravadas, monsterSelectMode), cima/baixo de traje (os abertos:
// state 1 e taken 1), X confirma, triangulo desfaz (ou, sem ninguem pronto, volta a tela 15). Todos prontos: m_monsterSel[i] e
// m_costume[0..1] (os trajes dos jogadores 3 e 4 vao para o wotm_split4.inc, wotmMultiSetCostume), m_numPlayers N, m_numAIs 0, e
// changeScreen(6, 8) como o retail (selecao de fase).
//
// Thread do EE: ganchos de screenTransition (esconde os itens das telas 5 e 6 e o rotulo do menu, mede o fade), screenGameModes2P,
// screenCharSelect2P, screenGetInput (sem entrada do jogo durante as escolhas; injeta o "voltar"), changeScreen (as voltas acima) e
// screenMain/screenWaitForStart (desliga o mapa manual e o multijogador). Janela: renderUi desenha a escolha numa textura propria
// (fonte do jogo, icones desenhados) e drawOnTelao a projeta no plano do objeto do filme do telao (shell->m_movieScreen); o rotulo
// vai no retangulo do item.
// Teste sem tela: WOTM_PADSEL_SCRIPT="seg:dur:disp:acao;..." (disp 0..3 = controle, kb = teclado; acao left, right, up, down, ok,
// back, start).
#include "wotm_mods.hpp"   // skins de mod na lista de trajes
#include <chrono>
#include <random>

void wotmMultiSetCostume(int player, uint32_t costume);   // wotm_split4.inc: traje dos jogadores 3 e 4 (o Shell so tem lugar para 2)
bool wotmTextMenuOn();   // wotm_mainmenu.inc: menu principal em texto (sem o rotulo "multiplayer" daqui)

namespace wotm_padsel
{
constexpr int kKb = PS2_PAD_MAX_GAMEPADS;   // indice do teclado na lista de dispositivos
constexpr int kDevs = PS2_PAD_MAX_GAMEPADS + 1;
constexpr int kMaxPlayers = 4;
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr uint32_t kMovieScreen = kShell + 0x160;   // shell->m_movieScreen
constexpr uint32_t kShellSound = kShell + 0x2918;
constexpr uint32_t kMonsterSel = kShell + 0x2938, kCostume = kShell + 0x2BE0, kNumPlayers = kShell + 0x2BB4, kNumAIs = kShell + 0x2BB8,
                   kMode = kShell + 0x2C10, kBattleMode = kShell + 0x2B54, kSw5CC = kShell + 0x5CC, kCostumeBox = kShell + 0x88;
constexpr uint32_t kSlotSize = 0x2C;   // SelectSlot (monster_select.h): taken +0, state +4; 4 trajes por coluna
enum Action { Left, Right, Ok, Back, Start, Up, Down, L1, R1, Square, kActions };   // L1/R1: abas do painel do wotm_mainmenu.inc; Square: so no roteiro (previa da aba VIDEO)
enum Phase { Off, Config, Reveal, Leaving };
// Casas da barra de monstros alem das 10 colunas: monstro aleatorio e personalizado (por enquanto nao faz nada)
constexpr int kCellRandom = 11, kCellCustom = 12;

// Cores dos jogadores 1 a 4
constexpr Color kPlayerColors[kMaxPlayers] = {{230, 120, 50, 255}, {70, 140, 235, 255}, {90, 190, 80, 255}, {230, 200, 60, 255}};
// Nomes do MonsterNames.h (tipo = m_typeBits >> 5)
constexpr const char *kMonsterNames[17] = {"",        "CONGAR", "ROBO-47",  "TOGERA", "ULTRA-V", "PREYTOR", "SPIDER", "KINETICLOPS", "MAGMO",
                                           "RAPTROS", "AGAMO",  "ZORGULON", "OGRE",   "ASSBOSS", "JELLY",   "FINAL",  ""};
inline const char *monsterName(uint32_t sel) { return (sel >> 5) < 17 ? kMonsterNames[sel >> 5] : ""; }

// Estado que a janela desenha (copiado sob g_m)
struct View
{
    bool on = false;      // escolha de controles
    bool select = false;  // selecao de monstros
    float alpha = 0.f;    // da escolha inteira (fade de entrada e saida)
    bool present[kDevs] = {};
    int slot[kDevs] = {};   // 0 fora, 1..8 jogador
    int count = 0;          // dispositivos presentes
    int players = 0;        // jogadores distintos escolhidos
    uint32_t screenObj = 0;
    uint32_t mgObj[24] = {}, mgUsed[2] = {};   // tela 7: objetos dos retratos grandes e o em uso de cada jogador (publish)
    // selecao
    int selPlayers = 0;
    uint32_t selMonster[kMaxPlayers] = {};   // m_monsterSel (tipo << 5)
    int selCostume[kMaxPlayers] = {}, selCostumes[kMaxPlayers] = {};
    int selCostumeIdx[kMaxPlayers] = {};   // posicao do traje na lista da coluna (-1: fora dela)
    int selBase[kMaxPlayers] = {};         // traje do disco do enquadramento (o base da skin)
    int selModCount[kMaxPlayers] = {};     // skins de mod no fim da lista
    char selCostumeName[kMaxPlayers][40] = {};
    char selLabel[kMaxPlayers][8] = {};    // "P1", "CPU"
    int selColor[kMaxPlayers] = {};        // indice em kPlayerColors
    bool selWaiting[kMaxPlayers] = {};     // quadro da CPU antes de o jogador escolher o dele (modo 2)
    bool selNoCostume = false;             // eliminacao: sem escolha de traje
    bool selReady[kMaxPlayers] = {};
    int selCell[kMaxPlayers] = {};         // casa da barra de monstros: 1..10 coluna, kCellRandom, kCellCustom
    int barMon[10] = {};                   // monstro (tipo) de cada coluna da selecao
    bool barOpen[10] = {};                 // coluna destravada
    // rotulo do menu principal
    uint32_t labelObj[4] = {};   // objetos do switch do "2 player" em uso ([0] = 0: sem rotulo); o menor e o texto
    uint32_t labelRef[4] = {};   // objetos do "1 player" em uso (texto e brilho): o menor da a coluna e a altura das letras
    bool labelLit = false;
    float labelAlpha = 0.f;
    // transicao de tela com animacao de camera (changeScreen com anim != 0): progresso 0..1 no sentido em que ela toca
    bool transMoving = false;
    float transProg = 0.f;
};
static std::mutex g_m;
static View g_view;

// Thread do EE
static Phase g_phase = Off;
static float g_uiAlpha = 0.f, g_reveal = 0.f;
static bool g_transMoving = false;   // a camera do jogo anda na transicao em curso (transitionHook)
static float g_transProg = 0.f;
static int g_slot[kDevs] = {};
static bool g_prev[kDevs][kActions] = {};
static bool g_primed = false;
static int g_inject = 0;   // codigo para o proximo screenGetInput (5 = voltar)
static int g_multi = 0;    // 3 ou 4: jogadores da partida multijogador escolhida (0 = 1 ou 2 jogadores, retail)
static PS2Runtime::RecompiledFunction g_origTransition = nullptr, g_origModes2P = nullptr, g_origGetInput = nullptr, g_origMain = nullptr,
                                      g_origWaitStart = nullptr, g_origSelect2P = nullptr, g_origChangeScreen = nullptr;
static uint32_t g_transRa = 0, g_modesRa = 0, g_selectRa = 0, g_mainRa = 0, g_select1Ra = 0, g_soloInRa = 0;
static int g_soloInPad = 0;
static bool g_select1Pending = false, g_soloInPending = false;
static PS2Runtime::RecompiledFunction g_origSelect1P = nullptr;
static bool g_transPending = false, g_modesPending = false, g_selectPending = false, g_mainPending = false;

// Selecao de monstros (thread do EE)
struct Pick
{
    int col = 1;       // coluna da tela de selecao (1..10)
    int costume = 0;   // 0..3
    bool ready = false;
};
static bool g_selOn = false, g_selPrimed = false;
static bool g_selLeaving = false;   // escolha feita (ou volta): a tela 6 segue escondida ate o fim do fade de saida
static int g_pickFor = 0;           // g_multi das escolhas guardadas em g_pick (outro numero: cursores espalhados de novo)
static Pick g_pick[kMaxPlayers];
static bool g_selPrev[kMaxPlayers][kActions] = {};
static int g_selDone = 0;   // quadros desde que todos ficaram prontos
// Troca de tela pedida por uma escolha: feita na saida da funcao da tela (screenGameModes2P, screenCharSelect2P). O changeScreen so
// grava currScreen e screenFirstPass = 1; chamado antes da funcao original, ela rodava a "primeira passada" da tela nova no lugar
// dela (e a zerava), e a troca as vezes nao acontecia.
enum After { AfterNone, AfterMultiFfa, AfterSelectDone, AfterSelectBack };
static After g_after = AfterNone;

struct ScriptStep { double t0, dur; int dev, act; bool fired = false; };
constexpr int kScriptMouse = -2;   // `mouse` no roteiro: act = casa da barra sob o cursor (cellN) ou clique nela (100 + N, clickN)
static std::vector<ScriptStep> g_script;

static void parseScript()
{
    const char *e = std::getenv("WOTM_PADSEL_SCRIPT");
    if (!e) return;
    std::string s(e);
    static const char *const names[kActions] = {"left", "right", "ok", "back", "start", "up", "down", "l1", "r1", "square"};
    for (size_t pos = 0; pos < s.size();)
    {
        size_t end = s.find(';', pos);
        if (end == std::string::npos) end = s.size();
        const std::string item = s.substr(pos, end - pos);
        pos = end + 1;
        double t0 = 0, dur = 0;
        char dev[8] = {}, act[8] = {};
        if (std::sscanf(item.c_str(), "%lf:%lf:%7[^:]:%7s", &t0, &dur, dev, act) != 4) continue;
        int cell = 0;
        if (!std::strcmp(dev, "mouse") && (std::sscanf(act, "cell%d", &cell) == 1 || std::sscanf(act, "click%d", &cell) == 1))
        {
            g_script.push_back({t0, dur, kScriptMouse, (act[1] == 'l' ? 100 : 0) + cell});
            continue;
        }
        const int d = std::strcmp(dev, "kb") == 0 ? kKb : std::atoi(dev);
        int a = -1;
        for (int i = 0; i < kActions; ++i)
            if (!std::strcmp(act, names[i])) a = i;
        if (d >= 0 && d < kDevs && a >= 0) g_script.push_back({t0, dur, d, a});
    }
}

// Teclado sempre; controle conectado ou citado no roteiro de teste (controles de mentira para testar sem tela)
static bool present(int d)
{
    if (d == kKb || IsGamepadAvailable(d))
        return true;
    for (const ScriptStep &st : g_script)
        if (st.dev == d) return true;
    return false;
}

// Acoes do dispositivo agora. bound: pelo mapa de teclas e botoes do jogador (wotm_settings.hpp; os botoes do PS2 X, triangulo, Start,
// L1, R1, direcional e analogico), como o jogo ve; sem ele, os botoes fisicos do mapa padrao (o painel de opcoes, que nao pode
// depender de um mapa que ele mesmo muda).
static void readDevice(int d, bool out[kActions], bool bound = true)
{
    namespace st = wotm::settings;
    using wotm_pad::kbDown;
    if (d == kKb)
    {
        out[Left] = kbDown(st::BLeft) || kbDown(st::BStickLeft);
        out[Right] = kbDown(st::BRight) || kbDown(st::BStickRight);
        out[Up] = kbDown(st::BUp) || kbDown(st::BStickUp);
        out[Down] = kbDown(st::BDown) || kbDown(st::BStickDown);
        out[Ok] = kbDown(st::BCross);
        out[Start] = kbDown(st::BStart);
        out[Back] = kbDown(st::BTriangle) || kbDown(st::BSelect);
        out[L1] = kbDown(st::BL1);
        out[R1] = kbDown(st::BR1);
        // mouse (etapa 9, cursor solto): esquerdo confirma, direito volta, rodinha sobe e desce
        if (!wotm::mouse::g_captured.load(std::memory_order_relaxed))
        {
            out[Ok] = out[Ok] || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !wotm::mouse::claimed());   // na barra: mouseStep
            out[Back] = out[Back] || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
            out[Up] = out[Up] || wotm_pad::g_wheelHeld > 0;
            out[Down] = out[Down] || wotm_pad::g_wheelHeld < 0;
        }
    }
    else
    {
        const float lx = GetGamepadAxisMovement(d, GAMEPAD_AXIS_LEFT_X), ly = GetGamepadAxisMovement(d, GAMEPAD_AXIS_LEFT_Y);
        const int player = wotm_pad::playerOfPad(d);
        auto btn = [&](st::Bind b, int fixed) { return bound ? wotm_pad::padDown(d, b, player) : IsGamepadButtonDown(d, fixed); };
        out[Left] = btn(st::BLeft, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || lx < -0.6f;
        out[Right] = btn(st::BRight, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || lx > 0.6f;
        out[Up] = btn(st::BUp, GAMEPAD_BUTTON_LEFT_FACE_UP) || ly < -0.6f;
        out[Down] = btn(st::BDown, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ly > 0.6f;
        out[Ok] = btn(st::BCross, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        out[Start] = btn(st::BStart, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        out[Back] = btn(st::BTriangle, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        out[L1] = btn(st::BL1, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        out[R1] = btn(st::BR1, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    }
    const double now = GetTime();
    for (const ScriptStep &st : g_script)
        if (st.dev == d && now >= st.t0 && now < st.t0 + st.dur)
            out[st.act] = true;
}

// Funcao do jogo com ate 5 argumentos inteiros (a0..a3, t0); o contexto volta como estava
static void callGuest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, uint32_t fn, uint32_t a0, uint32_t a1 = 0, uint32_t a2 = 0,
                      uint32_t a3 = 0, uint32_t t0 = 0)
{
    const PS2Runtime::RecompiledFunction f = rt->lookupFunction(fn);
    if (!f) return;
    const R5900Context saved = *ctx;
    const uint32_t a[5] = {a0, a1, a2, a3, t0};
    for (int i = 0; i < 5; ++i)
        ctx->r[4 + i] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a[i])));
    ctx->r[31] = _mm_set_epi64x(0, 0);
    ctx->pc = fn;
    f(rdram, ctx, rt);
    *ctx = saved;
}
static void sfx(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, int sound)   // ShellSound::playShellSFXSound (2/4 cursor, 6 voltar, 8 ok)
{
    callGuest(rdram, ctx, rt, wotm::fn::playShellSFXSound__10ShellSoundi, kShellSound, static_cast<uint32_t>(sound));
}
static void changeScreen(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, int from, int to, int dir, int anim)
{
    callGuest(rdram, ctx, rt, wotm::fn::changeScreen__Fiiiib, uint32_t(from), uint32_t(to), uint32_t(dir), uint32_t(anim), 1u);
}

static uint32_t rd32(const uint8_t *rdram, uint32_t a)
{
    uint32_t v;
    std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 4);
    return v;
}
static void wr32(uint8_t *rdram, uint32_t a, uint32_t v) { std::memcpy(rdram + (a & 0x1FFFFFFu), &v, 4); }
static uint32_t menuObj(int screen, uint32_t i) { return kShell + 0x6DC + (uint32_t(screen) * 90u + i) * 4u; }   // shell->m_menuObj[s][i]

static void setItemsAlpha(uint8_t *rdram, int screen, float a)
{
    for (uint32_t i = 0; i < 90; ++i)
        if (const uint32_t o = rd32(rdram, menuObj(screen, i)) & 0x1FFFFFFFu)
            std::memcpy(rdram + o + 0x4C, &a, 4);
}
static float itemsAlpha(uint8_t *rdram, int screen)
{
    for (uint32_t i = 0; i < 90; ++i)
        if (const uint32_t o = rd32(rdram, menuObj(screen, i)) & 0x1FFFFFFFu)
        {
            float a;
            std::memcpy(&a, rdram + o + 0x4C, 4);
            return a;
        }
    return 0.f;
}

// ------------------------------------------------------------------ rotulo "MULTIPLAYER" do menu principal

// Objetos de um switch (filhos objeto, ou os objetos de um filho grupo): f(objeto, filho em uso?)
template <class F>
static void forSwitchObjects(uint8_t *rdram, uint32_t sw, F f)
{
    sw &= 0x1FFFFFFFu;
    if (sw < 0x1000u || (rdram[sw] & 0x3F) != 6)   // SWITCH_NODE
        return;
    const uint32_t kids = rdram[sw + 0xB], which = rdram[sw + 8];
    for (uint32_t k = 0; k < kids && k < 16; ++k)
    {
        const uint32_t c = rd32(rdram, sw + 0xC + 4 * k) & 0x1FFFFFFFu;
        if (c < 0x1000u)
            continue;
        const uint32_t op = rdram[c] & 0x3F;
        if (op == 0)   // OBJECT_NODE
            f(c, k == which);
        else if (op == 1)   // GROUP_NODE (_hiergroup): numKids (u16) +8, filhos +0x20
            for (uint32_t g = 0, n = std::min<uint32_t>(rdram[c + 8] | (rdram[c + 9] << 8), 16u); g < n; ++g)
                if (const uint32_t o = rd32(rdram, c + 0x20 + 4 * g) & 0x1FFFFFFFu; o >= 0x1000u && (rdram[o] & 0x3F) == 0)
                    f(o, k == which);
    }
}

// Na tela 1 (e no fade de saida dela): esconde o "2 player" e publica o objeto em uso, o aceso e o alfa do fade (o do "1 player")
static void hideTwoPlayerItem(uint8_t *rdram)
{
    if (wotmTextMenuOn())   // os itens sao do wotm_mainmenu.inc
        return;
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen), last = rd32(rdram, wotm::sym::lastScreen);
    const bool betweenOut = rd32(rdram, wotm::sym::betweenScreens) && last == 1;
    uint32_t obj[4] = {}, ref[4] = {};
    int nRef = 0, nObj = 0;
    float alpha = 0.f;
    if (cur == 1 || betweenOut)
    {
        forSwitchObjects(rdram, rd32(rdram, kShell + 0x25C + (1 * 10 + 1) * 4), [&](uint32_t o, bool used) {
            if (used && nRef < 4) { std::memcpy(&alpha, rdram + o + 0x4C, 4); ref[nRef++] = o; }
        });
        forSwitchObjects(rdram, rd32(rdram, kShell + 0x25C + (1 * 10 + 2) * 4), [&](uint32_t o, bool used) {
            const float zero = 0.f;
            std::memcpy(rdram + o + 0x4C, &zero, 4);
            if (used && nObj < 4) obj[nObj++] = o;
        });
    }
    const bool lit = rd32(rdram, wotm::sym::currentSelection + 4) == 2;   // currentSelection[1]
    std::lock_guard<std::mutex> lk(g_m);
    std::copy(obj, obj + 4, g_view.labelObj);
    std::copy(ref, ref + 4, g_view.labelRef);
    g_view.labelLit = lit;
    g_view.labelAlpha = std::clamp(alpha, 0.f, 1.f);
}

// ------------------------------------------------------------------ escolha de controles

static int distinctPlayers(int map[kMaxPlayers + 1])
{
    bool used[kMaxPlayers + 1] = {};
    for (int d = 0; d < kDevs; ++d)
        if (present(d) && g_slot[d] > 0) used[g_slot[d]] = true;
    int n = 0;
    for (int p = 1; p <= kMaxPlayers; ++p)
    {
        if (map) map[p] = used[p] ? ++n : 0;
        else n += used[p];
    }
    return n;
}

static void setManual(bool on, const int map[kMaxPlayers + 1] = nullptr)
{
    if (on)
    {
        uint32_t devs[kMaxPlayers] = {};
        for (int d = 0; d < kDevs; ++d)
            if (present(d) && g_slot[d] > 0 && map[g_slot[d]] > 0)
                devs[map[g_slot[d]] - 1] |= d == kKb ? PS2_PAD_DEV_KEYBOARD : 1u << d;
        for (int p = 0; p < kMaxPlayers; ++p)
            wotm_pad::g_manualDevs[p] = devs[p];
    }
    if (wotm_pad::g_manual.exchange(on) != on)
        std::fprintf(stderr, "[padsel] controles %s\n", on ? "escolhidos: mapa manual" : "desligados: mapa padrao");
}

// 3 e 4: o que o item free-for-all da tela 5 faz, e direto a tela 15 (sem a 13, das CPUs)
static void startMultiFreeForAll(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    wr32(rdram, kMode, 3u);
    wr32(rdram, kBattleMode, 3u);
    wr32(rdram, kNumAIs, 0u);
    wr32(rdram, wotm::sym::g_levelList, wotm::sym::PO_MPFreeForAll);
    callGuest(rdram, ctx, runtime, wotm::fn::hierSetSwitch__FP11_hierswitchi, rd32(rdram, kSw5CC), 0u);
    changeScreen(rdram, ctx, runtime, 5, 15, 1, 2);
    callGuest(rdram, ctx, runtime, wotm::fn::initSelectSwitches__Fv, 0u);
}

static void configStep(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    bool now[kDevs][kActions] = {};
    for (int d = 0; d < kDevs; ++d)
        if (present(d)) readDevice(d, now[d]);
    if (!g_primed)   // o X que escolheu o item ainda pode estar apertado: borda so a partir de agora
    {
        std::memcpy(g_prev, now, sizeof g_prev);
        g_primed = true;
        return;
    }
    int moved = 0;
    bool confirm = false, back = false;
    for (int d = 0; d < kDevs; ++d)
    {
        if (!present(d)) { g_slot[d] = 0; continue; }
        bool e[kActions];
        for (int a = 0; a < kActions; ++a)
            e[a] = now[d][a] && !g_prev[d][a];
        const int before = g_slot[d];
        if (g_slot[d] == 0)
        {
            if (e[Ok])
            {
                bool used[kMaxPlayers + 1] = {};
                for (int o = 0; o < kDevs; ++o)
                    if (present(o) && g_slot[o] > 0) used[g_slot[o]] = true;
                for (int p = 1; p <= kMaxPlayers && !g_slot[d]; ++p)
                    if (!used[p]) g_slot[d] = p;
                moved = 4;
            }
            else if (e[Back])
                back = true;
        }
        else
        {
            if (e[Left]) { g_slot[d] = g_slot[d] > 1 ? g_slot[d] - 1 : kMaxPlayers; moved = 2; }
            if (e[Right]) { g_slot[d] = g_slot[d] < kMaxPlayers ? g_slot[d] + 1 : 1; moved = 4; }
            if (e[Back]) { g_slot[d] = 0; moved = 2; }
            if (e[Start]) confirm = true;
        }
        if (g_slot[d] != before)
            std::fprintf(stderr, "[padsel] %s %d: jogador %d\n", d == kKb ? "teclado" : "controle", d == kKb ? 0 : d, g_slot[d]);
    }
    std::memcpy(g_prev, now, sizeof g_prev);
    if (back)
    {
        g_phase = Leaving;
        g_inject = 5;   // o screenGameModes2P faz o resto (som, m_numPlayers 0, changeScreen de volta)
        std::fprintf(stderr, "[padsel] voltar ao menu principal\n");
        return;
    }
    int map[kMaxPlayers + 1];
    const int n = distinctPlayers(map);
    if (confirm && n >= 2)
    {
        setManual(true, map);
        sfx(rdram, ctx, runtime, 8);
        for (int d = 0; d < kDevs; ++d)
            if (present(d) && g_slot[d] > 0)
                std::fprintf(stderr, "[padsel] jogador %d: %s %d\n", map[g_slot[d]], d == kKb ? "teclado" : "controle", d == kKb ? 0 : d);
        if (n == 2)
        {
            g_multi = 0;
            g_phase = Reveal;
            g_reveal = 0.f;
            return;
        }
        g_multi = n;
        g_phase = Leaving;
        std::fprintf(stderr, "[padsel] %d jogadores: free for all, sem CPU\n", n);
        g_after = AfterMultiFfa;   // startMultiFreeForAll na saida do screenGameModes2P
        return;
    }
    if (moved)
        sfx(rdram, ctx, runtime, moved);
}

// ------------------------------------------------------------------ selecao de monstros (3 e 4)

static bool columnOpen(uint8_t *rdram, int col)
{
    if (col < 1 || col > 10)
        return false;
    if (col == 1 || col == 10)   // a primeira e a ultima colunas destravam (initSelectSwitches)
        return rd32(rdram, wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4) * kSlotSize + 4) == 1u;
    return true;
}
static bool costumeOpen(uint8_t *rdram, int col, int c)
{
    const uint32_t s = wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4 + c) * kSlotSize;
    return rd32(rdram, s + 4) == 1u && rd32(rdram, s) == 1u;
}
static int firstCostume(uint8_t *rdram, int col)
{
    for (int c = 0; c < 4; ++c)
        if (costumeOpen(rdram, col, c)) return c;
    return 0;
}
static int monsterOfCol(uint8_t *rdram, int col)
{
    return col >= 1 && col <= 10 ? int(rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(col - 1)) >> 5) : 0;
}

// Trajes de uma coluna na selecao: os do disco abertos (state 1 e taken 1, como o isSlotOpen) e depois as skins de mod do monstro
// (mods/<mod>/skins/, sempre abertas), menos as que outro jogador pronto ja levou (`taken`: pares monstro, traje). `noMods`: so os
// do disco (eliminacao: traje sempre 0).
constexpr int kMaxCostumes = 64;
struct CostumeList
{
    int n = 0;
    int c[kMaxCostumes] = {};
    int find(int costume) const
    {
        for (int i = 0; i < n; ++i)
            if (c[i] == costume) return i;
        return -1;
    }
};
// Trajes da coluna: os do disco que o jogo deixa escolher (abertos e nao levados) e as skins de mod do monstro (sempre abertas, repetiveis)
static CostumeList costumeList(uint8_t *rdram, int col, bool noMods = false)
{
    CostumeList l;
    if (col < 1 || col > 10)   // aleatorio, personalizado
        return l;
    for (int c = 0; c < 4; ++c)
        if (costumeOpen(rdram, col, c)) l.c[l.n++] = c;
    if (noMods)
        return l;
    const int mon = monsterOfCol(rdram, col);
    for (const wotm::mods::Skin &s : wotm::mods::Registry::get().skins())
    {
        if (s.monster != mon || l.n >= kMaxCostumes) continue;
        l.c[l.n++] = s.index;   // skin de mod: aberta e livre para qualquer numero de jogadores
    }
    return l;
}
// Nome do traje: "COSTUME n" (os do disco) ou o da skin
static std::string costumeName(int monster, int costume)
{
    if (costume < 4)
        return "COSTUME " + std::to_string(costume + 1);
    const wotm::mods::Skin *s = wotm::mods::Registry::get().skinByIndex(monster, costume);
    std::string n = s ? s->name : "COSTUME " + std::to_string(costume + 1);
    for (char &ch : n) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return n;
}
// Traje do disco que da o enquadramento e o modelo de reserva de um traje de mod (o "base" da skin)
static int baseCostume(int monster, int costume)
{
    if (costume < 4)
        return costume;
    const wotm::mods::Skin *s = wotm::mods::Registry::get().skinByIndex(monster, costume);
    return s ? s->base : 0;
}
static int stepColumn(uint8_t *rdram, int col, int dir)
{
    for (int i = 0; i < 10; ++i)
    {
        col += dir;
        if (col < 1) col = 10;
        if (col > 10) col = 1;
        if (columnOpen(rdram, col)) return col;
    }
    return col;
}
// Casa vizinha na barra: as colunas abertas, depois o aleatorio e o personalizado, em laco
static int stepCell(uint8_t *rdram, int cell, int dir)
{
    for (int i = 0; i < kCellCustom; ++i)
    {
        cell += dir;
        if (cell < 1) cell = kCellCustom;
        if (cell > kCellCustom) cell = 1;
        if (cell > 10 || columnOpen(rdram, cell)) return cell;
    }
    return cell;
}
// Monstro aleatorio: uma coluna aberta e um traje da lista dela (skins de mod incluidas; so os do disco com `noMods`)
static void randomPick(uint8_t *rdram, int &col, int &costume, bool noMods)
{
    static std::mt19937 rng(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    int open[10], n = 0;
    for (int c = 1; c <= 10; ++c)
        if (columnOpen(rdram, c)) open[n++] = c;
    col = n ? open[rng() % uint32_t(n)] : 1;
    const CostumeList l = costumeList(rdram, col, noMods);
    costume = l.n ? l.c[rng() % uint32_t(l.n)] : firstCostume(rdram, col);
}

static void selectBegin(uint8_t *rdram)
{
    g_selOn = true;
    g_selLeaving = false;
    g_selPrimed = false;
    g_selDone = 0;
    // cursores espalhados pelas colunas abertas; o que ja foi escolhido (volta da selecao de fase, revanche) fica
    const bool keep = g_pickFor == g_multi;
    g_pickFor = g_multi;
    int col = stepColumn(rdram, 0, 1);
    for (int p = 0; p < g_multi; ++p)
    {
        if (!keep || !columnOpen(rdram, g_pick[p].col) || costumeList(rdram, g_pick[p].col).find(g_pick[p].costume) < 0)
        {
            g_pick[p].col = col;
            g_pick[p].costume = firstCostume(rdram, col);
        }
        col = stepColumn(rdram, col, 1);
        g_pick[p].ready = false;
    }
    std::fprintf(stderr, "[padsel] selecao de monstros de %d jogadores\n", g_multi);
}

static void selectFinish(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    for (int p = 0; p < g_multi; ++p)
    {
        const uint32_t sel = rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(g_pick[p].col - 1));
        wr32(rdram, kMonsterSel + 4u * uint32_t(p), sel);   // jogadores em m_monsterSel[0 .. N) (com 5 a 8 nao ha IA)
        if (p < 2)
            wr32(rdram, kCostume + 4u * uint32_t(p), uint32_t(g_pick[p].costume));
        else
            wotmMultiSetCostume(p, uint32_t(g_pick[p].costume));
        std::fprintf(stderr, "[padsel] jogador %d: %s traje %d\n", p + 1, monsterName(sel), g_pick[p].costume);
    }
    for (int i = g_multi; i < 12; ++i)
        wr32(rdram, kMonsterSel + 4u * uint32_t(i), 0u);
    wr32(rdram, kNumPlayers, uint32_t(g_multi));
    wr32(rdram, kNumAIs, 0u);
    g_selLeaving = true;
    sfx(rdram, ctx, runtime, 8);
    g_after = AfterSelectDone;   // changeScreen(6, 8) na saida do screenCharSelect2P
}

// Mouse na barra de monstros (cursor solto): a janela (drawBar2d) poe aqui a casa sob o cursor (1..12, so as abertas; 0 = nenhuma) e
// avisa (claim) que o clique esquerdo e dela; a thread do EE anda o quadro do jogador do teclado ate a casa, uma casa por quadro, quando
// o mouse anda ou clica, e o clique confirma ao chegar (no aleatorio, sorteia). Tecla ou botao derruba o alvo.
static std::atomic<int> g_mouseCell{0};
struct MouseGoal
{
    int cell = 0;
    bool confirm = false;
};
static MouseGoal g_mouseGoal;
static uint32_t g_mouseSeq = 0;
static void mouseDrop()
{
    g_mouseGoal = MouseGoal{};
    g_mouseSeq = wotm::mouse::g_moveSeq.load(std::memory_order_relaxed);
}
// Passo do mouse para o quadro na casa `cur`: 3 esquerda, 4 direita, 6 confirma, 0 nada
static int mouseStep(int cur)
{
    namespace ms = wotm::mouse;
    const uint32_t seq = ms::g_moveSeq.load(std::memory_order_relaxed);
    const bool moved = seq != g_mouseSeq;
    g_mouseSeq = seq;
    const int h = g_mouseCell.load(std::memory_order_relaxed);
    const bool click = h && ms::takeClick(ms::g_clickL);
    if (h && (moved || click))
        g_mouseGoal = MouseGoal{h, click || (g_mouseGoal.confirm && g_mouseGoal.cell == h)};
    if (!g_mouseGoal.cell)
        return 0;
    if (g_mouseGoal.cell != cur)
        return g_mouseGoal.cell > cur ? 4 : 3;
    const bool ok = g_mouseGoal.confirm;
    mouseDrop();
    return ok ? 6 : 0;
}

static void selectStep(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    bool now[kMaxPlayers][kActions] = {};
    for (int d = 0; d < kDevs; ++d)
    {
        if (!present(d))
            continue;
        const uint32_t bit = d == kKb ? PS2_PAD_DEV_KEYBOARD : 1u << d;
        bool a[kActions] = {};
        readDevice(d, a);
        for (int p = 0; p < g_multi; ++p)
            if (wotm_pad::g_manualDevs[p].load(std::memory_order_relaxed) & bit)
                for (int k = 0; k < kActions; ++k)
                    now[p][k] = now[p][k] || a[k];
    }
    if (!g_selPrimed)
    {
        std::memcpy(g_selPrev, now, sizeof g_selPrev);
        g_selPrimed = true;
        return;
    }
    int sound = 0;
    bool back = false;
    for (int p = 0; p < g_multi; ++p)
    {
        bool e[kActions];
        for (int a = 0; a < kActions; ++a)
            e[a] = now[p][a] && !g_selPrev[p][a];
        Pick &k = g_pick[p];
        const bool kb = (wotm_pad::g_manualDevs[p].load(std::memory_order_relaxed) & PS2_PAD_DEV_KEYBOARD) != 0;
        if (kb && (k.ready || e[Left] || e[Right] || e[Up] || e[Down] || e[Ok] || e[Back]))
            mouseDrop();
        else if (kb)
        {
            const int m = mouseStep(k.col);
            e[Left] = m == 3;
            e[Right] = m == 4;
            e[Ok] = m == 6;
        }
        if (k.ready)
        {
            if (e[Back]) { k.ready = false; sound = 6; }
            continue;
        }
        if (e[Left] || e[Right])
        {
            k.col = stepCell(rdram, k.col, e[Left] ? -1 : 1);
            k.costume = k.col <= 10 ? firstCostume(rdram, k.col) : 0;
            sound = e[Left] ? 2 : 4;
        }
        if (k.col > 10)   // aleatorio (X sorteia monstro e traje e fica pronto) ou personalizado (nada, por enquanto)
        {
            if (e[Ok] && k.col == kCellRandom)
            {
                randomPick(rdram, k.col, k.costume, false);
                k.ready = true;
                sound = 8;
                wotm_narrator::announce(monsterOfCol(rdram, k.col));
            }
            if (e[Back]) back = true;
            continue;
        }
        const CostumeList list = costumeList(rdram, k.col);
        if ((e[Up] || e[Down]) && list.n > 0)
        {
            const int dir = e[Up] ? -1 : 1, at = list.find(k.costume);
            k.costume = list.c[at < 0 ? 0 : (at + dir + list.n) % list.n];
            sound = dir < 0 ? 2 : 4;
        }
        if (e[Ok] && list.find(k.costume) >= 0)
        {
            k.ready = true;
            sound = 8;
            wotm_narrator::announce(monsterOfCol(rdram, k.col));
        }
        if (e[Back]) back = true;
    }
    std::memcpy(g_selPrev, now, sizeof g_selPrev);
    bool anyReady = false, allReady = true;
    for (int p = 0; p < g_multi; ++p)
    {
        anyReady = anyReady || g_pick[p].ready;
        allReady = allReady && g_pick[p].ready;
    }
    if (back && !anyReady)
    {
        g_selLeaving = true;
        sfx(rdram, ctx, runtime, 6);
        g_after = AfterSelectBack;   // changeScreen(6, 15) na saida do screenCharSelect2P
        std::fprintf(stderr, "[padsel] selecao: volta ao \"first to\"\n");
        return;
    }
    if (sound)
        sfx(rdram, ctx, runtime, sound);
    g_selDone = allReady ? g_selDone + 1 : 0;
    if (g_selDone > 30)
        selectFinish(rdram, ctx, runtime);
}

// ------------------------------------------------------------------ selecao de 1 e 2 jogadores (telas 3 e 6)
// A logica e a do jogo (screenCharSelect1P / screenCharSelect2P: modos, CPU, eliminacao, o que acontece ao confirmar); o nativo
// esconde a tela, mostra os quadros no telao como na selecao de 3 e 4 e cuida do traje: cima/baixo andam por uma lista com os trajes
// do disco abertos e as skins de mod. O cursor de traje do jogo fica no traje escolhido (0..3) ou, numa skin, no primeiro traje do
// disco aberto (o "procurador": o jogo confirma por ele); na confirmacao o m_costume do quadro recebe o traje da skin. Esquerda/direita
// passam, depois da ultima coluna aberta, pelo aleatorio e pelo personalizado (casas so nossas, g_soloExtra). A fileira de retratos
// do jogo da lugar a barra 2D (drawBar2d, hideStrip) e o telao fica preto com o sheldemo (selectMovie).
// WOTM_NATSEL=0 volta as telas do jogo.
static int g_solo = 0;   // 3, 6, 7, 18 ou 19: a tela do jogo que a selecao nativa mostra
static bool g_soloLeaving = false;

static bool soloEnabled()
{
    static const bool on = [] {
        const char *e = std::getenv("WOTM_NATSEL");
        return !(e && std::strcmp(e, "0") == 0);
    }();
    return on;
}

// Um quadro da selecao do jogo: onde a tela guarda a coluna, o cursor de traje e o "pronto", e para onde vai a escolha
struct SoloPanel
{
    uint32_t col, cur, done, costumeDst, monsterDst;
    int pad;        // screenGetInput(pad) que o controla
    bool ai;        // a CPU do modo 2 (o jogador 1 escolhe por ela)
    bool waiting;   // CPU antes de o jogador 1 escolher o dele
};
static int soloPanels(uint8_t *rdram, SoloPanel out[kMaxPlayers], bool &noCostume)
{
    using namespace wotm::sym;
    noCostume = false;
    if (g_solo == 3)
    {
        out[0] = {currentSelection + 4u * 3u, curCostume1_2833, monsterIsSelected_2829, kCostume, kMonsterSel, 1, false, false};
        return 1;
    }
    if (g_solo == 7)
    {
        // Tela 7 (screenMinigames2P): cada controle escolhe o seu; a carga dos minigames usa o m_costume de cada jogador
        out[0] = {currentSelection + 4u * 7u, curCostume1_2912, playerOneIsDone_2914, kCostume, kMonsterSel, 1, false, false};
        out[1] = {currentSelection + 4u * 29u, curCostume2_2913, playerTwoIsDone_2915, kCostume + 4u, kMonsterSel + 4u, 2, false, false};
        return 2;
    }
    const uint32_t mode = rd32(rdram, kMode);
    noCostume = mode == 6;   // eliminacao: o jogo poe o traje 0
    auto on = [&](uint32_t a) { return rd32(rdram, a) != 0; };
    if (g_solo == 18 || g_solo == 19)
    {
        // Telas 18 (screenCharSelect3P, 3 monstros) e 19 (screenCharSelect4P, 4), vindas do "first to" com CPUs. Modo 2 (1 jogador):
        // o controle 1 escolhe o dele e depois cada CPU, uma por vez, na mesma coluna. Senao (2 jogadores): cada controle escolhe o
        // seu; na 18 o controle 1 escolhe a CPU depois dos dois; na 19 o 1 escolhe a 1a CPU depois dos dois e o 2, a 2a depois do dele.
        const bool four = g_solo == 19;
        const uint32_t c1 = currentSelection + 4u * uint32_t(g_solo), c2 = currentSelection + 4u * uint32_t(22 + g_solo);
        const uint32_t cur[4] = {four ? curCostume1_3007 : curCostume1_2868, four ? curCostume2_3008 : curCostume2_2869,
                                 four ? curCostume3_3009 : curCostume3_2870, curCostume4_3010};
        const uint32_t p1 = four ? playerOneIsDone_3011 : playerOneIsDone_2871, p2 = four ? playerTwoIsDone_3012 : playerTwoIsDone_2872,
                       a1 = four ? aiOneIsDone_3013 : aiOneIsDone_2873, a2 = four ? aiTwoIsDone_3014 : aiTwoIsDone_2874,
                       a3 = aiThreeIsDone_3015;
        if (mode == 2)
        {
            const int n = four ? 4 : 3;
            const uint32_t done[4] = {p1, a1, a2, a3};
            out[0] = {c1, cur[0], p1, kCostume, kMonsterSel, 1, false, false};
            for (int i = 1; i < n; ++i)   // CPU i: m_costume[1 + i], m_monsterSel[3 + i]
                out[i] = {c1, cur[i], done[i], kCostume + 4u * uint32_t(1 + i), kMonsterSel + 4u * uint32_t(3 + i), 1, true, !on(done[i - 1])};
            return n;
        }
        out[0] = {c1, cur[0], p1, kCostume, kMonsterSel, 1, false, false};
        out[1] = {c2, cur[1], p2, kCostume + 4u, kMonsterSel + 4u, 2, false, false};
        out[2] = {c1, cur[2], a1, kCostume + 8u, kMonsterSel + 16u, 1, true, !(on(p1) && on(p2))};
        if (!four)
            return 3;
        out[3] = {c2, cur[3], a2, kCostume + 12u, kMonsterSel + 20u, 2, true, !on(p2)};
        return 4;
    }
    // currentSelection: a coluna do jogador 1 de cada tela, e a do jogador 2 a partir do indice 22
    out[0] = {currentSelection + 4u * 6u, curCostume1_2894, playerOneIsDone_2896, kCostume, kMonsterSel, 1, false, false};
    if (mode == 2)   // 1 jogador e 1 CPU: o controle 1 escolhe a CPU na mesma coluna, depois do dele
        out[1] = {currentSelection + 4u * 6u, curCostume2_2895, AIplayerIsDone_2898, kCostume + 8u, kMonsterSel + 16u, 1, true,
                  rd32(rdram, playerOneIsDone_2896) == 0};
    else
        out[1] = {currentSelection + 4u * 28u, curCostume2_2895, playerTwoIsDone_2897, kCostume + 4u, kMonsterSel + 4u, 2, false, false};
    return 2;
}
static int colOfMonster(uint8_t *rdram, uint32_t sel)
{
    for (int c = 1; c <= 10; ++c)
        if (monsterOfCol(rdram, c) == int(sel >> 5)) return c;
    return 0;
}
static int g_soloSel[kMaxPlayers] = {}, g_soloCol[kMaxPlayers] = {-1, -1, -1, -1};   // traje escolhido (0..3 ou skin) e a coluna em que foi escolhido
static bool g_soloDonePrev[kMaxPlayers] = {};
static bool g_soloPrimed = false;   // primeiro soloSync da tela: um "pronto" que ja vinha marcado nao chama o narrador
static uint32_t g_soloTick = 0;
// Casa extra em que o quadro esta (0: a coluna do jogo; kCellRandom, kCellCustom: o cursor do jogo fica parado na coluna de onde saiu)
static int g_soloExtra[kMaxPlayers] = {};
// Codigo do screenGetInput ja decidido neste quadro, por controle (a tela 3 le o controle duas vezes por quadro)
static uint32_t g_soloCodeTick[3] = {}, g_soloCodeIn[3] = {}, g_soloCodeOut[3] = {};

static void soloBegin(int screen)
{
    g_solo = screen;
    g_soloLeaving = false;
    g_soloPrimed = false;
    for (int i = 0; i < kMaxPlayers; ++i)
    {
        g_soloCol[i] = -1;
        g_soloDonePrev[i] = false;
        g_soloExtra[i] = 0;
    }
    std::fprintf(stderr, "[padsel] selecao nativa da tela %d\n", screen);
}

// Rotulo do quadro i: P1, P2, CPU (ou CPU1, CPU2... com mais de uma)
static const char *soloLabel(const SoloPanel *pn, int n, int i)
{
    static char buf[kMaxPlayers][8];
    if (!pn[i].ai)
        return pn[i].pad == 2 ? "P2" : "P1";
    int cpus = 0, k = 0;
    for (int j = 0; j < n; ++j)
        if (pn[j].ai)
            ++cpus, k += j < i;
    if (cpus == 1)
        return "CPU";
    std::snprintf(buf[i], sizeof buf[i], "CPU%d", k + 1);
    return buf[i];
}

// Saida da funcao da tela, a cada quadro: acompanha o cursor do jogo e, na confirmacao com uma skin, grava o traje dela
static void soloSync(uint8_t *rdram)
{
    SoloPanel pn[kMaxPlayers];
    bool noCostume;
    const int n = soloPanels(rdram, pn, noCostume);
    for (int i = 0; i < n; ++i)
    {
        if (pn[i].waiting)
        {
            g_soloCol[i] = -1;
            g_soloDonePrev[i] = false;
            continue;
        }
        const bool done = rd32(rdram, pn[i].done) != 0;
        const int col = int(rd32(rdram, pn[i].col)), cur = int(rd32(rdram, pn[i].cur));
        if (!done)
        {
            if (col != g_soloCol[i] || noCostume)
                g_soloSel[i] = cur, g_soloCol[i] = col;
            else if (g_soloSel[i] < 4 && g_soloSel[i] != cur)   // o jogo mexeu no cursor (troca de coluna, traje que deixou de valer)
                g_soloSel[i] = cur;
            else if (g_soloSel[i] >= 4 && costumeList(rdram, col).find(g_soloSel[i]) < 0)
                g_soloSel[i] = cur;
        }
        else if (!g_soloDonePrev[i] && g_soloSel[i] >= 4 && !noCostume && g_soloCol[i] == col)
        {
            wr32(rdram, pn[i].costumeDst, uint32_t(g_soloSel[i]));
            // o jogo marcou o procurador como levado (taken 0); ele continua livre para o outro quadro
            wr32(rdram, wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4 + cur) * kSlotSize, 1u);
            const int mon = monsterOfCol(rdram, col);
            std::fprintf(stderr, "[padsel] %s: %s traje %d (%s)\n", soloLabel(pn, n, i), kMonsterNames[mon],
                         g_soloSel[i], costumeName(mon, g_soloSel[i]).c_str());
        }
        if (done && !g_soloDonePrev[i])
        {
            std::fprintf(stderr, "[padsel] pronto (%s): coluna %d, m_monsterSel %s, traje %u\n", soloLabel(pn, n, i), col,
                         monsterName(rd32(rdram, pn[i].monsterDst)), rd32(rdram, pn[i].costumeDst));
            if (g_soloPrimed)
                wotm_narrator::announce(col >= 1 && col <= 10 ? monsterOfCol(rdram, col) : int(rd32(rdram, pn[i].monsterDst) >> 5));
        }
        g_soloDonePrev[i] = done;
    }
    g_soloPrimed = true;
}

// Poe o cursor do jogo do quadro na coluna `col` com o traje `sel` (0..3 ou skin; numa skin, o primeiro traje do disco aberto faz as
// vezes dela)
static void soloSetCursor(uint8_t *rdram, const SoloPanel &p, int who, int col, int sel)
{
    const CostumeList l = costumeList(rdram, col);
    wr32(rdram, p.col, uint32_t(col));
    wr32(rdram, p.cur, uint32_t(sel < 4 ? sel : (l.n ? l.c[0] : firstCostume(rdram, col))));
    g_soloCol[who] = col;
    g_soloSel[who] = sel;
}

// Decide o codigo de um toque no quadro que o controle esta escolhendo:
//  - cima/baixo (1/2) andam pela lista de trajes (o jogo recebe 0);
//  - esquerda/direita (3/4) andam pelas colunas do jogo e, depois da ultima aberta, pelo aleatorio e o personalizado (casas nossas:
//    o cursor do jogo fica parado; ao sair delas o nativo poe o cursor na primeira ou na ultima coluna);
//  - X (6) no aleatorio sorteia monstro e traje, poe o cursor do jogo neles e deixa o X seguir; no personalizado nao faz nada.
static uint32_t soloDecide(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, int pad, uint32_t code)
{
    SoloPanel pn[kMaxPlayers];
    bool noCostume;
    const int n = soloPanels(rdram, pn, noCostume);
    int who = -1;
    for (int i = 0; i < n && who < 0; ++i)
        if (pn[i].pad == pad && !pn[i].waiting && !rd32(rdram, pn[i].done))
            who = i;
    if (who < 0)
        return code;   // pronto: o jogo so toca o som
    const SoloPanel &p = pn[who];
    const int col = int(rd32(rdram, p.col));
    if (g_soloCol[who] != col)
        g_soloCol[who] = col, g_soloSel[who] = int(rd32(rdram, p.cur));
    int &ex = g_soloExtra[who];
    const int first = stepColumn(rdram, 0, 1), last = stepColumn(rdram, 11, -1);
    if (code == 3 || code == 4)
    {
        const int dir = code == 3 ? -1 : 1, from = ex ? ex : col, to = stepCell(rdram, from, dir);
        if (to <= 10 && from <= 10 && !(dir > 0 && from == last) && !(dir < 0 && from == first))
            return code;   // de coluna em coluna: o jogo anda (e toca o som)
        ex = to > 10 ? to : 0;
        if (to <= 10)
            soloSetCursor(rdram, p, who, to, firstCostume(rdram, to));   // volta das casas extras para a primeira ou a ultima coluna
        sfx(rdram, ctx, runtime, dir < 0 ? 2 : 4);
        return 0;
    }
    if (code == 6)
    {
        if (ex == kCellCustom)
            return 0;
        if (ex == kCellRandom)
        {
            int c, s;
            randomPick(rdram, c, s, noCostume);
            ex = 0;
            soloSetCursor(rdram, p, who, c, s);
            std::fprintf(stderr, "[padsel] aleatorio: %s traje %d\n", kMonsterNames[monsterOfCol(rdram, c)], s);
        }
        return code;
    }
    // cima/baixo
    if (ex || noCostume)
        return ex ? 0 : code;
    const CostumeList l = costumeList(rdram, col);
    if (l.n <= 1)
        return 0;
    const int at = l.find(g_soloSel[who]);
    soloSetCursor(rdram, p, who, col, l.c[at < 0 ? 0 : (at + (code == 1 ? -1 : 1) + l.n) % l.n]);
    sfx(rdram, ctx, runtime, code == 1 ? 2 : 4);
    return 0;
}

// screenGetInput(pad) nas telas 3 e 6 (a tela 3 le o controle duas vezes por quadro: a segunda leitura recebe a mesma decisao)
static uint32_t soloInput(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, int pad, uint32_t code)
{
    // mouse na barra: o controle do jogador do teclado parado vira o passo do mouse (uma vez por quadro; a segunda leitura repete)
    if (pad >= 1 && pad <= 2 && pad == wotm::mouse::g_kbPlayer.load(std::memory_order_relaxed) + 1)
    {
        static uint32_t s_tick = ~0u, s_code = 0;
        if (code != 0)
            mouseDrop();
        else if (s_tick == g_soloTick)
            code = s_code;
        else
        {
            SoloPanel pn[kMaxPlayers];
            bool noCostume;
            const int n = soloPanels(rdram, pn, noCostume);
            int who = -1;
            for (int i = 0; i < n && who < 0; ++i)
                if (pn[i].pad == pad && !pn[i].waiting && !rd32(rdram, pn[i].done))
                    who = i;
            if (who < 0)
                mouseDrop();
            else
                code = uint32_t(mouseStep(g_soloExtra[who] ? g_soloExtra[who] : int(rd32(rdram, pn[who].col))));
            s_tick = g_soloTick;
            s_code = code;
        }
    }
    if (code < 1 || code > 6 || code == 5 || pad < 1 || pad > 2)
        return code;
    if (g_soloCodeTick[pad] == g_soloTick && g_soloCodeIn[pad] == code)
        return g_soloCodeOut[pad];
    const uint32_t out = soloDecide(rdram, ctx, runtime, pad, code);
    g_soloCodeTick[pad] = g_soloTick;
    g_soloCodeIn[pad] = code;
    g_soloCodeOut[pad] = out;
    return out;
}

// Traje do quadro p na View (lista da coluna, nome, base)
static void fillCostume(View &v, int p, const CostumeList &l)
{
    const int mon = int(v.selMonster[p] >> 5);
    v.selCostumes[p] = l.n;
    v.selModCount[p] = 0;
    for (int i = 0; i < l.n; ++i)
        v.selModCount[p] += l.c[i] >= 4;
    v.selCostumeIdx[p] = l.find(v.selCostume[p]);
    v.selBase[p] = baseCostume(mon, v.selCostume[p]);
    std::snprintf(v.selCostumeName[p], sizeof v.selCostumeName[p], "%s", costumeName(mon, v.selCostume[p]).c_str());
}

static void publishSolo(uint8_t *rdram, View &v)
{
    SoloPanel pn[kMaxPlayers];
    bool noCostume;
    const int n = soloPanels(rdram, pn, noCostume);
    v.selPlayers = n;
    v.selNoCostume = noCostume;
    for (int i = 0; i < n; ++i)
    {
        const bool done = !pn[i].waiting && rd32(rdram, pn[i].done) != 0;
        v.selReady[i] = done;
        v.selWaiting[i] = pn[i].waiting;
        v.selColor[i] = pn[i].ai ? -1 : pn[i].pad - 1;
        std::snprintf(v.selLabel[i], sizeof v.selLabel[i], "%s", soloLabel(pn, n, i));
        int col;
        if (done)
        {
            v.selMonster[i] = rd32(rdram, pn[i].monsterDst);
            v.selCostume[i] = int(rd32(rdram, pn[i].costumeDst));
            col = colOfMonster(rdram, v.selMonster[i]);
        }
        else
        {
            col = int(rd32(rdram, pn[i].col));
            v.selMonster[i] = col >= 1 && col <= 10 ? rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(col - 1)) : 0u;
            v.selCostume[i] = g_soloCol[i] == col ? g_soloSel[i] : int(rd32(rdram, pn[i].cur));
        }
        v.selCell[i] = done ? col : g_soloExtra[i] ? g_soloExtra[i] : col;
        if (pn[i].waiting || v.selCell[i] > 10)
        {
            v.selMonster[i] = 0;
            col = 0;   // lista vazia
        }
        fillCostume(v, i, costumeList(rdram, col, noCostume));
    }
}

static void publish(uint8_t *rdram)
{
    std::lock_guard<std::mutex> lk(g_m);
    View &v = g_view;
    v.on = g_phase != Off;
    v.select = g_selOn || g_solo;
    v.alpha = std::min(1.f, std::max(0.f, g_uiAlpha));
    v.count = 0;
    for (int d = 0; d < kDevs; ++d)
    {
        v.present[d] = present(d);
        v.slot[d] = g_slot[d];
        v.count += v.present[d];
    }
    v.players = distinctPlayers(nullptr);
    v.screenObj = rd32(rdram, kMovieScreen) & 0x1FFFFFFFu;
    // minigames de 2 (tela 7, no fliperama, sem o telao): os retratos grandes dos dois jogadores (switches Shell+0x1EC e +0x1F0, um
    // filho por coluna, com o "player N") saem da cena e o plano da selecao e o deles (mgPlane)
    std::fill(std::begin(v.mgObj), std::end(v.mgObj), 0u);
    v.mgUsed[0] = v.mgUsed[1] = 0;
    if (g_solo == 7 && !g_selOn)
    {
        int k = 0;
        for (int p = 0; p < 2; ++p)
            forSwitchObjects(rdram, rd32(rdram, kShell + (p ? 0x1F0u : 0x1ECu)), [&](uint32_t o, bool used) {
                if (k < int(std::size(v.mgObj))) v.mgObj[k++] = o;
                if (used) v.mgUsed[p] = o;
            });
        if (v.mgUsed[0] && v.mgUsed[1])
            v.screenObj = v.mgUsed[0];
    }
    v.transMoving = g_transMoving && rd32(rdram, wotm::sym::betweenScreens) != 0;
    v.transProg = g_transProg;
    v.selPlayers = g_selOn ? g_multi : 0;
    v.selNoCostume = false;
    for (int c = 1; c <= 10; ++c)
    {
        v.barMon[c - 1] = monsterOfCol(rdram, c);
        v.barOpen[c - 1] = columnOpen(rdram, c);
    }
    for (int p = 0; p < v.selPlayers; ++p)
    {
        const int col = g_pick[p].col;
        v.selCell[p] = col;
        v.selMonster[p] = col <= 10 ? rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(col - 1)) : 0u;
        v.selCostume[p] = g_pick[p].costume;
        v.selReady[p] = g_pick[p].ready;
        v.selWaiting[p] = false;
        v.selColor[p] = p;
        std::snprintf(v.selLabel[p], sizeof v.selLabel[p], "P%d", p + 1);
        fillCostume(v, p, costumeList(rdram, g_pick[p].col));
    }
    if (g_solo && !g_selOn)
        publishSolo(rdram, v);
}

// ------------------------------------------------------------------ ganchos

// Tela escondida pela escolha em curso (5: controles; 6: selecao de monstros), ou -1
static int hiddenScreen()
{
    if (g_phase == Config || g_phase == Leaving)
        return 5;
    if (g_selOn)
        return 6;
    if (g_solo)
        return g_solo;
    return -1;
}

static void hideSelect2P(uint8_t *rdram, int screen = 6)
{
    setItemsAlpha(rdram, screen, 0.f);
    for (uint32_t i = 0; i < 2; ++i)   // m_costumeBoxCs, m_costumeBoxCs2: drawMe (+0xC)
        if (const uint32_t cs = rd32(rdram, kCostumeBox + 4 * i) & 0x1FFFFFFFu; cs >= 0x1000u)
            rdram[cs + 0xC] = 0;
    if (screen == 18 || screen == 19)
    {
        for (uint32_t i = 0; i < 3; ++i)   // m_cs7C[0..2]: os outros paineis de traje das telas de 3 e 4 monstros
            if (const uint32_t cs = rd32(rdram, kShell + 0x7C + 4 * i) & 0x1FFFFFFFu; cs >= 0x1000u)
                rdram[cs + 0xC] = 0;
        for (uint32_t off = 0x1C0; off <= 0x1D8; off += 4)   // cursores de monstro (m_monsterCursorSw[0..2] e os da tela 19 depois)
            if (const uint32_t sw = rd32(rdram, kShell + off) & 0x1FFFFFFFu; sw >= 0x1000u)
                rdram[sw + 8] = 0;
    }
    if (screen == 7)
    {
        for (uint32_t i = 0; i < 2; ++i)   // m_costumeBoxCsMG[0..1]: os paineis de traje da selecao dos minigames
            if (const uint32_t cs = rd32(rdram, kShell + 0xB0 + 4 * i) & 0x1FFFFFFFu; cs >= 0x1000u)
                rdram[cs + 0xC] = 0;
    }
    // marcas "1"/"2" dos jogadores do retail nos retratos (shell->m_selectSw[coluna], m_selectSwMG na tela 7: 1, 2 ou 3 = os dois;
    // 4 = travado, fica)
    const uint32_t marks = screen == 7 ? 0x194u : 0x168u;
    for (uint32_t i = 0; i < 10; ++i)
        if (const uint32_t sw = rd32(rdram, kShell + marks + 4 * i) & 0x1FFFFFFFu; sw >= 0x1000u && rdram[sw + 8] >= 1 && rdram[sw + 8] <= 3)
            rdram[sw + 8] = 0;
}

// Progresso (0..1, no sentido em que toca) da animacao de camera da transicao em curso, shell->m_screenAnim[currAnimationIndex]
// (_animHandle de 16 bytes, que as funcoes recebem por ponteiro em a0). -1 sem animacao.
static float transitionProgress(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t idx = rd32(rdram, wotm::sym::currAnimationIndex);
    if (idx == 0 || idx >= 12) return -1.f;
    const uint32_t h = kShell + 0x61C + 16u * idx;
    const PS2Runtime::RecompiledFunction fp = rt->lookupFunction(wotm::fn::animationGetCurrentPercent__FG11_animHandle),
                                         fd = rt->lookupFunction(wotm::fn::animationGetDirection__FG11_animHandle);
    if (!fp || !fd) return -1.f;
    const R5900Context saved = *ctx;
    auto call = [&](uint32_t fn, PS2Runtime::RecompiledFunction f) {
        ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(h));
        ctx->r[31] = _mm_set_epi64x(0, 0);
        ctx->pc = fn;
        f(rdram, ctx, rt);
    };
    call(wotm::fn::animationGetCurrentPercent__FG11_animHandle, fp);
    const float pct = ctx->f[0];
    call(wotm::fn::animationGetDirection__FG11_animHandle, fd);
    const bool fwd = GPR_U32(ctx, 2) != 0;
    *ctx = saved;
    if (!(pct >= 0.f && pct <= 1.f)) return -1.f;
    return fwd ? pct : 1.f - pct;
}

// screenTransition: entrada vinda do menu principal (ou da volta da tela 15 no multijogador) comeca a escolha de controles; a
// entrada na tela 6 com o multijogador comeca a selecao. Enquanto uma delas vale, os itens da tela ficam com alfa 0 e o alfa que a
// transicao lhes daria vira o da escolha.
static void transitionHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenTransition__Fb)
    {
        const int cur = static_cast<int>(rd32(rdram, wotm::sym::currScreen)), last = static_cast<int>(rd32(rdram, wotm::sym::lastScreen));
        if (rd32(rdram, wotm::sym::screenFirstPass) && cur == 5 && (last == 1 || (last == 15 && g_multi)))
        {
            g_phase = Config;
            g_primed = false;
            g_uiAlpha = 0.f;
            for (int d = 0; d < kDevs; ++d)   // a escolha anterior fica, sem os desplugados
                if (!present(d)) g_slot[d] = 0;
            g_multi = 0;
            setManual(false);
            std::fprintf(stderr, "[padsel] escolha de controles\n");
        }
        // de volta a tela 6 (do "first to" ou da selecao de fase): uma selecao anterior que ainda constava como saindo recomeca
        if (rd32(rdram, wotm::sym::screenFirstPass) && cur == 6 && g_multi && (!g_selOn || g_selLeaving))
            selectBegin(rdram);
        // telas 3 e 6 de 1 e 2 jogadores: a selecao do jogo vista no telao
        if (rd32(rdram, wotm::sym::screenFirstPass) && (cur == 3 || cur == 7 || cur == 18 || cur == 19 || (cur == 6 && !g_multi)) && soloEnabled() &&
            (g_solo != cur || g_soloLeaving))
            soloBegin(cur);
        g_transRa = GPR_U32(ctx, 31);
        g_transPending = true;
    }
    g_origTransition(rdram, ctx, runtime);
    if (!g_transPending || ctx->pc != g_transRa)
        return;
    g_transPending = false;
    {
        const float p = transitionProgress(rdram, ctx, runtime);
        g_transMoving = p >= 0.f;
        g_transProg = g_transMoving ? p : 0.f;
    }
    const int hidden = hiddenScreen();
    if (hidden >= 0)
    {
        // entrando: so na fase de fade-in (antes disso o alfa e o resto da tela anterior); saindo: o fade-out da tela
        const bool entering = (hidden == 5 && g_phase == Config) || (hidden == 6 && g_selOn && !g_selLeaving) ||
                              (g_solo && !g_selOn && hidden == g_solo && !g_soloLeaving);
        const bool fading = entering ? rd32(rdram, wotm::sym::fadingIn_2979) != 0 : rd32(rdram, wotm::sym::fadingOut_2980) != 0;
        const bool ended = rd32(rdram, wotm::sym::betweenScreens) == 0;
        g_uiAlpha = fading ? std::min(1.f, std::max(0.f, itemsAlpha(rdram, hidden))) : (entering && ended ? 1.f : 0.f);
        setItemsAlpha(rdram, hidden, 0.f);
        if (hidden == 6 || hidden == 3 || hidden == 7 || hidden == 18 || hidden == 19)
            hideSelect2P(rdram, hidden);
        if (g_phase == Leaving && ended)
            g_phase = Off;
        if (hidden == 6 && g_selOn && g_selLeaving && ended && rd32(rdram, wotm::sym::currScreen) != 6)
            g_selOn = g_selLeaving = false;
        if (!g_selOn && g_solo && hidden == g_solo && g_soloLeaving && ended && rd32(rdram, wotm::sym::currScreen) != uint32_t(g_solo))
            g_solo = 0, g_soloLeaving = false;
    }
    hideTwoPlayerItem(rdram);
    publish(rdram);
}

static void modes2PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenGameModes2P__Fv)
    {
        if (g_phase == Config && rd32(rdram, wotm::sym::betweenScreens) == 0)   // no meio da transicao o changeScreen nao vale
        {
            g_uiAlpha = 1.f;
            configStep(rdram, ctx, runtime);
        }
        else if (g_phase == Reveal)
        {
            g_reveal = std::min(1.f, g_reveal + 0.05f);
            g_uiAlpha = 1.f - g_reveal;
            if (g_reveal >= 1.f) g_phase = Off;
        }
        g_modesRa = GPR_U32(ctx, 31);
        g_modesPending = true;
    }
    g_origModes2P(rdram, ctx, runtime);
    if (!g_modesPending || ctx->pc != g_modesRa)
        return;
    g_modesPending = false;
    if (g_after == AfterMultiFfa)
    {
        g_after = AfterNone;
        startMultiFreeForAll(rdram, ctx, runtime);
    }
    if (g_phase == Config || g_phase == Leaving)
        setItemsAlpha(rdram, 5, 0.f);
    else if (g_phase == Reveal)
        setItemsAlpha(rdram, 5, g_reveal);
    else if (g_reveal > 0.f && g_reveal < 2.f)
    {
        setItemsAlpha(rdram, 5, 1.f);
        g_reveal = 2.f;
    }
    publish(rdram);
}

static void soloExit(uint8_t *rdram);

static void select2PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const bool entry = ctx->pc == wotm::fn::screenCharSelect2P__Fv;
    if (entry && g_selOn && !g_selLeaving && rd32(rdram, wotm::sym::betweenScreens) == 0)
    {
        g_uiAlpha = 1.f;
        selectStep(rdram, ctx, runtime);
    }
    if (entry && (g_selOn || g_multi || g_solo == 6))
    {
        g_selectRa = GPR_U32(ctx, 31);
        g_selectPending = true;
        ++g_soloTick;
    }
    g_origSelect2P(rdram, ctx, runtime);
    if (!g_selectPending || ctx->pc != g_selectRa)
        return;
    g_selectPending = false;
    if (g_after == AfterSelectDone || g_after == AfterSelectBack)
    {
        const bool done = g_after == AfterSelectDone;
        g_after = AfterNone;
        if (done)
            changeScreen(rdram, ctx, runtime, 6, 8, 1, 3);
        else
        {
            // como a volta do jogo: o filme do menu de volta ao telao. Sem o continueDecoding o "first to" (que chama Play a cada
            // quadro) encerrava o filme logo no primeiro quadro, e o telao ficava preto
            callGuest(rdram, ctx, runtime, wotm::fn::moviesReadyScreenObject__FP11_hierobjectii, rd32(rdram, kMovieScreen), 8, 7);
            callGuest(rdram, ctx, runtime, wotm::fn::Play__6CMoviebPc, wotm::sym::myMovie, 0, wotm::sym::MainMovie);
            wr32(rdram, wotm::sym::continueDecoding, 1);
            changeScreen(rdram, ctx, runtime, 6, 15, -1, 0);
        }
    }
    if (g_selOn)
        hideSelect2P(rdram);
    else if (g_solo == 6)
        soloExit(rdram);
    publish(rdram);
}

// Saida das telas 3 e 6 com a selecao nativa: segue o jogo, esconde a tela e, se ela trocou de tela, comeca a sair
static void soloExit(uint8_t *rdram)
{
    if (!g_soloLeaving)
    {
        g_uiAlpha = rd32(rdram, wotm::sym::betweenScreens) == 0 ? 1.f : g_uiAlpha;
        soloSync(rdram);
    }
    hideSelect2P(rdram, g_solo);
    if (rd32(rdram, wotm::sym::currScreen) != uint32_t(g_solo))
        g_soloLeaving = true;
}

static void select1PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenCharSelect1P__Fv && g_solo == 3)
    {
        g_select1Ra = GPR_U32(ctx, 31);
        g_select1Pending = true;
        ++g_soloTick;
    }
    g_origSelect1P(rdram, ctx, runtime);
    if (!g_select1Pending || ctx->pc != g_select1Ra)
        return;
    g_select1Pending = false;
    if (g_solo == 3)
        soloExit(rdram);
    publish(rdram);
}

// Telas 18 e 19 (3 e 4 monstros, com CPUs): a mesma selecao nativa das telas 3 e 6
static PS2Runtime::RecompiledFunction g_origSelect3P = nullptr, g_origSelect4P = nullptr;
static uint32_t g_selectNRa[2] = {};
static bool g_selectNPending[2] = {};
static void selectNHook(int k, uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const int screen = 18 + k;
    if (ctx->pc == (k ? wotm::fn::screenCharSelect4P__Fv : wotm::fn::screenCharSelect3P__Fv) && g_solo == screen)
    {
        g_selectNRa[k] = GPR_U32(ctx, 31);
        g_selectNPending[k] = true;
        ++g_soloTick;
    }
    (k ? g_origSelect4P : g_origSelect3P)(rdram, ctx, runtime);
    if (!g_selectNPending[k] || ctx->pc != g_selectNRa[k])
        return;
    g_selectNPending[k] = false;
    if (g_solo == screen)
        soloExit(rdram);
    publish(rdram);
}
// Tela 7 (minigames de 2): a mesma selecao nativa
static PS2Runtime::RecompiledFunction g_origMinigames2P = nullptr;
static uint32_t g_minigamesRa = 0;
static bool g_minigamesPending = false;
static void minigames2PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenMinigames2P__Fv && g_solo == 7)
    {
        g_minigamesRa = GPR_U32(ctx, 31);
        g_minigamesPending = true;
        ++g_soloTick;
    }
    g_origMinigames2P(rdram, ctx, runtime);
    if (!g_minigamesPending || ctx->pc != g_minigamesRa)
        return;
    g_minigamesPending = false;
    if (g_solo == 7)
        soloExit(rdram);
    publish(rdram);
}
static void select3PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { selectNHook(0, rdram, ctx, runtime); }
static void select4PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) { selectNHook(1, rdram, ctx, runtime); }

static void getInputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen);
    // a selecao de 3 e 4 terminou de sair: a tela seguinte (o "first to" ou a selecao de fase) ja le a entrada. O screenTransition nao
    // ve o fim da transicao (o betweenScreens zera fora dele) e a selecao ficava como "saindo" (na volta a tela 6 nao recomecava).
    if (ctx->pc == wotm::fn::screenGetInput__Fi && g_selOn && g_selLeaving && cur != 6 && rd32(rdram, wotm::sym::betweenScreens) == 0)
    {
        g_selOn = g_selLeaving = false;
        publish(rdram);
    }
    if (ctx->pc == wotm::fn::screenGetInput__Fi && (cur == 5 || cur == 6))
    {
        if (cur == 5 && g_inject)
        {
            ctx->r[2] = _mm_set_epi64x(0, g_inject);
            g_inject = 0;
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
        if ((cur == 5 && (g_phase == Config || g_phase == Reveal || g_phase == Leaving)) || (cur == 6 && (g_selOn || g_multi)))
        {
            ctx->r[2] = _mm_set_epi64x(0, 0);
            ctx->pc = GPR_U32(ctx, 31);
            return;
        }
    }
    // selecao nativa de 1 e 2 jogadores: a que terminou de sair; cima/baixo da que esta na tela
    if (ctx->pc == wotm::fn::screenGetInput__Fi && g_solo && !g_selOn && g_soloLeaving && cur != uint32_t(g_solo) &&
        rd32(rdram, wotm::sym::betweenScreens) == 0)
    {
        g_solo = 0;
        g_soloLeaving = false;
        publish(rdram);
    }
    if (ctx->pc == wotm::fn::screenGetInput__Fi && g_solo && !g_selOn && !g_soloLeaving && cur == uint32_t(g_solo))
    {
        g_soloInRa = GPR_U32(ctx, 31);
        g_soloInPad = static_cast<int>(GPR_U32(ctx, 4));
        g_soloInPending = true;
    }
    g_origGetInput(rdram, ctx, runtime);
    if (!g_soloInPending || ctx->pc != g_soloInRa)
        return;
    g_soloInPending = false;
    const uint32_t raw = GPR_U32(ctx, 2), nc = soloInput(rdram, ctx, runtime, g_soloInPad, raw & 0xFFFFu);
    if (nc != (raw & 0xFFFFu))
        ctx->r[2] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>((raw & ~0xFFFFu) | nc)));
}

// changeScreen(de, para, sentido, animacao, ...): no multijogador a volta da tela 15 (que iria a 13, as CPUs) vem para a escolha
// de controles (5), e a volta desta (prevScreen 15) vai ao menu principal
static void changeScreenHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::changeScreen__Fiiiib)
    {
        const int32_t from = static_cast<int32_t>(GPR_U32(ctx, 4)), to = static_cast<int32_t>(GPR_U32(ctx, 5)),
                      dir = static_cast<int32_t>(GPR_U32(ctx, 6));
        if (g_multi && from == 15 && to == 13)
            ctx->r[5] = _mm_set_epi64x(0, 5);
        else if (from == 5 && to == 15 && dir < 0)
            ctx->r[5] = _mm_set_epi64x(0, 1);
        else if (from == 8 && to == 99)   // selecao de fase confirmada (o jogo ja gravou o m_levelNum)
            wotm_narrator::announceLevel(int(rd32(rdram, kShell + 0x2BB0)));
        else if (from == 20 && to == 1)   // Start no titulo
            wotm_narrator::stopIntro();
    }
    g_origChangeScreen(rdram, ctx, runtime);
}

// Menu principal: a partida multijogador acabou de vez, os controles voltam ao mapa padrao; o rotulo do "2 player" a cada quadro
static void mainHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenMain__Fv)
    {
        if (wotm_pad::g_manual.load(std::memory_order_relaxed))
            setManual(false);
        g_multi = 0;
        g_selOn = false;
        g_mainRa = GPR_U32(ctx, 31);
        g_mainPending = true;
    }
    g_origMain(rdram, ctx, runtime);
    if (!g_mainPending || ctx->pc != g_mainRa)
        return;
    g_mainPending = false;
    hideTwoPlayerItem(rdram);
}
static void waitStartHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenWaitForStart__Fv)
    {
        if (wotm_pad::g_manual.load(std::memory_order_relaxed))
            setManual(false);
        g_multi = 0;
        g_selOn = false;
        wotm_narrator::announceIntro();   // a camera do titulo acabou de parar (so roda depois da transicao)
    }
    g_origWaitStart(rdram, ctx, runtime);
}

// ------------------------------------------------------------------ desenho (janela)

static Vector3 vadd(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vector3 vsub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vector3 vscale(Vector3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
static Vector3 vneg(Vector3 a) { return {-a.x, -a.y, -a.z}; }
static float vdot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float vlen(Vector3 a) { return std::sqrt(vdot(a, a)); }
static Vector3 vnorm(Vector3 a) { const float l = vlen(a); return l > 0.f ? vscale(a, 1.f / l) : a; }
static Vector3 vcross(Vector3 a, Vector3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

constexpr int kW = 1200, kH = 500;   // textura da escolha (2,4:1: cabe abaixo do logo do telao)
static RenderTexture2D g_rt{}, g_labelRt{};
static float g_pos[kDevs] = {};   // coluna desenhada de cada dispositivo (anda ate a do jogador)
static bool g_posInit = false;

// Rotulo "multiplayer" no estilo dos outros itens do telao: minusculas numa fonte condensada (Bahnschrift do Windows, senao Arial
// Negrito, senao a do raylib), com contorno escuro; aceso creme, apagado no laranja terroso dos itens apagados. O brilho do item
// aceso continua o do jogo (objeto a parte do switch). Textura do tamanho do texto (g_labelRt), refeita quando o estado muda.
static const char *const kLabelText = "multiplayer";
constexpr float kLabelFont = 112.f, kLabelPad = 10.f;
static Font g_labelFont{};
static bool g_labelFontTried = false;
static int g_labelState = -1;   // aceso (1) ou apagado (0) desenhado em g_labelRt

static void renderLabel(bool lit)
{
    if (!g_labelFontTried)
    {
        g_labelFontTried = true;
        for (const char *path : {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/arialbd.ttf"})
            if (FileExists(path))
            {
                g_labelFont = LoadFontEx(path, int(kLabelFont), nullptr, 0);
                if (g_labelFont.texture.id) break;
            }
        if (!g_labelFont.texture.id)
            g_labelFont = GetFontDefault();
        SetTextureFilter(g_labelFont.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (g_labelRt.id && g_labelState == int(lit))
        return;
    const Vector2 sz = MeasureTextEx(g_labelFont, kLabelText, kLabelFont, 0.f);
    if (g_labelRt.id == 0)
    {
        g_labelRt = LoadRenderTexture(int(sz.x + 2.f * kLabelPad), int(sz.y + 2.f * kLabelPad));
        SetTextureFilter(g_labelRt.texture, TEXTURE_FILTER_BILINEAR);
    }
    g_labelState = int(lit);
    const Color fill = lit ? Color{242, 236, 222, 255} : Color{148, 90, 54, 255};
    const Color edge = lit ? Color{96, 54, 24, 255} : Color{36, 18, 8, 255};
    BeginTextureMode(g_labelRt);
    ClearBackground(Color{edge.r, edge.g, edge.b, 0});
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    const Vector2 at{kLabelPad, kLabelPad};
    for (int dy = -4; dy <= 4; dy += 2)   // contorno: o texto escuro deslocado em volta
        for (int dx = -4; dx <= 4; dx += 2)
            if (dx || dy)
                DrawTextEx(g_labelFont, kLabelText, Vector2{at.x + float(dx), at.y + float(dy)}, kLabelFont, 0.f, edge);
    DrawTextEx(g_labelFont, kLabelText, at, kLabelFont, 0.f, fill);
    EndBlendMode();
    EndTextureMode();
}

// Texto com a fonte 0 do jogo (atlas do quadro, `tex0`), como o fontBuildPrim: glifos de tamanho fixo, `h` = altura em pixels da
// textura; cada letra tem 1,6 x a altura em unidades da tela do jogo (640 x 224), ou seja 0,8 x a altura na tela.
static void text(std::vector<wotm::TextQuad> &out, const wotm::Ram &ram, uint64_t tex0, int rw, int rh, const char *s, float cx, float cy, float h,
                 Color c, float spacing = 2.f)
{
    if (!tex0) return;
    const uint32_t f = wotm::FontText::kFonts;
    const float cellU = ram.u16(f) / 16.f, cellV = ram.u16(f + 2) / 16.f, gw = ram.u16(f + 4) / 16.f, gh = ram.u16(f + 6) / 16.f;
    if (gw <= 0.f || gh <= 0.f) return;
    const bool rtx = ram.u32(wotm::FontText::kAtlasMode) != 0;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const float sx = 640.f / gr.w, sy = 224.f / float(rh);   // pixel da textura -> unidade da tela do jogo
    const float hv = h * sy, wv = 1.6f * hv, adv = wv + spacing;
    const size_t n = std::strlen(s);
    float x = (cx - gr.x0) * sx - adv * float(n) * 0.5f;
    const float y = cy * sy - hv * 0.5f;
    for (size_t i = 0; i < n; ++i, x += adv)
    {
        int ch = static_cast<unsigned char>(s[i]);
        if (ch >= 'a' && ch <= 'z') ch -= 0x20;
        if (ch <= ' ') continue;
        int row, col;
        float base;
        if (rtx) { row = ch >> 4; col = ch & 15; base = 1.f; }
        else { const int a = ch - 0x21; row = a >> 3; col = a & 7; base = 0.5f; }
        wotm::TextQuad t{};
        t.u0 = cellU * col + base; t.v0 = cellV * row + base;
        t.u1 = t.u0 + gw; t.v1 = t.v0 + gh;
        t.x0 = x; t.y0 = y; t.x1 = x + wv; t.y1 = y + hv;
        t.r = c.r; t.g = c.g; t.b = c.b; t.a = c.a;
        t.tex0 = tex0;
        out.push_back(t);
    }
}
// Altura de texto que cabe em `w` pixels (cada letra 1,6 x a altura mais o espaco)
static float fitText(const char *s, float h, float w) { return std::min(h, w / (1.6f * float(std::max<size_t>(1, std::strlen(s))) + 0.1f)); }
static Color gsColor(Color c, float k = 0.5f)   // cor do jogador como cor do texto do GS (0x80 = 1,0)
{
    return Color{static_cast<unsigned char>(c.r * k), static_cast<unsigned char>(c.g * k), static_cast<unsigned char>(c.b * k), 0x80};
}

static void gamepadIcon(float cx, float cy, float s, Color body, Color line, Color dark)
{
    const float o = s * 0.07f;
    DrawCircleV(Vector2{cx - 0.62f * s, cy + 0.28f * s}, 0.42f * s + o, line);
    DrawCircleV(Vector2{cx + 0.62f * s, cy + 0.28f * s}, 0.42f * s + o, line);
    DrawRectangleRounded(Rectangle{cx - 1.f * s - o, cy - 0.45f * s - o, 2.f * s + 2 * o, 0.9f * s + 2 * o}, 0.6f, 12, line);
    DrawCircleV(Vector2{cx - 0.62f * s, cy + 0.28f * s}, 0.42f * s, body);
    DrawCircleV(Vector2{cx + 0.62f * s, cy + 0.28f * s}, 0.42f * s, body);
    DrawRectangleRounded(Rectangle{cx - 1.f * s, cy - 0.45f * s, 2.f * s, 0.9f * s}, 0.6f, 12, body);
    const float dx = cx - 0.58f * s, dy = cy - 0.05f * s, k = 0.11f * s, L = 0.36f * s;
    DrawRectangleRec(Rectangle{dx - L / 2, dy - k / 2, L, k}, dark);
    DrawRectangleRec(Rectangle{dx - k / 2, dy - L / 2, k, L}, dark);
    const float bx = cx + 0.58f * s, by = cy - 0.05f * s, r = 0.075f * s, d = 0.14f * s;
    DrawCircleV(Vector2{bx, by - d}, r, dark);
    DrawCircleV(Vector2{bx, by + d}, r, dark);
    DrawCircleV(Vector2{bx - d, by}, r, dark);
    DrawCircleV(Vector2{bx + d, by}, r, dark);
    DrawCircleV(Vector2{cx - 0.25f * s, cy + 0.25f * s}, 0.13f * s, dark);
    DrawCircleV(Vector2{cx + 0.25f * s, cy + 0.25f * s}, 0.13f * s, dark);
}

static void keyboardIcon(float cx, float cy, float s, Color body, Color line, Color dark)
{
    const float o = s * 0.07f, W = 2.3f * s, H = 0.95f * s;
    DrawRectangleRounded(Rectangle{cx - W / 2 - o, cy - H / 2 - o, W + 2 * o, H + 2 * o}, 0.25f, 8, line);
    DrawRectangleRounded(Rectangle{cx - W / 2, cy - H / 2, W, H}, 0.25f, 8, body);
    const float k = 0.2f * s, g = 0.06f * s;
    for (int row = 0; row < 3; ++row)
    {
        const int keys = 9 - (row == 2);
        const float rowW = keys * k + (keys - 1) * g, x0 = cx - rowW / 2, y = cy - H / 2 + 0.1f * s + row * (k + g);
        for (int i = 0; i < keys; ++i)
            DrawRectangleRec(Rectangle{x0 + i * (k + g), y, k, k}, dark);
    }
    DrawRectangleRec(Rectangle{cx - 0.65f * s, cy + H / 2 - 0.1f * s - k, 1.3f * s, k}, dark);
}

static constexpr Color kCream{240, 222, 180, 255}, kLine{25, 18, 12, 255}, kDark{60, 48, 38, 255};

// Escolha de controles: quatro colunas (jogadores) em cima e, embaixo, quem esta fora; cada dispositivo anda ate a coluna dele
static void drawJoin(const View &v, std::vector<wotm::TextQuad> &quads, const wotm::Ram &ram, uint64_t tex0, float dt)
{
    constexpr float W = kW, H = kH;
    const float colW = W * 0.96f / kMaxPlayers, left = W * 0.02f, boxTop = H * 0.02f, boxBottom = H * 0.70f;
    auto colX = [&](float slot) { return left + colW * (slot - 0.5f); };   // slot 1..8
    bool used[kMaxPlayers + 1] = {};
    for (int d = 0; d < kDevs; ++d)
        if (v.present[d] && v.slot[d] > 0) used[v.slot[d]] = true;
    for (int p = 1; p <= kMaxPlayers; ++p)
    {
        const Color c = kPlayerColors[p - 1];
        const Rectangle box{colX(float(p)) - colW * 0.46f, boxTop, colW * 0.92f, boxBottom - boxTop};
        DrawRectangleRounded(box, 0.12f, 8, Color{0, 0, 0, static_cast<unsigned char>(used[p] ? 140 : 80)});
        DrawRectangleRoundedLinesEx(box, 0.12f, 8, 4.f, Color{c.r, c.g, c.b, static_cast<unsigned char>(used[p] ? 230 : 90)});
        char label[8];
        std::snprintf(label, sizeof label, "P%d", p);
        text(quads, ram, tex0, kW, kH, label, colX(float(p)), H * 0.09f, H * 0.07f, gsColor(c, used[p] ? 0.5f : 0.3f));
    }
    // dispositivos: de fora numa fileira embaixo; dentro, empilhados na coluna do jogador
    int outCount = 0, outIdx = 0, inCol[kMaxPlayers + 1] = {};
    for (int d = 0; d < kDevs; ++d)
        if (v.present[d] && v.slot[d] == 0) ++outCount;
    for (int d = 0; d < kDevs; ++d)
    {
        if (!v.present[d]) continue;
        float tx, ty;
        const float s = std::min(colW * 0.22f, H * 0.075f);
        if (v.slot[d] > 0)
        {
            tx = colX(float(v.slot[d]));
            ty = H * 0.25f + float(inCol[v.slot[d]]++) * H * 0.16f;
        }
        else
        {
            tx = W * 0.5f + (float(outIdx++) - 0.5f * float(outCount - 1)) * W * 0.11f;
            ty = H * 0.80f;
        }
        const float target = tx;
        g_pos[d] = g_posInit ? g_pos[d] + (target - g_pos[d]) * std::min(1.f, dt * 14.f) : target;
        const float x = g_pos[d];
        const Color body = v.slot[d] > 0 ? kPlayerColors[v.slot[d] - 1] : kCream;
        if (d == kKb) keyboardIcon(x, ty, s * 0.92f, body, kLine, kDark);
        else gamepadIcon(x, ty, s, body, kLine, kDark);
        char label[24];
        if (d == kKb) std::snprintf(label, sizeof label, "KEYBOARD");
        else std::snprintf(label, sizeof label, "PAD %d", d + 1);
        text(quads, ram, tex0, kW, kH, label, x, ty + s * 1.05f, fitText(label, s * 0.5f, colW * 0.9f), Color{0x80, 0x7A, 0x6C, 0x80}, 1.f);
    }
    g_posInit = true;
    // botoes: na legenda de baixo, no lugar da do jogo (wotm_legend.inc), com os do mapa (readDevice)
    namespace st = wotm::settings;
    using wotm_legend::bind;
    if (v.count < 2)
        wotm_legend::post({{{}, {}, "CONNECT A CONTROLLER"}, {bind(st::BCross), {}, "JOIN"}, {bind(st::BTriangle), {}, "BACK"}}, v.alpha);
    else if (v.players >= 2)
        wotm_legend::post({{bind(st::BStart), {}, "CONTINUE"}, {bind(st::BLeft), bind(st::BRight), "SWAP"}, {bind(st::BTriangle), {}, "BACK"}},
                          v.alpha);
    else
        wotm_legend::post({{bind(st::BCross), {}, "JOIN"}, {bind(st::BLeft), bind(st::BRight), "SWAP"}, {bind(st::BTriangle), {}, "BACK"}}, v.alpha);
}

// Fonte da selecao: a Oswald embutida no executavel (recurso FONT_OSWALD do native/wotm.rc, a da tela de opcoes); sem ela, a do raylib
static Font g_uiFont{};
static bool g_uiFontTried = false;
static const Font &uiFont()
{
    if (!g_uiFontTried)
    {
        g_uiFontTried = true;
        void *mod = GetModuleHandleA(nullptr);
        void *res = FindResourceA(mod, "FONT_OSWALD", reinterpret_cast<const char *>(static_cast<uintptr_t>(10)));   // RT_RCDATA
        void *h = res ? LoadResource(mod, res) : nullptr;
        const unsigned char *data = h ? static_cast<const unsigned char *>(LockResource(h)) : nullptr;
        const int size = res ? static_cast<int>(SizeofResource(mod, res)) : 0;
        if (data && size > 0)
            g_uiFont = LoadFontFromMemory(".ttf", data, size, 128, nullptr, 0);
        if (g_uiFont.texture.id)
        {
            GenTextureMipmaps(&g_uiFont.texture);
            SetTextureFilter(g_uiFont.texture, TEXTURE_FILTER_TRILINEAR);
        }
        else
        {
            std::fprintf(stderr, "[padsel] fonte FONT_OSWALD nao esta no executavel: selecao com a fonte do raylib\n");
            g_uiFont = GetFontDefault();
        }
    }
    return g_uiFont;
}
// Texto centrado em (cx, cy) com contorno escuro, encolhido para caber em `maxW`
static void outlined(const char *s, float cx, float cy, float size, float maxW, Color fill, float alpha = 1.f)
{
    const Font &f = uiFont();
    Vector2 sz = MeasureTextEx(f, s, size, size * 0.04f);
    if (sz.x > maxW && sz.x > 0.f)
    {
        size *= maxW / sz.x;
        sz = MeasureTextEx(f, s, size, size * 0.04f);
    }
    const Vector2 at{cx - sz.x * 0.5f, cy - sz.y * 0.5f};
    const unsigned char a = static_cast<unsigned char>(255.f * std::clamp(alpha, 0.f, 1.f));
    const float o = std::max(2.f, size * 0.05f);
    for (int k = 0; k < 8; ++k)
    {
        const float ang = float(k) * 0.785398f;
        DrawTextEx(f, s, Vector2{at.x + std::cos(ang) * o, at.y + std::sin(ang) * o}, size, size * 0.04f, Color{12, 8, 4, a});
    }
    DrawTextEx(f, s, at, size, size * 0.04f, Color{fill.r, fill.g, fill.b, a});
}

// Monstro 3D de cada jogador na selecao (o mesmo desenho da lista de saves: wotm_model.hpp, enquadramento do wotm_savepics.hpp), no
// RenderTexture do proprio Renderer, animado no idle desde a ultima troca. Os modelos ficam guardados por monstro e traje (trocar de
// volta nao rele o disco).
struct SelModel
{
    wotm::model::Renderer ren;
    uint32_t id = ~0u;   // monstro | traje << 8
    double t0 = 0.0;
    bool ready = false;
};
static SelModel g_selModel[kMaxPlayers];
static std::map<uint32_t, std::unique_ptr<wotm::model::Model>> g_selCache;
static wotm::model::FxTextures g_selFx;
static bool g_selFxTried = false;
constexpr int kSelPW = 1800, kSelPH = 1100;   // textura da selecao: o telao inteiro (~1,64:1), visto de frente (frameCamera)
static RenderTexture2D g_selRt{};

// Chave do cache: monstro | traje << 8 | traje do disco de reserva (o base da skin) << 16
static const wotm::model::Model *selModel(uint32_t id)
{
    if (auto it = g_selCache.find(id); it != g_selCache.end())
        return it->second.get();
    static const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    const char *pre = wotm::savepics::monsterFile(id & 0xFF);
    const uint32_t costume = (id >> 8) & 0xFF, base = (id >> 16) & 3;
    std::unique_ptr<wotm::model::Model> m;
    if (pre)
    {
        m = std::make_unique<wotm::model::Model>();
        const std::string iso = io.cdImage.string();
        // traje de mod: MON/<PREFIXO><n>.* vem de mods/ (ou do traje base, wotm_mods.hpp)
        if (m->load(io.cdRoot, iso, pre + std::to_string(costume)) || m->load(io.cdRoot, iso, pre + std::to_string(base)) ||
            m->load(io.cdRoot, iso, std::string(pre) + "0"))
            m->computeFraming(wotm::model::kIdle);   // caixa fixa: a camera nao acompanha o idle
        else
        {
            std::fprintf(stderr, "[padsel] MON/%s%u.NGP nao achado; sem monstro 3D\n", pre, costume);
            m.reset();
        }
    }
    return (g_selCache[id] = std::move(m)).get();
}

// Largura e posicao dos quadros na textura da selecao: lado a lado; um so (1 jogador) no meio, sem ocupar o telao inteiro
static float selBoxW(int n)
{
    const float pad = float(kSelPW) * 0.012f;
    const float w = (float(kSelPW) - pad * float(n + 1)) / float(n);
    return n == 1 ? std::min(w, float(kSelPW) * 0.46f) : w;
}
static float selBoxX(int n, int p)
{
    const float pad = float(kSelPW) * 0.012f, w = selBoxW(n);
    if (n == 1)
        return (float(kSelPW) - w) * 0.5f;
    return pad + float(p) * (w + pad);
}

// Antes do BeginTextureMode da selecao: o monstro de cada jogador no RenderTexture dele (w x h)
static void renderSelModels(const View &v, int w, int h)
{
    if (!g_selFxTried)
    {
        g_selFxTried = true;
        static const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
        g_selFx.load(io.cdRoot, io.cdImage.string());
    }
    const double now = GetTime();
    for (int p = 0; p < kMaxPlayers; ++p)
    {
        SelModel &s = g_selModel[p];
        s.ready = false;
        if (p >= v.selPlayers || v.selWaiting[p] || (v.selMonster[p] >> 5) == 0)
            continue;
        const uint32_t mon = v.selMonster[p] >> 5;
        const uint32_t id = mon | uint32_t(v.selCostume[p] & 0xFF) << 8 | uint32_t(v.selBase[p] & 3) << 16;
        if (id != s.id)
        {
            s.id = id;
            s.t0 = now;
        }
        const wotm::model::Model *m = selModel(id);
        if (!m)
            continue;
        auto *mm = const_cast<wotm::model::Model *>(m);
        const float frame = float((now - s.t0) * 60.0);
        if (!mm->pose(wotm::model::kIdle, frame))
            mm->pose(-1, 0.f);
        using D = wotm::savepics::Drawer;
        const D::Framing fr = D::framingOf(mon | uint32_t(v.selBase[p] & 3) << 8);   // enquadramento do traje do disco
        s.ren.setFx(&g_selFx);
        s.ren.shiftX = fr.shiftX;
        s.ren.shiftY = fr.shiftY;
        // de frente, virado um pouco para o centro da tela (os da esquerda para a direita e vice-versa)
        const float yaw = v.selPlayers > 1 ? 30.f - 60.f * float(p) / float(v.selPlayers - 1) : 0.f;
        s.ren.render(*m, w, h, yaw, 8.f, fr.zoom * 0.95f, [] {});
        s.ready = true;
    }
}

// Selecao de monstros: um quadro por jogador (ou CPU), lado a lado, no telao. Cada quadro: o rotulo em cima, o monstro 3D, o nome com
// as setas, o nome do traje e a faixa de trajes (um segmento por traje; os de mod em dourado; o escolhido mais largo e aceso) e o
// "READY".
static void drawSelect(const View &v)
{
    constexpr float W = kSelPW, H = kSelPH;
    const int n = std::max(1, v.selPlayers);
    const float top = H * 0.02f, bottom = H * 0.88f;
    const float boxW = selBoxW(n), boxH = bottom - top;
    const Color kName{246, 238, 220, 255}, kGold{236, 190, 70, 255}, kCpu{150, 150, 160, 255};
    for (int p = 0; p < v.selPlayers; ++p)
    {
        const Color c = v.selColor[p] >= 0 ? kPlayerColors[v.selColor[p] % kMaxPlayers] : kCpu;
        const float x = selBoxX(n, p), y = top, cx = x + boxW * 0.5f;
        const Rectangle box{x, y, boxW, boxH};
        DrawRectangleRounded(box, 0.06f, 8, Color{0, 0, 0, 200});
        DrawRectangleRounded(Rectangle{x, y, boxW, boxH * 0.12f}, 0.4f, 8, Color{c.r, c.g, c.b, 220});
        DrawRectangleRoundedLinesEx(box, 0.06f, 8, v.selReady[p] ? 10.f : 5.f, Color{c.r, c.g, c.b, 240});
        outlined(v.selLabel[p], cx, y + boxH * 0.06f, boxH * 0.10f, boxW * 0.9f, kName);
        if (v.selWaiting[p])   // CPU do modo 2: o jogador 1 escolhe por ela depois do dele
        {
            outlined("?", cx, y + boxH * 0.42f, boxH * 0.30f, boxW * 0.8f, Color{c.r, c.g, c.b, 255});
            outlined("WAITING", cx, y + boxH * 0.79f, boxH * 0.07f, boxW * 0.8f, kName);
            continue;
        }
        if (v.selCell[p] > 10 && !v.selReady[p])   // aleatorio ou personalizado
        {
            const bool rnd = v.selCell[p] == kCellRandom;
            outlined(rnd ? "?" : "+", cx, y + boxH * 0.42f, boxH * 0.34f, boxW * 0.8f, Color{c.r, c.g, c.b, 255});
            const float nameY = y + boxH * 0.76f;
            outlined(rnd ? "RANDOM" : "CUSTOM", cx, nameY, boxH * 0.085f, boxW * 0.72f, kName);
            outlined("<", x + boxW * 0.08f, nameY, boxH * 0.09f, boxW, c);
            outlined(">", x + boxW * 0.92f, nameY, boxH * 0.09f, boxW, c);
            outlined(rnd ? "MONSTER AND COSTUME AT RANDOM" : "COMING SOON", cx, y + boxH * 0.855f, boxH * 0.05f, boxW * 0.86f,
                     Color{205, 195, 175, 255});
            continue;
        }
        // monstro: do topo do quadro ate o nome
        const SelModel &s = g_selModel[p];
        if (s.ready)
        {
            const Texture2D &mt = s.ren.target().texture;
            const Rectangle dst{x + boxW * 0.03f, y + boxH * 0.13f, boxW * 0.94f, boxH * 0.58f};
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);   // cor pre-multiplicada (Renderer)
            DrawTexturePro(mt, Rectangle{0.f, 0.f, float(mt.width), -float(mt.height)}, dst, Vector2{0.f, 0.f}, 0.f, WHITE);
            EndBlendMode();
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        }
        const char *name = monsterName(v.selMonster[p]);
        const float nameY = y + boxH * (v.selNoCostume ? 0.82f : 0.76f);
        outlined(name, cx, nameY, boxH * 0.085f, boxW * (v.selReady[p] ? 0.94f : 0.72f), kName);
        if (!v.selReady[p])
        {
            outlined("<", x + boxW * 0.08f, nameY, boxH * 0.09f, boxW, c);
            outlined(">", x + boxW * 0.92f, nameY, boxH * 0.09f, boxW, c);
        }
        if (!v.selNoCostume)
        {
            // traje: o nome e a faixa (um segmento por traje da lista; os de mod em dourado)
            const bool mod = v.selCostume[p] >= 4;
            outlined(v.selCostumeName[p], cx, y + boxH * 0.855f, boxH * 0.05f, boxW * 0.86f, mod ? kGold : Color{205, 195, 175, 255});
            const int cnt = std::max(1, v.selCostumes[p]);
            if (cnt > 1 && !v.selReady[p])
            {
                // faixa vertical na beira direita do quadro, um segmento por traje de cima para baixo (cima/baixo trocam). Com muitos
                // trajes os segmentos afinam ate um minimo; dai em diante a faixa mostra uma janela em volta do escolhido, com setas
                // nas pontas indicando que ha mais. Embaixo, "n/total".
                const float tx = x + boxW * 0.925f, top = y + boxH * 0.17f, bot = y + boxH * 0.68f, avail = bot - top;
                const float gap = boxH * 0.007f, want = boxH * 0.045f, minH = boxH * 0.012f, segW = boxW * 0.028f;
                const int sel = std::max(0, v.selCostumeIdx[p]);
                int first = 0, shown = cnt;
                float segH = std::min(want, avail / float(cnt) - gap);
                const float arrow = boxH * 0.03f;
                if (segH < minH)
                {
                    segH = minH;
                    shown = std::max(3, int((avail - 2.f * arrow) / (minH + gap)));
                    first = std::clamp(sel - shown / 2, 0, cnt - shown);
                }
                const float used = float(shown) * (segH + gap) - gap;
                float sy = top + (avail - used) * 0.5f;
                const Color dim{90, 80, 70, 255}, modDim{120, 96, 40, 255};
                if (first > 0)
                    DrawTriangle(Vector2{tx, sy - arrow * 1.1f}, Vector2{tx - arrow * 0.6f, sy - arrow * 0.25f},
                                 Vector2{tx + arrow * 0.6f, sy - arrow * 0.25f}, kName);
                for (int k = first; k < first + shown; ++k, sy += segH + gap)
                {
                    const bool on = k == sel;
                    const float w = on ? segW * 1.8f : segW;
                    const Rectangle seg{tx - w * 0.5f, sy, w, segH};
                    DrawRectangleRounded(Rectangle{seg.x - 3.f, seg.y - 3.f, seg.width + 6.f, seg.height + 6.f}, 0.5f, 4, Color{12, 8, 4, 255});
                    const bool modSeg = k >= cnt - v.selModCount[p];   // os de mod vem depois dos do disco na lista
                    DrawRectangleRounded(seg, 0.5f, 4, on ? (mod ? kGold : kName) : modSeg ? modDim : dim);
                }
                if (first + shown < cnt)
                    DrawTriangle(Vector2{tx - arrow * 0.6f, sy + arrow * 0.25f - gap}, Vector2{tx, sy + arrow * 1.1f - gap},
                                 Vector2{tx + arrow * 0.6f, sy + arrow * 0.25f - gap}, kName);
                char num[16];
                std::snprintf(num, sizeof num, "%d/%d", sel + 1, cnt);
                outlined(num, tx, bot + boxH * 0.04f, boxH * 0.035f, boxW * 0.12f, Color{205, 195, 175, 255});
            }
        }
        if (v.selReady[p])
        {
            const Rectangle band{x + boxW * 0.08f, y + boxH * 0.40f, boxW * 0.84f, boxH * 0.13f};
            DrawRectangleRounded(band, 0.4f, 6, Color{c.r, c.g, c.b, 235});
            outlined("READY", cx, band.y + band.height * 0.5f, band.height * 0.75f, band.width * 0.9f, kName);
        }
    }
    bool all = v.selPlayers > 0;
    for (int p = 0; p < v.selPlayers; ++p)
        all = all && v.selReady[p];
    namespace st = wotm::settings;
    using wotm_legend::bind;
    if (all)
        outlined("GET READY", W * 0.5f, H * 0.94f, H * 0.07f, W * 0.9f, Color{246, 238, 220, 255});
    if (v.selNoCostume)
        wotm_legend::post({{bind(st::BLeft), bind(st::BRight), "MONSTER"}, {bind(st::BCross), {}, "READY"}, {bind(st::BTriangle), {}, "BACK"}},
                          v.alpha);
    else
        wotm_legend::post({{bind(st::BLeft), bind(st::BRight), "MONSTER"},
                           {bind(st::BUp), bind(st::BDown), "COSTUME"},
                           {bind(st::BCross), {}, "READY"},
                           {bind(st::BTriangle), {}, "BACK"}},
                          v.alpha);
}

// ------------------------------------------------------------------ barra de monstros (2D, acima da faixa preta de baixo)
// Os retratos das colunas sao os da fileira do jogo (que fica escondida, hideStrip): texturas 64x64 do SHELL/SHELL.RTX (T8H) e do
// SHELL.TEX (T8), com as paletas CT32 do .RTX (16x16; 8x2 nas silhuetas T4 dos travados). Lidos do disco uma vez, como as fotos da
// lista de saves (wotm_savepics.hpp). Mapa conferido exportando com o tools/tex.py e no objeto de cada retrato na cena.
struct PortraitTex
{
    int monster;            // tipo (kMonsterNames)
    uint16_t texId, palTbp; // retrato
    bool rtx;               // no .RTX (senao no .TEX)
    uint16_t lockId, lockPal; // silhueta do travado (0: nao tem)
};
constexpr PortraitTex kPortraits[] = {
    {1, 67, 201, true, 0, 0},     // Congar
    {2, 64, 189, true, 0, 0},     // Robo-47
    {3, 65, 193, true, 0, 0},     // Togera
    {4, 68, 205, false, 0, 0},    // Ultra-V
    {5, 66, 197, true, 0, 0},     // Preytor
    {7, 61, 177, false, 0, 0},    // Kineticlops
    {8, 63, 185, true, 0, 0},     // Magmo
    {9, 59, 172, true, 60, 176},  // Raptros
    {10, 62, 181, true, 0, 0},    // Agamo
    {11, 57, 167, true, 58, 171}, // Zorgulon
};
static Texture2D g_portrait[17]{}, g_portraitLock[17]{};
static bool g_portraitsTried = false;

// Uma textura 64x64 paletizada (T8/T8H com CLUT 16x16 em CSM1, T4HL/T4HH com CLUT 8x2) de cima para baixo (no arquivo as linhas estao
// de baixo para cima)
static Texture2D decodePortrait(const std::vector<uint8_t> &file, const std::vector<wotm::savepics::detail::Packet> &pk,
                                const std::vector<uint8_t> &rtx, const std::vector<wotm::savepics::detail::Packet> &rtxPk, uint16_t texId,
                                uint16_t palTbp)
{
    const wotm::savepics::detail::Packet *p = nullptr, *pal = nullptr;
    for (const auto &k : pk)
        if (k.texId == texId && k.psm != 0) p = &k;
    for (const auto &k : rtxPk)
        if (k.tbp == palTbp && k.psm == 0) pal = &k;
    if (!p || !pal)
        return Texture2D{};
    const bool t8 = p->psm == 0x13 || p->psm == 0x1B, t4 = p->psm == 0x14 || p->psm == 0x24 || p->psm == 0x2C;
    const uint32_t np = pal->w * pal->h, w = p->w, h = p->h;
    const size_t data = p->off + 0x80, need = t8 ? size_t(w) * h : size_t(w) * h / 2;
    if ((!t8 && !t4) || data + need > file.size() || pal->off + 0x10 + 4 * size_t(np) > rtx.size())
        return Texture2D{};
    std::vector<uint32_t> clut(np), px(size_t(w) * h);
    for (uint32_t i = 0; i < np; ++i)
    {
        const uint32_t j = (np >= 256) ? ((i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1)) : i;   // CSM1: bits 3 e 4 trocados
        uint32_t c;
        std::memcpy(&c, rtx.data() + pal->off + 0x10 + 4 * j, 4);
        const uint32_t a = c >> 24;
        clut[i] = (c & 0xFFFFFFu) | (a >= 0x80 ? 255u : a * 2u) << 24;
    }
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
        {
            const size_t i = size_t(y) * w + x;
            uint32_t k = t8 ? file[data + i] : ((i & 1) ? file[data + i / 2] >> 4 : file[data + i / 2] & 15);
            px[size_t(h - 1 - y) * w + x] = k < np ? clut[k] : 0u;
        }
    Image img{px.data(), int(w), int(h), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    return t;
}

static void loadPortraits()
{
    if (g_portraitsTried)
        return;
    g_portraitsTried = true;
    namespace sd = wotm::savepics::detail;
    static const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    const std::string iso = io.cdImage.string();
    std::vector<uint8_t> tex, rtx;
    if (!wotm::disc::readFile(io.cdRoot, iso, "SHELL/SHELL.TEX", tex) || !wotm::disc::readFile(io.cdRoot, iso, "SHELL/SHELL.RTX", rtx) ||
        !sd::unzipIe(tex) || !sd::unzipIe(rtx))
    {
        std::fprintf(stderr, "[padsel] barra de monstros: SHELL/SHELL.TEX ou .RTX nao achado; sem retratos\n");
        return;
    }
    const std::vector<sd::Packet> texPk = sd::packets(tex), rtxPk = sd::packets(rtx);
    int n = 0;
    for (const PortraitTex &p : kPortraits)
    {
        g_portrait[p.monster] = p.rtx ? decodePortrait(rtx, rtxPk, rtx, rtxPk, p.texId, p.palTbp) : decodePortrait(tex, texPk, rtx, rtxPk, p.texId, p.palTbp);
        n += g_portrait[p.monster].id != 0;
        if (p.lockId)
            g_portraitLock[p.monster] = decodePortrait(rtx, rtxPk, rtx, rtxPk, p.lockId, p.lockPal);
    }
    std::fprintf(stderr, "[padsel] barra de monstros: %d de %zu retratos\n", n, std::size(kPortraits));
}

// A barra: as 10 colunas da selecao (retrato; silhueta se travada), o aleatorio (?) e o personalizado, de lado a lado da janela, logo
// acima da faixa preta de baixo do menu (24 de 224 linhas). A casa de cada jogador fica com a moldura na cor dele e a etiqueta (P1, P2,
// CPU) em cima; pronto, a etiqueta fica cheia. Depois da cena e do texto do jogo, antes da legenda (wotm_live.inc).
static void drawBar2d(int RW, int RH)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    if (!v.select || v.alpha <= 0.f)
    {
        g_mouseCell.store(0, std::memory_order_relaxed);
        return;
    }
    loadPortraits();
    const float W = float(RW), H = float(RH), band = H * 24.f / 224.f;
    const float barH = H * 0.15f, barY = H - band - barH;
    const unsigned char A = static_cast<unsigned char>(255.f * std::clamp(v.alpha, 0.f, 1.f));
    auto fade = [&](Color c) { return Color{c.r, c.g, c.b, static_cast<unsigned char>(c.a * A / 255)}; };
    DrawRectangleGradientV(0, int(barY), RW, int(barH), fade(Color{0, 0, 0, 120}), fade(Color{0, 0, 0, 230}));
    DrawRectangle(0, int(barY), RW, std::max(2, int(H * 0.003f)), fade(Color{150, 110, 60, 200}));
    constexpr int kCells = 12;
    float cell = barH * 0.70f, gap = cell * 0.12f, extraGap = cell * 0.35f;
    float total = kCells * cell + (kCells - 1) * gap + extraGap;
    if (total > W * 0.96f)
    {
        const float k = W * 0.96f / total;
        cell *= k; gap *= k; extraGap *= k; total *= k;
    }
    const float x0 = (W - total) * 0.5f, cy = barY + barH * 0.56f, top = cy - cell * 0.5f;
    auto cellX = [&](int c) { return x0 + float(c - 1) * (cell + gap) + (c > 10 ? extraGap : 0.f); };   // c 1..12
    const Color kName{246, 238, 220, 255}, kCpu{150, 150, 160, 255};
    // casa sob o mouse (cursor solto; so as abertas, com a selecao ja na tela): o EE anda ate ela (mouseStep)
    int hover = 0;
    if (const wotm::mouse::Pointer &mp = wotm::mouse::g_ptr; mp.in && mp.rw > 0 && mp.rh > 0 && v.alpha > 0.5f)
    {
        const float px = mp.x * W / float(mp.rw), py = mp.y * H / float(mp.rh);
        for (int c = 1; c <= kCells && !hover; ++c)
            if (px >= cellX(c) && px < cellX(c) + cell && py >= top && py < top + cell && (c > 10 || v.barOpen[c - 1]))
                hover = c;
    }
    // roteiro de teste (`mouse:cellN`, `mouse:clickN`): o cursor de mentira na casa N
    const double now = GetTime();
    for (ScriptStep &st : g_script)
        if (st.dev == kScriptMouse && now >= st.t0 && now < st.t0 + st.dur)
        {
            hover = st.act % 100;
            if (!st.fired)
            {
                st.fired = true;
                wotm::mouse::g_moveSeq.fetch_add(1, std::memory_order_relaxed);
                if (st.act >= 100)
                    wotm::mouse::click(wotm::mouse::g_clickL);
            }
        }
    g_mouseCell.store(hover, std::memory_order_relaxed);
    if (hover)
        wotm::mouse::claim();
    // quem esta em cada casa
    for (int c = 1; c <= kCells; ++c)
    {
        const float x = cellX(c);
        const Rectangle r{x, top, cell, cell};
        bool here = false;
        for (int p = 0; p < v.selPlayers; ++p)
            here = here || (!v.selWaiting[p] && v.selCell[p] == c);
        DrawRectangleRec(Rectangle{r.x - 2.f, r.y - 2.f, r.width + 4.f, r.height + 4.f}, fade(c == hover ? Color{200, 180, 140, 255} : Color{10, 6, 2, 255}));
        if (c <= 10)
        {
            const int mon = v.barMon[c - 1];
            const bool open = v.barOpen[c - 1];
            const Texture2D &t = open || !g_portraitLock[mon].id ? g_portrait[mon] : g_portraitLock[mon];
            if (t.id)
                DrawTexturePro(t, Rectangle{0.f, 0.f, float(t.width), float(t.height)}, r, Vector2{0.f, 0.f}, 0.f,
                               fade(!open ? Color{70, 60, 60, 255} : here ? WHITE : Color{150, 140, 130, 255}));
            else
                outlined(open ? kMonsterNames[mon] : "?", r.x + cell * 0.5f, cy, cell * 0.2f, cell * 0.9f, kName, v.alpha);
        }
        else
        {
            DrawRectangleGradientV(int(r.x), int(r.y), int(r.width), int(r.height), fade(Color{60, 40, 28, 255}), fade(Color{24, 14, 8, 255}));
            const Color txt = here ? kName : Color{170, 150, 120, 255};
            if (c == kCellRandom)
                outlined("?", r.x + cell * 0.5f, cy, cell * 0.75f, cell * 0.8f, txt, v.alpha);
            else
            {
                outlined("+", r.x + cell * 0.5f, cy - cell * 0.12f, cell * 0.5f, cell * 0.8f, txt, v.alpha);
                outlined("CUSTOM", r.x + cell * 0.5f, cy + cell * 0.3f, cell * 0.2f, cell * 0.9f, txt, v.alpha);
            }
        }
    }
    // molduras e etiquetas dos jogadores (varios na mesma casa: molduras de fora para dentro, etiquetas lado a lado)
    int stack[kCells + 1] = {};
    for (int p = 0; p < v.selPlayers; ++p)
    {
        const int c = v.selCell[p];
        if (v.selWaiting[p] || c < 1 || c > kCells)
            continue;
        const int k = stack[c]++;
        const Color col = v.selColor[p] >= 0 ? kPlayerColors[v.selColor[p] % kMaxPlayers] : kCpu;
        const float x = cellX(c), o = 3.f + float(k) * cell * 0.06f;
        DrawRectangleLinesEx(Rectangle{x - o, top - o, cell + 2.f * o, cell + 2.f * o}, std::max(3.f, cell * 0.05f), fade(col));
        const float tw = cell * 0.46f, th = cell * 0.24f, tx = x + float(k) * (tw + 2.f), ty = top - o - th * 0.9f;
        const Rectangle tag{tx, ty, tw, th};
        if (v.selReady[p])
            DrawRectangleRec(tag, fade(col));
        else
        {
            DrawRectangleRec(tag, fade(Color{10, 6, 2, 230}));
            DrawRectangleLinesEx(tag, 2.f, fade(col));
        }
        outlined(v.selLabel[p], tx + tw * 0.5f, ty + th * 0.5f, th * 0.85f, tw * 0.9f, v.selReady[p] ? kName : col, v.alpha);
    }
}

// Fileira de retratos do jogo (o painel de baixo da selecao): sai da cena enquanto a selecao nativa esta na tela. Os objetos sao achados
// pela textura (retratos, molduras e marcas de jogador do SHELL); so durante a selecao, entao nao pega nada de outra tela.
static void hideStrip(wotm::Scene &scene, const wotm::Ram &ram)
{
    auto stripTex = [](uint32_t id) { return id == 54 || (id >= 57 && id <= 68) || (id >= 177 && id <= 185); };
    auto &items = scene.items;
    items.erase(std::remove_if(items.begin(), items.end(),
                               [&](const wotm::DrawItem &it) {
                                   const uint32_t n = it.node & 0x1FFFFFFFu;
                                   if (!ram.ok(n, 0x60) || (ram.u8(n) & 0x3F) != 0)
                                       return false;
                                   const uint32_t ns = std::min<uint32_t>(5, ram.u16(n + 0x5A));   // texId[5] em +0x50 (Scene::objectTexId)
                                   for (uint32_t s = 0; s < ns; ++s)
                                       if (stripTex(ram.u16(n + 0x50 + 2 * s)))
                                           return true;
                                   return false;
                               }),
                items.end());
}

// Escolha desenhada na textura propria (antes do BeginTextureMode do quadro). `tex0` = atlas da fonte do quadro anterior.
static void renderUi(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    // rotulo do menu principal
    if (v.labelObj[0] && v.labelAlpha > 0.f)
        renderLabel(v.labelLit);
    if ((!v.on && !v.select) || v.alpha <= 0.f)
    {
        g_posInit = false;
        return;
    }
    if (v.select)
    {
        if (g_selRt.id == 0)
        {
            g_selRt = LoadRenderTexture(kSelPW, kSelPH);
            SetTextureFilter(g_selRt.texture, TEXTURE_FILTER_BILINEAR);
        }
        const int n = std::max(1, v.selPlayers);
        // o RenderTexture do monstro com ~1,3 x o tamanho dele na textura da selecao
        renderSelModels(v, int(selBoxW(n) * 0.94f * 1.3f), int(float(kSelPH) * 0.86f * 0.58f * 1.3f));
        BeginTextureMode(g_selRt);
        ClearBackground(Color{0, 0, 0, 0});
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        drawSelect(v);
        EndBlendMode();
        EndTextureMode();
        return;
    }
    if (g_rt.id == 0)
    {
        g_rt = LoadRenderTexture(kW, kH);
        SetTextureFilter(g_rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    const float dt = std::min(0.1f, GetFrameTime());
    BeginTextureMode(g_rt);
    ClearBackground(Color{0, 0, 0, 0});
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    std::vector<wotm::TextQuad> quads;
    drawJoin(v, quads, ram, tex0, dt);
    scene.drawQuads(quads, float(kW), float(kH));
    EndBlendMode();
    EndTextureMode();
}

// Retangulo de um objeto do telao no mundo: centro, eixos direita/cima no plano dele (cima = Z do mundo no plano; direita no
// sentido da direita da camera), normal para a camera e os limites nesses eixos
struct Plane
{
    Vector3 c, right, up, n;
    float x0, x1, y0, y1;
};
static bool objectPlane(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam, uint32_t obj, Plane &out)
{
    const wotm::DrawItem *it = nullptr;
    for (const wotm::DrawItem &i : scene.items)
        if ((i.node & 0x1FFFFFFFu) == obj) { it = &i; break; }
    const wotm::MeshData *md = it ? scene.mesh(ram, it->node) : nullptr;
    if (!md || md->tris.size() < 3)
        return false;
    std::vector<Vector3> p(md->tris.size());
    for (size_t i = 0; i < p.size(); ++i)
    {
        float w[3];
        wotm::xform(it->m, md->tris[i].x, md->tris[i].y, md->tris[i].z, w);
        p[i] = Vector3{w[0], w[1], w[2]};
    }
    Vector3 n{0, 0, 0};
    float best = 0.f;
    for (size_t i = 0; i + 2 < p.size(); i += 3)
    {
        const Vector3 c = vcross(vsub(p[i + 1], p[i]), vsub(p[i + 2], p[i]));
        const float l = vlen(c);
        if (l > best) { best = l; n = vscale(c, 1.f / l); }
    }
    if (best <= 0.f) return false;
    Vector3 c{0, 0, 0};
    for (const Vector3 &q : p) c = vadd(c, q);
    c = vscale(c, 1.f / float(p.size()));
    if (vdot(n, vsub(cam.position, c)) < 0.f) n = vneg(n);
    Vector3 up = vsub(Vector3{0, 0, 1}, vscale(n, n.z));
    if (vlen(up) < 0.1f) return false;
    up = vnorm(up);
    const Vector3 camRight = vcross(vnorm(vsub(cam.target, cam.position)), cam.up);
    Vector3 right = vnorm(vcross(up, n));
    if (vdot(right, camRight) < 0.f) right = vneg(right);
    out = Plane{c, right, up, n, 1e30f, -1e30f, 1e30f, -1e30f};
    for (const Vector3 &q : p)
    {
        const Vector3 d = vsub(q, c);
        const float x = vdot(d, out.right), y = vdot(d, out.up);
        out.x0 = std::min(out.x0, x); out.x1 = std::max(out.x1, x); out.y0 = std::min(out.y0, y); out.y1 = std::max(out.y1, y);
    }
    return true;
}

// Plano da selecao: o telao ou, na tela 7, os dois retratos grandes do fliperama juntos (no plano do P1, com a faixa de baixo da tela
// dele: ~30% a mais), que saem da cena. Guardado para o drawOnTelao, que desenha depois que os objetos ja sairam.
static Plane g_mgPlane;
static bool g_mgPlaneOk = false;
static bool selectPlane(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam, const View &v, Plane &out)
{
    if (!v.mgUsed[0])
        return objectPlane(scene, ram, cam, v.screenObj, out);
    Plane a, b;
    if (objectPlane(scene, ram, cam, v.mgUsed[0], a) && objectPlane(scene, ram, cam, v.mgUsed[1], b))
    {
        for (const float x : {b.x0, b.x1})
            for (const float y : {b.y0, b.y1})
            {
                const Vector3 q = vadd(vadd(b.c, vscale(b.right, x)), vscale(b.up, y)), d = vsub(q, a.c);
                const float px = vdot(d, a.right), py = vdot(d, a.up);
                a.x0 = std::min(a.x0, px); a.x1 = std::max(a.x1, px); a.y0 = std::min(a.y0, py); a.y1 = std::max(a.y1, py);
            }
        const float w = a.x1 - a.x0, h = a.y1 - a.y0;
        a.x0 -= 0.02f * w, a.x1 += 0.02f * w, a.y1 += 0.03f * h, a.y0 -= 0.30f * h;
        g_mgPlane = a;
        if (!g_mgPlaneOk)
            std::fprintf(stderr, "[padsel] tela 7: plano dos retratos %.1f x %.1f\n", a.x1 - a.x0, a.y1 - a.y0);
        g_mgPlaneOk = true;
    }
    auto &items = scene.items;
    items.erase(std::remove_if(items.begin(), items.end(),
                               [&](const wotm::DrawItem &it) {
                                   const uint32_t n = it.node & 0x1FFFFFFFu;
                                   return std::find(std::begin(v.mgObj), std::end(v.mgObj), n) != std::end(v.mgObj);
                               }),
                items.end());
    out = g_mgPlane;
    return g_mgPlaneOk;
}

// Textura (de render: V invertido) num retangulo do plano: esquerda/topo e largura/altura nos eixos dele, alfa 0..1; `uv` = parte
// da textura (x0, y0, x1, y1 em fracao, y de cima para baixo); `tint` multiplica a cor
static void drawOnPlane(const Plane &pl, const Texture2D &tex, float left, float top, float wx, float hy, float alpha, float lift,
                        const float uv[4] = nullptr, Color tint = WHITE, bool premul = false)
{
    // premul: textura desenhada com BLEND_CUSTOM_SEPARATE sobre fundo transparente (cor ja multiplicada pelo alfa): sem multiplicar de
    // novo (a lista do menu tem itens e brilho translucidos)
    const float u0 = uv ? uv[0] : 0.f, u1 = uv ? uv[2] : 1.f, vt = uv ? 1.f - uv[1] : 1.f, vb = uv ? 1.f - uv[3] : 0.f;
    const Vector3 off = vscale(pl.n, lift);
    auto at = [&](float x, float y) { return vadd(vadd(vadd(pl.c, vscale(pl.right, x)), vscale(pl.up, y)), off); };
    const Vector3 tl = at(left, top), tr = at(left + wx, top), bl = at(left, top - hy), br = at(left + wx, top - hy);
    rlDrawRenderBatchActive();
    if (premul)
    {
        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
    }
    else
        BeginBlendMode(BLEND_ALPHA);
    // sem profundidade: os itens com alfa 0 continuam gravando Z na frente do telao (e nada fica entre a area e a camera)
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    const float a = std::clamp(alpha, 0.f, 1.f), k = premul ? a : 1.f;
    rlColor4ub(static_cast<unsigned char>(tint.r * k), static_cast<unsigned char>(tint.g * k), static_cast<unsigned char>(tint.b * k),
               static_cast<unsigned char>(a * 255.f));
    rlTexCoord2f(u0, vt); rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(u0, vb); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(u1, vb); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(u1, vt); rlVertex3f(tr.x, tr.y, tr.z);
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableDepthTest();
    EndBlendMode();
}

// Onde o mouse (wotm_mouse.hpp, cursor solto) cai no retangulo do plano em que drawOnPlane poe uma textura (mesmos left/top/wx/hy):
// u e v em 0..1, v de cima para baixo. Na thread da janela, com a camera do quadro (`cam`, a do BeginMode3D).
static bool hitOnPlane(const Camera3D &cam, const Plane &pl, float left, float top, float wx, float hy, float &u, float &v)
{
    const wotm::mouse::Pointer &p = wotm::mouse::g_ptr;
    if (!p.in || p.rw <= 0 || p.rh <= 0 || wx <= 0.f || hy <= 0.f)
        return false;
    const Ray r = GetScreenToWorldRayEx(Vector2{p.x, p.y}, cam, p.rw, p.rh);
    const float dn = vdot(r.direction, pl.n);
    if (std::fabs(dn) < 1e-6f)
        return false;
    const float t = vdot(vsub(pl.c, r.position), pl.n) / dn;
    if (t <= 0.f)
        return false;
    const Vector3 q = vsub(vadd(r.position, vscale(r.direction, t)), pl.c);
    u = (vdot(q, pl.right) - left) / wx;
    v = (top - vdot(q, pl.up)) / hy;
    return u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f;
}

// Camera de frente para o telao `pl`, a fracao `s` (0..1, suavizada) do caminho entre a do jogo e ela: telao com 58% da altura da
// tela, o centro dele a 41% do topo (as faixas pretas tomam 10,7% em cima e embaixo); a camera um pouco abaixo do centro dele, para os
// carros aparecerem embaixo. Painel "recompiled options" (wotm_mainmenu.inc) e selecao de monstros de 3 e 4.
static void telaoCamera(Camera3D &cam, const Plane &pl, float aspect, float s)
{
    const float t = s * s * (3.f - 2.f * s);
    const float tanY = std::tan(cam.fovy * DEG2RAD * 0.5f);
    constexpr float kHeight = 0.58f, kCenter = 0.41f, kDrop = 0.22f;
    const float h = pl.y1 - pl.y0, w = pl.x1 - pl.x0;
    const float d = std::max(h * 0.5f / (tanY * kHeight), w * 0.5f / (tanY * aspect * 0.96f));
    const Vector3 center = vadd(pl.c, vadd(vscale(pl.right, (pl.x0 + pl.x1) * 0.5f), vscale(pl.up, (pl.y0 + pl.y1) * 0.5f)));
    const Vector3 pos = vsub(vadd(center, vscale(pl.n, d)), vscale(pl.up, h * kDrop));
    const Vector3 target = vsub(center, vscale(pl.up, (0.5f - kCenter) * 2.f * tanY * d));   // olha abaixo do centro: o telao sobe
    // posicao e direcao interpoladas a parte (interpolar o alvo fazia o telao sair de quadro no meio do caminho)
    auto lerp = [&](const Vector3 &a, const Vector3 &b) { return vadd(a, vscale(vsub(b, a), t)); };
    const Vector3 f0 = vnorm(vsub(cam.target, cam.position)), f1 = vnorm(vsub(target, pos));
    cam.position = lerp(cam.position, pos);
    cam.target = vadd(cam.position, vnorm(lerp(f0, f1)));
    cam.up = vnorm(lerp(cam.up, pl.up));
}

// Selecao de monstros de 3 e 4: a camera vai ao telao em ~0,45 s (no fade de entrada) e volta na saida, como a do painel. Antes do
// BeginMode3D do quadro (wotm_live.inc).
static void frameCamera(wotm::Scene &scene, const wotm::Ram &ram, Camera3D &cam, float aspect)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    static float s_t = 0.f;
    static double s_last = 0.0;
    const double now = GetTime();
    const float dt = std::clamp(float(now - s_last), 0.f, 0.1f);
    s_last = now;
    const bool want = v.select && v.alpha > 0.f;
    // Saida para uma tela com animacao de camera (voltar da adventure a tela 2, confirmar e ir a selecao de fase): a camera do jogo
    // ja anda de onde ficaria a selecao original ate a tela seguinte. Voltar ao jogo em 0,45 s por fora somava os dois movimentos;
    // aqui a do quadro e a do jogo mais a diferenca para o telao no inicio da saida, que some com o progresso da animacao do jogo
    // (um movimento so, do telao ate a tela seguinte, no ritmo do jogo).
    static bool s_follow = false;
    static float s_q0 = 0.f;
    static Vector3 s_dPos{}, s_dFwd{}, s_dUp{};
    if (!want && s_t > 0.f && !s_follow && v.transMoving && v.transProg < 0.95f)
    {
        Plane pl0;
        if (v.screenObj && selectPlane(scene, ram, cam, v, pl0))
        {
            Camera3D t = cam;
            telaoCamera(t, pl0, aspect, s_t);   // a camera mostrada agora
            s_dPos = vsub(t.position, cam.position);
            s_dFwd = vsub(vnorm(vsub(t.target, t.position)), vnorm(vsub(cam.target, cam.position)));
            s_dUp = vsub(t.up, cam.up);
            s_q0 = v.transProg;
            s_follow = true;
        }
    }
    if (s_follow)
    {
        if (want || !v.transMoving)
        {
            s_follow = false;
            if (!want) s_t = 0.f;
        }
        else
        {
            const float k = 1.f - std::clamp((v.transProg - s_q0) / std::max(1.f - s_q0, 1e-3f), 0.f, 1.f);
            hideStrip(scene, ram);
            scene.overlay.clear();
            scene.hpOverlay.clear();
            const Vector3 fwd = vnorm(vadd(vnorm(vsub(cam.target, cam.position)), vscale(s_dFwd, k)));
            cam.position = vadd(cam.position, vscale(s_dPos, k));
            cam.target = vadd(cam.position, fwd);
            cam.up = vnorm(vadd(cam.up, vscale(s_dUp, k)));
            return;
        }
    }
    s_t = std::clamp(s_t + (want ? dt : -dt) / 0.45f, 0.f, 1.f);
    // telas de opcoes antes da selecao (11 eliminacao, 12 CPUs de 1 jogador, 13 CPUs do multijogador, 15 "first to"): a fileira do
    // jogo so prepara a selecao do jogo, que a nativa substitui
    const uint32_t cur = ram.u32(wotm::sym::currScreen);
    // tambem as telas da selecao (3, 6, 7, 18, 19): o changeScreen ja trocou a tela antes de o transitionHook publicar a selecao, e a
    // fileira do jogo aparecia por um quadro (ex.: "sudden death" na tela 15)
    const bool optionsScreen = soloEnabled() && (cur == 11 || cur == 12 || cur == 13 || cur == 15 || cur == 3 || cur == 6 || cur == 7 ||
                                                 cur == 18 || cur == 19);
    if (v.select || s_t > 0.f || optionsScreen)
        hideStrip(scene, ram);   // a barra nativa (drawBar2d) faz o papel da fileira de retratos (ate a camera voltar)
    Plane pl;
    if (s_t <= 0.f || !v.screenObj || !selectPlane(scene, ram, cam, v, pl))
        return;
    scene.overlay.clear();   // icones de botao 3D do jogo: seguem a camera dele
    scene.hpOverlay.clear();
    telaoCamera(cam, pl, aspect, s_t);
}

// A escolha no plano do telao (objeto do filme, shell->m_movieScreen), abaixo do logo, e o rotulo do menu principal no retangulo
// do item. Dentro do BeginMode3D do quadro, depois da cena.
// ------------------------------------------------------------------ rotulo com as letras do proprio telao
// "multiplayer" montado com as letras dos outros itens do menu principal, na fonte do jogo. Os itens sao quadros com textura de letras
// brancas com contorno preto (a cor, apagada ou acesa, e a dos vertices; o filho 0 do switch e o apagado, o 1 o aceso), guardadas de
// cabeca para baixo: o "1/2 player" usa um atlas com "player", "options" e os digitos, num quadro para o digito e outro para a palavra;
// "load game", "options" e "unlocks" tem cada um a sua. Aqui: "player" vem do quadro da palavra do proprio "2 player", t e i do
// "options", m do "load game" e u do "unlocks" (letras separadas pelas colunas sem alfa da regiao de UV de cada quadro). A palavra fica
// na escala de texel do "player", na linha de base dele e centrada no "2 player" inteiro, com a cor dos vertices de cada estado. Os
// recortes saem do .RTX na hora (Scene::decodeTex): nada do jogo fica no repo. Se algum recorte nao tiver o numero de letras esperado,
// fica o rotulo com fonte (renderLabel).
struct Crop
{
    std::vector<uint32_t> px;   // RGBA8, alfa 0..255, linhas de cima para baixo como na tela
    int w = 0, h = 0;
    const char *why = "";       // motivo da falha (log)
};
struct Seg { int x0, x1; };   // colunas [x0, x1)
// Quadro de uma malha: regiao da textura (texels) e o mapa texel -> plano do objeto (x = ax * u + bx, y = ay * v + by)
struct Quad
{
    float u0 = 1e30f, u1 = -1e30f, v0 = 1e30f, v1 = -1e30f;
    float ax = 0.f, bx = 0.f, ay = 0.f, by = 0.f;
    float x0 = 1e30f, x1 = -1e30f;   // extensao no plano (eixo direita)
};
struct GlyphLabel
{
    bool ok = false;
    Texture2D tex{};                 // a palavra em branco com contorno (a cor vai no desenho)
    int base = 0;                    // linha de base na textura (pixels, de cima)
    int playerBase = 0;              // linha de base no recorte do "player" do "2 player" (linhas na ordem da tela)
    float inkCx = 0.f;               // centro das letras do "2 player" no plano dele (medido na montagem)
};
static GlyphLabel g_glyph;
constexpr int kGlyphPad = 4;
constexpr uint32_t kInk = 200;   // alfa da letra (255); entre duas letras o contorno encostado fica em 170

// Objeto (OBJECT_NODE) do filho `child` do switch do item `item` do menu principal (shell->m_menuItemSw[1][item])
static uint32_t itemObject(const wotm::Ram &ram, int item, int child)
{
    const uint32_t sw = ram.u32(kShell + 0x25C + uint32_t(10 + item) * 4) & 0x1FFFFFFFu;
    if (!ram.ok(sw, 0x10) || (ram.u8(sw) & 0x3F) != 6 || child >= ram.u8(sw + 0xB))
        return 0;
    const uint32_t c = ram.u32(sw + 0xC + 4u * uint32_t(child)) & 0x1FFFFFFFu;
    if (!ram.ok(c, 0x24))
        return 0;
    if ((ram.u8(c) & 0x3F) == 0)
        return c;
    if ((ram.u8(c) & 0x3F) == 1 && ram.u16(c + 8) > 0)   // GROUP_NODE: primeiro filho em +0x20
        if (const uint32_t o = ram.u32(c + 0x20) & 0x1FFFFFFFu; ram.ok(o, 0x60) && (ram.u8(o) & 0x3F) == 0)
            return o;
    return 0;
}

// Quadros da malha do objeto: triangulos agrupados pela regiao de UV (os de um quadro dividem a mesma). UV normalizado e com a textura
// repetida (os itens usam u -1..0): o periodo e tirado pelo piso do minimo. Com `m` (matriz do item da cena) e `pl`, o mapa para o plano.
static std::vector<Quad> meshQuads(const wotm::MeshData &md, uint32_t tw, uint32_t th, const wotm::M4 *m = nullptr, const Plane *pl = nullptr)
{
    float fu = 1e30f, fv = 1e30f;
    for (const wotm::Vtx &t : md.tris)
    {
        fu = std::min(fu, t.u);
        fv = std::min(fv, t.v);
    }
    fu = std::floor(fu);
    fv = std::floor(fv);
    struct P { float u, v, x, y; };
    std::vector<std::vector<P>> groups;
    std::vector<Quad> boxes;
    for (size_t i = 0; i + 2 < md.tris.size(); i += 3)
    {
        P p[3];
        Quad b;
        for (int k = 0; k < 3; ++k)
        {
            const wotm::Vtx &t = md.tris[i + size_t(k)];
            p[k].u = (t.u - fu) * float(tw);
            p[k].v = (t.v - fv) * float(th);
            p[k].x = p[k].y = 0.f;
            if (m && pl)
            {
                float w[3];
                wotm::xform(*m, t.x, t.y, t.z, w);
                const Vector3 d = vsub(Vector3{w[0], w[1], w[2]}, pl->c);
                p[k].x = vdot(d, pl->right);
                p[k].y = vdot(d, pl->up);
            }
            b.u0 = std::min(b.u0, p[k].u); b.u1 = std::max(b.u1, p[k].u); b.v0 = std::min(b.v0, p[k].v); b.v1 = std::max(b.v1, p[k].v);
        }
        size_t g = 0;
        for (; g < boxes.size(); ++g)   // mesma regiao (com folga de 1 texel): mesmo quadro
            if (b.u0 < boxes[g].u1 + 1.f && boxes[g].u0 < b.u1 + 1.f && b.v0 < boxes[g].v1 + 1.f && boxes[g].v0 < b.v1 + 1.f)
                break;
        if (g == boxes.size())
        {
            boxes.push_back(b);
            groups.emplace_back();
        }
        Quad &q = boxes[g];
        q.u0 = std::min(q.u0, b.u0); q.u1 = std::max(q.u1, b.u1); q.v0 = std::min(q.v0, b.v0); q.v1 = std::max(q.v1, b.v1);
        groups[g].insert(groups[g].end(), p, p + 3);
    }
    for (size_t g = 0; g < boxes.size(); ++g)
    {
        Quad &q = boxes[g];
        if (!m || !pl)
            continue;
        const P *uMin = nullptr, *uMax = nullptr, *vMin = nullptr, *vMax = nullptr;
        for (const P &p : groups[g])
        {
            if (!uMin || p.u < uMin->u) uMin = &p;
            if (!uMax || p.u > uMax->u) uMax = &p;
            if (!vMin || p.v < vMin->v) vMin = &p;
            if (!vMax || p.v > vMax->v) vMax = &p;
            q.x0 = std::min(q.x0, p.x);
            q.x1 = std::max(q.x1, p.x);
        }
        if (uMax->u - uMin->u < 1.f || vMax->v - vMin->v < 1.f)
            continue;
        q.ax = (uMax->x - uMin->x) / (uMax->u - uMin->u);
        q.bx = uMin->x - q.ax * uMin->u;
        q.ay = (vMax->y - vMin->y) / (vMax->v - vMin->v);
        q.by = vMin->y - q.ay * vMin->v;
    }
    return boxes;
}

// Pixels da regiao de um quadro, com as linhas na ordem da tela (`flip`: a textura esta de cabeca para baixo)
static bool cropQuad(const std::vector<uint32_t> &px, uint32_t tw, uint32_t th, const Quad &q, bool flip, Crop &c)
{
    const int x0 = std::clamp(int(std::floor(q.u0)), 0, int(tw)), x1 = std::clamp(int(std::ceil(q.u1)), 0, int(tw));
    const int y0 = std::clamp(int(std::floor(q.v0)), 0, int(th)), y1 = std::clamp(int(std::ceil(q.v1)), 0, int(th));
    if (x1 - x0 < 8 || y1 - y0 < 4)
    {
        c.why = "regiao de UV pequena";
        return false;
    }
    c.w = x1 - x0;
    c.h = y1 - y0;
    c.px.resize(size_t(c.w) * c.h);
    for (int y = 0; y < c.h; ++y)
        std::memcpy(&c.px[size_t(y) * c.w], &px[size_t(flip ? y1 - 1 - y : y0 + y) * tw + x0], size_t(c.w) * 4);
    return true;
}

// O maior quadro (em texels) da malha de um objeto, recortado
static bool cropItem(wotm::Scene &scene, const wotm::Ram &ram, uint32_t obj, bool flip, Crop &c)
{
    std::vector<uint32_t> px;
    uint32_t tw = 0, th = 0;
    const wotm::MeshData *md = obj ? scene.mesh(ram, obj) : nullptr;
    if (!md || md->tris.empty())
    {
        c.why = "sem malha";
        return false;
    }
    if (!scene.decodeTex(ram, obj, px, tw, th, 0, false) || !tw || !th)
    {
        c.why = "textura nao decodificada";
        return false;
    }
    const std::vector<Quad> qs = meshQuads(*md, tw, th);
    const Quad *best = nullptr;
    for (const Quad &q : qs)
        if (!best || (q.u1 - q.u0) > (best->u1 - best->u0))
            best = &q;
    return best && cropQuad(px, tw, th, *best, flip, c);
}

static std::vector<Seg> segments(const Crop &c)
{
    std::vector<Seg> s;
    int start = -1;
    for (int x = 0; x <= c.w; ++x)
    {
        bool ink = false;
        for (int y = 0; x < c.w && y < c.h && !ink; ++y)
            ink = (c.px[size_t(y) * c.w + x] >> 24) >= kInk;
        if (ink && start < 0)
            start = x;
        else if (!ink && start >= 0)
        {
            if (x - start >= 2)   // menos de 2 colunas: sujeira
                s.push_back({start, x});
            start = -1;
        }
    }
    return s;
}

// Linhas com letra nas colunas [x0, x1): topo e pe (exclusivo)
static bool rowBox(const Crop &c, int x0, int x1, int &top, int &bottom)
{
    top = c.h;
    bottom = 0;
    for (int y = 0; y < c.h; ++y)
        for (int x = x0; x < x1; ++x)
            if ((c.px[size_t(y) * c.w + x] >> 24) >= kInk)
            {
                top = std::min(top, y);
                bottom = std::max(bottom, y + 1);
                break;
            }
    return bottom > top;
}

// Linha de base das colunas [x0, x1): a mediana do pe (ultima linha com letra) das colunas com letra; os descendentes (p, y, g) sao
// minoria e nao a mudam. 0 = sem letra.
static int baselineOf(const Crop &c, int x0, int x1)
{
    std::vector<int> feet;
    for (int x = x0; x < x1; ++x)
        for (int y = c.h - 1; y >= 0; --y)
            if ((c.px[size_t(y) * c.w + x] >> 24) >= kInk)
            {
                feet.push_back(y + 1);
                break;
            }
    if (feet.empty())
        return 0;
    std::nth_element(feet.begin(), feet.begin() + long(feet.size() / 2), feet.end());
    return feet[feet.size() / 2];
}

// Segmentos depois do maior vao (a palavra depois do espaco: "game" de "load game")
static std::vector<Seg> afterWidestGap(const std::vector<Seg> &s)
{
    size_t at = 0;
    int best = -1;
    for (size_t i = 1; i < s.size(); ++i)
        if (s[i].x0 - s[i - 1].x1 > best)
        {
            best = s[i].x0 - s[i - 1].x1;
            at = i;
        }
    return best < 0 ? std::vector<Seg>{} : std::vector<Seg>(s.begin() + long(at), s.end());
}

// "multiplayer" em branco com contorno (a fonte do jogo), com a linha de base em `base`. `P` = o "player" do "2 player".
static bool buildGlyphWord(wotm::Scene &scene, const wotm::Ram &ram, const Crop &P, bool flip, Texture2D &out, int &outBase, int &outPlayerBase)
{
    Crop O, G, U;
    if (!cropItem(scene, ram, itemObject(ram, 3, 0), flip, O) || !cropItem(scene, ram, itemObject(ram, 4, 0), flip, G) ||
        !cropItem(scene, ram, itemObject(ram, 5, 0), flip, U))
    {
        std::fprintf(stderr, "[padsel] rotulo com as letras do telao: itens sem textura (%s / %s / %s)\n", O.why, G.why, U.why);
        return false;
    }
    const std::vector<Seg> sP = segments(P), sO = segments(O), sG = afterWidestGap(segments(G)), sU = segments(U);
    // o "player" vai inteiro (no atlas algumas letras dele se encostam sem a coluna de 170): basta ter letra
    if (sP.empty() || sO.size() != 7 || sG.size() != 4 || sU.size() != 7)
    {
        std::fprintf(stderr, "[padsel] rotulo com as letras do telao: segmentos inesperados (player %zu, options %zu, game %zu, unlocks %zu)\n",
                     sP.size(), sO.size(), sG.size(), sU.size());
        return false;
    }
    int t, b;
    const int baseP = baselineOf(P, sP.front().x0, sP.back().x1), baseO = baselineOf(O, 0, O.w), baseG = baselineOf(G, 0, G.w),
              baseU = baselineOf(U, 0, U.w);
    if (baseP <= 0 || baseO <= 0 || baseG <= 0 || baseU <= 0)
        return false;
    int gap = 0;   // vao entre letras: o mediano do "options"
    {
        std::vector<int> g;
        for (size_t i = 1; i < sO.size(); ++i)
            g.push_back(sO[i].x0 - sO[i - 1].x1);
        std::sort(g.begin(), g.end());
        gap = g[g.size() / 2];
    }
    struct Piece { const Crop *c; int x0, x1, base; };
    // m (load game), u, l (unlocks), t, i (options), player
    const Piece pieces[] = {{&G, sG[2].x0, sG[2].x1, baseG}, {&U, sU[0].x0, sU[0].x1, baseU}, {&U, sU[2].x0, sU[2].x1, baseU},
                            {&O, sO[2].x0, sO[2].x1, baseO}, {&O, sO[3].x0, sO[3].x1, baseO}, {&P, sP.front().x0, sP.back().x1, baseP}};
    int asc = 0, desc = 0, w = 0;
    for (const Piece &p : pieces)
    {
        if (!rowBox(*p.c, p.x0, p.x1, t, b))
            return false;
        asc = std::max(asc, p.base - t);
        desc = std::max(desc, b - p.base);
        w += p.x1 - p.x0;
    }
    w += gap * int(std::size(pieces) - 1) + 2 * kGlyphPad;
    const int h = asc + desc + 2 * kGlyphPad, base = kGlyphPad + asc;
    std::vector<uint32_t> img(size_t(w) * h, 0u);
    int x = kGlyphPad;
    for (const Piece &p : pieces)
    {
        for (int sy = 0; sy < p.c->h; ++sy)
        {
            const int dy = base + (sy - p.base);
            if (dy < 0 || dy >= h)
                continue;
            // a letra e o contorno encostado de cada lado (a coluna de 170): no encontro de duas fica o pixel mais opaco
            for (int sx = std::max(0, p.x0 - 1); sx < std::min(p.c->w, p.x1 + 1); ++sx)
            {
                const int dx = x + sx - p.x0;
                if (dx < 0 || dx >= w)
                    continue;
                const uint32_t c = p.c->px[size_t(sy) * p.c->w + sx];
                uint32_t &d = img[size_t(dy) * w + dx];
                if ((c >> 24) > (d >> 24))
                    d = c;
            }
        }
        x += p.x1 - p.x0 + gap;
    }
    Image im{img.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    out = LoadTextureFromImage(im);
    if (!out.id)
        return false;
    SetTextureFilter(out, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(out, TEXTURE_WRAP_CLAMP);
    outBase = base;
    outPlayerBase = baseP;
    return true;
}

// Quadros do "2 player" no plano dele e o recorte do "player" (o maior quadro); `flip` pela orientacao do mapa (v sobe na tela)
struct OwnText
{
    Quad word;            // quadro do "player"
    float inkCx = 0.f;    // centro das letras do "2 player" inteiro (o "2" e o "player") no plano
    Crop P;               // recorte do "player" (so na montagem)
    bool flip = false;
};
// Quadros do "2 player" no plano. `build`: decodifica a textura, recorta o "player" e mede o centro pelas letras (os quadros tem margem
// vazia, desigual); senao so a geometria, com o tamanho de textura guardado na montagem.
static bool ownText(wotm::Scene &scene, const wotm::Ram &ram, uint32_t obj, const Plane &pl, bool build, OwnText &o)
{
    static uint32_t s_tw = 0, s_th = 0;
    const wotm::DrawItem *it = nullptr;
    for (const wotm::DrawItem &i : scene.items)
        if ((i.node & 0x1FFFFFFFu) == obj) { it = &i; break; }
    const wotm::MeshData *md = it ? scene.mesh(ram, it->node) : nullptr;
    std::vector<uint32_t> px;
    if (!md || md->tris.size() < 3)
    {
        o.P.why = "sem malha";
        return false;
    }
    if (build && (!scene.decodeTex(ram, obj, px, s_tw, s_th, 0, false) || !s_tw || !s_th))
    {
        o.P.why = "textura nao decodificada";
        return false;
    }
    if (!s_tw || !s_th)
        return false;
    const std::vector<Quad> qs = meshQuads(*md, s_tw, s_th, &it->m, &pl);
    const Quad *word = nullptr;
    for (const Quad &q : qs)
        if (!word || (q.u1 - q.u0) > (word->u1 - word->u0))
            word = &q;
    if (!word || word->ay == 0.f)
    {
        o.P.why = "sem quadro da palavra";
        return false;
    }
    o.word = *word;
    o.flip = word->ay > 0.f;   // v maior mais acima: a textura esta de cabeca para baixo
    if (!build)
        return true;
    float x0 = 1e30f, x1 = -1e30f;
    for (const Quad &q : qs)
    {
        Crop c;
        if (!cropQuad(px, s_tw, s_th, q, o.flip, c))
            continue;
        const std::vector<Seg> s = segments(c);
        if (s.empty())
            continue;
        const float u0 = std::floor(q.u0);   // coluna 0 do recorte
        for (const float u : {u0 + float(s.front().x0), u0 + float(s.back().x1)})
        {
            x0 = std::min(x0, q.ax * u + q.bx);
            x1 = std::max(x1, q.ax * u + q.bx);
        }
    }
    if (x1 < x0)
    {
        o.P.why = "sem letras";
        return false;
    }
    o.inkCx = 0.5f * (x0 + x1);
    return cropQuad(px, s_tw, s_th, o.word, o.flip, o.P);
}

// Cor dos vertices de um objeto (GS: 0x80 = 1,0), a media
static Color vertexColor(wotm::Scene &scene, const wotm::Ram &ram, uint32_t obj)
{
    const wotm::MeshData *md = obj ? scene.mesh(ram, obj) : nullptr;
    if (!md || md->tris.empty())
        return WHITE;
    float r = 0, g = 0, b = 0, a = 0;
    for (const wotm::Vtx &t : md->tris)
    {
        r += t.r;
        g += t.g;
        b += t.b;
        a += t.a;
    }
    const float n = float(md->tris.size()), k = 255.f / 128.f;
    auto c8 = [&](float v) { return static_cast<unsigned char>(std::clamp(v / n * k, 0.f, 255.f)); };
    return Color{c8(r), c8(g), c8(b), c8(a)};   // o alfa dos itens apagados fica abaixo de 1: escurecem sobre o telao
}

// Uma vez por sessao (as texturas do telao sao residentes); ate 20 tentativas, uma a cada 30 quadros
static void buildGlyphLabel(wotm::Scene &scene, const wotm::Ram &ram, uint32_t ownObj, const Plane &pl)
{
    static int frames = 0, tries = 0;
    if (g_glyph.ok || tries >= 20 || frames++ % 30 != 0)
        return;
    ++tries;
    OwnText own;
    if (!ownText(scene, ram, ownObj, pl, true, own))
    {
        if (tries == 1 || tries == 20)
            std::fprintf(stderr, "[padsel] rotulo com as letras do telao: \"2 player\" (objeto %x) %s; fica a fonte\n", ownObj, own.P.why);
        return;
    }
    Texture2D t{};
    int base = 0, playerBase = 0;
    if (!buildGlyphWord(scene, ram, own.P, own.flip, t, base, playerBase))
        return;
    g_glyph.tex = t;
    g_glyph.base = base;
    g_glyph.playerBase = playerBase;
    g_glyph.inkCx = own.inkCx;
    g_glyph.ok = true;
    std::fprintf(stderr, "[padsel] rotulo com as letras do telao: %dx%d\n", t.width, t.height);
}

// A palavra no lugar do "2 player": escala de texel e linha de base do quadro do "player", centrada no item inteiro, na cor dos vertices
static bool drawGlyphLabel(wotm::Scene &scene, const wotm::Ram &ram, const Plane &pl, uint32_t obj, bool lit, float alpha)
{
    OwnText own;
    if (!g_glyph.ok || !ownText(scene, ram, obj, pl, false, own))
        return false;
    const Quad &q = own.word;
    const float sx = std::fabs(q.ax), sy = std::fabs(q.ay);
    const float wx = float(g_glyph.tex.width) * sx, hy = float(g_glyph.tex.height) * sy;
    const float cx = g_glyph.inkCx;
    // pe da palavra montada = pe do "player" original (mesma fonte e escala): a linha do pe do l no recorte, de volta ao v do quadro
    const float baseV = own.flip ? q.v1 - float(g_glyph.playerBase) : q.v0 + float(g_glyph.playerBase);
    const float baseY = q.ay * baseV + q.by;
    const float top = baseY + float(g_glyph.base) * sy;
    const Color tint = vertexColor(scene, ram, itemObject(ram, 2, lit ? 1 : 0));
    const float uv[4] = {0.f, 1.f, 1.f, 0.f};   // textura comum (nao de render): V de cima para baixo
    // o alfa dos vertices escurece o item apagado; medido nas fotos, com ele cru o rotulo saia ~75% do brilho dos outros itens (o
    // desenho nativo dos objetos mistura de outro jeito): 1,35 x, ate 1
    const float va = std::min(1.f, 1.35f * float(tint.a) / 255.f);
    drawOnPlane(pl, g_glyph.tex, cx - wx * 0.5f, top, wx, hy, alpha * va, 0.004f * (pl.x1 - pl.x0), uv, tint);
    return true;
}

// Filme do telao na selecao: a tela 3 encerra o myMovie do jogo (o sheldemo do menu anterior); a selecao nativa toca o mesmo filme na
// textura do menu (Player::Screen), em laco, e o para na saida se o jogo nao tiver posto outro (myMovie +0x94 = 1: parado para o jogo).
// Thread da janela.
static void selectMovie(const wotm::Ram &ram, bool select)
{
#ifdef WOTM_HAVE_FMV
    static bool s_own = false;
    wotm::fmv::Player &p = wotm::fmv::Player::get();
    const bool gameStopped = ram.u32(wotm::sym::myMovie + 0x94) != 0;
    // enquanto a selecao esta na tela, o End do jogo nao para o filme: ele segue de onde estava (recomecar mostrava de novo o fade
    // do preto do comeco do sheldemo, um segundo fade-in logo depois do da transicao)
    p.setHold(wotm::fmv::Player::Screen, select);
    if (select && gameStopped)
    {
        if (!p.active(wotm::fmv::Player::Screen) || p.finished(wotm::fmv::Player::Screen))
            p.start(wotm::fmv::Player::Screen, "mov/sheldemo.pss", false);   // em laco
        s_own = true;
    }
    else if (!select && s_own)
    {
        if (gameStopped)
            p.stop(wotm::fmv::Player::Screen);
        s_own = false;
    }
#else
    (void)ram;
    (void)select;
#endif
}

static void drawOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    selectMovie(ram, v.select);
    // de varios objetos em uso de um item (o texto e o brilho, maior), o de menor altura
    auto textPlane = [&](const uint32_t objs[4], Plane &out) {
        bool got = false;
        Plane p;
        for (int i = 0; i < 4 && objs[i]; ++i)
            if (objectPlane(scene, ram, cam, objs[i], p) && (!got || p.y1 - p.y0 < out.y1 - out.y0))
            {
                out = p;
                got = true;
            }
        return got;
    };
    Plane pl, ref;
    const bool labelOn = v.labelObj[0] && v.labelAlpha > 0.f && objectPlane(scene, ram, cam, v.labelObj[0], pl);
    if (labelOn)
        buildGlyphLabel(scene, ram, v.labelObj[0], pl);
    if (labelOn && drawGlyphLabel(scene, ram, pl, v.labelObj[0], v.labelLit, v.labelAlpha))
    {
        // letras do telao (drawGlyphLabel)
    }
    else if (v.labelObj[0] && v.labelAlpha > 0.f && g_labelRt.id && textPlane(v.labelObj, pl))
    {
        // altura da linha = a do "1 player" (as letras dos itens tem o mesmo corpo); centro na coluna dele (os itens sao centrados), na
        // altura do proprio item
        float cx = 0.f, refH = pl.y1 - pl.y0;
        if (textPlane(v.labelRef, ref))
        {
            cx = vdot(vsub(ref.c, pl.c), pl.right);   // o texto fica no centroide da malha (o retangulo do quadro tem margem desigual)
            refH = ref.y1 - ref.y0;
        }
        const float texW = float(g_labelRt.texture.width), texH = float(g_labelRt.texture.height);
        // as letras ocupam ~74% da altura do quadro do item
        const float hy = 0.74f * refH * texH / (texH - 2.f * kLabelPad), wx = hy * texW / texH;
        drawOnPlane(pl, g_labelRt.texture, cx - wx * 0.5f, (pl.y0 + pl.y1) * 0.5f + hy * 0.5f, wx, hy, v.labelAlpha, 0.004f * (pl.x1 - pl.x0));
    }
    if ((!v.on && !v.select) || v.alpha <= 0.f || !v.screenObj)
        return;
    if (v.mgUsed[0] ? !g_mgPlaneOk : !objectPlane(scene, ram, cam, v.screenObj, pl))
        return;
    if (v.mgUsed[0])
        pl = g_mgPlane;
    if (v.select)
    {
        // selecao de monstros: o telao inteiro, visto de frente (frameCamera)
        // fundo: o telao preto com o filme do menu anterior (sheldemo, preto e branco) por cima, de ponta a ponta
        {
            const Texture2D white{rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            drawOnPlane(pl, white, pl.x0, pl.y1, pl.x1 - pl.x0, pl.y1 - pl.y0, v.alpha, 0.001f * (pl.x1 - pl.x0), nullptr, BLACK);
            if (scene.movieTexture)
                if (const unsigned id = scene.movieTexture())
                {
                    static const float uvMovie[4] = {0.f, 1.f, 1.f, 0.f};
                    const Texture2D mv{id, 256, 128, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                    drawOnPlane(pl, mv, pl.x0, pl.y1, pl.x1 - pl.x0, pl.y1 - pl.y0, v.alpha, 0.0015f * (pl.x1 - pl.x0), uvMovie);
                }
        }
        if (g_selRt.id == 0)
            return;
        const float fyTop = 0.03f, fyBottom = 0.97f;
        const float hy = (pl.y1 - pl.y0) * (fyBottom - fyTop), wx = std::min(hy * float(kSelPW) / float(kSelPH), (pl.x1 - pl.x0) * 0.96f);
        const float h = wx * float(kSelPH) / float(kSelPW);
        drawOnPlane(pl, g_selRt.texture, (pl.x0 + pl.x1) * 0.5f - wx * 0.5f, pl.y1 - (pl.y1 - pl.y0) * fyTop - (hy - h) * 0.5f, wx, h, v.alpha,
                    0.002f * (pl.x1 - pl.x0), nullptr, WHITE, true);
        return;
    }
    if (g_rt.id == 0)
        return;
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        std::fprintf(stderr, "[padsel] telao: no %x, %.1f x %.1f, centro %.1f %.1f %.1f\n", v.screenObj, pl.x1 - pl.x0, pl.y1 - pl.y0, pl.c.x, pl.c.y,
                     pl.c.z);
    }
    // escolha de controles: abaixo do logo (de 44% a 96% da altura), ou no telao inteiro com o menu em texto (o wotm_mainmenu.inc
    // tira o logo fora do "press start"); selecao de monstros: em cima (a tela 6 nao tem o logo e tem a fileira de retratos embaixo);
    // centrada, com a proporcao da textura (largura limitada: a altura acompanha, centrada na faixa)
    const bool full = !v.select && wotmTextMenuOn();
    const float fyTop = v.select ? 0.06f : full ? 0.15f : 0.44f, fyBottom = v.select ? 0.60f : full ? 0.97f : 0.96f;
    const float hy = (pl.y1 - pl.y0) * (fyBottom - fyTop), wx = std::min(hy * float(kW) / float(kH), (pl.x1 - pl.x0) * 0.92f);
    const float h = wx * float(kH) / float(kW);
    drawOnPlane(pl, g_rt.texture, (pl.x0 + pl.x1) * 0.5f - wx * 0.5f, pl.y1 - (pl.y1 - pl.y0) * fyTop - (hy - h) * 0.5f, wx, h, v.alpha,
                0.002f * (pl.x1 - pl.x0));
}
} // namespace wotm_padsel

void wotmPadSelInstall(PS2Runtime &runtime)
{
    using namespace wotm_padsel;
    parseScript();
    struct H { uint32_t addr; PS2Runtime::RecompiledFunction *orig; PS2Runtime::RecompiledFunction fn; };
    const H hooks[] = {
        {wotm::fn::screenTransition__Fb, &g_origTransition, &transitionHook},
        {wotm::fn::screenGameModes2P__Fv, &g_origModes2P, &modes2PHook},
        {wotm::fn::screenCharSelect2P__Fv, &g_origSelect2P, &select2PHook},
        {wotm::fn::screenCharSelect1P__Fv, &g_origSelect1P, &select1PHook},
        {wotm::fn::screenCharSelect3P__Fv, &g_origSelect3P, &select3PHook},
        {wotm::fn::screenCharSelect4P__Fv, &g_origSelect4P, &select4PHook},
        {wotm::fn::screenMinigames2P__Fv, &g_origMinigames2P, &minigames2PHook},
        {wotm::fn::screenGetInput__Fi, &g_origGetInput, &getInputHook},
        {wotm::fn::changeScreen__Fiiiib, &g_origChangeScreen, &changeScreenHook},
        {wotm::fn::screenMain__Fv, &g_origMain, &mainHook},
        {wotm::fn::screenWaitForStart__Fv, &g_origWaitStart, &waitStartHook},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
}
