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

// The kinds of row; which ones the panel shows, and in what order, depends on TYPE (rowsOf)
enum Row { RType, RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RCpu, RSave, RBack, kRowKinds };
constexpr Row kSkinRows[] = {RType, RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RSave, RBack};
constexpr Row kMonsterRows[] = {RType, RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RCpu, RSave, RBack};
constexpr int kMaxRows = int(std::size(kMonsterRows));   // the longer list (the panel's layout is fixed on it)
struct Tint { float hue = 0.f, sat = 0.f, bright = 0.f; bool zero() const { return hue == 0.f && sat == 0.f && bright == 0.f; } };
constexpr int kMaxTex = 32;   // texture groups per costume the creator can tint (more are left as they are, with a log line)
constexpr const char *kLetters = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";   // what up and down roll through
constexpr int kLetterCount = int(std::char_traits<char>::length(kLetters));
constexpr int kMonsterCount = int(std::size(kMonsters));
constexpr int kTogera = 2;   // kMonsters[2]: the monster the panel opens on
static_assert(kMonsters[kTogera].type == 3, "kMonsters[kTogera] must be togera");
constexpr const char *kHints[kRowKinds] = {
    "MAKE A NEW COSTUME OR A NEW MONSTER",
    "WHICH MONSTER TO RECOLOR", "THE COSTUME WHOSE TEXTURES ARE THE STARTING POINT", "WHICH PART OF THE SKIN THE THREE SLIDERS BELOW CHANGE",
    "TURNS THE COLORS OF THIS TEXTURE AROUND THE WHEEL", "GRAYER TO THE LEFT, STRONGER TO THE RIGHT", "DARKER TO THE LEFT, LIGHTER TO THE RIGHT",
    "THE COSTUME NAME IN THE SELECTION. X TO EDIT", "WHETHER COMPUTER PLAYERS MAY PICK IT",
    "WRITES THE SKIN AND ADDS IT TO THE COSTUME LIST", "LEAVES WITHOUT SAVING"};   // the game font has no '/'
// on the keyboard X is Space, which types a space while editing: Enter (or Esc) ends it there
constexpr const char *kNameHint = "LEFT AND RIGHT MOVE, UP AND DOWN CHANGE THE LETTER, ENTER OR X DONE";

// What the window thread needs (published under g_m inside View::creatorState)
struct State
{
    int monster = kTogera;      // index into wotm::mods::kMonsters
    int costume = 0;            // disc costume 0..3
    int tex = 0;                // texture row's index into the costume's texture groups (groupTextures)
    int texCount = 0;           // how many groups the current costume has (0 while the window thread loads it; capped at kMaxTex)
    Tint tint[kMaxTex];
    uint32_t tintGen = 1;       // bumped on every slider change
    char name[cr::kNameLen + 1] = {};
    int cursor = 0;
    bool editing = false;
    bool monsterType = false;   // TYPE: false SKIN, true MONSTER
    bool cpu = false;           // MONSTER: CPU CAN USE
    int row = 0;                // position in rowsOf(*this)
    int lastRow = RType;        // the row kind at the last leaveSaveCheck (moving off SAVE cancels OVERWRITE?)
    char saveValue[24] = {};    // "", "SAVED", "OVERWRITE?", "NAME IN USE", "MODS ARE OFF", "NO MODS FOLDER", "CANNOT WRITE",
                                // "NOT FOUND", kSavedRestart, "START WITH A LETTER" (MONSTER: a custom's id cannot start with a digit)
    double saveUntil = 0.0;     // GetTime() until which saveValue shows (0: until the next change)
    bool confirmOverwrite = false;
};
static_assert(std::is_trivially_copyable_v<State>, "State is copied into View every frame under g_m");
// The rows TYPE shows, in panel order; State::row is a position in it
struct RowList { const Row *row; int n; };
static RowList rowsOf(const State &s)
{
    if (s.monsterType) return {kMonsterRows, int(std::size(kMonsterRows))};
    return {kSkinRows, int(std::size(kSkinRows))};
}
// The kind of the row at position pos of s's list (RBack outside it)
static Row rowAt(const State &s, int pos)
{
    const RowList l = rowsOf(s);
    return pos >= 0 && pos < l.n ? l.row[pos] : RBack;
}
static Row rowKind(const State &s) { return rowAt(s, s.row); }
// The row's hint: kHints, with MONSTER's own wording where the row means something else there
static const char *hintFor(const State &s, Row kind)
{
    if (s.monsterType)
        switch (kind)
        {
        case RMonster: return "THE MONSTER IT PLAYS AS";
        case RName: return "THE MONSTER NAME IN THE CUSTOM LIST. X TO EDIT";
        case RSave: return "WRITES THE MONSTER AND ADDS IT TO THE CUSTOM LIST";
        default: break;
        }
    return kHints[kind];
}
// The SAVE row's messages are drawn in the game font: it has '?' and '.', but no ',' could be confirmed in its atlas
constexpr const char kSavedRestart[] = "SAVED. RESTART TO USE";   // the longest message
static_assert(sizeof(State::saveValue) >= sizeof(kSavedRestart), "saveValue must hold the longest message and its NUL");
static void setSave(State &s, const char *text) { std::snprintf(s.saveValue, sizeof s.saveValue, "%s", text); }
static State g_st;                             // EE thread only (the window reads the copy in View)
// The preview's texture group count, keyed by what it was loaded for: monster | costume << 8 | count << 16, one atomic so the key and
// the count always arrive together. Written by the window thread only (the Preview, below); the EE thread reads it through texCountFor
constexpr uint32_t kNoTexCount = 0xFFu;   // monster 0xFF: matches no State (nothing loaded)
static std::atomic<uint32_t> g_texCountShared{kNoTexCount};
static uint32_t packTexCount(int monster, int costume, int count)
{
    return uint32_t(monster & 0xFF) | uint32_t(costume & 0xFF) << 8 | uint32_t(std::clamp(count, 0, kMaxTex)) << 16;
}
// Groups of s's monster and costume, 0 while the window has not loaded them yet (a count of the previous costume does not count:
// its group indices may not exist in this one)
static int texCountFor(const State &s)
{
    const uint32_t v = g_texCountShared.load(std::memory_order_acquire);
    if ((v & 0xFFu) != uint32_t(s.monster & 0xFF) || ((v >> 8) & 0xFFu) != uint32_t(s.costume & 0xFF)) return 0;
    return std::clamp(int(v >> 16), 0, kMaxTex);
}

