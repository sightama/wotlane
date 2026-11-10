// ABOUTME: Map creator, runtime side: hashes the stage image when Shell::LoadLevelDB inflates it and applies the active map's
// ABOUTME: edits (native/wotm_stagemap.cpp) on entry of dbInitDb, before the game copies positions out of it. Included by wotm_overrides.cpp.
//
// Spec: docs/superpowers/specs/2026-11-09-map-creator-design.md ("Loading a map", "PR 1 checks" 1a-1d). EE thread only.
// - LoadLevelDB (0x1A9AB0) inflates lvl/<STAGE>.NGP to 0xA00000 through zipInflateAll, whose return value is the exact image size
//   (1a: the game keeps only the end rounded up); a tap on zipInflateAll takes it while a LoadLevelDB call is in progress. On its
//   completion the image is hashed (FNV-1a 64, as stage_map.py's baseHash; only with a map active) and g_pending is set.
// - A recompiled function can return midway (thread switch, e.g. waiting for the CD) and be resumed later by the dispatcher at
//   the table slot of the resume address, which a hook on the entry alone never sees (wotm_split4.inc, "The resume doesn't go
//   through this hook"). So the LoadLevelDB and zipInflateAll hooks are registered at every slot of their function (wrapAll), keep
//   the call's $ra from the entry and treat "the original came back with pc == that $ra" as the completion, whichever call of
//   the hook it happens in. The result does not depend on where the time slice ran out.
// - dbInitDb (0x12E418) is also called for the shell, the pre-shell and the load screen, all with 0xA00000 (1b): only the call
//   after a LoadLevelDB (g_pending) applies, once per load (1c: REBUILD LEVEL loads again and applies again; PLAY AGAIN and RESTART
//   don't run dbInitDb, the edited image stays).
// - The active map is set where a stage is chosen (setActive: the stage selection's confirm on a map slot, wotm_levelsel.inc; WOTM_MATCH's
//   map id in userintMainDirect) and cleared each time the menus load (Shell::LoadUserintDB entry) and on a confirm of any other
//   stage, so no path that starts a fight without that confirm (adventure, training, minigames) inherits an earlier map. It stays
//   set for the whole session (REBUILD LEVEL reloads without the menus); a map on another base stage applies nothing. The edits take grid list nodes and ROTATEs from the reserve (kReserve, 4 MB under
//   the top-of-RAM slots, 1d), cleared before each apply; if the loaded files reach it (or fileStatus can't say), nothing is applied.
// - Power-up places (the map's {"powerup": i, "from", "at"} edits) are not in the image: TheGame::InitAfterDbLoad, after dbInitDb,
//   calls PowerUpTool::loadPoints, which copies PTS/<STAGE>.PUP into PowerUpTool::instance's points and calls the virtual
//   exportPoints (0x1881D0), which kills the power-ups and creates one per point. A hook on exportPoints (wrapAll, acting on its
//   entry only) rewrites the points' positions first (sm::applyPowerUps), when the map's piece edits were applied to this load
//   (g_pups, set by applyActive, cleared by the next LoadLevelDB) and the map is still the active one. Game mode 8 (Big Shot) is
//   skipped, as InitAfterDbLoad skips loadPoints there. Writes are absolute: another exportPoints on the same points changes nothing.
// - WOTM_STAGE_DUMP=<path>: after apply, the image goes to <path> and the reserve to <path>.reserve (tools/stage_map.py check).
// wotm_overrides.cpp is built with strict float rules, but keep float math out of here anyway: it belongs in wotm_stagemap.cpp.
#include <fstream>

