// wotm_padsel.inc: multiplayer menu on the big screen (the "telao"; included by wotm_live.inc, alongside the native drawing).
//
// Main menu (screen 1): the "2 player" item becomes "MULTIPLAYER". Its switch objects (shell->m_menuItemSw[1][2]: the dim and the
// lit text) get alpha 0 and the native label is drawn in the same rectangle on the big screen, lit while the cursor is on it.
// The game still treats the item as 2 player: X goes to screen 5 (screenGameModes2P).
//
// Pad selection (screen 5 coming from the main menu): the big screen shows the connected pads and the keyboard. X (Space on the
// keyboard) joins as the lowest free player (1 to 4); left/right on the d-pad moves the device to another player (two devices on
// the same number are the same player, like the keyboard together with pad 1); triangle (X/Backspace on the keyboard) leaves, and
// from a device that is already out goes back to the main menu (screenGetInput returns code 5). Start (Enter) with 2 or more
// players confirms; the chosen numbers are renumbered in order (1, 3, 4 become 1, 2, 3). After that the port -> devices map in
// wotm_pad.inc is manual (wotm_pad::g_manual, g_manualDevs[player]) until the game goes back to the main menu or to "press start".
//   - 2, 3 or 4 players: as in retail, the "free-for-all / elimination / minigames" items appear (fade of the screen's objects);
//     with 3 and 4 (g_multi) the retail screens stay the same (13 CPUs, 0 to 8 - N, wotm_overrides.cpp ffaOptions2PMore, and 15
//     "first to"; 11 elimination continues; 14 minigame choice) and only the monster selections are ours. Backing out of those
//     screens shows the list again; backing out of the list goes to the main menu. The 3 and 4 player rules for elimination and
//     minigames are in wotm_modes4.inc.
//
// Monster selection for 3 and 4 (screen 6, screenCharSelect2P, with g_multi; in minigames the choice on screen 14 comes here instead
// of screen 7, the arcade one, which has no big screen): screenCharSelect2P runs without input (the big screen's movie keeps playing)
// with screen 6's objects and the costume panels hidden; the big screen shows one frame per player and one per CPU (the ones from
// screen 13, wotm_menucpus::g_want; free for all only). Each player, with their own devices: left/right changes monster (the unlocked
// columns, monsterSelectMode), up/down changes costume (the open ones: state 1 and taken 1), X confirms, triangle undoes (or, with
// nobody ready, goes back to the previous screen). After the columns come the RANDOM and CUSTOM cells (customStep: the list of custom
// monsters, as on screens 3 and 6; a CPU frame only lists those with "ai"; the custom's type goes to m_monsterSel and its costume to
// the frame's).
// After their own, each player picks the CPUs assigned to them, one at a time (CPU c belongs to player c % N: with 3 players and 5
// CPUs, player 1 picks CPUs 1 and 4); triangle on a CPU goes back to the same player's previous pick. In elimination (mode 6) the
// costume is always 0, with no CUSTOM, and each monster goes to only one player (on exit costume 0's SelectSlot is left with taken 0,
// as in retail; the picks go to Shell+0x2B84/0x2B88 and to wotm_modes4.inc for "play again"). All ready: m_monsterSel[i] and
// m_costume[0..1] (players 3 and 4's costumes go to wotm_split4.inc, wotmMultiSetCostume), the CPUs into m_monsterSel[4 + c] and
// m_costume[2 + c], m_numPlayers N, m_numAIs the CPUs, and changeScreen(6, 8) as in retail (stage select); in a minigame,
// changeScreen(6, 0x63) (load, like screenMinigames2P).
//
// EE thread: hooks on screenTransition (hides the items of screens 5 and 6 and the menu label, measures the fade), screenGameModes2P,
// screenCharSelect2P, screenGetInput (no game input during the selections; injects the "back"), changeScreen (the back paths above)
// and screenMain/screenWaitForStart (turn off the manual map and multiplayer). Window: renderUi draws the selection into its own
// texture (game font, drawn icons) and drawOnTelao projects it onto the plane of the big screen's movie object
// (shell->m_movieScreen); the label goes in the item's rectangle.
// Headless test: WOTM_PADSEL_SCRIPT="sec:dur:dev:action;..." (dev 0..3 = pad, kb = keyboard; action left, right, up, down, ok,
// back, start).
#include "wotm_mods.hpp"   // mod skins in the costume list
#include "wotm_menucpus.h"   // CPUs 4..7 from screen 12 (extraCpus)
#include "wotm_training.h"   // TRAINING: screens 12 and 15 skipped (changeScreenHook)
#include <chrono>
#include <random>

void wotmMultiSetCostume(int player, uint32_t costume);   // wotm_split4.inc: costume of players 3 and 4 (the Shell only has room for 2)
void wotmElimSetStart(int player, uint32_t sel);          // wotm_modes4.inc: each player's starting monster in elimination
bool wotmTextMenuOn();   // wotm_mainmenu.inc: text main menu (without this file's "multiplayer" label)

namespace wotm_padsel
{
constexpr int kKb = PS2_PAD_MAX_GAMEPADS;   // index of the keyboard in the device list
constexpr int kDevs = PS2_PAD_MAX_GAMEPADS + 1;
constexpr int kMaxPlayers = 4;
constexpr int kMaxPanels = 1 + wotm_menucpus::kMax;   // frames of the solo selection: the player and up to 7 CPUs (screen 19 with screen 12 at 4..7)
constexpr uint32_t kShell = wotm::sym::shellInfo;
constexpr uint32_t kMovieScreen = kShell + 0x160;   // shell->m_movieScreen
constexpr uint32_t kShellSound = kShell + 0x2918;
constexpr uint32_t kMonsterSel = kShell + 0x2938, kCostume = kShell + 0x2BE0, kNumPlayers = kShell + 0x2BB4, kNumAIs = kShell + 0x2BB8,
                   kMode = kShell + 0x2C10, kBattleMode = kShell + 0x2B54, kSw5CC = kShell + 0x5CC, kCostumeBox = kShell + 0x88;
constexpr uint32_t kSlotSize = 0x2C;   // SelectSlot (monster_select.h): taken +0, state +4; 4 costumes per column
enum Action { Left, Right, Ok, Back, Start, Up, Down, L1, R1, Square, kActions };   // L1/R1: tabs of the wotm_mainmenu.inc panel; Square: script only (VIDEO tab preview)
enum Phase { Off, Config, Reveal, Leaving };
// Monster bar cells past the 10 columns: random monster and custom monster (the mods' custom monsters)
constexpr int kCellRandom = 11, kCellCustom = 12;

// Colors of players 1 to 4
constexpr Color kPlayerColors[kMaxPlayers] = {{230, 120, 50, 255}, {70, 140, 235, 255}, {90, 190, 80, 255}, {230, 200, 60, 255}};
// Names from MonsterNames.h (type = m_typeBits >> 5)
constexpr const char *kMonsterNames[17] = {"",        "CONGAR", "ROBO-47",  "TOGERA", "ULTRA-V", "PREYTOR", "SPIDER", "KINETICLOPS", "MAGMO",
                                           "RAPTROS", "AGAMO",  "ZORGULON", "OGRE",   "ASSBOSS", "JELLY",   "FINAL",  ""};
// Custom monster (mods/<mod>/monsters/, wotm_mods.hpp) that uses `sel`'s type outside the columns (the Goliath: 13); nullptr for playable ones
inline const wotm::mods::Custom *customOfSel(uint32_t sel)
{
    return wotm::mods::monsterInfo(int(sel >> 5)) ? nullptr : wotm::mods::Registry::get().customByType(int(sel >> 5));
}
inline const char *monsterName(uint32_t sel)
{
    if (const wotm::mods::Custom *cu = customOfSel(sel))
    {
        static thread_local char buf[48];
        std::snprintf(buf, sizeof buf, "%s", cu->name.c_str());
        for (char *q = buf; *q; ++q) *q = char(std::toupper(static_cast<unsigned char>(*q)));
        return buf;
    }
    return (sel >> 5) < 17 ? kMonsterNames[sel >> 5] : "";
}

// State the window draws (copied under g_m)
struct View
{
    bool on = false;      // pad selection
    bool select = false;  // monster selection
    float alpha = 0.f;    // of the whole selection (fade in and out)
    bool present[kDevs] = {};
    int slot[kDevs] = {};   // 0 out, 1..8 player
    int count = 0;          // devices present
    int players = 0;        // distinct players chosen
    uint32_t screenObj = 0;
    uint32_t mgObj[24] = {}, mgUsed[2] = {};   // screen 7: objects of the large portraits and the one each player is using (publish)
    // selection
    int selPlayers = 0;
    uint32_t selMonster[kMaxPanels] = {};   // m_monsterSel (type << 5)
    int selCostume[kMaxPanels] = {}, selCostumes[kMaxPanels] = {};
    int selCostumeIdx[kMaxPanels] = {};   // position of the costume in the column's list (-1: not in it)
    int selBase[kMaxPanels] = {};         // disc costume used for framing (the skin's base)
    int selModCount[kMaxPanels] = {};     // mod skins at the end of the list
    char selCostumeName[kMaxPanels][40] = {};
    char selLabel[kMaxPanels][8] = {};    // "P1", "CPU"
    int selColor[kMaxPanels] = {};        // index into kPlayerColors
    bool selWaiting[kMaxPanels] = {};     // CPU frame before its player has picked their own (mode 2)
    bool selNoCostume = false;             // elimination: no costume choice
    bool selReady[kMaxPanels] = {};
    int selCell[kMaxPanels] = {};         // monster bar cell: 1..10 column, kCellRandom, kCellCustom
    int selCustom[kMaxPanels] = {};       // custom shown in the frame (index into Registry::customs(); -1 none)
    bool selList[kMaxPanels] = {};        // custom list open in the frame
    int selListHl[kMaxPanels] = {};       // highlighted line of the list
    bool selCpu[kMaxPanels] = {};         // CPU frame (the list marks customs without "ai")
    int barMon[10] = {};                   // monster (type) of each selection column
    bool barOpen[10] = {};                 // column unlocked
    // main menu label
    uint32_t labelObj[4] = {};   // switch objects of the "2 player" item in use ([0] = 0: no label); the smallest is the text
    uint32_t labelRef[4] = {};   // "1 player" objects in use (text and glow): the smallest gives the column and the letter height
    bool labelLit = false;
    float labelAlpha = 0.f;
    // screen transition with a camera animation (changeScreen with anim != 0): progress 0..1 in the direction it plays
    bool transMoving = false;
    float transProg = 0.f;
};
static std::mutex g_m;
static View g_view;

// EE thread
static Phase g_phase = Off;
static float g_uiAlpha = 0.f, g_reveal = 0.f;
static bool g_transMoving = false;   // the game camera moves during the current transition (transitionHook)
static float g_transProg = 0.f;
static int g_slot[kDevs] = {};
static bool g_prev[kDevs][kActions] = {};
static bool g_primed = false;
static int g_inject = 0;   // code for the next screenGetInput (5 = back)
static int g_multi = 0;    // 3 or 4: players in the chosen multiplayer match (0 = 1 or 2 players, retail)
static PS2Runtime::RecompiledFunction g_origTransition = nullptr, g_origModes2P = nullptr, g_origGetInput = nullptr, g_origMain = nullptr,
                                      g_origWaitStart = nullptr, g_origSelect2P = nullptr, g_origChangeScreen = nullptr;
static uint32_t g_transRa = 0, g_modesRa = 0, g_selectRa = 0, g_mainRa = 0, g_select1Ra = 0, g_soloInRa = 0;
static int g_soloInPad = 0;
static bool g_select1Pending = false, g_soloInPending = false;
static PS2Runtime::RecompiledFunction g_origSelect1P = nullptr;
static bool g_transPending = false, g_modesPending = false, g_selectPending = false, g_mainPending = false;

// Monster selection (EE thread)
struct Pick
{
    int col = 1;       // bar cell: column of the selection screen (1..10), kCellRandom or kCellCustom
    int costume = 0;   // 0..3 or skin (for a custom, its costume)
    bool ready = false;
    // CUSTOM cell (closed, list open, custom chosen; like the one on screens 3 and 6)
    int custom = 0;     // chosen custom (index into Registry::customs() + 1; 0 = none)
    int customHl = 0;   // highlighted line of the list (kept for the next time it opens)
    int customSkin = 0; // position of the costume in customCostumes
    bool list = false;  // list open
};
static bool g_selOn = false, g_selPrimed = false;
static bool g_selLeaving = false;   // selection made (or backed out): screen 6 stays hidden until the fade-out ends
static int g_pickFor = 0;           // players and CPUs the picks in g_pick were made for (a different count: cursors spread out again)
static int g_selCpus = 0;           // CPUs of the 3 and 4 player selection (g_pick[g_multi ..])
static Pick g_pick[kMaxPanels];     // players 0 .. g_multi - 1, then the CPUs
static bool g_selPrev[kMaxPlayers][kActions] = {};
static int g_selDone = 0;   // frames since everyone became ready
// Screen change requested by a selection: done on exit from the screen's function (screenGameModes2P, screenCharSelect2P).
// changeScreen only writes currScreen and screenFirstPass = 1; called before the original function, that function would run the new
// screen's "first pass" in place of its own (and clear it), and sometimes the change didn't happen.
enum After { AfterNone, AfterSelectDone, AfterSelectBack };
static After g_after = AfterNone;

struct ScriptStep { double t0, dur; int dev, act; bool fired = false; };
constexpr int kScriptMouse = -2;   // `mouse` in the script: act = bar cell under the cursor (cellN) or a click on it (100 + N, clickN)
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

// Keyboard always; a pad if connected or named in the test script (fake pads for headless testing)
static bool present(int d)
{
    if (d == kKb || IsGamepadAvailable(d))
        return true;
    for (const ScriptStep &st : g_script)
        if (st.dev == d) return true;
    return false;
}

// The device's actions right now. bound: through the player's key and button map (wotm_settings.hpp; the PS2 buttons X, triangle,
// Start, L1, R1, d-pad and analog stick), as the game sees them; without it, the physical buttons of the default map (for the options
// panel, which can't depend on a map it is itself changing).
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
        // mouse (step 9, cursor released): left confirms, right goes back, wheel moves up and down
        if (!wotm::mouse::g_captured.load(std::memory_order_relaxed))
        {
            out[Ok] = out[Ok] || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !wotm::mouse::claimed());   // on the bar: mouseStep
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

// Calls a game function with up to 5 integer arguments (a0..a3, t0); the context is restored afterwards
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
static void sfx(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, int sound)   // ShellSound::playShellSFXSound (2/4 cursor, 6 back, 8 ok)
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
// Menu camera animation (shell->m_screenAnim[anim]), played the way changeScreen plays it, forwards or in reverse
static void menuAnim(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, int anim, bool reverse)
{
    callGuest(rdram, ctx, rt, reverse ? wotm::fn::animationStartReverse__FG11_animHandleb : wotm::fn::animationStart__FG11_animHandleb,
              kShell + 0x61C + 0x10u * uint32_t(anim), 1u);
    std::memcpy(rdram + wotm::sym::currAnimationIndex, &anim, 4);
}
static uint32_t gameMode(const uint8_t *rdram) { return rd32(rdram, kMode); }
static bool elimination(const uint8_t *rdram) { return gameMode(rdram) == 6u; }   // costume 0, exclusive monster
static bool minigame(const uint8_t *rdram) { return gameMode(rdram) >= 7u && gameMode(rdram) <= 9u; }   // coming from screen 14
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

// ------------------------------------------------------------------ main menu "MULTIPLAYER" label

// Objects of a switch (object children, or the objects of a group child): f(object, is the child in use?)
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
        else if (op == 1)   // GROUP_NODE (_hiergroup): numKids (u16) +8, children +0x20
            for (uint32_t g = 0, n = std::min<uint32_t>(rdram[c + 8] | (rdram[c + 9] << 8), 16u); g < n; ++g)
                if (const uint32_t o = rd32(rdram, c + 0x20 + 4 * g) & 0x1FFFFFFFu; o >= 0x1000u && (rdram[o] & 0x3F) == 0)
                    f(o, k == which);
    }
}