// The monster's name in capitals (the MONSTER row and the preview's caption)
static std::string monsterLabel(int monster)
{
    std::string name = kMonsters[std::clamp(monster, 0, kMonsterCount - 1)].name;
    for (char &c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
    return name;
}
// "<MONSTER> N": N from 2 up while mods/Creator/skins/<id> exists (MONSTER: or mods/Creator/monsters/<id>, the custom's own folder;
// its skin takes skins/<id> too); the monster's name is cut so " N" fits the 11 letters
static std::string defaultNameFor(int monster, bool monsterType)
{
    std::string mon = kMonsters[std::clamp(monster, 0, kMonsterCount - 1)].name;
    for (char &c : mon) c = char(std::toupper(static_cast<unsigned char>(c)));
    const std::filesystem::path &mods = wotm::mods::Registry::get().dir();
    std::string name;
    for (int n = 2; n < 100; ++n)
    {
        const std::string num = " " + std::to_string(n);
        name = mon.substr(0, size_t(cr::kNameLen) - num.size()) + num;
        if (mods.empty()) break;
        const std::string id = cr::idFromName(name);
        std::error_code ec, ec2;
        const bool used = std::filesystem::exists(mods / cr::kMod / "skins" / id, ec) ||
                          (monsterType && std::filesystem::exists(mods / cr::kMod / "monsters" / id, ec2));
        if (!used) break;
    }
    return name;
}
static void defaultName(State &s)
{
    std::snprintf(s.name, sizeof s.name, "%s", defaultNameFor(s.monster, s.monsterType).c_str());
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
    g_st.row = 0;
    g_st.lastRow = rowKind(g_st);
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
    s.texCount = texCountFor(s);
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
    const Row kind = rowKind(s);
    if (kind != s.lastRow && s.lastRow == RSave)
    {
        s.confirmOverwrite = false;
        if (s.saveUntil == 0.0) setSave(s, "");
    }
    s.lastRow = kind;
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
    const Row kind = rowKind(s);
    switch (kind)
    {
    case RType:
    {
        // the monster, costume and tints stay (a skin can become a monster); the name follows only while it is the old default
        const bool wasDefault = std::string(s.name) == defaultNameFor(s.monster, s.monsterType);
        s.monsterType = !s.monsterType;
        s.row = std::clamp(s.row, 0, rowsOf(s).n - 1);
        if (wasDefault) defaultName(s);
        return true;
    }
    case RCpu:
        s.cpu = !s.cpu;
        return true;
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
        const int n = texCountFor(s);
        if (n > 0) s.tex = (std::clamp(s.tex, 0, n - 1) + dir + n) % n;
        return n > 1;
    }
    case RHue:
    case RSat:
    case RBright:
    {
        Tint &t = s.tint[std::clamp(s.tex, 0, kMaxTex - 1)];
        if (kind == RHue)
        {
            t.hue += float(dir) * 15.f;   // wraps inside -180..+180 (both ends are the same hue: +195 is -165)
            if (t.hue > 180.f) t.hue -= 360.f;
            if (t.hue < -180.f) t.hue += 360.f;
        }
        else if (kind == RSat) t.sat = stepClamp(t.sat, dir, 10.f, -100.f, 100.f);
        else t.bright = stepClamp(t.bright, dir, 10.f, -60.f, 60.f);
        ++s.tintGen;
        return true;
    }
    default: return false;
    }
}