namespace wotm_stagemap
{
namespace sm = wotm::stagemap;

constexpr uint32_t kLoadLevelDB = wotm::fn::LoadLevelDB__5Shell, kZipInflateAll = wotm::fn::zipInflateAll__FPcPv,
                   kDbInitDb = wotm::fn::dbInitDb__FP9_dbheader10_vramAddrs;
constexpr uint32_t kFileStatusAddr = wotm::sym::fileStatus, kWhichLevel = wotm::sym::whichLevel;
constexpr uint32_t kLevelNumAddr = wotm::sym::shellInfo + 0x2BB0;   // Shell::m_levelNum
// PowerUpTool::exportPoints (this in $a0: PowerUpTool::instance); the instance's PointToolKit::points[256] (64 bytes each, the
// position first) is at +0 and numPoints at +0x4000 (getPoint(i) is this + i * 64; exportPoints reads numPoints with lw 0x4000($s0))
constexpr uint32_t kExportPoints = wotm::fn::exportPoints__11PowerUpTool, kPowerUpTool = wotm::sym::x_11PowerUpTool_instance,
                   kNumPointsOff = 0x4000;
constexpr uint32_t kGameModeAddr = wotm::sym::shellGame + 0x1203C8;   // TheGame::m_gameMode

PS2Runtime::RecompiledFunction g_origLoadLevelDB = nullptr, g_origZip = nullptr, g_origDbInitDb = nullptr, g_origLoadUserintDB = nullptr,
                               g_origExportPoints = nullptr;

std::string g_active;          // map id (Registry::stages()), or empty: no map
bool g_hashed = false;         // g_hash is the hash of the last LoadLevelDB's image (taken only with a map active)
uint64_t g_hash = 0;
uint32_t g_size = 0;           // that image's exact size (zipInflateAll's return)
std::string g_stage;           // its stage file name, upper case ("CENTRAL"), from whichLevel
bool g_pending = false;        // set when LoadLevelDB completes, cleared at the next dbInitDb
uint32_t g_loadRa = 0;         // $ra of the LoadLevelDB call in progress (0: none); the zipInflateAll tap is live while set
uint32_t g_zipRa = 0;          // $ra of the zipInflateAll call that inflates the stage (0: none)
uint32_t g_zipSize = 0;        // what it returned (0: not yet)
std::string g_pupMap;          // the map whose edits were applied to the loaded stage (empty: none), for its power-up edits
std::vector<sm::PowerUpEdit> g_pups;
bool g_pupLogged = false;      // the power-up line was logged for this load

void setActive(const std::string &id)
{
    if (id != g_active)
        std::fprintf(stderr, "[stages] active map: %s\n", id.empty() ? "none" : id.c_str());
    g_active = id;
}
const std::string &active() { return g_active; }

std::string upper(std::string s)
{
    for (char &c : s)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string lower(std::string s)
{
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// GameLevelNames (char[28][10] at 0x6EF748, the stage's file name by Shell::m_levelNum; wotm_save.inc has the same list). Index
// for a map's base (case-insensitive), -1 for a name with no stage file.
int levelIndex(const std::string &base)
{
    static const char *const kFiles[28] = {"default", "central", "vegas", "canyon2", "metro", "airport", "3-mile", "sanfran", "island",
                                           "tokyo", "ufo", "capitol", "training", "canyon1", "minisf", "volcano", "tokyodm", nullptr,
                                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "dball", "bigshot", "crush"};
    const std::string b = lower(base);
    for (int i = 1; i < 28; ++i)
        if (kFiles[i] && b == kFiles[i])
            return i;
    return -1;
}

uint32_t rd32(const uint8_t *rdram, uint32_t a) { uint32_t v; std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4); return v; }

// Highest address the stage load reached (fileStatus: next load address of the TEX, NGP and RTX lists), 0 when unreadable.
// Also what the end-of-load log prints (wotmCheckLoadEnd).
uint32_t loadEnd(const uint8_t *rdram)
{
    const uint32_t texN = rdram[kFileStatusAddr + 0x38], ngpN = rdram[kFileStatusAddr + 0x74], resN = rdram[kFileStatusAddr + 0xB0];
    if (texN > 13 || ngpN > 13 || resN > 13)
        return 0;
    return std::max({rd32(rdram, kFileStatusAddr + 4 * texN), rd32(rdram, kFileStatusAddr + 0x3C + 4 * ngpN),
                     rd32(rdram, kFileStatusAddr + 0x78 + 4 * resN)});
}

// int zipInflateAll(char *path, void *dest): the inflated size
void zipTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kZipInflateAll && g_loadRa && !g_zipRa && !g_zipSize && GPR_U32(ctx, 5) == sm::kBase)
        g_zipRa = GPR_U32(ctx, 31);
    g_origZip(rdram, ctx, runtime);
    if (g_zipRa && ctx->pc == g_zipRa)
    {
        g_zipSize = GPR_U32(ctx, 2);
        g_zipRa = 0;
    }
}

void loadLevelDBTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kLoadLevelDB)
    {
        g_loadRa = GPR_U32(ctx, 31);
        g_zipRa = 0;
        g_zipSize = 0;
        g_pending = false;
        g_hashed = false;
        g_pupMap.clear();
        g_pups.clear();
    }
    g_origLoadLevelDB(rdram, ctx, runtime);
    if (!g_loadRa || ctx->pc != g_loadRa)
        return;   // returned midway: the resume comes through here again (wrapAll) and finishes
    g_loadRa = 0;
    // ngpAddr[1]: the image end rounded up plus 0x80 (fileOnlyNgpFile), only a sanity bound for the size
    const uint32_t bound = rd32(rdram, kFileStatusAddr + 0x3C + 4);
    if (!g_zipSize || g_zipSize > sm::kMaxNode || bound < sm::kBase || g_zipSize > bound - sm::kBase)
    {
        std::fprintf(stderr, "[stages] stage image size not taken (zipInflateAll returned %u, image end 0x%X): no map applies to this load%s%s\n",
                     g_zipSize, bound, g_active.empty() ? "" : "; active map ", g_active.c_str());
        return;
    }
    g_size = g_zipSize;
    const uint32_t level = rd32(rdram, kLevelNumAddr);
    g_stage.clear();
    if (level < 0x1D)   // LoadLevelDB copied the stage's name to whichLevel (or the command line put it there); 0x1D+: host0:monster.ngp
    {
        const char *w = reinterpret_cast<const char *>(rdram + kWhichLevel);
        g_stage = upper(std::string(w, strnlen(w, 15)));
    }
    g_pending = true;
    if (!g_active.empty())   // the map is chosen before the load (WOTM_MATCH, the stage selection's confirm)
    {
        g_hash = sm::fnv1a(rdram + sm::kBase, g_size);
        g_hashed = true;
        std::fprintf(stderr, "[stages] image %s %u bytes, hash %016llx\n", g_stage.c_str(), g_size, static_cast<unsigned long long>(g_hash));
    }
}

void dump(const uint8_t *rdram)
{
    const char *path = std::getenv("WOTM_STAGE_DUMP");
    if (!path || !*path)
        return;
    std::ofstream img(path, std::ios::binary), res(std::string(path) + ".reserve", std::ios::binary);
    img.write(reinterpret_cast<const char *>(rdram + sm::kBase), std::streamsize(g_size));
    res.write(reinterpret_cast<const char *>(rdram + sm::kReserve), std::streamsize(sm::kReserveSize));
    std::fprintf(stderr, img && res ? "[stages] dump %s\n" : "[stages] dump %s failed\n", path);
}

