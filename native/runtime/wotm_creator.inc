// ABOUTME: Character creator panel (CREATE on the single player list; WOTM_CREATOR scripted save): picks a monster and costume, tints
// ABOUTME: each texture (hue, saturation, brightness) with the selection's 3D model as a live preview, and saves a mod skin.
// EE thread: state, rows, input, save. Window thread: Preview (the model, tinted textures, render). Spec:
// docs/superpowers/specs/2026-11-05-character-creator-design.md
// Included by wotm_mainmenu.inc before its View (View carries a copy of State, published under g_m like the rest of the View). The
// menu's flags (g_creatorOpen, g_open, g_suppress) stay in wotm_mainmenu.inc; this file only knows the creator's own state. It is
// included inside namespace wotm_mainmenu; wotm_mainmenu.inc includes wotm_creator.hpp at global scope.

namespace wotm_creator
{
namespace cr = wotm::creator;
using wotm::mods::kMonsters;

enum Row { RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RSave, RBack, kRows };
struct Tint { float hue = 0.f, sat = 0.f, bright = 0.f; bool zero() const { return hue == 0.f && sat == 0.f && bright == 0.f; } };
constexpr int kMaxTex = 32;   // texture groups per costume the creator can tint (more are left as they are, with a log line)
constexpr const char *kLetters = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";   // what up and down roll through
constexpr int kLetterCount = int(std::char_traits<char>::length(kLetters));
constexpr int kMonsterCount = int(std::size(kMonsters));
constexpr int kTogera = 2;   // kMonsters[2]: the monster the panel opens on
static_assert(kMonsters[kTogera].type == 3, "kMonsters[kTogera] must be togera");
constexpr const char *kHints[kRows] = {
    "WHICH MONSTER TO RECOLOR", "THE COSTUME WHOSE TEXTURES ARE THE STARTING POINT", "WHICH PART OF THE SKIN THE THREE SLIDERS BELOW CHANGE",
    "TURNS THE COLORS OF THIS TEXTURE AROUND THE WHEEL", "GRAYER TO THE LEFT, STRONGER TO THE RIGHT", "DARKER TO THE LEFT, LIGHTER TO THE RIGHT",
    "THE COSTUME NAME IN THE SELECTION. X TO EDIT", "WRITES THE SKIN TO MODS/CREATOR/SKINS AND ADDS IT TO THE COSTUME LIST", "LEAVES WITHOUT SAVING"};
// on the keyboard X is Space, which types a space while editing: Enter (or Esc) ends it there
constexpr const char *kNameHint = "LEFT AND RIGHT MOVE, UP AND DOWN CHANGE THE LETTER, ENTER OR X DONE";

// What the window thread needs (published under g_m inside View::creatorState)
struct State
{
    int monster = kTogera;      // index into wotm::mods::kMonsters
    int costume = 0;            // disc costume 0..3
    int tex = 0;                // texture row's index into the costume's texture groups (groupTextures)
    int texCount = 0;           // how many groups the costume has (0 until the window thread loaded it; capped at kMaxTex)
    Tint tint[kMaxTex];
    uint32_t tintGen = 1;       // bumped on every slider change
    char name[cr::kNameLen + 1] = {};
    int cursor = 0;
    bool editing = false;
    int row = RMonster;
    int lastRow = RMonster;     // the row at the last leaveSaveCheck (moving off SAVE cancels OVERWRITE?)
    char saveValue[24] = {};    // "", "SAVED", "OVERWRITE?", "NAME IN USE", "MODS ARE OFF", "CANNOT WRITE", "SAVED, RESTART TO USE"
    double saveUntil = 0.0;     // GetTime() until which saveValue shows (0: until the next change)
    bool confirmOverwrite = false;
};
static_assert(std::is_trivially_copyable_v<State>, "State is copied into View every frame under g_m");
static_assert(sizeof(State::saveValue) >= sizeof("SAVED, RESTART TO USE"), "saveValue must hold the longest message and its NUL");
static void setSave(State &s, const char *text) { std::snprintf(s.saveValue, sizeof s.saveValue, "%s", text); }
static State g_st;                             // EE thread only (the window reads the copy in View)
static std::atomic<int> g_texCountShared{0};   // group count: written by the window thread, read by the EE thread

// "<MONSTER> N": N from 2 up while mods/Creator/skins/<id> exists; the monster's name is cut so " N" fits the 11 letters
static void defaultName(State &s)
{
    std::string mon = kMonsters[s.monster].name;
    for (char &c : mon) c = char(std::toupper(static_cast<unsigned char>(c)));
    const std::filesystem::path &mods = wotm::mods::Registry::get().dir();
    std::string name;
    for (int n = 2; n < 100; ++n)
    {
        const std::string num = " " + std::to_string(n);
        name = mon.substr(0, size_t(cr::kNameLen) - num.size()) + num;
        std::error_code ec;
        if (mods.empty() || !std::filesystem::exists(mods / cr::kMod / "skins" / cr::idFromName(name), ec)) break;
    }
    std::snprintf(s.name, sizeof s.name, "%s", name.c_str());
    s.cursor = 0;
}
// A new monster or costume: the tints start over (the textures are another set)
static void resetFor(State &s, bool monsterChanged)
{
    for (Tint &t : s.tint) t = Tint{};
    s.tex = 0;
    ++s.tintGen;
    if (monsterChanged)
    {
        s.costume = 0;
        defaultName(s);
    }
}
// X on CREATE: the panel opens on its first row; the last monster, costume and tints stay (reopening continues the work)
static void open()
{
    g_st.row = g_st.lastRow = RMonster;
    g_st.editing = false;
    setSave(g_st, "");
    g_st.saveUntil = 0.0;
    g_st.confirmOverwrite = false;
    if (g_st.name[0] == 0) defaultName(g_st);
}
// Every published frame: the group count the window found, the TEXTURE row inside it, SAVED gone after its 3 seconds
static void sync()
{
    State &s = g_st;
    s.texCount = std::clamp(g_texCountShared.load(std::memory_order_relaxed), 0, kMaxTex);
    s.tex = s.texCount > 0 ? std::clamp(s.tex, 0, s.texCount - 1) : 0;
    if (s.saveUntil > 0.0 && GetTime() >= s.saveUntil)
    {
        setSave(s, "");
        s.saveUntil = 0.0;
    }
}
// Name editing: the name as 11 slots (padded with spaces) while editing, trimmed back when it ends
static void padName(State &s)
{
    const size_t n = std::strlen(s.name);
    std::fill(s.name + n, s.name + cr::kNameLen, ' ');
    s.name[cr::kNameLen] = 0;
}
static void trimName(State &s)
{
    int n = int(std::strlen(s.name));
    while (n > 0 && s.name[n - 1] == ' ') s.name[--n] = 0;
    int lead = 0;
    while (s.name[lead] == ' ') ++lead;
    std::memmove(s.name, s.name + lead, std::strlen(s.name + lead) + 1);
    if (s.name[0] == 0) defaultName(s);   // never blank: an empty name would save as "skin"
    s.cursor = 0;
}
static int letterIndex(char c)
{
    const char *p = std::strchr(kLetters, c);
    return p && c ? int(p - kLetters) : 0;
}
// Keyboard while editing the name: a letter, digit, space or minus sets the slot under the cursor and advances it; Backspace clears
// the slot before the cursor and steps back. Every key pressed this frame counts, in ascending key code. Returns true when a key was
// typed: that frame's panel actions must not count too (Space is X, E is circle, WASD the left stick in the default map). The edge
// state is kept every frame, so the key that started editing (Space, as X) does not also type.
struct TypeKey { int code; char ch; };   // ch 0: Backspace
static const std::array<TypeKey, kLetterCount + 1> &typeKeyTable()
{
    static const std::array<TypeKey, kLetterCount + 1> t = [] {
        std::array<TypeKey, kLetterCount + 1> a{};
        for (int i = 0; i < kLetterCount; ++i)
        {
            const char c = kLetters[i];
            a[size_t(i)] = {c == ' ' ? KEY_SPACE : c == '-' ? KEY_MINUS : int(c), c};   // raylib's KEY_A..Z, KEY_ZERO..NINE are ASCII
        }
        a[kLetterCount] = {KEY_BACKSPACE, 0};
        std::sort(a.begin(), a.end(), [](const TypeKey &x, const TypeKey &y) { return x.code < y.code; });
        return a;
    }();
    return t;
}
static bool typeKeys(State &s)
{
    static bool prev[kLetterCount + 1] = {};   // per typeKeyTable() entry
    const auto &keys = typeKeyTable();
    bool typed = false;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const bool down = wotm_pad::inputDown(keys[i].code);
        const bool edge = down && !prev[i];
        prev[i] = down;
        if (!edge || !s.editing) continue;
        typed = true;
        if (keys[i].ch)
        {
            s.name[s.cursor] = keys[i].ch;
            s.cursor = std::min(s.cursor + 1, cr::kNameLen - 1);
        }
        else
        {
            s.cursor = std::max(s.cursor - 1, 0);
            s.name[s.cursor] = ' ';
        }
    }
    return typed;
}
// Moving off SAVE cancels a pending OVERWRITE? (a timed SAVED stays for its 3 seconds). Checked against the row of the last check,
// since the mouse moves the row before input() runs
static void leaveSaveCheck(State &s)
{
    if (s.row != s.lastRow && s.lastRow == RSave)
    {
        s.confirmOverwrite = false;
        if (s.saveUntil == 0.0) setSave(s, "");
    }
    s.lastRow = s.row;
}
static float stepClamp(float v, int dir, float step, float lo, float hi) { return std::clamp(v + float(dir) * step, lo, hi); }