// On screen 1 (and during its fade-out): hides "2 player" and publishes the object in use, whether it's lit, and the fade alpha
// (taken from "1 player")
static void hideTwoPlayerItem(uint8_t *rdram)
{
    if (wotmTextMenuOn())   // the items belong to wotm_mainmenu.inc
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

// ------------------------------------------------------------------ pad selection

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
        std::fprintf(stderr, "[padsel] pads %s\n", on ? "chosen: manual map" : "off: default map");
}

static void configStep(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    bool now[kDevs][kActions] = {};
    for (int d = 0; d < kDevs; ++d)
        if (present(d)) readDevice(d, now[d]);
    if (!g_primed)   // the X that chose the item may still be held: only count presses from now on
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
            std::fprintf(stderr, "[padsel] %s %d: player %d\n", d == kKb ? "keyboard" : "pad", d == kKb ? 0 : d, g_slot[d]);
    }
    std::memcpy(g_prev, now, sizeof g_prev);
    if (back)
    {
        g_phase = Leaving;
        g_inject = 5;   // screenGameModes2P does the rest (sound, m_numPlayers 0, changeScreen back)
        std::fprintf(stderr, "[padsel] back to the main menu\n");
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
                std::fprintf(stderr, "[padsel] player %d: %s %d\n", map[g_slot[d]], d == kKb ? "keyboard" : "pad", d == kKb ? 0 : d);
        wotm_menucpus::g_menuPlayers.store(n);   // the CPU maximum on screen 13 (8 monsters)
        // the retail mode list appears with 2, 3 or 4 (with 3 and 4 the monster selections are ours)
        g_multi = n >= 3 ? n : 0;
        g_phase = Reveal;
        g_reveal = 0.f;
        if (g_multi)
            std::fprintf(stderr, "[padsel] %d players: mode list\n", n);
        return;
    }
    if (moved)
        sfx(rdram, ctx, runtime, moved);
}

// ------------------------------------------------------------------ monster selection (3 and 4 players)

static bool columnOpen(uint8_t *rdram, int col)
{
    if (col < 1 || col > 10)
        return false;
    if (col == 1 || col == 10)   // the first and last columns are unlockable (initSelectSwitches)
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

// Costumes of a column in the selection: the open disc ones (state 1 and taken 1, like isSlotOpen) and then the monster's mod skins
// (mods/<mod>/skins/, always open), minus those another ready player already took (`taken`: monster, costume pairs). `noMods`: disc
// ones only (elimination: costume always 0).
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
// Column costumes: the disc ones the game lets you pick (open and not taken) and the monster's mod skins (always open, repeatable)
static CostumeList costumeList(uint8_t *rdram, int col, bool noMods = false)
{
    CostumeList l;
    if (col < 1 || col > 10)   // random, custom
        return l;
    for (int c = 0; c < 4; ++c)
        if (costumeOpen(rdram, col, c)) l.c[l.n++] = c;
    if (noMods)
        return l;
    const int mon = monsterOfCol(rdram, col);
    for (const wotm::mods::Skin &s : wotm::mods::Registry::get().skins())
    {
        if (s.monster != mon || !s.custom.empty() || l.n >= kMaxCostumes) continue;   // a custom's skins go in its own list
        l.c[l.n++] = s.index;   // mod skin: open and free for any number of players
    }
    return l;
}
// Costume name: "COSTUME n" (disc ones) or the skin's name
static std::string costumeName(int monster, int costume)
{
    if (costume < 4)
        return "COSTUME " + std::to_string(costume + 1);
    const wotm::mods::Skin *s = wotm::mods::Registry::get().skinByIndex(monster, costume);
    std::string n = s ? s->name : "COSTUME " + std::to_string(costume + 1);
    for (char &ch : n) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return n;
}
// Disc costume that provides the framing and the fallback model for a mod costume (the skin's "base")
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
// Neighboring cell on the bar: the open columns, then random and custom, wrapping around
static int stepCell(uint8_t *rdram, int cell, int dir, bool noCustom = false)
{
    const int last = noCustom ? kCellRandom : kCellCustom;
    for (int i = 0; i < last; ++i)
    {
        cell += dir;
        if (cell < 1) cell = last;
        if (cell > last) cell = 1;
        if (cell > 10 || columnOpen(rdram, cell)) return cell;
    }
    return cell;
}
// Random monster: an open column and a costume from its list (mod skins included; disc ones only with `noMods`)
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

// Custom monsters of the CUSTOM cell (mods/<mod>/monsters/, in Registry::customs())
static int customCount() { return int(wotm::mods::Registry::get().customs().size()); }
static const wotm::mods::Custom *customAt(int k)
{
    const auto &cs = wotm::mods::Registry::get().customs();
    return cs.empty() || k < 0 ? nullptr : &cs[size_t(k) % cs.size()];
}

// 3 and 4 player elimination: each monster goes to only one player. During the selection the other frames' "ready" is what counts;
// on exit costume 0 of the chosen columns is left with taken 0 (what screenCharSelect2P does in mode 6; wotm_modes4.inc takes the
// next monster for whoever falls from there). g_elimTaken remembers what the exit marked, so backing out of stage select frees it.
static bool g_elimTaken[10] = {};
static void elimRelease(uint8_t *rdram)
{
    for (int c = 0; c < 10; ++c)
        if (g_elimTaken[c])
        {
            wr32(rdram, wotm::sym::monsterSelectMode + uint32_t(c * 4) * kSlotSize, 1u);
            g_elimTaken[c] = false;
        }
}
static bool elimFree(uint8_t *rdram, int col, int self)
{
    if (col < 1 || col > 10 || !columnOpen(rdram, col) || !costumeOpen(rdram, col, 0))
        return false;
    for (int p = 0; p < g_multi + g_selCpus; ++p)
        if (p != self && g_pick[p].ready && g_pick[p].col == col)
            return false;
    return true;
}
static int elimRandom(uint8_t *rdram, int self)
{
    static std::mt19937 rng(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x5A5Au);
    int open[10], n = 0;
    for (int c = 1; c <= 10; ++c)
        if (elimFree(rdram, c, self)) open[n++] = c;
    return n ? open[rng() % uint32_t(n)] : 0;
}

static void selectBegin(uint8_t *rdram)
{
    g_selOn = true;
    g_selLeaving = false;
    g_selPrimed = false;
    g_selDone = 0;
    // CPUs from screen 13 (at most 8 monsters); elimination and minigames have none
    g_selCpus = gameMode(rdram) != 3u ? 0
                                      : std::clamp(wotm_menucpus::g_want.load(), 0, std::min(wotm_menucpus::maxFor(g_multi), kMaxPanels - g_multi));
    const bool elim = elimination(rdram);
    if (elim)   // back from stage select: the monsters the exit marked as taken are free again
        elimRelease(rdram);
    // cursors spread across the open columns; whatever was already picked (back from stage select, rematch) is kept
    const bool keep = g_pickFor == g_multi * 16 + g_selCpus;
    g_pickFor = g_multi * 16 + g_selCpus;
    int col = stepColumn(rdram, 0, 1);
    for (int p = 0; p < g_multi + g_selCpus; ++p)
    {
        Pick &k = g_pick[p];
        k.list = false;
        const bool keepCustom = keep && !elim && k.col == kCellCustom && k.custom && customAt(k.custom - 1);   // a chosen custom is kept
        if (!keepCustom && (!keep || !columnOpen(rdram, k.col) || costumeList(rdram, k.col, elim).find(elim ? 0 : k.costume) < 0))
        {
            k.col = col;
            k.costume = firstCostume(rdram, col);
            k.custom = k.customSkin = 0;
        }
        if (elim)
            g_pick[p].costume = 0;
        col = stepColumn(rdram, col, 1);
        g_pick[p].ready = false;
    }
    std::fprintf(stderr, "[padsel] monster selection (mode %u) for %d players and %d CPUs\n", gameMode(rdram), g_multi, g_selCpus);
}

// m_monsterSel for the pick: the column's, or the custom's type (its costume is already in Pick::costume)
static uint32_t pickSel(uint8_t *rdram, const Pick &k)
{
    if (k.col == kCellCustom)
        if (const wotm::mods::Custom *cu = customAt(k.custom - 1))
            return uint32_t(cu->type) << 5;
    return rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(k.col - 1));
}

static void selectFinish(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    for (int p = 0; p < g_multi; ++p)
    {
        const uint32_t sel = pickSel(rdram, g_pick[p]);
        wr32(rdram, kMonsterSel + 4u * uint32_t(p), sel);   // players into m_monsterSel[0 .. N) (with 5 to 8 there is no AI)
        if (p < 2)
            wr32(rdram, kCostume + 4u * uint32_t(p), uint32_t(g_pick[p].costume));
        else
            wotmMultiSetCostume(p, uint32_t(g_pick[p].costume));
        std::fprintf(stderr, "[padsel] player %d: %s costume %d\n", p + 1, monsterName(sel), g_pick[p].costume);
        if (elimination(rdram))   // dialog 0xB's "play again" goes back to these (Shell+0x2B84/0x2B88 as in retail; 3 and 4 in modes4)
        {
            if (p < 2)
                wr32(rdram, kShell + 0x2B84 + 4u * uint32_t(p), sel);
            wotmElimSetStart(p, sel);
            wr32(rdram, wotm::sym::monsterSelectMode + uint32_t((g_pick[p].col - 1) * 4) * kSlotSize, 0u);   // taken: in use
            g_elimTaken[g_pick[p].col - 1] = true;
        }
    }
    for (int i = g_multi; i < 12; ++i)
        wr32(rdram, kMonsterSel + 4u * uint32_t(i), 0u);
    for (int c = 0; c < g_selCpus; ++c)   // CPUs into m_monsterSel[4 + c], m_costume[2 + c] (wotm_split4.inc moves them out of the way at load)
    {
        const Pick &k = g_pick[g_multi + c];
        const uint32_t sel = pickSel(rdram, k);
        wr32(rdram, kMonsterSel + 4u * uint32_t(4 + c), sel);
        wr32(rdram, kCostume + 4u * uint32_t(2 + c), uint32_t(k.costume));
        std::fprintf(stderr, "[padsel] CPU%d: %s costume %d\n", c + 1, monsterName(sel), k.costume);
    }
    wr32(rdram, kNumPlayers, uint32_t(g_multi));
    wr32(rdram, kNumAIs, uint32_t(g_selCpus));
    g_selLeaving = true;
    sfx(rdram, ctx, runtime, 8);
    g_after = AfterSelectDone;   // changeScreen(6, 8) on exit from screenCharSelect2P
}

// Mouse on the monster bar (cursor released): the window (drawBar2d) puts the cell under the cursor here (1..12, open ones only;
// 0 = none) and announces (claim) that the left click is its own; the EE thread moves the keyboard player's frame toward that cell,
// one cell per frame, when the mouse moves or clicks, and the click confirms on arrival (on random, it rolls). A key or button press
// drops the target.
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
// Mouse step for the frame at cell `cur`: 3 left, 4 right, 6 confirm, 0 nothing
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