static void save(State &s, bool script);   // below, after groupsOf

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
    const Row kind = rowKind(s);
    if (e[PBack] || (e[POk] && kind == RBack))
    {
        panelSfx(rdram, ctx, runtime, 6);
        return true;
    }
    if (e[PUp] || e[PDown])
    {
        const int n = rowsOf(s).n;
        s.row = (std::clamp(s.row, 0, n - 1) + (e[PDown] ? 1 : n - 1)) % n;
        panelSfx(rdram, ctx, runtime, e[PDown] ? 4 : 2);
    }
    else if (e[POk] && kind == RName)
    {
        s.editing = true;
        padName(s);
        s.cursor = 0;
        panelSfx(rdram, ctx, runtime, 8);
    }
    else if (e[POk] && kind == RSave)
    {
        save(s, false);   // the files, the registry, OVERWRITE? and the errors (SKIN or MONSTER, by s.monsterType)
        // OK only when it saved; OVERWRITE? is a question (the cursor sound); an error, the back sound
        const bool saved = std::strncmp(s.saveValue, "SAVED", 5) == 0;
        panelSfx(rdram, ctx, runtime, saved ? 8 : std::strcmp(s.saveValue, "OVERWRITE?") == 0 ? 4 : 6);
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
// Texture groups of a loaded Model (decodeAllTextures already run): the indices of textures() per (rtx, texId), the first kMaxTex
// groups (one log line when it cuts). A group's members can in rare cases disagree on hiAlpha; the first member's decides (the PNG
// is per texId). Depends only on the Model: the preview (window thread) and the save (EE thread) both call it.
static std::vector<std::vector<int>> groupsOf(const wotm::model::Model &m)
{
    std::vector<cr::TexKey> keys;
    keys.reserve(m.texInfo().size());
    for (const wotm::model::TexInfo &t : m.texInfo()) keys.push_back(cr::TexKey{t.id, t.rtx});
    std::vector<std::vector<int>> g = cr::groupTextures(keys);
    if (g.size() > size_t(kMaxTex))
    {
        std::fprintf(stderr, "[creator] %s: %zu texture groups, tinting the first %d\n", m.name().c_str(), g.size(), kMaxTex);
        g.resize(size_t(kMaxTex));
    }
    return g;
}

// A file name in lowercase ASCII (anything else becomes '?'), read from the native name: no narrow conversion that could throw on a
// name outside the code page
static std::string asciiName(const std::filesystem::path &p)
{
    std::string out;
    const std::filesystem::path file = p.filename();   // kept alive: native() is a reference into it
    for (const auto c : file.native())
    {
        const auto u = static_cast<std::make_unsigned_t<std::decay_t<decltype(c)>>>(c);
        out += u < 128 ? char(std::tolower(int(u))) : '?';
    }
    return out;
}
// What cr::pngName writes, followed by `tail` (".png"; ".png.tmp" for a save's temp file): "t" or "rtx_t", 4 digits. n: asciiName's
static bool isSkinPngName(const std::string &n, const char *tail)
{
    const size_t at = n.rfind("rtx_", 0) == 0 ? 4 : 0, len = std::strlen(tail);
    if (n.size() != at + 5 + len || n[at] != 't') return false;
    for (size_t i = at + 1; i < at + 5; ++i)
        if (n[i] < '0' || n[i] > '9') return false;
    return n.compare(at + 5, len, tail) == 0;
}

// Section 3 of the spec (step 2: Section 2 of 2026-11-06-creator-monster-design.md), in steps. Each uses only its arguments and the
// registries, no window state: WOTM_CREATOR and WOTM_CREATOR_MONSTER run them at boot, before the window exists. Log paths are written
// as mods/Creator/skins/<id> (the id is ASCII; the mods folder may not be).
struct SaveCtx
{
    State &s;
    const std::string &id, &name;
    bool script;
    const wotm::mods::MonsterInfo &mi;
    std::filesystem::path mods;   // the registry's mods folder (empty: mods off)
    std::filesystem::path dir;    // mods/Creator/skins/<id>
    bool monster = false;         // MONSTER: a custom (monsterDir/monster.json) that owns the skin in dir
    std::filesystem::path monsterDir;   // mods/Creator/monsters/<id> (MONSTER only)
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> files;   // temp -> final, in write order
    std::set<std::string> written;   // the skin folder's file names this save wrote (lowercase): removeStale keeps these
    size_t rawCount = 0;
    void fail(const char *msg)
    {
        setSave(s, msg);
        s.saveUntil = 0.0;
    }
    // Every temp file written so far goes: a failed save leaves the folder as it was
    void dropTemps()
    {
        for (const auto &f : files)
        {
            std::error_code ec;
            std::filesystem::remove(f.first, ec);
        }
        files.clear();
    }
};

// Windows ignores case in paths: writing under "<parent>/mods" when the parent has "MODS" or "Mods" (and no "mods") would land in that
// folder, which the boot's choice of the mods folder (wotmModsInstall: an entry named exactly "mods") would not read back. True when
// the folder exists under exactly that name, or nothing else answers to it (a missing folder is created as named); false with the
// other entry's name (ASCII, as it is on disk) in `other`.
static bool modsFolderIsItself(std::filesystem::path mods, std::string &other)
{
    namespace fs = std::filesystem;
    mods = mods.lexically_normal();
    if (mods.filename().empty()) mods = mods.parent_path();   // "C:/x/mods/"
    const fs::path leaf = mods.filename(), parent = mods.parent_path().empty() ? fs::path(".") : mods.parent_path();
    // only a folder named "mods", the name the boot looks for without WOTM_MODS; another WOTM_MODS folder is read back by the same
    // path whatever its case
    if (leaf.native() != fs::path("mods").native()) return true;
    const std::string want = asciiName(leaf);
    bool exact = false, alias = false;
    std::error_code ec;
    for (fs::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec))
    {
        const fs::path name = it->path().filename();
        if (name.native() == leaf.native()) exact = true;
        else if (asciiName(name) == want)
        {
            alias = true;
            other.clear();
            for (const auto ch : name.native())
                other += static_cast<std::make_unsigned_t<std::decay_t<decltype(ch)>>>(ch) < 128 ? char(ch) : '?';
        }
    }
    return exact || !alias;
}