// Editing the name: left and right move the cursor, up and down roll the letter, X or triangle end editing
static void editInput(State &s, const bool e[kPanelActs], uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (e[POk] || e[PBack])
    {
        s.editing = false;
        trimName(s);
        std::fprintf(stderr, "[creator] name = \"%s\"\n", s.name);
        panelSfx(rdram, ctx, runtime, e[POk] ? 8 : 6);
    }
    else if (e[PLeft] || e[PRight])
    {
        s.cursor = std::clamp(s.cursor + (e[PLeft] ? -1 : 1), 0, cr::kNameLen - 1);
        panelSfx(rdram, ctx, runtime, e[PLeft] ? 2 : 4);
    }
    else if (e[PUp] || e[PDown])
    {
        s.name[s.cursor] = kLetters[(letterIndex(s.name[s.cursor]) + (e[PDown] ? 1 : kLetterCount - 1)) % kLetterCount];
        panelSfx(rdram, ctx, runtime, e[PDown] ? 4 : 2);
    }
}
// Left or right on the current row: steps its value. Returns true when something changed.
static bool adjust(State &s, int dir)
{
    switch (s.row)
    {
    case RMonster:
        s.monster = (s.monster + dir + kMonsterCount) % kMonsterCount;
        resetFor(s, true);
        return true;
    case RCostume:
    {
        const int n = std::max(1, wotm_monsters::costumes(kMonsters[s.monster].type));
        s.costume = (s.costume + dir + n) % n;
        resetFor(s, false);
        return true;
    }
    case RTexture:
    {
        const int n = std::clamp(g_texCountShared.load(std::memory_order_relaxed), 0, kMaxTex);
        if (n > 0) s.tex = (std::clamp(s.tex, 0, n - 1) + dir + n) % n;
        return n > 1;
    }
    case RHue:
    case RSat:
    case RBright:
    {
        Tint &t = s.tint[std::clamp(s.tex, 0, kMaxTex - 1)];
        if (s.row == RHue)
        {
            t.hue += float(dir) * 15.f;   // wraps inside -180..+180 (both ends are the same hue: +195 is -165)
            if (t.hue > 180.f) t.hue -= 360.f;
            if (t.hue < -180.f) t.hue += 360.f;
        }
        else if (s.row == RSat) t.sat = stepClamp(t.sat, dir, 10.f, -100.f, 100.f);
        else t.bright = stepClamp(t.bright, dir, 10.f, -60.f, 60.f);
        ++s.tintGen;
        return true;
    }
    default: return false;
    }
}