void applyActive(uint8_t *rdram)
{
    const std::string id = g_active;
    if (!g_hashed)
    {
        std::fprintf(stderr, "[stages] %s: the map became active after the stage loaded, nothing applied\n", id.c_str());
        return;
    }
    const std::vector<wotm::mods::Stage> all = wotm::mods::Registry::get().stages();
    const wotm::mods::Stage *st = nullptr;
    for (const wotm::mods::Stage &s : all)
        if (s.map.id == id)
            st = &s;
    if (!st)
    {
        std::fprintf(stderr, "[stages] %s: map not found in mods/, nothing applied\n", id.c_str());
        return;
    }
    const sm::Map &map = st->map;
    if (upper(map.base) != g_stage)
    {
        std::fprintf(stderr, "[stages] %s: base %s is not the loaded stage (%s), nothing applied\n", id.c_str(), upper(map.base).c_str(),
                     g_stage.empty() ? "none" : g_stage.c_str());
        return;
    }
    if (map.baseHash != g_hash)
    {
        std::fprintf(stderr, "[stages] %s: base %s does not match this disc\n", id.c_str(), upper(map.base).c_str());
        return;
    }
    if (const uint32_t end = loadEnd(rdram); !end || end > sm::kReserve)
    {
        std::fprintf(stderr, "[stages] %s: %s (end 0x%07X, reserve 0x%07X), nothing applied\n", id.c_str(),
                     end ? "the loaded files reach the map reserve" : "fileStatus does not say where the load ends", end, sm::kReserve);
        return;
    }
    std::memset(rdram + sm::kReserve, 0, sm::kReserveSize);
    sm::Mem mem;
    mem.r.push_back({sm::kBase, rdram + sm::kBase, g_size});
    mem.r.push_back({sm::kReserve, rdram + sm::kReserve, sm::kReserveSize});
    sm::Reserve res{sm::kReserve, sm::kReserve + sm::kReserveSize};
    const sm::Result r = sm::apply(mem, g_size, map, res);
    std::fprintf(stderr, "[stages] %s: applied %d edits, %d grid nodes relinked (%d from reserve)\n", id.c_str(), r.applied, r.relinked,
                 r.fromReserve);
    for (const std::string &l : r.log)
        std::fprintf(stderr, "[stages] %s: %s\n", id.c_str(), l.c_str());
    dump(rdram);
    g_pupMap = id;
    g_pups = map.powerups;
    g_pupLogged = false;
}

// PowerUpTool::exportPoints(void). Acted on at its entry only (a resumed call comes back with pc inside it)
void exportPointsTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kExportPoints && !g_pups.empty() && !g_pupMap.empty() && g_pupMap == g_active &&
        static_cast<int32_t>(rd32(rdram, kGameModeAddr)) != 8)
    {
        const uint32_t self = GPR_U32(ctx, 4) & PS2_RAM_MASK;
        if (self != kPowerUpTool)
        {
            if (!g_pupLogged)
                std::fprintf(stderr, "[stages] %s: exportPoints on 0x%X, not PowerUpTool::instance: power-ups not moved\n", g_pupMap.c_str(), self);
            g_pupLogged = true;
        }
        else
        {
            const uint32_t n = rd32(rdram, kPowerUpTool + kNumPointsOff);
            sm::Mem mem;
            mem.r.push_back({kPowerUpTool, rdram + kPowerUpTool, sm::kMaxPoints * sm::kPointSize});
            const sm::PowerUpResult r = sm::applyPowerUps(mem, kPowerUpTool, n, g_pups);
            if (!g_pupLogged)
            {
                std::fprintf(stderr, "[stages] %s: moved %d power-ups\n", g_pupMap.c_str(), r.moved);
                for (const std::string &l : r.log)
                    std::fprintf(stderr, "[stages] %s: %s\n", g_pupMap.c_str(), l.c_str());
            }
            g_pupLogged = true;
        }
    }
    g_origExportPoints(rdram, ctx, runtime);
}