// MONSTER: the id is free for a creator custom. NAME IN USE for a game monster's name or prefix (customBase) and for a custom that
// comes from another folder than monsterDir (another mod's); START WITH A LETTER for what readCustom refuses as an id
static bool monsterIdFree(SaveCtx &c)
{
    namespace fs = std::filesystem;
    if (wotm::mods::customBase(c.id))
    {
        c.fail("NAME IN USE");
        std::fprintf(stderr, "[creator] %s is a game monster's name; pick another name\n", c.id.c_str());
        return false;
    }
    if (std::isdigit(static_cast<unsigned char>(c.id[0])))
    {
        c.fail("START WITH A LETTER");
        std::fprintf(stderr, "[creator] %s: a monster's id starts with a letter; pick another name\n", c.id.c_str());
        return false;
    }
    const wotm::mods::Custom *o = wotm::mods::Registry::get().custom(c.id);
    auto same = [](const fs::path &p) {   // the same folder however it was written (as the registry's sameDir)
        std::error_code ec;
        const fs::path n = fs::weakly_canonical(p, ec);
        return ec ? p.lexically_normal() : n;
    };
    if (o && same(o->dir) != same(c.monsterDir))
    {
        c.fail("NAME IN USE");
        std::fprintf(stderr, "[creator] a monster %s already comes from %s; pick another name\n", c.id.c_str(), o->mod.c_str());
        return false;
    }
    return true;
}