// Input on the CUSTOM cell of a frame in the 3 and 4 player selection, in three steps as on screens 3 and 6 (customDecide): closed,
// X opens the list (up/down, X picks, triangle closes); once picked, up/down change costume (the monster.json one and its skins), X
// confirms and triangle goes back to the closed cell. `cpu`: CPU frame (only customs with "ai"). Returns the sound (0: nothing done).
static int customStep(Pick &k, const bool *e, bool cpu)
{
    const int nc = customCount();
    if (nc <= 0)
        return 0;
    if (k.list)
    {
        if (e[Up] || e[Down])
        {
            if (nc < 2)
                return 0;
            k.customHl = (k.customHl + (e[Up] ? -1 : 1) + nc) % nc;
            return e[Up] ? 2 : 4;
        }
        if (e[Back])
        {
            k.list = false;
            return 6;
        }
        const wotm::mods::Custom *cu = customAt(k.customHl);
        if (!e[Ok] || !cu || (cpu && !cu->ai))   // a boss's AI needs its own stage
            return 0;
        k.list = false;
        k.custom = k.customHl % nc + 1;
        k.customSkin = 0;
        k.costume = cu->costume;   // the first of customCostumes
        std::fprintf(stderr, "[padsel] custom in the list: %s (base %s, %zu costumes)\n", cu->id.c_str(), cu->typeName.c_str(),
                     wotm::mods::Registry::get().customCostumes(*cu).size());
        return 8;
    }
    if (const wotm::mods::Custom *cu = customAt(k.custom - 1))
    {
        if (e[Up] || e[Down])
        {
            const std::vector<int> l = wotm::mods::Registry::get().customCostumes(*cu);
            if (l.size() < 2)
                return 0;
            const int n = int(l.size());
            k.customSkin = (k.customSkin + (e[Up] ? -1 : 1) + n) % n;
            k.costume = l[size_t(k.customSkin)];
            return e[Up] ? 2 : 4;
        }
        if (e[Back])
        {
            k.custom = k.customSkin = 0;
            return 6;
        }
        if (!e[Ok])
            return 0;
        k.ready = true;
        std::fprintf(stderr, "[padsel] custom: %s (base %s, costume %d)\n", cu->id.c_str(), cu->typeName.c_str(), k.costume);
        return 8;
    }
    if (!e[Ok])
        return 0;
    k.list = true;
    k.customHl %= nc;
    return 8;
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
    const bool elim = elimination(rdram);
    for (int p = 0; p < g_multi; ++p)
    {
        bool e[kActions];
        for (int a = 0; a < kActions; ++a)
            e[a] = now[p][a] && !g_selPrev[p][a];
        // the player's queue: their own and the CPUs assigned to them (p, p + N, p + 2N...); picks the first one not ready
        const int total = g_multi + g_selCpus;
        int q[kMaxPanels], nq = 0;
        for (int j = p; j < total; j += g_multi)
            q[nq++] = j;
        int at = 0;
        while (at < nq - 1 && g_pick[q[at]].ready)
            ++at;
        const Pick &cur = g_pick[q[at]];
        const bool customOpen = cur.col == kCellCustom && (cur.list || cur.custom);   // triangle belongs to the CUSTOM cell
        if (at > 0 && !cur.ready && e[Back] && !customOpen)
        {
            g_pick[q[at - 1]].ready = false;   // CPU: triangle undoes the same player's previous pick
            sound = 6;
            continue;
        }
        Pick &k = g_pick[q[at]];
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
        if ((e[Left] || e[Right]) && !(k.col == kCellCustom && k.list))   // list open: only up/down, X and triangle
        {
            k.col = stepCell(rdram, k.col, e[Left] ? -1 : 1, elim);
            k.costume = k.col <= 10 && !elim ? firstCostume(rdram, k.col) : 0;
            k.custom = k.customSkin = 0;
            sound = e[Left] ? 2 : 4;
        }
        if (k.col == kCellCustom)
        {
            if (const int s = customStep(k, e, q[at] >= g_multi))
                sound = s;
            else if (e[Back] && !k.custom && !k.list)
                back = true;   // closed cell: triangle goes back a screen, as on the columns
            continue;
        }
        if (k.col > 10)   // random: X rolls a monster and costume and becomes ready
        {
            if (e[Ok] && k.col == kCellRandom)
            {
                if (elim)   // elimination: a monster nobody took, costume 0
                {
                    if (const int c = elimRandom(rdram, q[at]))
                        k.col = c, k.costume = 0;
                }
                else
                    randomPick(rdram, k.col, k.costume, false);
                if (k.col <= 10)
                {
                    k.ready = true;
                    sound = 8;
                    wotm_narrator::announce(monsterOfCol(rdram, k.col));
                }
            }
            if (e[Back]) back = true;
            continue;
        }
        if (elim)   // fixed costume; X only on a free monster
        {
            if (e[Ok] && elimFree(rdram, k.col, q[at]))
            {
                k.costume = 0;
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
    for (int p = 0; p < g_multi + g_selCpus; ++p)
    {
        anyReady = anyReady || g_pick[p].ready;
        allReady = allReady && g_pick[p].ready;
    }
    if (back && !anyReady)
    {
        g_selLeaving = true;
        sfx(rdram, ctx, runtime, 6);
        g_after = AfterSelectBack;   // changeScreen to the previous screen (15, 11 or 14) on exit from screenCharSelect2P
        std::fprintf(stderr, "[padsel] selection: back to the previous screen\n");
        return;
    }
    if (sound)
        sfx(rdram, ctx, runtime, sound);
    g_selDone = allReady ? g_selDone + 1 : 0;
    if (g_selDone > 30)
        selectFinish(rdram, ctx, runtime);
}

// ------------------------------------------------------------------ 1 and 2 player selection (screens 3 and 6)
// The logic is the game's (screenCharSelect1P / screenCharSelect2P: modes, CPU, elimination, what happens on confirm); the native
// layer hides the screen, shows the frames on the big screen as in the 3 and 4 player selection, and handles the costume: up/down
// walk a list with the open disc costumes and the mod skins. The game's costume cursor stays on the chosen costume (0..3) or, for a
// skin, on the first open disc costume (the "proxy": the game confirms through it); on confirm the frame's m_costume receives the
// skin's costume. Left/right go, after the last open column, through random and custom (cells that only we have, g_soloExtra). The
// game's portrait row gives way to the 2D bar (drawBar2d, hideStrip) and the big screen goes black with the sheldemo (selectMovie).
// WOTM_NATSEL=0 goes back to the game's screens.
static int g_solo = 0;   // 3, 6, 7, 18 or 19: the game screen the native selection is showing
static bool g_soloLeaving = false;

static bool soloEnabled()
{
    static const bool on = [] {
        const char *e = std::getenv("WOTM_NATSEL");
        return !(e && std::strcmp(e, "0") == 0);
    }();
    return on;
}

// Repeated monster and disc costume in the native selection (WOTM_SEL_REPEAT=0 restores the game's rule: each disc costume to one only)
static bool repeatAllowed()
{
    static const bool on = [] {
        const char *e = std::getenv("WOTM_SEL_REPEAT");
        return !(e && std::strcmp(e, "0") == 0);
    }();
    return on;
}

// One frame of the game's selection: where the screen keeps the column, the costume cursor and "ready", and where the pick goes
struct SoloPanel
{
    uint32_t col, cur, done, costumeDst, monsterDst;
    int pad;        // screenGetInput(pad) that controls it
    bool ai;        // the mode 2 CPU (player 1 picks for it)
    bool waiting;   // CPU before player 1 has picked their own
    int pick = -1;  // >= 0: CPU 3 onwards, already picked and stored in g_xPick[pick] (the game has no state for it)
};

// More CPUs on screen 19 than the game knows about (wotm_menucpus.h): 4 to 7 in single player (screen 12, mode 2) and 3 to 6 with 2
// players (screen 13). The game only has state for 3 CPUs (1 player) or 2 (2 players); the frame of its last CPU (the screen's 4th
// frame: curCostume4 and aiThreeIsDone with 1 player, aiTwoIsDone with 2) stands in for every CPU from there on, one at a time: when
// it confirms and CPUs are still missing, soloSync stores the pick in g_xPick, clears "ready", and the same pad (1 with 1 player, 2
// with 2) picks the next one on the same column. Triangle goes back one CPU (soloInput). While CPUs are missing, the screen's input
// holds off watchForASecond (the game's countdown to leave for the stage). Once the stage is confirmed, extraCpus writes the picks
// into the CPUs.
struct ExtraPick
{
    uint32_t monster, costume;   // m_monsterSel and m_costume the game (and soloSync) wrote for the live frame
    int col, cur;                // the game's column and costume cursor at confirm
    bool freeOnBack;             // disc slot taken by the pick (skin and custom already gave it back)
};
static ExtraPick g_xPick[wotm_menucpus::kMax] = {};
static int g_xStage = 0;   // CPUs stored in g_xPick (the live frame is the 4th frame + g_xStage)
static int g_xDone = 0;    // valid picks in g_xPick for extraCpus (g_xStage on exit from screen 19)
// Screen 19 with more CPUs than the game knows about: how many were requested, how many the game knows, and its live frame
struct XCfg
{
    int want = 0, base = 0;   // CPUs requested and the game's own (3 with 1 player, 2 with 2)
    uint32_t done = 0, cur = 0, col = 0;
    int pad = 0;              // pad that picks the extra CPUs
};
static XCfg extraCfg(uint8_t *rdram)
{
    XCfg x;
    if (g_solo != 19)
        return x;
    const int want = wotm_menucpus::g_want.load();
    if (rd32(rdram, kMode) == 2u)
        x = {want, 3, wotm::sym::aiThreeIsDone_3015, wotm::sym::curCostume4_3010, wotm::sym::currentSelection + 4u * 19u, 1};
    else if (rd32(rdram, kNumPlayers) == 2u)
        x = {want, 2, wotm::sym::aiTwoIsDone_3014, wotm::sym::curCostume4_3010, wotm::sym::currentSelection + 4u * (22u + 19u), 2};
    if (x.want <= x.base || x.want > wotm_menucpus::maxFor(x.base == 3 ? 1 : 2))
        x = XCfg{};
    return x;
}

// Screen 19's 4 frames plus those of the extra CPUs: the stored ones, the live frame (the 4th, the game's last CPU) and the waiting ones
static int extraPanels(uint8_t *rdram, SoloPanel out[kMaxPanels], int n)
{
    const XCfg x = extraCfg(rdram);
    if (!x.want)
        return n;
    const int last = 3 + x.want - x.base, live = 3 + std::min(g_xStage, x.want - x.base);
    const bool liveWaiting = out[3].waiting;
    for (int i = 3; i <= last; ++i)
    {
        out[i] = out[3];
        out[i].waiting = i > live || (i == live && g_xStage == 0 && liveWaiting);
        out[i].pick = i < live ? i - 3 : -1;
    }
    return last + 1;
}

static int soloPanels(uint8_t *rdram, SoloPanel out[kMaxPanels], bool &noCostume)
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
        // Screen 7 (screenMinigames2P): each pad picks its own; the minigame load uses each player's m_costume
        out[0] = {currentSelection + 4u * 7u, curCostume1_2912, playerOneIsDone_2914, kCostume, kMonsterSel, 1, false, false};
        out[1] = {currentSelection + 4u * 29u, curCostume2_2913, playerTwoIsDone_2915, kCostume + 4u, kMonsterSel + 4u, 2, false, false};
        return 2;
    }
    const uint32_t mode = rd32(rdram, kMode);
    noCostume = mode == 6;   // elimination: the game sets costume 0
    auto on = [&](uint32_t a) { return rd32(rdram, a) != 0; };
    if (g_solo == 18 || g_solo == 19)
    {
        // Screens 18 (screenCharSelect3P, 3 monsters) and 19 (screenCharSelect4P, 4), coming from "first to" with CPUs. Mode 2 (1
        // player): pad 1 picks its own and then each CPU, one at a time, on the same column. Otherwise (2 players): each pad picks its
        // own; on 18 pad 1 picks the CPU after both; on 19 pad 1 picks the 1st CPU after both and pad 2 the 2nd after its own.
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
            return four ? extraPanels(rdram, out, n) : n;
        }
        out[0] = {c1, cur[0], p1, kCostume, kMonsterSel, 1, false, false};
        out[1] = {c2, cur[1], p2, kCostume + 4u, kMonsterSel + 4u, 2, false, false};
        out[2] = {c1, cur[2], a1, kCostume + 8u, kMonsterSel + 16u, 1, true, !(on(p1) && on(p2))};
        if (!four)
            return 3;
        out[3] = {c2, cur[3], a2, kCostume + 12u, kMonsterSel + 20u, 2, true, !on(p2)};
        return extraPanels(rdram, out, 4);
    }
    // currentSelection: player 1's column for each screen, and player 2's starting at index 22
    out[0] = {currentSelection + 4u * 6u, curCostume1_2894, playerOneIsDone_2896, kCostume, kMonsterSel, 1, false, false};
    if (mode == 2)   // 1 player and 1 CPU: pad 1 picks the CPU on the same column, after its own
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
static int g_soloSel[kMaxPanels] = {}, g_soloCol[kMaxPanels] = {-1, -1, -1, -1, -1, -1, -1, -1};   // chosen costume (0..3 or skin) and the column it was chosen on
static bool g_soloDonePrev[kMaxPanels] = {};
static bool g_soloPrimed = false;   // the screen's first soloSync: a "ready" that was already set doesn't call the narrator
static uint32_t g_soloTick = 0;
// Extra cell the frame is on (0: the game's column; kCellRandom, kCellCustom: the game's cursor stays on the column it left from)
static int g_soloExtra[kMaxPanels] = {};
// CUSTOM cell, in three steps: closed ("+ CUSTOM"); X opens the custom list inside the frame (up/down, X picks, triangle closes);
// once picked, the frame shows the custom and up/down change costume (the monster.json one and its skins), X confirms and triangle
// goes back to the closed cell. On confirm the game's cursor stays on the column the frame left from (the proxy, with the first free
// costume) and the game confirms; soloSync swaps m_monsterSel and m_costume for the custom's and gives back the proxy's slot, as
// with skins.
//  g_soloCustom: highlighted line of the list (index into Registry::customs(); kept for the next time the list opens)
//  g_soloList: list open; g_soloChosen: chosen custom (index + 1; 0 = none); g_soloCustomSkin: position in customCostumes
//  g_soloCustomPick / g_soloCustomCostume: what X confirmed (index + 1) and its costume
static int g_soloCustom[kMaxPanels] = {}, g_soloCustomPick[kMaxPanels] = {}, g_soloChosen[kMaxPanels] = {},
           g_soloCustomSkin[kMaxPanels] = {}, g_soloCustomCostume[kMaxPanels] = {};
static bool g_soloList[kMaxPanels] = {};
// Chosen costume of the frame's custom (the monster.json one if the position fell outside the list)
static int chosenCostume(int who)
{
    const wotm::mods::Custom *cu = customAt(g_soloChosen[who] - 1);
    if (!cu)
        return 0;
    const std::vector<int> l = wotm::mods::Registry::get().customCostumes(*cu);
    return l[size_t(g_soloCustomSkin[who]) % l.size()];
}
static void customReset(int who)
{
    g_soloList[who] = false;
    g_soloChosen[who] = 0;
    g_soloCustomSkin[who] = 0;
}
// screenGetInput code already decided this frame, per pad (screen 3 reads the pad twice per frame)
static uint32_t g_soloCodeTick[3] = {}, g_soloCodeIn[3] = {}, g_soloCodeOut[3] = {};

static void soloBegin(int screen)
{
    g_solo = screen;
    g_soloLeaving = false;
    g_soloPrimed = false;
    g_xStage = g_xDone = 0;
    for (int i = 0; i < kMaxPanels; ++i)
    {
        g_soloCol[i] = -1;
        g_soloDonePrev[i] = false;
        g_soloExtra[i] = 0;
        g_soloCustomPick[i] = 0;
        customReset(i);
    }
    std::fprintf(stderr, "[padsel] native selection for screen %d\n", screen);
}

// Label of frame i: P1, P2, CPU (or CPU1, CPU2... when there is more than one)
static const char *soloLabel(const SoloPanel *pn, int n, int i)
{
    static char buf[kMaxPanels][8];
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

// On exit from the screen's function, every frame: follows the game's cursor and, on confirm with a skin, writes the skin's costume
static void soloSync(uint8_t *rdram)
{
    SoloPanel pn[kMaxPanels];
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
        if (pn[i].pick >= 0)
            continue;   // CPU already stored in g_xPick
        const bool done = rd32(rdram, pn[i].done) != 0;
        const int col = int(rd32(rdram, pn[i].col)), cur = int(rd32(rdram, pn[i].cur));
        if (done && !g_soloDonePrev[i] && g_soloCustomPick[i] && g_soloCol[i] == col)
        {
            // CUSTOM cell: the game confirmed the proxy column; the custom's type and costume apply, and the proxy's slot is freed
            const wotm::mods::Custom &cu = *customAt(g_soloCustomPick[i] - 1);
            wr32(rdram, pn[i].monsterDst, uint32_t(cu.type) << 5);
            wr32(rdram, pn[i].costumeDst, uint32_t(g_soloCustomCostume[i]));
            wr32(rdram, wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4 + cur) * kSlotSize, 1u);
            std::fprintf(stderr, "[padsel] %s: custom %s (type %d costume %d)\n", soloLabel(pn, n, i), cu.id.c_str(), cu.type,
                         g_soloCustomCostume[i]);
        }
        else if (!done && g_soloDonePrev[i])
            g_soloCustomPick[i] = 0;   // backed out (un-readied)
        if (!done)
        {
            if (col != g_soloCol[i] || noCostume)
                g_soloSel[i] = cur, g_soloCol[i] = col;
            else if (g_soloSel[i] < 4 && g_soloSel[i] != cur)   // the game moved the cursor (column change, a costume that stopped being valid)
                g_soloSel[i] = cur;
            else if (g_soloSel[i] >= 4 && costumeList(rdram, col).find(g_soloSel[i]) < 0)
                g_soloSel[i] = cur;
        }
        else if (!g_soloDonePrev[i] && !g_soloCustomPick[i] && g_soloSel[i] >= 4 && !noCostume && g_soloCol[i] == col)
        {
            wr32(rdram, pn[i].costumeDst, uint32_t(g_soloSel[i]));
            // the game marked the proxy as taken (taken 0); it stays free for the other frame
            wr32(rdram, wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4 + cur) * kSlotSize, 1u);
            const int mon = monsterOfCol(rdram, col);
            std::fprintf(stderr, "[padsel] %s: %s costume %d (%s)\n", soloLabel(pn, n, i), kMonsterNames[mon],
                         g_soloSel[i], costumeName(mon, g_soloSel[i]).c_str());
        }
        else if (done && !g_soloDonePrev[i] && !g_soloCustomPick[i] && repeatAllowed() && col >= 1 && col <= 10)
            // disc costume: the game marked the slot as taken; it is freed again for the other frames (repeats)
            wr32(rdram, wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4 + cur) * kSlotSize, 1u);
        if (done && !g_soloDonePrev[i])
        {
            std::fprintf(stderr, "[padsel] ready (%s): column %d, m_monsterSel %s, costume %u\n", soloLabel(pn, n, i), col,
                         monsterName(rd32(rdram, pn[i].monsterDst)), rd32(rdram, pn[i].costumeDst));
            const uint32_t msel = rd32(rdram, pn[i].monsterDst);
            if (!g_soloPrimed)
                ;
            else if (const wotm::mods::Custom *cu = g_soloCustomPick[i] ? customAt(g_soloCustomPick[i] - 1) : nullptr)
                wotm_narrator::announce(cu->narrator, cu->type);
            else
                wotm_narrator::announce(col >= 1 && col <= 10 ? monsterOfCol(rdram, col) : int(msel >> 5));
        }
        g_soloDonePrev[i] = done;
    }
    if (const XCfg x = extraCfg(rdram); x.want)
    {
        // the live frame's CPU confirmed and CPUs are still missing: store the pick and the live frame becomes the next CPU
        const int live = 3 + g_xStage;
        if (g_xStage < x.want - x.base && live < n && !pn[live].waiting && g_soloDonePrev[live] && rd32(rdram, x.done))
        {
            ExtraPick &e = g_xPick[g_xStage];
            e.monster = rd32(rdram, pn[live].monsterDst);
            e.costume = rd32(rdram, pn[live].costumeDst);
            e.col = int(rd32(rdram, pn[live].col));
            e.cur = int(rd32(rdram, pn[live].cur));
            e.freeOnBack = !g_soloCustomPick[live] && g_soloSel[live] < 4 && !repeatAllowed();
            wr32(rdram, x.done, 0);
            ++g_xStage;
            g_soloCol[live + 1] = -1;
            g_soloDonePrev[live + 1] = false;
            g_soloExtra[live + 1] = 0;
            g_soloCustomPick[live + 1] = 0;
            customReset(live + 1);
            const int cpu = live - 3 + x.base;
            std::fprintf(stderr, "[padsel] CPU%d stored (%s costume %u); picking CPU%d of %d\n", cpu, monsterName(e.monster),
                         e.costume, cpu + 1, x.want);
        }
        g_xDone = g_xStage;
    }
    g_soloPrimed = true;
}

