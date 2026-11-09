// ABOUTME: Map creator, runtime side: hashes the stage image when Shell::LoadLevelDB inflates it and applies the active map's
// ABOUTME: edits (native/wotm_stagemap.cpp) on entry of dbInitDb, before the game copies positions out of it. Included by wotm_overrides.cpp.
//
// Spec: docs/superpowers/specs/2026-11-09-map-creator-design.md ("Loading a map", "PR 1 checks" 1a-1d). EE thread only.
// - LoadLevelDB (0x1A9AB0) inflates lvl/<STAGE>.NGP to 0xA00000 through zipInflateAll, whose return value is the exact image size
//   (1a: the game keeps only the end rounded up); a tap on zipInflateAll takes it while the hook is inside the original. On exit
//   the image is hashed (FNV-1a 64, as stage_map.py's baseHash) and g_pending is set.
// - dbInitDb (0x12E418) is also called for the shell, the pre-shell and the load screen, all with 0xA00000 (1b): only the call
//   after a LoadLevelDB (g_pending) applies, once per load (1c: REBUILD LEVEL loads again and applies again; PLAY AGAIN and RESTART
//   don't run dbInitDb, the edited image stays).
// - The active map (setActive: WOTM_MATCH's map id; the stage selection later) stays set for the whole session; a map on another
//   base stage applies nothing. The edits take grid list nodes and ROTATEs from the reserve (kReserve, 1 MB under the top-of-RAM
//   slots, 1d), cleared before each apply; if the loaded files reach it, nothing is applied.
// - WOTM_STAGE_DUMP=<path>: after apply, the image goes to <path> and the reserve to <path>.reserve (tools/stage_map.py check).
// No float math here (this file is built with fast float rules): everything that computes goes through wotm::stagemap::apply.
#include <fstream>

namespace wotm_stagemap
{
namespace sm = wotm::stagemap;

constexpr uint32_t kLoadLevelDB = wotm::fn::LoadLevelDB__5Shell, kZipInflateAll = wotm::fn::zipInflateAll__FPcPv,
                   kDbInitDb = wotm::fn::dbInitDb__FP9_dbheader10_vramAddrs;
constexpr uint32_t kFileStatusAddr = wotm::sym::fileStatus, kWhichLevel = wotm::sym::whichLevel;
constexpr uint32_t kLevelNumAddr = wotm::sym::shellInfo + 0x2BB0;   // Shell::m_levelNum

PS2Runtime::RecompiledFunction g_origLoadLevelDB = nullptr, g_origZip = nullptr, g_origDbInitDb = nullptr;

std::string g_active;          // map id (Registry::stages()), or empty: no map
uint64_t g_hash = 0;           // of the image of the last LoadLevelDB
uint32_t g_size = 0;           // its exact size (zipInflateAll's return)
std::string g_stage;           // its stage file name, upper case ("CENTRAL"), from whichLevel
bool g_pending = false;        // set at LoadLevelDB's exit, cleared at the next dbInitDb
bool g_inLoad = false;         // inside the original LoadLevelDB (the zipInflateAll tap is live)
uint32_t g_zipSize = 0;        // what the tap saw (0: none)

void setActive(const std::string &id)
{
    if (id != g_active)
        std::fprintf(stderr, "[stages] active map: %s\n", id.empty() ? "none" : id.c_str());
    g_active = id;
}
const std::string &active() { return g_active; }

// GameLevelNames (char[28][10] at 0x6EF748, the stage's file name by Shell::m_levelNum; wotm_save.inc has the same list). Index
// for a map's base (case-insensitive), -1 for a name with no stage file.
int levelIndex(const std::string &base)
{
    static const char *const kFiles[28] = {"default", "central", "vegas", "canyon2", "metro", "airport", "3-mile", "sanfran", "island",
                                           "tokyo", "ufo", "capitol", "training", "canyon1", "minisf", "volcano", "tokyodm", nullptr,
                                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "dball", "bigshot", "crush"};
    std::string b;
    for (char c : base)
        b += char(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 1; i < 28; ++i)
        if (kFiles[i] && b == kFiles[i])
            return i;
    return -1;
}

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

uint32_t rd32(const uint8_t *rdram, uint32_t a) { uint32_t v; std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4); return v; }