// MODS ARE OFF, NO MODS FOLDER, NAME IN USE (a skin folder of another owner; MONSTER: monsterIdFree) or OVERWRITE? (an existing
// folder, first X): false stops the save
static bool checkExisting(SaveCtx &c)
{
    namespace fs = std::filesystem;
    if (c.dir.empty())
    {
        c.fail("MODS ARE OFF");
        std::fprintf(stderr, "[creator] mods are off (WOTM_MODS=0): %s not saved\n", c.id.c_str());
        return false;
    }
    std::string other;
    if (!modsFolderIsItself(c.mods, other))
    {
        c.fail("NO MODS FOLDER");
        std::fprintf(stderr, "[creator] no mods folder (\"%s\" there answers to \"%s\"): %s not saved; create mods/ next to the game\n",
                     other.c_str(), asciiName(c.mods).c_str(), c.id.c_str());
        return false;
    }
    if (c.monster && !monsterIdFree(c)) return false;
    std::error_code ec, ec2;
    const bool skinThere = fs::exists(c.dir, ec), monsterThere = c.monster && fs::exists(c.monsterDir, ec2);
    if (!skinThere && !monsterThere) return true;
    // a skin folder holds one owner's skin: SKIN, the monster's; MONSTER, the custom's own (its "monster" is the id). A missing or
    // unreadable skin.json is not "in use" (the overwrite question applies)
    std::ifstream in(c.dir / "skin.json", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    wotm::json::Value v;
    const wotm::json::Value *mon = nullptr;
    if (skinThere && in.is_open() && wotm::json::parse(text, v) && (mon = v.get("monster")) && mon->type == wotm::json::Value::Str)
    {
        std::string other = mon->s;
        for (char &ch : other) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (other != (c.monster ? c.id : std::string(c.mi.name)))
        {
            c.fail("NAME IN USE");
            if (wotm::mods::monsterInfo(other))
                std::fprintf(stderr, "[creator] %s is a %s skin; pick another name\n", c.id.c_str(), other.c_str());
            else
                std::fprintf(stderr, "[creator] mods/%s/skins/%s belongs to the monster %s; pick another name\n", cr::kMod, c.id.c_str(),
                             other.c_str());
            return false;
        }
    }
    if (!c.s.confirmOverwrite && !c.script)
    {
        c.fail("OVERWRITE?");
        c.s.confirmOverwrite = true;
        std::fprintf(stderr, "[creator] mods/%s/%s/%s exists: X again overwrites\n", cr::kMod, monsterThere ? "monsters" : "skins",
                     c.id.c_str());
        return false;
    }
    return true;
}

// Bytes to `path` (binary, wide-path safe). false on any error.
static bool writeBytes(const std::filesystem::path &path, const void *data, size_t n)
{
    std::ofstream o(path, std::ios::binary | std::ios::trunc);
    o.write(static_cast<const char *>(data), std::streamsize(n));
    o.close();
    return !o.fail();
}

// One temp PNG per tinted group (script: every group) from the EE thread's own Model of the disc costume, decoded the way the
// preview's is (load + decodeAllTextures, same order). No tinted group: nothing to load (the skin is the base costume renamed).
static bool writeGroupPngs(SaveCtx &c)
{
    bool any = c.script;
    for (const Tint &t : c.s.tint) any |= !t.zero();
    if (!any) return true;
    const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    const std::string file = std::string(c.mi.prefix) + std::to_string(c.s.costume);
    wotm::model::Model m;
    if (!m.load(io.cdRoot, io.cdImage.string(), file))
    {
        c.fail("NOT FOUND");
        std::fprintf(stderr, "[creator] MON/%s.NGP not found: %s not saved\n", file.c_str(), c.id.c_str());
        return false;
    }
    m.decodeAllTextures();
    const std::vector<std::vector<int>> groups = groupsOf(m);
    const std::vector<wotm::model::TexImage> &texs = m.textures();
    const std::vector<wotm::model::TexInfo> &info = m.texInfo();
    for (size_t g = 0; g < groups.size(); ++g)
    {
        const Tint &k = c.s.tint[g];
        if (!c.script && k.zero()) continue;   // the base costume's own texture serves (the registry aliases it)
        // one PNG per texId, whatever the palette: the group's first texture decides (as in the preview's TEXTURE row)
        const size_t i = size_t(groups[g][0]);
        std::vector<uint32_t> px = texs[i].rgba;
        wotm::tint::shift(px, k.hue, k.sat, k.bright);
        // hiAlpha: loadPng reads such a PNG raw (the fight: the GS's alpha; the model path: min(255, 2a) with TEX0.TCC set), and the
        // decoded pixels saturate every raw alpha from 0x80 up to 255, so the PNG gets the texture's own raw bytes: the fight then
        // draws the disc's alpha and the selection what the preview showed. Model::rawAlpha is empty when TCC is clear (no reader
        // uses the alpha then); hiAlpha is the same rawAmax > 0x80 test in Model::decodeTex and Scene::modTexture.
        if (info[i].hiAlpha && cr::applyRawAlpha(px, m.rawAlpha(i))) ++c.rawCount;
        const std::string png = cr::pngName(info[i].id, info[i].rtx);
        // encoded in memory and written through the path (the mods folder may hold characters a narrow file name cannot); the temp
        // name "<png>.tmp" ends in neither .png nor .json, so no scan or texture lookup ever takes it
        const std::filesystem::path tmp = c.dir / (png + ".tmp");
        c.files.emplace_back(tmp, c.dir / png);
        Image img{px.data(), texs[i].w, texs[i].h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        int size = 0;
        unsigned char *data = ExportImageToMemory(img, ".png", &size);
        const bool ok = data && size > 0 && writeBytes(tmp, data, size_t(size));
        if (data) MemFree(data);
        if (!ok)
        {
            c.fail("CANNOT WRITE");
            std::fprintf(stderr, "[creator] cannot write mods/%s/skins/%s/%s\n", cr::kMod, c.id.c_str(), png.c_str());
            return false;
        }
        c.written.insert(png);
    }
    return true;
}

// skin.json through its temp file, after the PNGs in `files` (MONSTER: its "monster" is the custom's id)
static bool writeSkinJson(SaveCtx &c)
{
    const std::string text = cr::skinJson(c.monster ? c.id : std::string(c.mi.name), c.id, c.name, c.s.costume);
    const std::filesystem::path tmp = c.dir / "skin.json.tmp";
    c.files.emplace_back(tmp, c.dir / "skin.json");
    if (writeBytes(tmp, text.data(), text.size())) return true;
    c.fail("CANNOT WRITE");
    std::fprintf(stderr, "[creator] cannot write mods/%s/skins/%s/skin.json\n", cr::kMod, c.id.c_str());
    return false;
}

// MONSTER: monster.json through its temp file, last in `files`, so commitFiles renames it after every PNG and the skin.json (a failed
// save never leaves a custom whose skin is missing). Its own skin is the one this save writes (same id).
static bool writeMonsterJson(SaveCtx &c)
{
    const std::string text = cr::monsterJson(c.id, c.name, c.mi.name, c.s.costume, c.s.cpu, c.id);
    const std::filesystem::path tmp = c.monsterDir / "monster.json.tmp";
    c.files.emplace_back(tmp, c.monsterDir / "monster.json");
    if (writeBytes(tmp, text.data(), text.size())) return true;
    c.fail("CANNOT WRITE");
    std::fprintf(stderr, "[creator] cannot write mods/%s/monsters/%s/monster.json\n", cr::kMod, c.id.c_str());
    return false;
}

// Every temp file over its final name (PNGs first, then skin.json, monster.json last). A rename that fails stops there: the temps left
// go, the files already renamed stay (a rename in the same folder failing midway is not expected; the log says so).
static bool commitFiles(SaveCtx &c)
{
    for (size_t k = 0; k < c.files.size(); ++k)
    {
        std::error_code ec;
        std::filesystem::rename(c.files[k].first, c.files[k].second, ec);
        if (!ec) continue;
        std::fprintf(stderr, "[creator] cannot write mods/%s/%s/%s: rename failed after %zu of %zu files: %s\n", cr::kMod,
                     asciiName(c.files[k].second.parent_path().parent_path()).c_str(), c.id.c_str(), k, c.files.size(), ec.message().c_str());
        c.files.erase(c.files.begin(), c.files.begin() + std::ptrdiff_t(k));
        c.dropTemps();
        c.fail("CANNOT WRITE");
        return false;
    }
    c.files.clear();
    return true;
}

// PNGs this save did not write (zero-tint groups of an earlier save, another costume's texIds) would retexture this costume; temp
// files an interrupted save left go too. Collected first, removed after the walk; no call here throws.
static void removeStale(SaveCtx &c)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> stale;
    std::error_code ec;
    for (fs::directory_iterator it(c.dir, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code fe;
        if (!it->is_regular_file(fe)) continue;
        const std::string n = asciiName(it->path());
        if ((isSkinPngName(n, ".png") && !c.written.count(n)) || isSkinPngName(n, ".png.tmp") || n == "skin.json.tmp")
            stale.push_back(it->path());
    }
    for (const fs::path &p : stale)
    {
        std::error_code rm;
        fs::remove(p, rm);
        std::fprintf(stderr, "[creator] removed %s (not in this save)%s\n", asciiName(p).c_str(), rm ? " FAILED" : "");
    }
}

// Registry::addSkin and the PNG map (MONSTER: Registry::addCustom first; its skin names it, so a refused custom means no addSkin):
// SAVED, or SAVED. RESTART TO USE when the registry refuses (the files stay; the next boot reads them)
static void registerLive(SaveCtx &c)
{
    std::string why;
    wotm::mods::Registry &reg = wotm::mods::Registry::get();
    const bool customOk = !c.monster || reg.addCustom(c.monsterDir, why);
    if (const wotm::mods::Skin *k = customOk ? reg.addSkin(c.dir, why) : nullptr)
    {
        const std::string stem = k->prefix + std::to_string(k->index);
        std::vector<wotm::mods::SkinPng> pngs;
        for (wotm::mods::SkinPng &p : reg.skinPngs())
            if (p.stem == stem) pngs.push_back(std::move(p));
        wotm::texmods::Index::get().replaceSkin(stem, pngs);
        setSave(c.s, "SAVED");
    }
    else
    {
        setSave(c.s, kSavedRestart);
        std::fprintf(stderr, "[creator] not registered: %s\n", why.c_str());
    }
}

// Writes mods/Creator/skins/<id>/ (one PNG per tinted texture group, skin.json) and registers it (script: every group is written,
// no OVERWRITE? question, no timed message); MONSTER (s.monsterType) also mods/Creator/monsters/<id>/monster.json, the custom that
// owns that skin. Every file goes to a temp name first and is renamed over the old one only when all were written, so a failed save
// leaves the previous skin (and custom) as it was.
static void saveAs(State &s, const std::string &id, const std::string &name, bool script)
{
    namespace fs = std::filesystem;
    const auto t0 = std::chrono::steady_clock::now();
    const fs::path &mods = wotm::mods::Registry::get().dir();
    SaveCtx c{s, id, name, script, kMonsters[std::clamp(s.monster, 0, kMonsterCount - 1)], mods,
              mods.empty() ? fs::path{} : mods / cr::kMod / "skins" / id};
    c.monster = s.monsterType;
    if (c.monster && !mods.empty()) c.monsterDir = mods / cr::kMod / "monsters" / id;
    if (!checkExisting(c)) return;
    s.confirmOverwrite = false;
    std::error_code ec;
    fs::create_directories(c.dir, ec);
    if (!ec && c.monster) fs::create_directories(c.monsterDir, ec);
    // only when empty: a folder this failed save created goes, an earlier save's stays
    auto dropEmpty = [&c] {
        std::error_code rm;
        fs::remove(c.dir, rm);
        if (c.monster) fs::remove(c.monsterDir, rm);
    };
    if (ec)
    {
        c.fail("CANNOT WRITE");
        std::fprintf(stderr, "[creator] cannot write mods/%s/%s/%s: %s\n", cr::kMod, c.monster ? "monsters" : "skins", id.c_str(),
                     ec.message().c_str());
        dropEmpty();
        return;
    }
    if (!writeGroupPngs(c) || !writeSkinJson(c) || (c.monster && !writeMonsterJson(c)))
    {
        c.dropTemps();
        dropEmpty();
        return;
    }
    if (!commitFiles(c)) return;
    removeStale(c);
    registerLive(c);
    s.saveUntil = script ? 0.0 : GetTime() + 3.0;   // GetTime needs the window: the script runs before it exists
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const char *raw = c.rawCount ? "; raw alpha kept on the textures above 0x80" : "";
    if (c.monster)
        std::fprintf(stderr, "[creator] saved monster %s (base %s costume %d, %zu png) in %.0f ms%s\n", id.c_str(), c.mi.name,
                     s.costume, c.written.size(), ms, raw);
    else
        std::fprintf(stderr, "[creator] saved mods/%s/skins/%s (%zu png) in %.0f ms%s\n", cr::kMod, id.c_str(), c.written.size(), ms, raw);
}
// X on SAVE: the id from the typed name (trimmed), shown as typed
static void save(State &s, bool script)
{
    std::string name = s.name;
    while (!name.empty() && name.back() == ' ') name.pop_back();
    name.erase(0, std::min(name.find_first_not_of(' '), name.size()));
    saveAs(s, cr::idFromName(name), name, script);
}
// WOTM_CREATOR=<monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]] and WOTM_CREATOR_MONSTER=<base>:<costume>:<id>[:...] (a custom
// monster, CPU CAN USE off) at boot (wotmCreatorInstall, after wotmModsInstall): the same save with the values on every texture and
// the name = the id in capitals; the game then boots as usual. Both may be set: the skin first.
static void scriptSave(const char *var, bool monsterType)
{
    const char *env = std::getenv(var);
    if (!env || !*env) return;
    cr::Script sc;
    std::string err;
    if (!(monsterType ? cr::parseMonsterScript(env, sc, err) : cr::parseScript(env, sc, err)))
    {
        std::fprintf(stderr, "[creator] %s ignored: %s\n", var, err.c_str());
        return;
    }
    State st;
    st.monsterType = monsterType;
    st.cpu = false;
    for (int i = 0; i < kMonsterCount; ++i)
        if (kMonsters[i].type == sc.monster) st.monster = i;
    st.costume = sc.costume;
    for (Tint &t : st.tint) t = Tint{sc.hue, sc.sat, sc.bright};
    std::string name = sc.id;
    for (char &c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
    saveAs(st, sc.id, name, true);
}
static void scriptInstall()
{
    scriptSave("WOTM_CREATOR", false);
    scriptSave("WOTM_CREATOR_MONSTER", true);
}

// Window thread only: the panel's 3D monster (like wotm_padsel's SelModel), its untinted textures and the groups the TEXTURE row
// walks. Loaded when the published monster or costume changes, retinted when tintGen changes, freed when the panel is not shown.
struct Preview
{
    std::unique_ptr<wotm::model::Model> model;
    wotm::model::Renderer ren;
    std::vector<wotm::model::TexImage> original;   // decoded textures before any tint
    std::vector<std::vector<int>> groups;          // textures() indices per group (groupsOf)
    std::vector<uint16_t> texIds;                  // each group's texId, for the TEXTURE row's label
    int monster = -1, costume = -1;
    uint32_t tintGen = 0;
    double t0 = 0.0;
    bool timed = false;                            // the first real retint of this load was timed (one log line)
    bool grewLogged = false;                       // a pose decoded textures decodeAllTextures missed (one log line per load)
    bool ok = false;
};
static Preview g_creatorPreview;

// GPU resources (Renderer) and the model freed; needs the GL context (window thread). The next renderPreview loads again.
static void releasePreview()
{
    Preview &p = g_creatorPreview;
    if (p.monster < 0 && !p.model) return;
    p.ren.release();
    p.model.reset();
    p.original.clear();
    p.original.shrink_to_fit();
    p.groups.clear();
    p.texIds.clear();
    p.monster = p.costume = -1;
    p.tintGen = 0;
    p.ok = false;
    g_texCountShared.store(kNoTexCount, std::memory_order_release);
}

// Called from renderPanel with v.creator, before the panel's BeginTextureMode (the Renderer has its own target): loads, tints, poses
// and renders the monster into g_creatorPreview.ren's target (w x h). Returns false when there is nothing to draw.
static bool renderPreview(const State &s, int w, int h)
{
    Preview &p = g_creatorPreview;
    const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    const wotm::mods::MonsterInfo &mi = kMonsters[std::clamp(s.monster, 0, kMonsterCount - 1)];
    if (p.monster != s.monster || p.costume != s.costume)
    {
        releasePreview();
        p.monster = s.monster;
        p.costume = s.costume;
        p.t0 = GetTime();
        p.timed = false;
        p.grewLogged = false;
        p.model = std::make_unique<wotm::model::Model>();
        const std::string file = std::string(mi.prefix) + std::to_string(s.costume);
        p.ok = p.model->load(io.cdRoot, io.cdImage.string(), file);
        if (p.ok)
        {
            // originals and groups from load + decodeAllTextures only, before any pose (computeFraming poses too): exactly what the
            // save's Model computes on the EE thread, in the same order
            p.model->decodeAllTextures();
            p.original = p.model->textures();
            p.groups = groupsOf(*p.model);
            for (const std::vector<int> &g : p.groups) p.texIds.push_back(p.model->texInfo()[size_t(g[0])].id);
            p.model->computeFraming(wotm::model::kIdle);   // fixed box: the camera does not follow the idle animation
        }
        else
        {
            std::fprintf(stderr, "[creator] MON/%s.NGP not found; no preview\n", file.c_str());
            p.model.reset();
        }
        g_texCountShared.store(packTexCount(s.monster, s.costume, int(p.groups.size())), std::memory_order_release);
    }
    if (!p.ok) return false;
    if (p.tintGen != s.tintGen)
    {
        // from the current list: textures a pose decoded after decodeAllTextures (in no group) stay untinted, and setTextures gets
        // a vector of the current size
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<wotm::model::TexImage> t = p.model->textures();
        bool any = false;
        for (size_t g = 0; g < p.groups.size(); ++g)
        {
            const Tint &k = s.tint[g];
            any |= !k.zero();
            for (int i : p.groups[g])
            {
                t[size_t(i)].rgba = p.original[size_t(i)].rgba;
                wotm::tint::shift(t[size_t(i)].rgba, k.hue, k.sat, k.bright);
            }
        }
        p.model->setTextures(std::move(t));
        p.tintGen = s.tintGen;
        if (any && !p.timed)
        {
            p.timed = true;
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::fprintf(stderr, "[creator] tint %.2f ms (%s, %zu textures)\n", ms, p.model->name().c_str(), p.original.size());
        }
    }
    const float frame = float((GetTime() - p.t0) * 60.0);
    if (!p.model->pose(wotm::model::kIdle, frame)) p.model->pose(-1, 0.f);
    if (!p.grewLogged && p.model->textures().size() > p.original.size())
    {   // a gap in decodeAllTextures' walk: these stay untinted and the save would not write them
        p.grewLogged = true;
        std::fprintf(stderr, "[creator] %s: a pose decoded %zu textures decodeAllTextures missed\n", p.model->name().c_str(),
                     p.model->textures().size() - p.original.size());
    }
    using D = wotm::savepics::Drawer;
    const D::Framing fr = D::framingOf(uint32_t(mi.type) | uint32_t(s.costume & 3) << 8);   // the disc costume's framing
    p.ren.shiftX = fr.shiftX;
    p.ren.shiftY = fr.shiftY;
    p.ren.fwd = fr.fwd;
    if (!wotm_padsel::g_selFxTried)   // the selection's effect textures (Agamo's fire), shared: loaded once, by whichever needs them first
    {
        wotm_padsel::g_selFxTried = true;
        wotm_padsel::g_selFx.load(io.cdRoot, io.cdImage.string());
    }
    p.ren.setFx(&wotm_padsel::g_selFx);
    p.ren.render(*p.model, w, h, 0.f, 8.f, fr.zoom * 0.95f, [] {});
    return true;
}
// Window thread: the TEXTURE row's value. LOADING only while the preview has not loaded s's monster and costume (or the EE thread
// has not seen its count yet); NOT FOUND when the costume's file did not load; NONE when it has no texture to tint.
static std::string texLabel(const State &s)
{
    const Preview &p = g_creatorPreview;
    if (p.monster != s.monster || p.costume != s.costume) return "LOADING";
    if (!p.ok) return "NOT FOUND";
    if (p.groups.empty()) return "NONE";
    if (s.texCount <= 0 || s.tex >= int(p.texIds.size())) return "LOADING";
    return "T" + std::to_string(p.texIds[size_t(s.tex)]) + " " + std::to_string(s.tex + 1) + " OF " + std::to_string(s.texCount);
}
} // namespace wotm_creator