// Triangle (from the extra CPUs' pad) on an extra CPU's live frame: goes back to the previous CPU, with the game's cursor on its
// pick (the disc slot it took is freed again, as when backing out in the game)
static bool extraBack(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, int pad)
{
    const XCfg x = extraCfg(rdram);
    if (!x.want || pad != x.pad || g_xStage <= 0 || rd32(rdram, x.done))
        return false;
    const int live = 3 + --g_xStage;
    const ExtraPick &e = g_xPick[g_xStage];
    wr32(rdram, x.col, uint32_t(e.col));
    wr32(rdram, x.cur, uint32_t(e.cur));
    if (e.freeOnBack)
        wr32(rdram, wotm::sym::monsterSelectMode + uint32_t((e.col - 1) * 4 + e.cur) * kSlotSize, 1u);
    g_soloCol[live] = e.col;   // g_soloSel, g_soloExtra and the frame's chosen custom stayed as they were at confirm
    g_soloDonePrev[live] = false;
    g_soloCustomPick[live] = 0;
    g_soloCol[live + 1] = -1;
    g_soloDonePrev[live + 1] = false;
    g_xDone = g_xStage;
    sfx(rdram, ctx, runtime, 6);
    std::fprintf(stderr, "[padsel] back to CPU%d\n", live - 3 + x.base);
    return true;
}

// Puts the frame's game cursor on column `col` with costume `sel` (0..3 or skin; for a skin, the first open disc costume stands in
// for it)
static void soloSetCursor(uint8_t *rdram, const SoloPanel &p, int who, int col, int sel)
{
    const CostumeList l = costumeList(rdram, col);
    wr32(rdram, p.col, uint32_t(col));
    wr32(rdram, p.cur, uint32_t(sel < 4 ? sel : (l.n ? l.c[0] : firstCostume(rdram, col))));
    g_soloCol[who] = col;
    g_soloSel[who] = sel;
}

// Input on the CUSTOM cell (up/down 1/2, triangle 5, X 6; left/right are handled by soloDecide). Returns the code for the game.
static uint32_t customDecide(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, const SoloPanel &p, int who, int col, uint32_t code)
{
    const int nc = customCount();
    if (g_soloList[who])
    {
        if (code == 1 || code == 2)
        {
            if (nc > 1)
            {
                g_soloCustom[who] = (g_soloCustom[who] + (code == 1 ? -1 : 1) + nc) % nc;
                sfx(rdram, ctx, runtime, code == 1 ? 2 : 4);
            }
            return 0;
        }
        if (code == 5)
        {
            g_soloList[who] = false;
            sfx(rdram, ctx, runtime, 6);
            return 0;
        }
        const wotm::mods::Custom *cu = customAt(g_soloCustom[who]);
        if (!cu || (p.ai && !cu->ai))   // CPU frame: only customs with "ai" (a boss's AI needs its own stage)
            return 0;
        g_soloList[who] = false;
        g_soloChosen[who] = g_soloCustom[who] % nc + 1;
        g_soloCustomSkin[who] = 0;
        sfx(rdram, ctx, runtime, 8);
        std::fprintf(stderr, "[padsel] custom in the list: %s (base %s, %zu costumes)\n", cu->id.c_str(), cu->typeName.c_str(),
                     wotm::mods::Registry::get().customCostumes(*cu).size());
        return 0;
    }
    if (g_soloChosen[who])
    {
        const wotm::mods::Custom *cu = customAt(g_soloChosen[who] - 1);
        if (code == 1 || code == 2)
        {
            const int n = int(wotm::mods::Registry::get().customCostumes(*cu).size());
            if (n > 1)
            {
                g_soloCustomSkin[who] = (g_soloCustomSkin[who] + (code == 1 ? -1 : 1) + n) % n;
                sfx(rdram, ctx, runtime, code == 1 ? 2 : 4);
            }
            return 0;
        }
        if (code == 5)
        {
            customReset(who);   // back to the closed cell (triangle doesn't leave the screen)
            sfx(rdram, ctx, runtime, 6);
            return 0;
        }
        g_soloCustomPick[who] = g_soloChosen[who];
        g_soloCustomCostume[who] = chosenCostume(who);
        soloSetCursor(rdram, p, who, col, firstCostume(rdram, col));   // the proxy column, with a free costume
        std::fprintf(stderr, "[padsel] custom: %s (base %s, costume %d)\n", cu->id.c_str(), cu->typeName.c_str(), g_soloCustomCostume[who]);
        return code;   // the game confirms (and plays the sound)
    }
    if (code == 6 && nc > 0)
    {
        g_soloList[who] = true;
        g_soloCustom[who] %= nc;
        sfx(rdram, ctx, runtime, 8);
        return 0;
    }
    return code == 5 ? code : 0;   // closed cell: triangle goes back a screen, as on the columns
}

// Decides the code for an input on the frame the pad is picking:
//  - up/down (1/2) walk the costume list (the game gets 0);
//  - left/right (3/4) walk the game's columns and, after the last open one, random and custom (our own cells: the game's cursor
//    stays put; when leaving them the native layer puts the cursor on the first or last column);
//  - X (6) on random rolls a monster and costume, puts the game's cursor on them and lets the X through; on custom it does nothing.
static uint32_t soloDecide(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, int pad, uint32_t code)
{
    SoloPanel pn[kMaxPanels];
    bool noCostume;
    const int n = soloPanels(rdram, pn, noCostume);
    int who = -1;
    for (int i = 0; i < n && who < 0; ++i)
        if (pn[i].pad == pad && !pn[i].waiting && pn[i].pick < 0 && !rd32(rdram, pn[i].done))
            who = i;
    if (who < 0)
        return code;   // ready: the game just plays the sound
    const SoloPanel &p = pn[who];
    const int col = int(rd32(rdram, p.col));
    if (g_soloCol[who] != col)
        g_soloCol[who] = col, g_soloSel[who] = int(rd32(rdram, p.cur));
    int &ex = g_soloExtra[who];
    const int first = stepColumn(rdram, 0, 1), last = stepColumn(rdram, 11, -1);
    if (ex == kCellCustom && code != 3 && code != 4)
        return customDecide(rdram, ctx, runtime, p, who, col, code);
    if (code == 5)
        return code;
    if (code == 3 || code == 4)
    {
        if (ex == kCellCustom && g_soloList[who])
            return 0;   // list open: only up/down, X and triangle
        g_soloCustomPick[who] = 0;
        customReset(who);
        const int dir = code == 3 ? -1 : 1, from = ex ? ex : col, to = stepCell(rdram, from, dir);
        if (to <= 10 && from <= 10 && !(dir > 0 && from == last) && !(dir < 0 && from == first))
            return code;   // column to column: the game moves (and plays the sound)
        ex = to > 10 ? to : 0;
        if (to <= 10)
            soloSetCursor(rdram, p, who, to, firstCostume(rdram, to));   // back from the extra cells to the first or last column
        sfx(rdram, ctx, runtime, dir < 0 ? 2 : 4);
        return 0;
    }
    if (code == 6)
    {
        if (ex == kCellRandom)
        {
            int c, s;
            randomPick(rdram, c, s, noCostume);
            ex = 0;
            soloSetCursor(rdram, p, who, c, s);
            std::fprintf(stderr, "[padsel] random: %s costume %d\n", kMonsterNames[monsterOfCol(rdram, c)], s);
        }
        return code;
    }
    // up/down
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

// screenGetInput(pad) on screens 3 and 6 (screen 3 reads the pad twice per frame: the second read gets the same decision)
static uint32_t soloInput(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime, int pad, uint32_t code)
{
    // mouse on the bar: the keyboard player's idle pad becomes the mouse step (once per frame; the second read repeats it)
    if (pad >= 1 && pad <= 2 && pad == wotm::mouse::g_kbPlayer.load(std::memory_order_relaxed) + 1)
    {
        static uint32_t s_tick = ~0u, s_code = 0;
        if (code != 0)
            mouseDrop();
        else if (s_tick == g_soloTick)
            code = s_code;
        else
        {
            SoloPanel pn[kMaxPanels];
            bool noCostume;
            const int n = soloPanels(rdram, pn, noCostume);
            int who = -1;
            for (int i = 0; i < n && who < 0; ++i)
                if (pn[i].pad == pad && !pn[i].waiting && pn[i].pick < 0 && !rd32(rdram, pn[i].done))
                    who = i;
            if (who < 0)
                mouseDrop();
            else
                code = uint32_t(mouseStep(g_soloExtra[who] ? g_soloExtra[who] : int(rd32(rdram, pn[who].col))));
            s_tick = g_soloTick;
            s_code = code;
        }
    }
    if (code < 1 || code > 6 || pad < 1 || pad > 2)
        return code;   // triangle (5) passes through: the CUSTOM cell's list uses it (soloDecide returns it on the other cells)
    if (g_soloCodeTick[pad] == g_soloTick && g_soloCodeIn[pad] == code)
        return g_soloCodeOut[pad];
    uint32_t out = soloDecide(rdram, ctx, runtime, pad, code);
    if (out == 5 && extraBack(rdram, ctx, runtime, pad))
        out = 0;   // extra CPU: back to the previous CPU (the game would go back to the frame before its own)
    g_soloCodeTick[pad] = g_soloTick;
    g_soloCodeIn[pad] = code;
    g_soloCodeOut[pad] = out;
    return out;
}

// Costume of frame p in the View (column list, name, base)
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

// Custom `k` (index into Registry::customs()) in frame p of the View, with costume `costume` (the monster and its costume range);
// does nothing when k is outside the list
static void fillCustom(View &v, int p, int k, int costume)
{
    const wotm::mods::Custom *cu = customAt(k);
    if (!cu)
        return;
    const std::vector<int> l = wotm::mods::Registry::get().customCostumes(*cu);
    v.selCustom[p] = k;
    v.selMonster[p] = uint32_t(cu->type) << 5;
    v.selCostume[p] = costume;
    v.selCostumes[p] = int(l.size());
    v.selModCount[p] = int(l.size()) - 1;
    v.selCostumeIdx[p] = int(std::find(l.begin(), l.end(), costume) - l.begin());
    v.selBase[p] = baseCostume(cu->type, costume);
    const std::string nm = costume == cu->costume ? "FROM MOD " + cu->mod : costumeName(cu->type, costume);
    std::snprintf(v.selCostumeName[p], sizeof v.selCostumeName[p], "%s", nm.c_str());
    for (char *q = v.selCostumeName[p]; *q; ++q) *q = char(std::toupper(static_cast<unsigned char>(*q)));
}

static void publishSolo(uint8_t *rdram, View &v)
{
    SoloPanel pn[kMaxPanels];
    bool noCostume;
    const int n = soloPanels(rdram, pn, noCostume);
    v.selPlayers = n;
    v.selNoCostume = noCostume;
    for (int i = 0; i < n; ++i)
    {
        const ExtraPick *xp = pn[i].pick >= 0 ? &g_xPick[pn[i].pick] : nullptr;   // CPU 3 onwards, already stored
        const bool done = xp || (!pn[i].waiting && rd32(rdram, pn[i].done) != 0);
        v.selReady[i] = done;
        v.selWaiting[i] = pn[i].waiting;
        v.selColor[i] = pn[i].ai ? -1 : pn[i].pad - 1;
        std::snprintf(v.selLabel[i], sizeof v.selLabel[i], "%s", soloLabel(pn, n, i));
        int col;
        if (done)
        {
            v.selMonster[i] = xp ? xp->monster : rd32(rdram, pn[i].monsterDst);
            v.selCostume[i] = int(xp ? xp->costume : rd32(rdram, pn[i].costumeDst));
            col = colOfMonster(rdram, v.selMonster[i]);
        }
        else
        {
            col = int(rd32(rdram, pn[i].col));
            v.selMonster[i] = col >= 1 && col <= 10 ? rd32(rdram, wotm::sym::monsterNametomonsterId + 4u * uint32_t(col - 1)) : 0u;
            v.selCostume[i] = g_soloCol[i] == col ? g_soloSel[i] : int(rd32(rdram, pn[i].cur));
        }
        const bool customDone = done && g_soloCustomPick[i] && customAt(g_soloCustomPick[i] - 1);
        v.selCell[i] = customDone ? kCellCustom
                       : done     ? (col ? col : customOfSel(v.selMonster[i]) ? kCellCustom : 0)
                       : g_soloExtra[i] ? g_soloExtra[i] : col;
        v.selCustom[i] = -1;
        v.selList[i] = false;
        v.selCpu[i] = pn[i].ai;
        if (!pn[i].waiting && v.selCell[i] == kCellCustom && customCount() > 0)
        {
            // CUSTOM cell: list open (no monster in the frame), custom chosen or confirmed (the monster and its costume range), or
            // closed ("+ CUSTOM")
            v.selListHl[i] = g_soloCustom[i] % customCount();
            const int k = customDone ? g_soloCustomPick[i] - 1 : !done && !g_soloList[i] ? g_soloChosen[i] - 1 : -1;
            v.selList[i] = !done && g_soloList[i];
            v.selMonster[i] = 0;
            fillCustom(v, i, k, customDone ? g_soloCustomCostume[i] : chosenCostume(i));
            continue;
        }
        if (pn[i].waiting || v.selCell[i] > 10)
        {
            v.selMonster[i] = 0;
            col = 0;   // empty list
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
    // 2 player minigames (screen 7, at the arcade, no big screen): the two players' large portraits (switches Shell+0x1EC and +0x1F0,
    // one child per column, with the "player N") leave the scene and the selection is drawn on their plane (mgPlane)
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
    v.selPlayers = g_selOn ? g_multi + g_selCpus : 0;
    v.selNoCostume = g_selOn && elimination(rdram);   // elimination: fixed costume
    for (int p = 0; p < kMaxPanels; ++p)
        v.selCustom[p] = -1, v.selList[p] = false, v.selCpu[p] = false;
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
        const bool cpu = p >= g_multi;
        // CPU: waits until its player (p % N) has reached it
        bool waiting = false;
        for (int j = p - g_multi; cpu && j >= 0; j -= g_multi)
            waiting = waiting || !g_pick[j].ready;
        v.selWaiting[p] = waiting;
        v.selCpu[p] = cpu;
        v.selColor[p] = cpu ? -1 : p;
        if (cpu)
            std::snprintf(v.selLabel[p], sizeof v.selLabel[p], "CPU%d", p - g_multi + 1);
        else
            std::snprintf(v.selLabel[p], sizeof v.selLabel[p], "P%d", p + 1);
        const Pick &k = g_pick[p];
        if (!waiting && col == kCellCustom && customCount() > 0)
        {
            // CUSTOM cell: list open (no monster in the frame), custom chosen or confirmed, or closed ("+ CUSTOM")
            v.selListHl[p] = k.customHl % customCount();
            v.selList[p] = !k.ready && k.list;
            fillCustom(v, p, k.list ? -1 : k.custom - 1, k.costume);
            continue;
        }
        fillCostume(v, p, costumeList(rdram, k.col, v.selNoCostume));
    }
    if (g_solo && !g_selOn)
        publishSolo(rdram, v);
}

// ------------------------------------------------------------------ hooks

// Screen hidden by the selection in progress (5: pads; 6: monster selection), or -1
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
        for (uint32_t i = 0; i < 3; ++i)   // m_cs7C[0..2]: the other costume panels of the 3 and 4 monster screens
            if (const uint32_t cs = rd32(rdram, kShell + 0x7C + 4 * i) & 0x1FFFFFFFu; cs >= 0x1000u)
                rdram[cs + 0xC] = 0;
        for (uint32_t off = 0x1C0; off <= 0x1D8; off += 4)   // monster cursors (m_monsterCursorSw[0..2] and screen 19's after them)
            if (const uint32_t sw = rd32(rdram, kShell + off) & 0x1FFFFFFFu; sw >= 0x1000u)
                rdram[sw + 8] = 0;
    }
    if (screen == 7)
    {
        for (uint32_t i = 0; i < 2; ++i)   // m_costumeBoxCsMG[0..1]: the costume panels of the minigame selection
            if (const uint32_t cs = rd32(rdram, kShell + 0xB0 + 4 * i) & 0x1FFFFFFFu; cs >= 0x1000u)
                rdram[cs + 0xC] = 0;
    }
    // the retail player "1"/"2" marks on the portraits (shell->m_selectSw[column], m_selectSwMG on screen 7: 1, 2 or 3 = both;
    // 4 = locked, stays)
    const uint32_t marks = screen == 7 ? 0x194u : 0x168u;
    for (uint32_t i = 0; i < 10; ++i)
        if (const uint32_t sw = rd32(rdram, kShell + marks + 4 * i) & 0x1FFFFFFFu; sw >= 0x1000u && rdram[sw + 8] >= 1 && rdram[sw + 8] <= 3)
            rdram[sw + 8] = 0;
}

// Progress (0..1, in the direction it plays) of the current transition's camera animation, shell->m_screenAnim[currAnimationIndex]
// (a 16-byte _animHandle, which the functions take by pointer in a0). -1 with no animation.
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

