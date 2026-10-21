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
void wotmMultiSetCostume(int player, uint32_t costume);   // wotm_split4.inc: traje dos jogadores 3 e 4 (o Shell so tem lugar para 2)

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
enum Action { Left, Right, Ok, Back, Start, Up, Down, kActions };
enum Phase { Off, Config, Reveal, Leaving };

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
    // selecao
    int selPlayers = 0;
    uint32_t selMonster[kMaxPlayers] = {};   // m_monsterSel (tipo << 5)
    int selCostume[kMaxPlayers] = {}, selCostumes[kMaxPlayers] = {};
    bool selReady[kMaxPlayers] = {};
    // rotulo do menu principal
    uint32_t labelObj[4] = {};   // objetos do switch do "2 player" em uso ([0] = 0: sem rotulo); o menor e o texto
    uint32_t labelRef[4] = {};   // objetos do "1 player" em uso (texto e brilho): o menor da a coluna e a altura das letras
    bool labelLit = false;
    float labelAlpha = 0.f;
};
static std::mutex g_m;
static View g_view;

// Thread do EE
static Phase g_phase = Off;
static float g_uiAlpha = 0.f, g_reveal = 0.f;
static int g_slot[kDevs] = {};
static bool g_prev[kDevs][kActions] = {};
static bool g_primed = false;
static int g_inject = 0;   // codigo para o proximo screenGetInput (5 = voltar)
static int g_multi = 0;    // 3 ou 4: jogadores da partida multijogador escolhida (0 = 1 ou 2 jogadores, retail)
static PS2Runtime::RecompiledFunction g_origTransition = nullptr, g_origModes2P = nullptr, g_origGetInput = nullptr, g_origMain = nullptr,
                                      g_origWaitStart = nullptr, g_origSelect2P = nullptr, g_origChangeScreen = nullptr;
static uint32_t g_transRa = 0, g_modesRa = 0, g_selectRa = 0, g_mainRa = 0;
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

struct ScriptStep { double t0, dur; int dev, act; };
static std::vector<ScriptStep> g_script;

static void parseScript()
{
    const char *e = std::getenv("WOTM_PADSEL_SCRIPT");
    if (!e) return;
    std::string s(e);
    static const char *const names[kActions] = {"left", "right", "ok", "back", "start", "up", "down"};
    for (size_t pos = 0; pos < s.size();)
    {
        size_t end = s.find(';', pos);
        if (end == std::string::npos) end = s.size();
        const std::string item = s.substr(pos, end - pos);
        pos = end + 1;
        double t0 = 0, dur = 0;
        char dev[8] = {}, act[8] = {};
        if (std::sscanf(item.c_str(), "%lf:%lf:%7[^:]:%7s", &t0, &dur, dev, act) != 4) continue;
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

// Acoes do dispositivo agora
static void readDevice(int d, bool out[kActions])
{
    if (d == kKb)
    {
        out[Left] = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_J);
        out[Right] = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_L);
        out[Up] = IsKeyDown(KEY_UP) || IsKeyDown(KEY_I);
        out[Down] = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_K);
        out[Ok] = IsKeyDown(KEY_SPACE);
        out[Start] = IsKeyDown(KEY_ENTER);
        out[Back] = IsKeyDown(KEY_X) || IsKeyDown(KEY_BACKSPACE);
    }
    else
    {
        const float lx = GetGamepadAxisMovement(d, GAMEPAD_AXIS_LEFT_X), ly = GetGamepadAxisMovement(d, GAMEPAD_AXIS_LEFT_Y);
        out[Left] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || lx < -0.6f;
        out[Right] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || lx > 0.6f;
        out[Up] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_LEFT_FACE_UP) || ly < -0.6f;
        out[Down] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ly > 0.6f;
        out[Ok] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        out[Start] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        out[Back] = IsGamepadButtonDown(d, GAMEPAD_BUTTON_RIGHT_FACE_UP);
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
    for (uint32_t k = 0; k < kids && k < 8; ++k)
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