// One frame of the open panel (e: this frame's presses, readPanel's edges plus the mouse). Returns true when the panel closes.
static bool input(const bool e[kPanelActs], uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    State &s = g_st;
    leaveSaveCheck(s);
    if (typeKeys(s))
    {
        panelSfx(rdram, ctx, runtime, 4);
        return false;
    }
    if (s.editing)
    {
        editInput(s, e, rdram, ctx, runtime);
        return false;
    }
    if (e[PBack] || (e[POk] && s.row == RBack))
    {
        panelSfx(rdram, ctx, runtime, 6);
        return true;
    }
    if (e[PUp] || e[PDown])
    {
        s.row = (s.row + (e[PDown] ? 1 : kRows - 1)) % kRows;
        panelSfx(rdram, ctx, runtime, e[PDown] ? 4 : 2);
    }
    else if (e[POk] && s.row == RName)
    {
        s.editing = true;
        padName(s);
        s.cursor = 0;
        panelSfx(rdram, ctx, runtime, 8);
    }
    else if (e[POk] && s.row == RSave)
    {
        // Task 7 replaces this placeholder with save(s, false) (the files, the registry, OVERWRITE? and the errors)
        setSave(s, "SAVED");
        s.saveUntil = GetTime() + 3.0;
        std::fprintf(stderr, "[creator] save (placeholder): %s costume %d \"%s\"\n", kMonsters[s.monster].name, s.costume, s.name);
        panelSfx(rdram, ctx, runtime, 8);
    }
    else if ((e[PLeft] || e[PRight]) && adjust(s, e[PLeft] ? -1 : 1))
    {
        if (s.saveUntil == 0.0) setSave(s, "");   // OVERWRITE? (Task 7) no longer applies to what is on screen
        s.confirmOverwrite = false;
        panelSfx(rdram, ctx, runtime, e[PLeft] ? 2 : 4);
    }
    leaveSaveCheck(s);
    return false;
}
} // namespace wotm_creator