// screenTransition: entering from the main menu (or backing out of screen 15 in multiplayer) starts the pad selection; entering
// screen 6 in multiplayer starts the monster selection. While either is active, the screen's items get alpha 0 and the alpha the
// transition would have given them becomes the selection's.
static void transitionHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenTransition__Fb)
    {
        const int cur = static_cast<int>(rd32(rdram, wotm::sym::currScreen)), last = static_cast<int>(rd32(rdram, wotm::sym::lastScreen));
        // backing out of the following screens (13, 15, 11, 14) shows the mode list, with the same players
        if (rd32(rdram, wotm::sym::screenFirstPass) && cur == 5 && last == 1)
        {
            g_phase = Config;
            g_primed = false;
            g_uiAlpha = 0.f;
            for (int d = 0; d < kDevs; ++d)   // the previous choice is kept, minus unplugged devices
                if (!present(d)) g_slot[d] = 0;
            g_multi = 0;
            setManual(false);
            std::fprintf(stderr, "[padsel] pad selection\n");
        }
        // back on screen 6 (from "first to", continues, the minigame choice or stage select): a previous selection still marked as
        // leaving starts over
        if (rd32(rdram, wotm::sym::screenFirstPass) && cur == 6 && g_multi && (!g_selOn || g_selLeaving))
            selectBegin(rdram);
        // 1 and 2 player screens 3 and 6: the game's selection shown on the big screen
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
        // entering: only during the fade-in (before that the alpha is left over from the previous screen); leaving: the screen's fade-out
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
        if (g_phase == Config && rd32(rdram, wotm::sym::betweenScreens) == 0)   // in the middle of a transition changeScreen doesn't take effect
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
        if (minigame(rdram))   // coming from screen 14: the load, like screenMinigames2P, or back to the arcade
        {
            if (done)
            {
                changeScreen(rdram, ctx, runtime, 6, 0x63, 1, 0);
                wr32(rdram, wotm::sym::continueDecoding, 0);
                callGuest(rdram, ctx, runtime, wotm::fn::terminateShellSound__10ShellSound, kShellSound);
            }
            else
            {
                changeScreen(rdram, ctx, runtime, 6, 14, -1, 0);
                menuAnim(rdram, ctx, runtime, 8, false);   // the camera from the big screen to the arcade (going from screen 5 to 14 is animation 8)
            }
        }
        else if (done)
            changeScreen(rdram, ctx, runtime, 6, 8, 1, 3);
        else
        {
            // like the game's own back path: the menu movie back on the big screen. Without continueDecoding, "first to" (which calls
            // Play every frame) ended the movie on the very first frame, and the big screen went black
            callGuest(rdram, ctx, runtime, wotm::fn::moviesReadyScreenObject__FP11_hierobjectii, rd32(rdram, kMovieScreen), 8, 7);
            callGuest(rdram, ctx, runtime, wotm::fn::Play__6CMoviebPc, wotm::sym::myMovie, 0, wotm::sym::MainMovie);
            wr32(rdram, wotm::sym::continueDecoding, 1);
            changeScreen(rdram, ctx, runtime, 6, elimination(rdram) ? 11 : 15, -1, 0);   // elimination continues or "first to"
        }
    }
    if (g_selOn)
        hideSelect2P(rdram);
    else if (g_solo == 6)
        soloExit(rdram);
    publish(rdram);
}

// Exit from screens 3 and 6 with the native selection: follows the game, hides the screen and, if it changed screens, starts leaving
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

// Screens 18 and 19 (3 and 4 monsters, with CPUs): the same native selection as screens 3 and 6
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
        // extra CPUs: while CPUs are missing, the game's countdown to leave for the stage (10 frames with everyone ready) doesn't
        // advance; confirming a CPU in the middle only lasts until soloSync, which clears "ready"
        if (const XCfg x = k ? extraCfg(rdram) : XCfg{}; x.want && g_xStage < x.want - x.base)
            wr32(rdram, wotm::sym::watchForASecond_3016, 10);
    }
    (k ? g_origSelect4P : g_origSelect3P)(rdram, ctx, runtime);
    if (!g_selectNPending[k] || ctx->pc != g_selectNRa[k])
        return;
    g_selectNPending[k] = false;
    if (g_solo == screen)
        soloExit(rdram);
    publish(rdram);
}
// Screen 7 (2 player minigames): the same native selection
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
    // the 3 and 4 player selection has finished leaving: the next screen ("first to" or stage select) already reads input.
    // screenTransition doesn't see the end of the transition (betweenScreens is cleared outside it) and the selection stayed
    // "leaving" (so on returning to screen 6 it didn't start over).
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
    // native 1 and 2 player selection: the one that finished leaving; up/down for the one on screen
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

// CPUs 4 to 7 in 1 player free for all (wotm_menucpus.h): screen 12 stored the count, the menu continued with 3 (first to,
// selection, stage). Once the stage is confirmed, m_numAIs gets the requested count and each CPU c from 3 onwards goes into
// m_monsterSel[3 + c], m_costume[1 + c]: the ones picked on screen 19 (g_xPick; the last one is from the game's CPU 3 frame, in
// m_monsterSel[6] and m_costume[4]). Without the native selection (WOTM_NATSEL=0) CPUs 4 to N are rolled among the open columns, with
// the lowest unlocked disc costume nobody on the same monster has (more than 4 of the same: repeats, like WOTM_MATCH).
static void extraCpus(uint8_t *rdram)
{
    const int want = wotm_menucpus::g_want.load();
    const int players = rd32(rdram, kMode) == 2u ? 1 : int(rd32(rdram, kNumPlayers)), base = players == 1 ? 3 : 2;
    if (players > 2 || want <= base || want > wotm_menucpus::maxFor(players) || rd32(rdram, kNumAIs) != uint32_t(base))
        return;
    auto monAt = [](int c) { return kMonsterSel + 4u * uint32_t(3 + c); };
    auto cosAt = [](int c) { return kCostume + 4u * uint32_t(1 + c); };
    if (g_xDone == want - base)
    {
        const uint32_t lastMon = rd32(rdram, monAt(base)), lastCos = rd32(rdram, cosAt(base));
        std::fprintf(stderr, "[padsel] %d CPUs picked in the selection:", want);
        for (int c = base; c <= want; ++c)
        {
            const bool last = c == want;
            const uint32_t m = last ? lastMon : g_xPick[c - base].monster, cs = last ? lastCos : g_xPick[c - base].costume;
            wr32(rdram, monAt(c), m);
            wr32(rdram, cosAt(c), cs);
            std::fprintf(stderr, " CPU%d %s:%u", c, monsterName(m), cs);
        }
        std::fprintf(stderr, "\n");
        wr32(rdram, kNumAIs, uint32_t(want));
        return;
    }
    static std::mt19937 rng(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    int open[10], nOpen = 0;
    for (int c = 1; c <= 10; ++c)
        if (columnOpen(rdram, c)) open[nOpen++] = c;
    if (!nOpen)
        return;
    int mon[2 + wotm_menucpus::kMax], cos[2 + wotm_menucpus::kMax];   // CPUs 1..want and, at 0 and at the end, the players
    mon[0] = int(rd32(rdram, kMonsterSel) >> 5), cos[0] = int(rd32(rdram, kCostume));
    mon[want + 1] = int(rd32(rdram, kMonsterSel + 4u) >> 5), cos[want + 1] = players == 2 ? int(rd32(rdram, kCostume + 4u)) : -1;
    for (int c = 1; c <= base; ++c)
        mon[c] = int(rd32(rdram, monAt(c)) >> 5), cos[c] = int(rd32(rdram, cosAt(c)));
    std::fprintf(stderr, "[padsel] %d CPUs: rolling CPUs %d to %d:", want, base + 1, want);
    for (int c = base + 1; c <= want; ++c)
    {
        const int col = open[rng() % uint32_t(nOpen)], m = monsterOfCol(rdram, col);
        int costume = -1;
        for (int k = 0; k < 4 && costume < 0; ++k)
        {
            const bool unlocked = rd32(rdram, wotm::sym::monsterSelectMode + uint32_t((col - 1) * 4 + k) * kSlotSize + 4) == 1u;
            bool used = false;
            for (int j = 0; j < c; ++j)
                used = used || (mon[j] == m && cos[j] == k);
            used = used || (mon[want + 1] == m && cos[want + 1] == k);
            if (unlocked && !used)
                costume = k;
        }
        if (costume < 0)
            costume = firstCostume(rdram, col);
        mon[c] = m, cos[c] = costume;
        wr32(rdram, monAt(c), uint32_t(m) << 5);
        wr32(rdram, cosAt(c), uint32_t(costume));
        std::fprintf(stderr, " %s:%d", kMonsterNames[m], costume);
    }
    std::fprintf(stderr, "\n");
    wr32(rdram, kNumAIs, uint32_t(want));
}

// changeScreen(from, to, direction, animation, ...): in multiplayer, backing out of screen 15 (which would go to 13, the CPUs) comes
// to the pad selection (5), with the camera back on the big screen, and backing out of that (prevScreen 15) goes to the main menu
static void changeScreenHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::changeScreen__Fiiiib)
    {
        const int32_t from = static_cast<int32_t>(GPR_U32(ctx, 4)), to = static_cast<int32_t>(GPR_U32(ctx, 5)),
                      dir = static_cast<int32_t>(GPR_U32(ctx, 6));
        const uint32_t base = rd32(rdram, kMode) == 2u ? 3u : rd32(rdram, kNumPlayers) == 2u ? 2u : 0u;
        if (to == 12 || to == 13)
            wotm_menucpus::g_want.store(0);   // screens 12 and 13 pick again
        else if ((to == 15 || to == 19) && base && !g_multi && rd32(rdram, kNumAIs) > base)
            wr32(rdram, kNumAIs, base);   // back from a fight with more CPUs: retail only knows 3 (1 player) or 2 (2) on these screens
        if (from == 2 && to == 12 && wotm_training::g_menuArmed.load())
        {
            // TRAINING (wotm_mainmenu.inc): skips screen 12 (CPU count) and 15 (rules), straight to selection (6) with 1 CPU, endless
            // fight (m_killTarget 0: whoever falls gets up). What those two would have done: clear the CPUs' monsters and the CPU 2+
            // column switch (screen 12's first pass) and resetSelectables (15). Animation 2 already takes the camera to where
            // screens 12/15/6 are.
            wr32(rdram, kNumAIs, 1);
            wr32(rdram, kShell + 0x2A44, 0);   // m_killTarget
            for (uint32_t a = kShell + 0x2948; a <= kShell + 0x2950; a += 4)
                wr32(rdram, a, 0);             // m_monsterSel[4..6]
            if (const uint32_t sw = rd32(rdram, kShell + 0x1E0))
                callGuest(rdram, ctx, runtime, wotm::fn::hierSetSwitch__FP11_hierswitchi, sw, 0u);
            callGuest(rdram, ctx, runtime, wotm::fn::resetSelectables__Fv, 0u);
            ctx->r[5] = _mm_set_epi64x(0, 6);
            std::fprintf(stderr, "[training] flow: screen 2 -> 6 (1 CPU, endless)\n");
        }
        else if (from == 6 && to == 2 && dir < 0 && wotm_training::g_menuArmed.load())
            ctx->r[7] = _mm_set_epi64x(0, 2);   // back from selection to screen 2 (the prevScreen) with the camera, instead of screen 6's hard cut
        else if (from == 5 && (to == 11 || to == 13 || to == 14 || to == 15) && dir < 0)   // backing out of the list goes to the main menu
            ctx->r[5] = _mm_set_epi64x(0, 1);
        else if (g_multi && from == 14 && to == 7 && dir > 0)   // 3 and 4 player minigame: the selection is the big screen's (screen 7 is the arcade)
        {
            ctx->r[5] = _mm_set_epi64x(0, 6);
            const uint32_t ra = GPR_U32(ctx, 31);
            g_origChangeScreen(rdram, ctx, runtime);
            if (ctx->pc == ra)
                menuAnim(rdram, ctx, runtime, 8, true);   // the camera goes back from the arcade to the big screen
            return;
        }
        else if (from == 8 && to == 99)   // stage select confirmed (the game has already written m_levelNum)
        {
            if (!wotm_levelsel::resolveRandom(rdram))   // RANDOM: the rolled stage (wotm_levelsel.inc), without the narrator (a surprise)
                wotm_narrator::announceLevel(int(rd32(rdram, kShell + 0x2BB0)));
            extraCpus(rdram);
        }
        else if (from == 20 && to == 1)   // Start on the title screen
            wotm_narrator::stopIntro();
    }
    g_origChangeScreen(rdram, ctx, runtime);
}

// Main menu: the multiplayer match is over for good, the pads go back to the default map; the "2 player" label every frame
static void mainHook(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::screenMain__Fv)
    {
        if (wotm_pad::g_manual.load(std::memory_order_relaxed))
            setManual(false);
        g_multi = 0;
        g_selOn = false;
        wotm_training::menuDisarm("main menu");   // the training fight is over (or the selection backed out all the way here)
        wotm_training::restoreDifficulty(rdram, wotm::sym::shellGame + wotm_training::kDifficulty);
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
        wotm_narrator::announceIntro();   // the title camera has just stopped (only runs after the transition)
    }
    g_origWaitStart(rdram, ctx, runtime);
}

// ------------------------------------------------------------------ drawing (window)