// Highest address the stage load reached (fileStatus: next load address of the TEX, NGP and RTX lists), 0 when unreadable.
// The same value the end-of-load log prints (wotmCheckLoadEnd).
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
    const bool mine = g_inLoad && ctx->pc == kZipInflateAll && GPR_U32(ctx, 5) == sm::kBase;
    const uint32_t ra = GPR_U32(ctx, 31);
    g_origZip(rdram, ctx, runtime);
    if (mine && ctx->pc == ra && !g_zipSize)
        g_zipSize = GPR_U32(ctx, 2);
}

void loadLevelDBTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc != kLoadLevelDB)
    {
        g_origLoadLevelDB(rdram, ctx, runtime);
        return;
    }
    const uint32_t ra = GPR_U32(ctx, 31);
    g_pending = false;
    g_zipSize = 0;
    g_inLoad = true;
    g_origLoadLevelDB(rdram, ctx, runtime);
    g_inLoad = false;
    if (ctx->pc != ra)
        return;
    // ngpAddr[1]: the image end rounded up plus 0x80 (fileOnlyNgpFile), only a sanity bound for the size
    const uint32_t bound = rd32(rdram, kFileStatusAddr + 0x3C + 4);
    if (!g_zipSize || g_zipSize > sm::kMaxNode || bound < sm::kBase || g_zipSize > bound - sm::kBase)
    {
        std::fprintf(stderr, "[stages] stage image size not taken (zipInflateAll returned %u, image end 0x%X): no map applies to this load\n",
                     g_zipSize, bound);
        return;
    }
    g_size = g_zipSize;
    g_hash = sm::fnv1a(rdram + sm::kBase, g_size);
    const uint32_t level = rd32(rdram, kLevelNumAddr);
    g_stage.clear();
    if (level < 0x1D)   // LoadLevelDB copied the stage's name to whichLevel (or the command line put it there); 0x1D+: host0:monster.ngp
    {
        const char *w = reinterpret_cast<const char *>(rdram + kWhichLevel);
        g_stage = upper(std::string(w, strnlen(w, 15)));
    }
    g_pending = true;
    if (!g_active.empty())
        std::fprintf(stderr, "[stages] image %s %u bytes, hash %016llx\n", g_stage.c_str(), g_size, static_cast<unsigned long long>(g_hash));
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
    if (const uint32_t end = loadEnd(rdram); end > sm::kReserve)
    {
        std::fprintf(stderr, "[stages] %s: the loaded files reach the map reserve (end 0x%07X, reserve 0x%07X), nothing applied\n", id.c_str(),
                     end, sm::kReserve);
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
}

void dbInitDbTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kDbInitDb && g_pending)
    {
        g_pending = false;
        if (!g_active.empty())
            applyActive(rdram);
    }
    g_origDbInitDb(rdram, ctx, runtime);
}

void install(PS2Runtime &runtime)
{
    if ((g_origLoadLevelDB = runtime.lookupFunction(kLoadLevelDB)) && (g_origZip = runtime.lookupFunction(kZipInflateAll)) &&
        (g_origDbInitDb = runtime.lookupFunction(kDbInitDb)))
    {
        runtime.registerFunction(kLoadLevelDB, &loadLevelDBTapped);
        runtime.registerFunction(kZipInflateAll, &zipTapped);
        runtime.registerFunction(kDbInitDb, &dbInitDbTapped);
    }
    else
        std::fprintf(stderr, "[stages] maps unavailable: LoadLevelDB, zipInflateAll or dbInitDb has no function in the runtime\n");
}
} // namespace wotm_stagemap
