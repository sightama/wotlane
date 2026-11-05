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
    int texCount = 0;           // how many groups the current costume has (0 while the window thread loads it; capped at kMaxTex)
    Tint tint[kMaxTex];
    uint32_t tintGen = 1;       // bumped on every slider change
    char name[cr::kNameLen + 1] = {};
    int cursor = 0;
    bool editing = false;
    int row = RMonster;
    int lastRow = RMonster;     // the row at the last leaveSaveCheck (moving off SAVE cancels OVERWRITE?)
    char saveValue[24] = {};    // "", "SAVED", "OVERWRITE?", "NAME IN USE", "MODS ARE OFF", "CANNOT WRITE", "NOT FOUND", kSavedRestart
    double saveUntil = 0.0;     // GetTime() until which saveValue shows (0: until the next change)
    bool confirmOverwrite = false;
};
static_assert(std::is_trivially_copyable_v<State>, "State is copied into View every frame under g_m");
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
        const int n = texCountFor(s);
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
        save(s, false);   // the files, the registry, OVERWRITE? and the errors
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

// "t0004.png" or "rtx_t0011.png" (any case): the names a save writes (cr::pngName)
static bool isSavedPngName(std::string n)
{
    for (char &c : n) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (n.rfind("rtx_", 0) == 0) n.erase(0, 4);
    if (n.size() < 6 || n[0] != 't' || n.compare(n.size() - 4, 4, ".png") != 0) return false;
    const std::string digits = n.substr(1, n.size() - 5);
    return !digits.empty() && digits.find_first_not_of("0123456789") == std::string::npos;
}