static Vector3 vadd(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vector3 vsub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vector3 vscale(Vector3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
static Vector3 vneg(Vector3 a) { return {-a.x, -a.y, -a.z}; }
static float vdot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float vlen(Vector3 a) { return std::sqrt(vdot(a, a)); }
static Vector3 vnorm(Vector3 a) { const float l = vlen(a); return l > 0.f ? vscale(a, 1.f / l) : a; }
static Vector3 vcross(Vector3 a, Vector3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

constexpr int kW = 1200, kH = 500;   // selection texture (2.4:1: fits below the big screen's logo)
static RenderTexture2D g_rt{}, g_labelRt{};
static float g_pos[kDevs] = {};   // drawn column of each device (moves toward its player's)
static bool g_posInit = false;

// "multiplayer" label in the style of the other big-screen items: lowercase in a condensed font (Windows' Bahnschrift, otherwise
// Arial Bold, otherwise raylib's), with a dark outline; cream when lit, the earthy orange of the dim items when dim. The lit item's
// glow is still the game's (a separate object in the switch). Text-sized texture (g_labelRt), redrawn when the state changes.
static const char *const kLabelText = "multiplayer";
constexpr float kLabelFont = 112.f, kLabelPad = 10.f;
static Font g_labelFont{};
static bool g_labelFontTried = false;
static int g_labelState = -1;   // lit (1) or dim (0) drawn into g_labelRt

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
    for (int dy = -4; dy <= 4; dy += 2)   // outline: the dark text offset all around
        for (int dx = -4; dx <= 4; dx += 2)
            if (dx || dy)
                DrawTextEx(g_labelFont, kLabelText, Vector2{at.x + float(dx), at.y + float(dy)}, kLabelFont, 0.f, edge);
    DrawTextEx(g_labelFont, kLabelText, at, kLabelFont, 0.f, fill);
    EndBlendMode();
    EndTextureMode();
}

// Text in the game's font 0 (the frame's atlas, `tex0`), like fontBuildPrim: fixed-size glyphs, `h` = height in texture pixels;
// each letter is 1.6 x the height in game screen units (640 x 224), i.e. 0.8 x the height on screen.
static void text(std::vector<wotm::TextQuad> &out, const wotm::Ram &ram, uint64_t tex0, int rw, int rh, const char *s, float cx, float cy, float h,
                 Color c, float spacing = 2.f)
{
    if (!tex0) return;
    const uint32_t f = wotm::FontText::kFonts;
    const float cellU = ram.u16(f) / 16.f, cellV = ram.u16(f + 2) / 16.f, gw = ram.u16(f + 4) / 16.f, gh = ram.u16(f + 6) / 16.f;
    if (gw <= 0.f || gh <= 0.f) return;
    const bool rtx = ram.u32(wotm::FontText::kAtlasMode) != 0;
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const float sx = 640.f / gr.w, sy = 224.f / float(rh);   // texture pixel -> game screen unit
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
// Text height that fits in `w` pixels (each letter 1.6 x the height plus the spacing)
static float fitText(const char *s, float h, float w) { return std::min(h, w / (1.6f * float(std::max<size_t>(1, std::strlen(s))) + 0.1f)); }
static Color gsColor(Color c, float k = 0.5f)   // player color as a GS text color (0x80 = 1.0)
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

// Pad selection: four columns (players) on top and, below, the devices that are out; each device moves to its column
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
    // devices: those out in a row at the bottom; those in, stacked in their player's column
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
    // buttons: in the bottom legend, in place of the game's (wotm_legend.inc), using the mapped ones (readDevice)
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

// Selection font: Oswald embedded in the executable (resource FONT_OSWALD in native/wotm.rc, the options screen's); otherwise raylib's
static Font g_uiFont{};
static bool g_uiFontTried = false;
static const Font &uiFont()
{
    if (!g_uiFontTried)
    {
        g_uiFontTried = true;
#ifdef _WIN32
        void *mod = GetModuleHandleA(nullptr);
        void *res = FindResourceA(mod, "FONT_OSWALD", reinterpret_cast<const char *>(static_cast<uintptr_t>(10)));   // RT_RCDATA
        void *h = res ? LoadResource(mod, res) : nullptr;
        const unsigned char *data = h ? static_cast<const unsigned char *>(LockResource(h)) : nullptr;
        const int size = res ? static_cast<int>(SizeofResource(mod, res)) : 0;
#else
        int size = 0;
        const unsigned char *data = wotmEmbeddedResource("FONT_OSWALD", size);
#endif
        if (data && size > 0)
            g_uiFont = LoadFontFromMemory(".ttf", data, size, 128, nullptr, 0);
        if (g_uiFont.texture.id)
        {
            GenTextureMipmaps(&g_uiFont.texture);
            SetTextureFilter(g_uiFont.texture, TEXTURE_FILTER_TRILINEAR);
        }
        else
        {
            std::fprintf(stderr, "[padsel] font FONT_OSWALD is not in the executable: selection uses raylib's font\n");
            g_uiFont = GetFontDefault();
        }
    }
    return g_uiFont;
}
// Text centered on (cx, cy) with a dark outline, shrunk to fit in `maxW`
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

// Each player's 3D monster in the selection (drawn the same way as the save list: wotm_model.hpp, framing from wotm_savepics.hpp),
// into the Renderer's own RenderTexture, playing idle since the last change. Models are cached per monster and costume (switching
// back doesn't reread the disc).
struct SelModel
{
    wotm::model::Renderer ren;
    uint32_t id = ~0u;   // monster | costume << 8
    double t0 = 0.0;
    bool ready = false;
};
static SelModel g_selModel[kMaxPanels];
static std::map<uint32_t, std::unique_ptr<wotm::model::Model>> g_selCache;
static wotm::model::FxTextures g_selFx;
static bool g_selFxTried = false;
constexpr int kSelPW = 1800, kSelPH = 1100;   // selection texture: the whole big screen (~1.64:1), seen head-on (frameCamera)
static RenderTexture2D g_selRt{};

// Cache key: monster | costume << 8 | fallback disc costume (the skin's base) << 16
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
        // mod costume: MON/<PREFIX><n>.* comes from mods/ (or from the base costume, wotm_mods.hpp)
        if (m->load(io.cdRoot, iso, pre + std::to_string(costume)) || m->load(io.cdRoot, iso, pre + std::to_string(base)) ||
            m->load(io.cdRoot, iso, std::string(pre) + "0"))
            m->computeFraming(wotm::model::kIdle);   // fixed box: the camera doesn't follow the idle animation
        else
        {
            std::fprintf(stderr, "[padsel] MON/%s%u.NGP not found; no 3D monster\n", pre, costume);
            m.reset();
        }
    }
    return (g_selCache[id] = std::move(m)).get();
}

// Width and position of the frames in the selection texture: side by side; a single one (1 player) centered, not filling the big screen
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
// Frame p of n in the selection texture: up to 4 in one row; from 5 to 8 (CPUs 4 to 7) two rows of 4, each frame half the height,
// the bottom row centered when it has fewer than 4
struct SelBox
{
    float x, y, w, h;
    int col, cols;   // position in the row and frames in it
};
static SelBox selBox(int n, int p)
{
    const float top = float(kSelPH) * 0.02f, bottom = float(kSelPH) * 0.88f, pad = float(kSelPW) * 0.012f;
    const int per = n > 4 ? 4 : std::max(1, n), rows = n > 4 ? 2 : 1, row = p / per, col = p % per;
    const int inRow = std::min(per, n - row * per);
    const float w = selBoxW(per), h = (bottom - top - pad * float(rows - 1)) / float(rows);
    const float shift = float(per - inRow) * (w + pad) * 0.5f;
    return {selBoxX(per, col) + shift, top + float(row) * (h + pad), w, h, col, inRow};
}

// Before the selection's BeginTextureMode: each player's monster into its RenderTexture (w x h)
static void renderSelModels(const View &v, int w, int h)
{
    if (!g_selFxTried)
    {
        g_selFxTried = true;
        static const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
        g_selFx.load(io.cdRoot, io.cdImage.string());
    }
    const double now = GetTime();
    for (int p = 0; p < kMaxPanels; ++p)
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
        const D::Framing fr = D::framingOf(mon | uint32_t(v.selBase[p] & 3) << 8);   // framing of the disc costume
        s.ren.setFx(&g_selFx);
        s.ren.shiftX = fr.shiftX;
        s.ren.shiftY = fr.shiftY;
        s.ren.fwd = fr.fwd;
        // facing forward, turned slightly toward the center of the screen (the left ones to the right and vice versa)
        const SelBox b = selBox(v.selPlayers, p);
        const float yaw = b.cols > 1 ? 30.f - 60.f * float(b.col) / float(b.cols - 1) : 0.f;
        s.ren.render(*m, w, h, yaw, 8.f, fr.zoom * 0.95f, [] {});
        s.ready = true;
    }
}

// Name of custom k in uppercase (the registry only changes before boot: the window reads it directly)
static const char *customName(int k)
{
    static thread_local char buf[48];
    const wotm::mods::Custom *cu = customAt(k);
    std::snprintf(buf, sizeof buf, "%s", cu ? cu->name.c_str() : "");
    for (char *q = buf; *q; ++q) *q = char(std::toupper(static_cast<unsigned char>(*q)));
    return buf;
}

// List of customs inside frame p: one line per custom (name; below it the mod, the skins and, on a CPU frame, "PLAYERS ONLY" for
// those without "ai"), the highlighted one in the player's color. With more lines than fit, a window around the highlighted one
// with arrows at the ends.
static void drawCustomList(const View &v, int p, float x, float y, float boxW, float boxH, Color c)
{
    const auto &cs = wotm::mods::Registry::get().customs();
    const Color kName{246, 238, 220, 255}, kSub{205, 195, 175, 255}, kOff{120, 112, 100, 255};
    const float cx = x + boxW * 0.5f;
    outlined("CUSTOM MONSTERS", cx, y + boxH * 0.175f, boxH * 0.055f, boxW * 0.86f, kName);
    const int n = int(cs.size()), hl = std::clamp(v.selListHl[p], 0, std::max(0, n - 1));
    const float top = y + boxH * 0.235f, bot = y + boxH * 0.90f, rowH = boxH * 0.13f, gap = boxH * 0.015f, arrow = boxH * 0.025f;
    const int fit = std::max(1, int((bot - top - 2.f * arrow) / (rowH + gap)));
    const int shown = std::min(n, fit), first = std::clamp(hl - shown / 2, 0, std::max(0, n - shown));
    float ry = top + arrow;
    if (first > 0)
        DrawTriangle(Vector2{cx, top - arrow * 0.2f}, Vector2{cx - arrow, top + arrow * 0.8f}, Vector2{cx + arrow, top + arrow * 0.8f}, kName);
    for (int k = first; k < first + shown; ++k, ry += rowH + gap)
    {
        const wotm::mods::Custom &cu = cs[size_t(k)];
        const bool on = k == hl, blocked = v.selCpu[p] && !cu.ai;
        const Rectangle row{x + boxW * 0.06f, ry, boxW * 0.88f, rowH};
        DrawRectangleRounded(row, 0.3f, 6, on ? Color{c.r, c.g, c.b, 225} : Color{40, 36, 32, 220});
        if (on)
            DrawRectangleRoundedLinesEx(row, 0.3f, 6, 3.f, kName);
        outlined(customName(k), cx, ry + rowH * 0.35f, rowH * 0.40f, row.width * 0.92f, blocked ? kOff : kName);
        const size_t skins = wotm::mods::Registry::get().customCostumes(cu).size() - 1;
        std::string sub = "MOD " + cu.mod;
        if (skins)
            sub += "  -  " + std::to_string(skins) + (skins == 1 ? " SKIN" : " SKINS");
        if (blocked)
            sub += "  -  PLAYERS ONLY";
        for (char &ch : sub) ch = char(std::toupper(static_cast<unsigned char>(ch)));
        outlined(sub.c_str(), cx, ry + rowH * 0.75f, rowH * 0.27f, row.width * 0.9f, blocked ? kOff : on ? kName : kSub);
    }
    if (first + shown < n)
        DrawTriangle(Vector2{cx - arrow, ry}, Vector2{cx, ry + arrow}, Vector2{cx + arrow, ry}, kName);
    char num[16];
    std::snprintf(num, sizeof num, "%d/%d", hl + 1, n);
    outlined(num, cx, y + boxH * 0.945f, boxH * 0.035f, boxW * 0.2f, kSub);
}

static Texture2D g_logo[17]{};   // each monster's logo (256x128 from SHELL.TEX, the save list's; loadPortraits)
static void loadPortraits();
static Texture2D menuLabel(const char *label);

