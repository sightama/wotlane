// ABOUTME: Map editor, EE thread side: a session over the stock stage on screen (WOTM_MAPEDIT=<map id>), the commands the window sends,
// ABOUTME: the live preview (pieces drawn where the session puts them) and the save to mods/Creator/stages/<id>/stage.json. Included by wotm_overrides.cpp.
//
// Spec: docs/superpowers/specs/2026-11-09-map-creator-design.md, "Editor". The editor is offline only (the preview writes guest
// RAM): it is never on during a network match. Its session starts from the stock stage of a direct match (WOTM_MATCH) with no
// map active; the edits it saves apply at the next load of the map, through wotm_stagemap.inc, like any map. The preview moves
// a piece's drawing (its TRANSLATE's translation or its ROTATE's translation row) and hides a removed one far below the stage;
// collision, turns and copies only exist once the map is loaded to play. WOTM_MAPEDIT_TEST=<commands> runs mapedit::parseScript's
// commands once the session exists (checks).

#include <array>

WotmMapEdit g_wotmMapEdit;

namespace wotm_mapedit_rt
{
namespace me = wotm::mapedit;
namespace sm = wotm::stagemap;

constexpr float kHiddenZ = -100000.f;   // a removed piece's preview place: far below anything the camera shows

static std::unique_ptr<me::Session> g_ses;
static uint32_t g_forLoad = 0;   // wotm_stagemap::g_loads the session was built for
static std::vector<std::array<float, 3>> g_written;   // what the preview last wrote for each stock piece
static bool g_scriptRun = false;

// WOTM_MAPEDIT=<map id> (a letter, then a-z, 0-9 and _, up to 16): enables the editor for the session's whole run
void init()
{
    const char *e = std::getenv("WOTM_MAPEDIT");
    if (!e || !*e)
        return;
    const std::string id = e;
    bool ok = id.size() <= 16 && id[0] >= 'a' && id[0] <= 'z';
    for (char c : id)
        ok = ok && ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_');
    if (!ok)
    {
        std::fprintf(stderr, "[mapedit] WOTM_MAPEDIT ignored: \"%s\" is not a map id (a letter, then a-z, 0-9 and _, up to 16)\n", e);
        return;
    }
    std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
    g_wotmMapEdit.enabled = true;
    g_wotmMapEdit.id = id;
    std::fprintf(stderr, "[mapedit] map editor on for \"%s\": F10 in a fight opens it\n", id.c_str());
}

// stage.json under the registry's mods folder, written to a temporary name and renamed over the old one
static std::string save(const me::Session &s, const std::string &id)
{
    namespace fs = std::filesystem;
    const fs::path mods = wotm::mods::Registry::get().dir();
    if (mods.empty())
        return "MODS ARE OFF";
    std::string name = id;
    for (char &c : name)
        c = c == '_' ? ' ' : char(std::toupper(static_cast<unsigned char>(c)));
    const sm::Map map = s.toMap(id, name.substr(0, size_t(sm::kNameLen)));
    const fs::path dir = mods / "Creator" / "stages" / id, file = dir / "stage.json", tmp = dir / "stage.json.tmp";
    std::error_code ec;
    fs::create_directories(dir, ec);
    {
        std::ofstream out(tmp, std::ios::binary);
        out << sm::write(map);
        if (!out)
        {
            std::fprintf(stderr, "[mapedit] cannot write %s\n", tmp.string().c_str());
            return "CANNOT WRITE";
        }
    }
    fs::rename(tmp, file, ec);
    if (ec)
    {
        std::fprintf(stderr, "[mapedit] cannot write %s: %s\n", file.string().c_str(), ec.message().c_str());
        return "CANNOT WRITE";
    }
    std::fprintf(stderr, "[mapedit] saved mods/Creator/stages/%s (%zu edits, %zu copies)\n", id.c_str(), map.edits.size(), map.copies.size());
    return "SAVED MODS/CREATOR/STAGES/" + name;
}

// The piece's place in guest RAM: a TRANSLATE's translation (+0x10), a ROTATE's translation row (+0x40)
static void writePlace(uint8_t *rdram, uint32_t node, const float pos[3])
{
    uint32_t op;
    std::memcpy(&op, rdram + node, 4);
    const uint32_t at = (op & 63) == sm::kOpRotate ? node + 0x40 : node + 0x10;
    for (uint32_t k = 0; k < 3; ++k)
        std::memcpy(rdram + at + 4 * k, &pos[k], 4);
}

// Every fight frame, slot 0 (the Monster::update hook): builds the session once per load, runs the commands, writes the preview
void tick(uint8_t *rdram)
{
    if (!g_wotmMapEdit.enabled || wotm_netstatus::g_active)
        return;
    if (g_forLoad != wotm_stagemap::g_loads)
    {
        g_forLoad = wotm_stagemap::g_loads;
        g_ses.reset();
        g_written.clear();
        g_scriptRun = false;
        if (!wotm_stagemap::g_active.empty())
            std::fprintf(stderr, "[mapedit] map %s is loaded: the editor starts from a stock stage (WOTM_MATCH with a stage number)\n",
                         wotm_stagemap::g_active.c_str());
        else if (wotm_stagemap::g_hashed && wotm_stagemap::g_size)
        {
            const sm::Mem m{{{sm::kBase, rdram + sm::kBase, wotm_stagemap::g_size}}};
            g_ses = std::make_unique<me::Session>(wotm_stagemap::g_stage, wotm_stagemap::g_hash, me::pieces(m));
            for (const me::Piece &p : g_ses->stagePieces())
                g_written.push_back({p.pos[0], p.pos[1], p.pos[2]});
            std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
            g_wotmMapEdit.ready = true;   // with `on`, in one step: the window closes an editor that isn't ready
            g_wotmMapEdit.on = true;      // WOTM_MAPEDIT asked for the editor: it opens with the stage (F10 closes and opens it)
            std::fprintf(stderr, "[mapedit] %s: %zu pieces to edit\n", wotm_stagemap::g_stage.c_str(), g_ses->stagePieces().size());
        }
    }
    std::lock_guard<std::mutex> lk(g_wotmMapEdit.m);
    g_wotmMapEdit.ready = g_ses != nullptr;
    if (!g_ses)
        return;
    g_wotmMapEdit.stage = wotm_stagemap::g_stage;
    std::vector<me::Command> cmds = std::move(g_wotmMapEdit.cmds);
    g_wotmMapEdit.cmds.clear();
    if (!g_scriptRun)
    {
        g_scriptRun = true;
        if (const char *t = std::getenv("WOTM_MAPEDIT_TEST"); t && *t)
        {
            std::vector<me::Command> script;
            std::string why;
            if (me::parseScript(t, script, why))
                cmds.insert(cmds.begin(), script.begin(), script.end());
            else
                std::fprintf(stderr, "[mapedit] WOTM_MAPEDIT_TEST ignored: %s\n", why.c_str());
        }
    }
    for (const me::Command &c : cmds)
        if (me::run(*g_ses, c))
            g_wotmMapEdit.msg = save(*g_ses, g_wotmMapEdit.id);
    const std::vector<me::ItemView> items = g_ses->view();
    for (size_t i = 0; i < g_written.size() && i < items.size(); ++i)
    {
        const float place[3] = {items[i].pos[0], items[i].pos[1], items[i].removed ? kHiddenZ : items[i].pos[2]};
        if (place[0] != g_written[i][0] || place[1] != g_written[i][1] || place[2] != g_written[i][2])
        {
            writePlace(rdram, items[i].node, place);
            g_written[i] = {place[0], place[1], place[2]};
        }
    }
    const sm::Map map = g_ses->toMap(g_wotmMapEdit.id, "X");
    g_wotmMapEdit.items = items;
    g_wotmMapEdit.selected = g_ses->selected();
    g_wotmMapEdit.edits = int(map.edits.size());
    g_wotmMapEdit.copies = int(map.copies.size());
}

} // namespace wotm_mapedit_rt