// Section 3 of the spec: writes mods/Creator/skins/<id>/ (one PNG per tinted texture group, skin.json) and registers it
// (Registry::addSkin, texmods::Index::replaceSkin). Uses only its arguments and the registries, no window state: WOTM_CREATOR calls it
// at boot, before the window exists (script: every group is written, no OVERWRITE? question, no timed message).
static void saveAs(State &s, const std::string &id, const std::string &name, bool script)
{
    namespace fs = std::filesystem;
    const auto t0 = std::chrono::steady_clock::now();
    const wotm::mods::MonsterInfo &mi = kMonsters[std::clamp(s.monster, 0, kMonsterCount - 1)];
    auto fail = [&s](const char *msg) {
        setSave(s, msg);
        s.saveUntil = 0.0;
    };
    const fs::path mods = wotm::mods::Registry::get().dir();
    if (mods.empty())
    {
        fail("MODS ARE OFF");
        std::fprintf(stderr, "[creator] mods are off (WOTM_MODS=0): %s not saved\n", id.c_str());
        return;
    }
    const fs::path dir = mods / cr::kMod / "skins" / id;
    std::error_code ec;
    const bool exists = fs::exists(dir, ec);
    if (exists)
    {
        // a skin folder holds one monster's skin; a missing or unreadable skin.json is not "in use" (the overwrite question applies)
        std::ifstream in(dir / "skin.json", std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        wotm::json::Value v;
        const wotm::json::Value *mon = nullptr;
        if (in.is_open() && wotm::json::parse(text, v) && (mon = v.get("monster")) && mon->type == wotm::json::Value::Str)
        {
            std::string other = mon->s;
            for (char &c : other) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (other != mi.name)
            {
                fail("NAME IN USE");
                std::fprintf(stderr, "[creator] %s is a %s skin; pick another name\n", id.c_str(), other.c_str());
                return;
            }
        }
        if (!s.confirmOverwrite && !script)
        {
            fail("OVERWRITE?");
            s.confirmOverwrite = true;
            std::fprintf(stderr, "[creator] mods/%s/skins/%s exists: X again overwrites\n", cr::kMod, id.c_str());
            return;
        }
    }
    s.confirmOverwrite = false;
    // The EE thread's own Model of the disc costume, decoded the way the preview's is (load + decodeAllTextures, same order)
    const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    const std::string file = std::string(mi.prefix) + std::to_string(s.costume);
    wotm::model::Model m;
    if (!m.load(io.cdRoot, io.cdImage.string(), file))
    {
        fail("NOT FOUND");
        std::fprintf(stderr, "[creator] MON/%s.NGP not found: %s not saved\n", file.c_str(), id.c_str());
        return;
    }
    m.decodeAllTextures();
    const std::vector<std::vector<int>> groups = groupsOf(m);
    const std::vector<wotm::model::TexImage> &texs = m.textures();
    const std::vector<wotm::model::TexInfo> &info = m.texInfo();
    fs::create_directories(dir, ec);
    if (ec)
    {
        fail("CANNOT WRITE");
        std::fprintf(stderr, "[creator] cannot write %s: %s\n", dir.string().c_str(), ec.message().c_str());
        return;
    }
    std::set<std::string> written;   // lowercase file names
    size_t halved = 0;
    for (size_t g = 0; g < groups.size(); ++g)
    {
        const Tint &k = s.tint[g];
        if (!script && k.zero()) continue;   // the base costume's own texture serves (the registry aliases it)
        // one PNG per texId, whatever the palette: the group's first texture decides (as in the preview's TEXTURE row)
        const size_t i = size_t(groups[g][0]);
        std::vector<uint32_t> px = texs[i].rgba;
        wotm::tint::shift(px, k.hue, k.sat, k.bright);
        // hiAlpha: loadPng reads the PNG raw (model path: min(255, 2a) when TEX0.TCC is set; the fight: the GS's alpha), so the
        // decoded 0..255 goes back to the raw scale (cr::halveAlpha: 255 stays 0xFF). With TCC clear loadPng ignores the PNG's alpha
        // (always opaque), so halving it is harmless there; TexInfo carries no TCC because the same rawAmax > 0x80 test (on the
        // original's raw alpha, whatever the TCC) decides hiAlpha in Model::decodeTex and in Scene::modTexture.
        if (info[i].hiAlpha)
        {
            cr::halveAlpha(px);
            ++halved;
        }
        const std::string png = cr::pngName(info[i].id, info[i].rtx);
        Image img{px.data(), texs[i].w, texs[i].h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        if (!ExportImage(img, (dir / png).string().c_str()))
        {
            fail("CANNOT WRITE");
            std::fprintf(stderr, "[creator] cannot write %s: ExportImage failed\n", (dir / png).string().c_str());
            return;
        }
        written.insert(png);
    }
    // PNGs this save did not write (zero-tint groups of an earlier save, another costume's texIds) would retexture this costume
    std::vector<fs::path> stale;   // collected first: no removal while the directory is being walked
    for (const fs::directory_entry &e : fs::directory_iterator(dir, ec))
    {
        std::string n = e.path().filename().string();
        if (!e.is_regular_file(ec) || !isSavedPngName(n)) continue;
        for (char &c : n) c = char(std::tolower(static_cast<unsigned char>(c)));
        if (!written.count(n)) stale.push_back(e.path());
    }
    for (const fs::path &p : stale)
    {
        std::error_code rm;
        fs::remove(p, rm);
        std::fprintf(stderr, "[creator] removed %s (not in this save)%s\n", p.filename().string().c_str(), rm ? " FAILED" : "");
    }
    {
        std::ofstream o(dir / "skin.json", std::ios::binary | std::ios::trunc);
        o << cr::skinJson(mi.name, id, name, s.costume);
        o.close();
        if (!o)
        {
            fail("CANNOT WRITE");
            std::fprintf(stderr, "[creator] cannot write %s\n", (dir / "skin.json").string().c_str());
            return;
        }
    }
    std::string why;
    if (const wotm::mods::Skin *k = wotm::mods::Registry::get().addSkin(dir, why))
    {
        const std::string stem = k->prefix + std::to_string(k->index);
        std::vector<wotm::mods::SkinPng> pngs;
        for (wotm::mods::SkinPng &p : wotm::mods::Registry::get().skinPngs())
            if (p.stem == stem) pngs.push_back(std::move(p));
        wotm::texmods::Index::get().replaceSkin(stem, pngs);
        setSave(s, "SAVED");
    }
    else
    {
        setSave(s, kSavedRestart);
        std::fprintf(stderr, "[creator] not registered: %s\n", why.c_str());
    }
    s.saveUntil = script ? 0.0 : GetTime() + 3.0;   // GetTime needs the window: the script runs before it exists
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "[creator] saved mods/%s/skins/%s (%zu png) in %.0f ms%s\n", cr::kMod, id.c_str(), written.size(), ms,
                 halved ? "; alpha on the raw scale for the textures above 0x80" : "");
}
// X on SAVE: the id from the typed name (trimmed), shown as typed
static void save(State &s, bool script)
{
    std::string name = s.name;
    while (!name.empty() && name.back() == ' ') name.pop_back();
    name.erase(0, std::min(name.find_first_not_of(' '), name.size()));
    saveAs(s, cr::idFromName(name), name, script);
}
// WOTM_CREATOR=<monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]] at boot (wotmCreatorInstall, after wotmModsInstall): the same
// save with the values on every texture and the name = the id in capitals; the game then boots as usual
static void scriptInstall()
{
    const char *env = std::getenv("WOTM_CREATOR");
    if (!env || !*env) return;
    cr::Script sc;
    std::string err;
    if (!cr::parseScript(env, sc, err))
    {
        std::fprintf(stderr, "[creator] WOTM_CREATOR ignored: %s\n", err.c_str());
        return;
    }
    State st;
    for (int i = 0; i < kMonsterCount; ++i)
        if (kMonsters[i].type == sc.monster) st.monster = i;
    st.costume = sc.costume;
    for (Tint &t : st.tint) t = Tint{sc.hue, sc.sat, sc.bright};
    std::string name = sc.id;
    for (char &c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
    saveAs(st, sc.id, name, true);
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