// Monster selection: one frame per player (or CPU), side by side, on the big screen. Each frame: the label on top, the 3D monster,
// the name with arrows, the costume name and the costume strip (one segment per costume; mod ones in gold; the chosen one wider and
// lit) and "READY".
static void drawSelect(const View &v)
{
    constexpr float W = kSelPW, H = kSelPH;
    const int n = std::max(1, v.selPlayers);
    const Color kName{246, 238, 220, 255}, kGold{236, 190, 70, 255}, kCpu{150, 150, 160, 255};
    // frame background like the game's: square, 80% opaque, from #1B0E0C at the top to #6F372B at the bottom
    const Color kBoxTop{0x1B, 0x0E, 0x0C, 204}, kBoxBottom{0x6F, 0x37, 0x2B, 204};
    loadPortraits();
    for (int p = 0; p < v.selPlayers; ++p)
    {
        const Color c = v.selColor[p] >= 0 ? kPlayerColors[v.selColor[p] % kMaxPlayers] : kCpu;
        const SelBox b = selBox(n, p);
        const float boxW = b.w, boxH = b.h, x = b.x, y = b.y, cx = x + boxW * 0.5f;
        const Rectangle box{x, y, boxW, boxH};
        DrawRectangleGradientV(int(x), int(y), int(boxW), int(boxH), kBoxTop, kBoxBottom);
        DrawRectangleRec(Rectangle{x, y, boxW, boxH * 0.12f}, Color{c.r, c.g, c.b, 220});
        DrawRectangleLinesEx(box, v.selReady[p] ? 10.f : 5.f, Color{c.r, c.g, c.b, 240});
        if (const Texture2D lt = menuLabel(v.selLabel[p]); lt.id)   // "1", "cpu 2": the main menu's font (menuLabel)
        {
            const float band = boxH * 0.12f;
            float th = band * 0.8f, tw = th * float(lt.width) / float(lt.height);
            if (tw > boxW * 0.9f)
                th *= boxW * 0.9f / tw, tw = boxW * 0.9f;
            DrawTexturePro(lt, Rectangle{0.f, 0.f, float(lt.width), float(lt.height)},
                           Rectangle{cx - tw * 0.5f, y + (band - th) * 0.5f, tw, th}, Vector2{0.f, 0.f}, 0.f, WHITE);
        }
        else
            outlined(v.selLabel[p], cx, y + boxH * 0.06f, boxH * 0.10f, boxW * 0.9f, kName);
        if (v.selWaiting[p])   // mode 2 CPU: player 1 picks for it after their own
        {
            outlined("?", cx, y + boxH * 0.42f, boxH * 0.30f, boxW * 0.8f, Color{c.r, c.g, c.b, 255});
            outlined("WAITING", cx, y + boxH * 0.79f, boxH * 0.07f, boxW * 0.8f, kName);
            continue;
        }
        if (v.selList[p])
        {
            drawCustomList(v, p, x, y, boxW, boxH, c);
            continue;
        }
        if (v.selCell[p] > 10 && !v.selReady[p] && (v.selMonster[p] >> 5) == 0)   // random, or custom while closed
        {
            const bool rnd = v.selCell[p] == kCellRandom;
            const bool none = wotm::mods::Registry::get().customs().empty();
            outlined(rnd ? "?" : "+", cx, y + boxH * 0.42f, boxH * 0.34f, boxW * 0.8f, Color{c.r, c.g, c.b, 255});
            const float nameY = y + boxH * 0.76f;
            outlined(rnd ? "RANDOM" : "CUSTOM", cx, nameY, boxH * 0.085f, boxW * 0.72f, kName);
            outlined("<", x + boxW * 0.08f, nameY, boxH * 0.09f, boxW, c);
            outlined(">", x + boxW * 0.92f, nameY, boxH * 0.09f, boxW, c);
            char sub[48];
            const size_t nc = wotm::mods::Registry::get().customs().size();
            std::snprintf(sub, sizeof sub, nc == 1 ? "1 CUSTOM MONSTER" : "%zu CUSTOM MONSTERS", nc);
            outlined(rnd ? "MONSTER AND COSTUME AT RANDOM" : none ? "NO CUSTOM MONSTERS IN MODS" : sub, cx,
                     y + boxH * 0.855f, boxH * 0.05f, boxW * 0.86f, Color{205, 195, 175, 255});
            continue;
        }
        // monster: from the top of the frame down to the name
        const SelModel &s = g_selModel[p];
        if (s.ready)
        {
            const Texture2D &mt = s.ren.target().texture;
            const Rectangle dst{x + boxW * 0.03f, y + boxH * 0.13f, boxW * 0.94f, boxH * 0.58f};
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);   // premultiplied color (Renderer)
            DrawTexturePro(mt, Rectangle{0.f, 0.f, float(mt.width), -float(mt.height)}, dst, Vector2{0.f, 0.f}, 0.f, WHITE);
            EndBlendMode();
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        }
        const char *name = v.selCustom[p] >= 0 ? customName(v.selCustom[p]) : monsterName(v.selMonster[p]);
        const float nameY = y + boxH * (v.selNoCostume ? 0.82f : 0.76f);
        const uint32_t lmon = v.selMonster[p] >> 5;
        const Texture2D *logo = v.selCustom[p] < 0 && lmon < std::size(g_logo) && g_logo[lmon].id ? &g_logo[lmon] : nullptr;
        if (logo)   // the game's logo (2:1) in place of the name; without a logo (custom, bosses), the name as text
        {
            // Congar (1), Agamo (10) and Ultra-V (4) have more empty margin around the art: drawn bigger to match the others' size
            const float big = lmon == 1 || lmon == 10 ? 1.35f : lmon == 4 ? 1.15f : 1.f;
            const float lw = std::min(boxW * (v.selReady[p] ? 0.94f : 0.72f) * big, boxH * 0.34f * big), lh = lw * 0.5f;
            DrawTexturePro(*logo, Rectangle{0.f, 0.f, float(logo->width), float(logo->height)},
                           Rectangle{cx - lw * 0.5f, nameY - lh * 0.5f, lw, lh}, Vector2{0.f, 0.f}, 0.f, WHITE);
        }
        else
            outlined(name, cx, nameY, boxH * 0.085f, boxW * (v.selReady[p] ? 0.94f : 0.72f), kName);
        if (!v.selReady[p])
        {
            outlined("<", x + boxW * 0.08f, nameY, boxH * 0.09f, boxW, c);
            outlined(">", x + boxW * 0.92f, nameY, boxH * 0.09f, boxW, c);
        }
        if (!v.selNoCostume)
        {
            // costume: the name and the strip (one segment per costume in the list; mod ones in gold)
            const bool mod = v.selCostume[p] >= 4;
            outlined(v.selCostumeName[p], cx, y + boxH * 0.855f, boxH * 0.05f, boxW * 0.86f, mod ? kGold : Color{205, 195, 175, 255});
            const int cnt = std::max(1, v.selCostumes[p]);
            if (cnt > 1 && !v.selReady[p])
            {
                // vertical strip on the frame's right edge, one segment per costume from top to bottom (up/down change it). With many
                // costumes the segments shrink down to a minimum; past that the strip shows a window around the chosen one, with arrows
                // at the ends showing there are more. Below it, "n/total".
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
                    const bool modSeg = k >= cnt - v.selModCount[p];   // mod ones come after the disc ones in the list
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
            DrawRectangleRec(band, Color{c.r, c.g, c.b, 235});
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
    bool list = false;
    for (int p = 0; p < v.selPlayers; ++p)
        list = list || v.selList[p];
    if (list)
        wotm_legend::post({{bind(st::BUp), bind(st::BDown), "CUSTOM MONSTER"}, {bind(st::BCross), {}, "SELECT"}, {bind(st::BTriangle), {}, "CLOSE"}},
                          v.alpha);
    else if (v.selNoCostume)
        wotm_legend::post({{bind(st::BLeft), bind(st::BRight), "MONSTER"}, {bind(st::BCross), {}, "READY"}, {bind(st::BTriangle), {}, "BACK"}},
                          v.alpha);
    else
        wotm_legend::post({{bind(st::BLeft), bind(st::BRight), "MONSTER"},
                           {bind(st::BUp), bind(st::BDown), "COSTUME"},
                           {bind(st::BCross), {}, "READY"},
                           {bind(st::BTriangle), {}, "BACK"}},
                          v.alpha);
}

// ------------------------------------------------------------------ monster bar (2D, above the black band at the bottom)
// The column portraits are the ones from the game's row (which stays hidden, hideStrip): 64x64 textures from SHELL/SHELL.RTX (T8H)
// and SHELL.TEX (T8), with the .RTX's CT32 palettes (16x16; 8x2 for the locked ones' T4 silhouettes). Read from disc once, like the
// save list pictures (wotm_savepics.hpp). Mapping checked by exporting with tools/tex.py and against each portrait's object in the scene.
struct PortraitTex
{
    int monster;            // type (kMonsterNames)
    uint16_t texId, palTbp; // portrait
    bool rtx;               // in the .RTX (otherwise in the .TEX)
    uint16_t lockId, lockPal; // locked silhouette (0: none)
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

// Frame labels in the main menu's font (the one of the "1", "2", "cpu 1" marks in the game's row, t178..t185, which are framed
// plates): the digits from t142 (screen 12's numbers, "7 cpu monsters") and the "cpu" of "cpu monsters" from t051, both T4 with the
// .RTX's palette 480. Rectangles measured on the image already flipped top-down: x0, y0, x1, y1 (exclusive) and the baseline.
struct Pixels
{
    std::vector<uint32_t> px;   // RGBA8, top to bottom
    int w = 0, h = 0;
};
struct InkRect { int x0, y0, x1, y1, base; };
constexpr uint16_t kMenuDigitsTex = 142, kMenuCpuTex = 51, kMenuFontPal = 480;
constexpr InkRect kMenuDigit[10] = {{6, 15, 31, 55, 55},     {46, 13, 62, 54, 54},   {77, 13, 102, 54, 54},  {116, 13, 141, 55, 55},
                                    {156, 12, 184, 55, 55},  {199, 12, 223, 56, 56}, {9, 64, 37, 111, 111},  {53, 63, 82, 110, 110},
                                    {98, 65, 126, 112, 112}, {142, 64, 171, 111, 111}};
constexpr InkRect kMenuCpu{2, 15, 68, 63, 51};   // the "p" descends 12 lines below the baseline
constexpr InkRect kMenuP{21, 15, 45, 63, 51};     // just the "p" of "cpu" (players: "p1")
static Pixels g_menuDigits, g_menuCpu;
static std::map<std::string, Texture2D> g_menuLabels;

// A 64x64 palettized texture (T8/T8H with a 16x16 CLUT in CSM1, T4HL/T4HH with an 8x2 CLUT), top to bottom (in the file the rows go
// bottom to top)
static Pixels decodePixels(const std::vector<uint8_t> &file, const std::vector<wotm::savepics::detail::Packet> &pk,
                           const std::vector<uint8_t> &rtx, const std::vector<wotm::savepics::detail::Packet> &rtxPk, uint16_t texId,
                           uint16_t palTbp)
{
    const wotm::savepics::detail::Packet *p = nullptr, *pal = nullptr;
    for (const auto &k : pk)
        if (k.texId == texId && k.psm != 0) p = &k;
    for (const auto &k : rtxPk)
        if (k.tbp == palTbp && k.psm == 0) pal = &k;
    if (!p || !pal)
        return {};
    const bool t8 = p->psm == 0x13 || p->psm == 0x1B, t4 = p->psm == 0x14 || p->psm == 0x24 || p->psm == 0x2C;
    const uint32_t np = pal->w * pal->h, w = p->w, h = p->h;
    const size_t data = p->off + 0x80, need = t8 ? size_t(w) * h : size_t(w) * h / 2;
    if ((!t8 && !t4) || data + need > file.size() || pal->off + 0x10 + 4 * size_t(np) > rtx.size())
        return {};
    std::vector<uint32_t> clut(np), px(size_t(w) * h);
    for (uint32_t i = 0; i < np; ++i)
    {
        const uint32_t j = (np >= 256) ? ((i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1)) : i;   // CSM1: bits 3 and 4 swapped
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
    return Pixels{std::move(px), int(w), int(h)};
}
static Texture2D decodePortrait(const std::vector<uint8_t> &file, const std::vector<wotm::savepics::detail::Packet> &pk,
                                const std::vector<uint8_t> &rtx, const std::vector<wotm::savepics::detail::Packet> &rtxPk, uint16_t texId,
                                uint16_t palTbp)
{
    Pixels d = decodePixels(file, pk, rtx, rtxPk, texId, palTbp);
    if (d.px.empty())
        return Texture2D{};
    Image img{d.px.data(), d.w, d.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
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
        std::fprintf(stderr, "[padsel] monster bar: SHELL/SHELL.TEX or .RTX not found; no portraits\n");
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
    int nl = 0;
    for (const wotm::savepics::LogoPic &l : wotm::savepics::kLogoPics)
        if (l.monster < std::size(g_logo))
            nl += (g_logo[l.monster] = decodePortrait(tex, texPk, rtx, rtxPk, l.t.texId, l.t.palTbp)).id != 0;
    std::fprintf(stderr, "[padsel] selection: %d of %zu logos\n", nl, std::size(wotm::savepics::kLogoPics));
    g_menuDigits = decodePixels(tex, texPk, rtx, rtxPk, kMenuDigitsTex, kMenuFontPal);
    g_menuCpu = decodePixels(tex, texPk, rtx, rtxPk, kMenuCpuTex, kMenuFontPal);
    if (g_menuDigits.px.empty() || g_menuCpu.px.empty())
        std::fprintf(stderr, "[padsel] selection: menu letters (SHELL t%d/t%d) not read; labels as text\n", kMenuDigitsTex, kMenuCpuTex);
    std::fprintf(stderr, "[padsel] monster bar: %d of %zu portraits\n", n, std::size(kPortraits));
}

// Label texture: "P<n>" becomes "p" and the digit, "CPU" and "CPU<n>" become "cpu" and the digit with a space, on the same
// baseline. Any other text (or letters that weren't read): an empty Texture2D, and the frame writes it in the selection font.
static Texture2D menuLabel(const char *label)
{
    loadPortraits();
    if (const auto it = g_menuLabels.find(label); it != g_menuLabels.end())
        return it->second;
    Texture2D &out = g_menuLabels[label];
    if (g_menuDigits.px.empty() || g_menuCpu.px.empty())
        return out;
    constexpr int kPad = 2, kWordGap = 12, kLetterGap = 2;
    struct Piece { const Pixels *src; InkRect r; int gap; };   // gap: space before the next piece
    std::vector<Piece> pieces;
    const char *d = label;
    if (label[0] == 'P' && label[1])
        pieces.push_back({&g_menuCpu, kMenuP, kLetterGap}), d = label + 1;
    else if (std::strncmp(label, "CPU", 3) == 0)
        pieces.push_back({&g_menuCpu, kMenuCpu, kWordGap}), d = label + 3;
    else
        return out;
    for (; *d; ++d)
    {
        if (*d < '0' || *d > '9')
            return out;
        pieces.push_back({&g_menuDigits, kMenuDigit[*d - '0'], kLetterGap});
    }
    int asc = 0, desc = 0, w = 2 * kPad;
    for (size_t i = 0; i < pieces.size(); ++i)
    {
        const InkRect &r = pieces[i].r;
        asc = std::max(asc, r.base - r.y0);
        desc = std::max(desc, r.y1 - r.base);
        w += r.x1 - r.x0 + (i ? pieces[i - 1].gap : 0);
    }
    const int h = asc + desc + 2 * kPad, base = kPad + asc;
    std::vector<uint32_t> px(size_t(w) * h, 0u);
    int x = kPad;
    for (size_t i = 0; i < pieces.size(); ++i)
    {
        const Pixels &s = *pieces[i].src;
        const InkRect &r = pieces[i].r;
        if (i)
            x += pieces[i - 1].gap;
        for (int sy = r.y0; sy < r.y1 && sy < s.h; ++sy)
            for (int sx = r.x0; sx < r.x1 && sx < s.w; ++sx)
                px[size_t(base - r.base + sy) * w + size_t(x + sx - r.x0)] = s.px[size_t(sy) * s.w + sx];
        x += r.x1 - r.x0;
    }
    Image img{px.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    out = LoadTextureFromImage(img);
    SetTextureFilter(out, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(out, TEXTURE_WRAP_CLAMP);
    return out;
}

// The bar: the selection's 10 columns (portrait; silhouette if locked), random (?) and custom, across the window, just above the
// menu's bottom black band (24 of 224 lines). Each player's cell gets a border in their color and a tag (P1, P2, CPU) on top; when
// ready, the tag is filled. Drawn after the scene and the game's text, before the legend (wotm_live.inc).
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
    // cell under the mouse (cursor released; open ones only, with the selection already on screen): the EE moves toward it (mouseStep)
    int hover = 0;
    if (const wotm::mouse::Pointer &mp = wotm::mouse::g_ptr; mp.in && mp.rw > 0 && mp.rh > 0 && v.alpha > 0.5f)
    {
        const float px = mp.x * W / float(mp.rw), py = mp.y * H / float(mp.rh);
        for (int c = 1; c <= kCells && !hover; ++c)
            if (px >= cellX(c) && px < cellX(c) + cell && py >= top && py < top + cell && (c > 10 || v.barOpen[c - 1]))
                hover = c;
    }
    // test script (`mouse:cellN`, `mouse:clickN`): the fake cursor on cell N
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
    // who is on each cell
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
    // player borders and tags (several on the same cell: borders from the outside in, tags side by side)
    int stack[kCells + 1] = {};
    for (int p = 0; p < v.selPlayers; ++p)
    {
        const int c = v.selCell[p];
        if (v.selWaiting[p] || c < 1 || c > kCells)
            continue;
        const int k = stack[c]++;
        const Color col = v.selColor[p] >= 0 ? kPlayerColors[v.selColor[p] % kMaxPlayers] : kCpu;
        const float x = cellX(c), o = 3.f + float(std::min(k, 3)) * cell * 0.06f;
        DrawRectangleLinesEx(Rectangle{x - o, top - o, cell + 2.f * o, cell + 2.f * o}, std::max(3.f, cell * 0.05f), fade(col));
        // tags two by two, the next ones stacked above (up to 8 frames on the same cell, CPUs 4 to 7)
        const float tw = cell * 0.46f, th = cell * 0.24f, tx = x + float(k % 2) * (tw + 2.f);
        const float ty = top - o - th * 0.9f - float(k / 2) * (th + 2.f);
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

// The game's portrait row (the selection's bottom panel): removed from the scene while the native selection is on screen. The objects
// are found by texture (SHELL portraits, borders and player marks); only during the selection, so it catches nothing from other screens.
static void hideStrip(wotm::Scene &scene, const wotm::Ram &ram)
{
    auto stripTex = [](uint32_t id) { return id == 54 || (id >= 57 && id <= 68) || (id >= 177 && id <= 185); };
    auto &items = scene.items;
    items.erase(std::remove_if(items.begin(), items.end(),
                               [&](const wotm::DrawItem &it) {
                                   const uint32_t n = it.node & 0x1FFFFFFFu;
                                   if (!ram.ok(n, 0x60) || (ram.u8(n) & 0x3F) != 0)
                                       return false;
                                   const uint32_t ns = std::min<uint32_t>(5, ram.u16(n + 0x5A));   // texId[5] at +0x50 (Scene::objectTexId)
                                   for (uint32_t s = 0; s < ns; ++s)
                                       if (stripTex(ram.u16(n + 0x50 + 2 * s)))
                                           return true;
                                   return false;
                               }),
                items.end());
}

// Selection drawn into its own texture (before the frame's BeginTextureMode). `tex0` = the previous frame's font atlas.
static void renderUi(wotm::Scene &scene, const wotm::Ram &ram, uint64_t tex0)
{
    View v;
    {
        std::lock_guard<std::mutex> lk(g_m);
        v = g_view;
    }
    // main menu label
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
        // the monster's RenderTexture at ~1.3 x its size in the selection texture
        const SelBox b = selBox(n, 0);
        renderSelModels(v, int(b.w * 0.94f * 1.3f), int(b.h * 0.58f * 1.3f));
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

// World rectangle of a big-screen object: center, right/up axes in its plane (up = world Z projected onto the plane; right in the
// direction of the camera's right), normal toward the camera, and the extents along those axes
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

// Selection plane: the big screen or, on screen 7, the arcade's two large portraits together (in P1's plane, plus the bottom band of
// its screen: ~30% more), which are removed from the scene. Stored for drawOnTelao, which draws after the objects are already gone.
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
            std::fprintf(stderr, "[padsel] screen 7: portrait plane %.1f x %.1f\n", a.x1 - a.x0, a.y1 - a.y0);
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

// A (render) texture (V flipped) on a rectangle of the plane: left/top and width/height along its axes, alpha 0..1; `uv` = the part
// of the texture (x0, y0, x1, y1 as fractions, y top to bottom); `tint` multiplies the color
static void drawOnPlane(const Plane &pl, const Texture2D &tex, float left, float top, float wx, float hy, float alpha, float lift,
                        const float uv[4] = nullptr, Color tint = WHITE, bool premul = false)
{
    // premul: a texture drawn with BLEND_CUSTOM_SEPARATE over a transparent background (color already multiplied by alpha): don't
    // multiply again (the menu list has translucent items and glow)
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
    // no depth: the alpha 0 items still write Z in front of the big screen (and nothing sits between the area and the camera)
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

// Where the mouse (wotm_mouse.hpp, cursor released) lands on the plane rectangle where drawOnPlane puts a texture (same
// left/top/wx/hy): u and v in 0..1, v top to bottom. On the window thread, with the frame's camera (`cam`, the BeginMode3D one).
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

// Camera facing the big screen `pl`, at fraction `s` (0..1, smoothed) of the way from the game's camera to it: the big screen takes
// 58% of the screen height, its center 41% from the top (the black bands take 10.7% at top and bottom); the camera sits a little
// below its center, so the cars show up underneath. Used by the "recompiled options" panel (wotm_mainmenu.inc) and the 3 and 4
// player monster selection.
static void telaoCamera(Camera3D &cam, const Plane &pl, float aspect, float s)
{
    const float t = s * s * (3.f - 2.f * s);
    const float tanY = std::tan(cam.fovy * DEG2RAD * 0.5f);
    constexpr float kHeight = 0.58f, kCenter = 0.41f, kDrop = 0.22f;
    const float h = pl.y1 - pl.y0, w = pl.x1 - pl.x0;
    const float d = std::max(h * 0.5f / (tanY * kHeight), w * 0.5f / (tanY * aspect * 0.96f));
    const Vector3 center = vadd(pl.c, vadd(vscale(pl.right, (pl.x0 + pl.x1) * 0.5f), vscale(pl.up, (pl.y0 + pl.y1) * 0.5f)));
    const Vector3 pos = vsub(vadd(center, vscale(pl.n, d)), vscale(pl.up, h * kDrop));
    const Vector3 target = vsub(center, vscale(pl.up, (0.5f - kCenter) * 2.f * tanY * d));   // looks below the center: the big screen moves up
    // position and direction interpolated separately (interpolating the target made the big screen leave the frame halfway)
    auto lerp = [&](const Vector3 &a, const Vector3 &b) { return vadd(a, vscale(vsub(b, a), t)); };
    const Vector3 f0 = vnorm(vsub(cam.target, cam.position)), f1 = vnorm(vsub(target, pos));
    cam.position = lerp(cam.position, pos);
    cam.target = vadd(cam.position, vnorm(lerp(f0, f1)));
    cam.up = vnorm(lerp(cam.up, pl.up));
}

// 3 and 4 player monster selection: the camera moves to the big screen in ~0.45 s (during the fade-in) and back on exit, like the
// panel's. Before the frame's BeginMode3D (wotm_live.inc).
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
    // Exit to a screen with a camera animation (backing out of adventure to screen 2, confirming and going to stage select): the
    // game's camera is already moving from where the original selection would be to the next screen. Easing back to the game over
    // 0.45 s on top of that added the two movements together; here the frame's camera is the game's plus the offset to the big
    // screen at the start of the exit, which fades out with the game animation's progress (a single movement, from the big screen
    // to the next screen, at the game's pace).
    static bool s_follow = false;
    static float s_q0 = 0.f;
    static Vector3 s_dPos{}, s_dFwd{}, s_dUp{};
    if (!want && s_t > 0.f && !s_follow && v.transMoving && v.transProg < 0.95f)
    {
        Plane pl0;
        if (v.screenObj && selectPlane(scene, ram, cam, v, pl0))
        {
            Camera3D t = cam;
            telaoCamera(t, pl0, aspect, s_t);   // the camera shown right now
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
    // options screens before the selection (11 elimination, 12 single player CPUs, 13 multiplayer CPUs, 15 "first to"): the game's
    // row only prepares the game's selection, which the native one replaces
    const uint32_t cur = ram.u32(wotm::sym::currScreen);
    // also the selection screens (3, 6, 7, 18, 19): changeScreen has already switched screens before transitionHook publishes the
    // selection, and the game's row showed up for one frame (e.g. "sudden death" on screen 15)
    const bool optionsScreen = soloEnabled() && (cur == 11 || cur == 12 || cur == 13 || cur == 15 || cur == 3 || cur == 6 || cur == 7 ||
                                                 cur == 18 || cur == 19);
    if (v.select || s_t > 0.f || optionsScreen)
        hideStrip(scene, ram);   // the native bar (drawBar2d) takes the place of the portrait row (until the camera is back)
    Plane pl;
    if (s_t <= 0.f || !v.screenObj || !selectPlane(scene, ram, cam, v, pl))
        return;
    scene.overlay.clear();   // the game's 3D button icons: they follow its camera
    scene.hpOverlay.clear();
    telaoCamera(cam, pl, aspect, s_t);
}

// The selection on the big screen's plane (the movie object, shell->m_movieScreen), below the logo, and the main menu label in the
// item's rectangle. Inside the frame's BeginMode3D, after the scene.
// ------------------------------------------------------------------ label built from the big screen's own letters
// "multiplayer" assembled from the letters of the other main menu items, in the game's font. The items are quads textured with white
// letters with a black outline (the color, dim or lit, comes from the vertices; switch child 0 is the dim one, 1 the lit one), stored
// upside down: "1/2 player" uses an atlas with "player", "options" and the digits, one quad for the digit and another for the word;
// "load game", "options" and "unlocks" each have their own. Here: "player" comes from the word quad of "2 player" itself, t and i from
// "options", m from "load game" and u from "unlocks" (letters split at the alpha-free columns of each quad's UV region). The word uses
// the texel scale of "player", sits on its baseline and is centered on the whole "2 player", with each state's vertex color. The crops
// come out of the .RTX at runtime (Scene::decodeTex): nothing from the game is kept in the repo. If any crop doesn't have the expected
// number of letters, the font-based label (renderLabel) is used instead.
struct Crop
{
    std::vector<uint32_t> px;   // RGBA8, alpha 0..255, rows top to bottom as on screen
    int w = 0, h = 0;
    const char *why = "";       // failure reason (log)
};
struct Seg { int x0, x1; };   // columns [x0, x1)
// A mesh quad: texture region (texels) and the texel -> object plane map (x = ax * u + bx, y = ay * v + by)
struct Quad
{
    float u0 = 1e30f, u1 = -1e30f, v0 = 1e30f, v1 = -1e30f;
    float ax = 0.f, bx = 0.f, ay = 0.f, by = 0.f;
    float x0 = 1e30f, x1 = -1e30f;   // extent on the plane (right axis)
};
struct GlyphLabel
{
    bool ok = false;
    Texture2D tex{};                 // the word in white with an outline (the color is applied when drawing)
    int base = 0;                    // baseline in the texture (pixels, from the top)
    int playerBase = 0;              // baseline in the crop of "2 player"'s "player" (rows in screen order)
    float inkCx = 0.f;               // center of the "2 player" letters on its plane (measured during assembly)
};
static GlyphLabel g_glyph;
constexpr int kGlyphPad = 4;
constexpr uint32_t kInk = 200;   // letter alpha (255); between two letters the touching outline is at 170

// Object (OBJECT_NODE) of child `child` of the switch for main menu item `item` (shell->m_menuItemSw[1][item])
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
    if ((ram.u8(c) & 0x3F) == 1 && ram.u16(c + 8) > 0)   // GROUP_NODE: first child at +0x20
        if (const uint32_t o = ram.u32(c + 0x20) & 0x1FFFFFFFu; ram.ok(o, 0x60) && (ram.u8(o) & 0x3F) == 0)
            return o;
    return 0;
}

// Quads of the object's mesh: triangles grouped by UV region (those of one quad share it). UV is normalized and the texture repeats
// (the items use u -1..0): the period is removed using the floor of the minimum. With `m` (the scene item's matrix) and `pl`, the
// map to the plane.
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
        for (; g < boxes.size(); ++g)   // same region (with 1 texel of slack): same quad
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

// Pixels of a quad's region, with rows in screen order (`flip`: the texture is upside down)
static bool cropQuad(const std::vector<uint32_t> &px, uint32_t tw, uint32_t th, const Quad &q, bool flip, Crop &c)
{
    const int x0 = std::clamp(int(std::floor(q.u0)), 0, int(tw)), x1 = std::clamp(int(std::ceil(q.u1)), 0, int(tw));
    const int y0 = std::clamp(int(std::floor(q.v0)), 0, int(th)), y1 = std::clamp(int(std::ceil(q.v1)), 0, int(th));
    if (x1 - x0 < 8 || y1 - y0 < 4)
    {
        c.why = "UV region too small";
        return false;
    }
    c.w = x1 - x0;
    c.h = y1 - y0;
    c.px.resize(size_t(c.w) * c.h);
    for (int y = 0; y < c.h; ++y)
        std::memcpy(&c.px[size_t(y) * c.w], &px[size_t(flip ? y1 - 1 - y : y0 + y) * tw + x0], size_t(c.w) * 4);
    return true;
}

// The largest quad (in texels) of an object's mesh, cropped
static bool cropItem(wotm::Scene &scene, const wotm::Ram &ram, uint32_t obj, bool flip, Crop &c)
{
    std::vector<uint32_t> px;
    uint32_t tw = 0, th = 0;
    const wotm::MeshData *md = obj ? scene.mesh(ram, obj) : nullptr;
    if (!md || md->tris.empty())
    {
        c.why = "no mesh";
        return false;
    }
    if (!scene.decodeTex(ram, obj, px, tw, th, 0, false) || !tw || !th)
    {
        c.why = "texture not decoded";
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
            if (x - start >= 2)   // fewer than 2 columns: noise
                s.push_back({start, x});
            start = -1;
        }
    }
    return s;
}

// Rows with ink in columns [x0, x1): top and bottom (exclusive)
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

// Baseline of columns [x0, x1): the median foot (last row with ink) of the inked columns; descenders (p, y, g) are a minority and
// don't change it. 0 = no ink.
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

// Segments after the widest gap (the word after the space: "game" in "load game")
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

// "multiplayer" in white with an outline (the game's font), with its baseline at `base`. `P` = the "player" of "2 player".
static bool buildGlyphWord(wotm::Scene &scene, const wotm::Ram &ram, const Crop &P, bool flip, Texture2D &out, int &outBase, int &outPlayerBase)
{
    Crop O, G, U;
    if (!cropItem(scene, ram, itemObject(ram, 3, 0), flip, O) || !cropItem(scene, ram, itemObject(ram, 4, 0), flip, G) ||
        !cropItem(scene, ram, itemObject(ram, 5, 0), flip, U))
    {
        std::fprintf(stderr, "[padsel] label from big-screen letters: items without texture (%s / %s / %s)\n", O.why, G.why, U.why);
        return false;
    }
    const std::vector<Seg> sP = segments(P), sO = segments(O), sG = afterWidestGap(segments(G)), sU = segments(U);
    // "player" goes in whole (in the atlas some of its letters touch without the 170 column): it just needs some ink
    if (sP.empty() || sO.size() != 7 || sG.size() != 4 || sU.size() != 7)
    {
        std::fprintf(stderr, "[padsel] label from big-screen letters: unexpected segments (player %zu, options %zu, game %zu, unlocks %zu)\n",
                     sP.size(), sO.size(), sG.size(), sU.size());
        return false;
    }
    int t, b;
    const int baseP = baselineOf(P, sP.front().x0, sP.back().x1), baseO = baselineOf(O, 0, O.w), baseG = baselineOf(G, 0, G.w),
              baseU = baselineOf(U, 0, U.w);
    if (baseP <= 0 || baseO <= 0 || baseG <= 0 || baseU <= 0)
        return false;
    int gap = 0;   // gap between letters: the median from "options"
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
            // the letter and the touching outline on each side (the 170 column): where two meet, the more opaque pixel wins
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

// The "2 player" quads on its plane and the crop of "player" (the largest quad); `flip` from the map's orientation (v goes up on screen)
struct OwnText
{
    Quad word;            // the "player" quad
    float inkCx = 0.f;    // center of the whole "2 player" letters (the "2" and "player") on the plane
    Crop P;               // crop of "player" (only during assembly)
    bool flip = false;
};
// The "2 player" quads on the plane. `build`: decodes the texture, crops "player" and measures the center from the letters (the quads
// have uneven empty margins); otherwise just the geometry, using the texture size stored during assembly.
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
        o.P.why = "no mesh";
        return false;
    }
    if (build && (!scene.decodeTex(ram, obj, px, s_tw, s_th, 0, false) || !s_tw || !s_th))
    {
        o.P.why = "texture not decoded";
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
        o.P.why = "no word quad";
        return false;
    }
    o.word = *word;
    o.flip = word->ay > 0.f;   // larger v is higher up: the texture is upside down
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
        const float u0 = std::floor(q.u0);   // column 0 of the crop
        for (const float u : {u0 + float(s.front().x0), u0 + float(s.back().x1)})
        {
            x0 = std::min(x0, q.ax * u + q.bx);
            x1 = std::max(x1, q.ax * u + q.bx);
        }
    }
    if (x1 < x0)
    {
        o.P.why = "no letters";
        return false;
    }
    o.inkCx = 0.5f * (x0 + x1);
    return cropQuad(px, s_tw, s_th, o.word, o.flip, o.P);
}

// Vertex color of an object (GS: 0x80 = 1.0), averaged
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
    return Color{c8(r), c8(g), c8(b), c8(a)};   // dim items have alpha below 1: they darken over the big screen
}

// Once per session (the big screen's textures are resident); up to 20 attempts, one every 30 frames
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
            std::fprintf(stderr, "[padsel] label from big-screen letters: \"2 player\" (object %x) %s; keeping the font\n", ownObj, own.P.why);
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
    std::fprintf(stderr, "[padsel] label from big-screen letters: %dx%d\n", t.width, t.height);
}

// The word in place of "2 player": texel scale and baseline from the "player" quad, centered on the whole item, in the vertex color
static bool drawGlyphLabel(wotm::Scene &scene, const wotm::Ram &ram, const Plane &pl, uint32_t obj, bool lit, float alpha)
{
    OwnText own;
    if (!g_glyph.ok || !ownText(scene, ram, obj, pl, false, own))
        return false;
    const Quad &q = own.word;
    const float sx = std::fabs(q.ax), sy = std::fabs(q.ay);
    const float wx = float(g_glyph.tex.width) * sx, hy = float(g_glyph.tex.height) * sy;
    const float cx = g_glyph.inkCx;
    // foot of the assembled word = foot of the original "player" (same font and scale): the l's foot row in the crop, mapped back to the quad's v
    const float baseV = own.flip ? q.v1 - float(g_glyph.playerBase) : q.v0 + float(g_glyph.playerBase);
    const float baseY = q.ay * baseV + q.by;
    const float top = baseY + float(g_glyph.base) * sy;
    const Color tint = vertexColor(scene, ram, itemObject(ram, 2, lit ? 1 : 0));
    const float uv[4] = {0.f, 1.f, 1.f, 0.f};   // plain texture (not a render target): V top to bottom
    // the vertex alpha darkens the dim item; measured on the photos, using it raw made the label ~75% as bright as the other items
    // (the native object drawing blends differently): 1.35 x, capped at 1
    const float va = std::min(1.f, 1.35f * float(tint.a) / 255.f);
    drawOnPlane(pl, g_glyph.tex, cx - wx * 0.5f, top, wx, hy, alpha * va, 0.004f * (pl.x1 - pl.x0), uv, tint);
    return true;
}

// Big-screen movie during the selection: screen 3 ends the game's myMovie (the sheldemo from the previous menu); the native selection
// plays the same movie on the menu texture (Player::Screen), looping, and stops it on exit if the game hasn't put another one on
// (myMovie +0x94 = 1: stopped as far as the game knows). Window thread.
static void selectMovie(const wotm::Ram &ram, bool select)
{
#ifdef WOTM_HAVE_FMV
    static bool s_own = false;
    wotm::fmv::Player &p = wotm::fmv::Player::get();
    const bool gameStopped = ram.u32(wotm::sym::myMovie + 0x94) != 0;
    // while the selection is on screen, the game's End doesn't stop the movie: it continues from where it was (restarting showed the
    // fade from black at the start of the sheldemo again, a second fade-in right after the transition's)
    p.setHold(wotm::fmv::Player::Screen, select);
    if (select && gameStopped)
    {
        if (!p.active(wotm::fmv::Player::Screen) || p.finished(wotm::fmv::Player::Screen))
            p.start(wotm::fmv::Player::Screen, "mov/sheldemo.pss", false);   // looping
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
    // of an item's several objects in use (the text and the larger glow), the one with the smallest height
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
        // big-screen letters (drawGlyphLabel)
    }
    else if (v.labelObj[0] && v.labelAlpha > 0.f && g_labelRt.id && textPlane(v.labelObj, pl))
    {
        // line height = that of "1 player" (the items' letters share the same size); centered on its column (the items are
        // centered), at the item's own height
        float cx = 0.f, refH = pl.y1 - pl.y0;
        if (textPlane(v.labelRef, ref))
        {
            cx = vdot(vsub(ref.c, pl.c), pl.right);   // the text sits at the mesh centroid (the quad's rectangle has uneven margins)
            refH = ref.y1 - ref.y0;
        }
        const float texW = float(g_labelRt.texture.width), texH = float(g_labelRt.texture.height);
        // the letters take up ~74% of the item quad's height
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
        // monster selection: the whole big screen, seen head-on (frameCamera)
        // background: the big screen in black with the previous menu's movie (sheldemo, black and white) over it, edge to edge
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
        std::fprintf(stderr, "[padsel] big screen: node %x, %.1f x %.1f, center %.1f %.1f %.1f\n", v.screenObj, pl.x1 - pl.x0, pl.y1 - pl.y0, pl.c.x, pl.c.y,
                     pl.c.z);
    }
    // pad selection: below the logo (from 44% to 96% of the height), or on the whole big screen with the text menu (wotm_mainmenu.inc
    // removes the logo outside "press start"); monster selection: at the top (screen 6 has no logo and has the portrait row at the
    // bottom); centered, with the texture's aspect ratio (width capped: the height follows, centered in the band)
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