static void publish(uint8_t *rdram)
{
    std::lock_guard<std::mutex> lk(g_m);
    View &v = g_view;
    v.on = g_phase != Off;
    v.select = g_selOn;
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
    v.selPlayers = g_selOn ? g_multi : 0;
    for (int p = 0; p < v.selPlayers; ++p)
    {
        v.selMonster[p] = rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(g_pick[p].col - 1));
        v.selCostume[p] = g_pick[p].costume;
        v.selReady[p] = g_pick[p].ready;
        int open = 0;
        for (int c = 0; c < 4; ++c)
        {
            const uint32_t s = wotm::sym::monsterSelectMode + uint32_t((g_pick[p].col - 1) * 4 + c) * kSlotSize;
            open += rd32(rdram, s + 4) == 1u && rd32(rdram, s) == 1u;
        }
        v.selCostumes[p] = open;
    }
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
        if (!keep || !columnOpen(rdram, g_pick[p].col) || !costumeOpen(rdram, g_pick[p].col, g_pick[p].costume))
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
        if (k.ready)
        {
            if (e[Back]) { k.ready = false; sound = 6; }
            continue;
        }
        if (e[Left] || e[Right])
        {
            k.col = stepColumn(rdram, k.col, e[Left] ? -1 : 1);
            k.costume = firstCostume(rdram, k.col);
            sound = e[Left] ? 2 : 4;
        }
        if (e[Up] || e[Down])
        {
            const int dir = e[Up] ? -1 : 1;
            for (int i = 0; i < 4; ++i)
            {
                k.costume = (k.costume + dir + 4) % 4;
                if (costumeOpen(rdram, k.col, k.costume)) break;
            }
            sound = dir < 0 ? 2 : 4;
        }
        if (e[Ok] && costumeOpen(rdram, k.col, k.costume)) { k.ready = true; sound = 8; }
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

// ------------------------------------------------------------------ ganchos

// Tela escondida pela escolha em curso (5: controles; 6: selecao de monstros), ou -1
static int hiddenScreen()
{
    if (g_phase == Config || g_phase == Leaving)
        return 5;
    if (g_selOn)
        return 6;
    return -1;
}

static void hideSelect2P(uint8_t *rdram)
{
    setItemsAlpha(rdram, 6, 0.f);
    for (uint32_t i = 0; i < 2; ++i)   // m_costumeBoxCs, m_costumeBoxCs2: drawMe (+0xC)
        if (const uint32_t cs = rd32(rdram, kCostumeBox + 4 * i) & 0x1FFFFFFFu; cs >= 0x1000u)
            rdram[cs + 0xC] = 0;
    // marcas "1"/"2" dos jogadores do retail nos retratos (shell->m_selectSw[coluna]: 1, 2 ou 3 = os dois; 4 = travado, fica)
    for (uint32_t i = 0; i < 10; ++i)
        if (const uint32_t sw = rd32(rdram, kShell + 0x168 + 4 * i) & 0x1FFFFFFFu; sw >= 0x1000u && rdram[sw + 8] >= 1 && rdram[sw + 8] <= 3)
            rdram[sw + 8] = 0;
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
        if (rd32(rdram, wotm::sym::screenFirstPass) && cur == 6 && g_multi && !g_selOn)
            selectBegin(rdram);
        g_transRa = GPR_U32(ctx, 31);
        g_transPending = true;
    }
    g_origTransition(rdram, ctx, runtime);
    if (!g_transPending || ctx->pc != g_transRa)
        return;
    g_transPending = false;
    const int hidden = hiddenScreen();
    if (hidden >= 0)
    {
        // entrando: so na fase de fade-in (antes disso o alfa e o resto da tela anterior); saindo: o fade-out da tela
        const bool entering = (hidden == 5 && g_phase == Config) || (hidden == 6 && !g_selLeaving);
        const bool fading = entering ? rd32(rdram, wotm::sym::fadingIn_2979) != 0 : rd32(rdram, wotm::sym::fadingOut_2980) != 0;
        const bool ended = rd32(rdram, wotm::sym::betweenScreens) == 0;
        g_uiAlpha = fading ? std::min(1.f, std::max(0.f, itemsAlpha(rdram, hidden))) : (entering && ended ? 1.f : 0.f);
        setItemsAlpha(rdram, hidden, 0.f);
        if (hidden == 6)
            hideSelect2P(rdram);
        if (g_phase == Leaving && ended)
            g_phase = Off;
        if (hidden == 6 && g_selLeaving && ended && rd32(rdram, wotm::sym::currScreen) != 6)
            g_selOn = g_selLeaving = false;
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

static void select2PHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const bool entry = ctx->pc == wotm::fn::screenCharSelect2P__Fv;
    if (entry && g_selOn && !g_selLeaving && rd32(rdram, wotm::sym::betweenScreens) == 0)
    {
        g_uiAlpha = 1.f;
        selectStep(rdram, ctx, runtime);
    }
    if (entry && (g_selOn || g_multi))
    {
        g_selectRa = GPR_U32(ctx, 31);
        g_selectPending = true;
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
            changeScreen(rdram, ctx, runtime, 6, 15, -1, 0);
    }
    if (g_selOn)
        hideSelect2P(rdram);
    publish(rdram);
}