// dbInitDb(_dbheader *db, vramAddrs *v). Acted on at its entry only (a resumed call comes back with pc inside it)
void dbInitDbTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kDbInitDb && g_pending)
    {
        g_pending = false;
        // the stage's dbInitDb (LoadLevelFiles): the image at 0xA00000 is NGP file 0, and the monsters' NGPs follow it. LoadMonstersDB
        // runs before dbInitDb (Shell.cpp, LoadLevelFiles) and adds at least one monster NGP, so a stage load has 2 or more NGP files;
        // the shell, the pre-shell and the load screen (fileOnlyNgpFile) have exactly one. A load with no monster (PR 2's editor) must
        // revisit this test.
        const uint32_t ngpN = rdram[kFileStatusAddr + 0x74];
        const bool stage = GPR_U32(ctx, 4) == sm::kBase && ngpN >= 2 && ngpN <= 13 && rd32(rdram, kFileStatusAddr + 0x3C) == sm::kBase;
        if (!stage && !g_active.empty())
            std::fprintf(stderr, "[stages] %s: the dbInitDb after the stage load is not the stage's (db 0x%X, %u NGP files), nothing applied\n",
                         g_active.c_str(), GPR_U32(ctx, 4), ngpN);
        else if (stage && !g_active.empty())
            applyActive(rdram);
    }
    g_origDbInitDb(rdram, ctx, runtime);
}

// Shell::LoadUserintDB (shell.ngp: the menus load, Shell.cpp's main loop, before userintMain): no map until a stage is chosen again
void loadUserintDBTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::LoadUserintDB__5Shell)
        setActive("");
    g_origLoadUserintDB(rdram, ctx, runtime);
}

// Registers `hook` at the function's entry and at every resume point inside it (the function table has one slot per address a
// thread can resume at, all pointing at the same recompiled function, which picks up from ctx->pc). A hook on the entry alone
// misses the completion of a call that returned midway; with every slot wrapped, the resume comes through the hook too.
// Returns how many slots were wrapped (0: the function is missing).
int wrapAll(PS2Runtime &runtime, uint32_t entry, uint32_t size, PS2Runtime::RecompiledFunction hook, PS2Runtime::RecompiledFunction &orig)
{
    if (!runtime.hasFunction(entry) || !(orig = runtime.lookupFunction(entry)))
        return 0;
    int n = 0;
    for (uint32_t a = entry; a < entry + size; a += 4)
        if (runtime.hasFunction(a) && runtime.lookupFunction(a) == orig)
        {
            runtime.registerFunction(a, hook);
            ++n;
        }
    return n;
}

void install(PS2Runtime &runtime)
{
    // sizes from native/wotm_syms.h (LoadLevelDB__5Shell 0x140, zipInflateAll__FPcPv 0x108)
    const int nLoad = wrapAll(runtime, kLoadLevelDB, 0x140, &loadLevelDBTapped, g_origLoadLevelDB);
    const int nZip = nLoad ? wrapAll(runtime, kZipInflateAll, 0x108, &zipTapped, g_origZip) : 0;
    if (nLoad && nZip && runtime.hasFunction(kDbInitDb) && (g_origDbInitDb = runtime.lookupFunction(kDbInitDb)) &&
        runtime.hasFunction(wotm::fn::LoadUserintDB__5Shell) && (g_origLoadUserintDB = runtime.lookupFunction(wotm::fn::LoadUserintDB__5Shell)))
    {
        runtime.registerFunction(kDbInitDb, &dbInitDbTapped);
        runtime.registerFunction(wotm::fn::LoadUserintDB__5Shell, &loadUserintDBTapped);
        // size from native/wotm_syms.h (exportPoints__11PowerUpTool 0x80)
        const int nPup = wrapAll(runtime, kExportPoints, 0x80, &exportPointsTapped, g_origExportPoints);
        std::fprintf(stderr, "[stages] map hooks: LoadLevelDB (%d entry and resume points), zipInflateAll (%d), dbInitDb, exportPoints (%d)%s\n",
                     nLoad, nZip, nPup, nPup ? "" : ": power-up places stay where the disc puts them");
    }
    else
        std::fprintf(stderr, "[stages] maps unavailable: LoadLevelDB, zipInflateAll, dbInitDb or LoadUserintDB has no function in the runtime\n");
}
} // namespace wotm_stagemap