static void getInputHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const uint32_t cur = rd32(rdram, wotm::sym::currScreen);
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
    g_origGetInput(rdram, ctx, runtime);
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
    const char *hint = v.count < 2 ? "CONNECT A CONTROLLER - X TO JOIN"
                       : v.players >= 2 ? "START TO CONTINUE - LEFT/RIGHT TO SWAP"
                                        : "X TO JOIN - LEFT/RIGHT TO SWAP";
    const bool blink = v.players >= 2 && std::fmod(GetTime(), 1.0) > 0.6;
    if (!blink)
        text(quads, ram, tex0, kW, kH, hint, W * 0.5f, H * 0.94f, fitText(hint, H * 0.06f, W * 0.95f), Color{0x80, 0x7A, 0x60, 0x80});
}

// Selecao de monstros: um quadro por jogador, lado a lado
static void drawSelect(const View &v, std::vector<wotm::TextQuad> &quads, const wotm::Ram &ram, uint64_t tex0)
{
    constexpr float W = kW, H = kH;
    const int n = v.selPlayers, cols = n > 4 ? (n + 1) / 2 : n, rows = n > 4 ? 2 : 1;
    const float pad = W * 0.012f, boxW = (W - pad * float(cols + 1)) / float(cols), boxH = (H * 0.86f - pad * float(rows + 1)) / float(rows);
    for (int p = 0; p < n; ++p)
    {
        const Color c = kPlayerColors[p];
        const float x = pad + float(p % cols) * (boxW + pad), y = pad + float(p / cols) * (boxH + pad);
        const Rectangle box{x, y, boxW, boxH};
        DrawRectangleRounded(box, 0.1f, 8, Color{0, 0, 0, 150});
        DrawRectangleRoundedLinesEx(box, 0.1f, 8, v.selReady[p] ? 7.f : 4.f, Color{c.r, c.g, c.b, 230});
        const float cx = x + boxW * 0.5f;
        char label[32];
        std::snprintf(label, sizeof label, "PLAYER %d", p + 1);
        text(quads, ram, tex0, kW, kH, label, cx, y + boxH * 0.13f, fitText(label, boxH * 0.11f, boxW * 0.9f), gsColor(c));
        const char *name = monsterName(v.selMonster[p]);
        text(quads, ram, tex0, kW, kH, name, cx, y + boxH * 0.40f, fitText(name, boxH * 0.17f, boxW * 0.7f), Color{0x80, 0x7A, 0x6C, 0x80});
        if (!v.selReady[p])
        {
            const float ah = boxH * 0.12f;
            text(quads, ram, tex0, kW, kH, "<", x + boxW * 0.08f, y + boxH * 0.40f, ah, gsColor(c));
            text(quads, ram, tex0, kW, kH, ">", x + boxW * 0.92f, y + boxH * 0.40f, ah, gsColor(c));
        }
        // trajes: um ponto por traje aberto, o escolhido cheio
        char cos[24];
        std::snprintf(cos, sizeof cos, "COSTUME %d", v.selCostume[p] + 1);
        text(quads, ram, tex0, kW, kH, cos, cx, y + boxH * 0.62f, fitText(cos, boxH * 0.08f, boxW * 0.9f), Color{0x70, 0x68, 0x58, 0x80}, 1.f);
        if (v.selReady[p])
        {
            DrawRectangleRounded(Rectangle{x + boxW * 0.15f, y + boxH * 0.74f, boxW * 0.7f, boxH * 0.17f}, 0.4f, 6, Color{c.r, c.g, c.b, 200});
            text(quads, ram, tex0, kW, kH, "READY", cx, y + boxH * 0.825f, fitText("READY", boxH * 0.1f, boxW * 0.6f), Color{0x10, 0x10, 0x10, 0x80});
        }
    }
    bool all = n > 0;
    for (int p = 0; p < n; ++p)
        all = all && v.selReady[p];
    const char *hint = all ? "GET READY" : "LEFT/RIGHT MONSTER - UP/DOWN COSTUME - X READY";
    text(quads, ram, tex0, kW, kH, hint, W * 0.5f, H * 0.94f, fitText(hint, H * 0.06f, W * 0.95f), Color{0x80, 0x7A, 0x60, 0x80});
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
    if (v.select)
        drawSelect(v, quads, ram, tex0);
    else
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

// Textura (de render: V invertido) num retangulo do plano: esquerda/topo e largura/altura nos eixos dele, alfa 0..1; `uv` = parte
// da textura (x0, y0, x1, y1 em fracao, y de cima para baixo)
static void drawOnPlane(const Plane &pl, const Texture2D &tex, float left, float top, float wx, float hy, float alpha, float lift,
                        const float uv[4] = nullptr)
{
    const float u0 = uv ? uv[0] : 0.f, u1 = uv ? uv[2] : 1.f, vt = uv ? 1.f - uv[1] : 1.f, vb = uv ? 1.f - uv[3] : 0.f;
    const Vector3 off = vscale(pl.n, lift);
    auto at = [&](float x, float y) { return vadd(vadd(vadd(pl.c, vscale(pl.right, x)), vscale(pl.up, y)), off); };
    const Vector3 tl = at(left, top), tr = at(left + wx, top), bl = at(left, top - hy), br = at(left + wx, top - hy);
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    // sem profundidade: os itens com alfa 0 continuam gravando Z na frente do telao (e nada fica entre a area e a camera)
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, static_cast<unsigned char>(std::clamp(alpha, 0.f, 1.f) * 255.f));
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

// A escolha no plano do telao (objeto do filme, shell->m_movieScreen), abaixo do logo, e o rotulo do menu principal no retangulo
// do item. Dentro do BeginMode3D do quadro, depois da cena.
static void drawOnTelao(wotm::Scene &scene, const wotm::Ram &ram, const Camera3D &cam)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
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
    if (v.labelObj[0] && v.labelAlpha > 0.f && g_labelRt.id && textPlane(v.labelObj, pl))
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
    if ((!v.on && !v.select) || v.alpha <= 0.f || g_rt.id == 0 || !v.screenObj || !objectPlane(scene, ram, cam, v.screenObj, pl))
        return;
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        std::fprintf(stderr, "[padsel] telao: no %x, %.1f x %.1f, centro %.1f %.1f %.1f\n", v.screenObj, pl.x1 - pl.x0, pl.y1 - pl.y0, pl.c.x, pl.c.y,
                     pl.c.z);
    }
    // escolha de controles: abaixo do logo (de 44% a 96% da altura); selecao de monstros: em cima (a tela 6 nao tem o logo e tem a
    // fileira de retratos embaixo); centrada, com a proporcao da textura
    const float fyTop = v.select ? 0.06f : 0.44f, fyBottom = v.select ? 0.60f : 0.96f;
    const float hy = (pl.y1 - pl.y0) * (fyBottom - fyTop), wx = std::min(hy * float(kW) / float(kH), (pl.x1 - pl.x0) * 0.92f);
    drawOnPlane(pl, g_rt.texture, (pl.x0 + pl.x1) * 0.5f - wx * 0.5f, pl.y1 - (pl.y1 - pl.y0) * fyTop, wx, hy, v.alpha, 0.002f * (pl.x1 - pl.x0));
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
        {wotm::fn::screenGetInput__Fi, &g_origGetInput, &getInputHook},
        {wotm::fn::changeScreen__Fiiiib, &g_origChangeScreen, &changeScreenHook},
        {wotm::fn::screenMain__Fv, &g_origMain, &mainHook},
        {wotm::fn::screenWaitForStart__Fv, &g_origWaitStart, &waitStartHook},
    };
    for (const H &h : hooks)
        if ((*h.orig = runtime.lookupFunction(h.addr)))
            runtime.registerFunction(h.addr, h.fn);
}
